/**
 * @file ft2_plugin_mix_panel.h
 * @brief Sample mixing modal panel for sample editor
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
void ft2_mix_panel_hide(void);
bool ft2_mix_panel_is_active(void);
void ft2_mix_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_mix_panel_apply(void);

int8_t ft2_mix_panel_get_balance(void);
void ft2_mix_panel_set_balance(int8_t balance);

#ifdef __cplusplus
}
#endif

