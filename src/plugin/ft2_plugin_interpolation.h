/**
 * @file ft2_plugin_interpolation.h
 * @brief Interpolation LUT generation for mixer.
 *
 * Modes: none, linear, quadratic, cubic, sinc8, sinc16.
 * Tables are reference-counted and shared across all instances.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mixer fixed-point fractional bits (32-bit position) */
#define PLUGIN_MIXER_FRAC_BITS  32
#define PLUGIN_MIXER_FRAC_SCALE ((int64_t)1 << PLUGIN_MIXER_FRAC_BITS)
#define PLUGIN_MIXER_FRAC_MASK  (PLUGIN_MIXER_FRAC_SCALE - 1)

enum ft2_interpolation_mode {
	FT2_INTERP_DISABLED  = 0, /* Point/nearest neighbor */
	FT2_INTERP_LINEAR    = 1, /* 2-point linear */
	FT2_INTERP_QUADRATIC = 2, /* 3-point quadratic spline */
	FT2_INTERP_CUBIC     = 3, /* 4-point Catmull-Rom */
	FT2_INTERP_SINC8     = 4, /* 8-point windowed sinc */
	FT2_INTERP_SINC16    = 5, /* 16-point windowed sinc */
	FT2_NUM_INTERP_MODES
};

/* Quadratic spline: 3 taps, 8192 phases */
#define QUADRATIC_SPLINE_WIDTH       3
#define QUADRATIC_SPLINE_PHASES      8192
#define QUADRATIC_SPLINE_PHASES_BITS 13
#define QUADRATIC_SPLINE_FRACSHIFT   (PLUGIN_MIXER_FRAC_BITS - QUADRATIC_SPLINE_PHASES_BITS)

/* Cubic spline: 4 taps, 8192 phases */
#define CUBIC_SPLINE_WIDTH      4
#define CUBIC_SPLINE_WIDTH_BITS 2
#define CUBIC_SPLINE_PHASES      8192
#define CUBIC_SPLINE_PHASES_BITS 13
#define CUBIC_SPLINE_FRACSHIFT  (PLUGIN_MIXER_FRAC_BITS - (CUBIC_SPLINE_PHASES_BITS + CUBIC_SPLINE_WIDTH_BITS))
#define CUBIC_SPLINE_FRACMASK   ((CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES) - CUBIC_SPLINE_WIDTH)

/* Windowed sinc: 3 kernels (different cutoffs), 8192 phases each */
#define SINC_KERNELS     3
#define SINC_PHASES      8192
#define SINC_PHASES_BITS 13

#define SINC8_WIDTH_BITS  3
#define SINC8_FRACSHIFT   (PLUGIN_MIXER_FRAC_BITS - (SINC_PHASES_BITS + SINC8_WIDTH_BITS))
#define SINC8_FRACMASK    ((8 * SINC_PHASES) - 8)

#define SINC16_WIDTH_BITS 4
#define SINC16_FRACSHIFT  (PLUGIN_MIXER_FRAC_BITS - (SINC_PHASES_BITS + SINC16_WIDTH_BITS))
#define SINC16_FRACMASK   ((16 * SINC_PHASES) - 16)

/* Global tables (shared across instances) */
typedef struct ft2_interp_tables_t {
	bool initialized;
	int32_t refCount;
	float *fQuadraticSplineLUT;              /* 3*8192 floats */
	float *fCubicSplineLUT;                  /* 4*8192 floats */
	float *fSinc8[SINC_KERNELS];             /* 8*8192 floats per kernel */
	float *fSinc16[SINC_KERNELS];            /* 16*8192 floats per kernel */
	uint64_t sincRatio1;                     /* Threshold: kernel 0 vs 1 */
	uint64_t sincRatio2;                     /* Threshold: kernel 1 vs 2 */
} ft2_interp_tables_t;

/* Init/free (reference counted) */
bool ft2_interp_tables_init(void);
void ft2_interp_tables_free(void);
ft2_interp_tables_t *ft2_interp_tables_get(void);

/* Select sinc kernel based on resampling ratio */
const float *ft2_select_sinc_kernel(uint64_t delta, ft2_interp_tables_t *tables, bool *is16Point);

#ifdef __cplusplus
}
#endif

