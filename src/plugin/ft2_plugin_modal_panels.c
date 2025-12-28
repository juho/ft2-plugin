/**
 * @file ft2_plugin_modal_panels.c
 * @brief Modal panel manager implementation
 */

#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_volume_panel.h"
#include "ft2_plugin_resample_panel.h"
#include "ft2_plugin_echo_panel.h"
#include "ft2_plugin_mix_panel.h"
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_filter_panel.h"

/* Currently active panel type */
static modal_panel_type_t activePanel = MODAL_PANEL_NONE;

bool ft2_modal_panel_is_any_active(void)
{
	return activePanel != MODAL_PANEL_NONE;
}

modal_panel_type_t ft2_modal_panel_get_active(void)
{
	return activePanel;
}

void ft2_modal_panel_draw_active(struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	switch (activePanel)
	{
		case MODAL_PANEL_VOLUME:
			ft2_volume_panel_draw(video, bmp);
			break;
		case MODAL_PANEL_RESAMPLE:
			ft2_resample_panel_draw(video, bmp);
			break;
		case MODAL_PANEL_ECHO:
			ft2_echo_panel_draw(video, bmp);
			break;
		case MODAL_PANEL_MIX:
			ft2_mix_panel_draw(video, bmp);
			break;
		case MODAL_PANEL_WAVE:
			ft2_wave_panel_draw(video, bmp);
			break;
		case MODAL_PANEL_FILTER:
			ft2_filter_panel_draw(video, bmp);
			break;
		default:
			break;
	}
}

void ft2_modal_panel_close_active(void)
{
	switch (activePanel)
	{
		case MODAL_PANEL_VOLUME:
			ft2_volume_panel_hide();
			break;
		case MODAL_PANEL_RESAMPLE:
			ft2_resample_panel_hide();
			break;
		case MODAL_PANEL_ECHO:
			ft2_echo_panel_hide();
			break;
		case MODAL_PANEL_MIX:
			ft2_mix_panel_hide();
			break;
		case MODAL_PANEL_WAVE:
			ft2_wave_panel_hide();
			break;
		case MODAL_PANEL_FILTER:
			ft2_filter_panel_hide();
			break;
		default:
			break;
	}
}

void ft2_modal_panel_set_active(modal_panel_type_t type)
{
	/* Close any currently active panel first */
	if (activePanel != MODAL_PANEL_NONE && activePanel != type)
		ft2_modal_panel_close_active();
	
	activePanel = type;
}

void ft2_modal_panel_set_inactive(modal_panel_type_t type)
{
	if (activePanel == type)
		activePanel = MODAL_PANEL_NONE;
}

