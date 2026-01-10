/**
 * @file ft2_plugin_echo_panel.h
 * @brief Echo effect modal panel for sample editor.
 *
 * Parameters: echo count, distance (samples), fade %, add memory option.
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

void ft2_echo_panel_show(struct ft2_instance_t *inst);
void ft2_echo_panel_hide(struct ft2_instance_t *inst);
bool ft2_echo_panel_is_active(struct ft2_instance_t *inst);
void ft2_echo_panel_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_echo_panel_apply(struct ft2_instance_t *inst);
bool ft2_echo_panel_mouse_down(struct ft2_instance_t *inst, int32_t x, int32_t y, int button);

#ifdef __cplusplus
}
#endif
