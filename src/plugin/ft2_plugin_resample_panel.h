/**
 * @file ft2_plugin_resample_panel.h
 * @brief Resample modal panel for sample editor
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

void ft2_resample_panel_show(struct ft2_instance_t *inst);
void ft2_resample_panel_hide(void);
bool ft2_resample_panel_is_active(void);
void ft2_resample_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_resample_panel_apply(void);

int8_t ft2_resample_panel_get_tones(void);
void ft2_resample_panel_set_tones(int8_t tones);

#ifdef __cplusplus
}
#endif

