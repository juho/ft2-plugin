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

typedef struct {
	bool active;
	ft2_instance_t *instance;
	int16_t echoNum;       /* 0-64 */
	int32_t echoDistance;  /* 0-16384 (*16 = samples) */
	int16_t echoVolChange; /* 0-100% fade per echo */
	bool echoAddMemory;    /* Extend sample to fit echoes */
} echo_panel_state_t;

static echo_panel_state_t state = {
	.active = false,
	.instance = NULL,
	.echoNum = 1,
	.echoDistance = 0x100,
	.echoVolChange = 30,
	.echoAddMemory = false
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

static ft2_sample_t *getCurrentSample(void)
{
	if (state.instance == NULL) return NULL;
	ft2_instance_t *inst = state.instance;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (instr == NULL) return NULL;
	return &instr->smp[inst->editor.curSmp];
}

/* ---------- Widget setup ---------- */

/* Uses reserved widget slots PB_RES_1..8 and SB_RES_1..3 */
static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL) return;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	pushButton_t *p;
	scrollBar_t *s;

	/* Create/Exit buttons */
	p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Create"; p->x = 345; p->y = 266; p->w = 56; p->h = 16;
	p->callbackFuncOnUp = onCreateClick;
	widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 402; p->y = 266; p->w = 55; p->h = 16;
	p->callbackFuncOnUp = onExitClick;
	widgets->pushButtonVisible[PB_RES_2] = true;

	/* Echo num arrows */
	p = &widgets->pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 224; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoNumDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 224; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoNumUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	/* Distance arrows */
	p = &widgets->pushButtons[PB_RES_5];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 238; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoDistDown;
	widgets->pushButtonVisible[PB_RES_5] = true;

	p = &widgets->pushButtons[PB_RES_6];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 238; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoDistUp;
	widgets->pushButtonVisible[PB_RES_6] = true;

	/* Fade out arrows */
	p = &widgets->pushButtons[PB_RES_7];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 345; p->y = 252; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoVolDown;
	widgets->pushButtonVisible[PB_RES_7] = true;

	p = &widgets->pushButtons[PB_RES_8];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 432; p->y = 252; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEchoVolUp;
	widgets->pushButtonVisible[PB_RES_8] = true;

	/* Scrollbars */
	s = &widgets->scrollBars[SB_RES_1];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 224; s->w = 64; s->h = 13; s->callbackFunc = onEchoNumScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 64);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)state.echoNum, false);

	s = &widgets->scrollBars[SB_RES_2];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 238; s->w = 64; s->h = 13; s->callbackFunc = onEchoDistScrollbar;
	widgets->scrollBarState[SB_RES_2].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_2, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_2, 16384);
	setScrollBarPos(inst, widgets, NULL, SB_RES_2, (uint32_t)state.echoDistance, false);

	s = &widgets->scrollBars[SB_RES_3];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 368; s->y = 252; s->w = 64; s->h = 13; s->callbackFunc = onEchoVolScrollbar;
	widgets->scrollBarState[SB_RES_3].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_3, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_3, 100);
	setScrollBarPos(inst, widgets, NULL, SB_RES_3, (uint32_t)state.echoVolChange, false);
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

static void onCreateClick(ft2_instance_t *inst) { (void)inst; ft2_echo_panel_apply(); }
static void onExitClick(ft2_instance_t *inst) { (void)inst; ft2_echo_panel_hide(); }

static void onEchoNumScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.echoNum = (int16_t)pos; }
static void onEchoDistScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.echoDistance = (int32_t)pos; }
static void onEchoVolScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.echoVolChange = (int16_t)pos; }

static void onEchoNumDown(ft2_instance_t *inst) { (void)inst; if (state.echoNum > 0) state.echoNum--; }
static void onEchoNumUp(ft2_instance_t *inst) { (void)inst; if (state.echoNum < 64) state.echoNum++; }
static void onEchoDistDown(ft2_instance_t *inst) { (void)inst; if (state.echoDistance > 0) state.echoDistance--; }
static void onEchoDistUp(ft2_instance_t *inst) { (void)inst; if (state.echoDistance < 16384) state.echoDistance++; }
static void onEchoVolDown(ft2_instance_t *inst) { (void)inst; if (state.echoVolChange > 0) state.echoVolChange--; }
static void onEchoVolUp(ft2_instance_t *inst) { (void)inst; if (state.echoVolChange < 100) state.echoVolChange++; }

/* ---------- Drawing ---------- */

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 171, y = 220, w = 291, h = 66;

	/* 3D beveled frame */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	/* Labels */
	textOutShadow(video, bmp, 177, 226, PAL_FORGRND, PAL_BUTTON2, "Number of echoes");
	textOutShadow(video, bmp, 177, 240, PAL_FORGRND, PAL_BUTTON2, "Echo distance");
	textOutShadow(video, bmp, 177, 254, PAL_FORGRND, PAL_BUTTON2, "Fade out");
	textOutShadow(video, bmp, 192, 270, PAL_FORGRND, PAL_BUTTON2, "Add memory to sample");

	/* Values */
	charOut(video, bmp, 315 + (2 * 7), 226, PAL_FORGRND, '0' + (char)(state.echoNum / 10));
	charOut(video, bmp, 315 + (3 * 7), 226, PAL_FORGRND, '0' + (state.echoNum % 10));
	hexOut(video, bmp, 308, 240, PAL_FORGRND, state.echoDistance << 4, 5);
	if (state.echoVolChange <= 100)
		textOutFixed(video, bmp, 312, 254, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[state.echoVolChange]);
	charOutShadow(video, bmp, 313 + (3 * 7), 254, PAL_FORGRND, PAL_BUTTON2, '%');

	/* Add memory checkbox */
	fillRect(video, 176, 268, 12, 12, PAL_DESKTOP);
	hLine(video, 176, 268, 12, PAL_BUTTON2);
	vLine(video, 176, 268, 12, PAL_BUTTON2);
	hLine(video, 176, 279, 12, PAL_BUTTON1);
	vLine(video, 187, 268, 12, PAL_BUTTON1);
	if (state.echoAddMemory) charOut(video, bmp, 178, 270, PAL_FORGRND, 'x');
}

/* ---------- Echo algorithm ---------- */

/*
 * Apply echo effect: for each output sample, sum original + decaying echoes.
 * nEchoes = number of echoes until volume drops below 1 LSB.
 * If addMemory, output length = input + distance*(nEchoes-1).
 */
static void applyEchoToSample(void)
{
	if (state.instance == NULL || state.echoNum < 1) return;
	ft2_instance_t *inst = state.instance;

	ft2_sample_t *s = getCurrentSample();
	if (s == NULL || s->dataPtr == NULL || s->length == 0) return;

	int32_t readLen = s->length;
	int8_t *readPtr = s->dataPtr;
	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	int32_t distance = state.echoDistance * 16;
	double dVolChange = state.echoVolChange / 100.0;

	/* Count echoes until amplitude drops below 1 LSB */
	double dSmp = sample16Bit ? 32768.0 : 128.0;
	int32_t k = 0;
	while (k < state.echoNum && dSmp >= 1.0) { dSmp *= dVolChange; k++; }
	int32_t nEchoes = k + 1;
	if (nEchoes < 1) return;

	int32_t writeLen = readLen;
	if (state.echoAddMemory) {
		int64_t tmp64 = (int64_t)distance * (nEchoes - 1) + writeLen;
		if (tmp64 > MAX_SAMPLE_LEN) tmp64 = MAX_SAMPLE_LEN;
		writeLen = (int32_t)tmp64;
	}

	/* Allocate with interpolation padding */
	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
	size_t allocSize = (size_t)(leftPadding + writeLen * bytesPerSample + rightPadding);

	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (newOrigPtr == NULL) return;
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
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;
			
			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;
			
			while (echoCycle > 0)
			{
				if (echoRead >= 0 && echoRead < readLen)
					dSmpOut += (double)readPtr16[echoRead] * dSmpMul;
				
				dSmpMul *= dVolChange;
				echoRead -= distance;
				
				if (echoRead < 0)
					break;
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
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;
			
			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;
			
			while (echoCycle > 0)
			{
				if (echoRead >= 0 && echoRead < readLen)
					dSmpOut += (double)readPtr[echoRead] * dSmpMul;
				
				dSmpMul *= dVolChange;
				echoRead -= distance;
				
				if (echoRead < 0)
					break;
				echoCycle--;
			}
			
			int32_t smp32 = (int32_t)dSmpOut;
			if (smp32 < -128) smp32 = -128;
			if (smp32 > 127) smp32 = 127;
			writePtr8[writeIdx++] = (int8_t)smp32;
		}
	}
	
	if (s->origDataPtr != NULL) free(s->origDataPtr);
	s->origDataPtr = newOrigPtr;
	s->dataPtr = newData;
	s->length = writeLen;

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* ---------- Public API ---------- */

void ft2_echo_panel_show(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	ft2_sample_t *s = NULL;
	if (inst->editor.curInstr != 0 && inst->editor.curInstr <= 128) {
		ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
		if (instr != NULL) s = &instr->smp[inst->editor.curSmp];
	}
	if (s == NULL || s->dataPtr == NULL || s->length == 0) return;

	state.active = true;
	state.instance = inst;
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_ECHO);
}

void ft2_echo_panel_hide(void)
{
	if (!state.active) return;
	hideWidgets();
	state.active = false;
	if (state.instance != NULL) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_ECHO);
}

bool ft2_echo_panel_is_active(void) { return state.active; }

void ft2_echo_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL) return;
	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance != NULL && state.instance->ui != NULL) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (widgets == NULL) return;

	setScrollBarPos(state.instance, widgets, video, SB_RES_1, (uint32_t)state.echoNum, false);
	setScrollBarPos(state.instance, widgets, video, SB_RES_2, (uint32_t)state.echoDistance, false);
	setScrollBarPos(state.instance, widgets, video, SB_RES_3, (uint32_t)state.echoVolChange, false);

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i]) drawPushButton(widgets, video, bmp, PB_RES_1 + i);
	for (int i = 0; i < 3; i++)
		if (widgets->scrollBarState[SB_RES_1 + i].visible) drawScrollBar(widgets, video, SB_RES_1 + i);
}

void ft2_echo_panel_apply(void)
{
	if (!state.active) return;
	applyEchoToSample();
	ft2_echo_panel_hide();
}

bool ft2_echo_panel_mouse_down(int32_t x, int32_t y, int button)
{
	if (!state.active || button != 1) return false;
	/* Checkbox hit test */
	if (x >= 176 && x < 188 && y >= 268 && y < 280) {
		state.echoAddMemory = !state.echoAddMemory;
		return true;
	}
	return false;
}

/* State accessors */
int16_t ft2_echo_panel_get_num(void) { return state.echoNum; }
void ft2_echo_panel_set_num(int16_t num) { state.echoNum = num; }
int32_t ft2_echo_panel_get_distance(void) { return state.echoDistance; }
void ft2_echo_panel_set_distance(int32_t dist) { state.echoDistance = dist; }
int16_t ft2_echo_panel_get_vol_change(void) { return state.echoVolChange; }
void ft2_echo_panel_set_vol_change(int16_t vol) { state.echoVolChange = vol; }
bool ft2_echo_panel_get_add_memory(void) { return state.echoAddMemory; }
void ft2_echo_panel_set_add_memory(bool add) { state.echoAddMemory = add; }

