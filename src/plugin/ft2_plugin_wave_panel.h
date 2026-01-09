/*
** FT2 Plugin - Wave Generator Length Input Panel API
** Prompts for cycle length and generates waveforms.
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

typedef enum { WAVE_TYPE_TRIANGLE = 0, WAVE_TYPE_SAW, WAVE_TYPE_SINE, WAVE_TYPE_SQUARE } wave_type_t;

/* Panel visibility */
void ft2_wave_panel_show(struct ft2_instance_t *inst, wave_type_t waveType);
void ft2_wave_panel_hide(void);
bool ft2_wave_panel_is_active(void);
void ft2_wave_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Input handling */
bool ft2_wave_panel_key_down(int keycode);
bool ft2_wave_panel_char_input(char c);

struct ft2_instance_t *ft2_wave_panel_get_instance(void);

#ifdef __cplusplus
}
#endif
