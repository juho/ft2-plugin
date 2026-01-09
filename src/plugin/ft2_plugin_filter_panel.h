/**
 * @file ft2_plugin_filter_panel.h
 * @brief Filter cutoff input panel for sample editor.
 *
 * Modal panel for entering low-pass or high-pass cutoff frequency in Hz.
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

typedef enum {
	FILTER_TYPE_LOWPASS = 0,
	FILTER_TYPE_HIGHPASS = 1
} filter_type_t;

/* Panel lifecycle */
void ft2_filter_panel_show(struct ft2_instance_t *inst, filter_type_t filterType);
void ft2_filter_panel_hide(void);
bool ft2_filter_panel_is_active(void);
void ft2_filter_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Input handling */
bool ft2_filter_panel_key_down(int keycode);
bool ft2_filter_panel_char_input(char c);

struct ft2_instance_t *ft2_filter_panel_get_instance(void);

#ifdef __cplusplus
}
#endif

