/*
** FT2 Plugin - Wave Generator Length Input Panel
** Prompts for waveform cycle length, then generates triangle/saw/sine/square.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LOOP_FWD 1

typedef struct {
	bool active;
	ft2_instance_t *instance;
	wave_type_t waveType;
	char inputBuffer[16];
	int inputCursorPos;
} wave_panel_state_t;

static wave_panel_state_t state = { .active = false, .instance = NULL, .waveType = WAVE_TYPE_TRIANGLE, .inputBuffer = "64", .inputCursorPos = 2 };
static int32_t lastWaveLength = 64;

static void onOKClick(ft2_instance_t *inst);
static void onCancelClick(ft2_instance_t *inst);

/* ------------------------------------------------------------------------- */
/*                             HELPERS                                       */
/* ------------------------------------------------------------------------- */

/* Allocate/replace sample with new 16-bit buffer */
static ft2_sample_t *setupNewSample(ft2_instance_t *inst, int32_t length)
{
	if (!inst || inst->editor.curInstr == 0) return NULL;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr)
	{
		ft2_instance_alloc_instr(inst, inst->editor.curInstr);
		instr = inst->replayer.instr[inst->editor.curInstr];
		if (!instr) return NULL;
	}

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_stop_sample_voices(inst, s);

	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = NULL; s->dataPtr = NULL; }

	int32_t allocLen = (length * 2) + 64; /* 16-bit + padding */
	s->origDataPtr = (int8_t *)malloc(allocLen);
	if (!s->origDataPtr) return NULL;

	memset(s->origDataPtr, 0, allocLen);
	s->dataPtr = s->origDataPtr + 8; /* SMP_DAT_OFFSET */
	s->length = length;
	s->loopStart = s->loopLength = 0;
	s->volume = 64;
	s->panning = 128;
	s->finetune = s->relativeNote = 0;
	s->flags = FT2_SAMPLE_16BIT;

	return s;
}

/* ------------------------------------------------------------------------- */
/*                       WAVE GENERATION                                     */
/* ------------------------------------------------------------------------- */

/* Triangle: ramps from -1 to +1 and back, piecewise linear */
static void generateTriangle(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double delta = 4.0 / lastWaveLength;
	double phase = 0.0;
	int16_t *ptr = (int16_t *)s->dataPtr;

	for (int32_t i = 0; i < newLength; i++)
	{
		double t = (phase > 3.0) ? phase - 4.0 : (phase >= 1.0) ? 2.0 - phase : phase;
		*ptr++ = (int16_t)(t * INT16_MAX);
		phase = fmod(phase + delta, 4.0);
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Sawtooth: linear ramp using 64-bit fixed point */
static void generateSaw(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	uint64_t point64 = 0, delta64 = ((uint64_t)(INT16_MAX * 2) << 32ULL) / lastWaveLength;
	int16_t *ptr = (int16_t *)s->dataPtr;

	for (int32_t i = 0; i < newLength; i++, point64 += delta64)
		*ptr++ = (int16_t)(point64 >> 32);

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Sine */
static void generateSine(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double dMul = (2.0 * M_PI) / lastWaveLength;
	int16_t *ptr = (int16_t *)s->dataPtr;

	for (int32_t i = 0; i < newLength; i++)
		*ptr++ = (int16_t)(INT16_MAX * sin(i * dMul));

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Square: 50% duty cycle */
static void generateSquare(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	int32_t cycles = getSfxCycles(inst);
	uint32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const uint32_t half = lastWaveLength / 2;
	int16_t val = INT16_MAX;
	uint32_t counter = 0;
	int16_t *ptr = (int16_t *)s->dataPtr;

	for (uint32_t i = 0; i < newLength; i++)
	{
		*ptr++ = val;
		if (++counter >= half) { counter = 0; val = -val; }
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

static void applyWaveGeneration(void)
{
	if (!state.active || !state.instance) return;
	int32_t newLen = atoi(state.inputBuffer);
	if (newLen < 2 || newLen > 65536) return;
	lastWaveLength = newLen;

	switch (state.waveType)
	{
		case WAVE_TYPE_TRIANGLE: generateTriangle(state.instance); break;
		case WAVE_TYPE_SAW:      generateSaw(state.instance); break;
		case WAVE_TYPE_SINE:     generateSine(state.instance); break;
		case WAVE_TYPE_SQUARE:   generateSquare(state.instance); break;
	}
}

/* ------------------------------------------------------------------------- */
/*                          WIDGET SETUP                                     */
/* ------------------------------------------------------------------------- */

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	pushButton_t *p;
	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "OK"; p->x = 246; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onOKClick; widgets->pushButtonVisible[PB_RES_1] = true;

	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel"; p->x = 346; p->y = 291; p->w = 80; p->h = 16;
	p->callbackFuncOnUp = onCancelClick; widgets->pushButtonVisible[PB_RES_2] = true;
}

static void hideWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	ft2_widgets_t *widgets = (inst && inst->ui) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
}

/* ------------------------------------------------------------------------- */
/*                          CALLBACKS                                        */
/* ------------------------------------------------------------------------- */

static void onOKClick(ft2_instance_t *inst) { (void)inst; applyWaveGeneration(); ft2_wave_panel_hide(); }
static void onCancelClick(ft2_instance_t *inst) { (void)inst; ft2_wave_panel_hide(); }

/* ------------------------------------------------------------------------- */
/*                           DRAWING                                         */
/* ------------------------------------------------------------------------- */

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 186, y = 249, w = 300, h = 67;

	/* Background and borders */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);

	/* Centered headline */
	const char *headline = "Enter new waveform length:";
	textOut(video, bmp, (632 - textWidth(headline)) / 2, y + 4, PAL_FORGRND, headline);

	/* Input field */
	int ix = x + 10, iy = y + 28, iw = w - 20, ih = 12;
	fillRect(video, ix, iy, iw, ih, PAL_DESKTOP);
	hLine(video, ix, iy, iw, PAL_BUTTON2);
	vLine(video, ix, iy, ih, PAL_BUTTON2);
	hLine(video, ix, iy + ih - 1, iw, PAL_BUTTON1);
	vLine(video, ix + iw - 1, iy, ih, PAL_BUTTON1);
	textOut(video, bmp, ix + 2, iy + 2, PAL_FORGRND, state.inputBuffer);
	vLine(video, ix + 2 + textWidth(state.inputBuffer), iy + 2, 8, PAL_FORGRND);
}

/* ------------------------------------------------------------------------- */
/*                          PUBLIC API                                       */
/* ------------------------------------------------------------------------- */

void ft2_wave_panel_show(ft2_instance_t *inst, wave_type_t waveType)
{
	if (!inst || inst->editor.curInstr == 0) return;
	state.active = true;
	state.instance = inst;
	state.waveType = waveType;
	snprintf(state.inputBuffer, sizeof(state.inputBuffer), "%d", lastWaveLength);
	state.inputCursorPos = (int)strlen(state.inputBuffer);
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_WAVE);
}

void ft2_wave_panel_hide(void)
{
	if (!state.active) return;
	hideWidgets();
	state.active = false;
	if (state.instance) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_WAVE);
}

bool ft2_wave_panel_is_active(void) { return state.active; }

void ft2_wave_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || !video) return;
	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance && state.instance->ui) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (!widgets) return;

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
}

/* ------------------------------------------------------------------------- */
/*                        INPUT HANDLING                                     */
/* ------------------------------------------------------------------------- */

bool ft2_wave_panel_key_down(int keycode)
{
	if (!state.active) return false;

	if (keycode == FT2_KEY_RETURN) { applyWaveGeneration(); ft2_wave_panel_hide(); return true; }
	if (keycode == FT2_KEY_ESCAPE) { ft2_wave_panel_hide(); return true; }
	if (keycode == FT2_KEY_BACKSPACE)
	{
		int len = (int)strlen(state.inputBuffer);
		if (len > 0) { state.inputBuffer[len - 1] = '\0'; state.inputCursorPos = len - 1; }
		return true;
	}
	return true;
}

bool ft2_wave_panel_char_input(char c)
{
	if (!state.active) return false;
	if (c >= '0' && c <= '9')
	{
		int len = (int)strlen(state.inputBuffer);
		if (len < 5) { state.inputBuffer[len] = c; state.inputBuffer[len + 1] = '\0'; state.inputCursorPos = len + 1; }
	}
	return true;
}

ft2_instance_t *ft2_wave_panel_get_instance(void) { return state.instance; }
