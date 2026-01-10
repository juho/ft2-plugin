/*
** FT2 Plugin - Wave Generator Length Input Panel API
** Prompts for cycle length and generates waveforms.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_plugin_modal_panels.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

void ft2_wave_panel_show(struct ft2_instance_t *inst, wave_type_t waveType);
void ft2_wave_panel_hide(struct ft2_instance_t *inst);
bool ft2_wave_panel_is_active(struct ft2_instance_t *inst);
void ft2_wave_panel_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
bool ft2_wave_panel_key_down(struct ft2_instance_t *inst, int keycode);
bool ft2_wave_panel_char_input(struct ft2_instance_t *inst, char c);

#ifdef __cplusplus
}
#endif
