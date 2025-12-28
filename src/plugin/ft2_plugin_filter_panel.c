/**
 * @file ft2_plugin_filter_panel.c
 * @brief Filter cutoff input panel implementation
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
#include "../ft2_instance.h"

/* Panel state */
typedef struct {
	bool active;
	ft2_instance_t *instance;
	filter_type_t filterType;
	char inputBuffer[16];
	int inputCursorPos;
} filter_panel_state_t;

static filter_panel_state_t state = {
	.active = false,
	.instance = NULL,
	.filterType = FILTER_TYPE_LOWPASS,
	.inputBuffer = "2000",
	.inputCursorPos = 4
};

/* Persistent values */
static int32_t lastLpCutoff = 2000;
static int32_t lastHpCutoff = 200;

/* Forward declarations */
static void onOKClick(ft2_instance_t *inst);
static void onCancelClick(ft2_instance_t *inst);

/* Widget setup */
static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL)
		return;
	
	pushButton_t *p;
	
	/* OK button */
	p = &pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "OK";
	p->x = 246;
	p->y = 291;
	p->w = 80;
	p->h = 16;
	p->callbackFuncOnUp = onOKClick;
	p->visible = true;
	
	/* Cancel button */
	p = &pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel";
	p->x = 346;
	p->y = 291;
	p->w = 80;
	p->h = 16;
	p->callbackFuncOnUp = onCancelClick;
	p->visible = true;
}

static void hideWidgets(void)
{
	for (int i = 0; i < 8; i++)
		hidePushButton(PB_RES_1 + i);
}

/* Apply filter */
static void applyFilter(void)
{
	if (!state.active || state.instance == NULL)
		return;
	
	int32_t cutoff = atoi(state.inputBuffer);
	if (cutoff < 1 || cutoff > 99999)
		return;
	
	/* Store last used value */
	if (state.filterType == FILTER_TYPE_LOWPASS)
		lastLpCutoff = cutoff;
	else
		lastHpCutoff = cutoff;
	
	/* Call the smpfx filter functions */
	smpfx_apply_filter(state.instance, state.filterType == FILTER_TYPE_LOWPASS ? 0 : 1, cutoff);
}

/* Widget callbacks */
static void onOKClick(ft2_instance_t *inst)
{
	(void)inst;
	applyFilter();
	ft2_filter_panel_hide();
}

static void onCancelClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_filter_panel_hide();
}

/* Draw the panel */
static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 146;
	const int16_t y = 249;
	const int16_t w = 380;
	const int16_t h = 67;
	
	/* Main fill */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	
	/* Outer border */
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	
	/* Inner border */
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);
	
	/* Title bottom line */
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);
	
	/* Headline */
	const char *headline;
	if (state.filterType == FILTER_TYPE_LOWPASS)
		headline = "Enter low-pass filter cutoff (in Hz):";
	else
		headline = "Enter high-pass filter cutoff (in Hz):";
	
	int hlen = textWidth(headline);
	int headlineX = (632 - hlen) / 2;
	textOut(video, bmp, headlineX, y + 4, PAL_FORGRND, headline);
	
	/* Input field */
	int inputX = x + 10;
	int inputY = y + 28;
	int inputW = w - 20;
	int inputH = 12;
	
	/* Input box background */
	fillRect(video, inputX, inputY, inputW, inputH, PAL_DESKTOP);
	
	/* Input box border */
	hLine(video, inputX, inputY, inputW, PAL_BUTTON2);
	vLine(video, inputX, inputY, inputH, PAL_BUTTON2);
	hLine(video, inputX, inputY + inputH - 1, inputW, PAL_BUTTON1);
	vLine(video, inputX + inputW - 1, inputY, inputH, PAL_BUTTON1);
	
	/* Input text */
	textOut(video, bmp, inputX + 2, inputY + 2, PAL_FORGRND, state.inputBuffer);
	
	/* Cursor */
	int cursorX = inputX + 2 + textWidth(state.inputBuffer);
	vLine(video, cursorX, inputY + 2, 8, PAL_FORGRND);
}

/* Public API */

void ft2_filter_panel_show(ft2_instance_t *inst, filter_type_t filterType)
{
	if (inst == NULL)
		return;
	
	if (inst->editor.curInstr == 0)
		return;
	
	state.active = true;
	state.instance = inst;
	state.filterType = filterType;
	
	/* Initialize input with last cutoff value */
	if (filterType == FILTER_TYPE_LOWPASS)
		snprintf(state.inputBuffer, sizeof(state.inputBuffer), "%d", lastLpCutoff);
	else
		snprintf(state.inputBuffer, sizeof(state.inputBuffer), "%d", lastHpCutoff);
	
	state.inputCursorPos = (int)strlen(state.inputBuffer);
	
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_FILTER);
}

void ft2_filter_panel_hide(void)
{
	if (!state.active)
		return;
	
	hideWidgets();
	state.active = false;
	
	if (state.instance != NULL)
		state.instance->uiState.updateSampleEditor = true;
	
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_FILTER);
}

bool ft2_filter_panel_is_active(void)
{
	return state.active;
}

void ft2_filter_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL)
		return;
	
	drawFrame(video, bmp);
	
	/* Draw buttons */
	for (int i = 0; i < 8; i++)
	{
		if (pushButtons[PB_RES_1 + i].visible)
			drawPushButton(video, bmp, PB_RES_1 + i);
	}
}

bool ft2_filter_panel_key_down(int keycode)
{
	if (!state.active)
		return false;
	
	/* Enter confirms */
	if (keycode == FT2_KEY_RETURN)
	{
		applyFilter();
		ft2_filter_panel_hide();
		return true;
	}
	
	/* Escape cancels */
	if (keycode == FT2_KEY_ESCAPE)
	{
		ft2_filter_panel_hide();
		return true;
	}
	
	/* Backspace */
	if (keycode == FT2_KEY_BACKSPACE)
	{
		int len = (int)strlen(state.inputBuffer);
		if (len > 0)
		{
			state.inputBuffer[len - 1] = '\0';
			state.inputCursorPos = len - 1;
		}
		return true;
	}
	
	return true;
}

bool ft2_filter_panel_char_input(char c)
{
	if (!state.active)
		return false;
	
	/* Only accept digits */
	if (c >= '0' && c <= '9')
	{
		int len = (int)strlen(state.inputBuffer);
		if (len < 5) /* Max 5 digits (99999) */
		{
			state.inputBuffer[len] = c;
			state.inputBuffer[len + 1] = '\0';
			state.inputCursorPos = len + 1;
		}
	}
	
	return true;
}

ft2_instance_t *ft2_filter_panel_get_instance(void)
{
	return state.instance;
}

