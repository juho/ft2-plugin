/*
** FT2 Plugin - Sample Channels Panel API
** Converts mono/stereo channel layouts for the current sample.
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

void ft2_channels_panel_show(struct ft2_instance_t *inst);
void ft2_channels_panel_hide(struct ft2_instance_t *inst);
bool ft2_channels_panel_is_active(struct ft2_instance_t *inst);
void ft2_channels_panel_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

#ifdef __cplusplus
}
#endif
