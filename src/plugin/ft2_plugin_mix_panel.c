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
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_xm2_convert.h"
#include "../ft2_instance.h"

#define MIX_STATE(inst) (&FT2_UI(inst)->modalPanels.mix)

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

static void onMixClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onBalanceDown(ft2_instance_t *inst);
static void onBalanceUp(ft2_instance_t *inst);

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	mix_panel_state_t *state = MIX_STATE(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	pushButton_t *p;
	scrollBar_t *s;

	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Mix"; p->x = 197; p->y = 258; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onMixClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 361; p->y = 258; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	p = &widgets->pushButtons[PB_RES_3]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 322; p->y = 244; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onBalanceDown;
	widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 411; p->y = 244; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onBalanceUp;
	widgets->pushButtonVisible[PB_RES_4] = true;

	s = &widgets->scrollBars[SB_RES_1]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 345; s->y = 244; s->w = 66; s->h = 13; s->callbackFunc = onBalanceScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 100);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)state->mixBalance, false);
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++) hideScrollBar(widgets, SB_RES_1 + i);
}

static void onMixClick(ft2_instance_t *inst) { ft2_mix_panel_apply(inst); }
static void onExitClick(ft2_instance_t *inst) { ft2_mix_panel_hide(inst); }
static void onBalanceScrollbar(ft2_instance_t *inst, uint32_t pos) { if (inst && inst->ui) MIX_STATE(inst)->mixBalance = (int8_t)pos; }
static void onBalanceDown(ft2_instance_t *inst) { if (inst && inst->ui) { mix_panel_state_t *s = MIX_STATE(inst); if (s->mixBalance > 0) s->mixBalance--; } }
static void onBalanceUp(ft2_instance_t *inst) { if (inst && inst->ui) { mix_panel_state_t *s = MIX_STATE(inst); if (s->mixBalance < 100) s->mixBalance++; } }

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 192, y = 240, w = 248, h = 38;
	mix_panel_state_t *state = MIX_STATE(inst);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(video, bmp, 198, 246, PAL_FORGRND, PAL_BUTTON2, "Mixing balance");
	if (state->mixBalance <= 100)
		textOutFixed(video, bmp, 299, 246, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[state->mixBalance]);
}

static void applyMixToSample(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	mix_panel_state_t *state = MIX_STATE(inst);

	int16_t dstIns = inst->editor.curInstr, dstSmp = inst->editor.curSmp;
	int16_t mixIns = inst->editor.srcInstr, mixSmp = inst->editor.srcSmp;

	if (dstIns == 0 || dstIns > 128) return;
	if (dstIns == mixIns && dstSmp == mixSmp) return;

	ft2_instr_t *dstInstr = inst->replayer.instr[dstIns];
	if (!dstInstr) return;

	ft2_sample_t *s = &dstInstr->smp[dstSmp];

	ft2_sample_format_t dstFormat = ft2_sample_get_format(s);
	bool dstHasData = ft2_sample_has_data(s, dstFormat);
	int32_t dstLen = dstHasData ? s->length : 0;
	bool dstStereo = dstHasData ? ft2_sample_is_stereo(s, dstFormat) : false;

	ft2_sample_t *mixSample = NULL;
	ft2_sample_format_t mixFormat = FT2_SMP_FORMAT_8BIT;
	bool mixHasData = false;
	int32_t mixLen = 0;
	bool mixStereo = false;

	if (mixIns > 0 && mixIns <= 128)
	{
		ft2_instr_t *mixInstr = inst->replayer.instr[mixIns];
		if (mixInstr)
		{
			mixSample = &mixInstr->smp[mixSmp];
			mixFormat = ft2_sample_get_format(mixSample);
			mixHasData = ft2_sample_has_data(mixSample, mixFormat);
			mixLen = mixHasData ? mixSample->length : 0;
			mixStereo = mixHasData ? ft2_sample_is_stereo(mixSample, mixFormat) : false;
		}
	}

	if (!dstHasData && !mixHasData) return;

	const int32_t newLen = (dstLen > mixLen) ? dstLen : mixLen;
	if (newLen <= 0) return;

	ft2_sample_format_t outFormat = dstHasData ? dstFormat : mixFormat;
	if (dstHasData && mixHasData && mixFormat > outFormat)
		outFormat = mixFormat;

	const bool outStereo = dstStereo || mixStereo;
	const uint32_t bytesPerSample = ft2_sample_format_bytes(outFormat);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)newLen * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (!newOrigL) return;
	void *newOrigR = NULL;
	if (outStereo)
	{
		newOrigR = calloc(allocSize, 1);
		if (!newOrigR) { free(newOrigL); return; }
	}

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;
	uint8_t *newDataR = outStereo ? (uint8_t *)newOrigR + padding : NULL;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	ft2_sample_t outSmp = *s;
	outSmp.origDataPtr = outSmp.dataPtr = NULL;
	outSmp.origDataPtrR = outSmp.dataPtrR = NULL;
	outSmp.origDataPtr24 = outSmp.dataPtr24 = NULL;
	outSmp.origDataPtr24R = outSmp.dataPtr24R = NULL;
	outSmp.origDataPtrF = outSmp.dataPtrF = NULL;
	outSmp.origDataPtrFR = outSmp.dataPtrFR = NULL;

	if (outFormat == FT2_SMP_FORMAT_24BIT)
	{
		outSmp.origDataPtr24 = (int32_t *)newOrigL;
		outSmp.dataPtr24 = (int32_t *)newDataL;
		if (outStereo)
		{
			outSmp.origDataPtr24R = (int32_t *)newOrigR;
			outSmp.dataPtr24R = (int32_t *)newDataR;
		}
	}
	else if (outFormat == FT2_SMP_FORMAT_FLOAT32)
	{
		outSmp.origDataPtrF = (float *)newOrigL;
		outSmp.dataPtrF = (float *)newDataL;
		if (outStereo)
		{
			outSmp.origDataPtrFR = (float *)newOrigR;
			outSmp.dataPtrFR = (float *)newDataR;
		}
	}
	else
	{
		outSmp.origDataPtr = (int8_t *)newOrigL;
		outSmp.dataPtr = (int8_t *)newDataL;
		if (outStereo)
		{
			outSmp.origDataPtrR = (int8_t *)newOrigR;
			outSmp.dataPtrR = (int8_t *)newDataR;
		}
	}

	double dMixA = (100 - state->mixBalance) / 100.0;
	double dMixB = state->mixBalance / 100.0;

	for (int32_t i = 0; i < newLen; i++)
	{
		for (int ch = 0; ch < (outStereo ? 2 : 1); ch++)
		{
			const bool rightChannel = (ch == 1);
			float a = 0.0f, b = 0.0f;
			if (mixHasData && i < mixLen)
			{
				const bool mixRight = mixStereo ? rightChannel : false;
				a = ft2_sample_get_float(mixSample, mixFormat, mixRight, i);
			}
			if (dstHasData && i < dstLen)
			{
				const bool dstRight = dstStereo ? rightChannel : false;
				b = ft2_sample_get_float(s, dstFormat, dstRight, i);
			}
			ft2_sample_set_float(&outSmp, outFormat, rightChannel, i, (float)(a * dMixA + b * dMixB));
		}
	}

	outSmp.length = newLen;
	outSmp.format = outFormat;
	if (outFormat == FT2_SMP_FORMAT_16BIT)
		outSmp.flags |= FT2_SAMPLE_16BIT;
	else
		outSmp.flags &= ~FT2_SAMPLE_16BIT;
	if (outStereo)
		outSmp.flags |= FT2_SAMPLE_STEREO;
	else
		outSmp.flags &= ~FT2_SAMPLE_STEREO;
	outSmp.isFixed = false;
	outSmp.fixedPos = 0;

	if (!dstHasData && mixHasData)
	{
		outSmp.sampleRate = mixSample->sampleRate;
		outSmp.relativeNote = mixSample->relativeNote;
		outSmp.finetune = mixSample->finetune;
	}

	ft2_sample_free_buffers(s);
	*s = outSmp;
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_xm2_update_usage(inst);
}

void ft2_mix_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui || inst->editor.curInstr == 0) return;
	mix_panel_state_t *state = MIX_STATE(inst);
	state->active = true;
	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_MIX);
}

void ft2_mix_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	mix_panel_state_t *state = MIX_STATE(inst);
	if (!state->active) return;
	
	hideWidgets(inst);
	state->active = false;
	inst->uiState.updateSampleEditor = true;
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_MIX);
}

bool ft2_mix_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return MIX_STATE(inst)->active;
}

void ft2_mix_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	mix_panel_state_t *state = MIX_STATE(inst);
	if (!state->active) return;

	drawFrame(inst, video, bmp);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	setScrollBarPos(inst, widgets, video, SB_RES_1, (uint32_t)state->mixBalance, false);
	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
	if (widgets->scrollBarState[SB_RES_1].visible)
		drawScrollBar(widgets, video, SB_RES_1);
}

void ft2_mix_panel_apply(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!MIX_STATE(inst)->active) return;
	applyMixToSample(inst);
	ft2_mix_panel_hide(inst);
}
