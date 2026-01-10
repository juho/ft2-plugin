/**
 * @file ft2_plugin_modal_panels.c
 * @brief Modal panel manager (Volume, Resample, Echo, Mix, Wave, Filter).
 *
 * Only one modal panel can be active at a time. The manager tracks the active
 * panel and routes draw/close calls to the appropriate panel implementation.
 */

#include <string.h>
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_volume_panel.h"
#include "ft2_plugin_resample_panel.h"
#include "ft2_plugin_echo_panel.h"
#include "ft2_plugin_mix_panel.h"
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_filter_panel.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

#define MODAL_STATE(inst) (&FT2_UI(inst)->modalPanels)

void ft2_modal_panel_init(ft2_modal_state_t *state)
{
	if (!state) return;
	memset(state, 0, sizeof(ft2_modal_state_t));
	state->activePanel = MODAL_PANEL_NONE;
	
	/* Volume panel defaults */
	state->volume.startVol = 100.0;
	state->volume.endVol = 100.0;
	
	/* Resample panel defaults */
	state->resample.relReSmp = 0;
	
	/* Echo panel defaults */
	state->echo.echoNum = 1;
	state->echo.echoDistance = 0x100;
	state->echo.echoVolChange = 30;
	state->echo.echoAddMemory = false;
	
	/* Mix panel defaults */
	state->mix.mixBalance = 50;
	
	/* Wave panel defaults */
	state->wave.waveType = WAVE_TYPE_TRIANGLE;
	strcpy(state->wave.inputBuffer, "64");
	state->wave.inputCursorPos = 2;
	state->wave.lastWaveLength = 64;
	
	/* Filter panel defaults */
	state->filter.filterType = FILTER_TYPE_LOWPASS;
	strcpy(state->filter.inputBuffer, "2000");
	state->filter.inputCursorPos = 4;
	state->filter.lastLpCutoff = 2000;
	state->filter.lastHpCutoff = 200;
}

bool ft2_modal_panel_is_any_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return MODAL_STATE(inst)->activePanel != MODAL_PANEL_NONE;
}

modal_panel_type_t ft2_modal_panel_get_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return MODAL_PANEL_NONE;
	return MODAL_STATE(inst)->activePanel;
}

void ft2_modal_panel_draw_active(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui) return;
	
	switch (MODAL_STATE(inst)->activePanel)
	{
		case MODAL_PANEL_VOLUME:   ft2_volume_panel_draw(inst, video, bmp); break;
		case MODAL_PANEL_RESAMPLE: ft2_resample_panel_draw(inst, video, bmp); break;
		case MODAL_PANEL_ECHO:     ft2_echo_panel_draw(inst, video, bmp); break;
		case MODAL_PANEL_MIX:      ft2_mix_panel_draw(inst, video, bmp); break;
		case MODAL_PANEL_WAVE:     ft2_wave_panel_draw(inst, video, bmp); break;
		case MODAL_PANEL_FILTER:   ft2_filter_panel_draw(inst, video, bmp); break;
		default: break;
	}
}

void ft2_modal_panel_close_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	
	switch (MODAL_STATE(inst)->activePanel)
	{
		case MODAL_PANEL_VOLUME:   ft2_volume_panel_hide(inst); break;
		case MODAL_PANEL_RESAMPLE: ft2_resample_panel_hide(inst); break;
		case MODAL_PANEL_ECHO:     ft2_echo_panel_hide(inst); break;
		case MODAL_PANEL_MIX:      ft2_mix_panel_hide(inst); break;
		case MODAL_PANEL_WAVE:     ft2_wave_panel_hide(inst); break;
		case MODAL_PANEL_FILTER:   ft2_filter_panel_hide(inst); break;
		default: break;
	}
}

void ft2_modal_panel_set_active(ft2_instance_t *inst, modal_panel_type_t type)
{
	if (!inst || !inst->ui) return;
	ft2_modal_state_t *state = MODAL_STATE(inst);
	
	if (state->activePanel != MODAL_PANEL_NONE && state->activePanel != type)
		ft2_modal_panel_close_active(inst);
	state->activePanel = type;
}

void ft2_modal_panel_set_inactive(ft2_instance_t *inst, modal_panel_type_t type)
{
	if (!inst || !inst->ui) return;
	ft2_modal_state_t *state = MODAL_STATE(inst);
	
	if (state->activePanel == type)
		state->activePanel = MODAL_PANEL_NONE;
}
