/**
 * @file ft2_plugin_about.h
 * @brief About screen with animated 3D starfield and credits.
 *
 * Two display modes:
 *   New mode: float-precision starfield, waving FT2 logo, blue-tinted stars
 *   Classic mode: integer starfield (galaxy/spiral/stars), original FT2 logo
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_widgets_t;

/* Initialize static state (sinus tables, star positions). Call once at startup. */
void ft2_about_init(void);

/* Display the about screen. Draws framework and initializes starfield pattern. */
void ft2_about_show(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Render one animation frame (starfield rotation, logo wave). Call per UI frame. */
void ft2_about_render_frame(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Wrapper for render_frame (legacy interface). */
void ft2_about_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Set display mode: true = new (waving logo), false = classic FT2. */
void ft2_about_set_mode(bool newMode);

/* Get current display mode. */
bool ft2_about_get_mode(void);

#ifdef __cplusplus
}
#endif
