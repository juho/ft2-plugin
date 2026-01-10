/**
 * @file ft2_plugin_echo_panel.c
 * @brief Echo effect modal panel for sample editor.
 *
 * Parameters:
 *   - Number of echoes (0-64)
 *   - Distance (0-16384, *16 = actual samples between echoes)
 *   - Fade out (0-100%, volume multiplier per echo)
 *   - Add memory: if true, extends sample length to fit all echoes
 */

#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_echo_panel.h"
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
#define ECHO_STATE(inst) (&FT2_UI(inst)->modalPanels.echo)

/* Right-aligned 3-digit decimal strings for display */
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

static void onCreateClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onEchoNumScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onEchoDistScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onEchoVolScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onEchoNumDown(ft2_instance_t *inst);
static void onEchoNumUp(ft2_instance_t *inst);
static void onEchoDistDown(ft2_instance_t *inst);
static void onEchoDistUp(ft2_instance_t *inst);
static void onEchoVolDown(ft2_instance_t *inst);
static void onEchoVolUp(ft2_instance_t *inst);

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
	echo_panel_state_t *state = ECHO_STATE(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	pushButton_t *p;
	scrollBar_t *s;

	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Create"; p->x = 345; p->y = 266; p->w = 56; p->h = 16;
	p->callbackFuncOnUp = onCreateClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 402; p->y = 266; p->w = 55; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	p = &widgets->pushButtons[PB_RES_3]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 224; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoNumDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 224; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoNumUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	p = &widgets->pushButtons[PB_RES_5]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 238; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoDistDown;
	widgets->pushButtonVisible[PB_RES_5] = true;

	p = &widgets->pushButtons[PB_RES_6]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 238; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoDistUp;
	widgets->pushButtonVisible[PB_RES_6] = true;

	p = &widgets->pushButtons[PB_RES_7]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 252; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoVolDown;
	widgets->pushButtonVisible[PB_RES_7] = true;

	p = &widgets->pushButtons[PB_RES_8]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 252; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoVolUp;
	widgets->pushButtonVisible[PB_RES_8] = true;

	s = &widgets->scrollBars[SB_RES_1]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 224; s->w = 64; s->h = 13; s->callbackFunc = onEchoNumScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 64);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)state->echoNum, false);

	s = &widgets->scrollBars[SB_RES_2]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 238; s->w = 64; s->h = 13; s->callbackFunc = onEchoDistScrollbar;
	widgets->scrollBarState[SB_RES_2].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_2, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_2, 16384);
	setScrollBarPos(inst, widgets, NULL, SB_RES_2, (uint32_t)state->echoDistance, false);

	s = &widgets->scrollBars[SB_RES_3]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 252; s->w = 64; s->h = 13; s->callbackFunc = onEchoVolScrollbar;
	widgets->scrollBarState[SB_RES_3].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_3, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_3, 100);
	setScrollBarPos(inst, widgets, NULL, SB_RES_3, (uint32_t)state->echoVolChange, false);
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++) hideScrollBar(widgets, SB_RES_1 + i);
}

static void onCreateClick(ft2_instance_t *inst) { ft2_echo_panel_apply(inst); }
static void onExitClick(ft2_instance_t *inst) { ft2_echo_panel_hide(inst); }

static void onEchoNumScrollbar(ft2_instance_t *inst, uint32_t pos) { if (inst && inst->ui) ECHO_STATE(inst)->echoNum = (int16_t)pos; }
static void onEchoDistScrollbar(ft2_instance_t *inst, uint32_t pos) { if (inst && inst->ui) ECHO_STATE(inst)->echoDistance = (int32_t)pos; }
static void onEchoVolScrollbar(ft2_instance_t *inst, uint32_t pos) { if (inst && inst->ui) ECHO_STATE(inst)->echoVolChange = (int16_t)pos; }

static void onEchoNumDown(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoNum > 0) s->echoNum--; } }
static void onEchoNumUp(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoNum < 64) s->echoNum++; } }
static void onEchoDistDown(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoDistance > 0) s->echoDistance--; } }
static void onEchoDistUp(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoDistance < 16384) s->echoDistance++; } }
static void onEchoVolDown(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoVolChange > 0) s->echoVolChange--; } }
static void onEchoVolUp(ft2_instance_t *inst) { if (inst && inst->ui) { echo_panel_state_t *s = ECHO_STATE(inst); if (s->echoVolChange < 100) s->echoVolChange++; } }

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 171, y = 220, w = 291, h = 66;
	echo_panel_state_t *state = ECHO_STATE(inst);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(video, bmp, 177, 226, PAL_FORGRND, PAL_BUTTON2, "Number of echoes");
	textOutShadow(video, bmp, 177, 240, PAL_FORGRND, PAL_BUTTON2, "Echo distance");
	textOutShadow(video, bmp, 177, 254, PAL_FORGRND, PAL_BUTTON2, "Fade out");
	textOutShadow(video, bmp, 192, 270, PAL_FORGRND, PAL_BUTTON2, "Add memory to sample");

	charOut(video, bmp, 315 + (2 * 7), 226, PAL_FORGRND, '0' + (char)(state->echoNum / 10));
	charOut(video, bmp, 315 + (3 * 7), 226, PAL_FORGRND, '0' + (state->echoNum % 10));
	hexOut(video, bmp, 308, 240, PAL_FORGRND, state->echoDistance << 4, 5);
	if (state->echoVolChange <= 100)
		textOutFixed(video, bmp, 312, 254, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[state->echoVolChange]);
	charOutShadow(video, bmp, 313 + (3 * 7), 254, PAL_FORGRND, PAL_BUTTON2, '%');

	fillRect(video, 176, 268, 12, 12, PAL_DESKTOP);
	hLine(video, 176, 268, 12, PAL_BUTTON2);
	vLine(video, 176, 268, 12, PAL_BUTTON2);
	hLine(video, 176, 279, 12, PAL_BUTTON1);
	vLine(video, 187, 268, 12, PAL_BUTTON1);
	if (state->echoAddMemory) charOut(video, bmp, 178, 270, PAL_FORGRND, 'x');
}

static void applyEchoToSample(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	echo_panel_state_t *state = ECHO_STATE(inst);
	if (state->echoNum < 1) return;

	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || !s->dataPtr || s->length == 0) return;

	int32_t readLen = s->length;
	int8_t *readPtr = s->dataPtr;
	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	int32_t distance = state->echoDistance * 16;
	double dVolChange = state->echoVolChange / 100.0;

	double dSmp = sample16Bit ? 32768.0 : 128.0;
	int32_t k = 0;
	while (k < state->echoNum && dSmp >= 1.0) { dSmp *= dVolChange; k++; }
	int32_t nEchoes = k + 1;
	if (nEchoes < 1) return;

	int32_t writeLen = readLen;
	if (state->echoAddMemory) {
		int64_t tmp64 = (int64_t)distance * (nEchoes - 1) + writeLen;
		if (tmp64 > MAX_SAMPLE_LEN) tmp64 = MAX_SAMPLE_LEN;
		writeLen = (int32_t)tmp64;
	}

	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
	size_t allocSize = (size_t)(leftPadding + writeLen * bytesPerSample + rightPadding);

	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (!newOrigPtr) return;
	int8_t *newData = newOrigPtr + leftPadding;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);
	
	int32_t writeIdx = 0;
	
	if (sample16Bit)
	{
		const int16_t *readPtr16 = (const int16_t *)readPtr;
		int16_t *writePtr16 = (int16_t *)newData;
		
		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0, dSmpMul = 1.0;
			int32_t echoRead = writeIdx, echoCycle = nEchoes;
			
			while (echoCycle > 0) {
				if (echoRead >= 0 && echoRead < readLen) dSmpOut += (double)readPtr16[echoRead] * dSmpMul;
				dSmpMul *= dVolChange; echoRead -= distance;
				if (echoRead < 0) break;
				echoCycle--;
			}
			
			int32_t smp32 = (int32_t)dSmpOut;
			if (smp32 < -32768) smp32 = -32768;
			if (smp32 > 32767) smp32 = 32767;
			writePtr16[writeIdx++] = (int16_t)smp32;
		}
	}
	else
	{
		int8_t *writePtr8 = newData;
		
		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0, dSmpMul = 1.0;
			int32_t echoRead = writeIdx, echoCycle = nEchoes;
			
			while (echoCycle > 0) {
				if (echoRead >= 0 && echoRead < readLen) dSmpOut += (double)readPtr[echoRead] * dSmpMul;
				dSmpMul *= dVolChange; echoRead -= distance;
				if (echoRead < 0) break;
				echoCycle--;
			}
			
			int32_t smp32 = (int32_t)dSmpOut;
			if (smp32 < -128) smp32 = -128;
			if (smp32 > 127) smp32 = 127;
			writePtr8[writeIdx++] = (int8_t)smp32;
		}
	}
	
	if (s->origDataPtr) free(s->origDataPtr);
	s->origDataPtr = newOrigPtr;
	s->dataPtr = newData;
	s->length = writeLen;

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void ft2_echo_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || !s->dataPtr || s->length == 0) return;

	echo_panel_state_t *state = ECHO_STATE(inst);
	state->active = true;
	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_ECHO);
}

void ft2_echo_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	echo_panel_state_t *state = ECHO_STATE(inst);
	if (!state->active) return;
	
	hideWidgets(inst);
	state->active = false;
	inst->uiState.updateSampleEditor = true;
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_ECHO);
}

bool ft2_echo_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return ECHO_STATE(inst)->active;
}

void ft2_echo_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	echo_panel_state_t *state = ECHO_STATE(inst);
	if (!state->active) return;

	drawFrame(inst, video, bmp);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	setScrollBarPos(inst, widgets, video, SB_RES_1, (uint32_t)state->echoNum, false);
	setScrollBarPos(inst, widgets, video, SB_RES_2, (uint32_t)state->echoDistance, false);
	setScrollBarPos(inst, widgets, video, SB_RES_3, (uint32_t)state->echoVolChange, false);

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i]) drawPushButton(widgets, video, bmp, PB_RES_1 + i);
	for (int i = 0; i < 3; i++)
		if (widgets->scrollBarState[SB_RES_1 + i].visible) drawScrollBar(widgets, video, SB_RES_1 + i);
}

void ft2_echo_panel_apply(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!ECHO_STATE(inst)->active) return;
	applyEchoToSample(inst);
	ft2_echo_panel_hide(inst);
}

bool ft2_echo_panel_mouse_down(ft2_instance_t *inst, int32_t x, int32_t y, int button)
{
	if (!inst || !inst->ui || button != 1) return false;
	echo_panel_state_t *state = ECHO_STATE(inst);
	if (!state->active) return false;
	
	if (x >= 176 && x < 188 && y >= 268 && y < 280) {
		state->echoAddMemory = !state->echoAddMemory;
		return true;
	}
	return false;
}
