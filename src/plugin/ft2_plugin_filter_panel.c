/**
 * @file ft2_plugin_filter_panel.c
 * @brief Filter cutoff input panel for sample editor.
 *
 * Modal panel to enter low-pass or high-pass cutoff frequency (Hz).
 * Remembers last used values per filter type.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft2_plugin_filter_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

#define FILTER_STATE(inst) (&FT2_UI(inst)->modalPanels.filter)

static void onOKClick(ft2_instance_t *inst);
static void onCancelClick(ft2_instance_t *inst);

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	pushButton_t *p;

	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "OK"; p->x = 246; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onOKClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel"; p->x = 346; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onCancelClick; widgets->pushButtonVisible[PB_RES_2] = true;
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
}

static void applyFilter(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	filter_panel_state_t *state = FILTER_STATE(inst);
	if (!state->active) return;

	int32_t cutoff = atoi(state->inputBuffer);
	if (cutoff < 1 || cutoff > 99999) return;

	if (state->filterType == FILTER_TYPE_LOWPASS)
		state->lastLpCutoff = cutoff;
	else
		state->lastHpCutoff = cutoff;

	smpfx_apply_filter(inst, state->filterType == FILTER_TYPE_LOWPASS ? 0 : 1, cutoff);
}

static void onOKClick(ft2_instance_t *inst) { applyFilter(inst); ft2_filter_panel_hide(inst); }
static void onCancelClick(ft2_instance_t *inst) { ft2_filter_panel_hide(inst); }

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 146, y = 249, w = 380, h = 67;
	filter_panel_state_t *state = FILTER_STATE(inst);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);

	const char *headline = (state->filterType == FILTER_TYPE_LOWPASS)
		? "Enter low-pass filter cutoff (in Hz):"
		: "Enter high-pass filter cutoff (in Hz):";
	int hlen = textWidth(headline);
	textOut(video, bmp, (632 - hlen) / 2, y + 4, PAL_FORGRND, headline);

	int inputX = x + 10, inputY = y + 28, inputW = w - 20, inputH = 12;
	fillRect(video, inputX, inputY, inputW, inputH, PAL_DESKTOP);
	hLine(video, inputX, inputY, inputW, PAL_BUTTON2);
	vLine(video, inputX, inputY, inputH, PAL_BUTTON2);
	hLine(video, inputX, inputY + inputH - 1, inputW, PAL_BUTTON1);
	vLine(video, inputX + inputW - 1, inputY, inputH, PAL_BUTTON1);
	textOut(video, bmp, inputX + 2, inputY + 2, PAL_FORGRND, state->inputBuffer);

	int cursorX = inputX + 2 + textWidth(state->inputBuffer);
	vLine(video, cursorX, inputY + 2, 8, PAL_FORGRND);
}

void ft2_filter_panel_show(ft2_instance_t *inst, filter_type_t filterType)
{
	if (!inst || !inst->ui || inst->editor.curInstr == 0) return;

	filter_panel_state_t *state = FILTER_STATE(inst);
	state->active = true;
	state->filterType = filterType;

	snprintf(state->inputBuffer, sizeof(state->inputBuffer), "%d",
	         (filterType == FILTER_TYPE_LOWPASS) ? state->lastLpCutoff : state->lastHpCutoff);
	state->inputCursorPos = (int)strlen(state->inputBuffer);

	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_FILTER);
}

void ft2_filter_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	filter_panel_state_t *state = FILTER_STATE(inst);
	if (!state->active) return;
	
	hideWidgets(inst);
	state->active = false;
	inst->uiState.updateSampleEditor = true;
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_FILTER);
}

bool ft2_filter_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return FILTER_STATE(inst)->active;
}

void ft2_filter_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	filter_panel_state_t *state = FILTER_STATE(inst);
	if (!state->active) return;
	
	drawFrame(inst, video, bmp);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i]) drawPushButton(widgets, video, bmp, PB_RES_1 + i);
}

bool ft2_filter_panel_key_down(ft2_instance_t *inst, int keycode)
{
	if (!inst || !inst->ui) return false;
	filter_panel_state_t *state = FILTER_STATE(inst);
	if (!state->active) return false;

	if (keycode == FT2_KEY_RETURN) { applyFilter(inst); ft2_filter_panel_hide(inst); return true; }
	if (keycode == FT2_KEY_ESCAPE) { ft2_filter_panel_hide(inst); return true; }
	if (keycode == FT2_KEY_BACKSPACE) {
		int len = (int)strlen(state->inputBuffer);
		if (len > 0) { state->inputBuffer[len - 1] = '\0'; state->inputCursorPos = len - 1; }
		return true;
	}
	return true;
}

bool ft2_filter_panel_char_input(ft2_instance_t *inst, char c)
{
	if (!inst || !inst->ui) return false;
	filter_panel_state_t *state = FILTER_STATE(inst);
	if (!state->active) return false;
	
	if (c >= '0' && c <= '9') {
		int len = (int)strlen(state->inputBuffer);
		if (len < 5) { state->inputBuffer[len] = c; state->inputBuffer[len + 1] = '\0'; state->inputCursorPos = len + 1; }
	}
	return true;
}
