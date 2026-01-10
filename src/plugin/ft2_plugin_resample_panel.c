/*
** FT2 Plugin - Resample Modal Panel
** Resamples current sample by relative halftones (-36 to +36).
** Adjusts sample length and relative note to maintain pitch.
*/

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ft2_plugin_resample_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

#define MAX_SAMPLE_LEN 0x3FFFFFFF
#define RES_STATE(inst) (&FT2_UI(inst)->modalPanels.resample)

static void onResampleClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onTonesScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onTonesDown(ft2_instance_t *inst);
static void onTonesUp(ft2_instance_t *inst);

static ft2_sample_t *getCurrentSample(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	pushButton_t *p;
	scrollBar_t *s;

	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Resample"; p->x = 214; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onResampleClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 345; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	p = &widgets->pushButtons[PB_RES_3]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 314; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onTonesDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 401; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onTonesUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	s = &widgets->scrollBars[SB_RES_1]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 337; s->y = 234; s->w = 64; s->h = 13; s->callbackFunc = onTonesScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 72);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)(state->relReSmp + 36), false);
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++) hideScrollBar(widgets, SB_RES_1 + i);
}

static void onResampleClick(ft2_instance_t *inst) { ft2_resample_panel_apply(inst); }
static void onExitClick(ft2_instance_t *inst) { ft2_resample_panel_hide(inst); }

static void onTonesScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	if (!inst || !inst->ui) return;
	RES_STATE(inst)->relReSmp = (int8_t)((int32_t)pos - 36);
}

static void onTonesDown(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	if (state->relReSmp > -36) state->relReSmp--;
}

static void onTonesUp(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	if (state->relReSmp < 36) state->relReSmp++;
}

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 209, y = 230, w = 217, h = 54;
	resample_panel_state_t *state = RES_STATE(inst);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	int32_t newLen = 0;
	ft2_sample_t *s = getCurrentSample(inst);
	if (s && s->dataPtr)
	{
		double dNewLen = s->length * pow(2.0, state->relReSmp * (1.0 / 12.0));
		if (dNewLen > (double)MAX_SAMPLE_LEN) dNewLen = (double)MAX_SAMPLE_LEN;
		newLen = (int32_t)dNewLen;
	}

	textOutShadow(video, bmp, 215, 236, PAL_FORGRND, PAL_BUTTON2, "Rel. h.tones");
	textOutShadow(video, bmp, 215, 250, PAL_FORGRND, PAL_BUTTON2, "New sample size");
	hexOut(video, bmp, 361, 250, PAL_FORGRND, newLen, 8);

	char sign = (state->relReSmp == 0) ? ' ' : (state->relReSmp < 0) ? '-' : '+';
	uint16_t val = (state->relReSmp < 0) ? -state->relReSmp : state->relReSmp;

	if (val > 9) {
		charOut(video, bmp, 291, 236, PAL_FORGRND, sign);
		charOut(video, bmp, 298, 236, PAL_FORGRND, '0' + (val / 10));
		charOut(video, bmp, 305, 236, PAL_FORGRND, '0' + (val % 10));
	} else {
		charOut(video, bmp, 298, 236, PAL_FORGRND, sign);
		charOut(video, bmp, 305, 236, PAL_FORGRND, '0' + val);
	}
}

static void applyResampleToSample(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || !s->dataPtr || s->length == 0) return;

	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	const double dRatio = pow(2.0, (int32_t)state->relReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dRatio;
	if (dNewLen > (double)MAX_SAMPLE_LEN) dNewLen = (double)MAX_SAMPLE_LEN;
	const uint32_t newLen = (uint32_t)floor(dNewLen);
	if (newLen == 0) return;

	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t padding = FT2_MAX_TAPS * bytesPerSample;
	size_t allocSize = (size_t)(padding + newLen * bytesPerSample + padding);

	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (!newOrigPtr) return;
	int8_t *newData = newOrigPtr + padding;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	const uint64_t delta64 = (uint64_t)round(4294967296.0 / dRatio);
	uint64_t posFrac64 = 0;

	if (sample16Bit)
	{
		const int16_t *src16 = (const int16_t *)s->dataPtr;
		int16_t *dst16 = (int16_t *)newData;
		for (uint32_t i = 0; i < newLen; i++)
		{
			uint32_t srcIdx = (uint32_t)(posFrac64 >> 32);
			if (srcIdx >= (uint32_t)s->length) srcIdx = s->length - 1;
			dst16[i] = src16[srcIdx];
			posFrac64 += delta64;
		}
	}
	else
	{
		const int8_t *src8 = s->dataPtr;
		int8_t *dst8 = newData;
		for (uint32_t i = 0; i < newLen; i++)
		{
			uint32_t srcIdx = (uint32_t)(posFrac64 >> 32);
			if (srcIdx >= (uint32_t)s->length) srcIdx = s->length - 1;
			dst8[i] = src8[srcIdx];
			posFrac64 += delta64;
		}
	}

	if (s->origDataPtr) free(s->origDataPtr);
	s->origDataPtr = newOrigPtr;
	s->dataPtr = newData;
	s->relativeNote += state->relReSmp;
	s->length = newLen;
	s->loopStart = (int32_t)(s->loopStart * dRatio);
	s->loopLength = (int32_t)(s->loopLength * dRatio);

	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void ft2_resample_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || !s->dataPtr || s->length == 0) return;

	resample_panel_state_t *state = RES_STATE(inst);
	state->active = true;
	state->relReSmp = 0;
	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_RESAMPLE);
}

void ft2_resample_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	if (!state->active) return;

	hideWidgets(inst);
	state->active = false;
	inst->uiState.updateSampleEditor = true;
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_RESAMPLE);
}

bool ft2_resample_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return RES_STATE(inst)->active;
}

void ft2_resample_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	resample_panel_state_t *state = RES_STATE(inst);
	if (!state->active) return;

	drawFrame(inst, video, bmp);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	setScrollBarPos(inst, widgets, video, SB_RES_1, (uint32_t)(state->relReSmp + 36), false);

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);

	if (widgets->scrollBarState[SB_RES_1].visible)
		drawScrollBar(widgets, video, SB_RES_1);
}

void ft2_resample_panel_apply(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!RES_STATE(inst)->active) return;
	applyResampleToSample(inst);
	ft2_resample_panel_hide(inst);
}
