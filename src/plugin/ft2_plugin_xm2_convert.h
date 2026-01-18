/*
** FT2 Plugin - XM2 to XM conversion helpers
** Keeps usesXm2Features accurate and provides XM conversion utilities.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FT2_XM2_FEATURE_STEREO     = 1 << 0,
	FT2_XM2_FEATURE_FORMAT     = 1 << 1,
	FT2_XM2_FEATURE_SAMPLE_RATE = 1 << 2,
	FT2_XM2_FEATURE_HIGH_RATE  = 1 << 3,
	FT2_XM2_FEATURE_SAMPLE_SLOTS = 1 << 4,
	FT2_XM2_FEATURE_ENV_CURVE  = 1 << 5,
	FT2_XM2_FEATURE_ENV_LENGTH = 1 << 6
};

/* Recompute song.usesXm2Features by scanning actual data */
void ft2_xm2_update_usage(struct ft2_instance_t *inst);

/* Returns true if XM2 features are present, optionally returns bitmask */
bool ft2_xm2_song_needs_conversion(struct ft2_instance_t *inst, uint32_t *outFlags);

/* Convert one sample in-place to XM-compatible format */
bool ft2_xm2_conform_sample_inplace(struct ft2_instance_t *inst, ft2_sample_t *s);

/* Convert whole song in-place to XM-compatible format */
bool ft2_xm2_conform_song_inplace(struct ft2_instance_t *inst);

/* Build an XM-compatible copy of a sample (caller frees with ft2_xm2_free_sample_copy) */
bool ft2_xm2_build_xm_sample_copy(const ft2_instance_t *inst, const ft2_sample_t *src, ft2_sample_t *dst);
void ft2_xm2_free_sample_copy(ft2_sample_t *s);

typedef struct ft2_xm2_sample_params_t {
	ft2_sample_format_t format;
	int32_t length;
	int32_t loopStart;
	int32_t loopLength;
	double targetRate;
	bool resample;
} ft2_xm2_sample_params_t;

/* Compute XM-compatible params for a sample */
bool ft2_xm2_get_xm_sample_params(const ft2_instance_t *inst, const ft2_sample_t *src, ft2_xm2_sample_params_t *out);

#ifdef __cplusplus
}
#endif
