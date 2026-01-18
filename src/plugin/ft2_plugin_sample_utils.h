/*
** FT2 Plugin - Sample Utilities
** Shared helpers for format/stereo/sample-rate aware sample access.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

ft2_sample_format_t ft2_sample_get_format(const ft2_sample_t *s);
bool ft2_sample_has_data(const ft2_sample_t *s, ft2_sample_format_t format);
bool ft2_sample_is_stereo(const ft2_sample_t *s, ft2_sample_format_t format);
void *ft2_sample_get_data_ptr(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel);
const void *ft2_sample_get_data_ptr_const(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel);
float ft2_sample_get_float(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t pos);
void ft2_sample_set_float(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t pos, float value);
double ft2_sample_get_playback_rate(const ft2_instance_t *inst, const ft2_sample_t *s);
void ft2_sample_free_buffers(ft2_sample_t *s);
bool ft2_sample_alloc_buffers(ft2_sample_t *s, int32_t length, ft2_sample_format_t format, bool stereo);
int32_t ft2_get_max_samples_per_instr(const ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
