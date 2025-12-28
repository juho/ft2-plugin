/**
 * @file ft2_plugin_wave_panel.h
 * @brief Wave generator length input panel for sample editor
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

/* Wave types */
typedef enum {
	WAVE_TYPE_TRIANGLE = 0,
	WAVE_TYPE_SAW = 1,
	WAVE_TYPE_SINE = 2,
	WAVE_TYPE_SQUARE = 3
} wave_type_t;

void ft2_wave_panel_show(struct ft2_instance_t *inst, wave_type_t waveType);
void ft2_wave_panel_hide(void);
bool ft2_wave_panel_is_active(void);
void ft2_wave_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Input handling */
bool ft2_wave_panel_key_down(int keycode);
bool ft2_wave_panel_char_input(char c);

/* Get instance for external access */
struct ft2_instance_t *ft2_wave_panel_get_instance(void);

#ifdef __cplusplus
}
#endif

