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

typedef struct {
	bool active;
	ft2_instance_t *instance;
	int8_t relReSmp;  /* -36 to +36 halftones */
} resample_panel_state_t;

static resample_panel_state_t state = { false, NULL, 0 };

static void onResampleClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onTonesScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onTonesDown(ft2_instance_t *inst);
static void onTonesUp(ft2_instance_t *inst);

/* ------------------------------------------------------------------------- */
/*                              HELPERS                                      */
/* ------------------------------------------------------------------------- */

static ft2_sample_t *getCurrentSample(void)
{
	if (!state.instance)
		return NULL;

	ft2_instance_t *inst = state.instance;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128)
		return NULL;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

/* ------------------------------------------------------------------------- */
/*                           WIDGET SETUP                                    */
/* ------------------------------------------------------------------------- */

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (!inst) return;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	pushButton_t *p;
	scrollBar_t *s;

	/* Resample button */
	p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Resample";
	p->x = 214; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onResampleClick;
	widgets->pushButtonVisible[PB_RES_1] = true;

	/* Exit button */
	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit";
	p->x = 345; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick;
	widgets->pushButtonVisible[PB_RES_2] = true;

	/* Tones decrement arrow */
	p = &widgets->pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 314; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3;
	p->callbackFuncOnDown = onTonesDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	/* Tones increment arrow */
	p = &widgets->pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 401; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3;
	p->callbackFuncOnDown = onTonesUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	/* Tones scrollbar (range 0-72, maps to -36..+36) */
	s = &widgets->scrollBars[SB_RES_1];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 337; s->y = 234; s->w = 64; s->h = 13;
	s->callbackFunc = onTonesScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 72);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)(state.relReSmp + 36), false);
}

static void hideWidgets(void)
{
	ft2_widgets_t *widgets = (state.instance && state.instance->ui) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (!widgets) return;

	for (int i = 0; i < 8; i++)
		hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++)
		hideScrollBar(widgets, SB_RES_1 + i);
}

/* ------------------------------------------------------------------------- */
/*                            CALLBACKS                                      */
/* ------------------------------------------------------------------------- */

static void onResampleClick(ft2_instance_t *inst)  { (void)inst; ft2_resample_panel_apply(); }
static void onExitClick(ft2_instance_t *inst)      { (void)inst; ft2_resample_panel_hide(); }

static void onTonesScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	(void)inst;
	state.relReSmp = (int8_t)((int32_t)pos - 36);
}

static void onTonesDown(ft2_instance_t *inst) { (void)inst; if (state.relReSmp > -36) state.relReSmp--; }
static void onTonesUp(ft2_instance_t *inst)   { (void)inst; if (state.relReSmp < 36) state.relReSmp++; }

/* ------------------------------------------------------------------------- */
/*                              DRAWING                                      */
/* ------------------------------------------------------------------------- */

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 209, y = 230, w = 217, h = 54;

	/* Panel background with 3D borders */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	/* Calculate new length: len * 2^(halftones/12) */
	int32_t newLen = 0;
	ft2_sample_t *s = getCurrentSample();
	if (s && s->dataPtr)
	{
		double dNewLen = s->length * pow(2.0, state.relReSmp * (1.0 / 12.0));
		if (dNewLen > (double)MAX_SAMPLE_LEN) dNewLen = (double)MAX_SAMPLE_LEN;
		newLen = (int32_t)dNewLen;
	}

	textOutShadow(video, bmp, 215, 236, PAL_FORGRND, PAL_BUTTON2, "Rel. h.tones");
	textOutShadow(video, bmp, 215, 250, PAL_FORGRND, PAL_BUTTON2, "New sample size");
	hexOut(video, bmp, 361, 250, PAL_FORGRND, newLen, 8);

	/* Signed halftone value display */
	char sign = (state.relReSmp == 0) ? ' ' : (state.relReSmp < 0) ? '-' : '+';
	uint16_t val = (state.relReSmp < 0) ? -state.relReSmp : state.relReSmp;

	if (val > 9)
	{
		charOut(video, bmp, 291, 236, PAL_FORGRND, sign);
		charOut(video, bmp, 298, 236, PAL_FORGRND, '0' + (val / 10));
		charOut(video, bmp, 305, 236, PAL_FORGRND, '0' + (val % 10));
	}
	else
	{
		charOut(video, bmp, 298, 236, PAL_FORGRND, sign);
		charOut(video, bmp, 305, 236, PAL_FORGRND, '0' + val);
	}
}

/* ------------------------------------------------------------------------- */
/*                         RESAMPLE ALGORITHM                                */
/* ------------------------------------------------------------------------- */

/*
** Resamples using nearest-neighbor interpolation with 32.32 fixed-point stepping.
** New length = old length * 2^(halftones/12).
** Adjusts relativeNote to compensate so pitch is preserved on playback.
*/
static void applyResampleToSample(void)
{
	if (!state.instance) return;

	ft2_instance_t *inst = state.instance;
	ft2_sample_t *s = getCurrentSample();
	if (!s || !s->dataPtr || s->length == 0) return;

	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	const double dRatio = pow(2.0, (int32_t)state.relReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dRatio;
	if (dNewLen > (double)MAX_SAMPLE_LEN) dNewLen = (double)MAX_SAMPLE_LEN;
	const uint32_t newLen = (uint32_t)floor(dNewLen);
	if (newLen == 0) return;

	/* Allocate with padding for interpolation taps */
	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t padding = FT2_MAX_TAPS * bytesPerSample;
	size_t allocSize = (size_t)(padding + newLen * bytesPerSample + padding);

	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (!newOrigPtr) return;
	int8_t *newData = newOrigPtr + padding;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	/* 32.32 fixed-point resampling */
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

	s->relativeNote += state.relReSmp;
	s->length = newLen;
	s->loopStart = (int32_t)(s->loopStart * dRatio);
	s->loopLength = (int32_t)(s->loopLength * dRatio);

	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* ------------------------------------------------------------------------- */
/*                            PUBLIC API                                     */
/* ------------------------------------------------------------------------- */

void ft2_resample_panel_show(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_sample_t *s = NULL;
	if (inst->editor.curInstr != 0 && inst->editor.curInstr <= 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
		if (instr) s = &instr->smp[inst->editor.curSmp];
	}

	if (!s || !s->dataPtr || s->length == 0)
		return;

	state.active = true;
	state.instance = inst;
	state.relReSmp = 0;

	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_RESAMPLE);
}

void ft2_resample_panel_hide(void)
{
	if (!state.active) return;

	hideWidgets();
	state.active = false;
	if (state.instance) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_RESAMPLE);
}

bool ft2_resample_panel_is_active(void) { return state.active; }

void ft2_resample_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || !video) return;

	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance && state.instance->ui) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (!widgets) return;

	setScrollBarPos(state.instance, widgets, video, SB_RES_1, (uint32_t)(state.relReSmp + 36), false);

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);

	if (widgets->scrollBarState[SB_RES_1].visible)
		drawScrollBar(widgets, video, SB_RES_1);
}

void ft2_resample_panel_apply(void)
{
	if (!state.active) return;
	applyResampleToSample();
	ft2_resample_panel_hide();
}

int8_t ft2_resample_panel_get_tones(void)      { return state.relReSmp; }
void ft2_resample_panel_set_tones(int8_t tones) { state.relReSmp = tones; }

