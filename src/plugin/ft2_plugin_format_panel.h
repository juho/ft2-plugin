/*
** FT2 Plugin - Sample Format/Rate Panel API
** Resamples current sample to a target rate and format.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

void ft2_format_panel_show(struct ft2_instance_t *inst);
void ft2_format_panel_hide(struct ft2_instance_t *inst);
bool ft2_format_panel_is_active(struct ft2_instance_t *inst);
void ft2_format_panel_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
bool ft2_format_panel_key_down(struct ft2_instance_t *inst, int keycode);
bool ft2_format_panel_char_input(struct ft2_instance_t *inst, char c);

#ifdef __cplusplus
}
#endif
