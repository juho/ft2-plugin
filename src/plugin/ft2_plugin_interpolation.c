/**
 * @file ft2_plugin_interpolation.c
 * @brief Interpolation LUT generation for all 6 interpolation modes.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_interpolation.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Global interpolation tables - shared across all instances */
static ft2_interp_tables_t g_interpTables = { 0 };

/* Sinc kernel configuration */
typedef struct sincKernel_t
{
	double kaiserBeta;
	double sincCutoff;
} sincKernel_t;

static const sincKernel_t sincKernelConfig[SINC_KERNELS] =
{
	{ 9.6377, 1.000 }, /* kernel #1 */
	{ 8.5000, 0.750 }, /* kernel #2 */
	{ 7.3000, 0.425 }  /* kernel #3 */
};

/* Zeroth-order modified Bessel function of the first kind (series approximation) */
static double besselI0(double z)
{
	double s = 1.0, ds = 1.0, d = 2.0;
	const double zz = z * z;

	do
	{
		ds *= zz / (d * d);
		s += ds;
		d += 2.0;
	}
	while (ds > s * 1E-12);

	return s;
}

static double sinc(double x, double cutoff)
{
	if (x == 0.0)
		return cutoff;
	
	x *= M_PI;
	return sin(cutoff * x) / x;
}

static bool setupQuadraticSplineTable(void)
{
	const size_t tableSize = QUADRATIC_SPLINE_WIDTH * QUADRATIC_SPLINE_PHASES * sizeof(float);
	g_interpTables.fQuadraticSplineLUT = (float *)malloc(tableSize);
	if (g_interpTables.fQuadraticSplineLUT == NULL)
		return false;

	float *fPtr = g_interpTables.fQuadraticSplineLUT;
	for (int32_t i = 0; i < QUADRATIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / QUADRATIC_SPLINE_PHASES);
		const double x2 = x1 * x1;

		const double t1 = (x1 * -1.5) + (x2 * 0.5) + 1.0;
		const double t2 = (x1 * 2.0) + (x2 * -1.0);
		const double t3 = (x1 * -0.5) + (x2 * 0.5);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
	}

	return true;
}

static bool setupCubicSplineTable(void)
{
	const size_t tableSize = CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES * sizeof(float);
	g_interpTables.fCubicSplineLUT = (float *)malloc(tableSize);
	if (g_interpTables.fCubicSplineLUT == NULL)
		return false;

	float *fPtr = g_interpTables.fCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1;
		const double x3 = x2 * x1;

		const double t1 = (x1 * -0.5) + (x2 * 1.0) + (x3 * -0.5);
		const double t2 = (x2 * -2.5) + (x3 * 1.5) + 1.0;
		const double t3 = (x1 * 0.5) + (x2 * 2.0) + (x3 * -1.5);
		const double t4 = (x2 * -0.5) + (x3 * 0.5);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
	}

	return true;
}

static void makeSincKernel(float *fOut, int32_t numPoints, int32_t numPhases, double beta, double cutoff)
{
	const int32_t kernelLen = numPhases * numPoints;
	const int32_t pointBits = (int32_t)log2(numPoints);
	const int32_t pointMask = numPoints - 1;
	const int32_t centerPoint = (numPoints / 2) - 1;
	const double besselI0Beta = 1.0 / besselI0(beta);
	const double phaseMul = 1.0 / numPhases;
	const double xMul = 1.0 / (numPoints / 2);

	for (int32_t i = 0; i < kernelLen; i++)
	{
		const double x = ((i & pointMask) - centerPoint) - ((i >> pointBits) * phaseMul);

		/* Kaiser-Bessel window */
		const double n = x * xMul;
		double windowArg = 1.0 - n * n;
		if (windowArg < 0.0)
			windowArg = 0.0;
		const double window = besselI0(beta * sqrt(windowArg)) * besselI0Beta;

		fOut[i] = (float)(sinc(x, cutoff) * window);
	}
}

static bool setupWindowedSincTables(void)
{
	for (int32_t i = 0; i < SINC_KERNELS; i++)
	{
		g_interpTables.fSinc8[i] = (float *)malloc(8 * SINC_PHASES * sizeof(float));
		g_interpTables.fSinc16[i] = (float *)malloc(16 * SINC_PHASES * sizeof(float));

		if (g_interpTables.fSinc8[i] == NULL || g_interpTables.fSinc16[i] == NULL)
			return false;

		makeSincKernel(g_interpTables.fSinc8[i], 8, SINC_PHASES,
		               sincKernelConfig[i].kaiserBeta, sincKernelConfig[i].sincCutoff);
		makeSincKernel(g_interpTables.fSinc16[i], 16, SINC_PHASES,
		               sincKernelConfig[i].kaiserBeta, sincKernelConfig[i].sincCutoff);
	}

	/* Resampling ratios for sinc kernel selection */
	g_interpTables.sincRatio1 = (uint64_t)(1.1875 * PLUGIN_MIXER_FRAC_SCALE);
	g_interpTables.sincRatio2 = (uint64_t)(1.5000 * PLUGIN_MIXER_FRAC_SCALE);

	return true;
}

bool ft2_interp_tables_init(void)
{
	if (g_interpTables.initialized)
	{
		g_interpTables.refCount++;
		return true;
	}

	memset(&g_interpTables, 0, sizeof(g_interpTables));

	if (!setupQuadraticSplineTable())
		goto fail;

	if (!setupCubicSplineTable())
		goto fail;

	if (!setupWindowedSincTables())
		goto fail;

	g_interpTables.initialized = true;
	g_interpTables.refCount = 1;
	return true;

fail:
	ft2_interp_tables_free();
	return false;
}

void ft2_interp_tables_free(void)
{
	if (!g_interpTables.initialized)
		return;

	g_interpTables.refCount--;
	if (g_interpTables.refCount > 0)
		return;

	if (g_interpTables.fQuadraticSplineLUT != NULL)
	{
		free(g_interpTables.fQuadraticSplineLUT);
		g_interpTables.fQuadraticSplineLUT = NULL;
	}

	if (g_interpTables.fCubicSplineLUT != NULL)
	{
		free(g_interpTables.fCubicSplineLUT);
		g_interpTables.fCubicSplineLUT = NULL;
	}

	for (int32_t i = 0; i < SINC_KERNELS; i++)
	{
		if (g_interpTables.fSinc8[i] != NULL)
		{
			free(g_interpTables.fSinc8[i]);
			g_interpTables.fSinc8[i] = NULL;
		}

		if (g_interpTables.fSinc16[i] != NULL)
		{
			free(g_interpTables.fSinc16[i]);
			g_interpTables.fSinc16[i] = NULL;
		}
	}

	g_interpTables.initialized = false;
}

ft2_interp_tables_t *ft2_interp_tables_get(void)
{
	if (!g_interpTables.initialized)
		return NULL;
	return &g_interpTables;
}

const float *ft2_select_sinc_kernel(uint64_t delta, ft2_interp_tables_t *tables, bool *is16Point)
{
	if (tables == NULL)
		return NULL;

	int32_t kernelIdx;
	if (delta <= tables->sincRatio1)
		kernelIdx = 0;
	else if (delta <= tables->sincRatio2)
		kernelIdx = 1;
	else
		kernelIdx = 2;

	/* Use 16-point for high quality at lower resampling ratios */
	*is16Point = (delta <= tables->sincRatio1);

	if (*is16Point)
		return tables->fSinc16[kernelIdx];
	else
		return tables->fSinc8[kernelIdx];
}

