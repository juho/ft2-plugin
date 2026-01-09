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

/* Persistent cutoff values per filter type */
static int32_t lastLpCutoff = 2000;
static int32_t lastHpCutoff = 200;

static void onOKClick(ft2_instance_t *inst);
static void onCancelClick(ft2_instance_t *inst);

/* ---------- Widget setup ---------- */

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL) return;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	pushButton_t *p;

	p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "OK"; p->x = 246; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onOKClick;
	widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel"; p->x = 346; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onCancelClick;
	widgets->pushButtonVisible[PB_RES_2] = true;
}

static void hideWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	ft2_widgets_t *widgets = (inst != NULL && inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
}

/* ---------- Filter application ---------- */

static void applyFilter(void)
{
	if (!state.active || state.instance == NULL) return;

	int32_t cutoff = atoi(state.inputBuffer);
	if (cutoff < 1 || cutoff > 99999) return;

	if (state.filterType == FILTER_TYPE_LOWPASS)
		lastLpCutoff = cutoff;
	else
		lastHpCutoff = cutoff;

	smpfx_apply_filter(state.instance, state.filterType == FILTER_TYPE_LOWPASS ? 0 : 1, cutoff);
}

/* ---------- Callbacks ---------- */

static void onOKClick(ft2_instance_t *inst) { (void)inst; applyFilter(); ft2_filter_panel_hide(); }
static void onCancelClick(ft2_instance_t *inst) { (void)inst; ft2_filter_panel_hide(); }

/* ---------- Drawing ---------- */

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 146, y = 249, w = 380, h = 67;

	/* 3D beveled frame with title bar */
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

	/* Headline */
	const char *headline = (state.filterType == FILTER_TYPE_LOWPASS)
		? "Enter low-pass filter cutoff (in Hz):"
		: "Enter high-pass filter cutoff (in Hz):";
	int hlen = textWidth(headline);
	textOut(video, bmp, (632 - hlen) / 2, y + 4, PAL_FORGRND, headline);

	/* Input field */
	int inputX = x + 10, inputY = y + 28, inputW = w - 20, inputH = 12;
	fillRect(video, inputX, inputY, inputW, inputH, PAL_DESKTOP);
	hLine(video, inputX, inputY, inputW, PAL_BUTTON2);
	vLine(video, inputX, inputY, inputH, PAL_BUTTON2);
	hLine(video, inputX, inputY + inputH - 1, inputW, PAL_BUTTON1);
	vLine(video, inputX + inputW - 1, inputY, inputH, PAL_BUTTON1);
	textOut(video, bmp, inputX + 2, inputY + 2, PAL_FORGRND, state.inputBuffer);

	/* Cursor */
	int cursorX = inputX + 2 + textWidth(state.inputBuffer);
	vLine(video, cursorX, inputY + 2, 8, PAL_FORGRND);
}

/* ---------- Public API ---------- */

void ft2_filter_panel_show(ft2_instance_t *inst, filter_type_t filterType)
{
	if (inst == NULL || inst->editor.curInstr == 0) return;

	state.active = true;
	state.instance = inst;
	state.filterType = filterType;

	/* Initialize with last used value for this filter type */
	snprintf(state.inputBuffer, sizeof(state.inputBuffer), "%d",
	         (filterType == FILTER_TYPE_LOWPASS) ? lastLpCutoff : lastHpCutoff);
	state.inputCursorPos = (int)strlen(state.inputBuffer);

	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_FILTER);
}

void ft2_filter_panel_hide(void)
{
	if (!state.active) return;
	hideWidgets();
	state.active = false;
	if (state.instance != NULL) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_FILTER);
}

bool ft2_filter_panel_is_active(void) { return state.active; }

void ft2_filter_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL) return;
	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance != NULL && state.instance->ui != NULL) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (widgets == NULL) return;

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i]) drawPushButton(widgets, video, bmp, PB_RES_1 + i);
}

/* ---------- Input handling ---------- */

bool ft2_filter_panel_key_down(int keycode)
{
	if (!state.active) return false;

	if (keycode == FT2_KEY_RETURN) { applyFilter(); ft2_filter_panel_hide(); return true; }
	if (keycode == FT2_KEY_ESCAPE) { ft2_filter_panel_hide(); return true; }
	if (keycode == FT2_KEY_BACKSPACE) {
		int len = (int)strlen(state.inputBuffer);
		if (len > 0) { state.inputBuffer[len - 1] = '\0'; state.inputCursorPos = len - 1; }
		return true;
	}
	return true;
}

bool ft2_filter_panel_char_input(char c)
{
	if (!state.active) return false;
	/* Digits only, max 5 chars (99999 Hz) */
	if (c >= '0' && c <= '9') {
		int len = (int)strlen(state.inputBuffer);
		if (len < 5) { state.inputBuffer[len] = c; state.inputBuffer[len + 1] = '\0'; state.inputCursorPos = len + 1; }
	}
	return true;
}

ft2_instance_t *ft2_filter_panel_get_instance(void) { return state.instance; }

