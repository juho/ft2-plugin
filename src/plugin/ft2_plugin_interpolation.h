#pragma once

/**
 * @file ft2_plugin_interpolation.h
 * @brief Interpolation LUT generation and management for the plugin.
 *
 * Provides all 6 interpolation modes:
 * - Point (no interpolation)
 * - Linear (2-point)
 * - Quadratic spline (3-point)
 * - Cubic spline (4-point Catmull-Rom)
 * - 8-point windowed sinc
 * - 16-point windowed sinc
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mixer fractional bits - must match original */
#define PLUGIN_MIXER_FRAC_BITS 32
#define PLUGIN_MIXER_FRAC_SCALE ((int64_t)1 << PLUGIN_MIXER_FRAC_BITS)
#define PLUGIN_MIXER_FRAC_MASK (PLUGIN_MIXER_FRAC_SCALE - 1)

/* Interpolation mode enum */
enum ft2_interpolation_mode
{
	FT2_INTERP_DISABLED  = 0,  /* Point/nearest neighbor */
	FT2_INTERP_LINEAR    = 1,  /* 2-point linear */
	FT2_INTERP_QUADRATIC = 2,  /* 3-point quadratic spline */
	FT2_INTERP_CUBIC     = 3,  /* 4-point cubic spline (Catmull-Rom) */
	FT2_INTERP_SINC8     = 4,  /* 8-point windowed sinc */
	FT2_INTERP_SINC16    = 5,  /* 16-point windowed sinc */
	FT2_NUM_INTERP_MODES
};

/* Quadratic spline parameters */
#define QUADRATIC_SPLINE_WIDTH 3
#define QUADRATIC_SPLINE_PHASES 8192
#define QUADRATIC_SPLINE_PHASES_BITS 13
#define QUADRATIC_SPLINE_FRACSHIFT (PLUGIN_MIXER_FRAC_BITS - QUADRATIC_SPLINE_PHASES_BITS)

/* Cubic spline parameters */
#define CUBIC_SPLINE_WIDTH 4
#define CUBIC_SPLINE_WIDTH_BITS 2
#define CUBIC_SPLINE_PHASES 8192
#define CUBIC_SPLINE_PHASES_BITS 13
#define CUBIC_SPLINE_FRACSHIFT (PLUGIN_MIXER_FRAC_BITS - (CUBIC_SPLINE_PHASES_BITS + CUBIC_SPLINE_WIDTH_BITS))
#define CUBIC_SPLINE_FRACMASK ((CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES) - CUBIC_SPLINE_WIDTH)

/* Windowed sinc parameters */
#define SINC_KERNELS 3
#define SINC_PHASES 8192
#define SINC_PHASES_BITS 13

#define SINC8_WIDTH_BITS 3
#define SINC8_FRACSHIFT (PLUGIN_MIXER_FRAC_BITS - (SINC_PHASES_BITS + SINC8_WIDTH_BITS))
#define SINC8_FRACMASK ((8 * SINC_PHASES) - 8)

#define SINC16_WIDTH_BITS 4
#define SINC16_FRACSHIFT (PLUGIN_MIXER_FRAC_BITS - (SINC_PHASES_BITS + SINC16_WIDTH_BITS))
#define SINC16_FRACMASK ((16 * SINC_PHASES) - 16)

/**
 * @brief Global interpolation tables structure.
 * 
 * These tables are shared across all plugin instances since they're
 * computed constants that don't change based on sample rate.
 */
typedef struct ft2_interp_tables_t
{
	bool initialized;
	int32_t refCount;
	
	/* Quadratic spline LUT */
	float *fQuadraticSplineLUT;
	
	/* Cubic spline LUT */
	float *fCubicSplineLUT;
	
	/* Windowed sinc LUTs (3 kernels each for different frequency ratios) */
	float *fSinc8[SINC_KERNELS];
	float *fSinc16[SINC_KERNELS];
	
	/* Sinc ratio thresholds for kernel selection */
	uint64_t sincRatio1;
	uint64_t sincRatio2;
} ft2_interp_tables_t;

/**
 * @brief Initialize the global interpolation tables.
 * @return true on success, false on failure.
 */
bool ft2_interp_tables_init(void);

/**
 * @brief Free the global interpolation tables.
 */
void ft2_interp_tables_free(void);

/**
 * @brief Get pointer to the global interpolation tables.
 * @return Pointer to the tables, or NULL if not initialized.
 */
ft2_interp_tables_t *ft2_interp_tables_get(void);

/**
 * @brief Select the appropriate sinc kernel based on resampling ratio.
 * @param delta The voice delta (resampling ratio).
 * @param tables The interpolation tables.
 * @param is16Point Output: true if 16-point sinc should be used.
 * @return Pointer to the appropriate sinc LUT.
 */
const float *ft2_select_sinc_kernel(uint64_t delta, ft2_interp_tables_t *tables, bool *is16Point);

#ifdef __cplusplus
}
#endif

