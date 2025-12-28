/**
 * @file ft2_plugin_wave_panel.c
 * @brief Wave generator length input panel implementation
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
#include "../ft2_instance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LOOP_FWD 1

/* Panel state */
typedef struct {
	bool active;
	ft2_instance_t *instance;
	wave_type_t waveType;
	char inputBuffer[16];
	int inputCursorPos;
} wave_panel_state_t;

static wave_panel_state_t state = {
	.active = false,
	.instance = NULL,
	.waveType = WAVE_TYPE_TRIANGLE,
	.inputBuffer = "64",
	.inputCursorPos = 2
};

/* Persistent values */
static int32_t lastWaveLength = 64;

/* Forward declarations */
static void onOKClick(ft2_instance_t *inst);
static void onCancelClick(ft2_instance_t *inst);

/* Sample setup helper */
static ft2_sample_t *setupNewSample(ft2_instance_t *inst, int32_t length)
{
	if (inst == NULL || inst->editor.curInstr == 0)
		return NULL;
	
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (instr == NULL)
	{
		ft2_instance_alloc_instr(inst, inst->editor.curInstr);
		instr = inst->replayer.instr[inst->editor.curInstr];
		if (instr == NULL)
			return NULL;
	}
	
	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	
	/* Stop voices playing this sample before replacing it */
	ft2_stop_sample_voices(inst, s);
	
	/* Free existing sample data */
	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->origDataPtr = NULL;
		s->dataPtr = NULL;
	}
	
	/* Allocate new sample data (always 16-bit) */
	int32_t allocLen = (length * 2) + 64; /* 16-bit + padding */
	s->origDataPtr = (int8_t *)malloc(allocLen);
	if (s->origDataPtr == NULL)
		return NULL;
	
	memset(s->origDataPtr, 0, allocLen);
	s->dataPtr = s->origDataPtr + 8; /* SMP_DAT_OFFSET */
	
	s->length = length;
	s->loopStart = 0;
	s->loopLength = 0;
	s->volume = 64;
	s->panning = 128;
	s->finetune = 0;
	s->relativeNote = 0;
	s->flags = FT2_SAMPLE_16BIT;
	
	return s;
}

/* Wave generation functions */
static void generateTriangle(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1)
		return;
	
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (s == NULL)
		return;
	
	const double delta = 4.0 / lastWaveLength;
	double phase = 0.0;
	
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
	{
		double t = phase;
		if (t > 3.0)
			t -= 4.0;
		else if (t >= 1.0)
			t = 2.0 - t;
		
		*ptr16++ = (int16_t)(t * INT16_MAX);
		phase = fmod(phase + delta, 4.0);
	}
	
	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	
	inst->uiState.updateSampleEditor = true;
}

static void generateSaw(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1)
		return;
	
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (s == NULL)
		return;
	
	uint64_t point64 = 0;
	uint64_t delta64 = ((uint64_t)(INT16_MAX * 2) << 32ULL) / lastWaveLength;
	
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = (int16_t)(point64 >> 32);
		point64 += delta64;
	}
	
	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	
	inst->uiState.updateSampleEditor = true;
}

static void generateSine(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1)
		return;
	
	int32_t cycles = getSfxCycles(inst);
	int32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (s == NULL)
		return;
	
	const double dMul = (2.0 * M_PI) / lastWaveLength;
	
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
		*ptr16++ = (int16_t)(INT16_MAX * sin(i * dMul));
	
	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	
	inst->uiState.updateSampleEditor = true;
}

static void generateSquare(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1)
		return;
	
	int32_t cycles = getSfxCycles(inst);
	uint32_t newLength = lastWaveLength * cycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (s == NULL)
		return;
	
	const uint32_t halfWaveLength = lastWaveLength / 2;
	
	int16_t currValue = INT16_MAX;
	uint32_t counter = 0;
	
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (uint32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = currValue;
		if (++counter >= halfWaveLength)
		{
			counter = 0;
			currValue = -currValue;
		}
	}
	
	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	
	inst->uiState.updateSampleEditor = true;
}

static void applyWaveGeneration(void)
{
	if (!state.active || state.instance == NULL)
		return;
	
	int32_t newLen = atoi(state.inputBuffer);
	if (newLen < 2 || newLen > 65536)
		return;
	
	lastWaveLength = newLen;
	
	switch (state.waveType)
	{
		case WAVE_TYPE_TRIANGLE:
			generateTriangle(state.instance);
			break;
		case WAVE_TYPE_SAW:
			generateSaw(state.instance);
			break;
		case WAVE_TYPE_SINE:
			generateSine(state.instance);
			break;
		case WAVE_TYPE_SQUARE:
			generateSquare(state.instance);
			break;
	}
}

/* Widget setup */
static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL)
		return;
	
	pushButton_t *p;
	
	/* OK button */
	p = &pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "OK";
	p->x = 246;
	p->y = 291;
	p->w = 80;
	p->h = 16;
	p->callbackFuncOnUp = onOKClick;
	p->visible = true;
	
	/* Cancel button */
	p = &pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Cancel";
	p->x = 346;
	p->y = 291;
	p->w = 80;
	p->h = 16;
	p->callbackFuncOnUp = onCancelClick;
	p->visible = true;
}

static void hideWidgets(void)
{
	for (int i = 0; i < 8; i++)
		hidePushButton(PB_RES_1 + i);
}

/* Widget callbacks */
static void onOKClick(ft2_instance_t *inst)
{
	(void)inst;
	applyWaveGeneration();
	ft2_wave_panel_hide();
}

static void onCancelClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_wave_panel_hide();
}

/* Draw the panel */
static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 186;
	const int16_t y = 249;
	const int16_t w = 300;
	const int16_t h = 67;
	
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
	
	/* Title bottom line */
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);
	
	/* Headline */
	const char *headline = "Enter new waveform length:";
	int hlen = textWidth(headline);
	int headlineX = (632 - hlen) / 2;
	textOut(video, bmp, headlineX, y + 4, PAL_FORGRND, headline);
	
	/* Input field */
	int inputX = x + 10;
	int inputY = y + 28;
	int inputW = w - 20;
	int inputH = 12;
	
	/* Input box background */
	fillRect(video, inputX, inputY, inputW, inputH, PAL_DESKTOP);
	
	/* Input box border */
	hLine(video, inputX, inputY, inputW, PAL_BUTTON2);
	vLine(video, inputX, inputY, inputH, PAL_BUTTON2);
	hLine(video, inputX, inputY + inputH - 1, inputW, PAL_BUTTON1);
	vLine(video, inputX + inputW - 1, inputY, inputH, PAL_BUTTON1);
	
	/* Input text */
	textOut(video, bmp, inputX + 2, inputY + 2, PAL_FORGRND, state.inputBuffer);
	
	/* Cursor */
	int cursorX = inputX + 2 + textWidth(state.inputBuffer);
	vLine(video, cursorX, inputY + 2, 8, PAL_FORGRND);
}

/* Public API */

void ft2_wave_panel_show(ft2_instance_t *inst, wave_type_t waveType)
{
	if (inst == NULL)
		return;
	
	if (inst->editor.curInstr == 0)
		return;
	
	state.active = true;
	state.instance = inst;
	state.waveType = waveType;
	
	/* Initialize input with last wave length */
	snprintf(state.inputBuffer, sizeof(state.inputBuffer), "%d", lastWaveLength);
	state.inputCursorPos = (int)strlen(state.inputBuffer);
	
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_WAVE);
}

void ft2_wave_panel_hide(void)
{
	if (!state.active)
		return;
	
	hideWidgets();
	state.active = false;
	
	if (state.instance != NULL)
		state.instance->uiState.updateSampleEditor = true;
	
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_WAVE);
}

bool ft2_wave_panel_is_active(void)
{
	return state.active;
}

void ft2_wave_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL)
		return;
	
	drawFrame(video, bmp);
	
	/* Draw buttons */
	for (int i = 0; i < 8; i++)
	{
		if (pushButtons[PB_RES_1 + i].visible)
			drawPushButton(video, bmp, PB_RES_1 + i);
	}
}

bool ft2_wave_panel_key_down(int keycode)
{
	if (!state.active)
		return false;
	
	/* Enter confirms */
	if (keycode == FT2_KEY_RETURN)
	{
		applyWaveGeneration();
		ft2_wave_panel_hide();
		return true;
	}
	
	/* Escape cancels */
	if (keycode == FT2_KEY_ESCAPE)
	{
		ft2_wave_panel_hide();
		return true;
	}
	
	/* Backspace */
	if (keycode == FT2_KEY_BACKSPACE)
	{
		int len = (int)strlen(state.inputBuffer);
		if (len > 0)
		{
			state.inputBuffer[len - 1] = '\0';
			state.inputCursorPos = len - 1;
		}
		return true;
	}
	
	return true;
}

bool ft2_wave_panel_char_input(char c)
{
	if (!state.active)
		return false;
	
	/* Only accept digits */
	if (c >= '0' && c <= '9')
	{
		int len = (int)strlen(state.inputBuffer);
		if (len < 5) /* Max 5 digits (65536) */
		{
			state.inputBuffer[len] = c;
			state.inputBuffer[len + 1] = '\0';
			state.inputCursorPos = len + 1;
		}
	}
	
	return true;
}

ft2_instance_t *ft2_wave_panel_get_instance(void)
{
	return state.instance;
}

