/*
** FT2 Plugin - Resample Modal Panel
** Resamples current sample by relative halftones (-36 to +36).
** Adjusts sample length and relative note to maintain pitch.
*/

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft2_plugin_resample_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_common.h"
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
#define RES_STATE(inst) (&FT2_UI(inst)->modalPanels.resample)

#define RESAMPLE_RATE_MIN 200
#define RESAMPLE_RATE_MAX 22000
#define RESAMPLE_BITS_MIN 4
#define RESAMPLE_BITS_MAX 16

static void onResampleClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onTonesScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onTonesDown(ft2_instance_t *inst);
static void onTonesUp(ft2_instance_t *inst);
static void onRateScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onRateDown(ft2_instance_t *inst);
static void onRateUp(ft2_instance_t *inst);
static void onBitsScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onBitsDown(ft2_instance_t *inst);
static void onBitsUp(ft2_instance_t *inst);

static ft2_sample_t *getCurrentSample(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

static uint8_t getNativeBitDepth(const ft2_sample_t *s)
{
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	return (format == FT2_SMP_FORMAT_8BIT) ? 8 : 16;
}

static int32_t clampResampleRate(int32_t rate)
{
	if (rate < RESAMPLE_RATE_MIN) return RESAMPLE_RATE_MIN;
	if (rate > RESAMPLE_RATE_MAX) return RESAMPLE_RATE_MAX;
	return rate;
}

static uint8_t clampResampleBits(uint8_t bits, uint8_t nativeBits)
{
	if (bits < RESAMPLE_BITS_MIN) bits = RESAMPLE_BITS_MIN;
	if (bits > RESAMPLE_BITS_MAX) bits = RESAMPLE_BITS_MAX;
	if (bits > nativeBits) bits = nativeBits;
	return bits;
}

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	const bool xm2Enabled = inst->config.enableXm2Features;

	pushButton_t *p;
	scrollBar_t *s;

	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Resample"; p->x = 214; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onResampleClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 345; p->y = 264; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	if (xm2Enabled)
	{
		ft2_sample_t *smp = getCurrentSample(inst);
		const uint8_t nativeBits = smp ? getNativeBitDepth(smp) : RESAMPLE_BITS_MAX;
		state->targetRateHz = clampResampleRate(state->targetRateHz);
		state->targetBits = clampResampleBits(state->targetBits, nativeBits);

		p = &widgets->pushButtons[PB_RES_3]; memset(p, 0, sizeof(pushButton_t));
		p->caption = ARROW_LEFT_STRING; p->x = 314; p->y = 234; p->w = 23; p->h = 13;
		p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onRateDown;
		widgets->pushButtonVisible[PB_RES_3] = true;

		p = &widgets->pushButtons[PB_RES_4]; memset(p, 0, sizeof(pushButton_t));
		p->caption = ARROW_RIGHT_STRING; p->x = 401; p->y = 234; p->w = 23; p->h = 13;
		p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onRateUp;
		widgets->pushButtonVisible[PB_RES_4] = true;

		p = &widgets->pushButtons[PB_RES_5]; memset(p, 0, sizeof(pushButton_t));
		p->caption = ARROW_LEFT_STRING; p->x = 314; p->y = 248; p->w = 23; p->h = 13;
		p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onBitsDown;
		widgets->pushButtonVisible[PB_RES_5] = true;

		p = &widgets->pushButtons[PB_RES_6]; memset(p, 0, sizeof(pushButton_t));
		p->caption = ARROW_RIGHT_STRING; p->x = 401; p->y = 248; p->w = 23; p->h = 13;
		p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onBitsUp;
		widgets->pushButtonVisible[PB_RES_6] = true;

		s = &widgets->scrollBars[SB_RES_1]; memset(s, 0, sizeof(scrollBar_t));
		s->x = 337; s->y = 234; s->w = 64; s->h = 13; s->callbackFunc = onRateScrollbar;
		widgets->scrollBarState[SB_RES_1].visible = true;
		setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
		setScrollBarEnd(inst, widgets, NULL, SB_RES_1, (uint32_t)(RESAMPLE_RATE_MAX - RESAMPLE_RATE_MIN));
		setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)(state->targetRateHz - RESAMPLE_RATE_MIN), false);

		s = &widgets->scrollBars[SB_RES_2]; memset(s, 0, sizeof(scrollBar_t));
		s->x = 337; s->y = 248; s->w = 64; s->h = 13; s->callbackFunc = onBitsScrollbar;
		widgets->scrollBarState[SB_RES_2].visible = true;
		setScrollBarPageLength(inst, widgets, NULL, SB_RES_2, 1);
		setScrollBarEnd(inst, widgets, NULL, SB_RES_2, (uint32_t)(nativeBits - RESAMPLE_BITS_MIN));
		setScrollBarPos(inst, widgets, NULL, SB_RES_2, (uint32_t)(state->targetBits - RESAMPLE_BITS_MIN), false);
	}
	else
	{
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

static void onRateScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	state->targetRateHz = clampResampleRate((int32_t)pos + RESAMPLE_RATE_MIN);
}

static void onRateDown(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	state->targetRateHz = clampResampleRate(state->targetRateHz - 1);
}

static void onRateUp(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	state->targetRateHz = clampResampleRate(state->targetRateHz + 1);
}

static void onBitsScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *smp = getCurrentSample(inst);
	const uint8_t nativeBits = smp ? getNativeBitDepth(smp) : RESAMPLE_BITS_MAX;
	state->targetBits = clampResampleBits((uint8_t)pos + RESAMPLE_BITS_MIN, nativeBits);
}

static void onBitsDown(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *smp = getCurrentSample(inst);
	const uint8_t nativeBits = smp ? getNativeBitDepth(smp) : RESAMPLE_BITS_MAX;
	state->targetBits = clampResampleBits((uint8_t)(state->targetBits - 1), nativeBits);
}

static void onBitsUp(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *smp = getCurrentSample(inst);
	const uint8_t nativeBits = smp ? getNativeBitDepth(smp) : RESAMPLE_BITS_MAX;
	state->targetBits = clampResampleBits((uint8_t)(state->targetBits + 1), nativeBits);
}

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 209, y = 230, w = 217, h = 54;
	resample_panel_state_t *state = RES_STATE(inst);
	const bool xm2Enabled = inst->config.enableXm2Features;

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	if (xm2Enabled)
	{
		ft2_sample_t *s = getCurrentSample(inst);
		const uint8_t nativeBits = s ? getNativeBitDepth(s) : RESAMPLE_BITS_MAX;
		state->targetRateHz = clampResampleRate(state->targetRateHz);
		state->targetBits = clampResampleBits(state->targetBits, nativeBits);

		textOutShadow(video, bmp, 215, 236, PAL_FORGRND, PAL_BUTTON2, "Rate (Hz)");
		textOutShadow(video, bmp, 215, 250, PAL_FORGRND, PAL_BUTTON2, "Bit depth");

		char rateStr[8];
		snprintf(rateStr, sizeof(rateStr), "%d", state->targetRateHz);
		textOut(video, bmp, 280, 236, PAL_FORGRND, rateStr);

		char bitsStr[4];
		snprintf(bitsStr, sizeof(bitsStr), "%d", state->targetBits);
		textOut(video, bmp, 280, 250, PAL_FORGRND, bitsStr);
	}
	else
	{
		int32_t newLen = 0;
		ft2_sample_t *s = getCurrentSample(inst);
		ft2_sample_format_t format = ft2_sample_get_format(s);
		if (s && ft2_sample_has_data(s, format))
		{
			double dNewLen = s->length * pow(2.0, state->relReSmp * (1.0 / 12.0));
			if (dNewLen > (double)FT2_MAX_SAMPLE_LEN) dNewLen = (double)FT2_MAX_SAMPLE_LEN;
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
}

static void quantizeSampleBitDepth(ft2_sample_t *s, ft2_sample_format_t format, uint8_t effectiveBits, uint8_t nativeBits)
{
	if (s == NULL || s->length <= 0 || effectiveBits < RESAMPLE_BITS_MIN)
		return;

	if ((format == FT2_SMP_FORMAT_8BIT || format == FT2_SMP_FORMAT_16BIT) && effectiveBits >= nativeBits)
		return;

	const float maxInt = (float)((1 << (effectiveBits - 1)) - 1);
	const bool isStereo = ft2_sample_is_stereo(s, format);

	for (int32_t i = 0; i < s->length; i++)
	{
		float v = ft2_sample_get_float(s, format, false, i);
		v = roundf(v * maxInt) / maxInt;
		ft2_sample_set_float(s, format, false, i, v);
		if (isStereo)
		{
			v = ft2_sample_get_float(s, format, true, i);
			v = roundf(v * maxInt) / maxInt;
			ft2_sample_set_float(s, format, true, i, v);
		}
	}
}

static double getResamplePosition(int32_t index, int32_t srcLen, int32_t dstLen)
{
	if (srcLen <= 1 || dstLen <= 1)
		return 0.0;

	return (double)index * (double)(srcLen - 1) / (double)(dstLen - 1);
}

static float sampleLinearFromSample(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, double pos)
{
	if (s == NULL || s->length <= 0)
		return 0.0f;

	const int32_t lastIndex = s->length - 1;
	if (lastIndex <= 0 || pos <= 0.0)
		return ft2_sample_get_float(s, format, rightChannel, 0);
	if (pos >= (double)lastIndex)
		return ft2_sample_get_float(s, format, rightChannel, lastIndex);

	const int32_t idx = (int32_t)pos;
	int32_t idx2 = idx + 1;
	if (idx2 > lastIndex)
		idx2 = lastIndex;

	const float frac = (float)(pos - (double)idx);
	const float a = ft2_sample_get_float(s, format, rightChannel, idx);
	const float b = ft2_sample_get_float(s, format, rightChannel, idx2);
	return a + ((b - a) * frac);
}

static float sampleLinearFromBuffer(const float *buffer, int32_t length, double pos)
{
	if (buffer == NULL || length <= 0)
		return 0.0f;

	const int32_t lastIndex = length - 1;
	if (lastIndex <= 0 || pos <= 0.0)
		return buffer[0];
	if (pos >= (double)lastIndex)
		return buffer[lastIndex];

	const int32_t idx = (int32_t)pos;
	int32_t idx2 = idx + 1;
	if (idx2 > lastIndex)
		idx2 = lastIndex;

	const float frac = (float)(pos - (double)idx);
	return buffer[idx] + ((buffer[idx2] - buffer[idx]) * frac);
}

static void resampleSampleToFloat(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, float *dst, int32_t dstLen)
{
	if (s == NULL || dst == NULL || dstLen <= 0)
		return;

	const int32_t srcLen = s->length;
	int32_t i = 0;
	while (i < dstLen)
	{
		const double pos = getResamplePosition(i, srcLen, dstLen);
		dst[i] = sampleLinearFromSample(s, format, rightChannel, pos);
		i++;
	}
}

static void resampleFloatToSample(const float *src, int32_t srcLen, ft2_sample_t *dst, ft2_sample_format_t format, bool rightChannel, int32_t dstLen)
{
	if (src == NULL || dst == NULL || dstLen <= 0)
		return;

	int32_t i = 0;
	while (i < dstLen)
	{
		const double pos = getResamplePosition(i, srcLen, dstLen);
		const float v = sampleLinearFromBuffer(src, srcLen, pos);
		ft2_sample_set_float(dst, format, rightChannel, i, v);
		i++;
	}
}

static void applyResampleToSampleXm2(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || s->length == 0) return;

	const uint8_t nativeBits = getNativeBitDepth(s);
	const uint8_t effectiveBits = clampResampleBits(state->targetBits, nativeBits);
	int32_t clampedTargetRate = clampResampleRate(state->targetRateHz);

	const double srcRate = ft2_sample_get_playback_rate(inst, s);
	if (srcRate <= 0.0)
		return;

	if ((double)clampedTargetRate > srcRate)
		clampedTargetRate = (int32_t)llround(srcRate);

	const int32_t origLen = s->length;
	if (origLen <= 0)
		return;

	int32_t downLen = (int32_t)llround((double)origLen * ((double)clampedTargetRate / srcRate));
	if (downLen < 1)
		downLen = 1;
	if (downLen > origLen)
		downLen = origLen;

	const bool isStereo = ft2_sample_is_stereo(s, format);
	float *tmpL = (float *)calloc((size_t)downLen, sizeof(float));
	if (tmpL == NULL)
		return;
	float *tmpR = NULL;
	if (isStereo)
	{
		tmpR = (float *)calloc((size_t)downLen, sizeof(float));
		if (tmpR == NULL)
		{
			free(tmpL);
			return;
		}
	}

	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)origLen * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (!newOrigL)
	{
		free(tmpR);
		free(tmpL);
		return;
	}
	void *newOrigR = NULL;
	if (isStereo)
	{
		newOrigR = calloc(allocSize, 1);
		if (!newOrigR)
		{
			free(newOrigL);
			free(tmpR);
			free(tmpL);
			return;
		}
	}

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;
	uint8_t *newDataR = isStereo ? (uint8_t *)newOrigR + padding : NULL;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	ft2_sample_t srcSmp = *s;
	ft2_sample_t dstSmp = *s;
	dstSmp.origDataPtr = dstSmp.dataPtr = NULL;
	dstSmp.origDataPtrR = dstSmp.dataPtrR = NULL;
	dstSmp.origDataPtr24 = dstSmp.dataPtr24 = NULL;
	dstSmp.origDataPtr24R = dstSmp.dataPtr24R = NULL;
	dstSmp.origDataPtrF = dstSmp.dataPtrF = NULL;
	dstSmp.origDataPtrFR = dstSmp.dataPtrFR = NULL;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		dstSmp.origDataPtr24 = (int32_t *)newOrigL;
		dstSmp.dataPtr24 = (int32_t *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtr24R = (int32_t *)newOrigR;
			dstSmp.dataPtr24R = (int32_t *)newDataR;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		dstSmp.origDataPtrF = (float *)newOrigL;
		dstSmp.dataPtrF = (float *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtrFR = (float *)newOrigR;
			dstSmp.dataPtrFR = (float *)newDataR;
		}
	}
	else
	{
		dstSmp.origDataPtr = (int8_t *)newOrigL;
		dstSmp.dataPtr = (int8_t *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtrR = (int8_t *)newOrigR;
			dstSmp.dataPtrR = (int8_t *)newDataR;
		}
	}

	dstSmp.format = format;
	dstSmp.flags = s->flags;
	dstSmp.isFixed = false;
	dstSmp.fixedPos = 0;
	dstSmp.length = origLen;

	resampleSampleToFloat(&srcSmp, format, false, tmpL, downLen);
	if (isStereo)
		resampleSampleToFloat(&srcSmp, format, true, tmpR, downLen);

	resampleFloatToSample(tmpL, downLen, &dstSmp, format, false, origLen);
	if (isStereo)
		resampleFloatToSample(tmpR, downLen, &dstSmp, format, true, origLen);

	dstSmp.length = origLen;
	dstSmp.loopStart = s->loopStart;
	dstSmp.loopLength = s->loopLength;
	dstSmp.sampleRate = s->sampleRate;

	quantizeSampleBitDepth(&dstSmp, format, effectiveBits, nativeBits);

	free(tmpR);
	free(tmpL);

	ft2_sample_free_buffers(s);
	*s = dstSmp;
	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
	ft2_xm2_update_usage(inst);
	ft2_song_mark_modified(inst);
}

static void applyResampleToSample(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	resample_panel_state_t *state = RES_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || s->length == 0) return;

	const double dRatio = pow(2.0, (int32_t)state->relReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dRatio;
	if (dNewLen > (double)FT2_MAX_SAMPLE_LEN) dNewLen = (double)FT2_MAX_SAMPLE_LEN;
	const uint32_t newLen = (uint32_t)floor(dNewLen);
	if (newLen == 0) return;

	const bool isStereo = ft2_sample_is_stereo(s, format);
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)newLen * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (!newOrigL) return;
	void *newOrigR = NULL;
	if (isStereo)
	{
		newOrigR = calloc(allocSize, 1);
		if (!newOrigR) { free(newOrigL); return; }
	}

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;
	uint8_t *newDataR = isStereo ? (uint8_t *)newOrigR + padding : NULL;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	ft2_sample_t srcSmp = *s;
	ft2_sample_t dstSmp = *s;
	dstSmp.origDataPtr = dstSmp.dataPtr = NULL;
	dstSmp.origDataPtrR = dstSmp.dataPtrR = NULL;
	dstSmp.origDataPtr24 = dstSmp.dataPtr24 = NULL;
	dstSmp.origDataPtr24R = dstSmp.dataPtr24R = NULL;
	dstSmp.origDataPtrF = dstSmp.dataPtrF = NULL;
	dstSmp.origDataPtrFR = dstSmp.dataPtrFR = NULL;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		dstSmp.origDataPtr24 = (int32_t *)newOrigL;
		dstSmp.dataPtr24 = (int32_t *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtr24R = (int32_t *)newOrigR;
			dstSmp.dataPtr24R = (int32_t *)newDataR;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		dstSmp.origDataPtrF = (float *)newOrigL;
		dstSmp.dataPtrF = (float *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtrFR = (float *)newOrigR;
			dstSmp.dataPtrFR = (float *)newDataR;
		}
	}
	else
	{
		dstSmp.origDataPtr = (int8_t *)newOrigL;
		dstSmp.dataPtr = (int8_t *)newDataL;
		if (isStereo)
		{
			dstSmp.origDataPtrR = (int8_t *)newOrigR;
			dstSmp.dataPtrR = (int8_t *)newDataR;
		}
	}
	dstSmp.format = format;
	if (format == FT2_SMP_FORMAT_16BIT)
		dstSmp.flags |= FT2_SAMPLE_16BIT;
	else
		dstSmp.flags &= ~FT2_SAMPLE_16BIT;
	if (isStereo)
		dstSmp.flags |= FT2_SAMPLE_STEREO;
	else
		dstSmp.flags &= ~FT2_SAMPLE_STEREO;
	dstSmp.isFixed = false;
	dstSmp.fixedPos = 0;

	const uint64_t delta64 = (uint64_t)round(4294967296.0 / dRatio);
	uint64_t posFrac64 = 0;
	for (uint32_t i = 0; i < newLen; i++)
	{
		uint32_t srcIdx = (uint32_t)(posFrac64 >> 32);
		if (srcIdx >= (uint32_t)srcSmp.length) srcIdx = (uint32_t)(srcSmp.length - 1);

		float v = ft2_sample_get_float(&srcSmp, format, false, (int32_t)srcIdx);
		ft2_sample_set_float(&dstSmp, format, false, (int32_t)i, v);
		if (isStereo)
		{
			v = ft2_sample_get_float(&srcSmp, format, true, (int32_t)srcIdx);
			ft2_sample_set_float(&dstSmp, format, true, (int32_t)i, v);
		}
		posFrac64 += delta64;
	}

	ft2_sample_free_buffers(s);
	*s = dstSmp;
	s->length = (int32_t)newLen;
	s->loopStart = (int32_t)round(s->loopStart * dRatio);
	s->loopLength = (int32_t)round(s->loopLength * dRatio);
	if (s->sampleRate > 0)
	{
		uint32_t newRate = (uint32_t)round((double)s->sampleRate * dRatio);
		s->sampleRate = (newRate == 0) ? 1 : newRate;
	}
	else
	{
		s->relativeNote += state->relReSmp;
	}

	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_xm2_update_usage(inst);
}

void ft2_resample_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || s->length == 0) return;

	resample_panel_state_t *state = RES_STATE(inst);
	state->active = true;
	if (inst->config.enableXm2Features)
	{
		double rate = ft2_sample_get_playback_rate(inst, s);
		if (rate <= 0.0) rate = 8363.0;
		state->targetRateHz = clampResampleRate((int32_t)llround(rate));
		state->targetBits = clampResampleBits(getNativeBitDepth(s), getNativeBitDepth(s));
	}
	else
	{
		state->relReSmp = 0;
	}
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
	ft2_sample_ed_mark_waveform_dirty(inst);
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
	if (inst->config.enableXm2Features)
	{
		setScrollBarPos(inst, widgets, video, SB_RES_1, (uint32_t)(state->targetRateHz - RESAMPLE_RATE_MIN), false);
		setScrollBarPos(inst, widgets, video, SB_RES_2, (uint32_t)(state->targetBits - RESAMPLE_BITS_MIN), false);
	}
	else
	{
		setScrollBarPos(inst, widgets, video, SB_RES_1, (uint32_t)(state->relReSmp + 36), false);
	}

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);

	if (widgets->scrollBarState[SB_RES_1].visible)
		drawScrollBar(widgets, video, SB_RES_1);
	if (widgets->scrollBarState[SB_RES_2].visible)
		drawScrollBar(widgets, video, SB_RES_2);
}

void ft2_resample_panel_apply(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!RES_STATE(inst)->active) return;
	if (inst->config.enableXm2Features)
		applyResampleToSampleXm2(inst);
	else
		applyResampleToSample(inst);
	ft2_resample_panel_hide(inst);
}
