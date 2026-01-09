/**
 * @file ft2_plugin_mix_panel.c
 * @brief Sample mixing modal panel.
 *
 * Mixes source sample (srcInstr/srcSmp) into destination sample (curInstr/curSmp)
 * with adjustable balance. Result is placed in destination, extending length if
 * source is longer. Output bit depth is the higher of the two inputs.
 */

#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_mix_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

static const char *dec3StrTab[101] = {
	"  0","  1","  2","  3","  4","  5","  6","  7","  8","  9",
	" 10"," 11"," 12"," 13"," 14"," 15"," 16"," 17"," 18"," 19",
	" 20"," 21"," 22"," 23"," 24"," 25"," 26"," 27"," 28"," 29",
	" 30"," 31"," 32"," 33"," 34"," 35"," 36"," 37"," 38"," 39",
	" 40"," 41"," 42"," 43"," 44"," 45"," 46"," 47"," 48"," 49",
	" 50"," 51"," 52"," 53"," 54"," 55"," 56"," 57"," 58"," 59",
	" 60"," 61"," 62"," 63"," 64"," 65"," 66"," 67"," 68"," 69",
	" 70"," 71"," 72"," 73"," 74"," 75"," 76"," 77"," 78"," 79",
	" 80"," 81"," 82"," 83"," 84"," 85"," 86"," 87"," 88"," 89",
	" 90"," 91"," 92"," 93"," 94"," 95"," 96"," 97"," 98"," 99",
	"100"
};

typedef struct {
	bool active;
	ft2_instance_t *instance;
	int8_t mixBalance; /* 0=all source, 100=all destination */
} mix_panel_state_t;

static mix_panel_state_t state = { .active = false, .instance = NULL, .mixBalance = 50 };

static void onMixClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onBalanceDown(ft2_instance_t *inst);
static void onBalanceUp(ft2_instance_t *inst);

/* ---------- Widget setup ---------- */

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL) return;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	pushButton_t *p;
	scrollBar_t *s;

	p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Mix"; p->x = 197; p->y = 258; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onMixClick;
	widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 361; p->y = 258; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick;
	widgets->pushButtonVisible[PB_RES_2] = true;

	p = &widgets->pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 322; p->y = 244; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3;
	p->callbackFuncOnDown = onBalanceDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 411; p->y = 244; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3;
	p->callbackFuncOnDown = onBalanceUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	s = &widgets->scrollBars[SB_RES_1];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 345; s->y = 244; s->w = 66; s->h = 13;
	s->callbackFunc = onBalanceScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 100);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)state.mixBalance, false);
}

static void hideWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	ft2_widgets_t *widgets = (inst != NULL && inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++) hideScrollBar(widgets, SB_RES_1 + i);
}

/* ---------- Callbacks ---------- */

static void onMixClick(ft2_instance_t *inst) { (void)inst; ft2_mix_panel_apply(); }
static void onExitClick(ft2_instance_t *inst) { (void)inst; ft2_mix_panel_hide(); }
static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.mixBalance = (int8_t)pos; }
static void onBalanceDown(ft2_instance_t *inst) { (void)inst; if (state.mixBalance > 0) state.mixBalance--; }
static void onBalanceUp(ft2_instance_t *inst) { (void)inst; if (state.mixBalance < 100) state.mixBalance++; }

/* ---------- Drawing ---------- */

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 192, y = 240, w = 248, h = 38;

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	/* Outer border (raised) */
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);

	/* Inner border (sunken) */
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(video, bmp, 198, 246, PAL_FORGRND, PAL_BUTTON2, "Mixing balance");
	if (state.mixBalance <= 100)
		textOutFixed(video, bmp, 299, 246, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[state.mixBalance]);
}

/* ---------- Mix algorithm ---------- */

/* Mix source sample into destination with balance weighting.
 * dMixA = source weight (0-1), dMixB = dest weight (0-1).
 * Output length = max(srcLen, dstLen), bit depth = max(srcBits, dstBits). */
static void applyMixToSample(void)
{
	if (state.instance == NULL) return;
	ft2_instance_t *inst = state.instance;

	int16_t dstIns = inst->editor.curInstr, dstSmp = inst->editor.curSmp;
	int16_t mixIns = inst->editor.srcInstr, mixSmp = inst->editor.srcSmp;

	if (dstIns == 0 || dstIns > 128) return;
	if (dstIns == mixIns && dstSmp == mixSmp) return;

	ft2_instr_t *dstInstr = inst->replayer.instr[dstIns];
	if (dstInstr == NULL) return;

	ft2_sample_t *s = &dstInstr->smp[dstSmp];

	int8_t *dstPtr = NULL; uint8_t dstFlags = 0; int32_t dstLen = 0;
	if (s->dataPtr != NULL) { dstPtr = s->dataPtr; dstLen = s->length; dstFlags = s->flags; }

	int8_t *mixPtr = NULL; uint8_t mixFlags = 0; int32_t mixLen = 0;
	if (mixIns > 0 && mixIns <= 128) {
		ft2_instr_t *mixInstr = inst->replayer.instr[mixIns];
		if (mixInstr != NULL) {
			ft2_sample_t *mixSample = &mixInstr->smp[mixSmp];
			if (mixSample->dataPtr != NULL) {
				mixPtr = mixSample->dataPtr; mixLen = mixSample->length; mixFlags = mixSample->flags;
			}
		}
	}

	if (dstPtr == NULL && mixPtr == NULL) return;

	int32_t newLen = (dstLen > mixLen) ? dstLen : mixLen;
	bool newIs16Bit = ((dstFlags | mixFlags) & FT2_SAMPLE_16BIT) != 0;

	/* Allocate with interpolation padding */
	int32_t bytesPerSample = newIs16Bit ? 2 : 1;
	int32_t padding = FT2_MAX_TAPS * bytesPerSample;
	size_t allocSize = (size_t)(padding * 2 + newLen * bytesPerSample);

	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (newOrigPtr == NULL) return;
	int8_t *newData = newOrigPtr + padding;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	double dMixA = (100 - state.mixBalance) / 100.0;
	double dMixB = state.mixBalance / 100.0;
	bool dstIs16 = (dstFlags & FT2_SAMPLE_16BIT) != 0;
	bool mixIs16 = (mixFlags & FT2_SAMPLE_16BIT) != 0;

	if (newIs16Bit) {
		int16_t *newPtr16 = (int16_t *)newData;
		for (int32_t i = 0; i < newLen; i++) {
			double dSmpA = 0.0, dSmpB = 0.0;
			if (i < mixLen && mixPtr) dSmpA = mixIs16 ? ((int16_t *)mixPtr)[i] : mixPtr[i] * 256.0;
			if (i < dstLen && dstPtr) dSmpB = dstIs16 ? ((int16_t *)dstPtr)[i] : dstPtr[i] * 256.0;
			int32_t smp32 = (int32_t)(dSmpA * dMixA + dSmpB * dMixB);
			if (smp32 < -32768) smp32 = -32768; else if (smp32 > 32767) smp32 = 32767;
			newPtr16[i] = (int16_t)smp32;
		}
		s->flags |= FT2_SAMPLE_16BIT;
	} else {
		for (int32_t i = 0; i < newLen; i++) {
			double dSmpA = (i < mixLen && mixPtr) ? mixPtr[i] : 0.0;
			double dSmpB = (i < dstLen && dstPtr) ? dstPtr[i] : 0.0;
			int32_t smp32 = (int32_t)(dSmpA * dMixA + dSmpB * dMixB);
			if (smp32 < -128) smp32 = -128; else if (smp32 > 127) smp32 = 127;
			newData[i] = (int8_t)smp32;
		}
	}

	free(s->origDataPtr);
	s->origDataPtr = newOrigPtr;
	s->dataPtr = newData;
	s->length = newLen;

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* ---------- Public API ---------- */

void ft2_mix_panel_show(ft2_instance_t *inst)
{
	if (inst == NULL || inst->editor.curInstr == 0) return;
	state.active = true;
	state.instance = inst;
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_MIX);
}

void ft2_mix_panel_hide(void)
{
	if (!state.active) return;
	hideWidgets();
	state.active = false;
	if (state.instance) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_MIX);
}

bool ft2_mix_panel_is_active(void) { return state.active; }

void ft2_mix_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL) return;
	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance && state.instance->ui) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (widgets == NULL) return;

	setScrollBarPos(state.instance, widgets, video, SB_RES_1, (uint32_t)state.mixBalance, false);
	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
	if (widgets->scrollBarState[SB_RES_1].visible)
		drawScrollBar(widgets, video, SB_RES_1);
}

void ft2_mix_panel_apply(void)
{
	if (!state.active) return;
	applyMixToSample();
	ft2_mix_panel_hide();
}

int8_t ft2_mix_panel_get_balance(void) { return state.mixBalance; }
void ft2_mix_panel_set_balance(int8_t balance) { state.mixBalance = balance; }

