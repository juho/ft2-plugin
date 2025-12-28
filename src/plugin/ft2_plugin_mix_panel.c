/**
 * @file ft2_plugin_mix_panel.c
 * @brief Sample mixing modal panel implementation
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
#include "../ft2_instance.h"

/* 3-digit decimal string lookup table */
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

/* Panel state */
typedef struct {
	bool active;
	ft2_instance_t *instance;
	int8_t mixBalance; /* 0-100 (0 = all src, 100 = all dst) */
} mix_panel_state_t;

static mix_panel_state_t state = {
	.active = false,
	.instance = NULL,
	.mixBalance = 50
};

/* Forward declarations */
static void onMixClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onBalanceDown(ft2_instance_t *inst);
static void onBalanceUp(ft2_instance_t *inst);

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL)
		return;
	
	pushButton_t *p;
	scrollBar_t *s;
	
	/* "Mix" pushbutton */
	p = &pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Mix";
	p->x = 197;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = onMixClick;
	p->visible = true;
	
	/* "Exit" pushbutton */
	p = &pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit";
	p->x = 361;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = onExitClick;
	p->visible = true;
	
	/* Balance left arrow */
	p = &pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 322;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onBalanceDown;
	p->visible = true;
	
	/* Balance right arrow */
	p = &pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 411;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onBalanceUp;
	p->visible = true;
	
	/* Balance scrollbar */
	s = &scrollBars[SB_RES_1];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 345;
	s->y = 244;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = onBalanceScrollbar;
	s->visible = true;
	setScrollBarPageLength(inst, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, NULL, SB_RES_1, 100);
	setScrollBarPos(inst, NULL, SB_RES_1, (uint32_t)state.mixBalance, false);
}

static void hideWidgets(void)
{
	for (int i = 0; i < 8; i++)
		hidePushButton(PB_RES_1 + i);
	
	for (int i = 0; i < 3; i++)
		hideScrollBar(SB_RES_1 + i);
}

static void onMixClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_mix_panel_apply();
}

static void onExitClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_mix_panel_hide();
}

static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	(void)inst;
	state.mixBalance = (int8_t)pos;
}

static void onBalanceDown(ft2_instance_t *inst)
{
	(void)inst;
	if (state.mixBalance > 0)
		state.mixBalance--;
}

static void onBalanceUp(ft2_instance_t *inst)
{
	(void)inst;
	if (state.mixBalance < 100)
		state.mixBalance++;
}

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 192;
	const int16_t y = 240;
	const int16_t w = 248;
	const int16_t h = 38;
	
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
	
	/* Labels */
	textOutShadow(video, bmp, 198, 246, PAL_FORGRND, PAL_BUTTON2, "Mixing balance");
	if (state.mixBalance <= 100)
		textOutFixed(video, bmp, 299, 246, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[state.mixBalance]);
}

static void applyMixToSample(void)
{
	if (state.instance == NULL)
		return;
	
	ft2_instance_t *inst = state.instance;
	
	int16_t dstIns = inst->editor.curInstr;
	int16_t dstSmp = inst->editor.curSmp;
	int16_t mixIns = inst->editor.srcInstr;
	int16_t mixSmp = inst->editor.srcSmp;
	
	if (dstIns == 0 || dstIns > 128)
		return;
	
	if (dstIns == mixIns && dstSmp == mixSmp)
		return;
	
	ft2_instr_t *dstInstr = inst->replayer.instr[dstIns];
	if (dstInstr == NULL)
		return;
	
	ft2_sample_t *s = &dstInstr->smp[dstSmp];
	
	int8_t *dstPtr = NULL;
	uint8_t dstFlags = 0;
	int32_t dstLen = 0;
	
	if (s->dataPtr != NULL)
	{
		dstPtr = s->dataPtr;
		dstLen = s->length;
		dstFlags = s->flags;
	}
	
	int8_t *mixPtr = NULL;
	uint8_t mixFlags = 0;
	int32_t mixLen = 0;
	
	if (mixIns > 0 && mixIns <= 128)
	{
		ft2_instr_t *mixInstr = inst->replayer.instr[mixIns];
		if (mixInstr != NULL)
		{
			ft2_sample_t *mixSample = &mixInstr->smp[mixSmp];
			if (mixSample->dataPtr != NULL)
			{
				mixPtr = mixSample->dataPtr;
				mixLen = mixSample->length;
				mixFlags = mixSample->flags;
			}
		}
	}
	
	if (dstPtr == NULL && mixPtr == NULL)
		return;
	
	int32_t newLen = (dstLen > mixLen) ? dstLen : mixLen;
	bool newIs16Bit = ((dstFlags & FT2_SAMPLE_16BIT) != 0) || ((mixFlags & FT2_SAMPLE_16BIT) != 0);
	
	/* Allocate with proper padding for interpolation */
	int32_t bytesPerSample = newIs16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t dataLen = newLen * bytesPerSample;
	size_t allocSize = (size_t)(leftPadding + dataLen + rightPadding);
	
	int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
	if (newOrigPtr == NULL)
		return;
	
	int8_t *newData = newOrigPtr + leftPadding;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);
	
	ft2_unfix_sample(s);
	
	double dMixA = (100 - state.mixBalance) / 100.0;
	double dMixB = state.mixBalance / 100.0;
	
	bool dstIs16 = (dstFlags & FT2_SAMPLE_16BIT) != 0;
	bool mixIs16 = (mixFlags & FT2_SAMPLE_16BIT) != 0;
	
	if (newIs16Bit)
	{
		int16_t *newPtr16 = (int16_t *)newData;
		
		for (int32_t i = 0; i < newLen; i++)
		{
			double dSmpA = 0.0;
			double dSmpB = 0.0;
			
			if (i < mixLen && mixPtr != NULL)
			{
				if (mixIs16)
					dSmpA = ((int16_t *)mixPtr)[i];
				else
					dSmpA = mixPtr[i] * 256.0;
			}
			
			if (i < dstLen && dstPtr != NULL)
			{
				if (dstIs16)
					dSmpB = ((int16_t *)dstPtr)[i];
				else
					dSmpB = dstPtr[i] * 256.0;
			}
			
			double dOut = (dSmpA * dMixA) + (dSmpB * dMixB);
			int32_t smp32 = (int32_t)dOut;
			if (smp32 < -32768) smp32 = -32768;
			if (smp32 > 32767) smp32 = 32767;
			newPtr16[i] = (int16_t)smp32;
		}
		
		s->flags |= FT2_SAMPLE_16BIT;
	}
	else
	{
		int8_t *newPtr8 = newData;
		
		for (int32_t i = 0; i < newLen; i++)
		{
			double dSmpA = 0.0;
			double dSmpB = 0.0;
			
			if (i < mixLen && mixPtr != NULL)
				dSmpA = mixPtr[i];
			
			if (i < dstLen && dstPtr != NULL)
				dSmpB = dstPtr[i];
			
			double dOut = (dSmpA * dMixA) + (dSmpB * dMixB);
			int32_t smp32 = (int32_t)dOut;
			if (smp32 < -128) smp32 = -128;
			if (smp32 > 127) smp32 = 127;
			newPtr8[i] = (int8_t)smp32;
		}
	}
	
	/* Free old sample data properly */
	if (s->origDataPtr != NULL)
		free(s->origDataPtr);

	s->origDataPtr = newOrigPtr;
	s->dataPtr = newData;
	s->length = newLen;
	
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Public API */

void ft2_mix_panel_show(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	if (inst->editor.curInstr == 0)
		return;
	
	state.active = true;
	state.instance = inst;
	
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_MIX);
}

void ft2_mix_panel_hide(void)
{
	if (!state.active)
		return;
	
	hideWidgets();
	state.active = false;
	
	if (state.instance != NULL)
		state.instance->uiState.updateSampleEditor = true;
	
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_MIX);
}

bool ft2_mix_panel_is_active(void)
{
	return state.active;
}

void ft2_mix_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL)
		return;
	
	drawFrame(video, bmp);
	
	if (state.instance != NULL)
		setScrollBarPos(state.instance, video, SB_RES_1, (uint32_t)state.mixBalance, false);
	
	for (int i = 0; i < 8; i++)
	{
		if (pushButtons[PB_RES_1 + i].visible)
			drawPushButton(video, bmp, PB_RES_1 + i);
	}
	
	if (scrollBars[SB_RES_1].visible)
		drawScrollBar(video, SB_RES_1);
}

void ft2_mix_panel_apply(void)
{
	if (!state.active)
		return;
	
	applyMixToSample();
	ft2_mix_panel_hide();
}

int8_t ft2_mix_panel_get_balance(void)
{
	return state.mixBalance;
}

void ft2_mix_panel_set_balance(int8_t balance)
{
	state.mixBalance = balance;
}

