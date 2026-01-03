/**
 * @file ft2_plugin_sample_ed.c
 * @brief Exact port of sample editor from ft2_sample_ed.c
 *
 * This is a full port of the FT2 sample editor drawing code, adapted for
 * the plugin architecture with instance-aware state.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_instance.h"

/* Use macros from ft2_plugin_video.h: SGN, ABS, CLAMP */

/* Forward declaration */
static ft2_sample_t *getCurrentSampleWithInst(ft2_sample_editor_t *editor, ft2_instance_t *inst);

/* ============ STATIC DRAWING FUNCTIONS ============ */

/* Sample line drawing - exact port from ft2_sample_ed.c */
static void sampleLine(ft2_video_t *video, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	int32_t d;
	const int32_t dx = x2 - x1;
	const int32_t ax = ABS(dx) * 2;
	const int32_t sx = SGN(dx);
	const int32_t dy = y2 - y1;
	const int32_t ay = ABS(dy) * 2;
	const int32_t sy = SGN(dy);
	int32_t x = x1;
	int32_t y = y1;

	const uint32_t pixVal = video->palette[PAL_PATTEXT];

	if (ax > ay)
	{
		d = ay - (ax / 2);
		while (true)
		{
			if (y >= 0 && y < SCREEN_H && x >= 0 && x < SAMPLE_AREA_WIDTH)
			{
				if (y >= SAMPLE_AREA_Y_START && y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT)
					video->frameBuffer[(y * SCREEN_W) + x] = pixVal;
			}

			if (x == x2)
				break;

			if (d >= 0)
			{
				y += sy;
				d -= ax;
			}

			x += sx;
			d += ay;
		}
	}
	else
	{
		d = ax - (ay / 2);
		while (true)
		{
			if (y >= 0 && y < SCREEN_H && x >= 0 && x < SAMPLE_AREA_WIDTH)
			{
				if (y >= SAMPLE_AREA_Y_START && y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT)
					video->frameBuffer[(y * SCREEN_W) + x] = pixVal;
			}

			if (y == y2)
				break;

			if (d >= 0)
			{
				x += sx;
				d -= ay;
			}

			y += sy;
			d += ax;
		}
	}
}

/* Get scaled sample value for display */
static int32_t getScaledSample(ft2_sample_editor_t *ed, ft2_sample_t *s, int32_t pos)
{
	if (s == NULL || s->dataPtr == NULL || pos < 0 || pos >= s->length)
		return SAMPLE_AREA_Y_CENTER;

	int32_t sample;
	
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr;
		sample = ptr16[pos];
		/* Scale from -32768..32767 to display range */
		sample = SAMPLE_AREA_Y_CENTER - ((sample * (SAMPLE_AREA_HEIGHT / 2)) / 32768);
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr;
		sample = ptr8[pos];
		/* Scale from -128..127 to display range */
		sample = SAMPLE_AREA_Y_CENTER - ((sample * (SAMPLE_AREA_HEIGHT / 2)) / 128);
	}

	return CLAMP(sample, SAMPLE_AREA_Y_START, SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT - 1);
}

/* Convert mouse Y position to sample value (0-255 range) for draw mode */
static int32_t mouseYToSampleY(int32_t my)
{
	my -= SAMPLE_AREA_Y_START;
	const double dTmp = my * (256.0 / SAMPLE_AREA_HEIGHT);
	const int32_t tmp32 = (int32_t)(dTmp + 0.5);
	return 255 - CLAMP(tmp32, 0, 255);
}

/* Get sample data peak (min/max) for a range */
static void getSampleDataPeak(ft2_sample_t *s, int32_t start, int32_t count, int16_t *outMin, int16_t *outMax)
{
	if (s == NULL || s->dataPtr == NULL || count <= 0)
	{
		*outMin = SAMPLE_AREA_Y_CENTER;
		*outMax = SAMPLE_AREA_Y_CENTER;
		return;
	}

	int32_t min = 0x7FFFFFFF;
	int32_t max = -0x7FFFFFFF;

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr;
		for (int32_t i = 0; i < count; i++)
		{
			int32_t pos = start + i;
			if (pos >= s->length)
				break;

			int32_t sample = ptr16[pos];
			if (sample < min) min = sample;
			if (sample > max) max = sample;
		}
		/* Scale to display coordinates */
		min = SAMPLE_AREA_Y_CENTER - ((min * (SAMPLE_AREA_HEIGHT / 2)) / 32768);
		max = SAMPLE_AREA_Y_CENTER - ((max * (SAMPLE_AREA_HEIGHT / 2)) / 32768);
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr;
		for (int32_t i = 0; i < count; i++)
		{
			int32_t pos = start + i;
			if (pos >= s->length)
				break;

			int32_t sample = ptr8[pos];
			if (sample < min) min = sample;
			if (sample > max) max = sample;
		}
		/* Scale to display coordinates */
		min = SAMPLE_AREA_Y_CENTER - ((min * (SAMPLE_AREA_HEIGHT / 2)) / 128);
		max = SAMPLE_AREA_Y_CENTER - ((max * (SAMPLE_AREA_HEIGHT / 2)) / 128);
	}

	*outMin = (int16_t)CLAMP(min, SAMPLE_AREA_Y_START, SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT - 1);
	*outMax = (int16_t)CLAMP(max, SAMPLE_AREA_Y_START, SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT - 1);
}

/* Update scaling factors - matches original ft2_sample_ed.c updateViewSize()/updateScrPos() */
static void updateScalingFactors(ft2_sample_editor_t *ed)
{
	if (ed == NULL)
		return;

	if (ed->viewSize > 0)
	{
		ed->dPos2ScrMul = (double)SAMPLE_AREA_WIDTH / ed->viewSize;
		ed->dScr2SmpPosMul = ed->viewSize * (1.0 / SAMPLE_AREA_WIDTH);
	}
	else
	{
		ed->dPos2ScrMul = 1.0;
		ed->dScr2SmpPosMul = 1.0;
	}

	/* Must use floor() here - matches original FT2 updateScrPos() */
	ed->dScrPosScaled = floor(ed->scrPos * ed->dPos2ScrMul);
}

/* Screen position to sample position - matches original scr2SmpPos() */
int32_t ft2_sample_scr2SmpPos(ft2_instance_t *inst, int32_t x)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || editor->viewSize <= 0)
		return 0;

	if (x < 0)
		x = 0;

	double dPos = (editor->dScrPosScaled + x) * editor->dScr2SmpPosMul;
	int32_t smpPos = (int32_t)dPos;

	/* Clamp to sample length if we have a sample */
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s != NULL && smpPos > s->length)
		smpPos = s->length;

	return smpPos;
}

/* Sample position to screen position - matches original smpPos2Scr() */
int32_t ft2_sample_smpPos2Scr(ft2_instance_t *inst, int32_t pos)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || editor->viewSize <= 0)
		return -1;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL)
		return -1;

	if (pos > s->length)
		pos = s->length;

	double dPos = (pos * editor->dPos2ScrMul) + 0.5; /* Pre-rounding bias needed */
	dPos -= editor->dScrPosScaled;

	/* Clamp to valid range */
	dPos = CLAMP(dPos, INT32_MIN, INT32_MAX);

	return (int32_t)dPos;
}

/* ============ PUBLIC FUNCTIONS ============ */

void ft2_sample_ed_init(ft2_sample_editor_t *editor, ft2_video_t *video)
{
	if (editor == NULL)
		return;

	memset(editor, 0, sizeof(ft2_sample_editor_t));
	editor->video = video;
	editor->currInstr = 1;
	editor->currSample = 0;
	editor->scrPos = 0;
	editor->viewSize = 0;
	editor->rangeStart = 0;
	editor->rangeEnd = 0;
	editor->hasRange = false;
	editor->oldSmpPosLine = -1;
	editor->dPos2ScrMul = 1.0;
	editor->dScr2SmpPosMul = 1.0;
	editor->dScrPosScaled = 0.0;
}

void ft2_sample_ed_set_sample(ft2_instance_t *inst, int instr, int sample)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL)
		return;

	/* Check if instrument or sample slot changed */
	bool slotChanged = (editor->currInstr != instr || editor->currSample != sample);

	editor->currInstr = (int16_t)instr;
	editor->currSample = (int16_t)sample;

	/* Get sample length */
	int32_t smpLen = 0;
	if (inst != NULL && instr > 0 && instr < 128)
	{
		ft2_instr_t *ins = inst->replayer.instr[instr];
		if (ins != NULL && sample >= 0 && sample < 16)
		{
			smpLen = ins->smp[sample].length;
		}
	}
	
	/* Reset view only when:
	 * 1. Sample slot changed (different instr/sample selected), OR
	 * 2. Current view is invalid (viewSize > sample length or viewSize <= 0)
	 * 
	 * Note: Do NOT reset just because viewSize != smpLen - that just means
	 * the user has zoomed in, which is valid and should be preserved.
	 */
	bool needsViewReset = slotChanged ||
	                      (smpLen > 0 && editor->viewSize > smpLen) ||
	                      (smpLen > 0 && editor->viewSize <= 0);

	if (slotChanged)
	{
		editor->rangeStart = 0;
		editor->rangeEnd = 0;
		editor->hasRange = false;
	}

	if (needsViewReset)
	{
		editor->viewSize = smpLen;
		editor->scrPos = 0;
		editor->oldViewSize = smpLen;
		editor->oldScrPos = 0;
		editor->rangeStart = 0;
		editor->rangeEnd = 0;
		editor->hasRange = false;
	}

	/* Sanitize scroll position */
	if (smpLen > 0)
	{
		if (editor->scrPos + editor->viewSize > smpLen)
		{
			editor->scrPos = smpLen - editor->viewSize;
			if (editor->scrPos < 0)
			{
				editor->scrPos = 0;
				editor->viewSize = smpLen;
			}
		}
	}

	/* Configure the sample scrollbar - always sync to current editor state */
	ft2_widgets_t *widgets = (inst != NULL && inst->ui != NULL) ?
		&((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		if (smpLen > 0)
		{
			widgets->scrollBarState[SB_SAMP_SCROLL].end = (uint32_t)smpLen;
			widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
			widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
		}
		else
		{
			widgets->scrollBarState[SB_SAMP_SCROLL].end = 1;
			widgets->scrollBarState[SB_SAMP_SCROLL].page = 1;
			widgets->scrollBarState[SB_SAMP_SCROLL].pos = 0;
		}
	}

	updateScalingFactors(editor);
}

void ft2_sample_ed_draw_waveform(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed == NULL || ed->video == NULL || inst == NULL)
		return;

	ft2_video_t *video = ed->video;

	/* Clear sample data area */
	for (int32_t y = SAMPLE_AREA_Y_START; y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT; y++)
	{
		memset(&video->frameBuffer[y * SCREEN_W], 0, SAMPLE_AREA_WIDTH * sizeof(uint32_t));
	}

	/* Draw center line */
	hLine(video, 0, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);

	/* Get current sample */
	if (ed->currInstr <= 0 || ed->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[ed->currInstr];
	if (instr == NULL)
		return;

	if (ed->currSample < 0 || ed->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[ed->currSample];
	if (s->dataPtr == NULL || s->length <= 0)
		return;

	if (ed->viewSize <= 0)
		return;

	updateScalingFactors(ed);

	if (ed->viewSize <= SAMPLE_AREA_WIDTH)
	{
		/* Zoomed in (or 1:1) */
		for (int32_t x = 0; x <= SAMPLE_AREA_WIDTH; x++)
		{
			int32_t currSmpPos = ft2_sample_scr2SmpPos(inst, x);
			int32_t nextSmpPos = ft2_sample_scr2SmpPos(inst, x + 1);

			if (currSmpPos >= s->length) currSmpPos = s->length - 1;
			if (nextSmpPos >= s->length) nextSmpPos = s->length - 1;

			int32_t x1 = ft2_sample_smpPos2Scr(inst, currSmpPos);
			int32_t x2 = ft2_sample_smpPos2Scr(inst, nextSmpPos);
			int32_t y1 = getScaledSample(ed, s, currSmpPos);
			int32_t y2 = getScaledSample(ed, s, nextSmpPos);

			x1 = CLAMP(x1, 0, SAMPLE_AREA_WIDTH - 1);
			x2 = CLAMP(x2, 0, SAMPLE_AREA_WIDTH - 1);

			/* Kludge: sometimes the last point wouldn't reach the end of the sample window */
			if (x == SAMPLE_AREA_WIDTH)
				x2 = SAMPLE_AREA_WIDTH - 1;

			sampleLine(video, x1, x2, y1, y2);
		}
	}
	else
	{
		/* Zoomed out */
		const int32_t firstSamplePoint = getScaledSample(ed, s, ft2_sample_scr2SmpPos(inst, 0));

		int32_t oldMin = firstSamplePoint;
		int32_t oldMax = firstSamplePoint;

		for (int16_t x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			int32_t smpIdx = ft2_sample_scr2SmpPos(inst, x);
			int32_t smpNum = ft2_sample_scr2SmpPos(inst, x + 1) - smpIdx;

			/* Prevent look-up overflow */
			if (smpIdx + smpNum > s->length)
				smpNum = s->length - smpIdx;

			if (smpNum > 0)
			{
				int16_t min, max;
				getSampleDataPeak(s, smpIdx, smpNum, &min, &max);

				if (x != 0)
				{
					if (min > oldMax) sampleLine(video, x - 1, x, oldMax, min);
					if (max < oldMin) sampleLine(video, x - 1, x, oldMin, max);
				}

				sampleLine(video, x, x, max, min);

				oldMin = min;
				oldMax = max;
			}
		}
	}
}

void ft2_sample_ed_draw_range(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed == NULL || ed->video == NULL || !ed->hasRange)
		return;

	(void)inst; /* Used for coordinate conversion */
	ft2_video_t *video = ed->video;

	int32_t start = ed->rangeStart;
	int32_t end = ed->rangeEnd;

	/* Ensure start <= end */
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}

	/* Convert to screen coordinates */
	int32_t x1 = ft2_sample_smpPos2Scr(inst, start);
	int32_t x2 = ft2_sample_smpPos2Scr(inst, end);

	x1 = CLAMP(x1, 0, SAMPLE_AREA_WIDTH - 1);
	x2 = CLAMP(x2, 0, SAMPLE_AREA_WIDTH - 1);

	if (x1 > x2)
		return;

	/* Calculate range length - even point selection (x1 == x2) draws 1 pixel */
	int32_t rangeLen = (x2 + 1) - x1;

	/* Draw range by XORing with palette index 2 (range color) */
	for (int32_t y = SAMPLE_AREA_Y_START; y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT; y++)
	{
		uint32_t *ptr = &video->frameBuffer[(y * SCREEN_W) + x1];
		for (int32_t x = 0; x < rangeLen; x++, ptr++)
		{
			*ptr = video->palette[(*ptr >> 24) ^ 2];
		}
	}
}

/* Draw loop pin bitmap with transparency */
static void drawLoopPinSprite(ft2_video_t *video, const uint8_t *src8, int32_t xPos)
{
	if (video == NULL || src8 == NULL)
		return;

	const int32_t spriteW = 16;
	const int32_t spriteH = SAMPLE_AREA_HEIGHT;
	const int32_t yPos = SAMPLE_AREA_Y_START;

	int32_t sw = spriteW;
	int32_t sx = xPos;
	const uint8_t *srcPtr = src8;

	/* If x is negative, adjust variables */
	if (sx < 0)
	{
		sw += sx;
		srcPtr -= sx;
		sx = 0;
	}

	if (sw <= 0)
		return;

	/* Handle x clipping on right side */
	if (sx + sw > SCREEN_W)
		sw = SCREEN_W - sx;

	if (sw <= 0)
		return;

	int32_t srcPitch = spriteW - sw;
	int32_t dstPitch = SCREEN_W - sw;

	uint32_t *dst32 = &video->frameBuffer[(yPos * SCREEN_W) + sx];

	for (int32_t y = 0; y < spriteH; y++)
	{
		for (int32_t x = 0; x < sw; x++)
		{
			if (*srcPtr != PAL_TRANSPR)
				*dst32 = video->palette[*srcPtr];
			
			dst32++;
			srcPtr++;
		}

		srcPtr += srcPitch;
		dst32 += dstPitch;
	}
}

void ft2_sample_ed_draw_loop_points(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed == NULL || ed->video == NULL || inst == NULL || ed->bmp == NULL)
		return;

	ft2_video_t *video = ed->video;
	const ft2_bmp_t *bmp = ed->bmp;

	/* Check for loop pin bitmap */
	if (bmp->loopPins == NULL)
		return;

	/* Get current sample */
	if (ed->currInstr <= 0 || ed->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[ed->currInstr];
	if (instr == NULL)
		return;

	if (ed->currSample < 0 || ed->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[ed->currSample];
	uint8_t loopType = GET_LOOPTYPE(s->flags);

	if (loopType == LOOP_OFF || s->loopLength <= 0)
		return;

	int32_t loopStart = s->loopStart;
	int32_t loopEnd = loopStart + s->loopLength;

	/* Convert to screen coordinates */
	int32_t x1 = ft2_sample_smpPos2Scr(inst, loopStart);
	int32_t x2 = ft2_sample_smpPos2Scr(inst, loopEnd);

	/* Bitmap layout: 4 states, each 16 * 154 bytes
	 * State 0: left loop pin (normal)
	 * State 1: left loop pin (clicked)
	 * State 2: right loop pin (normal)
	 * State 3: right loop pin (clicked)
	 */
	const int32_t pinDataSize = 16 * SAMPLE_AREA_HEIGHT;

	/* Draw left loop pin if visible (position sprite at x - 8 so triangle is centered) */
	if (x1 >= -8 && x1 <= SAMPLE_AREA_WIDTH + 8)
	{
		int32_t state = inst->uiState.leftLoopPinMoving ? 1 : 0;
		const uint8_t *leftPinData = &bmp->loopPins[state * pinDataSize];
		drawLoopPinSprite(video, leftPinData, x1 - 8);
	}

	/* Draw right loop pin if visible */
	if (x2 >= -8 && x2 <= SAMPLE_AREA_WIDTH + 8)
	{
		int32_t state = inst->uiState.rightLoopPinMoving ? 3 : 2;
		const uint8_t *rightPinData = &bmp->loopPins[state * pinDataSize];
		drawLoopPinSprite(video, rightPinData, x2 - 8);
	}
}

/* Note name lookup tables */
static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };

static void updateSampleEditorRadioButtons(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	if (editor == NULL || inst == NULL)
		return;
	
	/* Get current sample */
	ft2_sample_t *s = NULL;
	if (editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		if (instr != NULL && editor->currSample >= 0 && editor->currSample < 16)
			s = &instr->smp[editor->currSample];
	}

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	
	if (s == NULL)
	{
		/* No sample - set defaults */
		if (widgets != NULL)
		{
			widgets->radioButtonState[RB_SAMPLE_NO_LOOP] = RADIOBUTTON_CHECKED;
			widgets->radioButtonState[RB_SAMPLE_FWD_LOOP] = RADIOBUTTON_UNCHECKED;
			widgets->radioButtonState[RB_SAMPLE_BIDI_LOOP] = RADIOBUTTON_UNCHECKED;
			widgets->radioButtonState[RB_SAMPLE_8BIT] = RADIOBUTTON_CHECKED;
			widgets->radioButtonState[RB_SAMPLE_16BIT] = RADIOBUTTON_UNCHECKED;
		}
		return;
	}

	/* Update loop type radio buttons */
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	if (widgets != NULL)
	{
		widgets->radioButtonState[RB_SAMPLE_NO_LOOP] = (loopType == LOOP_OFF) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
		widgets->radioButtonState[RB_SAMPLE_FWD_LOOP] = (loopType == LOOP_FWD) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
		widgets->radioButtonState[RB_SAMPLE_BIDI_LOOP] = (loopType == LOOP_BIDI) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
	}

	/* Update bit depth radio buttons */
	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;
	if (widgets != NULL)
	{
		widgets->radioButtonState[RB_SAMPLE_8BIT] = is16Bit ? RADIOBUTTON_UNCHECKED : RADIOBUTTON_CHECKED;
		widgets->radioButtonState[RB_SAMPLE_16BIT] = is16Bit ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
	}
}

static void drawPlayNote(ft2_video_t *video, const ft2_bmp_t *bmp, uint8_t noteNr)
{
	char noteStr[4];
	
	uint8_t note = noteNr % 12;
	uint8_t octave = noteNr / 12;
	
	noteStr[0] = sharpNote1Char[note];
	noteStr[1] = sharpNote2Char[note];
	noteStr[2] = '0' + octave;
	noteStr[3] = '\0';
	
	textOut(video, bmp, 5, 369, PAL_FORGRND, noteStr);
}

static void drawSmpEdHexValue(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t x, int16_t y, int32_t value)
{
	char str[12];
	snprintf(str, sizeof(str), "%08X", (unsigned int)value);
	textOut(video, bmp, x, y, PAL_FORGRND, str);
}

/* Draw sample playback position line */
void ft2_sample_ed_draw_pos_line(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || editor->video == NULL || inst == NULL)
		return;

	ft2_video_t *video = editor->video;

	/* Check if we're playing a sample in sample editor mode */
	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	if (editor->currSample < 0 || editor->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->dataPtr == NULL || s->length <= 0)
		return;

	/* Check each voice to see if it's playing this sample */
	for (int ch = 0; ch < FT2_MAX_CHANNELS; ch++)
	{
		ft2_voice_t *v = &inst->voice[ch];
		if (!v->active)
			continue;

		/* Check if this voice is playing the current instrument's sample */
		bool isSameSample = false;
		if (v->base8 != NULL && (const int8_t *)s->dataPtr == v->base8)
			isSameSample = true;
		else if (v->base16 != NULL && (const int16_t *)s->dataPtr == v->base16)
			isSameSample = true;

		if (!isSameSample)
			continue;

		/* Get the current position */
		int32_t smpPos = (int32_t)v->position;
		if (smpPos < 0 || smpPos >= s->length)
			continue;

		/* Convert to screen position */
		int32_t screenX = ft2_sample_smpPos2Scr(inst, smpPos);
		if (screenX < 0 || screenX >= SAMPLE_AREA_WIDTH)
			continue;

		/* Draw vertical line at position */
		for (int32_t y = SAMPLE_AREA_Y_START; y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT; y++)
		{
			uint32_t *ptr = &video->frameBuffer[(y * SCREEN_W) + screenX];
			*ptr = video->palette[PAL_PATTEXT]; /* Use pattern text color for contrast */
		}

		/* Only show one position line - first active voice playing this sample */
		break;
	}
}

void ft2_sample_ed_draw(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || editor->video == NULL)
		return;

	/* Get bmp from UI */
	const ft2_bmp_t *bmp = (inst != NULL && inst->ui != NULL) ? &FT2_UI(inst)->bmp : NULL;

	/* Store bmp pointer for loop pin drawing */
	editor->bmp = bmp;

	ft2_video_t *video = editor->video;

	/* Update radio button states to reflect current sample */
	updateSampleEditorRadioButtons(editor, inst);

	/* Draw sample editor framework - exact match of standalone showSampleEditor() */
	drawFramework(video, 0,   329, 632, 17, FRAMEWORK_TYPE1);  /* scrollbar area */

	/* Check if effects panel is shown */
	if (inst != NULL && inst->uiState.sampleEditorEffectsShown)
	{
		/* Draw effects panel - call into smpfx module */
		drawSampleEffectsScreen(inst, video, bmp);

		/* Draw right side - loop/bit controls and display values */
		drawFramework(video, 353, 346,  74, 54, FRAMEWORK_TYPE1);  /* loop/bit radio buttons */
		drawFramework(video, 427, 346, 205, 54, FRAMEWORK_TYPE1);  /* exit/display/values */

		/* Draw loop/bit labels */
		if (bmp != NULL)
		{
			textOutShadow(video, bmp, 371, 352, PAL_FORGRND, PAL_DSKTOP2, "No loop");
			textOutShadow(video, bmp, 371, 369, PAL_FORGRND, PAL_DSKTOP2, "Forward");
			textOutShadow(video, bmp, 371, 386, PAL_FORGRND, PAL_DSKTOP2, "Pingpong");
			textOutShadow(video, bmp, 446, 369, PAL_FORGRND, PAL_DSKTOP2, "8-bit");
			textOutShadow(video, bmp, 445, 384, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
			textOutShadow(video, bmp, 488, 350, PAL_FORGRND, PAL_DSKTOP2, "Display");
			textOutShadow(video, bmp, 488, 362, PAL_FORGRND, PAL_DSKTOP2, "Length");
			textOutShadow(video, bmp, 488, 375, PAL_FORGRND, PAL_DSKTOP2, "Repeat");
			textOutShadow(video, bmp, 488, 387, PAL_FORGRND, PAL_DSKTOP2, "Replen.");
		}
	}
	else
	{
		/* Draw normal sample editor controls */
	drawFramework(video, 0,   346, 115, 54, FRAMEWORK_TYPE1);  /* play section */
	drawFramework(video, 115, 346, 133, 54, FRAMEWORK_TYPE1);  /* show/range section */
	drawFramework(video, 248, 346,  49, 54, FRAMEWORK_TYPE1);  /* cut/copy/paste */
	drawFramework(video, 297, 346,  56, 54, FRAMEWORK_TYPE1);  /* crop/volume/effects */
	drawFramework(video, 353, 346,  74, 54, FRAMEWORK_TYPE1);  /* loop/bit radio buttons */
	drawFramework(video, 427, 346, 205, 54, FRAMEWORK_TYPE1);  /* exit/display/values */
	drawFramework(video, 2,   366,  34, 15, FRAMEWORK_TYPE2);  /* play note box */

	/* Draw labels - exact positions from standalone */
	if (bmp != NULL)
	{
		textOutShadow(video, bmp, 5,   352, PAL_FORGRND, PAL_DSKTOP2, "Play:");
		textOutShadow(video, bmp, 371, 352, PAL_FORGRND, PAL_DSKTOP2, "No loop");
		textOutShadow(video, bmp, 371, 369, PAL_FORGRND, PAL_DSKTOP2, "Forward");
		textOutShadow(video, bmp, 371, 386, PAL_FORGRND, PAL_DSKTOP2, "Pingpong");
		textOutShadow(video, bmp, 446, 369, PAL_FORGRND, PAL_DSKTOP2, "8-bit");
		textOutShadow(video, bmp, 445, 384, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
		textOutShadow(video, bmp, 488, 350, PAL_FORGRND, PAL_DSKTOP2, "Display");
		textOutShadow(video, bmp, 488, 362, PAL_FORGRND, PAL_DSKTOP2, "Length");
		textOutShadow(video, bmp, 488, 375, PAL_FORGRND, PAL_DSKTOP2, "Repeat");
		textOutShadow(video, bmp, 488, 387, PAL_FORGRND, PAL_DSKTOP2, "Replen.");

		/* Draw play note */
		if (inst != NULL)
			drawPlayNote(video, bmp, inst->editor.smpEd_NoteNr);
		}
	}

	/* Draw sample area background */
	fillRect(video, 0, SAMPLE_AREA_Y_START, SAMPLE_AREA_WIDTH, SAMPLE_AREA_HEIGHT, PAL_BCKGRND);

	/* Clear two lines in the sample data view that are never written to when the sampler is open */
	hLine(video, 0, 173, SAMPLE_AREA_WIDTH, PAL_BCKGRND);
	hLine(video, 0, 328, SAMPLE_AREA_WIDTH, PAL_BCKGRND);

	/* Draw waveform */
	ft2_sample_ed_draw_waveform(inst);

	/* Save old values after drawing waveform (for zoom calculations) */
	editor->oldScrPos = editor->scrPos;
	editor->oldViewSize = editor->viewSize;

	/* Draw range if selected */
	if (editor->hasRange)
		ft2_sample_ed_draw_range(inst);

	/* Draw loop points */
	ft2_sample_ed_draw_loop_points(inst);

	/* Draw sample playback position line */
	ft2_sample_ed_draw_pos_line(inst);

	/* Draw hex values for Display/Length/Repeat/Replen */
	if (bmp != NULL && inst != NULL && editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		if (instr != NULL && editor->currSample >= 0 && editor->currSample < 16)
		{
			ft2_sample_t *s = &instr->smp[editor->currSample];
			
			/* Display = view size - x=536 matches standalone */
			drawSmpEdHexValue(video, bmp, 536, 350, editor->viewSize);
			/* Length */
			drawSmpEdHexValue(video, bmp, 536, 362, s->length);
			/* Repeat (loop start) */
			drawSmpEdHexValue(video, bmp, 536, 375, s->loopStart);
			/* Replen (loop length) */
			drawSmpEdHexValue(video, bmp, 536, 387, s->loopLength);
		}
	}
}

/**
 * Zoom in sample data towards mouse X position.
 * Matches original ft2_sample_ed.c: mouseZoomSampleDataIn() / zoomSampleDataIn()
 */
void ft2_sample_ed_zoom_in(ft2_instance_t *inst, int32_t mouseX)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->dataPtr == NULL)
		return;

	if (editor->oldViewSize <= 2)
		return;

	/* Step is ~10% of view, like original */
	int32_t step = (editor->oldViewSize + 5) / 10;
	if (step < 1)
		step = 1;

	editor->viewSize = editor->oldViewSize - (step * 2);
	if (editor->viewSize < 2)
		editor->viewSize = 2;

	updateScalingFactors(editor);

	/* Calculate offset based on mouse position to zoom towards cursor */
	int32_t tmp32 = (mouseX - (SAMPLE_AREA_WIDTH / 2)) * step;
	tmp32 += SAMPLE_AREA_WIDTH / 4; /* rounding bias */
	tmp32 /= SAMPLE_AREA_WIDTH / 2;

	step += tmp32;

	int64_t newScrPos64 = editor->oldScrPos + step;
	if (newScrPos64 + editor->viewSize > s->length)
		newScrPos64 = s->length - editor->viewSize;
	if (newScrPos64 < 0)
		newScrPos64 = 0;

	editor->scrPos = (int32_t)newScrPos64;

	/* Update scrollbar */
	ft2_widgets_t *widgets = (inst->ui != NULL) ?
		&((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
	}
}

/**
 * Zoom out sample data from mouse X position.
 * Matches original ft2_sample_ed.c: mouseZoomSampleDataOut() / zoomSampleDataOut()
 */
void ft2_sample_ed_zoom_out(ft2_instance_t *inst, int32_t mouseX)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->dataPtr == NULL)
		return;

	if (editor->oldViewSize == s->length)
		return;

	/* Step is ~10% of view, like original */
	int32_t step = (editor->oldViewSize + 5) / 10;
	if (step < 1)
		step = 1;

	int64_t newViewSize64 = (int64_t)editor->oldViewSize + (step * 2);
	if (newViewSize64 > s->length)
	{
		editor->viewSize = s->length;
		editor->scrPos = 0;
	}
	else
	{
		/* Calculate offset based on mouse position */
		int32_t tmp32 = (mouseX - (SAMPLE_AREA_WIDTH / 2)) * step;
		tmp32 += SAMPLE_AREA_WIDTH / 4; /* rounding bias */
		tmp32 /= SAMPLE_AREA_WIDTH / 2;

		step += tmp32;

		editor->viewSize = (int32_t)(newViewSize64 & 0xFFFFFFFF);

		editor->scrPos = editor->oldScrPos - step;
		if (editor->scrPos < 0)
			editor->scrPos = 0;

		if (editor->scrPos + editor->viewSize > s->length)
			editor->scrPos = s->length - editor->viewSize;
	}

	updateScalingFactors(editor);

	/* Update scrollbar */
	ft2_widgets_t *widgets = (inst->ui != NULL) ?
		&((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
	}
}

void ft2_sample_ed_show_all(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	editor->scrPos = 0;

	/* Get sample length */
	int32_t smpLen = 0;
	if (editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		if (instr != NULL && editor->currSample >= 0 && editor->currSample < 16)
		{
			smpLen = instr->smp[editor->currSample].length;
		}
	}
	
	editor->viewSize = smpLen;

	/* Update scrollbar - must set end, page, and pos to sync properly */
	ft2_widgets_t *widgets = (inst->ui != NULL) ?
		&((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].end = (uint32_t)smpLen;
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = 0;
	}

	updateScalingFactors(editor);
}

void ft2_sample_ed_show_loop(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	/* Get current sample */
	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	if (editor->currSample < 0 || editor->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	uint8_t loopType = GET_LOOPTYPE(s->flags);

	if (loopType != LOOP_OFF && s->loopLength > 0)
	{
		editor->scrPos = s->loopStart;
		editor->viewSize = s->loopLength;

		/* Update scrollbar */
		ft2_widgets_t *widgets = (inst->ui != NULL) ?
			&((ft2_ui_t *)inst->ui)->widgets : NULL;
		if (widgets != NULL)
		{
			widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
			widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
		}

		updateScalingFactors(editor);
	}
}

void ft2_sample_ed_show_range(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	/* Get current sample */
	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	if (editor->currSample < 0 || editor->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->dataPtr == NULL)
		return;

	/* Check if we have a valid range (rangeStart < rangeEnd) */
	if (editor->rangeStart < editor->rangeEnd)
	{
		editor->viewSize = editor->rangeEnd - editor->rangeStart;
		if (editor->viewSize < 2)
			editor->viewSize = 2;

		editor->scrPos = editor->rangeStart;

		/* Update scrollbar */
		ft2_widgets_t *widgets = (inst->ui != NULL) ?
			&((ft2_ui_t *)inst->ui)->widgets : NULL;
		if (widgets != NULL)
		{
			widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
			widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
		}

		updateScalingFactors(editor);
		
		inst->uiState.updateSampleEditor = true;
	}
	else
	{
		/* Show error message - no valid range */
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL)
		{
			ft2_dialog_show_message(&ui->dialog, "System message", "Cannot show empty range!");
		}
	}
}

void ft2_sample_ed_set_selection(ft2_instance_t *inst, int32_t start, int32_t end)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL)
		return;

	(void)inst; /* Unused, but kept for API consistency */
	editor->rangeStart = start;
	editor->rangeEnd = end;
	/* Mark as valid selection if end > 0 (matches standalone's smpEd_Rx2 behavior) */
	editor->hasRange = (end > 0);
}

void ft2_sample_ed_clear_selection(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL)
		return;

	(void)inst; /* Unused, but kept for API consistency */
	editor->rangeStart = 0;
	editor->rangeEnd = 0;
	editor->hasRange = false;
}

void ft2_sample_ed_range_all(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	/* Get current sample */
	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return;

	if (editor->currSample < 0 || editor->currSample >= 16)
		return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->length > 0)
	{
		editor->rangeStart = 0;
		editor->rangeEnd = s->length;
		editor->hasRange = true;
	}
}

/* Get current loop pin screen X position based on loop points */
static int32_t getLeftLoopPinScreenX(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->loopLength <= 0)
		return -100;
	
	return ft2_sample_smpPos2Scr(inst, s->loopStart) - 8;
}

static int32_t getRightLoopPinScreenX(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->loopLength <= 0)
		return -100;
	
	return ft2_sample_smpPos2Scr(inst, s->loopStart + s->loopLength) - 8;
}

/* Set loop start position from screen X */
static void setLeftLoopPinPos(ft2_sample_editor_t *editor, int32_t x, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t newLoopStart = ft2_sample_scr2SmpPos(inst, x);
	int32_t loopEnd = s->loopStart + s->loopLength;

	if (newLoopStart < 0)
		newLoopStart = 0;

	if (newLoopStart >= loopEnd)
		newLoopStart = loopEnd - 1;

	if (newLoopStart < 0)
		newLoopStart = 0;

	/* Stop voices playing this sample before changing loop points (matches standalone) */
	ft2_stop_sample_voices(inst, s);

	s->loopStart = newLoopStart;
	s->loopLength = loopEnd - newLoopStart;

	if (s->loopLength < 0)
		s->loopLength = 0;

	inst->uiState.updateSampleEditor = true;
}

/* Set loop end position from screen X */
static void setRightLoopPinPos(ft2_sample_editor_t *editor, int32_t x, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t loopEnd = ft2_sample_scr2SmpPos(inst, x);

	if (loopEnd < s->loopStart)
		loopEnd = s->loopStart;

	if (loopEnd > s->length)
		loopEnd = s->length;

	/* Stop voices playing this sample before changing loop points (matches standalone) */
	ft2_stop_sample_voices(inst, s);

	s->loopLength = loopEnd - s->loopStart;

	if (s->loopLength < 0)
		s->loopLength = 0;

	inst->uiState.updateSampleEditor = true;
}

/* Edit sample data by drawing with right mouse button (ported from standalone) */
static void editSampleData(ft2_sample_editor_t *editor, int32_t mx, int32_t my, bool mouseButtonHeld, bool shiftPressed, ft2_instance_t *inst)
{
	if (editor == NULL || inst == NULL)
		return;
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int8_t *ptr8;
	int16_t *ptr16;
	int32_t tmp32, p, vl, tvl, r, rl, rvl, start, end;

	if (mx > SCREEN_W)
		mx = SCREEN_W;

	if (!mouseButtonHeld)
	{
		ft2_unfix_sample(s);
		inst->editor.editSampleFlag = true;

		editor->lastDrawX = ft2_sample_scr2SmpPos(inst, mx);
		editor->lastDrawY = mouseYToSampleY(my);

		editor->lastMouseX = mx;
		editor->lastMouseY = my;
	}
	else if (mx == editor->lastMouseX && my == editor->lastMouseY)
	{
		return;
	}

	if (mx != editor->lastMouseX)
		p = ft2_sample_scr2SmpPos(inst, mx);
	else
		p = editor->lastDrawX;

	if (!shiftPressed && my != editor->lastMouseY)
		vl = mouseYToSampleY(my);
	else
		vl = editor->lastDrawY;

	editor->lastMouseX = mx;
	editor->lastMouseY = my;

	r = p;
	rvl = vl;

	if (p > editor->lastDrawX)
	{
		tmp32 = p;
		p = editor->lastDrawX;
		editor->lastDrawX = tmp32;

		tmp32 = editor->lastDrawY;
		editor->lastDrawY = vl;
		vl = tmp32;
	}

	if (s->flags & SAMPLE_16BIT)
	{
		ptr16 = (int16_t *)s->dataPtr;

		start = p;
		if (start < 0)
			start = 0;

		end = editor->lastDrawX + 1;
		if (end > s->length)
			end = s->length;

		if (p == editor->lastDrawX)
		{
			const int16_t smpVal = (int16_t)((vl << 8) ^ 0x8000);
			for (rl = start; rl < end; rl++)
				ptr16[rl] = smpVal;
		}
		else
		{
			int32_t y = editor->lastDrawY - vl;
			int32_t x = editor->lastDrawX - p;

			if (x != 0)
			{
				double dMul = 1.0 / x;
				int32_t i = 0;

				for (rl = start; rl < end; rl++)
				{
					tvl = y * i;
					tvl = (int32_t)(tvl * dMul);
					tvl += vl;
					tvl <<= 8;
					tvl ^= 0x8000;

					ptr16[rl] = (int16_t)tvl;
					i++;
				}
			}
		}
	}
	else
	{
		ptr8 = s->dataPtr;

		start = p;
		if (start < 0)
			start = 0;

		end = editor->lastDrawX + 1;
		if (end > s->length)
			end = s->length;

		if (p == editor->lastDrawX)
		{
			const int8_t smpVal = (int8_t)(vl ^ 0x80);
			for (rl = start; rl < end; rl++)
				ptr8[rl] = smpVal;
		}
		else
		{
			int32_t y = editor->lastDrawY - vl;
			int32_t x = editor->lastDrawX - p;

			if (x != 0)
			{
				double dMul = 1.0 / x;
				int32_t i = 0;

				for (rl = start; rl < end; rl++)
				{
					tvl = y * i;
					tvl = (int32_t)(tvl * dMul);
					tvl += vl;
					tvl ^= 0x80;

					ptr8[rl] = (int8_t)tvl;
					i++;
				}
			}
		}
	}

	editor->lastDrawY = rvl;
	editor->lastDrawX = r;

	inst->uiState.updateSampleEditor = true;
}

void ft2_sample_ed_mouse_click(ft2_instance_t *inst, int x, int y, int button)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;
	
	/* Clamp coordinates */
	int32_t mx = CLAMP(x, 0, SCREEN_W + 8);
	int32_t my = CLAMP(y, 0, SCREEN_H - 1);

	/* Reset drag state */
	inst->uiState.leftLoopPinMoving = false;
	inst->uiState.rightLoopPinMoving = false;
	inst->uiState.sampleDataOrLoopDrag = -1;

	editor->mouseXOffs = 0;
	editor->lastMouseX = mx;
	editor->lastMouseY = my;

	if (button == 1) /* Left button */
	{
		/* Check for loop pin clicks */
		if (my >= SAMPLE_AREA_Y_START && my < SAMPLE_AREA_Y_START + 9) /* Top 9 pixels - left loop pin area */
		{
			int32_t leftPinPos = getLeftLoopPinScreenX(editor, inst);
			if (mx >= leftPinPos && mx <= leftPinPos + 16)
			{
				editor->mouseXOffs = (leftPinPos + 8) - mx;
				inst->uiState.sampleDataOrLoopDrag = 1;
				inst->uiState.leftLoopPinMoving = true;
				editor->lastMouseX = mx;
				inst->uiState.updateSampleEditor = true;
				return;
			}
		}
		else if (my >= SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT - 9) /* Bottom 9 pixels - right loop pin area */
		{
			int32_t rightPinPos = getRightLoopPinScreenX(editor, inst);
			if (mx >= rightPinPos && mx <= rightPinPos + 16)
			{
				editor->mouseXOffs = (rightPinPos + 8) - mx;
				inst->uiState.sampleDataOrLoopDrag = 1;
				inst->uiState.rightLoopPinMoving = true;
				editor->lastMouseX = mx;
				inst->uiState.updateSampleEditor = true;
				return;
			}
		}

		/* Not on a loop pin - start range selection */
		if (mx >= 0 && mx < SAMPLE_AREA_WIDTH)
		{
			editor->lastMouseX = mx;
			inst->uiState.sampleDataOrLoopDrag = mx;

			int32_t smpPos = ft2_sample_scr2SmpPos(inst, mx);
			editor->rangeStart = smpPos;
			editor->rangeEnd = smpPos;
			/* Mark as valid selection even for point - this allows paste to insert at position */
			editor->hasRange = true;
			inst->uiState.updateSampleEditor = true;
		}
	}
	else if (button == 2) /* Right button - draw mode */
	{
		/* Only allow drawing if we have an instrument selected */
		if (inst->editor.curInstr == 0)
			return;

		inst->uiState.sampleDataOrLoopDrag = 1;
		editSampleData(editor, mx, my, false, false, inst);
	}
}

void ft2_sample_ed_mouse_drag(ft2_instance_t *inst, int x, int y, bool shiftPressed)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	int32_t mx = CLAMP(x, 0, SCREEN_W + 8);
	int32_t my = CLAMP(y, 0, SCREEN_H - 1);

	/* Check if we're in draw mode (right-button drag) */
	if (inst->editor.editSampleFlag)
	{
		editSampleData(editor, mx, my, true, shiftPressed, inst);
		return;
	}

	if (mx == editor->lastMouseX)
		return;

	if (inst->uiState.leftLoopPinMoving)
	{
		editor->lastMouseX = mx;
		setLeftLoopPinPos(editor, editor->mouseXOffs + mx, inst);
	}
	else if (inst->uiState.rightLoopPinMoving)
	{
		editor->lastMouseX = mx;
		setRightLoopPinPos(editor, editor->mouseXOffs + mx, inst);
	}
	else if (inst->uiState.sampleDataOrLoopDrag >= 0)
	{
		/* Range selection */
		editor->lastMouseX = mx;

		int32_t dragStartX = inst->uiState.sampleDataOrLoopDrag;

		if (mx > dragStartX)
		{
			editor->rangeStart = ft2_sample_scr2SmpPos(inst, dragStartX);
			editor->rangeEnd = ft2_sample_scr2SmpPos(inst, mx);
		}
		else if (mx < dragStartX)
		{
			editor->rangeStart = ft2_sample_scr2SmpPos(inst, mx);
			editor->rangeEnd = ft2_sample_scr2SmpPos(inst, dragStartX);
		}
		else
		{
			editor->rangeStart = ft2_sample_scr2SmpPos(inst, mx);
			editor->rangeEnd = editor->rangeStart;
		}

		/* Always mark as valid selection - even point selection is valid for paste insert */
		editor->hasRange = true;
		inst->uiState.updateSampleEditor = true;
	}
}

void ft2_sample_ed_mouse_up(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	/* If we were drawing with right button, finalize */
	if (inst->editor.editSampleFlag)
	{
		ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
		if (s != NULL)
			ft2_fix_sample(s);

		inst->editor.editSampleFlag = false;
		inst->uiState.updateSampleEditor = true;
	}

	/* Clear loop pin moving states */
	if (inst->uiState.leftLoopPinMoving || inst->uiState.rightLoopPinMoving)
	{
		inst->uiState.leftLoopPinMoving = false;
		inst->uiState.rightLoopPinMoving = false;
		inst->uiState.updateSampleEditor = true;
	}

	inst->uiState.sampleDataOrLoopDrag = -1;
}

/* Clipboard for sample data (non-static for access from drawSampleEditorExt) */
static int8_t *sampleClipboard = NULL;
int32_t clipboardLength = 0;
static bool clipboardIs16Bit = false;
static bool clipboardDidCopyWholeSample = false;
static ft2_sample_t clipboardSampleInfo;

static ft2_sample_t *getCurrentSampleWithInst(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	if (editor == NULL || inst == NULL)
		return NULL;
	
	if (editor->currInstr <= 0 || editor->currInstr >= 128)
		return NULL;
	
	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (instr == NULL)
		return NULL;
	
	if (editor->currSample < 0 || editor->currSample >= 16)
		return NULL;
	
	return &instr->smp[editor->currSample];
}

void ft2_sample_ed_cut(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	/* Cut requires a range (not just a point) */
	if (editor == NULL || editor->rangeEnd == 0 || editor->rangeStart == editor->rangeEnd)
		return;
	
	if (inst != NULL && inst->config.smpCutToBuffer)
		ft2_sample_ed_copy(inst);
	
	ft2_sample_ed_delete(inst);
}

void ft2_sample_ed_copy(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;
	
	/* If no selection or rangeEnd is 0, copy whole sample */
	int32_t start, end;
	if (!editor->hasRange || editor->rangeEnd == 0)
	{
		start = 0;
		end = s->length;
	}
	else
	{
		start = editor->rangeStart;
		end = editor->rangeEnd;
		if (start > end)
		{
			int32_t tmp = start;
			start = end;
			end = tmp;
		}
	}
	
	if (start < 0) start = 0;
	if (end > s->length) end = s->length;
	
	int32_t len = end - start;
	if (len <= 0)
		return;
	
	/* Free old clipboard */
	if (sampleClipboard != NULL)
	{
		free(sampleClipboard);
		sampleClipboard = NULL;
	}
	
	/* Copy to clipboard */
	int32_t bytesPerSample = (s->flags & SAMPLE_16BIT) ? 2 : 1;
	clipboardLength = len;
	clipboardIs16Bit = (s->flags & SAMPLE_16BIT) != 0;
	
	sampleClipboard = (int8_t *)malloc((size_t)(len * bytesPerSample));
	if (sampleClipboard != NULL)
	{
		memcpy(sampleClipboard, s->dataPtr + (start * bytesPerSample), (size_t)(len * bytesPerSample));
	}
	
	/* Store sample info if copying whole sample (for paste-overwrite) */
	if (start == 0 && end == s->length)
	{
		clipboardSampleInfo = *s;
		clipboardDidCopyWholeSample = true;
	}
	else
	{
		clipboardDidCopyWholeSample = false;
	}
}

/* Helper to paste copied data with bit-depth conversion */
static void pasteCopiedData(int8_t *dataPtr, int32_t offset, int32_t length, bool destIs16Bit)
{
	if (destIs16Bit)
	{
		if (clipboardIs16Bit)
		{
			/* Both 16-bit: direct copy */
			memcpy(&dataPtr[offset << 1], sampleClipboard, (size_t)(length * sizeof(int16_t)));
		}
		else
		{
			/* Convert 8-bit to 16-bit */
			int16_t *ptr16 = (int16_t *)dataPtr + offset;
			for (int32_t i = 0; i < length; i++)
				ptr16[i] = sampleClipboard[i] << 8;
		}
	}
	else
	{
		if (!clipboardIs16Bit)
		{
			/* Both 8-bit: direct copy */
			memcpy(&dataPtr[offset], sampleClipboard, (size_t)(length * sizeof(int8_t)));
		}
		else
		{
			/* Convert 16-bit to 8-bit */
			int8_t *ptr8 = &dataPtr[offset];
			int16_t *ptr16 = (int16_t *)sampleClipboard;
			for (int32_t i = 0; i < length; i++)
				ptr8[i] = ptr16[i] >> 8;
		}
	}
}

/* Helper to overwrite sample with clipboard content */
static void pasteOverwrite(ft2_sample_editor_t *editor, ft2_sample_t *s, ft2_instance_t *inst)
{
	int32_t bytesPerSample = clipboardIs16Bit ? 2 : 1;
	
	/* Free old sample data */
	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->dataPtr = NULL;
		s->origDataPtr = NULL;
	}
	
	/* Allocate new buffer with extra space for interpolation */
	size_t allocSize = (size_t)(clipboardLength * bytesPerSample) + 256;
	s->origDataPtr = (int8_t *)malloc(allocSize);
	if (s->origDataPtr == NULL)
		return;
	
	memset(s->origDataPtr, 0, allocSize);
	s->dataPtr = s->origDataPtr + 128;
	
	memcpy(s->dataPtr, sampleClipboard, (size_t)(clipboardLength * bytesPerSample));
	
	/* Restore sample info if we copied a whole sample */
	if (clipboardDidCopyWholeSample)
	{
		memcpy(s->name, clipboardSampleInfo.name, 22);
		s->length = clipboardSampleInfo.length;
		s->loopStart = clipboardSampleInfo.loopStart;
		s->loopLength = clipboardSampleInfo.loopLength;
		s->volume = clipboardSampleInfo.volume;
		s->panning = clipboardSampleInfo.panning;
		s->finetune = clipboardSampleInfo.finetune;
		s->relativeNote = clipboardSampleInfo.relativeNote;
		s->flags = clipboardSampleInfo.flags;
	}
	else
	{
		s->name[0] = '\0';
		s->length = clipboardLength;
		s->loopStart = 0;
		s->loopLength = 0;
		s->volume = 64;
		s->panning = 128;
		s->finetune = 0;
		s->relativeNote = 0;
		s->flags = clipboardIs16Bit ? SAMPLE_16BIT : 0;
	}
	
	/* Set range to pasted area (the whole sample in overwrite case) */
	editor->rangeStart = 0;
	editor->rangeEnd = s->length;
	editor->hasRange = true;
	ft2_sample_ed_show_all(inst);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_paste(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || sampleClipboard == NULL || clipboardLength <= 0)
		return;
	
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	
	/* If no sample data or no selection, do overwrite paste */
	if (s == NULL || s->dataPtr == NULL || s->length <= 0 || editor->rangeEnd == 0)
	{
		if (s != NULL)
			pasteOverwrite(editor, s, inst);
		return;
	}
	
	/* Insert paste: paste clipboard data at selection, replacing the selected range */
	int32_t rx1 = editor->rangeStart;
	int32_t rx2 = editor->rangeEnd;
	
	if (rx1 > rx2)
	{
		int32_t tmp = rx1;
		rx1 = rx2;
		rx2 = tmp;
	}
	
	if (rx1 < 0) rx1 = 0;
	if (rx2 > s->length) rx2 = s->length;
	
	bool destIs16Bit = (s->flags & SAMPLE_16BIT) != 0;
	
	/* Calculate new length */
	int32_t newLength = s->length + clipboardLength - (rx2 - rx1);
	if (newLength <= 0)
		return;
	
	/* Check max sample length */
	#define MAX_SAMPLE_LEN 0x3FFFFFFF
	if (newLength > MAX_SAMPLE_LEN)
		return;
	
	/* Allocate new buffer */
	int32_t bytesPerSample = destIs16Bit ? 2 : 1;
	size_t allocSize = (size_t)(newLength * bytesPerSample) + 256;
	
	int8_t *newOrigPtr = (int8_t *)malloc(allocSize);
	if (newOrigPtr == NULL)
		return;
	
	memset(newOrigPtr, 0, allocSize);
	int8_t *newDataPtr = newOrigPtr + 128;
	
	ft2_unfix_sample(s);
	
	/* Copy left part of original sample (before selection) */
	if (rx1 > 0)
		memcpy(newDataPtr, s->dataPtr, (size_t)(rx1 * bytesPerSample));
	
	/* Paste clipboard data at rx1 position */
	pasteCopiedData(newDataPtr, rx1, clipboardLength, destIs16Bit);
	
	/* Copy right part of original sample (after selection) */
	if (rx2 < s->length)
	{
		memmove(&newDataPtr[(rx1 + clipboardLength) * bytesPerSample],
		        &s->dataPtr[rx2 * bytesPerSample],
		        (size_t)((s->length - rx2) * bytesPerSample));
	}
	
	/* Free old sample data and assign new */
	free(s->origDataPtr);
	s->origDataPtr = newOrigPtr;
	s->dataPtr = newDataPtr;
	
	/* Adjust loop points if necessary */
	if (rx2 - rx1 != clipboardLength)
	{
		int32_t loopAdjust = clipboardLength - (rx2 - rx1);
		
		if (s->loopStart > rx2)
		{
			s->loopStart += loopAdjust;
		}
		
		if (s->loopStart + s->loopLength > rx2)
		{
			s->loopLength += loopAdjust;
		}
		
		if (s->loopStart > newLength)
		{
			s->loopStart = 0;
			s->loopLength = 0;
		}
		
		if (s->loopStart + s->loopLength > newLength)
		{
			s->loopLength = newLength - s->loopStart;
		}
		
		if (s->loopLength < 0)
		{
			s->loopStart = 0;
			s->loopLength = 0;
			s->flags &= ~(LOOP_FWD | LOOP_BIDI);
		}
	}
	
	s->length = newLength;
	
	ft2_fix_sample(s);
	
	/* Set new range to pasted area (so pressing paste again replaces it, not whole sample) */
	editor->rangeStart = rx1;
	editor->rangeEnd = rx1 + clipboardLength;
	editor->hasRange = true;
	
	ft2_sample_ed_show_all(inst);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_delete(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL)
		return;
	
	/* Need a valid range (not just a point) to delete */
	if (editor->rangeEnd == 0 || editor->rangeStart == editor->rangeEnd)
		return;
	
	int32_t start = editor->rangeStart;
	int32_t end = editor->rangeEnd;
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}
	
	if (start < 0) start = 0;
	if (end > s->length) end = s->length;
	
	int32_t delLen = end - start;
	if (delLen <= 0 || delLen >= s->length)
		return;
	
	int32_t bytesPerSample = (s->flags & SAMPLE_16BIT) ? 2 : 1;
	int32_t newLen = s->length - delLen;
	
	ft2_unfix_sample(s);
	
	/* Move data after selection to fill the gap */
	memmove(s->dataPtr + (start * bytesPerSample),
	        s->dataPtr + (end * bytesPerSample),
	        (size_t)((s->length - end) * bytesPerSample));
	
	s->length = newLen;
	
	/* Adjust loop */
	if (s->loopStart >= end)
		s->loopStart -= delLen;
	else if (s->loopStart > start)
		s->loopStart = start;
	
	if (s->loopStart < 0)
		s->loopStart = 0;
	if (s->loopStart + s->loopLength > newLen)
		s->loopLength = newLen - s->loopStart;
	if (s->loopLength < 0)
	{
		s->loopLength = 0;
		s->flags &= ~(LOOP_FWD | LOOP_BIDI);
	}
	
	ft2_fix_sample(s);
	
	editor->rangeStart = 0;
	editor->rangeEnd = 0;
	editor->hasRange = false;
	ft2_sample_ed_show_all(inst);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_reverse(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL)
		return;
	
	int32_t start = editor->hasRange ? editor->rangeStart : 0;
	int32_t end = editor->hasRange ? editor->rangeEnd : s->length;
	
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}
	
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		for (int32_t i = start, j = end - 1; i < j; i++, j--)
		{
			int16_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		for (int32_t i = start, j = end - 1; i < j; i++, j--)
		{
			int8_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
		}
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_normalize(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;
	
	int32_t start = editor->hasRange ? editor->rangeStart : 0;
	int32_t end = editor->hasRange ? editor->rangeEnd : s->length;
	
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}
	
	/* Find peak */
	int32_t peak = 0;
	
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		for (int32_t i = start; i < end; i++)
		{
			int32_t absVal = abs((int)ptr[i]);
			if (absVal > peak)
				peak = absVal;
		}
		
		if (peak > 0 && peak < 32767)
		{
			double factor = 32767.0 / (double)peak;
			for (int32_t i = start; i < end; i++)
			{
				int32_t newVal = (int32_t)(ptr[i] * factor);
				if (newVal > 32767) newVal = 32767;
				if (newVal < -32768) newVal = -32768;
				ptr[i] = (int16_t)newVal;
			}
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		for (int32_t i = start; i < end; i++)
		{
			int32_t absVal = abs((int)ptr[i]);
			if (absVal > peak)
				peak = absVal;
		}
		
		if (peak > 0 && peak < 127)
		{
			double factor = 127.0 / (double)peak;
			for (int32_t i = start; i < end; i++)
			{
				int32_t newVal = (int32_t)(ptr[i] * factor);
				if (newVal > 127) newVal = 127;
				if (newVal < -128) newVal = -128;
				ptr[i] = (int8_t)newVal;
			}
		}
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_fade_in(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || !editor->hasRange)
		return;
	
	int32_t start = editor->rangeStart;
	int32_t end = editor->rangeEnd;
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}
	
	int32_t len = end - start;
	if (len <= 0)
		return;
	
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		for (int32_t i = 0; i < len; i++)
		{
			double factor = (double)i / (double)len;
			ptr[start + i] = (int16_t)(ptr[start + i] * factor);
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		for (int32_t i = 0; i < len; i++)
		{
			double factor = (double)i / (double)len;
			ptr[start + i] = (int8_t)(ptr[start + i] * factor);
		}
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_fade_out(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || !editor->hasRange)
		return;
	
	int32_t start = editor->rangeStart;
	int32_t end = editor->rangeEnd;
	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}
	
	int32_t len = end - start;
	if (len <= 0)
		return;
	
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		for (int32_t i = 0; i < len; i++)
		{
			double factor = 1.0 - ((double)i / (double)len);
			ptr[start + i] = (int16_t)(ptr[start + i] * factor);
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		for (int32_t i = 0; i < len; i++)
		{
			double factor = 1.0 - ((double)i / (double)len);
			ptr[start + i] = (int8_t)(ptr[start + i] * factor);
		}
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

/* Helper functions for sample value access */
static double getSampleValuePlugin(int8_t *dataPtr, int32_t position, bool is16Bit)
{
	if (is16Bit)
		return (double)((int16_t *)dataPtr)[position];
	else
		return (double)dataPtr[position];
}

static void putSampleValuePlugin(int8_t *dataPtr, int32_t position, double sample, bool is16Bit)
{
	if (is16Bit)
	{
		int32_t val = (int32_t)sample;
		if (val < -32768) val = -32768;
		else if (val > 32767) val = 32767;
		((int16_t *)dataPtr)[position] = (int16_t)val;
	}
	else
	{
		int32_t val = (int32_t)sample;
		if (val < -128) val = -128;
		else if (val > 127) val = 127;
		dataPtr[position] = (int8_t)val;
	}
}

void ft2_sample_ed_crossfade_loop(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (editor == NULL || inst == NULL)
		return;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	uint8_t loopType = GET_LOOPTYPE(s->flags);
	if (loopType == LOOP_OFF)
		return; /* X-Fade only works on looped samples */

	/* Need a valid range selected */
	if (!editor->hasRange || editor->rangeEnd <= editor->rangeStart)
		return;

	int32_t x1 = editor->rangeStart;
	int32_t x2 = editor->rangeEnd;
	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;

	/* For forward loops, apply crossfade at loop point */
	if (loopType == LOOP_FWD)
	{
		if (x1 > s->loopStart)
		{
			x1 -= s->loopLength;
			x2 -= s->loopLength;
		}

		if (x1 < 0 || x2 <= x1 || x2 >= s->length)
			return;

		int32_t length = x2 - x1;
		int32_t x = (length + 1) >> 1;
		int32_t y1 = s->loopStart - x;
		int32_t y2 = s->loopStart + s->loopLength - x;

		if (y1 < 0 || y2 + length >= s->length)
			return;

		int32_t d1 = length;
		int32_t d2 = s->loopStart - y1;
		int32_t d3 = length - d2;

		if (y1 + length <= s->loopStart || d1 == 0 || d3 == 0 || d1 > s->loopLength)
			return;

		double dR = (s->loopStart - x) / (double)length;
		double dD1 = d1;
		double dD1Mul = 1.0 / d1;
		double dD2Mul = 1.0 / d2;
		double dD3Mul = 1.0 / d3;

		ft2_unfix_sample(s);

		for (int32_t i = 0; i < length; i++)
		{
			int32_t aIdx = y1 + i;
			int32_t bIdx = y2 + i;
			double dI = i;

			double dA = getSampleValuePlugin(s->dataPtr, aIdx, is16Bit);
			double dB = getSampleValuePlugin(s->dataPtr, bIdx, is16Bit);
			double dS2 = dI * dD1Mul;
			double dS1 = 1.0 - dS2;

			double dC, dD;
			if (y1 + i < s->loopStart)
			{
				double dS3 = 1.0 - (1.0 - dR) * dI * dD2Mul;
				double dS4 = dR * dI * dD2Mul;
				
				dC = (dA * dS3 + dB * dS4) / (dS3 + dS4);
				dD = (dA * dS2 + dB * dS1) / (dS1 + dS2);
			}
			else
			{
				double dS3 = 1.0 - (1.0 - dR) * (dD1 - dI) * dD3Mul;
				double dS4 = dR * (dD1 - dI) * dD3Mul;

				dC = (dA * dS2 + dB * dS1) / (dS1 + dS2);
				dD = (dA * dS4 + dB * dS3) / (dS3 + dS4);
			}

			putSampleValuePlugin(s->dataPtr, aIdx, dC, is16Bit);
			putSampleValuePlugin(s->dataPtr, bIdx, dD, is16Bit);
		}

		ft2_fix_sample(s);
		inst->replayer.song.isModified = true;
	}

	inst->uiState.updateSampleEditor = true;
}

/* ============ VISIBILITY FUNCTIONS ============ */

void showSampleEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Hide other bottom screens */
	if (inst->uiState.instEditorShown)
		hideInstEditor(inst);
	hidePatternEditor(inst);  /* Hides scrollbar and buttons too */
	
	/* Hide I.E.Ext since we're switching to sample editor */
	if (inst->uiState.instEditorExtShown)
		hideInstEditorExt(inst);
	
	inst->uiState.sampleEditorShown = true;
	inst->uiState.updateSampleEditor = true;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Show all sample editor pushbuttons - exact match of standalone showSampleEditor() */
	widgets->pushButtonVisible[PB_SAMP_SCROLL_LEFT] = true;
	widgets->pushButtonVisible[PB_SAMP_SCROLL_RIGHT] = true;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = true;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = true;
	widgets->pushButtonVisible[PB_SAMP_STOP] = true;
	widgets->pushButtonVisible[PB_SAMP_PWAVE] = true;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = true;
	widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = true;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = true;
	widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = true;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = true;
	widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = true;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = true;
	/* widgets->pushButtonVisible[PB_SAMP_SAVE_RNG] = true; */ /* TODO: Implement save range */
	widgets->pushButtonVisible[PB_SAMP_CUT] = true;
	widgets->pushButtonVisible[PB_SAMP_COPY] = true;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = true;
	widgets->pushButtonVisible[PB_SAMP_CROP] = true;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = true;
	widgets->pushButtonVisible[PB_SAMP_EFFECTS] = true;
	widgets->pushButtonVisible[PB_SAMP_EXIT] = true;
	widgets->pushButtonVisible[PB_SAMP_CLEAR] = true;
	widgets->pushButtonVisible[PB_SAMP_MIN] = true;
	widgets->pushButtonVisible[PB_SAMP_REPEAT_UP] = true;
	widgets->pushButtonVisible[PB_SAMP_REPEAT_DOWN] = true;
	widgets->pushButtonVisible[PB_SAMP_REPLEN_UP] = true;
	widgets->pushButtonVisible[PB_SAMP_REPLEN_DOWN] = true;

	/* Show radio button groups */
	widgets->radioButtonVisible[RB_SAMPLE_NO_LOOP] = true;
	widgets->radioButtonVisible[RB_SAMPLE_FWD_LOOP] = true;
	widgets->radioButtonVisible[RB_SAMPLE_BIDI_LOOP] = true;
	widgets->radioButtonVisible[RB_SAMPLE_8BIT] = true;
	widgets->radioButtonVisible[RB_SAMPLE_16BIT] = true;

	/* Show scrollbar */
	widgets->scrollBarState[SB_SAMP_SCROLL].visible = true;
}

void hideSampleEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.sampleEditorShown = false;
	inst->uiState.leftLoopPinMoving = false;
	inst->uiState.rightLoopPinMoving = false;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Hide all sample editor pushbuttons */
	widgets->pushButtonVisible[PB_SAMP_SCROLL_LEFT] = false;
	widgets->pushButtonVisible[PB_SAMP_SCROLL_RIGHT] = false;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = false;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_STOP] = false;
	widgets->pushButtonVisible[PB_SAMP_PWAVE] = false;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_SAVE_RNG] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = false;
	widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = false;
	widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = false;
	widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;
	widgets->pushButtonVisible[PB_SAMP_EXIT] = false;
	widgets->pushButtonVisible[PB_SAMP_CLEAR] = false;
	widgets->pushButtonVisible[PB_SAMP_MIN] = false;
	widgets->pushButtonVisible[PB_SAMP_REPEAT_UP] = false;
	widgets->pushButtonVisible[PB_SAMP_REPEAT_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_REPLEN_UP] = false;
	widgets->pushButtonVisible[PB_SAMP_REPLEN_DOWN] = false;

	/* Hide radio button groups */
	widgets->radioButtonVisible[RB_SAMPLE_NO_LOOP] = false;
	widgets->radioButtonVisible[RB_SAMPLE_FWD_LOOP] = false;
	widgets->radioButtonVisible[RB_SAMPLE_BIDI_LOOP] = false;
	widgets->radioButtonVisible[RB_SAMPLE_8BIT] = false;
	widgets->radioButtonVisible[RB_SAMPLE_16BIT] = false;

	/* Hide scrollbar */
	widgets->scrollBarState[SB_SAMP_SCROLL].visible = false;

	/* Also hide extended sample editor buttons */
	hideSampleEditorExtButtons(inst);
	inst->uiState.sampleEditorExtShown = false;

	/* Hide sample effects screen if open */
	if (inst->uiState.sampleEditorEffectsShown)
		hideSampleEffectsScreen(inst);
}

void toggleSampleEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.sampleEditorShown)
	{
		hideSampleEditor(inst);
		/* Return to pattern editor */
		inst->uiState.patternEditorShown = true;
	}
	else
	{
		showSampleEditor(inst);
	}
}

void exitSampleEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	hideSampleEditor(inst);
	inst->uiState.patternEditorShown = true;
}

/* ============ EXTENDED SAMPLE EDITOR ============ */

void drawSampleEditorExt(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	/* Draw extended sample editor framework - matches standalone ft2_sample_ed.c lines 3465-3477 */
	drawFramework(video, 0,   92, 158, 44, FRAMEWORK_TYPE1);
	drawFramework(video, 0,  136, 158, 37, FRAMEWORK_TYPE1);
	drawFramework(video, 158, 92, 133, 81, FRAMEWORK_TYPE1);

	/* Labels */
	textOutShadow(video, bmp, 4, 96, PAL_FORGRND, PAL_DSKTOP2, "Rng.:");
	charOutShadow(video, bmp, 91, 95, PAL_FORGRND, PAL_DSKTOP2, '-');
	textOutShadow(video, bmp, 4, 110, PAL_FORGRND, PAL_DSKTOP2, "Range size");
	textOutShadow(video, bmp, 4, 124, PAL_FORGRND, PAL_DSKTOP2, "Copy buf. size");

	textOutShadow(video, bmp, 162, 96, PAL_FORGRND, PAL_DSKTOP2, "Src.instr.");
	textOutShadow(video, bmp, 245, 96, PAL_FORGRND, PAL_DSKTOP2, "smp.");
	textOutShadow(video, bmp, 162, 109, PAL_FORGRND, PAL_DSKTOP2, "Dest.instr.");
	textOutShadow(video, bmp, 245, 109, PAL_FORGRND, PAL_DSKTOP2, "smp.");

	/* Show extended sample editor buttons */
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_widgets_t *widgets = &ui->widgets;
		widgets->pushButtonVisible[PB_SAMP_EXT_CLEAR_COPYBUF] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_CONV] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_ECHO] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_BACKWARDS] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_CONV_W] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_MORPH] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_COPY_INS] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_COPY_SMP] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_XCHG_INS] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_XCHG_SMP] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_RESAMPLE] = true;
		widgets->pushButtonVisible[PB_SAMP_EXT_MIX_SAMPLE] = true;
	}

	/* Draw range values - positions match standalone (35, 99, 99, 99) */
	ft2_sample_editor_t *editor = (inst != NULL && inst->ui != NULL) ? FT2_SAMPLE_ED(inst) : NULL;
	if (editor != NULL)
	{
		hexOutBg(video, bmp, 35, 96, PAL_FORGRND, PAL_DESKTOP, editor->rangeStart, 8);
		hexOutBg(video, bmp, 99, 96, PAL_FORGRND, PAL_DESKTOP, editor->rangeEnd, 8);

		int32_t rangeSize = 0;
		if (editor->rangeEnd > editor->rangeStart)
			rangeSize = editor->rangeEnd - editor->rangeStart;
		hexOutBg(video, bmp, 99, 110, PAL_FORGRND, PAL_DESKTOP, rangeSize, 8);
	}

	/* Draw copy buffer size */
	extern int32_t clipboardLength;
	hexOutBg(video, bmp, 99, 124, PAL_FORGRND, PAL_DESKTOP, clipboardLength, 8);

	/* Draw source/dest instrument and sample numbers */
	hexOutBg(video, bmp, 225, 96, PAL_FORGRND, PAL_DESKTOP, inst->editor.srcInstr, 2);
	hexOutBg(video, bmp, 274, 96, PAL_FORGRND, PAL_DESKTOP, inst->editor.srcSmp, 2);
	hexOutBg(video, bmp, 225, 109, PAL_FORGRND, PAL_DESKTOP, inst->editor.curInstr, 2);
	hexOutBg(video, bmp, 274, 109, PAL_FORGRND, PAL_DESKTOP, inst->editor.curSmp, 2);
}

void hideSampleEditorExtButtons(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;
	ft2_widgets_t *widgets = &ui->widgets;
	
	widgets->pushButtonVisible[PB_SAMP_EXT_CLEAR_COPYBUF] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_CONV] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_ECHO] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_BACKWARDS] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_CONV_W] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_MORPH] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_COPY_INS] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_COPY_SMP] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_XCHG_INS] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_XCHG_SMP] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_RESAMPLE] = false;
	widgets->pushButtonVisible[PB_SAMP_EXT_MIX_SAMPLE] = false;
}

void showSampleEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Exit extended pattern editor if active */
	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);

	/* Hide other top-left panel overlays (I.E.Ext, Transpose, Adv.Edit, Trim) */
	hideAllTopLeftPanelOverlays(inst);

	/* Show sample editor if not already shown */
	if (!inst->uiState.sampleEditorShown)
		showSampleEditor(inst);

	/* Hide inst editor if shown (S.E.Ext requires sample editor to be active) */
	if (inst->uiState.instEditorShown)
		hideInstEditor(inst);

	inst->uiState.sampleEditorExtShown = true;
	inst->uiState.scopesShown = false;
}

void hideSampleEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.sampleEditorExtShown = false;

	/* Hide extended sample editor buttons */
	hideSampleEditorExtButtons(inst);

	/* Show scopes again and trigger framework redraw */
	inst->uiState.scopesShown = true;

	/* Trigger scope framework redraw to clear the extended panel area */
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
		ui->scopes.needsFrameworkRedraw = true;
}

void toggleSampleEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.sampleEditorExtShown)
		hideSampleEditorExt(inst);
	else
		showSampleEditorExt(inst);
}

/* ============ SAMPLE MEMORY ============ */

bool allocateSmpData(ft2_instance_t *inst, int instrNum, int smpNum, int32_t length, bool sample16Bit)
{
	if (inst == NULL || instrNum < 1 || instrNum > 128 || smpNum < 0 || smpNum >= 16)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return false;

	ft2_sample_t *s = &instr->smp[smpNum];

	/* Free existing data */
	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->origDataPtr = NULL;
		s->dataPtr = NULL;
	}

	if (length <= 0)
	{
		s->length = 0;
		return true;
	}

	/* Calculate allocation size with padding for interpolation on both sides */
	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t dataLen = length * bytesPerSample;
	int32_t allocLen = leftPadding + dataLen + rightPadding;

	s->origDataPtr = (int8_t *)calloc(allocLen, 1);
	if (s->origDataPtr == NULL)
		return false;

	/* dataPtr points past the left padding area */
	s->dataPtr = s->origDataPtr + leftPadding;

	return true;
}

void freeSmpData(ft2_instance_t *inst, int instrNum, int smpNum)
{
	if (inst == NULL || instrNum < 1 || instrNum > 128 || smpNum < 0 || smpNum >= 16)
		return;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[smpNum];

	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->origDataPtr = NULL;
	}

	s->dataPtr = NULL;
	s->length = 0;
	s->isFixed = false;
	s->loopStart = 0;
	s->loopLength = 0;
}

/* Callback for clear sample confirmation dialog */
static void onClearSampleResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || result != DIALOG_RESULT_YES)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];

	/* Stop voices playing this sample before clearing */
	if (instr != NULL)
	{
		ft2_sample_t *s = &instr->smp[curSmp];
		ft2_stop_sample_voices(inst, s);
	}

	freeSmpData(inst, curInstr, curSmp);

	if (instr != NULL)
	{
		ft2_sample_t *s = &instr->smp[curSmp];
		memset(s->name, 0, sizeof(s->name));
	}

	/* Reset sample editor view */
	ft2_sample_ed_show_all(inst);

	inst->uiState.updateSampleEditor = true;
	ft2_song_mark_modified(inst);
}

void clearSample(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Check if there's actually a sample to clear */
	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length <= 0)
		return;

	/* Show confirmation dialog */
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Clear sample?",
			inst, onClearSampleResult, NULL);
	}
}

/* Callback for clear instrument confirmation dialog */
static void onClearInstrResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                               const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || result != DIALOG_RESULT_YES)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	if (curInstr == 0)
		return;

	/* Stop voices playing this instrument before clearing */
	ft2_stop_all_voices(inst);

	ft2_instance_free_instr(inst, curInstr);
	memset(inst->replayer.song.instrName[curInstr], 0, 23);

	inst->editor.currVolEnvPoint = 0;
	inst->editor.currPanEnvPoint = 0;
	inst->uiState.updateInstrSwitcher = true;
	inst->uiState.updateSampleEditor = true;

	ft2_song_mark_modified(inst);
}

void clearInstr(ft2_instance_t *inst)
{
	if (inst == NULL || inst->editor.curInstr == 0)
		return;

	if (inst->replayer.instr[inst->editor.curInstr] == NULL)
		return;

	/* Show confirmation dialog */
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Clear instrument?",
			inst, onClearInstrResult, NULL);
	}
}

void clearCopyBuffer(ft2_instance_t *inst)
{
	(void)inst;
	
	if (sampleClipboard != NULL)
	{
		free(sampleClipboard);
		sampleClipboard = NULL;
	}
	clipboardLength = 0;
	clipboardIs16Bit = false;
}

/* ============ COPY/EXCHANGE OPERATIONS ============ */

/* Clone sample data from src to dst */
bool cloneSample(ft2_sample_t *src, ft2_sample_t *dst)
{
	if (dst == NULL)
		return false;

	/* Free existing data in dst - must free origDataPtr, not dataPtr */
	if (dst->origDataPtr != NULL)
	{
		free(dst->origDataPtr);
		dst->origDataPtr = NULL;
		dst->dataPtr = NULL;
	}

	if (src == NULL)
	{
		memset(dst, 0, sizeof(ft2_sample_t));
		return true;
	}

	/* Copy sample struct */
	memcpy(dst, src, sizeof(ft2_sample_t));

	/* Zero out pointer stuff */
	dst->origDataPtr = NULL;
	dst->dataPtr = NULL;
	dst->isFixed = false;
	dst->fixedPos = 0;

	/* If source has sample data, allocate and copy with proper padding */
	if (src->length > 0 && src->dataPtr != NULL)
	{
		bool is16Bit = (src->flags & SAMPLE_16BIT) != 0;
		int32_t bytesPerSample = is16Bit ? 2 : 1;
		int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
		int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
		int32_t dataLen = src->length * bytesPerSample;
		int32_t allocLen = leftPadding + dataLen + rightPadding;

		dst->origDataPtr = (int8_t *)calloc(allocLen, 1);
		if (dst->origDataPtr == NULL)
		{
			dst->length = 0;
			return false;
		}

		dst->dataPtr = dst->origDataPtr + leftPadding;
		memcpy(dst->dataPtr, src->dataPtr, dataLen);
		ft2_fix_sample(dst);
	}

	return true;
}

void copySmp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t srcInstr = inst->editor.srcInstr;
	uint8_t curSmp = inst->editor.curSmp;
	uint8_t srcSmp = inst->editor.srcSmp;

	/* Can't copy to instrument 0 or copy same to same */
	if (curInstr == 0 || (curInstr == srcInstr && curSmp == srcSmp))
		return;

	/* Get source sample */
	ft2_sample_t *src = NULL;
	if (inst->replayer.instr[srcInstr] != NULL)
		src = &inst->replayer.instr[srcInstr]->smp[srcSmp];

	/* Allocate dest instrument if needed */
	if (inst->replayer.instr[curInstr] == NULL)
	{
		if (!ft2_instance_alloc_instr(inst, curInstr))
			return;
	}

	ft2_sample_t *dst = &inst->replayer.instr[curInstr]->smp[curSmp];

	if (!cloneSample(src, dst))
		return;

	inst->uiState.updateSampleEditor = true;
	inst->uiState.updateInstrSwitcher = true;
}

void xchgSmp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;
	uint8_t srcSmp = inst->editor.srcSmp;

	/* Can't exchange in instrument 0 or exchange same with same */
	if (curInstr == 0 || curSmp == srcSmp)
		return;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *src = &instr->smp[srcSmp];
	ft2_sample_t *dst = &instr->smp[curSmp];

	/* Swap sample structs */
	ft2_sample_t tmp = *dst;
	*dst = *src;
	*src = tmp;

	inst->uiState.updateSampleEditor = true;
	inst->uiState.updateInstrSwitcher = true;
}

void copyInstr(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t srcInstr = inst->editor.srcInstr;

	/* Can't copy to instrument 0 or copy same to same */
	if (curInstr == 0 || curInstr == srcInstr)
		return;

	ft2_instr_t *srcIns = inst->replayer.instr[srcInstr];

	/* Allocate dest instrument if needed */
	if (inst->replayer.instr[curInstr] == NULL)
	{
		if (!ft2_instance_alloc_instr(inst, curInstr))
			return;
	}

	ft2_instr_t *dstIns = inst->replayer.instr[curInstr];

	/* Free all sample data in dest first */
	for (int i = 0; i < 16; i++)
	{
		if (dstIns->smp[i].dataPtr != NULL)
		{
			free(dstIns->smp[i].dataPtr);
			dstIns->smp[i].dataPtr = NULL;
		}
	}

	if (srcIns == NULL)
	{
		/* Clear the dest instrument */
		memset(dstIns, 0, sizeof(ft2_instr_t));
	}
	else
	{
		/* Copy instrument header (not samples) */
		memcpy(dstIns->note2SampleLUT, srcIns->note2SampleLUT, sizeof(dstIns->note2SampleLUT));
		memcpy(dstIns->volEnvPoints, srcIns->volEnvPoints, sizeof(dstIns->volEnvPoints));
		memcpy(dstIns->panEnvPoints, srcIns->panEnvPoints, sizeof(dstIns->panEnvPoints));
		dstIns->volEnvLength = srcIns->volEnvLength;
		dstIns->panEnvLength = srcIns->panEnvLength;
		dstIns->volEnvSustain = srcIns->volEnvSustain;
		dstIns->volEnvLoopStart = srcIns->volEnvLoopStart;
		dstIns->volEnvLoopEnd = srcIns->volEnvLoopEnd;
		dstIns->panEnvSustain = srcIns->panEnvSustain;
		dstIns->panEnvLoopStart = srcIns->panEnvLoopStart;
		dstIns->panEnvLoopEnd = srcIns->panEnvLoopEnd;
		dstIns->volEnvFlags = srcIns->volEnvFlags;
		dstIns->panEnvFlags = srcIns->panEnvFlags;
		dstIns->autoVibType = srcIns->autoVibType;
		dstIns->autoVibSweep = srcIns->autoVibSweep;
		dstIns->autoVibDepth = srcIns->autoVibDepth;
		dstIns->autoVibRate = srcIns->autoVibRate;
		dstIns->fadeout = srcIns->fadeout;
		dstIns->midiOn = srcIns->midiOn;
		dstIns->midiChannel = srcIns->midiChannel;
		dstIns->midiProgram = srcIns->midiProgram;
		dstIns->midiBend = srcIns->midiBend;
		dstIns->mute = srcIns->mute;
		dstIns->numSamples = srcIns->numSamples;

		/* Clone all samples */
		for (int i = 0; i < 16; i++)
		{
			memset(&dstIns->smp[i], 0, sizeof(ft2_sample_t));
			if (!cloneSample(&srcIns->smp[i], &dstIns->smp[i]))
				break;
		}
	}

	inst->uiState.updateSampleEditor = true;
	inst->uiState.updateInstrSwitcher = true;
}

void xchgInstr(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t srcInstr = inst->editor.srcInstr;

	/* Can't exchange instrument 0 or exchange same with same */
	if (curInstr == 0 || curInstr == srcInstr)
		return;

	/* Swap instrument pointers */
	ft2_instr_t *tmp = inst->replayer.instr[curInstr];
	inst->replayer.instr[curInstr] = inst->replayer.instr[srcInstr];
	inst->replayer.instr[srcInstr] = tmp;

	/* Note: we do not swap instrument names (like FT2) */

	inst->uiState.updateSampleEditor = true;
	inst->uiState.updateInstrSwitcher = true;
}

/* ============ SAMPLE PROCESSING ============ */

void sampleBackwards(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length < 2)
		return;

	ft2_sample_editor_t *ed = (inst->ui != NULL) ? FT2_SAMPLE_ED(inst) : NULL;
	
	int32_t start = (ed != NULL && ed->hasRange) ? ed->rangeStart : 0;
	int32_t end = (ed != NULL && ed->hasRange) ? ed->rangeEnd : s->length;

	if (start > end)
	{
		int32_t tmp = start;
		start = end;
		end = tmp;
	}

	if (start >= end)
		return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		for (int32_t i = start, j = end - 1; i < j; i++, j--)
		{
			int16_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		for (int32_t i = start, j = end - 1; i < j; i++, j--)
		{
			int8_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
		}
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void sampleChangeSign(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length == 0)
		return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;

	if (is16Bit)
	{
		int16_t *ptr = (int16_t *)s->dataPtr;
		int32_t len = s->length;
		for (int32_t i = 0; i < len; i++)
			ptr[i] ^= 0x8000;
	}
	else
	{
		int8_t *ptr = s->dataPtr;
		int32_t len = s->length;
		for (int32_t i = 0; i < len; i++)
			ptr[i] ^= 0x80;
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void fixDC(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length == 0)
		return;

	ft2_sample_editor_t *ed = (inst->ui != NULL) ? FT2_SAMPLE_ED(inst) : NULL;
	bool sampleDataMarked = (ed != NULL && ed->hasRange && ed->rangeStart != ed->rangeEnd);

	int32_t start = sampleDataMarked ? ed->rangeStart : 0;
	int32_t length = sampleDataMarked ? (ed->rangeEnd - ed->rangeStart) : s->length;

	if (start > ed->rangeEnd && sampleDataMarked)
	{
		start = ed->rangeEnd;
		length = ed->rangeStart - ed->rangeEnd;
	}

	if (length <= 0 || length > s->length)
		return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;

	/* Calculate DC offset */
	int64_t sum = 0;

	if (is16Bit)
	{
		int16_t *ptr = (int16_t *)s->dataPtr + start;
		for (int32_t i = 0; i < length; i++)
			sum += ptr[i];
	}
	else
	{
		int8_t *ptr = s->dataPtr + start;
		for (int32_t i = 0; i < length; i++)
			sum += ptr[i];
	}

	int32_t offset = (int32_t)((sum + (length >> 1)) / length);

	if (offset == 0)
	{
		ft2_fix_sample(s);
		return;
	}

	/* Remove DC offset */
	if (is16Bit)
	{
		int16_t *ptr = (int16_t *)s->dataPtr + start;
		for (int32_t i = 0; i < length; i++)
		{
			int32_t val = ptr[i] - offset;
			if (val < -32768) val = -32768;
			if (val > 32767) val = 32767;
			ptr[i] = (int16_t)val;
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr + start;
		for (int32_t i = 0; i < length; i++)
		{
			int32_t val = ptr[i] - offset;
			if (val < -128) val = -128;
			if (val > 127) val = 127;
			ptr[i] = (int8_t)val;
		}
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void sampleByteSwap(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length == 0)
		return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;

	int32_t len = s->length;
	if (!is16Bit)
		len >>= 1;

	int8_t *ptr = s->dataPtr;
	for (int32_t i = 0; i < len; i++, ptr += 2)
	{
		int8_t tmp = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = tmp;
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

static bool reallocateSmpData(ft2_sample_t *s, int32_t newLength, bool is16Bit)
{
	if (s == NULL)
		return false;

	int32_t bytesPerSample = is16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * bytesPerSample;
	int32_t dataLen = newLength * bytesPerSample;
	size_t allocSize = (size_t)(leftPadding + dataLen + rightPadding);

	/* If no origDataPtr, need to allocate fresh */
	if (s->origDataPtr == NULL)
	{
		s->origDataPtr = (int8_t *)calloc(allocSize, 1);
		if (s->origDataPtr == NULL)
			return false;
		
		s->dataPtr = s->origDataPtr + leftPadding;
		return true;
	}

	/* Realloc on origDataPtr, not dataPtr! */
	int8_t *newPtr = (int8_t *)realloc(s->origDataPtr, allocSize);
	if (newPtr == NULL)
		return false;

	s->origDataPtr = newPtr;
	s->dataPtr = s->origDataPtr + leftPadding;
	return true;
}

void sampCrop(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	
	/* Check if we have a valid selection with rangeEnd > 0 */
	if (ed->rangeEnd == 0)
		return;
	
	/* Need a proper range for crop (not just a point) */
	if (ed->rangeStart == ed->rangeEnd)
		return;

	ft2_sample_t *s = getCurrentSampleWithInst(ed, inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t r1 = ed->rangeStart;
	int32_t r2 = ed->rangeEnd;

	if (r1 > r2)
	{
		int32_t tmp = r1;
		r1 = r2;
		r2 = tmp;
	}
	
	/* Clamp to valid sample range */
	if (r1 < 0) r1 = 0;
	if (r2 > s->length) r2 = s->length;

	/* Nothing to crop if the whole sample is selected */
	if (r1 == 0 && r2 == s->length)
		return;

	if (r1 >= s->length || r2 <= 0)
		return;

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;
	int32_t newLength = r2 - r1;

	if (newLength <= 0)
		return;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	/* Move the cropped portion to the beginning */
	if (r1 > 0)
	{
		int32_t bytesPerSample = is16Bit ? 2 : 1;
		memmove(s->dataPtr, s->dataPtr + (r1 * bytesPerSample), (size_t)(newLength * bytesPerSample));
	}

	/* Adjust loop points BEFORE changing length */
	int32_t oldLoopStart = s->loopStart;
	int32_t oldLoopEnd = s->loopStart + s->loopLength;

	if (s->loopLength > 0)
	{
		oldLoopStart -= r1;
		oldLoopEnd -= r1;

		if (oldLoopStart < 0)
			oldLoopStart = 0;
		if (oldLoopEnd > newLength)
			oldLoopEnd = newLength;

		s->loopStart = oldLoopStart;
		s->loopLength = oldLoopEnd - oldLoopStart;

		if (s->loopLength <= 0)
		{
			s->loopStart = 0;
			s->loopLength = 0;
			s->flags &= ~(LOOP_FWD | LOOP_BIDI);
		}
	}

	/* Update length before realloc */
	s->length = newLength;

	/* Realloc to new size (this now uses origDataPtr correctly) */
	if (!reallocateSmpData(s, newLength, is16Bit))
	{
		ft2_fix_sample(s);
		inst->uiState.updateSampleEditor = true;
		return;
	}

	ft2_fix_sample(s);

	/* Select the new (cropped) sample range */
	ed->rangeStart = 0;
	ed->rangeEnd = newLength;
	ed->hasRange = true;
	ed->viewSize = newLength;
	ed->scrPos = 0;

	inst->uiState.updateSampleEditor = true;
	ft2_song_mark_modified(inst);
}

/* Callback for minimize sample confirmation dialog */
static void onMinimizeSampleResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                   const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || result != DIALOG_RESULT_YES)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Double-check loop is still valid */
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	if (loopType == LOOP_OFF)
		return;

	if (s->loopStart + s->loopLength >= s->length)
		return;

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	s->length = s->loopStart + s->loopLength;

	reallocateSmpData(s, s->length, is16Bit);

	ft2_fix_sample(s);

	ft2_sample_ed_show_all(inst);

	inst->uiState.updateSampleEditor = true;
	ft2_song_mark_modified(inst);
}

void sampMinimize(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;

	/* Check for loop - matches standalone behavior */
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	if (loopType == LOOP_OFF)
	{
		if (ui != NULL)
			ft2_dialog_show_message(&ui->dialog, "System message", "Only a looped sample can be minimized!");
		return;
	}

	/* Check if already minimized - matches standalone behavior */
	if (s->loopStart + s->loopLength >= s->length)
	{
		if (ui != NULL)
			ft2_dialog_show_message(&ui->dialog, "System message", "This sample is already minimized.");
		return;
	}

	/* Show confirmation dialog */
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Minimize sample?",
			inst, onMinimizeSampleResult, NULL);
	}
}

/* ============ REPEAT/LENGTH CONTROLS ============ */

void sampRepeatUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];

	if (s->loopStart < s->length - s->loopLength)
	{
		/* Stop voices playing this sample before changing loop points */
		ft2_stop_sample_voices(inst, s);
		s->loopStart++;
		ft2_song_mark_modified(inst);
	}

	inst->uiState.updateSampleEditor = true;
}

void sampRepeatDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];

	if (s->loopStart > 0)
	{
		/* Stop voices playing this sample before changing loop points */
		ft2_stop_sample_voices(inst, s);
		s->loopStart--;
		ft2_song_mark_modified(inst);
	}

	inst->uiState.updateSampleEditor = true;
}

void sampReplenUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];

	if (s->loopStart + s->loopLength < s->length)
	{
		/* Stop voices playing this sample before changing loop points */
		ft2_stop_sample_voices(inst, s);
		s->loopLength++;
		ft2_song_mark_modified(inst);
	}

	inst->uiState.updateSampleEditor = true;
}

void sampReplenDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];

	if (s->loopLength > 0)
	{
		/* Stop voices playing this sample before changing loop points */
		ft2_stop_sample_voices(inst, s);
		s->loopLength--;
		ft2_song_mark_modified(inst);
	}

	inst->uiState.updateSampleEditor = true;
}

/* ============ SAMPLE VOLUME ============ */

void sampApplyVolume(ft2_instance_t *inst, double startVol, double endVol)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);

	uint8_t curInstr = inst->editor.curInstr;
	uint8_t curSmp = inst->editor.curSmp;

	if (curInstr == 0)
		return;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (instr == NULL)
		return;

	ft2_sample_t *s = &instr->smp[curSmp];
	if (s->dataPtr == NULL || s->length <= 0)
		return;

	/* Skip if no change */
	if (startVol == 100.0 && endVol == 100.0)
		return;

	/* Determine range to operate on */
	int32_t x1, x2;
	if (ed->hasRange && ed->rangeStart < ed->rangeEnd)
	{
		x1 = ed->rangeStart;
		x2 = ed->rangeEnd;
		if (x2 > s->length)
			x2 = s->length;
		if (x1 < 0)
			x1 = 0;
		if (x2 <= x1)
			return;
	}
	else
	{
		x1 = 0;
		x2 = s->length;
	}

	int32_t len = x2 - x1;
	if (len <= 0)
		return;

	bool mustInterpolate = (startVol != endVol);
	double dVolDelta = ((endVol - startVol) / 100.0) / len;
	double dVol = startVol / 100.0;

	ft2_unfix_sample(s);

	bool is16Bit = (s->flags & SAMPLE_16BIT) != 0;
	if (is16Bit)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				if (smp32 < -32768) smp32 = -32768;
				else if (smp32 > 32767) smp32 = 32767;
				ptr16[i] = (int16_t)smp32;
				dVol += dVolDelta;
			}
		}
		else
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				if (smp32 < -32768) smp32 = -32768;
				else if (smp32 > 32767) smp32 = 32767;
				ptr16[i] = (int16_t)smp32;
			}
		}
	}
	else
	{
		int8_t *ptr8 = s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				if (smp32 < -128) smp32 = -128;
				else if (smp32 > 127) smp32 = 127;
				ptr8[i] = (int8_t)smp32;
				dVol += dVolDelta;
			}
		}
		else
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				if (smp32 < -128) smp32 = -128;
				else if (smp32 > 127) smp32 = 127;
				ptr8[i] = (int8_t)smp32;
			}
		}
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
	ft2_song_mark_modified(inst);
}
