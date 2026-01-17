/**
 * @file ft2_plugin_common.h
 * @brief Common utility macros and constants shared across plugin modules.
 */

#ifndef FT2_PLUGIN_COMMON_H
#define FT2_PLUGIN_COMMON_H

/* Common utility macros */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif

/* Sample length limit (used by sample operations) */
#define FT2_MAX_SAMPLE_LEN 0x3FFFFFFF

#endif
