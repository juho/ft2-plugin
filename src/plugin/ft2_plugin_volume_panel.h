/**
 * @file ft2_plugin_volume_panel.h
 * @brief Volume adjustment modal panel for sample editor
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

/**
 * Show the volume panel.
 * @param inst The FT2 instance
 */
void ft2_volume_panel_show(struct ft2_instance_t *inst);

/**
 * Hide the volume panel.
 */
void ft2_volume_panel_hide(void);

/**
 * Check if the volume panel is active.
 */
bool ft2_volume_panel_is_active(void);

/**
 * Draw the volume panel.
 */
void ft2_volume_panel_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Apply the volume settings to the sample and close the panel.
 */
void ft2_volume_panel_apply(void);

/**
 * Find the maximum scale value and set both volumes to it.
 */
void ft2_volume_panel_find_max_scale(void);

/**
 * Get the current start volume value.
 */
double ft2_volume_panel_get_start_vol(void);

/**
 * Set the start volume value.
 */
void ft2_volume_panel_set_start_vol(double vol);

/**
 * Get the current end volume value.
 */
double ft2_volume_panel_get_end_vol(void);

/**
 * Set the end volume value.
 */
void ft2_volume_panel_set_end_vol(double vol);

#ifdef __cplusplus
}
#endif

