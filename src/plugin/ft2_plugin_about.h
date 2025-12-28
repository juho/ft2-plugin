/**
 * @file ft2_plugin_about.h
 * @brief About screen - exact port from ft2_about.c
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;

/**
 * Initialize the about screen (called once at startup).
 */
void ft2_about_init(void);

/**
 * Show the about screen (called when opening).
 * Draws the framework and initializes starfield if using old mode.
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_about_show(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Render one frame of the about screen animation.
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_about_render_frame(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Draw the about screen (calls render_frame internally).
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_about_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Update about screen animation state (deprecated, handled in render_frame).
 */
void ft2_about_update(void);

/**
 * Set about screen mode.
 * @param newMode true for new starfield with waving logo, false for classic FT2 starfield
 */
void ft2_about_set_mode(bool newMode);

/**
 * Get current about screen mode.
 * @return true if using new mode, false for classic mode
 */
bool ft2_about_get_mode(void);

#ifdef __cplusplus
}
#endif
