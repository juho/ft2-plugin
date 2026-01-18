/*
** FT2 Plugin - Channels Modal Panel
** Converts current sample between mono and stereo.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft2_plugin_channels_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_common.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_xm2_convert.h"
#include "../ft2_instance.h"

#define CHNS_STATE(inst) (&FT2_UI(inst)->modalPanels.channels)

#define PANEL_X 186
#define PANEL_Y 230
#define PANEL_W 300
#define PANEL_H 92

static void onApplyClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onReadLeft(ft2_instance_t *inst);
static void onReadRight(ft2_instance_t *inst);
static void onDownmix(ft2_instance_t *inst);
static void onConvertYes(ft2_instance_t *inst);
static void onConvertNo(ft2_instance_t *inst);

static ft2_sample_t *getCurrentSample(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

static bool getSampleInfo(ft2_instance_t *inst, ft2_sample_t **outSmp, ft2_sample_format_t *outFormat, bool *outStereo)
{
	ft2_sample_t *s = getCurrentSample(inst);
	if (outSmp) *outSmp = s;

	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (outFormat) *outFormat = format;

	const bool isStereo = (s != NULL) && ft2_sample_is_stereo(s, format);
	if (outStereo) *outStereo = isStereo;

	return (s != NULL);
}

static void syncButtons(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	channels_panel_state_t *state = CHNS_STATE(inst);

	ft2_sample_t *s = NULL;
	bool isStereo = false;
	getSampleInfo(inst, &s, NULL, &isStereo);

	widgets->pushButtonLocked[PB_RES_3] = false;
	widgets->pushButtonLocked[PB_RES_4] = false;
	widgets->pushButtonLocked[PB_RES_5] = false;

	if (isStereo)
	{
		if (state->toMonoMode > FT2_CHNS_TO_MONO_DOWNMIX)
			state->toMonoMode = FT2_CHNS_TO_MONO_DOWNMIX;

		widgets->pushButtonLocked[PB_RES_3] = (state->toMonoMode == FT2_CHNS_TO_MONO_LEFT);
		widgets->pushButtonLocked[PB_RES_4] = (state->toMonoMode == FT2_CHNS_TO_MONO_RIGHT);
		widgets->pushButtonLocked[PB_RES_5] = (state->toMonoMode == FT2_CHNS_TO_MONO_DOWNMIX);
	}
	else
	{
		if (state->toStereoMode > FT2_CHNS_TO_STEREO_NO)
			state->toStereoMode = FT2_CHNS_TO_STEREO_YES;

		widgets->pushButtonLocked[PB_RES_3] = (state->toStereoMode == FT2_CHNS_TO_STEREO_YES);
		widgets->pushButtonLocked[PB_RES_4] = (state->toStereoMode == FT2_CHNS_TO_STEREO_NO);
	}
}

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	const int16_t buttonY = PANEL_Y + PANEL_H - 22;
	const int16_t buttonW = 54;
	const int16_t buttonGap = 10;
	const int16_t buttonsTotal = (buttonW * 2) + buttonGap;
	const int16_t buttonsX = PANEL_X + (PANEL_W - buttonsTotal) / 2;
	const int16_t optionY = PANEL_Y + 40;

	ft2_sample_t *s = NULL;
	bool isStereo = false;
	getSampleInfo(inst, &s, NULL, &isStereo);

	pushButton_t *p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Apply"; p->x = buttonsX; p->y = buttonY; p->w = buttonW; p->h = 16;
	p->callbackFuncOnUp = onApplyClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel"; p->x = buttonsX + buttonW + buttonGap; p->y = buttonY; p->w = buttonW; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	if (isStereo)
	{
		const int16_t optionW = 72;
		const int16_t optionGap = 8;
		const int16_t optionTotal = (optionW * 3) + (optionGap * 2);
		const int16_t optionX = PANEL_X + (PANEL_W - optionTotal) / 2;

		p = &widgets->pushButtons[PB_RES_3];
		memset(p, 0, sizeof(pushButton_t));
		p->caption = "Read left"; p->x = optionX; p->y = optionY; p->w = optionW; p->h = 16;
		p->callbackFuncOnUp = onReadLeft; widgets->pushButtonVisible[PB_RES_3] = true;

		p = &widgets->pushButtons[PB_RES_4];
		memset(p, 0, sizeof(pushButton_t));
		p->caption = "Read right"; p->x = optionX + optionW + optionGap; p->y = optionY; p->w = optionW; p->h = 16;
		p->callbackFuncOnUp = onReadRight; widgets->pushButtonVisible[PB_RES_4] = true;

		p = &widgets->pushButtons[PB_RES_5];
		memset(p, 0, sizeof(pushButton_t));
		p->caption = "Downmix"; p->x = optionX + (optionW + optionGap) * 2; p->y = optionY; p->w = optionW; p->h = 16;
		p->callbackFuncOnUp = onDownmix; widgets->pushButtonVisible[PB_RES_5] = true;
	}
	else
	{
		const int16_t optionW = 54;
		const int16_t optionGap = 12;
		const int16_t optionTotal = (optionW * 2) + optionGap;
		const int16_t optionX = PANEL_X + (PANEL_W - optionTotal) / 2;

		p = &widgets->pushButtons[PB_RES_3];
		memset(p, 0, sizeof(pushButton_t));
		p->caption = "Yes"; p->x = optionX; p->y = optionY; p->w = optionW; p->h = 16;
		p->callbackFuncOnUp = onConvertYes; widgets->pushButtonVisible[PB_RES_3] = true;

		p = &widgets->pushButtons[PB_RES_4];
		memset(p, 0, sizeof(pushButton_t));
		p->caption = "No"; p->x = optionX + optionW + optionGap; p->y = optionY; p->w = optionW; p->h = 16;
		p->callbackFuncOnUp = onConvertNo; widgets->pushButtonVisible[PB_RES_4] = true;
	}

	syncButtons(inst);
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	int i = 0;
	while (i < 8)
	{
		hidePushButton(widgets, PB_RES_1 + i);
		i++;
	}

	i = 0;
	while (i < 3)
	{
		widgets->pushButtonLocked[PB_RES_3 + i] = false;
		i++;
	}
}

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = PANEL_X, y = PANEL_Y, w = PANEL_W, h = PANEL_H;
	const int16_t headerY = y + 6;
	const int16_t labelY = y + 24;

	ft2_sample_t *s = NULL;
	bool isStereo = false;
	getSampleInfo(inst, &s, NULL, &isStereo);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	const char *currentStr = "Current format: ---";
	if (s != NULL)
		currentStr = isStereo ? "Current format: Stereo" : "Current format: Mono";

	textOutShadow(video, bmp, x + 10, headerY, PAL_FORGRND, PAL_BUTTON2, currentStr);
	textOutShadow(video, bmp, x + 10, labelY, PAL_FORGRND, PAL_BUTTON2, isStereo ? "Convert to mono:" : "Convert to stereo:");
}

static bool convertStereoToMono(ft2_instance_t *inst, ft2_sample_t *s, ft2_sample_format_t format, uint8_t mode)
{
	if (!inst || !s || s->length <= 0) return false;
	if (!ft2_sample_has_data(s, format) || !ft2_sample_is_stereo(s, format)) return false;

	const int32_t len = s->length;
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)len * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (!newOrigL)
		return false;

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;

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
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		dstSmp.origDataPtrF = (float *)newOrigL;
		dstSmp.dataPtrF = (float *)newDataL;
	}
	else
	{
		dstSmp.origDataPtr = (int8_t *)newOrigL;
		dstSmp.dataPtr = (int8_t *)newDataL;
	}

	dstSmp.format = format;
	dstSmp.flags = s->flags;
	if (format == FT2_SMP_FORMAT_16BIT)
		dstSmp.flags |= FT2_SAMPLE_16BIT;
	else
		dstSmp.flags &= ~FT2_SAMPLE_16BIT;
	dstSmp.flags &= ~FT2_SAMPLE_STEREO;
	dstSmp.isFixed = false;
	dstSmp.fixedPos = 0;
	dstSmp.length = len;

	int32_t i = 0;
	while (i < len)
	{
		const float left = ft2_sample_get_float(&srcSmp, format, false, i);
		const float right = ft2_sample_get_float(&srcSmp, format, true, i);
		float v = left;

		if (mode == FT2_CHNS_TO_MONO_RIGHT)
			v = right;
		else if (mode == FT2_CHNS_TO_MONO_DOWNMIX)
			v = 0.5f * (left + right);

		ft2_sample_set_float(&dstSmp, format, false, i, v);
		i++;
	}

	dstSmp.loopStart = s->loopStart;
	dstSmp.loopLength = s->loopLength;
	dstSmp.sampleRate = s->sampleRate;
	dstSmp.relativeNote = s->relativeNote;
	dstSmp.finetune = s->finetune;

	ft2_sample_free_buffers(s);
	*s = dstSmp;
	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	return true;
}

static bool convertMonoToStereo(ft2_instance_t *inst, ft2_sample_t *s, ft2_sample_format_t format)
{
	if (!inst || !s || s->length <= 0) return false;
	if (!ft2_sample_has_data(s, format) || ft2_sample_is_stereo(s, format)) return false;

	const int32_t len = s->length;
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)len * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (!newOrigL)
		return false;
	void *newOrigR = calloc(allocSize, 1);
	if (!newOrigR)
	{
		free(newOrigL);
		return false;
	}

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;
	uint8_t *newDataR = (uint8_t *)newOrigR + padding;

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
		dstSmp.origDataPtr24R = (int32_t *)newOrigR;
		dstSmp.dataPtr24R = (int32_t *)newDataR;
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		dstSmp.origDataPtrF = (float *)newOrigL;
		dstSmp.dataPtrF = (float *)newDataL;
		dstSmp.origDataPtrFR = (float *)newOrigR;
		dstSmp.dataPtrFR = (float *)newDataR;
	}
	else
	{
		dstSmp.origDataPtr = (int8_t *)newOrigL;
		dstSmp.dataPtr = (int8_t *)newDataL;
		dstSmp.origDataPtrR = (int8_t *)newOrigR;
		dstSmp.dataPtrR = (int8_t *)newDataR;
	}

	dstSmp.format = format;
	dstSmp.flags = s->flags;
	if (format == FT2_SMP_FORMAT_16BIT)
		dstSmp.flags |= FT2_SAMPLE_16BIT;
	else
		dstSmp.flags &= ~FT2_SAMPLE_16BIT;
	dstSmp.flags |= FT2_SAMPLE_STEREO;
	dstSmp.isFixed = false;
	dstSmp.fixedPos = 0;
	dstSmp.length = len;

	int32_t i = 0;
	while (i < len)
	{
		const float v = ft2_sample_get_float(&srcSmp, format, false, i);
		ft2_sample_set_float(&dstSmp, format, false, i, v);
		ft2_sample_set_float(&dstSmp, format, true, i, v);
		i++;
	}

	dstSmp.loopStart = s->loopStart;
	dstSmp.loopLength = s->loopLength;
	dstSmp.sampleRate = s->sampleRate;
	dstSmp.relativeNote = s->relativeNote;
	dstSmp.finetune = s->finetune;

	ft2_sample_free_buffers(s);
	*s = dstSmp;
	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	return true;
}

static void applyChannelConversion(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	channels_panel_state_t *state = CHNS_STATE(inst);

	ft2_sample_t *s = NULL;
	ft2_sample_format_t format = FT2_SMP_FORMAT_8BIT;
	bool isStereo = false;
	getSampleInfo(inst, &s, &format, &isStereo);
	if (!s || !ft2_sample_has_data(s, format) || s->length <= 0) return;

	bool changed = false;
	if (isStereo)
	{
		changed = convertStereoToMono(inst, s, format, state->toMonoMode);
	}
	else
	{
		if (state->toStereoMode == FT2_CHNS_TO_STEREO_YES)
			changed = convertMonoToStereo(inst, s, format);
	}

	if (changed)
	{
		ft2_xm2_update_usage(inst);

		ft2_sample_ed_mark_waveform_dirty(inst);
		inst->uiState.updateInstrSwitcher = true;
		ft2_song_mark_modified(inst);
	}
}

static void onApplyClick(ft2_instance_t *inst)
{
	applyChannelConversion(inst);
	ft2_channels_panel_hide(inst);
}

static void onExitClick(ft2_instance_t *inst) { ft2_channels_panel_hide(inst); }

static void onReadLeft(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	CHNS_STATE(inst)->toMonoMode = FT2_CHNS_TO_MONO_LEFT;
	syncButtons(inst);
}

static void onReadRight(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	CHNS_STATE(inst)->toMonoMode = FT2_CHNS_TO_MONO_RIGHT;
	syncButtons(inst);
}

static void onDownmix(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	CHNS_STATE(inst)->toMonoMode = FT2_CHNS_TO_MONO_DOWNMIX;
	syncButtons(inst);
}

static void onConvertYes(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	CHNS_STATE(inst)->toStereoMode = FT2_CHNS_TO_STEREO_YES;
	syncButtons(inst);
}

static void onConvertNo(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	CHNS_STATE(inst)->toStereoMode = FT2_CHNS_TO_STEREO_NO;
	syncButtons(inst);
}

void ft2_channels_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui || !inst->config.enableXm2Features) return;
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || s->length == 0) return;

	channels_panel_state_t *state = CHNS_STATE(inst);
	state->active = true;

	if (ft2_sample_is_stereo(s, format))
		state->toMonoMode = FT2_CHNS_TO_MONO_DOWNMIX;
	else
		state->toStereoMode = FT2_CHNS_TO_STEREO_YES;

	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_CHANNELS);
}

void ft2_channels_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	channels_panel_state_t *state = CHNS_STATE(inst);
	if (!state->active) return;

	hideWidgets(inst);
	state->active = false;
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_CHANNELS);
}

bool ft2_channels_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return CHNS_STATE(inst)->active;
}

void ft2_channels_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	channels_panel_state_t *state = CHNS_STATE(inst);
	if (!state->active) return;

	drawFrame(inst, video, bmp);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	int i = 0;
	while (i < 8)
	{
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
		i++;
	}
}
