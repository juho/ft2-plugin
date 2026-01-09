/**
 * @file ft2_plugin_interpolation.c
 * @brief Interpolation LUT generation for mixer.
 *
 * Generates precomputed tables for:
 *   - Quadratic spline (3-point, 8192 phases)
 *   - Cubic spline (4-point Catmull-Rom, 8192 phases)
 *   - Windowed sinc (8/16-point Kaiser-Bessel, 3 kernels for different ratios)
 *
 * Tables are shared across all plugin instances (reference counted).
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_interpolation.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static ft2_interp_tables_t g_interpTables = { 0 };

/* Kaiser-Bessel window parameters per kernel (beta, cutoff) */
typedef struct sincKernel_t {
	double kaiserBeta;
	double sincCutoff;
} sincKernel_t;

/* Kernel 0: high quality (ratio <= 1.1875x)
 * Kernel 1: medium quality (ratio <= 1.5x)
 * Kernel 2: low quality (ratio > 1.5x) */
static const sincKernel_t sincKernelConfig[SINC_KERNELS] = {
	{ 9.6377, 1.000 },
	{ 8.5000, 0.750 },
	{ 7.3000, 0.425 }
};

/* Zeroth-order modified Bessel function (series approximation) */
static double besselI0(double z)
{
	double s = 1.0, ds = 1.0, d = 2.0;
	const double zz = z * z;
	do {
		ds *= zz / (d * d);
		s += ds;
		d += 2.0;
	} while (ds > s * 1E-12);
	return s;
}

/* Normalized sinc: sin(pi*x*cutoff)/(pi*x) */
static double sinc(double x, double cutoff)
{
	if (x == 0.0) return cutoff;
	x *= M_PI;
	return sin(cutoff * x) / x;
}

/* 3-point quadratic spline coefficients */
static bool setupQuadraticSplineTable(void)
{
	const size_t tableSize = QUADRATIC_SPLINE_WIDTH * QUADRATIC_SPLINE_PHASES * sizeof(float);
	g_interpTables.fQuadraticSplineLUT = (float *)malloc(tableSize);
	if (g_interpTables.fQuadraticSplineLUT == NULL) return false;

	float *fPtr = g_interpTables.fQuadraticSplineLUT;
	for (int32_t i = 0; i < QUADRATIC_SPLINE_PHASES; i++) {
		const double x1 = i * (1.0 / QUADRATIC_SPLINE_PHASES);
		const double x2 = x1 * x1;
		*fPtr++ = (float)((x1 * -1.5) + (x2 * 0.5) + 1.0);
		*fPtr++ = (float)((x1 * 2.0) + (x2 * -1.0));
		*fPtr++ = (float)((x1 * -0.5) + (x2 * 0.5));
	}
	return true;
}

/* 4-point Catmull-Rom cubic spline coefficients */
static bool setupCubicSplineTable(void)
{
	const size_t tableSize = CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES * sizeof(float);
	g_interpTables.fCubicSplineLUT = (float *)malloc(tableSize);
	if (g_interpTables.fCubicSplineLUT == NULL) return false;

	float *fPtr = g_interpTables.fCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++) {
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1;
		const double x3 = x2 * x1;
		*fPtr++ = (float)((x1 * -0.5) + (x2 * 1.0) + (x3 * -0.5));
		*fPtr++ = (float)((x2 * -2.5) + (x3 * 1.5) + 1.0);
		*fPtr++ = (float)((x1 * 0.5) + (x2 * 2.0) + (x3 * -1.5));
		*fPtr++ = (float)((x2 * -0.5) + (x3 * 0.5));
	}
	return true;
}

/* Generate windowed sinc kernel with Kaiser-Bessel window */
static void makeSincKernel(float *fOut, int32_t numPoints, int32_t numPhases, double beta, double cutoff)
{
	const int32_t kernelLen = numPhases * numPoints;
	const int32_t pointBits = (int32_t)log2(numPoints);
	const int32_t pointMask = numPoints - 1;
	const int32_t centerPoint = (numPoints / 2) - 1;
	const double besselI0Beta = 1.0 / besselI0(beta);
	const double phaseMul = 1.0 / numPhases;
	const double xMul = 1.0 / (numPoints / 2);

	for (int32_t i = 0; i < kernelLen; i++) {
		const double x = ((i & pointMask) - centerPoint) - ((i >> pointBits) * phaseMul);
		const double n = x * xMul;
		double windowArg = 1.0 - n * n;
		if (windowArg < 0.0) windowArg = 0.0;
		const double window = besselI0(beta * sqrt(windowArg)) * besselI0Beta;
		fOut[i] = (float)(sinc(x, cutoff) * window);
	}
}

/* Generate 8-point and 16-point sinc tables for all 3 kernels */
static bool setupWindowedSincTables(void)
{
	for (int32_t i = 0; i < SINC_KERNELS; i++) {
		g_interpTables.fSinc8[i] = (float *)malloc(8 * SINC_PHASES * sizeof(float));
		g_interpTables.fSinc16[i] = (float *)malloc(16 * SINC_PHASES * sizeof(float));
		if (g_interpTables.fSinc8[i] == NULL || g_interpTables.fSinc16[i] == NULL)
			return false;
		makeSincKernel(g_interpTables.fSinc8[i], 8, SINC_PHASES,
		               sincKernelConfig[i].kaiserBeta, sincKernelConfig[i].sincCutoff);
		makeSincKernel(g_interpTables.fSinc16[i], 16, SINC_PHASES,
		               sincKernelConfig[i].kaiserBeta, sincKernelConfig[i].sincCutoff);
	}
	g_interpTables.sincRatio1 = (uint64_t)(1.1875 * PLUGIN_MIXER_FRAC_SCALE);
	g_interpTables.sincRatio2 = (uint64_t)(1.5000 * PLUGIN_MIXER_FRAC_SCALE);
	return true;
}

/* ---------- Public API ---------- */

bool ft2_interp_tables_init(void)
{
	if (g_interpTables.initialized) {
		g_interpTables.refCount++;
		return true;
	}
	memset(&g_interpTables, 0, sizeof(g_interpTables));
	if (!setupQuadraticSplineTable()) goto fail;
	if (!setupCubicSplineTable()) goto fail;
	if (!setupWindowedSincTables()) goto fail;
	g_interpTables.initialized = true;
	g_interpTables.refCount = 1;
	return true;
fail:
	ft2_interp_tables_free();
	return false;
}

void ft2_interp_tables_free(void)
{
	if (!g_interpTables.initialized) return;
	g_interpTables.refCount--;
	if (g_interpTables.refCount > 0) return;

	free(g_interpTables.fQuadraticSplineLUT);
	g_interpTables.fQuadraticSplineLUT = NULL;
	free(g_interpTables.fCubicSplineLUT);
	g_interpTables.fCubicSplineLUT = NULL;
	for (int32_t i = 0; i < SINC_KERNELS; i++) {
		free(g_interpTables.fSinc8[i]);
		g_interpTables.fSinc8[i] = NULL;
		free(g_interpTables.fSinc16[i]);
		g_interpTables.fSinc16[i] = NULL;
	}
	g_interpTables.initialized = false;
}

ft2_interp_tables_t *ft2_interp_tables_get(void)
{
	return g_interpTables.initialized ? &g_interpTables : NULL;
}

/* Select sinc kernel based on resampling ratio (delta).
 * Higher ratios use smaller kernels with more aggressive cutoff. */
const float *ft2_select_sinc_kernel(uint64_t delta, ft2_interp_tables_t *tables, bool *is16Point)
{
	if (tables == NULL) return NULL;

	int32_t kernelIdx;
	if (delta <= tables->sincRatio1) kernelIdx = 0;
	else if (delta <= tables->sincRatio2) kernelIdx = 1;
	else kernelIdx = 2;

	*is16Point = (delta <= tables->sincRatio1);
	return *is16Point ? tables->fSinc16[kernelIdx] : tables->fSinc8[kernelIdx];
}

