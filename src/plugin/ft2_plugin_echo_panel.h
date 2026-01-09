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

/* Panel lifecycle */
void ft2_echo_panel_show(struct ft2_instance_t *inst);
void ft2_echo_panel_hide(void);
bool ft2_echo_panel_is_active(void);
void ft2_echo_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_echo_panel_apply(void);

/* State accessors */
int16_t ft2_echo_panel_get_num(void);
void ft2_echo_panel_set_num(int16_t num);
int32_t ft2_echo_panel_get_distance(void);
void ft2_echo_panel_set_distance(int32_t dist);
int16_t ft2_echo_panel_get_vol_change(void);
void ft2_echo_panel_set_vol_change(int16_t vol);
bool ft2_echo_panel_get_add_memory(void);
void ft2_echo_panel_set_add_memory(bool add);

/* Input: checkbox toggle */
bool ft2_echo_panel_mouse_down(int32_t x, int32_t y, int button);

#ifdef __cplusplus
}
#endif

