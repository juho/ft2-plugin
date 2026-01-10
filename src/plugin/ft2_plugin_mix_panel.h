/**
 * @file ft2_plugin_mix_panel.h
 * @brief Sample mixing modal panel (mixes src sample into current sample).
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

void ft2_mix_panel_show(struct ft2_instance_t *inst);
void ft2_mix_panel_hide(struct ft2_instance_t *inst);
bool ft2_mix_panel_is_active(struct ft2_instance_t *inst);
void ft2_mix_panel_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_mix_panel_apply(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
