/**
 * @file ft2_plugin_modal_panels.h
 * @brief Modal panel system for complex sample editor dialogs
 *
 * Provides a coordinated system for managing modal panels (Volume, Resample,
 * Echo, Mix). Each panel is a self-contained module, and this manager tracks
 * which one is active and provides common drawing/input routing.
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
 * Modal panel types
 */
typedef enum modal_panel_type_t
{
	MODAL_PANEL_NONE = 0,
	MODAL_PANEL_VOLUME,
	MODAL_PANEL_RESAMPLE,
	MODAL_PANEL_ECHO,
	MODAL_PANEL_MIX,
	MODAL_PANEL_WAVE,
	MODAL_PANEL_FILTER
} modal_panel_type_t;

/**
 * Check if any modal panel is currently active.
 * @return true if a panel is active
 */
bool ft2_modal_panel_is_any_active(void);

/**
 * Get the type of the currently active panel.
 * @return The active panel type, or MODAL_PANEL_NONE if none active
 */
modal_panel_type_t ft2_modal_panel_get_active(void);

/**
 * Draw the currently active modal panel.
 * Does nothing if no panel is active.
 */
void ft2_modal_panel_draw_active(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Close the currently active modal panel.
 * Does nothing if no panel is active.
 */
void ft2_modal_panel_close_active(void);

/**
 * Notify the panel manager that a panel has been shown.
 * Called by individual panel modules when they activate.
 */
void ft2_modal_panel_set_active(modal_panel_type_t type);

/**
 * Notify the panel manager that a panel has been hidden.
 * Called by individual panel modules when they deactivate.
 */
void ft2_modal_panel_set_inactive(modal_panel_type_t type);

#ifdef __cplusplus
}
#endif

