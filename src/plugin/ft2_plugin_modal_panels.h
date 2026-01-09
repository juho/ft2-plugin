/**
 * @file ft2_plugin_modal_panels.h
 * @brief Modal panel manager for sample editor dialogs.
 *
 * Only one panel can be active at a time. Each panel (Volume, Resample, Echo,
 * Mix, Wave, Filter) is a self-contained module; this manager tracks the
 * active panel and routes draw/close calls.
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

typedef enum modal_panel_type_t {
	MODAL_PANEL_NONE = 0,
	MODAL_PANEL_VOLUME,
	MODAL_PANEL_RESAMPLE,
	MODAL_PANEL_ECHO,
	MODAL_PANEL_MIX,
	MODAL_PANEL_WAVE,
	MODAL_PANEL_FILTER
} modal_panel_type_t;

bool ft2_modal_panel_is_any_active(void);
modal_panel_type_t ft2_modal_panel_get_active(void);
void ft2_modal_panel_draw_active(struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_modal_panel_close_active(void);

/* Called by panel modules on show/hide */
void ft2_modal_panel_set_active(modal_panel_type_t type);
void ft2_modal_panel_set_inactive(modal_panel_type_t type);

#ifdef __cplusplus
}
#endif

