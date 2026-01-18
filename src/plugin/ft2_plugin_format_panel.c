/*
** FT2 Plugin - Sample Format/Rate Panel
** Resamples current sample to a target rate and format.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_format_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_common.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

#define FORMAT_STATE(inst) (&FT2_UI(inst)->modalPanels.format)

#define PANEL_X 186
#define PANEL_Y 230
#define PANEL_W 300
#define PANEL_H 92

static ft2_sample_t *getCurrentSample(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

static ft2_sample_format_t getSampleFormat(const ft2_sample_t *s)
{
	if (s == NULL) return FT2_SMP_FORMAT_8BIT;
	if (s->format == FT2_SMP_FORMAT_24BIT || s->format == FT2_SMP_FORMAT_FLOAT32)
		return s->format;
	return (s->flags & SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
}

static uint8_t getSampleBitDepth(const ft2_sample_t *s)
{
	switch (getSampleFormat(s))
	{
		default:
		case FT2_SMP_FORMAT_8BIT: return 8;
		case FT2_SMP_FORMAT_16BIT: return 16;
		case FT2_SMP_FORMAT_24BIT: return 24;
		case FT2_SMP_FORMAT_FLOAT32: return 32;
	}
}

static const char *getSampleFormatLabel(const ft2_sample_t *s)
{
	switch (getSampleFormat(s))
	{
		case FT2_SMP_FORMAT_FLOAT32: return "32 bit float";
		case FT2_SMP_FORMAT_24BIT: return "24 bit signed";
		case FT2_SMP_FORMAT_16BIT: return "16 bit signed";
		case FT2_SMP_FORMAT_8BIT: return "8 bit signed";
		default: return "---";
	}
}

static bool sampleHasData(const ft2_sample_t *s)
{
	if (s == NULL) return false;
	switch (getSampleFormat(s))
	{
		case FT2_SMP_FORMAT_24BIT: return (s->dataPtr24 != NULL);
		case FT2_SMP_FORMAT_FLOAT32: return (s->dataPtrF != NULL);
		default: return (s->dataPtr != NULL);
	}
}

static uint32_t getSampleCurrentRate(const ft2_instance_t *inst, const ft2_sample_t *s)
{
	if (s == NULL) return 0;

	uint32_t rate = s->sampleRate;
	if (rate == 0)
		rate = (uint32_t)round(ft2_sample_get_playback_rate(inst, s));
	if (rate == 0)
		rate = 8363;

	return rate;
}

static void syncFormatButtons(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *w = &FT2_UI(inst)->widgets;
	format_panel_state_t *state = FORMAT_STATE(inst);

	w->pushButtonLocked[PB_RES_3] = (state->targetFormat == FT2_SMP_FORMAT_8BIT);
	w->pushButtonLocked[PB_RES_4] = (state->targetFormat == FT2_SMP_FORMAT_16BIT);
	w->pushButtonLocked[PB_RES_5] = (state->targetFormat == FT2_SMP_FORMAT_24BIT);
	w->pushButtonLocked[PB_RES_6] = (state->targetFormat == FT2_SMP_FORMAT_FLOAT32);
}

static void onFormat8(ft2_instance_t *inst)  { FORMAT_STATE(inst)->targetFormat = FT2_SMP_FORMAT_8BIT; syncFormatButtons(inst); }
static void onFormat16(ft2_instance_t *inst) { FORMAT_STATE(inst)->targetFormat = FT2_SMP_FORMAT_16BIT; syncFormatButtons(inst); }
static void onFormat24(ft2_instance_t *inst) { FORMAT_STATE(inst)->targetFormat = FT2_SMP_FORMAT_24BIT; syncFormatButtons(inst); }
static void onFormat32f(ft2_instance_t *inst) { FORMAT_STATE(inst)->targetFormat = FT2_SMP_FORMAT_FLOAT32; syncFormatButtons(inst); }

static void onApplyClick(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	format_panel_state_t *state = FORMAT_STATE(inst);
	uint32_t rate = (uint32_t)atoi(state->rateBuffer);
	if (rate > 0)
		ft2_sample_ed_resample_format(inst, state->targetFormat, rate);
	ft2_format_panel_hide(inst);
}
static void onExitClick(ft2_instance_t *inst) { ft2_format_panel_hide(inst); }

static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	const int16_t buttonY = PANEL_Y + PANEL_H - 22;
	const int16_t buttonW = 54;
	const int16_t buttonGap = 10;
	const int16_t buttonsTotal = (buttonW * 2) + buttonGap;
	const int16_t buttonsX = PANEL_X + (PANEL_W - buttonsTotal) / 2;
	const int16_t formatY = PANEL_Y + 40;

	pushButton_t *p = &widgets->pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Apply"; p->x = buttonsX; p->y = buttonY; p->w = buttonW; p->h = 16;
	p->callbackFuncOnUp = onApplyClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel"; p->x = buttonsX + buttonW + buttonGap; p->y = buttonY; p->w = buttonW; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_2] = true;

	p = &widgets->pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "8"; p->x = PANEL_X + 150; p->y = formatY; p->w = 32; p->h = 16;
	p->callbackFuncOnUp = onFormat8; widgets->pushButtonVisible[PB_RES_3] = true;

	p = &widgets->pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "16"; p->x = PANEL_X + 186; p->y = formatY; p->w = 32; p->h = 16;
	p->callbackFuncOnUp = onFormat16; widgets->pushButtonVisible[PB_RES_4] = true;

	p = &widgets->pushButtons[PB_RES_5];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "24"; p->x = PANEL_X + 222; p->y = formatY; p->w = 32; p->h = 16;
	p->callbackFuncOnUp = onFormat24; widgets->pushButtonVisible[PB_RES_5] = true;

	p = &widgets->pushButtons[PB_RES_6];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "32f"; p->x = PANEL_X + 258; p->y = formatY; p->w = 32; p->h = 16;
	p->callbackFuncOnUp = onFormat32f; widgets->pushButtonVisible[PB_RES_6] = true;

	syncFormatButtons(inst);
}

static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 4; i++) widgets->pushButtonLocked[PB_RES_3 + i] = false;
}

static void drawFrame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = PANEL_X, y = PANEL_Y, w = PANEL_W, h = PANEL_H;
	format_panel_state_t *state = FORMAT_STATE(inst);

	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	const int16_t headerY = y + 6;
	const int16_t labelY = y + 24;

	ft2_sample_t *s = getCurrentSample(inst);
	const uint32_t currentRate = s ? getSampleCurrentRate(inst, s) : 0;
	const char *currentFormat = s ? getSampleFormatLabel(s) : NULL;

	char currentStr[32];
	if (currentRate > 0 && currentFormat != NULL && currentFormat[0] != '\0')
		snprintf(currentStr, sizeof(currentStr), "Current: %u Hz / %s", currentRate, currentFormat);
	else
		snprintf(currentStr, sizeof(currentStr), "Current: ---");

	textOutShadow(video, bmp, x + 10, headerY, PAL_FORGRND, PAL_BUTTON2, currentStr);
	textOutShadow(video, bmp, x + 10, labelY, PAL_FORGRND, PAL_BUTTON2, "Target rate (Hz)");
	textOutShadow(video, bmp, x + 150, labelY, PAL_FORGRND, PAL_BUTTON2, "Bit depth");

	(void)state;
}

void ft2_format_panel_draw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !inst->ui || !video) return;
	format_panel_state_t *state = FORMAT_STATE(inst);
	if (!state->active) return;

	drawFrame(inst, video, bmp);
	ft2_textbox_draw_with_cursor(video, bmp, TB_FORMAT_RATE, ft2_textbox_get_blink_state(inst), inst);

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
}

void ft2_format_panel_show(ft2_instance_t *inst)
{
	if (!inst || !inst->ui || !inst->config.enableXm2Features) return;
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || !sampleHasData(s) || s->length == 0) return;

	format_panel_state_t *state = FORMAT_STATE(inst);
	state->active = true;
	state->targetFormat = getSampleFormat(s);

	uint32_t rate = getSampleCurrentRate(inst, s);

	snprintf(state->rateBuffer, sizeof(state->rateBuffer), "%u", (unsigned int)rate);
	ft2_textbox_configure(inst, TB_FORMAT_RATE, state->rateBuffer, 7, true, true, PAL_BCKGRND);
	ft2_textbox_activate_id(inst, TB_FORMAT_RATE);

	setupWidgets(inst);
	ft2_modal_panel_set_active(inst, MODAL_PANEL_FORMAT);
}

void ft2_format_panel_hide(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	format_panel_state_t *state = FORMAT_STATE(inst);
	if (!state->active) return;

	hideWidgets(inst);
	ft2_textbox_hide(inst, TB_FORMAT_RATE);
	state->active = false;
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_modal_panel_set_inactive(inst, MODAL_PANEL_FORMAT);
}

bool ft2_format_panel_is_active(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return false;
	return FORMAT_STATE(inst)->active;
}

bool ft2_format_panel_key_down(ft2_instance_t *inst, int keycode)
{
	if (!inst || !inst->ui) return false;
	format_panel_state_t *state = FORMAT_STATE(inst);
	if (!state->active) return false;

	if (keycode == FT2_KEY_RETURN)
	{
		uint32_t rate = (uint32_t)atoi(state->rateBuffer);
		if (rate > 0)
			ft2_sample_ed_resample_format(inst, state->targetFormat, rate);
		ft2_format_panel_hide(inst);
		return true;
	}

	if (keycode == FT2_KEY_ESCAPE)
	{
		ft2_format_panel_hide(inst);
		return true;
	}

	ft2_textbox_handle_key(inst, keycode, 0);

	return true;
}

bool ft2_format_panel_char_input(ft2_instance_t *inst, char c)
{
	if (!inst || !inst->ui) return false;
	format_panel_state_t *state = FORMAT_STATE(inst);
	if (!state->active) return false;

	ft2_textbox_input_char(inst, c);
	return true;
}
