/*
** FT2 Plugin - Sample Editor
** Port of ft2_sample_ed.c with instance-aware state.
**
** Implements waveform display, range selection, loop pin editing, clipboard
** operations, and sample processing (normalize, fade, reverse, etc).
*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_common.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_sample_utils.h"
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
#include "ft2_plugin_slicer_panel.h"
#include "ft2_plugin_xm2_convert.h"
#include "ft2_instance.h"

/* Shared sample helpers */
#define sampleGetFormat ft2_sample_get_format
#define sampleHasData ft2_sample_has_data
#define sampleGetDataPtr ft2_sample_get_data_ptr
#define sampleGetFloat ft2_sample_get_float
#define sampleSetFloat ft2_sample_set_float
#define sampleIsStereo(s) ft2_sample_is_stereo((s), ft2_sample_get_format(s))
#define getMaxSamples(inst) ft2_get_max_samples_per_instr(inst)
#define PEAK_CACHE_BLOCK_SIZE 256

static ft2_sample_t *getCurrentSampleWithInst(ft2_sample_editor_t *editor, ft2_instance_t *inst);
static void getSampleDataPeakChannelCached(ft2_sample_editor_t *editor, ft2_sample_t *s,
                                           ft2_sample_format_t format, bool rightChannel,
                                           int32_t start, int32_t count, int32_t yCenter,
                                           int32_t halfHeight, int16_t *outMin, int16_t *outMax);
static void freePeakCache(ft2_sample_editor_t *editor);
static bool ensurePeakCache(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                             ft2_sample_format_t format, bool isStereo);

/* ------------------------------------------------------------------------- */
/*                         WAVEFORM DRAWING                                  */
/* ------------------------------------------------------------------------- */

/* Bresenham line drawing for waveform display with Y clipping */
static void sampleLineClipped(ft2_video_t *video, int32_t x1, int32_t x2, int32_t y1, int32_t y2,
                               int32_t yMin, int32_t yMax)
{
	if (!video || !video->frameBuffer)
		return;

	const int32_t dx = x2 - x1, dy = y2 - y1;
	const int32_t ax = ABS(dx) * 2, ay = ABS(dy) * 2;
	const int32_t sx = SGN(dx), sy = SGN(dy);
	int32_t x = x1, y = y1, d;
	const uint32_t pixVal = video->palette[PAL_PATTEXT];

	if (ax > ay)
	{
		d = ay - (ax / 2);
		while (true)
		{
			if (y >= yMin && y < yMax && x >= 0 && x < SAMPLE_AREA_WIDTH)
				video->frameBuffer[(y * SCREEN_W) + x] = pixVal;

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
			if (y >= yMin && y < yMax && x >= 0 && x < SAMPLE_AREA_WIDTH)
				video->frameBuffer[(y * SCREEN_W) + x] = pixVal;

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

/* Fast line drawing (endpoints are already clipped) */
static void sampleLineFast(ft2_video_t *video, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	if (!video || !video->frameBuffer)
		return;

	const int32_t dx = x2 - x1, dy = y2 - y1;
	const int32_t ax = ABS(dx) * 2, ay = ABS(dy) * 2;
	const int32_t sx = SGN(dx), sy = SGN(dy);
	int32_t x = x1, y = y1, d;
	const uint32_t pixVal = video->palette[PAL_PATTEXT];
	const int32_t pitch = sy * SCREEN_W;
	uint32_t *dst32 = &video->frameBuffer[(y * SCREEN_W) + x];

	if (ax > ay)
	{
		d = ay - (ax / 2);
		while (true)
		{
			*dst32 = pixVal;
			if (x == x2)
				break;

			if (d >= 0)
			{
				y += sy;
				d -= ax;
				dst32 += pitch;
			}

			x += sx;
			d += ay;
			dst32 += sx;
		}
	}
	else
	{
		d = ax - (ay / 2);
		while (true)
		{
			*dst32 = pixVal;
			if (y == y2)
				break;

			if (d >= 0)
			{
				x += sx;
				d -= ay;
				dst32 += sx;
			}

			y += sy;
			d += ax;
			dst32 += pitch;
		}
	}
}

/* Wrapper for mono/full-height display */
static void sampleLine(ft2_video_t *video, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	sampleLineClipped(video, x1, x2, y1, y2, SAMPLE_AREA_Y_START, SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT);
}

/* Scales sample value to display Y coordinate for a specific channel and Y range */
static int32_t getScaledSampleChannel(ft2_sample_t *s, int32_t pos, bool rightChannel,
                                       int32_t yCenter, int32_t halfHeight)
{
	const ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || pos < 0 || pos >= s->length)
		return yCenter;

	const bool fixedScan = (s->isFixed && s->length > s->loopLength + s->loopStart);
	const bool insideFixed = fixedScan &&
	                         (pos >= s->fixedPos && pos < s->fixedPos + FT2_MAX_RIGHT_TAPS);

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		const float *ptrF = rightChannel ? s->dataPtrFR : s->dataPtrF;
		const float *fixedF = rightChannel ? s->fixedSmpFR : s->fixedSmpF;
		if (ptrF == NULL)
			return yCenter;
		float v = insideFixed ? fixedF[pos - s->fixedPos] : ptrF[pos];
		int32_t sample = yCenter - (int32_t)roundf(v * (float)halfHeight);
		return CLAMP(sample, yCenter - halfHeight, yCenter + halfHeight - 1);
	}

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		const int32_t *ptr24 = rightChannel ? s->dataPtr24R : s->dataPtr24;
		const int32_t *fixed24 = rightChannel ? s->fixedSmp24R : s->fixedSmp24;
		if (ptr24 == NULL)
			return yCenter;
		const int32_t v = insideFixed ? fixed24[pos - s->fixedPos] : ptr24[pos];
		const int64_t y64 = (int64_t)v * halfHeight;
		int32_t sample = yCenter - (int32_t)(y64 >> 23);
		return CLAMP(sample, yCenter - halfHeight, yCenter + halfHeight - 1);
	}

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		const int16_t *ptr16 = rightChannel ? (const int16_t *)s->dataPtrR : (const int16_t *)s->dataPtr;
		const int16_t *fixed16 = rightChannel ? s->fixedSmpR : s->fixedSmp;
		if (ptr16 == NULL)
			return yCenter;
		const int32_t v = insideFixed ? fixed16[pos - s->fixedPos] : ptr16[pos];
		int32_t sample = yCenter - ((v * halfHeight) >> 15);
		return CLAMP(sample, yCenter - halfHeight, yCenter + halfHeight - 1);
	}

	{
		const int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
		const int16_t *fixed16 = rightChannel ? s->fixedSmpR : s->fixedSmp;
		if (ptr8 == NULL)
			return yCenter;
		const int32_t v = insideFixed ? (int8_t)fixed16[pos - s->fixedPos] : ptr8[pos];
		int32_t sample = yCenter - ((v * halfHeight) >> 7);
		return CLAMP(sample, yCenter - halfHeight, yCenter + halfHeight - 1);
	}
}

/* Scales sample value to display Y coordinate (mono or combined for stereo) */
static int32_t getScaledSample(ft2_sample_editor_t *ed, ft2_sample_t *s, int32_t pos)
{
	(void)ed;
	return getScaledSampleChannel(s, pos, false, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_HEIGHT / 2);
}

/* Converts mouse Y to sample value (0-255) for draw mode */
static int32_t mouseYToSampleY(int32_t my)
{
	my -= SAMPLE_AREA_Y_START;
	int32_t tmp32 = (int32_t)(my * (256.0 / SAMPLE_AREA_HEIGHT) + 0.5);
	return 255 - CLAMP(tmp32, 0, 255);
}

static int32_t mouseYToSampleYChannel(int32_t my, int32_t yStart, int32_t yHeight)
{
	my -= yStart;
	int32_t tmp32 = (int32_t)(my * (256.0 / yHeight) + 0.5);
	return 255 - CLAMP(tmp32, 0, 255);
}

static void getMinMax16(const int16_t *p, int32_t scanLen, int16_t *min16, int16_t *max16)
{
	int16_t minVal = 32767;
	int16_t maxVal = -32768;
	int32_t i = 0;
	while (i < scanLen)
	{
		const int16_t smp16 = p[i];
		if (smp16 < minVal) minVal = smp16;
		if (smp16 > maxVal) maxVal = smp16;
		i++;
	}
	*min16 = minVal;
	*max16 = maxVal;
}

static void getMinMax8(const int8_t *p, int32_t scanLen, int8_t *min8, int8_t *max8)
{
	int8_t minVal = 127;
	int8_t maxVal = -128;
	int32_t i = 0;
	while (i < scanLen)
	{
		const int8_t smp8 = p[i];
		if (smp8 < minVal) minVal = smp8;
		if (smp8 > maxVal) maxVal = smp8;
		i++;
	}
	*min8 = minVal;
	*max8 = maxVal;
}

static void getMinMax24(const int32_t *p, int32_t scanLen, int32_t *min24, int32_t *max24)
{
	int32_t minVal = 8388607;
	int32_t maxVal = -8388608;
	int32_t i = 0;
	while (i < scanLen)
	{
		const int32_t smp24 = p[i];
		if (smp24 < minVal) minVal = smp24;
		if (smp24 > maxVal) maxVal = smp24;
		i++;
	}
	*min24 = minVal;
	*max24 = maxVal;
}

static void getMinMaxF(const float *p, int32_t scanLen, float *minF, float *maxF)
{
	float minVal = 1.0f;
	float maxVal = -1.0f;
	int32_t i = 0;
	while (i < scanLen)
	{
		const float smpF = p[i];
		if (smpF < minVal) minVal = smpF;
		if (smpF > maxVal) maxVal = smpF;
		i++;
	}
	*minF = minVal;
	*maxF = maxVal;
}

static void getSpecialMinMax16(const ft2_sample_t *s, bool rightChannel,
                               int32_t index, int32_t scanEnd, int16_t *min16, int16_t *max16)
{
	int16_t minVal2, maxVal2;
	const int16_t *ptr16 = rightChannel ? (const int16_t *)s->dataPtrR : (const int16_t *)s->dataPtr;
	const int16_t *fixedSmp = rightChannel ? s->fixedSmpR : s->fixedSmp;
	int16_t minVal = 32767;
	int16_t maxVal = -32768;

	if (index < s->fixedPos)
	{
		getMinMax16(&ptr16[index], s->fixedPos - index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	const int32_t tapIndex = index - s->fixedPos;
	const int32_t scanLength = FT2_MAX_RIGHT_TAPS - tapIndex;
	int32_t tmpScanEnd = index + scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const int16_t *smpReadPtr = fixedSmp + tapIndex;
	while (index < tmpScanEnd)
	{
		const int16_t smp16 = *smpReadPtr++;
		if (smp16 < minVal) minVal = smp16;
		if (smp16 > maxVal) maxVal = smp16;
		index++;
	}

	if (index < scanEnd)
	{
		getMinMax16(&ptr16[index], scanEnd - index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*min16 = minVal;
	*max16 = maxVal;
}

static void getSpecialMinMax8(const ft2_sample_t *s, bool rightChannel,
                              int32_t index, int32_t scanEnd, int8_t *min8, int8_t *max8)
{
	int8_t minVal2, maxVal2;
	const int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
	const int16_t *fixedSmp = rightChannel ? s->fixedSmpR : s->fixedSmp;
	int8_t minVal = 127;
	int8_t maxVal = -128;

	if (index < s->fixedPos)
	{
		getMinMax8(&ptr8[index], s->fixedPos - index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	const int32_t tapIndex = index - s->fixedPos;
	const int32_t scanLength = FT2_MAX_RIGHT_TAPS - tapIndex;
	int32_t tmpScanEnd = index + scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const int16_t *smpReadPtr = fixedSmp + tapIndex;
	while (index < tmpScanEnd)
	{
		const int8_t smp8 = (int8_t)(*smpReadPtr++);
		if (smp8 < minVal) minVal = smp8;
		if (smp8 > maxVal) maxVal = smp8;
		index++;
	}

	if (index < scanEnd)
	{
		getMinMax8(&ptr8[index], scanEnd - index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*min8 = minVal;
	*max8 = maxVal;
}

static void getSpecialMinMax24(const ft2_sample_t *s, bool rightChannel,
                               int32_t index, int32_t scanEnd, int32_t *min24, int32_t *max24)
{
	int32_t minVal2, maxVal2;
	const int32_t *ptr24 = rightChannel ? s->dataPtr24R : s->dataPtr24;
	const int32_t *fixedSmp = rightChannel ? s->fixedSmp24R : s->fixedSmp24;
	int32_t minVal = 8388607;
	int32_t maxVal = -8388608;

	if (index < s->fixedPos)
	{
		getMinMax24(&ptr24[index], s->fixedPos - index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	const int32_t tapIndex = index - s->fixedPos;
	const int32_t scanLength = FT2_MAX_RIGHT_TAPS - tapIndex;
	int32_t tmpScanEnd = index + scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const int32_t *smpReadPtr = fixedSmp + tapIndex;
	while (index < tmpScanEnd)
	{
		const int32_t smp24 = *smpReadPtr++;
		if (smp24 < minVal) minVal = smp24;
		if (smp24 > maxVal) maxVal = smp24;
		index++;
	}

	if (index < scanEnd)
	{
		getMinMax24(&ptr24[index], scanEnd - index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*min24 = minVal;
	*max24 = maxVal;
}

static void getSpecialMinMaxF(const ft2_sample_t *s, bool rightChannel,
                              int32_t index, int32_t scanEnd, float *minF, float *maxF)
{
	float minVal2, maxVal2;
	const float *ptrF = rightChannel ? s->dataPtrFR : s->dataPtrF;
	const float *fixedSmp = rightChannel ? s->fixedSmpFR : s->fixedSmpF;
	float minVal = 1.0f;
	float maxVal = -1.0f;

	if (index < s->fixedPos)
	{
		getMinMaxF(&ptrF[index], s->fixedPos - index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	const int32_t tapIndex = index - s->fixedPos;
	const int32_t scanLength = FT2_MAX_RIGHT_TAPS - tapIndex;
	int32_t tmpScanEnd = index + scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const float *smpReadPtr = fixedSmp + tapIndex;
	while (index < tmpScanEnd)
	{
		const float smpF = *smpReadPtr++;
		if (smpF < minVal) minVal = smpF;
		if (smpF > maxVal) maxVal = smpF;
		index++;
	}

	if (index < scanEnd)
	{
		getMinMaxF(&ptrF[index], scanEnd - index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*minF = minVal;
	*maxF = maxVal;
}

/* Gets min/max sample values for a range with channel selection and Y range */
static void getSampleDataPeakChannel(ft2_sample_t *s, int32_t start, int32_t count,
                                      bool rightChannel, int32_t yCenter, int32_t halfHeight,
                                      int16_t *outMin, int16_t *outMax)
{
	const ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || count <= 0)
	{
		*outMin = *outMax = (int16_t)yCenter;
		return;
	}

	if (start < 0)
		start = 0;
	if (start + count > s->length)
		count = s->length - start;
	if (count <= 0)
	{
		*outMin = *outMax = (int16_t)yCenter;
		return;
	}

	const int32_t scanEnd = start + count;
	const bool fixedScan = (s->isFixed && s->length > s->loopLength + s->loopStart);
	const bool insideRange = fixedScan &&
	                         ((start >= s->fixedPos && start < s->fixedPos + FT2_MAX_RIGHT_TAPS) ||
	                          (start < s->fixedPos && scanEnd >= s->fixedPos));

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		float minF, maxF;
		const float *ptrF = rightChannel ? s->dataPtrFR : s->dataPtrF;
		if (ptrF == NULL)
		{
			*outMin = *outMax = (int16_t)yCenter;
			return;
		}

		if (insideRange)
			getSpecialMinMaxF(s, rightChannel, start, scanEnd, &minF, &maxF);
		else
			getMinMaxF(&ptrF[start], count, &minF, &maxF);

		int32_t minY = yCenter - (int32_t)roundf(minF * (float)halfHeight);
		int32_t maxY = yCenter - (int32_t)roundf(maxF * (float)halfHeight);
		*outMin = (int16_t)CLAMP(minY, yCenter - halfHeight, yCenter + halfHeight - 1);
		*outMax = (int16_t)CLAMP(maxY, yCenter - halfHeight, yCenter + halfHeight - 1);
		return;
	}

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		int32_t min24, max24;
		const int32_t *ptr24 = rightChannel ? s->dataPtr24R : s->dataPtr24;
		if (ptr24 == NULL)
		{
			*outMin = *outMax = (int16_t)yCenter;
			return;
		}

		if (insideRange)
			getSpecialMinMax24(s, rightChannel, start, scanEnd, &min24, &max24);
		else
			getMinMax24(&ptr24[start], count, &min24, &max24);

		const int64_t minY64 = (int64_t)min24 * halfHeight;
		const int64_t maxY64 = (int64_t)max24 * halfHeight;
		int32_t minY = yCenter - (int32_t)(minY64 >> 23);
		int32_t maxY = yCenter - (int32_t)(maxY64 >> 23);
		*outMin = (int16_t)CLAMP(minY, yCenter - halfHeight, yCenter + halfHeight - 1);
		*outMax = (int16_t)CLAMP(maxY, yCenter - halfHeight, yCenter + halfHeight - 1);
		return;
	}

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		int16_t min16, max16;
		const int16_t *ptr16 = rightChannel ? (const int16_t *)s->dataPtrR : (const int16_t *)s->dataPtr;
		if (ptr16 == NULL)
		{
			*outMin = *outMax = (int16_t)yCenter;
			return;
		}

		if (insideRange)
			getSpecialMinMax16(s, rightChannel, start, scanEnd, &min16, &max16);
		else
			getMinMax16(&ptr16[start], count, &min16, &max16);

		int32_t minY = yCenter - ((min16 * halfHeight) >> 15);
		int32_t maxY = yCenter - ((max16 * halfHeight) >> 15);
		*outMin = (int16_t)CLAMP(minY, yCenter - halfHeight, yCenter + halfHeight - 1);
		*outMax = (int16_t)CLAMP(maxY, yCenter - halfHeight, yCenter + halfHeight - 1);
		return;
	}

	{
		int8_t min8, max8;
		const int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
		if (ptr8 == NULL)
		{
			*outMin = *outMax = (int16_t)yCenter;
			return;
		}

		if (insideRange)
			getSpecialMinMax8(s, rightChannel, start, scanEnd, &min8, &max8);
		else
			getMinMax8(&ptr8[start], count, &min8, &max8);

		int32_t minY = yCenter - ((min8 * halfHeight) >> 7);
		int32_t maxY = yCenter - ((max8 * halfHeight) >> 7);
		*outMin = (int16_t)CLAMP(minY, yCenter - halfHeight, yCenter + halfHeight - 1);
		*outMax = (int16_t)CLAMP(maxY, yCenter - halfHeight, yCenter + halfHeight - 1);
	}
}

/* Gets min/max sample values for a range (zoomed-out display) - mono wrapper */
static void getSampleDataPeak(ft2_sample_t *s, int32_t start, int32_t count, int16_t *outMin, int16_t *outMax)
{
	getSampleDataPeakChannel(s, start, count, false, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_HEIGHT / 2, outMin, outMax);
}

/* ------------------------------------------------------------------------- */
/*                       COORDINATE CONVERSION                               */
/* ------------------------------------------------------------------------- */

/* Updates scaling factors for view<->sample coordinate conversion */
static void updateScalingFactors(ft2_sample_editor_t *ed)
{
	if (!ed) return;

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
	/* floor() required here to match original FT2 behavior */
	ed->dScrPosScaled = floor(ed->scrPos * ed->dPos2ScrMul);
}

/* Screen X -> sample position */
int32_t ft2_sample_scr2SmpPos(ft2_instance_t *inst, int32_t x)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || editor->viewSize <= 0) return 0;
	if (x < 0) x = 0;

	int32_t smpPos = (int32_t)((editor->dScrPosScaled + x) * editor->dScr2SmpPosMul);

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s && smpPos > s->length) smpPos = s->length;
	return smpPos;
}

/* Sample position -> screen X */
int32_t ft2_sample_smpPos2Scr(ft2_instance_t *inst, int32_t pos)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || editor->viewSize <= 0) return -1;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (!s) return -1;
	if (pos > s->length) pos = s->length;

	double dPos = (pos * editor->dPos2ScrMul) + 0.5 - editor->dScrPosScaled;
	return (int32_t)CLAMP(dPos, INT32_MIN, INT32_MAX);
}

/* ------------------------------------------------------------------------- */
/*                           INITIALIZATION                                  */
/* ------------------------------------------------------------------------- */

void ft2_sample_ed_init(ft2_sample_editor_t *editor, ft2_video_t *video)
{
	if (!editor) return;
	memset(editor, 0, sizeof(ft2_sample_editor_t));
	editor->video = video;
	editor->currInstr = 1;
	editor->oldSmpPosLine = -1;
	editor->oldRangeStart = 0;
	editor->oldRangeEnd = 0;
	editor->oldHasRange = false;
	editor->oldLoopStart = 0;
	editor->oldLoopLength = 0;
	editor->oldLoopActive = false;
	editor->dPos2ScrMul = 1.0;
	editor->dScr2SmpPosMul = 1.0;
	editor->waveformCache = (uint32_t *)malloc((size_t)SAMPLE_AREA_WIDTH * SAMPLE_AREA_HEIGHT * sizeof(uint32_t));
	editor->waveformCacheValid = false;
	editor->waveformDirty = true;
	editor->overlaysDirty = true;
	editor->cacheInstr = -1;
	editor->cacheSample = -1;
	editor->peakMinL = NULL;
	editor->peakMaxL = NULL;
	editor->peakMinR = NULL;
	editor->peakMaxR = NULL;
	editor->peakBlockSize = PEAK_CACHE_BLOCK_SIZE;
	editor->peakBlockCount = 0;
	editor->peakCacheValid = false;
	editor->peakInstr = -1;
	editor->peakSample = -1;
	editor->peakLength = 0;
	editor->peakFormat = FT2_SMP_FORMAT_8BIT;
	editor->peakStereo = false;
	editor->peakDataPtrL = NULL;
	editor->peakDataPtrR = NULL;

	/* Sample effects defaults */
	editor->smpfx.lastFilterType = FILTER_LOWPASS;
	editor->smpfx.lastLpCutoff = 2000;
	editor->smpfx.lastHpCutoff = 200;
	editor->smpfx.smpCycles = 1;
	editor->smpfx.lastWaveLength = 64;
	editor->smpfx.lastAmp = 75;
}

void ft2_sample_ed_free(ft2_sample_editor_t *editor)
{
	if (!editor)
		return;

	if (editor->waveformCache)
	{
		free(editor->waveformCache);
		editor->waveformCache = NULL;
	}

	freePeakCache(editor);
}

/*
** Sets active sample and updates view state.
** Only resets view when slot changes or current view is invalid.
** Preserves user zoom when just refreshing the same sample.
*/
void ft2_sample_ed_set_sample(ft2_instance_t *inst, int instr, int sample)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor) return;

	bool slotChanged = (editor->currInstr != instr || editor->currSample != sample);
	editor->currInstr = (int16_t)instr;
	editor->currSample = (int16_t)sample;

	int32_t smpLen = 0;
	if (inst && instr > 0 && instr < 128)
	{
		ft2_instr_t *ins = inst->replayer.instr[instr];
		const int32_t maxSamples = getMaxSamples(inst);
		if (ins && sample >= 0 && sample < maxSamples)
			smpLen = ins->smp[sample].length;
	}

	bool needsViewReset = slotChanged ||
	                      (smpLen > 0 && editor->viewSize > smpLen) ||
	                      (smpLen > 0 && editor->viewSize <= 0);

	if (slotChanged)
	{
		editor->rangeStart = editor->rangeEnd = 0;
		editor->hasRange = false;
		editor->oldRangeStart = 0;
		editor->oldRangeEnd = 0;
		editor->oldHasRange = false;
		editor->oldLoopStart = 0;
		editor->oldLoopLength = 0;
		editor->oldLoopActive = false;
		editor->oldSmpPosLine = -1;
		editor->overlaysDirty = true;
	}

	if (needsViewReset)
	{
		editor->viewSize = editor->oldViewSize = smpLen;
		editor->scrPos = editor->oldScrPos = 0;
		editor->rangeStart = editor->rangeEnd = 0;
		editor->hasRange = false;
	}

	/* Sanitize scroll position */
	if (smpLen > 0 && editor->scrPos + editor->viewSize > smpLen)
	{
		editor->scrPos = smpLen - editor->viewSize;
		if (editor->scrPos < 0)
		{
			editor->scrPos = 0;
			editor->viewSize = smpLen;
		}
	}

	/* Sync scrollbar */
	ft2_widgets_t *widgets = (inst && inst->ui) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
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

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (s && s->length > 0)
	{
		ft2_sample_format_t format = sampleGetFormat(s);
		if (sampleHasData(s, format))
			ensurePeakCache(editor, s, format, sampleIsStereo(s));
	}
}

/* ------------------------------------------------------------------------- */
/*                        WAVEFORM RENDERING                                 */
/* ------------------------------------------------------------------------- */

/* Draw waveform for a single channel in specified Y region */
static void drawWaveformChannel(ft2_instance_t *inst, ft2_sample_t *s, ft2_video_t *video,
                                 bool rightChannel, int32_t yStart, int32_t yHeight, int32_t yCenter)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	const int32_t halfHeight = yHeight / 2;
	(void)yStart;
	(void)yHeight;

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
			int32_t y1 = getScaledSampleChannel(s, currSmpPos, rightChannel, yCenter, halfHeight);
			int32_t y2 = getScaledSampleChannel(s, nextSmpPos, rightChannel, yCenter, halfHeight);

			x1 = CLAMP(x1, 0, SAMPLE_AREA_WIDTH - 1);
			x2 = CLAMP(x2, 0, SAMPLE_AREA_WIDTH - 1);

			if (x == SAMPLE_AREA_WIDTH)
				x2 = SAMPLE_AREA_WIDTH - 1;

			sampleLineFast(video, x1, x2, y1, y2);
		}
	}
	else
	{
		/* Zoomed out */
		const int32_t firstSamplePoint = getScaledSampleChannel(s, ft2_sample_scr2SmpPos(inst, 0),
		                                                         rightChannel, yCenter, halfHeight);

		int32_t oldMin = firstSamplePoint;
		int32_t oldMax = firstSamplePoint;

		for (int16_t x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			int32_t smpIdx = ft2_sample_scr2SmpPos(inst, x);
			int32_t smpNum = ft2_sample_scr2SmpPos(inst, x + 1) - smpIdx;

			if (smpIdx + smpNum > s->length)
				smpNum = s->length - smpIdx;

			if (smpNum > 0)
			{
				int16_t min, max;
				if (ed->peakCacheValid && ed->viewSize > SAMPLE_AREA_WIDTH)
					getSampleDataPeakChannelCached(ed, s, sampleGetFormat(s), rightChannel, smpIdx, smpNum,
					                               yCenter, halfHeight, &min, &max);
				else
					getSampleDataPeakChannel(s, smpIdx, smpNum, rightChannel, yCenter, halfHeight, &min, &max);

				if (x != 0)
				{
					if (min > oldMax) sampleLineFast(video, x - 1, x, oldMax, min);
					if (max < oldMin) sampleLineFast(video, x - 1, x, oldMin, max);
				}

				sampleLineFast(video, x, x, max, min);

				oldMin = min;
				oldMax = max;
			}
		}
	}
}

static void cacheSampleArea(ft2_sample_editor_t *editor)
{
	if (!editor || !editor->video || editor->waveformCache == NULL)
		return;

	ft2_video_t *video = editor->video;
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++)
	{
		const uint32_t *src = &video->frameBuffer[(SAMPLE_AREA_Y_START + y) * SCREEN_W];
		uint32_t *dst = &editor->waveformCache[y * SAMPLE_AREA_WIDTH];
		memcpy(dst, src, SAMPLE_AREA_WIDTH * sizeof(uint32_t));
	}
	editor->waveformCacheValid = true;
}

static void blitSampleAreaCache(ft2_sample_editor_t *editor)
{
	if (!editor || !editor->video || editor->waveformCache == NULL || !editor->waveformCacheValid)
		return;

	ft2_video_t *video = editor->video;
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++)
	{
		const uint32_t *src = &editor->waveformCache[y * SAMPLE_AREA_WIDTH];
		uint32_t *dst = &video->frameBuffer[(SAMPLE_AREA_Y_START + y) * SCREEN_W];
		memcpy(dst, src, SAMPLE_AREA_WIDTH * sizeof(uint32_t));
	}
}

static const void *getSampleDataPtr(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel)
{
	if (s == NULL)
		return NULL;

	return ft2_sample_get_data_ptr_const(s, format, rightChannel);
}

static int32_t sampleTo24bit(ft2_sample_format_t format, int32_t sample)
{
	switch (format)
	{
		case FT2_SMP_FORMAT_16BIT: return sample << 8;
		case FT2_SMP_FORMAT_8BIT:  return sample << 16;
		case FT2_SMP_FORMAT_24BIT: return sample;
		default: return sample;
	}
}

static int32_t getSampleValue24(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t pos)
{
	if (!s || pos < 0 || pos >= s->length)
		return 0;

	const bool fixedScan = (s->isFixed && s->length > s->loopLength + s->loopStart);
	const bool insideFixed = fixedScan &&
	                         (pos >= s->fixedPos && pos < s->fixedPos + FT2_MAX_RIGHT_TAPS);

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		const float *ptrF = rightChannel ? s->dataPtrFR : s->dataPtrF;
		const float *fixedF = rightChannel ? s->fixedSmpFR : s->fixedSmpF;
		if (ptrF == NULL)
			return 0;

		float v = insideFixed ? fixedF[pos - s->fixedPos] : ptrF[pos];
		if (v > 1.0f) v = 1.0f;
		if (v < -1.0f) v = -1.0f;
		int32_t v24 = (int32_t)roundf(v * 8388607.0f);
		if (v24 > 8388607) v24 = 8388607;
		if (v24 < -8388608) v24 = -8388608;
		return v24;
	}

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		const int32_t *ptr24 = rightChannel ? s->dataPtr24R : s->dataPtr24;
		const int32_t *fixed24 = rightChannel ? s->fixedSmp24R : s->fixedSmp24;
		if (ptr24 == NULL)
			return 0;
		return insideFixed ? fixed24[pos - s->fixedPos] : ptr24[pos];
	}

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		const int16_t *ptr16 = rightChannel ? (const int16_t *)s->dataPtrR : (const int16_t *)s->dataPtr;
		const int16_t *fixed16 = rightChannel ? s->fixedSmpR : s->fixedSmp;
		if (ptr16 == NULL)
			return 0;
		int32_t v = insideFixed ? fixed16[pos - s->fixedPos] : ptr16[pos];
		return sampleTo24bit(format, v);
	}

	{
		const int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
		const int16_t *fixed16 = rightChannel ? s->fixedSmpR : s->fixedSmp;
		if (ptr8 == NULL)
			return 0;
		int32_t v = insideFixed ? (int8_t)fixed16[pos - s->fixedPos] : ptr8[pos];
		return sampleTo24bit(format, v);
	}
}

static void freePeakCache(ft2_sample_editor_t *editor)
{
	if (!editor)
		return;

	if (editor->peakMinL) { free(editor->peakMinL); editor->peakMinL = NULL; }
	if (editor->peakMaxL) { free(editor->peakMaxL); editor->peakMaxL = NULL; }
	if (editor->peakMinR) { free(editor->peakMinR); editor->peakMinR = NULL; }
	if (editor->peakMaxR) { free(editor->peakMaxR); editor->peakMaxR = NULL; }

	editor->peakBlockCount = 0;
	editor->peakCacheValid = false;
}

static bool peakCacheNeedsRebuild(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                                  ft2_sample_format_t format, bool isStereo)
{
	if (!editor || !s)
		return true;
	if (!editor->peakCacheValid)
		return true;
	if (editor->peakInstr != editor->currInstr || editor->peakSample != editor->currSample)
		return true;
	if (editor->peakLength != s->length)
		return true;
	if (editor->peakFormat != format || editor->peakStereo != isStereo)
		return true;

	const void *dataPtrL = getSampleDataPtr(s, format, false);
	if (editor->peakDataPtrL != dataPtrL)
		return true;

	const void *dataPtrR = isStereo ? getSampleDataPtr(s, format, true) : NULL;
	if (editor->peakDataPtrR != dataPtrR)
		return true;

	return false;
}

static bool allocatePeakCache(ft2_sample_editor_t *editor, int32_t blockCount, bool isStereo)
{
	if (!editor || blockCount <= 0)
		return false;

	freePeakCache(editor);

	editor->peakMinL = (int32_t *)malloc((size_t)blockCount * sizeof(int32_t));
	editor->peakMaxL = (int32_t *)malloc((size_t)blockCount * sizeof(int32_t));
	if (!editor->peakMinL || !editor->peakMaxL)
	{
		freePeakCache(editor);
		return false;
	}

	if (isStereo)
	{
		editor->peakMinR = (int32_t *)malloc((size_t)blockCount * sizeof(int32_t));
		editor->peakMaxR = (int32_t *)malloc((size_t)blockCount * sizeof(int32_t));
		if (!editor->peakMinR || !editor->peakMaxR)
		{
			freePeakCache(editor);
			return false;
		}
	}

	editor->peakBlockCount = blockCount;
	editor->peakBlockSize = PEAK_CACHE_BLOCK_SIZE;
	return true;
}

static void buildPeakCacheChannel(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                                  ft2_sample_format_t format, bool rightChannel,
                                  int32_t blockSize, int32_t blockCount,
                                  int32_t *minDst, int32_t *maxDst)
{
	int32_t block = 0;
	int32_t idx = 0;

	while (block < blockCount)
	{
		int32_t minVal = 8388607;
		int32_t maxVal = -8388608;
		int32_t i = 0;

		while (i < blockSize && idx < s->length)
		{
			const int32_t v = getSampleValue24(s, format, rightChannel, idx);
			if (v < minVal) minVal = v;
			if (v > maxVal) maxVal = v;
			idx++;
			i++;
		}

		minDst[block] = minVal;
		maxDst[block] = maxVal;
		block++;
	}
}

static bool buildPeakCache(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                           ft2_sample_format_t format, bool isStereo)
{
	if (!editor || !s || s->length <= 0)
		return false;

	const int32_t blockSize = PEAK_CACHE_BLOCK_SIZE;
	const int32_t blockCount = (s->length + blockSize - 1) / blockSize;
	if (!allocatePeakCache(editor, blockCount, isStereo))
		return false;

	buildPeakCacheChannel(editor, s, format, false, blockSize, blockCount,
	                      editor->peakMinL, editor->peakMaxL);
	if (isStereo)
		buildPeakCacheChannel(editor, s, format, true, blockSize, blockCount,
		                      editor->peakMinR, editor->peakMaxR);

	editor->peakCacheValid = true;
	editor->peakInstr = editor->currInstr;
	editor->peakSample = editor->currSample;
	editor->peakLength = s->length;
	editor->peakFormat = format;
	editor->peakStereo = isStereo;
	editor->peakDataPtrL = getSampleDataPtr(s, format, false);
	editor->peakDataPtrR = isStereo ? getSampleDataPtr(s, format, true) : NULL;
	return true;
}

static bool ensurePeakCache(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                             ft2_sample_format_t format, bool isStereo)
{
	if (!editor || !s)
		return false;
	if (!peakCacheNeedsRebuild(editor, s, format, isStereo))
		return true;
	return buildPeakCache(editor, s, format, isStereo);
}

static void getSampleDataPeakChannelCached(ft2_sample_editor_t *editor, ft2_sample_t *s,
                                           ft2_sample_format_t format, bool rightChannel,
                                           int32_t start, int32_t count, int32_t yCenter,
                                           int32_t halfHeight, int16_t *outMin, int16_t *outMax)
{
	if (!editor || !s || count <= 0 || !editor->peakCacheValid || editor->peakBlockCount <= 0)
	{
		getSampleDataPeakChannel(s, start, count, rightChannel, yCenter, halfHeight, outMin, outMax);
		return;
	}

	if (start < 0)
		start = 0;
	if (start + count > s->length)
		count = s->length - start;
	if (count <= 0)
	{
		*outMin = *outMax = (int16_t)yCenter;
		return;
	}

	const int32_t end = start + count;
	const int32_t blockSize = editor->peakBlockSize;
	int32_t firstBlock = start / blockSize;
	int32_t lastBlock = (end - 1) / blockSize;
	if (firstBlock < 0) firstBlock = 0;
	if (lastBlock >= editor->peakBlockCount) lastBlock = editor->peakBlockCount - 1;

	int32_t minVal = 8388607;
	int32_t maxVal = -8388608;

	if (firstBlock == lastBlock)
	{
		int32_t idx = start;
		while (idx < end)
		{
			const int32_t v = getSampleValue24(s, format, rightChannel, idx);
			if (v < minVal) minVal = v;
			if (v > maxVal) maxVal = v;
			idx++;
		}
	}
	else
	{
		int32_t idx = start;
		int32_t blockEnd = (firstBlock + 1) * blockSize;
		while (idx < blockEnd)
		{
			const int32_t v = getSampleValue24(s, format, rightChannel, idx);
			if (v < minVal) minVal = v;
			if (v > maxVal) maxVal = v;
			idx++;
		}

		const int32_t *minSrc = rightChannel ? editor->peakMinR : editor->peakMinL;
		const int32_t *maxSrc = rightChannel ? editor->peakMaxR : editor->peakMaxL;
		int32_t block = firstBlock + 1;
		while (block < lastBlock)
		{
			const int32_t bmin = minSrc[block];
			const int32_t bmax = maxSrc[block];
			if (bmin < minVal) minVal = bmin;
			if (bmax > maxVal) maxVal = bmax;
			block++;
		}

		idx = lastBlock * blockSize;
		while (idx < end)
		{
			const int32_t v = getSampleValue24(s, format, rightChannel, idx);
			if (v < minVal) minVal = v;
			if (v > maxVal) maxVal = v;
			idx++;
		}
	}

	const int64_t minY64 = (int64_t)minVal * halfHeight;
	const int64_t maxY64 = (int64_t)maxVal * halfHeight;
	int32_t minY = yCenter - (int32_t)(minY64 >> 23);
	int32_t maxY = yCenter - (int32_t)(maxY64 >> 23);
	*outMin = (int16_t)CLAMP(minY, yCenter - halfHeight, yCenter + halfHeight - 1);
	*outMax = (int16_t)CLAMP(maxY, yCenter - halfHeight, yCenter + halfHeight - 1);
}

static bool waveformNeedsRedraw(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                                 ft2_sample_format_t format, bool isStereo)
{
	if (!editor || editor->waveformCache == NULL)
		return true;
	if (!editor->waveformCacheValid || editor->waveformDirty)
		return true;
	if (editor->cacheInstr != editor->currInstr || editor->cacheSample != editor->currSample)
		return true;
	if (editor->cacheScrPos != editor->scrPos || editor->cacheViewSize != editor->viewSize)
		return true;

	const int32_t length = (s != NULL) ? s->length : 0;
	if (editor->cacheLength != length)
		return true;

	if (s == NULL)
		return false;

	if (editor->cacheFormat != format || editor->cacheStereo != isStereo)
		return true;

	const void *dataPtr = getSampleDataPtr(s, format, false);
	if (editor->cacheDataPtr != dataPtr)
		return true;

	const void *dataPtrR = isStereo ? getSampleDataPtr(s, format, true) : NULL;
	if (editor->cacheDataPtrR != dataPtrR)
		return true;

	return false;
}

static void updateWaveformCacheKeys(ft2_sample_editor_t *editor, const ft2_sample_t *s,
                                    ft2_sample_format_t format, bool isStereo)
{
	if (!editor)
		return;

	editor->cacheInstr = editor->currInstr;
	editor->cacheSample = editor->currSample;
	editor->cacheScrPos = editor->scrPos;
	editor->cacheViewSize = editor->viewSize;
	editor->cacheLength = (s != NULL) ? s->length : 0;
	editor->cacheFormat = format;
	editor->cacheStereo = isStereo;
	editor->cacheDataPtr = getSampleDataPtr(s, format, false);
	editor->cacheDataPtrR = isStereo ? getSampleDataPtr(s, format, true) : NULL;
	editor->waveformDirty = false;
	editor->waveformCacheValid = (editor->waveformCache != NULL);
}

void ft2_sample_ed_draw_waveform(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed || !ed->video || !inst) return;

	ft2_video_t *video = ed->video;

	if (ed->currInstr <= 0 || ed->currInstr >= 128) return;
	ft2_instr_t *instr = inst->replayer.instr[ed->currInstr];
	if (!instr) return;
	const int32_t maxSamples = getMaxSamples(inst);
	if (ed->currSample < 0 || ed->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[ed->currSample];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length <= 0 || ed->viewSize <= 0) return;

	updateScalingFactors(ed);

	bool isStereo = sampleIsStereo(s);
	ensurePeakCache(ed, s, format, isStereo);

	if (isStereo)
	{
		/* Draw separator line between L and R channels */
		hLine(video, 0, SAMPLE_AREA_SEPARATOR, SAMPLE_AREA_WIDTH, PAL_DESKTOP);

		/* Draw center lines for L and R */
		hLine(video, 0, SAMPLE_AREA_L_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);
		hLine(video, 0, SAMPLE_AREA_R_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);

		/* Draw L channel in top half */
		drawWaveformChannel(inst, s, video, false, SAMPLE_AREA_L_START, SAMPLE_AREA_L_HEIGHT, SAMPLE_AREA_L_CENTER);

		/* Draw R channel in bottom half */
		drawWaveformChannel(inst, s, video, true, SAMPLE_AREA_R_START, SAMPLE_AREA_R_HEIGHT, SAMPLE_AREA_R_CENTER);

		/* Draw "L" and "R" labels */
		if (ed->bmp)
		{
			textOutShadow(video, ed->bmp, 2, SAMPLE_AREA_L_START + 2, PAL_FORGRND, PAL_DSKTOP2, "L");
			textOutShadow(video, ed->bmp, 2, SAMPLE_AREA_R_START + 2, PAL_FORGRND, PAL_DSKTOP2, "R");
		}
	}
	else
	{
		/* Mono: draw single waveform using full height */
		hLine(video, 0, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);
		drawWaveformChannel(inst, s, video, false, SAMPLE_AREA_Y_START, SAMPLE_AREA_HEIGHT, SAMPLE_AREA_Y_CENTER);
	}
}

static void xorSampleRange(ft2_instance_t *inst, int32_t start, int32_t end)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed || !ed->video) return;

	ft2_video_t *video = ed->video;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }

	int32_t x1 = CLAMP(ft2_sample_smpPos2Scr(inst, start), 0, SAMPLE_AREA_WIDTH - 1);
	int32_t x2 = CLAMP(ft2_sample_smpPos2Scr(inst, end), 0, SAMPLE_AREA_WIDTH - 1);
	if (x1 > x2) return;

	int32_t rangeLen = (x2 + 1) - x1;
	for (int32_t y = SAMPLE_AREA_Y_START; y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT; y++)
	{
		uint32_t *ptr = &video->frameBuffer[(y * SCREEN_W) + x1];
		for (int32_t x = 0; x < rangeLen; x++, ptr++)
			*ptr = video->palette[(*ptr >> 24) ^ 2];
	}
}

/* Draws selection range by XORing with palette index 2 */
void ft2_sample_ed_draw_range(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed || !ed->video || !ed->hasRange) return;

	xorSampleRange(inst, ed->rangeStart, ed->rangeEnd);
}
/* ------------------------------------------------------------------------- */
/*                          LOOP PIN DRAWING                                 */
/* ------------------------------------------------------------------------- */

/* Draws loop pin sprite with clipping and transparency */
static void drawLoopPinSprite(ft2_video_t *video, const uint8_t *src8, int32_t xPos)
{
	if (!video || !src8) return;

	const int32_t spriteW = 16, spriteH = SAMPLE_AREA_HEIGHT;
	int32_t sw = spriteW, sx = xPos;
	const uint8_t *srcPtr = src8;

	/* Left clip */
	if (sx < 0) { sw += sx; srcPtr -= sx; sx = 0; }
	if (sw <= 0) return;

	/* Right clip */
	if (sx + sw > SCREEN_W) sw = SCREEN_W - sx;
	if (sw <= 0) return;

	int32_t srcPitch = spriteW - sw, dstPitch = SCREEN_W - sw;
	uint32_t *dst32 = &video->frameBuffer[(SAMPLE_AREA_Y_START * SCREEN_W) + sx];

	for (int32_t y = 0; y < spriteH; y++)
	{
		for (int32_t x = 0; x < sw; x++, dst32++, srcPtr++)
			if (*srcPtr != PAL_TRANSPR)
				*dst32 = video->palette[*srcPtr];
		srcPtr += srcPitch;
		dst32 += dstPitch;
	}
}

/* Draws loop start/end pin sprites. Bitmap has 4 states: left normal/clicked, right normal/clicked */
void ft2_sample_ed_draw_loop_points(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed || !ed->video || !inst || !ed->bmp || !ed->bmp->loopPins) return;

	if (ed->currInstr <= 0 || ed->currInstr >= 128) return;
	ft2_instr_t *instr = inst->replayer.instr[ed->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || ed->currSample < 0 || ed->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[ed->currSample];
	if (GET_LOOPTYPE(s->flags) == LOOP_OFF || s->loopLength <= 0) return;

	int32_t x1 = ft2_sample_smpPos2Scr(inst, s->loopStart);
	int32_t x2 = ft2_sample_smpPos2Scr(inst, s->loopStart + s->loopLength);
	const int32_t pinDataSize = 16 * SAMPLE_AREA_HEIGHT;

	if (x1 >= -8 && x1 <= SAMPLE_AREA_WIDTH + 8)
	{
		int32_t state = inst->uiState.leftLoopPinMoving ? 1 : 0;
		drawLoopPinSprite(ed->video, &ed->bmp->loopPins[state * pinDataSize], x1 - 8);
	}

	if (x2 >= -8 && x2 <= SAMPLE_AREA_WIDTH + 8)
	{
		int32_t state = inst->uiState.rightLoopPinMoving ? 3 : 2;
		drawLoopPinSprite(ed->video, &ed->bmp->loopPins[state * pinDataSize], x2 - 8);
	}
}

/* ------------------------------------------------------------------------- */
/*                         UI STATE HELPERS                                  */
/* ------------------------------------------------------------------------- */

static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };

static void drawSampleBitDepthLabels(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp, const ft2_sample_t *s)
{
	if (bmp == NULL || video == NULL)
		return;

	(void)s;

	if (inst != NULL && inst->config.enableXm2Features)
		return;

	if (inst == NULL || !inst->config.enableXm2Features)
	{
		textOutShadow(video, bmp, 446, 369, PAL_FORGRND, PAL_DSKTOP2, "8-bit");
		textOutShadow(video, bmp, 445, 384, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
	}
}

/* Syncs radio button state with current sample's loop type and bit depth */
static void updateSampleEditorRadioButtons(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	if (!editor || !inst) return;

	ft2_sample_t *s = NULL;
	if (editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		const int32_t maxSamples = getMaxSamples(inst);
		if (instr && editor->currSample >= 0 && editor->currSample < maxSamples)
			s = &instr->smp[editor->currSample];
	}

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	if (!s)
	{
		widgets->radioButtonState[RB_SAMPLE_NO_LOOP] = RADIOBUTTON_CHECKED;
		widgets->radioButtonState[RB_SAMPLE_FWD_LOOP] = RADIOBUTTON_UNCHECKED;
		widgets->radioButtonState[RB_SAMPLE_BIDI_LOOP] = RADIOBUTTON_UNCHECKED;
		widgets->radioButtonState[RB_SAMPLE_8BIT] = RADIOBUTTON_CHECKED;
		widgets->radioButtonState[RB_SAMPLE_16BIT] = RADIOBUTTON_UNCHECKED;
		return;
	}

	uint8_t loopType = GET_LOOPTYPE(s->flags);
	widgets->radioButtonState[RB_SAMPLE_NO_LOOP] = (loopType == LOOP_OFF) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
	widgets->radioButtonState[RB_SAMPLE_FWD_LOOP] = (loopType == LOOP_FWD) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
	widgets->radioButtonState[RB_SAMPLE_BIDI_LOOP] = (loopType == LOOP_BIDI) ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;

	ft2_sample_format_t format = sampleGetFormat(s);
	const bool is16Bit = (format != FT2_SMP_FORMAT_8BIT);
	widgets->radioButtonState[RB_SAMPLE_8BIT] = is16Bit ? RADIOBUTTON_UNCHECKED : RADIOBUTTON_CHECKED;
	widgets->radioButtonState[RB_SAMPLE_16BIT] = is16Bit ? RADIOBUTTON_CHECKED : RADIOBUTTON_UNCHECKED;
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

static void invertSamplePosLine(ft2_sample_editor_t *editor, int32_t x)
{
	if (!editor || !editor->video)
		return;
	if (x < 0 || x >= SAMPLE_AREA_WIDTH)
		return;

	ft2_video_t *video = editor->video;
	uint32_t *ptr32 = &video->frameBuffer[(SAMPLE_AREA_Y_START * SCREEN_W) + x];
	int32_t y = 0;
	while (y < SAMPLE_AREA_HEIGHT)
	{
		*ptr32 = video->palette[(*ptr32 >> 24) ^ 1];
		ptr32 += SCREEN_W;
		y++;
	}
}

/* Draws vertical playback position line for first voice playing this sample */
void ft2_sample_ed_draw_pos_line(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !editor->video || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || editor->currSample < 0 || editor->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length <= 0) return;

	/* Use the tracked channel for the current sample (set by ft2_instance_trigger_note) */
	uint8_t ch = inst->editor.curSmpChannel;
	if (ch >= FT2_MAX_CHANNELS) return;

	ft2_voice_t *v = &inst->voice[ch];
	if (!v->active)
	{
		if (editor->oldSmpPosLine != -1)
			invertSamplePosLine(editor, editor->oldSmpPosLine);
		editor->oldSmpPosLine = -1;
		return;
	}

	bool isSameSample = false;
	if (v->base8 && s->dataPtr != NULL && v->base8 == (const int8_t *)s->dataPtr)
		isSameSample = true;
	else if (v->base16 && s->dataPtr != NULL && v->base16 == (const int16_t *)s->dataPtr)
		isSameSample = true;
	else if (v->base24 && s->dataPtr24 != NULL && v->base24 == (const int32_t *)s->dataPtr24)
		isSameSample = true;
	else if (v->baseF && s->dataPtrF != NULL && v->baseF == (const float *)s->dataPtrF)
		isSameSample = true;
	if (!isSameSample)
	{
		if (editor->oldSmpPosLine != -1)
			invertSamplePosLine(editor, editor->oldSmpPosLine);
		editor->oldSmpPosLine = -1;
		return;
	}

	int32_t smpPos = (int32_t)v->position;
	if (smpPos < 0 || smpPos >= s->length)
	{
		if (editor->oldSmpPosLine != -1)
			invertSamplePosLine(editor, editor->oldSmpPosLine);
		editor->oldSmpPosLine = -1;
		return;
	}

	int32_t screenX = ft2_sample_smpPos2Scr(inst, smpPos);
	if (screenX < 0 || screenX >= SAMPLE_AREA_WIDTH)
	{
		if (editor->oldSmpPosLine != -1)
			invertSamplePosLine(editor, editor->oldSmpPosLine);
		editor->oldSmpPosLine = -1;
		return;
	}

	if (screenX != editor->oldSmpPosLine)
	{
		if (editor->oldSmpPosLine != -1)
			invertSamplePosLine(editor, editor->oldSmpPosLine);
		invertSamplePosLine(editor, screenX);
		editor->oldSmpPosLine = screenX;
	}
}

void ft2_sample_ed_mark_waveform_dirty(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed)
	{
		ed->waveformDirty = true;
		ed->peakCacheValid = false;
	}
	inst->uiState.updateSampleEditor = true;
}

void ft2_sample_ed_mark_overlay_dirty(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed)
		ed->overlaysDirty = true;
	inst->uiState.updateSampleEditor = true;
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

	/* Check if slicer or effects panel is shown */
	if (inst != NULL && inst->uiState.sampleEditorSlicerShown)
	{
		/* Draw slicer panel */
		drawSlicerScreen(inst, video, bmp);

		/* Draw right side - loop/bit controls and display values */
		drawFramework(video, 353, 346,  74, 54, FRAMEWORK_TYPE1);  /* loop/bit radio buttons */
		drawFramework(video, 427, 346, 205, 54, FRAMEWORK_TYPE1);  /* exit/display/values */

		/* Draw loop/bit labels */
	if (bmp != NULL)
		{
			textOutShadow(video, bmp, 371, 352, PAL_FORGRND, PAL_DSKTOP2, "No loop");
			textOutShadow(video, bmp, 371, 369, PAL_FORGRND, PAL_DSKTOP2, "Forward");
			textOutShadow(video, bmp, 371, 386, PAL_FORGRND, PAL_DSKTOP2, "Pingpong");
		drawSampleBitDepthLabels(inst, video, bmp, getCurrentSampleWithInst(editor, inst));
			textOutShadow(video, bmp, 488, 350, PAL_FORGRND, PAL_DSKTOP2, "Display");
			textOutShadow(video, bmp, 488, 362, PAL_FORGRND, PAL_DSKTOP2, "Length");
			textOutShadow(video, bmp, 488, 375, PAL_FORGRND, PAL_DSKTOP2, "Repeat");
			textOutShadow(video, bmp, 488, 387, PAL_FORGRND, PAL_DSKTOP2, "Replen.");
		}
	}
	else if (inst != NULL && inst->uiState.sampleEditorEffectsShown)
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
		drawSampleBitDepthLabels(inst, video, bmp, getCurrentSampleWithInst(editor, inst));
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
		drawSampleBitDepthLabels(inst, video, bmp, getCurrentSampleWithInst(editor, inst));
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
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	const bool isStereo = sampleIsStereo(s);
	const bool needsWaveform = waveformNeedsRedraw(editor, s, format, isStereo);

	/* Clear two lines in the sample data view that are never written to when the sampler is open */
	hLine(video, 0, 173, SAMPLE_AREA_WIDTH, PAL_BCKGRND);
	hLine(video, 0, 328, SAMPLE_AREA_WIDTH, PAL_BCKGRND);

	bool loopActive = false;
	int32_t loopStart = 0;
	int32_t loopLength = 0;
	if (s != NULL && GET_LOOPTYPE(s->flags) != LOOP_OFF && s->loopLength > 0)
	{
		loopActive = true;
		loopStart = s->loopStart;
		loopLength = s->loopLength;
	}

	const bool loopChanged = (loopActive != editor->oldLoopActive ||
	                          loopStart != editor->oldLoopStart ||
	                          loopLength != editor->oldLoopLength);
	const bool rangeChanged = (editor->hasRange != editor->oldHasRange ||
	                           editor->rangeStart != editor->oldRangeStart ||
	                           editor->rangeEnd != editor->oldRangeEnd);

	bool redrawBase = false;
	if (needsWaveform)
	{
		fillRect(video, 0, SAMPLE_AREA_Y_START, SAMPLE_AREA_WIDTH, SAMPLE_AREA_HEIGHT, PAL_BCKGRND);

		/* Draw waveform */
		ensurePeakCache(editor, s, format, isStereo);
		ft2_sample_ed_draw_waveform(inst);

		if (editor->waveformCache != NULL)
		{
			cacheSampleArea(editor);
			updateWaveformCacheKeys(editor, s, format, isStereo);
		}

		editor->overlaysDirty = true;
		editor->oldSmpPosLine = -1;
		redrawBase = true;
	}
	else if (editor->overlaysDirty || loopChanged)
	{
		blitSampleAreaCache(editor);
		editor->oldSmpPosLine = -1;
		redrawBase = true;
	}

	if (redrawBase)
	{
		if (editor->hasRange)
			xorSampleRange(inst, editor->rangeStart, editor->rangeEnd);

		editor->oldHasRange = editor->hasRange;
		editor->oldRangeStart = editor->rangeStart;
		editor->oldRangeEnd = editor->rangeEnd;

		ft2_sample_ed_draw_loop_points(inst);

		if (inst->uiState.sampleEditorSlicerShown)
			drawSliceMarkers(inst, video, bmp);

		editor->overlaysDirty = false;
		editor->oldLoopActive = loopActive;
		editor->oldLoopStart = loopStart;
		editor->oldLoopLength = loopLength;
	}
	else if (rangeChanged)
	{
		if (editor->oldHasRange)
			xorSampleRange(inst, editor->oldRangeStart, editor->oldRangeEnd);
		if (editor->hasRange)
			xorSampleRange(inst, editor->rangeStart, editor->rangeEnd);

		editor->oldHasRange = editor->hasRange;
		editor->oldRangeStart = editor->rangeStart;
		editor->oldRangeEnd = editor->rangeEnd;

		ft2_sample_ed_draw_loop_points(inst);
		if (inst->uiState.sampleEditorSlicerShown)
			drawSliceMarkers(inst, video, bmp);

		editor->oldLoopActive = loopActive;
		editor->oldLoopStart = loopStart;
		editor->oldLoopLength = loopLength;
	}

	/* Save old values after drawing waveform (for zoom calculations) */
	editor->oldScrPos = editor->scrPos;
	editor->oldViewSize = editor->viewSize;

	/* Draw sample playback position line */
	ft2_sample_ed_draw_pos_line(inst);

	/* Draw hex values for Display/Length/Repeat/Replen */
	if (bmp != NULL && inst != NULL && editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		const int32_t maxSamples = getMaxSamples(inst);
		if (instr != NULL && editor->currSample >= 0 && editor->currSample < maxSamples)
		{
			ft2_sample_t *s = &instr->smp[editor->currSample];
			
			/* Display */
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

/* ------------------------------------------------------------------------- */
/*                            ZOOM CONTROL                                   */
/* ------------------------------------------------------------------------- */

/* Zooms in ~10% towards mouse position */
void ft2_sample_ed_zoom_in(ft2_instance_t *inst, int32_t mouseX)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || editor->oldViewSize <= 2) return;

	int32_t step = (editor->oldViewSize + 5) / 10;
	if (step < 1) step = 1;

	editor->viewSize = editor->oldViewSize - (step * 2);
	if (editor->viewSize < 2)
		editor->viewSize = 2;

	updateScalingFactors(editor);

	/* Offset based on mouse position */
	int32_t tmp32 = (mouseX - (SAMPLE_AREA_WIDTH / 2)) * step;
	tmp32 = (tmp32 + SAMPLE_AREA_WIDTH / 4) / (SAMPLE_AREA_WIDTH / 2);
	step += tmp32;

	int64_t newScrPos64 = editor->oldScrPos + step;
	if (newScrPos64 + editor->viewSize > s->length) newScrPos64 = s->length - editor->viewSize;
	if (newScrPos64 < 0) newScrPos64 = 0;
	editor->scrPos = (int32_t)newScrPos64;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
	}
}

/* Zooms out ~10% from mouse position */
void ft2_sample_ed_zoom_out(ft2_instance_t *inst, int32_t mouseX)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || editor->oldViewSize == s->length) return;

	int32_t step = (editor->oldViewSize + 5) / 10;
	if (step < 1) step = 1;

	int64_t newViewSize64 = (int64_t)editor->oldViewSize + (step * 2);
	if (newViewSize64 > s->length)
	{
		editor->viewSize = s->length;
		editor->scrPos = 0;
	}
	else
	{
		int32_t tmp32 = (mouseX - (SAMPLE_AREA_WIDTH / 2)) * step;
		tmp32 = (tmp32 + SAMPLE_AREA_WIDTH / 4) / (SAMPLE_AREA_WIDTH / 2);
		step += tmp32;

		editor->viewSize = (int32_t)(newViewSize64 & 0xFFFFFFFF);
		editor->scrPos = editor->oldScrPos - step;
		if (editor->scrPos < 0) editor->scrPos = 0;
		if (editor->scrPos + editor->viewSize > s->length)
			editor->scrPos = s->length - editor->viewSize;
	}

	updateScalingFactors(editor);

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
	}
}

/* Scrolls sample view left by 1/32 of view size */
void ft2_sample_ed_scroll_left(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	int32_t smpLen = s ? s->length : 0;

	if (editor->viewSize == 0 || editor->viewSize >= smpLen)
		return;

	int32_t scrollAmount = editor->viewSize / 32;
	if (scrollAmount < 1) scrollAmount = 1;

	editor->scrPos -= scrollAmount;
	if (editor->scrPos < 0) editor->scrPos = 0;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;

	updateScalingFactors(editor);
}

/* Scrolls sample view right by 1/32 of view size */
void ft2_sample_ed_scroll_right(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	int32_t smpLen = s ? s->length : 0;

	if (editor->viewSize == 0 || editor->viewSize >= smpLen)
		return;

	int32_t scrollAmount = editor->viewSize / 32;
	if (scrollAmount < 1) scrollAmount = 1;

	editor->scrPos += scrollAmount;
	if (editor->scrPos + editor->viewSize > smpLen)
		editor->scrPos = smpLen - editor->viewSize;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;

	updateScalingFactors(editor);
}

/* ------------------------------------------------------------------------- */
/*                           VIEW PRESETS                                    */
/* ------------------------------------------------------------------------- */

void ft2_sample_ed_show_all(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	editor->scrPos = 0;
	int32_t smpLen = 0;
	if (editor->currInstr > 0 && editor->currInstr < 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
		const int32_t maxSamples = getMaxSamples(inst);
		if (instr && editor->currSample >= 0 && editor->currSample < maxSamples)
			smpLen = instr->smp[editor->currSample].length;
	}
	editor->viewSize = smpLen;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
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
	if (!editor || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || editor->currSample < 0 || editor->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (GET_LOOPTYPE(s->flags) == LOOP_OFF || s->loopLength <= 0) return;

	editor->scrPos = s->loopStart;
	editor->viewSize = s->loopLength;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets)
	{
		widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
		widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
	}
	updateScalingFactors(editor);
}

void ft2_sample_ed_show_range(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || editor->currSample < 0 || editor->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format)) return;

	if (editor->rangeStart < editor->rangeEnd)
	{
		editor->viewSize = editor->rangeEnd - editor->rangeStart;
		if (editor->viewSize < 2) editor->viewSize = 2;
		editor->scrPos = editor->rangeStart;

		ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
		if (widgets)
		{
			widgets->scrollBarState[SB_SAMP_SCROLL].page = (uint32_t)editor->viewSize;
			widgets->scrollBarState[SB_SAMP_SCROLL].pos = (uint32_t)editor->scrPos;
		}
		updateScalingFactors(editor);
		inst->uiState.updateSampleEditor = true;
	}
	else
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_message(&ui->dialog, "System message", "Cannot show empty range!");
	}
}

/* ------------------------------------------------------------------------- */
/*                          SELECTION CONTROL                                */
/* ------------------------------------------------------------------------- */

void ft2_sample_ed_set_selection(ft2_instance_t *inst, int32_t start, int32_t end)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor) return;
	editor->rangeStart = start;
	editor->rangeEnd = end;
	editor->hasRange = (end > 0);
}

void ft2_sample_ed_clear_selection(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor) return;
	editor->rangeStart = editor->rangeEnd = 0;
	editor->hasRange = false;
}

void ft2_sample_ed_range_all(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return;

	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || editor->currSample < 0 || editor->currSample >= maxSamples) return;

	ft2_sample_t *s = &instr->smp[editor->currSample];
	if (s->length > 0)
	{
		editor->rangeStart = 0;
		editor->rangeEnd = s->length;
		editor->hasRange = true;
	}
}

/* ------------------------------------------------------------------------- */
/*                          LOOP PIN DRAGGING                                */
/* ------------------------------------------------------------------------- */

static int32_t getLeftLoopPinScreenX(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (!s || s->loopLength <= 0) return -100;
	return ft2_sample_smpPos2Scr(inst, s->loopStart) - 8;
}

static int32_t getRightLoopPinScreenX(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	if (!s || s->loopLength <= 0) return -100;
	return ft2_sample_smpPos2Scr(inst, s->loopStart + s->loopLength) - 8;
}

static void setLeftLoopPinPos(ft2_sample_editor_t *editor, int32_t x, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	int32_t loopEnd = s->loopStart + s->loopLength;
	int32_t newLoopStart = ft2_sample_scr2SmpPos(inst, x);
	if (newLoopStart < 0) newLoopStart = 0;
	if (newLoopStart >= loopEnd) newLoopStart = loopEnd - 1;
	if (newLoopStart < 0) newLoopStart = 0;

	ft2_stop_sample_voices(inst, s);
	s->loopStart = newLoopStart;
	s->loopLength = loopEnd - newLoopStart;
	if (s->loopLength < 0) s->loopLength = 0;
	ft2_sample_ed_mark_overlay_dirty(inst);
}

static void setRightLoopPinPos(ft2_sample_editor_t *editor, int32_t x, ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	int32_t loopEnd = ft2_sample_scr2SmpPos(inst, x);
	if (loopEnd < s->loopStart) loopEnd = s->loopStart;
	if (loopEnd > s->length) loopEnd = s->length;

	ft2_stop_sample_voices(inst, s);
	s->loopLength = loopEnd - s->loopStart;
	if (s->loopLength < 0) s->loopLength = 0;
	ft2_sample_ed_mark_overlay_dirty(inst);
}

/* ------------------------------------------------------------------------- */
/*                       SAMPLE DRAW MODE                                    */
/* ------------------------------------------------------------------------- */

/* Right-click drawing: interpolated line drawing into sample data */
static void editSampleData(ft2_sample_editor_t *editor, int32_t mx, int32_t my, bool mouseButtonHeld, bool shiftPressed, ft2_instance_t *inst)
{
	if (!editor || !inst) return;
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	bool isStereo = sampleIsStereo(s);
	bool rightChannel = false;
	int32_t channelYStart = SAMPLE_AREA_Y_START;
	int32_t channelYHeight = SAMPLE_AREA_HEIGHT;
	if (isStereo)
	{
		rightChannel = (my >= SAMPLE_AREA_R_START && my < SAMPLE_AREA_R_START + SAMPLE_AREA_R_HEIGHT);
		channelYStart = rightChannel ? SAMPLE_AREA_R_START : SAMPLE_AREA_L_START;
		channelYHeight = rightChannel ? SAMPLE_AREA_R_HEIGHT : SAMPLE_AREA_L_HEIGHT;
	}

	int32_t tmp32, p, vl, r, rl, rvl, start, end;

	if (mx > SCREEN_W)
		mx = SCREEN_W;

	if (!mouseButtonHeld)
	{
		ft2_unfix_sample(s);
		inst->editor.editSampleFlag = true;

		editor->lastDrawX = ft2_sample_scr2SmpPos(inst, mx);
		editor->lastDrawY = isStereo ? mouseYToSampleYChannel(my, channelYStart, channelYHeight)
		                             : mouseYToSampleY(my);

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
		vl = isStereo ? mouseYToSampleYChannel(my, channelYStart, channelYHeight)
		              : mouseYToSampleY(my);
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

	start = p;
	if (start < 0)
		start = 0;

	end = editor->lastDrawX + 1;
	if (end > s->length)
		end = s->length;

	const float startVal = ((float)vl - 128.0f) * (1.0f / 128.0f);
	const float endVal = ((float)editor->lastDrawY - 128.0f) * (1.0f / 128.0f);
	const int32_t x = editor->lastDrawX - p;

	if (p == editor->lastDrawX)
	{
		for (rl = start; rl < end; rl++)
			sampleSetFloat(s, format, rightChannel, rl, startVal);
	}
	else if (x != 0)
	{
		const float invX = 1.0f / (float)x;
		for (rl = start; rl < end; rl++)
		{
			const int32_t i = rl - p;
			const float t = (float)i * invX;
			const float v = startVal + ((endVal - startVal) * t);
			sampleSetFloat(s, format, rightChannel, rl, v);
		}
	}

	editor->lastDrawY = rvl;
	editor->lastDrawX = r;

	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* ------------------------------------------------------------------------- */
/*                          MOUSE HANDLING                                   */
/* ------------------------------------------------------------------------- */

void ft2_sample_ed_mouse_click(ft2_instance_t *inst, int x, int y, int button)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	int32_t mx = CLAMP(x, 0, SCREEN_W + 8);
	int32_t my = CLAMP(y, 0, SCREEN_H - 1);

	inst->uiState.leftLoopPinMoving = false;
	inst->uiState.rightLoopPinMoving = false;
	inst->uiState.sampleDataOrLoopDrag = -1;
	editor->mouseXOffs = 0;
	editor->lastMouseX = mx;
	editor->lastMouseY = my;

	if (button == 1) /* Left button */
	{
		/* Slicer mode - check for marker drag or slice preview */
		if (inst->uiState.sampleEditorSlicerShown && my >= SAMPLE_AREA_Y_START && my < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT)
		{
			/* Check if clicking on a marker */
			int8_t markerIdx = slicerHitTestMarker(inst, mx);
			if (markerIdx >= 0)
			{
				slicerStartMarkerDrag(inst, markerIdx);
				inst->uiState.sampleDataOrLoopDrag = 2;  /* 2 = slicer marker drag */
				editor->lastMouseX = mx;
				return;
			}

			/* Not on a marker - preview the slice under cursor */
			slicerPreviewSlice(inst, mx);
			return;
		}

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
				ft2_sample_ed_mark_overlay_dirty(inst);
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
				ft2_sample_ed_mark_overlay_dirty(inst);
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
	if (!editor || !inst) return;

	int32_t mx = CLAMP(x, 0, SCREEN_W + 8);
	int32_t my = CLAMP(y, 0, SCREEN_H - 1);

	if (inst->editor.editSampleFlag)
	{
		editSampleData(editor, mx, my, true, shiftPressed, inst);
		return;
	}

	if (mx == editor->lastMouseX) return;

	/* Slicer marker dragging */
	if (inst->uiState.sampleDataOrLoopDrag == 2 && inst->uiState.sampleEditorSlicerShown)
	{
		editor->lastMouseX = mx;
		slicerDragMarker(inst, mx);
		return;
	}

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

		editor->hasRange = true;
		inst->uiState.updateSampleEditor = true;
	}
}

void ft2_sample_ed_mouse_up(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	if (inst->editor.editSampleFlag)
	{
		ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
		if (s) ft2_fix_sample(s);
		inst->editor.editSampleFlag = false;
		ft2_sample_ed_mark_waveform_dirty(inst);
	}

	if (inst->uiState.leftLoopPinMoving || inst->uiState.rightLoopPinMoving)
	{
		inst->uiState.leftLoopPinMoving = false;
		inst->uiState.rightLoopPinMoving = false;
		ft2_sample_ed_mark_overlay_dirty(inst);
	}

	/* End slicer marker dragging */
	if (inst->uiState.sampleDataOrLoopDrag == 2 && inst->uiState.sampleEditorSlicerShown)
		slicerEndMarkerDrag(inst);

	inst->uiState.sampleDataOrLoopDrag = -1;
}

/* ------------------------------------------------------------------------- */
/*                       CLIPBOARD OPERATIONS                                */
/* ------------------------------------------------------------------------- */

static ft2_sample_t *getCurrentSampleWithInst(ft2_sample_editor_t *editor, ft2_instance_t *inst)
{
	if (!editor || !inst) return NULL;
	if (editor->currInstr <= 0 || editor->currInstr >= 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[editor->currInstr];
	const int32_t maxSamples = getMaxSamples(inst);
	if (!instr || editor->currSample < 0 || editor->currSample >= maxSamples) return NULL;
	return &instr->smp[editor->currSample];
}

void ft2_sample_ed_cut(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || editor->rangeEnd == 0 || editor->rangeStart == editor->rangeEnd)
		return;

	if (inst && inst->config.smpCutToBuffer)
		ft2_sample_ed_copy(inst);
	ft2_sample_ed_delete(inst);
}

void ft2_sample_ed_copy(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	smp_clipboard_t *clip = &editor->clipboard;
	int32_t start, end;
	if (!editor->hasRange || editor->rangeEnd == 0)
	{
		start = 0; end = s->length;
	}
	else
	{
		start = editor->rangeStart; end = editor->rangeEnd;
		if (start > end) { int32_t tmp = start; start = end; end = tmp; }
	}
	if (start < 0) start = 0;
	if (end > s->length) end = s->length;

	int32_t len = end - start;
	if (len <= 0) return;

	/* Free existing clipboard data */
	if (clip->data) { free(clip->data); clip->data = NULL; }
	if (clip->dataR) { free(clip->dataR); clip->dataR = NULL; }

	int32_t bytesPerSample = (int32_t)ft2_sample_format_bytes(format);
	bool isStereo = sampleIsStereo(s);
	uint8_t *srcL = (uint8_t *)sampleGetDataPtr(s, format, false);
	uint8_t *srcR = isStereo ? (uint8_t *)sampleGetDataPtr(s, format, true) : NULL;

	clip->length = len;
	clip->format = format;
	clip->bytesPerSample = (uint32_t)bytesPerSample;
	clip->is16Bit = (format == FT2_SMP_FORMAT_16BIT);
	clip->isStereo = isStereo;

	/* Copy left channel */
	clip->data = (int8_t *)malloc((size_t)(len * bytesPerSample));
	if (clip->data)
		memcpy(clip->data, srcL + (start * bytesPerSample), (size_t)(len * bytesPerSample));

	/* Copy right channel for stereo */
	if (isStereo)
	{
		clip->dataR = (int8_t *)malloc((size_t)(len * bytesPerSample));
		if (clip->dataR)
			memcpy(clip->dataR, srcR + (start * bytesPerSample), (size_t)(len * bytesPerSample));
	}

	if (start == 0 && end == s->length)
	{
		clip->sampleInfo = *s;
		clip->hasInfo = true;
		clip->didCopyWholeSample = true;
	}
	else
	{
		clip->hasInfo = false;
		clip->didCopyWholeSample = false;
	}
}

static float clipboardGetFloat(const smp_clipboard_t *clip, bool rightChannel, int32_t pos)
{
	if (clip == NULL)
		return 0.0f;

	const int8_t *src = rightChannel ? clip->dataR : clip->data;
	if (src == NULL && rightChannel)
		src = clip->data;
	if (src == NULL)
		return 0.0f;

	switch (clip->format)
	{
		default:
		case FT2_SMP_FORMAT_8BIT:
			return src[pos] * (1.0f / 128.0f);
		case FT2_SMP_FORMAT_16BIT:
			return ((const int16_t *)src)[pos] * (1.0f / 32768.0f);
		case FT2_SMP_FORMAT_24BIT:
			return ((const int32_t *)src)[pos] * (1.0f / 8388608.0f);
		case FT2_SMP_FORMAT_FLOAT32:
			return ((const float *)src)[pos];
	}
}

/* Pastes clipboard data with automatic format conversion */
static void pasteCopiedData(const smp_clipboard_t *clip, ft2_sample_t *dest,
                            ft2_sample_format_t destFormat, bool rightChannel,
                            int32_t offset, int32_t length)
{
	if (clip == NULL || dest == NULL || clip->data == NULL || length <= 0)
		return;

	void *destPtr = sampleGetDataPtr(dest, destFormat, rightChannel);
	if (destPtr == NULL)
		return;

	if (clip->format == destFormat)
	{
		const uint32_t bytesPerSample = ft2_sample_format_bytes(destFormat);
		memcpy((uint8_t *)destPtr + (offset * bytesPerSample),
		       rightChannel ? clip->dataR : clip->data,
		       (size_t)length * bytesPerSample);
		return;
	}

	for (int32_t i = 0; i < length; i++)
	{
		float v = clipboardGetFloat(clip, rightChannel, i);
		sampleSetFloat(dest, destFormat, rightChannel, offset + i, v);
	}
}

/* Replaces entire sample with clipboard content */
static void pasteOverwrite(ft2_sample_editor_t *editor, ft2_sample_t *s, ft2_instance_t *inst)
{
	smp_clipboard_t *clip = &editor->clipboard;
	int32_t bytesPerSample = (int32_t)ft2_sample_format_bytes(clip->format);
	
	/* Free old sample data */
	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->dataPtr = NULL;
		s->origDataPtr = NULL;
	}
	if (s->origDataPtr24 != NULL)
	{
		free(s->origDataPtr24);
		s->dataPtr24 = NULL;
		s->origDataPtr24 = NULL;
	}
	if (s->origDataPtrF != NULL)
	{
		free(s->origDataPtrF);
		s->dataPtrF = NULL;
		s->origDataPtrF = NULL;
	}
	if (s->origDataPtrR != NULL)
	{
		free(s->origDataPtrR);
		s->dataPtrR = NULL;
		s->origDataPtrR = NULL;
	}
	if (s->origDataPtr24R != NULL)
	{
		free(s->origDataPtr24R);
		s->dataPtr24R = NULL;
		s->origDataPtr24R = NULL;
	}
	if (s->origDataPtrFR != NULL)
	{
		free(s->origDataPtrFR);
		s->dataPtrFR = NULL;
		s->origDataPtrFR = NULL;
	}
	
	/* Allocate new buffer with extra space for interpolation */
	size_t allocSize = (size_t)(clip->length * bytesPerSample) + 256;
	if (clip->format == FT2_SMP_FORMAT_24BIT)
	{
		s->origDataPtr24 = (int32_t *)malloc(allocSize);
		if (s->origDataPtr24 == NULL)
			return;
		memset(s->origDataPtr24, 0, allocSize);
		s->dataPtr24 = (int32_t *)((uint8_t *)s->origDataPtr24 + 128);
		memcpy(s->dataPtr24, clip->data, (size_t)(clip->length * bytesPerSample));
	}
	else if (clip->format == FT2_SMP_FORMAT_FLOAT32)
	{
		s->origDataPtrF = (float *)malloc(allocSize);
		if (s->origDataPtrF == NULL)
			return;
		memset(s->origDataPtrF, 0, allocSize);
		s->dataPtrF = (float *)((uint8_t *)s->origDataPtrF + 128);
		memcpy(s->dataPtrF, clip->data, (size_t)(clip->length * bytesPerSample));
	}
	else
	{
		s->origDataPtr = (int8_t *)malloc(allocSize);
		if (s->origDataPtr == NULL)
			return;
		memset(s->origDataPtr, 0, allocSize);
		s->dataPtr = s->origDataPtr + 128;
		memcpy(s->dataPtr, clip->data, (size_t)(clip->length * bytesPerSample));
	}

	/* Copy right channel for stereo */
	if (clip->isStereo && clip->dataR != NULL)
	{
		if (clip->format == FT2_SMP_FORMAT_24BIT)
		{
			s->origDataPtr24R = (int32_t *)malloc(allocSize);
			if (s->origDataPtr24R != NULL)
			{
				memset(s->origDataPtr24R, 0, allocSize);
				s->dataPtr24R = (int32_t *)((uint8_t *)s->origDataPtr24R + 128);
				memcpy(s->dataPtr24R, clip->dataR, (size_t)(clip->length * bytesPerSample));
			}
		}
		else if (clip->format == FT2_SMP_FORMAT_FLOAT32)
		{
			s->origDataPtrFR = (float *)malloc(allocSize);
			if (s->origDataPtrFR != NULL)
			{
				memset(s->origDataPtrFR, 0, allocSize);
				s->dataPtrFR = (float *)((uint8_t *)s->origDataPtrFR + 128);
				memcpy(s->dataPtrFR, clip->dataR, (size_t)(clip->length * bytesPerSample));
			}
		}
		else
		{
			s->origDataPtrR = (int8_t *)malloc(allocSize);
			if (s->origDataPtrR != NULL)
			{
				memset(s->origDataPtrR, 0, allocSize);
				s->dataPtrR = s->origDataPtrR + 128;
				memcpy(s->dataPtrR, clip->dataR, (size_t)(clip->length * bytesPerSample));
			}
		}
	}
	
	/* Restore sample info if we copied a whole sample */
	if (clip->didCopyWholeSample && clip->hasInfo)
	{
		memcpy(s->name, clip->sampleInfo.name, 22);
		s->length = clip->sampleInfo.length;
		s->loopStart = clip->sampleInfo.loopStart;
		s->loopLength = clip->sampleInfo.loopLength;
		s->volume = clip->sampleInfo.volume;
		s->panning = clip->sampleInfo.panning;
		s->finetune = clip->sampleInfo.finetune;
		s->relativeNote = clip->sampleInfo.relativeNote;
		s->flags = clip->sampleInfo.flags;
		s->format = clip->sampleInfo.format;
		s->sampleRate = clip->sampleInfo.sampleRate;
	}
	else
	{
		s->name[0] = '\0';
		s->length = clip->length;
		s->loopStart = 0;
		s->loopLength = 0;
		s->volume = 64;
		s->panning = 128;
		s->finetune = 0;
		s->relativeNote = 0;
		s->flags = (clip->format == FT2_SMP_FORMAT_16BIT) ? SAMPLE_16BIT : 0;
		if (clip->isStereo)
			s->flags |= FT2_SAMPLE_STEREO;
		s->format = clip->format;
		s->sampleRate = 0;
	}
	
	/* Set range to pasted area (the whole sample in overwrite case) */
	editor->rangeStart = 0;
	editor->rangeEnd = s->length;
	editor->hasRange = true;
	ft2_sample_ed_show_all(inst);
	
	if (inst != NULL)
	{
		inst->replayer.song.isModified = true;
		ft2_xm2_update_usage(inst);
	}
}

/* Inserts clipboard at selection, or overwrites if no sample/selection */
void ft2_sample_ed_paste(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	smp_clipboard_t *clip = &editor->clipboard;
	if (!editor || !clip->data || clip->length <= 0) return;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0 || editor->rangeEnd == 0)
	{
		if (s) pasteOverwrite(editor, s, inst);
		return;
	}
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
	
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	
	/* Calculate new length */
	int32_t newLength = s->length + clip->length - (rx2 - rx1);
	if (newLength <= 0)
		return;
	
	/* Check max sample length */
	if (newLength > FT2_MAX_SAMPLE_LEN)
		return;
	
	/* Allocate new buffer */
	size_t allocSize = (size_t)(newLength * bytesPerSample) + 256;

	void *newOrigPtr = malloc(allocSize);
	if (newOrigPtr == NULL)
		return;

	memset(newOrigPtr, 0, allocSize);
	uint8_t *newDataPtr = (uint8_t *)newOrigPtr + 128;

	void *newOrigPtrR = NULL;
	uint8_t *newDataPtrR = NULL;
	const bool isStereo = sampleIsStereo(s);
	if (isStereo)
	{
		newOrigPtrR = malloc(allocSize);
		if (newOrigPtrR == NULL)
		{
			free(newOrigPtr);
			return;
		}
		memset(newOrigPtrR, 0, allocSize);
		newDataPtrR = (uint8_t *)newOrigPtrR + 128;
	}
	
	ft2_unfix_sample(s);
	
	/* Copy left part of original sample (before selection) */
	if (rx1 > 0)
	{
		uint8_t *srcL = (uint8_t *)sampleGetDataPtr(s, format, false);
		memcpy(newDataPtr, srcL, (size_t)(rx1 * bytesPerSample));
		if (isStereo)
		{
			uint8_t *srcR = (uint8_t *)sampleGetDataPtr(s, format, true);
			memcpy(newDataPtrR, srcR, (size_t)(rx1 * bytesPerSample));
		}
	}
	
	/* Paste clipboard data at rx1 position */
	{
		ft2_sample_t tmp = *s;
		tmp.format = format;
		tmp.dataPtr = (int8_t *)newDataPtr;
		tmp.dataPtrR = (int8_t *)newDataPtrR;
		tmp.dataPtr24 = (int32_t *)newDataPtr;
		tmp.dataPtr24R = (int32_t *)newDataPtrR;
		tmp.dataPtrF = (float *)newDataPtr;
		tmp.dataPtrFR = (float *)newDataPtrR;

		pasteCopiedData(clip, &tmp, format, false, rx1, clip->length);
		if (isStereo)
			pasteCopiedData(clip, &tmp, format, true, rx1, clip->length);
	}
	
	/* Copy right part of original sample (after selection) */
	if (rx2 < s->length)
	{
		uint8_t *srcL = (uint8_t *)sampleGetDataPtr(s, format, false);
		memmove(&newDataPtr[(rx1 + clip->length) * bytesPerSample],
		        &srcL[rx2 * bytesPerSample],
		        (size_t)((s->length - rx2) * bytesPerSample));
		if (isStereo)
		{
			uint8_t *srcR = (uint8_t *)sampleGetDataPtr(s, format, true);
			memmove(&newDataPtrR[(rx1 + clip->length) * bytesPerSample],
			        &srcR[rx2 * bytesPerSample],
			        (size_t)((s->length - rx2) * bytesPerSample));
		}
	}
	
	/* Free old sample data and assign new */
	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = NULL; }
	if (s->origDataPtr24) { free(s->origDataPtr24); s->origDataPtr24 = NULL; }
	if (s->origDataPtrF) { free(s->origDataPtrF); s->origDataPtrF = NULL; }
	if (s->origDataPtrR) { free(s->origDataPtrR); s->origDataPtrR = NULL; }
	if (s->origDataPtr24R) { free(s->origDataPtr24R); s->origDataPtr24R = NULL; }
	if (s->origDataPtrFR) { free(s->origDataPtrFR); s->origDataPtrFR = NULL; }

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		s->origDataPtr24 = (int32_t *)newOrigPtr;
		s->dataPtr24 = (int32_t *)newDataPtr;
		if (isStereo)
		{
			s->origDataPtr24R = (int32_t *)newOrigPtrR;
			s->dataPtr24R = (int32_t *)newDataPtrR;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		s->origDataPtrF = (float *)newOrigPtr;
		s->dataPtrF = (float *)newDataPtr;
		if (isStereo)
		{
			s->origDataPtrFR = (float *)newOrigPtrR;
			s->dataPtrFR = (float *)newDataPtrR;
		}
	}
	else
	{
		s->origDataPtr = (int8_t *)newOrigPtr;
		s->dataPtr = (int8_t *)newDataPtr;
		if (isStereo)
		{
			s->origDataPtrR = (int8_t *)newOrigPtrR;
			s->dataPtrR = (int8_t *)newDataPtrR;
		}
	}
	
	/* Adjust loop points if necessary */
	if (rx2 - rx1 != clip->length)
	{
		int32_t loopAdjust = clip->length - (rx2 - rx1);
		
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
	editor->rangeEnd = rx1 + clip->length;
	editor->hasRange = true;
	
	ft2_sample_ed_show_all(inst);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_delete(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format)) return;
	if (editor->rangeEnd == 0 || editor->rangeStart == editor->rangeEnd) return;
	
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
	
	int32_t bytesPerSample = (int32_t)ft2_sample_format_bytes(format);
	int32_t newLen = s->length - delLen;
	const bool isStereo = sampleIsStereo(s);
	uint8_t *dataPtr = (uint8_t *)sampleGetDataPtr(s, format, false);
	uint8_t *dataPtrR = isStereo ? (uint8_t *)sampleGetDataPtr(s, format, true) : NULL;
	
	ft2_unfix_sample(s);
	
	/* Move data after selection to fill the gap */
	memmove(dataPtr + (start * bytesPerSample),
	        dataPtr + (end * bytesPerSample),
	        (size_t)((s->length - end) * bytesPerSample));

	if (isStereo)
	{
		memmove(dataPtrR + (start * bytesPerSample),
		        dataPtrR + (end * bytesPerSample),
		        (size_t)((s->length - end) * bytesPerSample));
	}
	
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

static void reverseSampleRange(void *dataPtr, int32_t start, int32_t end, ft2_sample_format_t format)
{
	if (dataPtr == NULL || end - start < 2)
		return;

	int32_t i = start;
	int32_t j = end - 1;

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		int16_t *ptr = (int16_t *)dataPtr;
		while (i < j)
		{
			int16_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
			i++;
			j--;
		}
	}
	else if (format == FT2_SMP_FORMAT_24BIT)
	{
		int32_t *ptr = (int32_t *)dataPtr;
		while (i < j)
		{
			int32_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
			i++;
			j--;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		float *ptr = (float *)dataPtr;
		while (i < j)
		{
			float tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
			i++;
			j--;
		}
	}
	else
	{
		int8_t *ptr = dataPtr;
		while (i < j)
		{
			int8_t tmp = ptr[i];
			ptr[i] = ptr[j];
			ptr[j] = tmp;
			i++;
			j--;
		}
	}
}

static float getSamplePeakRange(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel,
                                int32_t start, int32_t end)
{
	if (s == NULL || end <= start)
		return 0.0f;

	float peak = 0.0f;
	for (int32_t i = start; i < end; i++)
	{
		float v = fabsf(sampleGetFloat(s, format, rightChannel, i));
		if (v > peak) peak = v;
	}
	return peak;
}

static void scaleSampleRangeLinear(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel,
                                   int32_t start, int32_t len, double startFactor, double endFactor)
{
	if (s == NULL || len <= 0)
		return;

	double dVol = startFactor;
	const bool mustInterpolate = (startFactor != endFactor);
	const double dVolDelta = mustInterpolate ? ((endFactor - startFactor) / len) : 0.0;

	for (int32_t i = 0; i < len; i++)
	{
		float v = sampleGetFloat(s, format, rightChannel, start + i);
		sampleSetFloat(s, format, rightChannel, start + i, (float)(v * dVol));
		if (mustInterpolate) dVol += dVolDelta;
	}
}

static void removeDCOffsetRange(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel,
                                int32_t start, int32_t length)
{
	if (s == NULL || length <= 0)
		return;

	double sum = 0.0;
	for (int32_t i = 0; i < length; i++)
		sum += sampleGetFloat(s, format, rightChannel, start + i);

	const double avg = sum / (double)length;
	if (avg == 0.0)
		return;

	for (int32_t i = 0; i < length; i++)
	{
		float v = sampleGetFloat(s, format, rightChannel, start + i);
		sampleSetFloat(s, format, rightChannel, start + i, (float)(v - avg));
	}
}

/* ------------------------------------------------------------------------- */
/*                       SAMPLE PROCESSING                                   */
/* ------------------------------------------------------------------------- */

void ft2_sample_ed_reverse(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format)) return;

	int32_t start = editor->hasRange ? editor->rangeStart : 0;
	int32_t end = editor->hasRange ? editor->rangeEnd : s->length;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }
	
	void *dataPtr = sampleGetDataPtr(s, format, false);
	reverseSampleRange(dataPtr, start, end, format);
	if (sampleIsStereo(s))
	{
		void *dataPtrR = sampleGetDataPtr(s, format, true);
		reverseSampleRange(dataPtrR, start, end, format);
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

/* Scales sample to full volume range */
void ft2_sample_ed_normalize(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	int32_t start = editor->hasRange ? editor->rangeStart : 0;
	int32_t end = editor->hasRange ? editor->rangeEnd : s->length;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }

	const int32_t len = end - start;
	float peak = getSamplePeakRange(s, format, false, start, end);
	if (sampleIsStereo(s))
	{
		float peakR = getSamplePeakRange(s, format, true, start, end);
		if (peakR > peak)
			peak = peakR;
	}

	if (peak > 0.0f)
	{
		if (peak < 1.0f)
		{
			const double factor = 1.0 / (double)peak;
			scaleSampleRangeLinear(s, format, false, start, len, factor, factor);
			if (sampleIsStereo(s))
				scaleSampleRangeLinear(s, format, true, start, len, factor, factor);
		}
	}
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_fade_in(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || !editor->hasRange) return;

	int32_t start = editor->rangeStart, end = editor->rangeEnd;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }
	int32_t len = end - start;
	if (len <= 0) return;

	scaleSampleRangeLinear(s, format, false, start, len, 0.0, 1.0);
	if (sampleIsStereo(s))
		scaleSampleRangeLinear(s, format, true, start, len, 0.0, 1.0);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

void ft2_sample_ed_fade_out(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || !editor->hasRange) return;

	int32_t start = editor->rangeStart, end = editor->rangeEnd;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }
	int32_t len = end - start;
	if (len <= 0) return;

	scaleSampleRangeLinear(s, format, false, start, len, 1.0, 0.0);
	if (sampleIsStereo(s))
		scaleSampleRangeLinear(s, format, true, start, len, 1.0, 0.0);
	
	if (inst != NULL)
		inst->replayer.song.isModified = true;
}

/* Sample value accessors for crossfade algorithm */
static double getSampleValuePlugin(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t position)
{
	return (double)sampleGetFloat(s, format, rightChannel, position);
}

static void putSampleValuePlugin(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel,
                                 int32_t position, double sample)
{
	sampleSetFloat(s, format, rightChannel, position, (float)sample);
}

static void crossfadeLoopChannel(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel,
                                 int32_t length, int32_t y1, int32_t y2, int32_t loopStart,
                                 double dR, double dD1, double dD1Mul, double dD2Mul, double dD3Mul)
{
	if (s == NULL || length <= 0)
		return;

	for (int32_t i = 0; i < length; i++)
	{
		int32_t aIdx = y1 + i;
		int32_t bIdx = y2 + i;
		double dI = i;

		double dA = getSampleValuePlugin(s, format, rightChannel, aIdx);
		double dB = getSampleValuePlugin(s, format, rightChannel, bIdx);
		double dS2 = dI * dD1Mul;
		double dS1 = 1.0 - dS2;

		double dC, dD;
		if (y1 + i < loopStart)
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

		putSampleValuePlugin(s, format, rightChannel, aIdx, dC);
		putSampleValuePlugin(s, format, rightChannel, bIdx, dD);
	}
}

/* Crossfades loop boundary to eliminate click. Uses selection as fade zone. */
void ft2_sample_ed_crossfade_loop(ft2_instance_t *inst)
{
	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	if (!editor || !inst) return;

	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	uint8_t loopType = GET_LOOPTYPE(s->flags);
	if (loopType == LOOP_OFF) return;
	if (!editor->hasRange || editor->rangeEnd <= editor->rangeStart) return;

	int32_t x1 = editor->rangeStart;
	int32_t x2 = editor->rangeEnd;
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

		crossfadeLoopChannel(s, format, false, length, y1, y2, s->loopStart,
		                     dR, dD1, dD1Mul, dD2Mul, dD3Mul);
		if (sampleIsStereo(s))
		{
			crossfadeLoopChannel(s, format, true, length, y1, y2, s->loopStart,
			                     dR, dD1, dD1Mul, dD2Mul, dD3Mul);
		}

		ft2_fix_sample(s);
		inst->replayer.song.isModified = true;
	}

	inst->uiState.updateSampleEditor = true;
}

/* ------------------------------------------------------------------------- */
/*                           VISIBILITY                                      */
/* ------------------------------------------------------------------------- */

void showSampleEditor(ft2_instance_t *inst)
{
	if (!inst) return;

	if (inst->uiState.instEditorShown) hideInstEditor(inst);
	hidePatternEditor(inst);
	if (inst->uiState.instEditorExtShown) hideInstEditorExt(inst);

	inst->uiState.sampleEditorShown = true;
	ft2_sample_ed_mark_overlay_dirty(inst);

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

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
	widgets->pushButtonVisible[PB_SAMP_SLICER] = true;
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
	widgets->radioButtonVisible[RB_SAMPLE_8BIT] = !inst->config.enableXm2Features;
	widgets->radioButtonVisible[RB_SAMPLE_16BIT] = !inst->config.enableXm2Features;
	widgets->pushButtonVisible[PB_SAMP_FORMAT] = inst->config.enableXm2Features;
	widgets->pushButtonVisible[PB_SAMP_CHNS] = inst->config.enableXm2Features;

	/* Show scrollbar */
	widgets->scrollBarState[SB_SAMP_SCROLL].visible = true;
}

void hideSampleEditor(ft2_instance_t *inst)
{
	if (!inst) return;

	inst->uiState.sampleEditorShown = false;
	inst->uiState.leftLoopPinMoving = false;
	inst->uiState.rightLoopPinMoving = false;

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

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
	widgets->pushButtonVisible[PB_SAMP_SLICER] = false;
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
	widgets->pushButtonVisible[PB_SAMP_FORMAT] = false;
	widgets->pushButtonVisible[PB_SAMP_CHNS] = false;

	/* Hide scrollbar */
	widgets->scrollBarState[SB_SAMP_SCROLL].visible = false;

	/* Also hide extended sample editor buttons */
	hideSampleEditorExtButtons(inst);
	inst->uiState.sampleEditorExtShown = false;

	/* Hide sample effects screen if open */
	if (inst->uiState.sampleEditorEffectsShown)
		hideSampleEffectsScreen(inst);

	/* Hide slicer screen if open */
	if (inst->uiState.sampleEditorSlicerShown)
		hideSlicerScreen(inst);
}

void toggleSampleEditor(ft2_instance_t *inst)
{
	if (!inst) return;
	if (inst->uiState.sampleEditorShown)
	{
		hideSampleEditor(inst);
		inst->uiState.patternEditorShown = true;
	}
	else
	{
		showSampleEditor(inst);
	}
}

void exitSampleEditor(ft2_instance_t *inst)
{
	if (!inst) return;
	hideSampleEditor(inst);
	inst->uiState.patternEditorShown = true;
}

/* ------------------------------------------------------------------------- */
/*                     EXTENDED SAMPLE EDITOR                                */
/* ------------------------------------------------------------------------- */

void drawSampleEditorExt(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video) return;
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
	hexOutBg(video, bmp, 99, 124, PAL_FORGRND, PAL_DESKTOP, editor->clipboard.length, 8);

	/* Draw source/dest instrument and sample numbers */
	hexOutBg(video, bmp, 225, 96, PAL_FORGRND, PAL_DESKTOP, inst->editor.srcInstr, 2);
	hexOutBg(video, bmp, 274, 96, PAL_FORGRND, PAL_DESKTOP, inst->editor.srcSmp, 2);
	hexOutBg(video, bmp, 225, 109, PAL_FORGRND, PAL_DESKTOP, inst->editor.curInstr, 2);
	hexOutBg(video, bmp, 274, 109, PAL_FORGRND, PAL_DESKTOP, inst->editor.curSmp, 2);
}

void hideSampleEditorExtButtons(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (!ui) return;
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
	if (!inst) return;

	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);
	hideAllTopLeftPanelOverlays(inst);
	if (!inst->uiState.sampleEditorShown)
		showSampleEditor(inst);
	if (inst->uiState.instEditorShown)
		hideInstEditor(inst);

	inst->uiState.sampleEditorExtShown = true;
	inst->uiState.scopesShown = false;
}

void hideSampleEditorExt(ft2_instance_t *inst)
{
	if (!inst) return;

	inst->uiState.sampleEditorExtShown = false;
	hideSampleEditorExtButtons(inst);
	inst->uiState.scopesShown = true;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ui->scopes.needsFrameworkRedraw = true;
}

void toggleSampleEditorExt(ft2_instance_t *inst)
{
	if (!inst) return;
	if (inst->uiState.sampleEditorExtShown)
		hideSampleEditorExt(inst);
	else
		showSampleEditorExt(inst);
}

/* ------------------------------------------------------------------------- */
/*                          SAMPLE MEMORY                                    */
/* ------------------------------------------------------------------------- */

/* Allocates sample data with interpolation padding on both sides */
bool allocateSmpData(ft2_instance_t *inst, int instrNum, int smpNum, int32_t length, bool sample16Bit)
{
	const int32_t maxSamples = getMaxSamples(inst);
	if (!inst || instrNum < 1 || instrNum > 128 || smpNum < 0 || smpNum >= maxSamples)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (!instr) return false;

	ft2_sample_t *s = &instr->smp[smpNum];

	if (s->origDataPtr)
	{
		free(s->origDataPtr);
		s->origDataPtr = s->dataPtr = NULL;
	}
	if (s->origDataPtr24)
	{
		free(s->origDataPtr24);
		s->origDataPtr24 = s->dataPtr24 = NULL;
	}
	if (s->origDataPtrF)
	{
		free(s->origDataPtrF);
		s->origDataPtrF = s->dataPtrF = NULL;
	}

	/* Free right channel if exists */
	if (s->origDataPtrR)
	{
		free(s->origDataPtrR);
		s->origDataPtrR = s->dataPtrR = NULL;
	}
	if (s->origDataPtr24R)
	{
		free(s->origDataPtr24R);
		s->origDataPtr24R = s->dataPtr24R = NULL;
	}
	if (s->origDataPtrFR)
	{
		free(s->origDataPtrFR);
		s->origDataPtrFR = s->dataPtrFR = NULL;
	}

	/* Clear stereo flag (allocateSmpDataStereo sets it) */
	s->flags &= ~FT2_SAMPLE_STEREO;
	s->format = sample16Bit ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
	s->sampleRate = 0;

	if (length <= 0) { s->length = 0; return true; }

	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t padding = FT2_MAX_TAPS * bytesPerSample;
	int32_t allocLen = padding + (length * bytesPerSample) + padding;

	s->origDataPtr = (int8_t *)calloc(allocLen, 1);
	if (!s->origDataPtr) return false;

	s->dataPtr = s->origDataPtr + padding;
	return true;
}

/* Allocates stereo sample data with interpolation padding on both sides for both channels */
bool allocateSmpDataStereo(ft2_instance_t *inst, int instrNum, int smpNum, int32_t length, bool sample16Bit)
{
	const int32_t maxSamples = getMaxSamples(inst);
	if (!inst || instrNum < 1 || instrNum > 128 || smpNum < 0 || smpNum >= maxSamples)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (!instr) return false;

	ft2_sample_t *s = &instr->smp[smpNum];

	/* Free existing data */
	if (s->origDataPtr)
	{
		free(s->origDataPtr);
		s->origDataPtr = s->dataPtr = NULL;
	}
	if (s->origDataPtr24)
	{
		free(s->origDataPtr24);
		s->origDataPtr24 = s->dataPtr24 = NULL;
	}
	if (s->origDataPtrF)
	{
		free(s->origDataPtrF);
		s->origDataPtrF = s->dataPtrF = NULL;
	}
	if (s->origDataPtrR)
	{
		free(s->origDataPtrR);
		s->origDataPtrR = s->dataPtrR = NULL;
	}
	if (s->origDataPtr24R)
	{
		free(s->origDataPtr24R);
		s->origDataPtr24R = s->dataPtr24R = NULL;
	}
	if (s->origDataPtrFR)
	{
		free(s->origDataPtrFR);
		s->origDataPtrFR = s->dataPtrFR = NULL;
	}

	if (length <= 0)
	{
		s->length = 0;
		s->flags &= ~FT2_SAMPLE_STEREO;
		s->format = sample16Bit ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
		s->sampleRate = 0;
		return true;
	}

	int32_t bytesPerSample = sample16Bit ? 2 : 1;
	int32_t padding = FT2_MAX_TAPS * bytesPerSample;
	int32_t allocLen = padding + (length * bytesPerSample) + padding;

	/* Allocate left channel */
	s->origDataPtr = (int8_t *)calloc(allocLen, 1);
	if (!s->origDataPtr) return false;
	s->dataPtr = s->origDataPtr + padding;

	/* Allocate right channel */
	s->origDataPtrR = (int8_t *)calloc(allocLen, 1);
	if (!s->origDataPtrR)
	{
		free(s->origDataPtr);
		s->origDataPtr = s->dataPtr = NULL;
		return false;
	}
	s->dataPtrR = s->origDataPtrR + padding;

	s->flags |= FT2_SAMPLE_STEREO;
	s->format = sample16Bit ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
	s->sampleRate = 0;
	return true;
}

void freeSmpData(ft2_instance_t *inst, int instrNum, int smpNum)
{
	const int32_t maxSamples = getMaxSamples(inst);
	if (!inst || instrNum < 1 || instrNum > 128 || smpNum < 0 || smpNum >= maxSamples)
		return;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[smpNum];
	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = NULL; }
	s->dataPtr = NULL;
	if (s->origDataPtr24) { free(s->origDataPtr24); s->origDataPtr24 = NULL; }
	s->dataPtr24 = NULL;
	if (s->origDataPtrF) { free(s->origDataPtrF); s->origDataPtrF = NULL; }
	s->dataPtrF = NULL;

	/* Free right channel for stereo samples */
	if (s->origDataPtrR) { free(s->origDataPtrR); s->origDataPtrR = NULL; }
	s->dataPtrR = NULL;
	if (s->origDataPtr24R) { free(s->origDataPtr24R); s->origDataPtr24R = NULL; }
	s->dataPtr24R = NULL;
	if (s->origDataPtrFR) { free(s->origDataPtrFR); s->origDataPtrFR = NULL; }
	s->dataPtrFR = NULL;
	s->flags &= ~FT2_SAMPLE_STEREO;
	s->format = FT2_SMP_FORMAT_8BIT;
	s->sampleRate = 0;

	s->length = s->loopStart = s->loopLength = 0;
	s->isFixed = false;
}

static void onClearSampleResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (!inst || result != DIALOG_RESULT_YES) return;

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

	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_song_mark_modified(inst);
	ft2_xm2_update_usage(inst);
}

void clearSample(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length <= 0) return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Clear sample?",
	                                  inst, onClearSampleResult, NULL);
}

static void onClearInstrResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                               const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (!inst || result != DIALOG_RESULT_YES) return;

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
	ft2_sample_ed_mark_waveform_dirty(inst);

	ft2_song_mark_modified(inst);
	ft2_xm2_update_usage(inst);
}

void clearInstr(ft2_instance_t *inst)
{
	if (!inst || inst->editor.curInstr == 0) return;
	if (!inst->replayer.instr[inst->editor.curInstr]) return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Clear instrument?",
	                                  inst, onClearInstrResult, NULL);
}

void clearCopyBuffer(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smp_clipboard_t *clip = &FT2_SAMPLE_ED(inst)->clipboard;
	if (clip->data) { free(clip->data); clip->data = NULL; }
	if (clip->dataR) { free(clip->dataR); clip->dataR = NULL; }
	clip->length = 0;
	clip->is16Bit = false;
	clip->format = FT2_SMP_FORMAT_8BIT;
	clip->bytesPerSample = 1;
	clip->isStereo = false;
	clip->didCopyWholeSample = false;
	clip->hasInfo = false;
}

/* ------------------------------------------------------------------------- */
/*                      COPY/EXCHANGE OPERATIONS                             */
/* ------------------------------------------------------------------------- */

/* Deep copies sample including data allocation (handles stereo) */
bool cloneSample(ft2_sample_t *src, ft2_sample_t *dst)
{
	if (!dst) return false;

	/* Free existing data */
	if (dst->origDataPtr) { free(dst->origDataPtr); dst->origDataPtr = dst->dataPtr = NULL; }
	if (dst->origDataPtrR) { free(dst->origDataPtrR); dst->origDataPtrR = dst->dataPtrR = NULL; }
	if (dst->origDataPtr24) { free(dst->origDataPtr24); dst->origDataPtr24 = dst->dataPtr24 = NULL; }
	if (dst->origDataPtr24R) { free(dst->origDataPtr24R); dst->origDataPtr24R = dst->dataPtr24R = NULL; }
	if (dst->origDataPtrF) { free(dst->origDataPtrF); dst->origDataPtrF = dst->dataPtrF = NULL; }
	if (dst->origDataPtrFR) { free(dst->origDataPtrFR); dst->origDataPtrFR = dst->dataPtrFR = NULL; }

	if (!src) { memset(dst, 0, sizeof(ft2_sample_t)); return true; }

	memcpy(dst, src, sizeof(ft2_sample_t));
	dst->origDataPtr = dst->dataPtr = NULL;
	dst->origDataPtrR = dst->dataPtrR = NULL;
	dst->origDataPtr24 = dst->dataPtr24 = NULL;
	dst->origDataPtr24R = dst->dataPtr24R = NULL;
	dst->origDataPtrF = dst->dataPtrF = NULL;
	dst->origDataPtrFR = dst->dataPtrFR = NULL;
	dst->isFixed = false;
	dst->fixedPos = 0;

	ft2_sample_format_t format = sampleGetFormat(src);
	if (src->length > 0 && sampleHasData(src, format))
	{
		const bool isStereo = sampleIsStereo(src);
		const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
		const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
		const int32_t dataLen = src->length * (int32_t)bytesPerSample;
		const size_t allocSize = (size_t)padding + (size_t)dataLen + (size_t)padding;

		void *origL = calloc(allocSize, 1);
		if (!origL) { dst->length = 0; dst->flags &= ~FT2_SAMPLE_STEREO; return false; }
		uint8_t *dataL = (uint8_t *)origL + padding;
		memcpy(dataL, sampleGetDataPtr(src, format, false), dataLen);

		void *origR = NULL;
		uint8_t *dataR = NULL;
		if (isStereo)
		{
			origR = calloc(allocSize, 1);
			if (!origR)
			{
				free(origL);
				dst->length = 0;
				dst->flags &= ~FT2_SAMPLE_STEREO;
				return false;
			}
			dataR = (uint8_t *)origR + padding;
			memcpy(dataR, sampleGetDataPtr(src, format, true), dataLen);
		}
		else
		{
			dst->flags &= ~FT2_SAMPLE_STEREO;
		}

		if (format == FT2_SMP_FORMAT_24BIT)
		{
			dst->origDataPtr24 = (int32_t *)origL;
			dst->dataPtr24 = (int32_t *)dataL;
			if (isStereo)
			{
				dst->origDataPtr24R = (int32_t *)origR;
				dst->dataPtr24R = (int32_t *)dataR;
			}
		}
		else if (format == FT2_SMP_FORMAT_FLOAT32)
		{
			dst->origDataPtrF = (float *)origL;
			dst->dataPtrF = (float *)dataL;
			if (isStereo)
			{
				dst->origDataPtrFR = (float *)origR;
				dst->dataPtrFR = (float *)dataR;
			}
		}
		else
		{
			dst->origDataPtr = (int8_t *)origL;
			dst->dataPtr = (int8_t *)dataL;
			if (isStereo)
			{
				dst->origDataPtrR = (int8_t *)origR;
				dst->dataPtrR = (int8_t *)dataR;
			}
		}

		ft2_fix_sample(dst);
	}
	return true;
}

void copySmp(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t curInstr = inst->editor.curInstr, srcInstr = inst->editor.srcInstr;
	uint8_t curSmp = inst->editor.curSmp, srcSmp = inst->editor.srcSmp;
	if (curInstr == 0 || (curInstr == srcInstr && curSmp == srcSmp)) return;

	ft2_sample_t *src = inst->replayer.instr[srcInstr] ? &inst->replayer.instr[srcInstr]->smp[srcSmp] : NULL;
	if (!inst->replayer.instr[curInstr] && !ft2_instance_alloc_instr(inst, curInstr)) return;

	if (!cloneSample(src, &inst->replayer.instr[curInstr]->smp[curSmp])) return;
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
}

void xchgSmp(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t curInstr = inst->editor.curInstr, curSmp = inst->editor.curSmp, srcSmp = inst->editor.srcSmp;
	if (curInstr == 0 || curSmp == srcSmp) return;

	ft2_instr_t *instr = inst->replayer.instr[curInstr];
	if (!instr) return;

	ft2_sample_t tmp = instr->smp[curSmp];
	instr->smp[curSmp] = instr->smp[srcSmp];
	instr->smp[srcSmp] = tmp;
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
}

void copyInstr(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t curInstr = inst->editor.curInstr, srcInstr = inst->editor.srcInstr;
	if (curInstr == 0 || curInstr == srcInstr) return;

	ft2_instr_t *srcIns = inst->replayer.instr[srcInstr];
	if (!inst->replayer.instr[curInstr] && !ft2_instance_alloc_instr(inst, curInstr)) return;

	ft2_instr_t *dstIns = inst->replayer.instr[curInstr];

	const int32_t maxSamples = getMaxSamples(inst);
	for (int i = 0; i < maxSamples; i++)
		freeSmpData(inst, curInstr, i);

	if (!srcIns)
	{
		memset(dstIns, 0, sizeof(ft2_instr_t));
		dstIns->volEnvSpeed = 1;
		dstIns->panEnvSpeed = 1;
	}
	else
	{
		memcpy(dstIns->note2SampleLUT, srcIns->note2SampleLUT, sizeof(dstIns->note2SampleLUT));
		memcpy(dstIns->volEnvPoints, srcIns->volEnvPoints, sizeof(dstIns->volEnvPoints));
		memcpy(dstIns->panEnvPoints, srcIns->panEnvPoints, sizeof(dstIns->panEnvPoints));
		memcpy(dstIns->volEnvCurve, srcIns->volEnvCurve, sizeof(dstIns->volEnvCurve));
		memcpy(dstIns->panEnvCurve, srcIns->panEnvCurve, sizeof(dstIns->panEnvCurve));
		dstIns->volEnvSpeed = srcIns->volEnvSpeed;
		dstIns->panEnvSpeed = srcIns->panEnvSpeed;
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
		dstIns->numSamples = (srcIns->numSamples > maxSamples) ? maxSamples : srcIns->numSamples;

		for (int i = 0; i < maxSamples; i++)
		{
			memset(&dstIns->smp[i], 0, sizeof(ft2_sample_t));
			if (!cloneSample(&srcIns->smp[i], &dstIns->smp[i])) break;
		}
	}
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
}

void xchgInstr(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t curInstr = inst->editor.curInstr, srcInstr = inst->editor.srcInstr;
	if (curInstr == 0 || curInstr == srcInstr) return;

	ft2_instr_t *tmp = inst->replayer.instr[curInstr];
	inst->replayer.instr[curInstr] = inst->replayer.instr[srcInstr];
	inst->replayer.instr[srcInstr] = tmp;
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
}

/* Reverses sample data in selection or entire sample */
void sampleBackwards(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length < 2) return;

	ft2_sample_editor_t *ed = inst->ui ? FT2_SAMPLE_ED(inst) : NULL;
	int32_t start = (ed && ed->hasRange) ? ed->rangeStart : 0;
	int32_t end = (ed && ed->hasRange) ? ed->rangeEnd : s->length;
	if (start > end) { int32_t tmp = start; start = end; end = tmp; }
	if (start >= end) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	void *dataPtr = sampleGetDataPtr(s, format, false);
	reverseSampleRange(dataPtr, start, end, format);
	if (sampleIsStereo(s))
	{
		void *dataPtrR = sampleGetDataPtr(s, format, true);
		reverseSampleRange(dataPtrR, start, end, format);
	}

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* XORs sample data with sign bit to convert signed<->unsigned */
void sampleChangeSign(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length == 0) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		ft2_fix_sample(s);
		ft2_sample_ed_mark_waveform_dirty(inst);
		return;
	}
	else if (format == FT2_SMP_FORMAT_24BIT)
	{
		int32_t *ptr = (int32_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < s->length; i++) ptr[i] ^= 0x800000;
		if (sampleIsStereo(s))
		{
			int32_t *ptrR = (int32_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < s->length; i++) ptrR[i] ^= 0x800000;
		}
	}
	else if (format == FT2_SMP_FORMAT_16BIT)
	{
		int16_t *ptr = (int16_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < s->length; i++) ptr[i] ^= 0x8000;
		if (sampleIsStereo(s))
		{
			int16_t *ptrR = (int16_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < s->length; i++) ptrR[i] ^= 0x8000;
		}
	}
	else
	{
		int8_t *ptr = (int8_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < s->length; i++) ptr[i] ^= 0x80;
		if (sampleIsStereo(s))
		{
			int8_t *ptrR = (int8_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < s->length; i++) ptrR[i] ^= 0x80;
		}
	}

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Removes DC offset (average value) from sample */
void fixDC(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length == 0) return;

	ft2_sample_editor_t *ed = inst->ui ? FT2_SAMPLE_ED(inst) : NULL;
	bool sampleDataMarked = ed && ed->hasRange && ed->rangeStart != ed->rangeEnd;

	int32_t start = sampleDataMarked ? ed->rangeStart : 0;
	int32_t length = sampleDataMarked ? (ed->rangeEnd - ed->rangeStart) : s->length;
	if (start > ed->rangeEnd && sampleDataMarked) { start = ed->rangeEnd; length = ed->rangeStart - ed->rangeEnd; }
	if (length <= 0 || length > s->length) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	removeDCOffsetRange(s, format, false, start, length);
	if (sampleIsStereo(s))
		removeDCOffsetRange(s, format, true, start, length);

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Swaps byte order (useful for importing wrong-endian samples) */
void sampleByteSwap(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length == 0) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (format == FT2_SMP_FORMAT_8BIT)
	{
		ft2_fix_sample(s);
		ft2_sample_ed_mark_waveform_dirty(inst);
		return;
	}

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		int32_t len = s->length;
		uint8_t *ptr = (uint8_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < len; i++, ptr += 2)
		{
			uint8_t tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
		}

		if (sampleIsStereo(s))
		{
			ptr = (uint8_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < len; i++, ptr += 2)
			{
				uint8_t tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
			}
		}
	}
	else if (format == FT2_SMP_FORMAT_24BIT)
	{
		int32_t len = s->length;
		int32_t *ptr = (int32_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < len; i++)
		{
			uint32_t v = (uint32_t)(ptr[i] & 0xFFFFFF);
			uint32_t swapped = ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
			if (swapped & 0x800000)
				swapped |= 0xFF000000;
			ptr[i] = (int32_t)swapped;
		}

		if (sampleIsStereo(s))
		{
			ptr = (int32_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < len; i++)
			{
				uint32_t v = (uint32_t)(ptr[i] & 0xFFFFFF);
				uint32_t swapped = ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
				if (swapped & 0x800000)
					swapped |= 0xFF000000;
				ptr[i] = (int32_t)swapped;
			}
		}
	}
	else
	{
		int32_t len = s->length;
		uint8_t *ptr = (uint8_t *)sampleGetDataPtr(s, format, false);
		for (int32_t i = 0; i < len; i++, ptr += 4)
		{
			uint8_t tmp0 = ptr[0];
			uint8_t tmp1 = ptr[1];
			ptr[0] = ptr[3];
			ptr[1] = ptr[2];
			ptr[2] = tmp1;
			ptr[3] = tmp0;
		}

		if (sampleIsStereo(s))
		{
			ptr = (uint8_t *)sampleGetDataPtr(s, format, true);
			for (int32_t i = 0; i < len; i++, ptr += 4)
			{
				uint8_t tmp0 = ptr[0];
				uint8_t tmp1 = ptr[1];
				ptr[0] = ptr[3];
				ptr[1] = ptr[2];
				ptr[2] = tmp1;
				ptr[3] = tmp0;
			}
		}
	}

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Resizes sample allocation preserving existing data (handles stereo) */
static bool reallocateSmpData(ft2_sample_t *s, int32_t newLength, ft2_sample_format_t format)
{
	if (!s) return false;

	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)newLength * bytesPerSample) + (size_t)padding;
	const bool isStereo = (s->flags & FT2_SAMPLE_STEREO) != 0;

	void *oldL = sampleGetDataPtr(s, format, false);
	void *oldR = isStereo ? sampleGetDataPtr(s, format, true) : NULL;
	const size_t copyLen = (size_t)MIN(s->length, newLength) * bytesPerSample;

	void *newOrig = NULL;
	void *newOrigR = NULL;
	uint8_t *newData = NULL;
	uint8_t *newDataR = NULL;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		newOrig = calloc(allocSize, 1);
		if (!newOrig) return false;
		newData = (uint8_t *)newOrig + padding;

		if (isStereo)
		{
			newOrigR = calloc(allocSize, 1);
			if (!newOrigR) { free(newOrig); return false; }
			newDataR = (uint8_t *)newOrigR + padding;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		newOrig = calloc(allocSize, 1);
		if (!newOrig) return false;
		newData = (uint8_t *)newOrig + padding;

		if (isStereo)
		{
			newOrigR = calloc(allocSize, 1);
			if (!newOrigR) { free(newOrig); return false; }
			newDataR = (uint8_t *)newOrigR + padding;
		}
	}
	else
	{
		newOrig = calloc(allocSize, 1);
		if (!newOrig) return false;
		newData = (uint8_t *)newOrig + padding;

		if (isStereo)
		{
			newOrigR = calloc(allocSize, 1);
			if (!newOrigR) { free(newOrig); return false; }
			newDataR = (uint8_t *)newOrigR + padding;
		}
	}

	if (oldL && copyLen > 0)
		memcpy(newData, oldL, copyLen);
	if (isStereo && oldR && copyLen > 0)
		memcpy(newDataR, oldR, copyLen);

	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = s->dataPtr = NULL; }
	if (s->origDataPtr24) { free(s->origDataPtr24); s->origDataPtr24 = s->dataPtr24 = NULL; }
	if (s->origDataPtrF) { free(s->origDataPtrF); s->origDataPtrF = s->dataPtrF = NULL; }
	if (s->origDataPtrR) { free(s->origDataPtrR); s->origDataPtrR = s->dataPtrR = NULL; }
	if (s->origDataPtr24R) { free(s->origDataPtr24R); s->origDataPtr24R = s->dataPtr24R = NULL; }
	if (s->origDataPtrFR) { free(s->origDataPtrFR); s->origDataPtrFR = s->dataPtrFR = NULL; }

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		s->origDataPtr24 = (int32_t *)newOrig;
		s->dataPtr24 = (int32_t *)newData;
		if (isStereo)
		{
			s->origDataPtr24R = (int32_t *)newOrigR;
			s->dataPtr24R = (int32_t *)newDataR;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		s->origDataPtrF = (float *)newOrig;
		s->dataPtrF = (float *)newData;
		if (isStereo)
		{
			s->origDataPtrFR = (float *)newOrigR;
			s->dataPtrFR = (float *)newDataR;
		}
	}
	else
	{
		s->origDataPtr = (int8_t *)newOrig;
		s->dataPtr = (int8_t *)newData;
		if (isStereo)
		{
			s->origDataPtrR = (int8_t *)newOrigR;
			s->dataPtrR = (int8_t *)newDataR;
		}
	}

	return true;
}

/* Crops sample to selected range, deleting data outside selection */
void sampCrop(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;

	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (ed->rangeEnd == 0 || ed->rangeStart == ed->rangeEnd) return;

	ft2_sample_t *s = getCurrentSampleWithInst(ed, inst);
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

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

	int32_t newLength = r2 - r1;

	if (newLength <= 0)
		return;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	/* Move the cropped portion to the beginning */
	if (r1 > 0)
	{
		uint32_t bytesPerSample = ft2_sample_format_bytes(format);
		uint8_t *dataPtr = (uint8_t *)sampleGetDataPtr(s, format, false);
		memmove(dataPtr, dataPtr + (r1 * bytesPerSample), (size_t)(newLength * bytesPerSample));
		if (sampleIsStereo(s))
		{
			uint8_t *dataPtrR = (uint8_t *)sampleGetDataPtr(s, format, true);
			memmove(dataPtrR, dataPtrR + (r1 * bytesPerSample), (size_t)(newLength * bytesPerSample));
		}
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

	/* Realloc to new size */
	if (!reallocateSmpData(s, newLength, format))
	{
		ft2_fix_sample(s);
		ft2_sample_ed_mark_waveform_dirty(inst);
		return;
	}

	ft2_fix_sample(s);

	/* Select the new (cropped) sample range */
	ed->rangeStart = 0;
	ed->rangeEnd = newLength;
	ed->hasRange = true;
	ed->viewSize = newLength;
	ed->scrPos = 0;

	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_song_mark_modified(inst);
	ft2_xm2_update_usage(inst);
}

static void onMinimizeSampleResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                   const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (!inst || result != DIALOG_RESULT_YES) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;
	if (GET_LOOPTYPE(s->flags) == LOOP_OFF || s->loopStart + s->loopLength >= s->length) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);
	s->length = s->loopStart + s->loopLength;
	reallocateSmpData(s, s->length, format);
	ft2_fix_sample(s);

	ft2_sample_ed_show_all(inst);
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_song_mark_modified(inst);
}

/* Truncates sample at loop end (removes post-loop data) */
void sampMinimize(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!s || !sampleHasData(s, format) || s->length <= 0) return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (GET_LOOPTYPE(s->flags) == LOOP_OFF)
	{
		if (ui) ft2_dialog_show_message(&ui->dialog, "System message", "Only a looped sample can be minimized!");
		return;
	}
	if (s->loopStart + s->loopLength >= s->length)
	{
		if (ui) ft2_dialog_show_message(&ui->dialog, "System message", "This sample is already minimized.");
		return;
	}

	if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Minimize sample?",
	                                  inst, onMinimizeSampleResult, NULL);
}

/* ------------------------------------------------------------------------- */
/*                        REPEAT/LENGTH CONTROLS                             */
/* ------------------------------------------------------------------------- */

void sampRepeatUp(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;
	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];

	if (s->loopStart < s->length - s->loopLength)
	{
		ft2_stop_sample_voices(inst, s);
		s->loopStart++;
		ft2_song_mark_modified(inst);
	}
	ft2_sample_ed_mark_overlay_dirty(inst);
}

void sampRepeatDown(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;
	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];

	if (s->loopStart > 0)
	{
		ft2_stop_sample_voices(inst, s);
		s->loopStart--;
		ft2_song_mark_modified(inst);
	}
	ft2_sample_ed_mark_overlay_dirty(inst);
}

void sampReplenUp(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;
	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];

	if (s->loopStart + s->loopLength < s->length)
	{
		ft2_stop_sample_voices(inst, s);
		s->loopLength++;
		ft2_song_mark_modified(inst);
	}
	ft2_sample_ed_mark_overlay_dirty(inst);
}

void sampReplenDown(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;
	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];

	if (s->loopLength > 0)
	{
		ft2_stop_sample_voices(inst, s);
		s->loopLength--;
		ft2_song_mark_modified(inst);
	}
	ft2_sample_ed_mark_overlay_dirty(inst);
}

/* ------------------------------------------------------------------------- */
/*                         SAMPLE VOLUME                                     */
/* ------------------------------------------------------------------------- */

/* Applies volume scaling with optional fade (startVol -> endVol as %) */
void sampApplyVolume(ft2_instance_t *inst, double startVol, double endVol)
{
	if (!inst || !inst->ui) return;

	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (inst->editor.curInstr == 0) return;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr) return;

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_sample_format_t format = sampleGetFormat(s);
	if (!sampleHasData(s, format) || s->length <= 0) return;
	if (startVol == 100.0 && endVol == 100.0) return;

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

	ft2_unfix_sample(s);

	const double startFactor = startVol / 100.0;
	const double endFactor = endVol / 100.0;

	scaleSampleRangeLinear(s, format, false, x1, len, startFactor, endFactor);
	if (sampleIsStereo(s))
		scaleSampleRangeLinear(s, format, true, x1, len, startFactor, endFactor);

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
	ft2_song_mark_modified(inst);
}

bool ft2_sample_ed_resample_format(ft2_instance_t *inst, ft2_sample_format_t targetFormat, uint32_t targetRate)
{
	if (!inst || !inst->ui || targetRate == 0)
		return false;

	ft2_sample_editor_t *editor = FT2_SAMPLE_ED(inst);
	ft2_sample_t *s = getCurrentSampleWithInst(editor, inst);
	ft2_sample_format_t srcFormat = sampleGetFormat(s);
	if (!s || !sampleHasData(s, srcFormat) || s->length <= 0)
		return false;

	if (targetFormat < FT2_SMP_FORMAT_8BIT || targetFormat > FT2_SMP_FORMAT_FLOAT32)
		return false;

	const bool isStereo = sampleIsStereo(s);
	const double srcRate = (s->sampleRate > 0) ? (double)s->sampleRate : ft2_sample_get_playback_rate(inst, s);
	if (srcRate <= 0.0)
		return false;

	const double ratio = (double)targetRate / srcRate;
	if (ratio <= 0.0)
		return false;

	double dNewLen = s->length * ratio;
	if (dNewLen < 1.0)
		return false;
	if (dNewLen > (double)FT2_MAX_SAMPLE_LEN)
		dNewLen = (double)FT2_MAX_SAMPLE_LEN;
	const int32_t newLen = (int32_t)floor(dNewLen);
	if (newLen < 1)
		return false;

	const uint32_t bytesPerSample = ft2_sample_format_bytes(targetFormat);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)newLen * bytesPerSample) + (size_t)padding;

	void *newOrig = calloc(allocSize, 1);
	if (newOrig == NULL)
		return false;

	void *newOrigR = NULL;
	if (isStereo)
	{
		newOrigR = calloc(allocSize, 1);
		if (newOrigR == NULL)
		{
			free(newOrig);
			return false;
		}
	}

	uint8_t *newData = (uint8_t *)newOrig + padding;
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

	if (targetFormat == FT2_SMP_FORMAT_24BIT)
	{
		dstSmp.origDataPtr24 = (int32_t *)newOrig;
		dstSmp.dataPtr24 = (int32_t *)newData;
		if (isStereo)
		{
			dstSmp.origDataPtr24R = (int32_t *)newOrigR;
			dstSmp.dataPtr24R = (int32_t *)newDataR;
		}
	}
	else if (targetFormat == FT2_SMP_FORMAT_FLOAT32)
	{
		dstSmp.origDataPtrF = (float *)newOrig;
		dstSmp.dataPtrF = (float *)newData;
		if (isStereo)
		{
			dstSmp.origDataPtrFR = (float *)newOrigR;
			dstSmp.dataPtrFR = (float *)newDataR;
		}
	}
	else
	{
		dstSmp.origDataPtr = (int8_t *)newOrig;
		dstSmp.dataPtr = (int8_t *)newData;
		if (isStereo)
		{
			dstSmp.origDataPtrR = (int8_t *)newOrigR;
			dstSmp.dataPtrR = (int8_t *)newDataR;
		}
	}

	dstSmp.format = targetFormat;
	dstSmp.flags = s->flags;
	if (targetFormat == FT2_SMP_FORMAT_16BIT)
		dstSmp.flags |= SAMPLE_16BIT;
	else
		dstSmp.flags &= ~SAMPLE_16BIT;
	if (isStereo)
		dstSmp.flags |= FT2_SAMPLE_STEREO;
	else
		dstSmp.flags &= ~FT2_SAMPLE_STEREO;

	dstSmp.length = newLen;
	dstSmp.loopStart = (int32_t)round(s->loopStart * ratio);
	dstSmp.loopLength = (int32_t)round(s->loopLength * ratio);
	dstSmp.sampleRate = targetRate;
	dstSmp.relativeNote = 0;
	dstSmp.finetune = 0;
	dstSmp.isFixed = false;
	dstSmp.fixedPos = 0;

	const uint64_t delta64 = (uint64_t)round(4294967296.0 * srcRate / (double)targetRate);
	uint64_t posFrac64 = 0;
	for (int32_t i = 0; i < newLen; i++)
	{
		uint32_t srcIdx = (uint32_t)(posFrac64 >> 32);
		if (srcIdx >= (uint32_t)srcSmp.length)
			srcIdx = (uint32_t)(srcSmp.length - 1);

		float v = sampleGetFloat(&srcSmp, srcFormat, false, (int32_t)srcIdx);
		sampleSetFloat(&dstSmp, targetFormat, false, i, v);
		if (isStereo)
		{
			v = sampleGetFloat(&srcSmp, srcFormat, true, (int32_t)srcIdx);
			sampleSetFloat(&dstSmp, targetFormat, true, i, v);
		}
		posFrac64 += delta64;
	}

	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = NULL; }
	if (s->origDataPtrR) { free(s->origDataPtrR); s->origDataPtrR = NULL; }
	if (s->origDataPtr24) { free(s->origDataPtr24); s->origDataPtr24 = NULL; }
	if (s->origDataPtr24R) { free(s->origDataPtr24R); s->origDataPtr24R = NULL; }
	if (s->origDataPtrF) { free(s->origDataPtrF); s->origDataPtrF = NULL; }
	if (s->origDataPtrFR) { free(s->origDataPtrFR); s->origDataPtrFR = NULL; }

	*s = dstSmp;
	ft2_sanitize_sample(s);
	ft2_fix_sample(s);

	ft2_xm2_update_usage(inst);

	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
	ft2_song_mark_modified(inst);
	return true;
}
