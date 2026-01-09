/*
** FT2 Plugin - Scope Rendering
** Port of ft2_scopes.c for channel waveform display.
**
** Scopes show real-time sample playback with optional interpolation.
** Updates at 64 Hz, supports lined/dotted display, mute/solo via mouse.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_instance.h"

#define NS_PER_SCOPE_TICK (1000000000ULL / SCOPE_HZ)  /* ~15.6ms per tick */

/* ------------------------------------------------------------------------- */
/*                    PLATFORM-SPECIFIC TIMING                               */
/* ------------------------------------------------------------------------- */

#ifdef _WIN32
#include <windows.h>
static uint64_t getTickNs(void)
{
	static LARGE_INTEGER freq = {0};
	if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return (uint64_t)((double)count.QuadPart * 1e9 / (double)freq.QuadPart);
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
static uint64_t getTickNs(void)
{
	static mach_timebase_info_data_t info = {0};
	if (info.denom == 0) mach_timebase_info(&info);
	return mach_absolute_time() * info.numer / info.denom;
}
#else
#include <time.h>
static uint64_t getTickNs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

/* ------------------------------------------------------------------------- */
/*                           LOOKUP TABLES                                   */
/* ------------------------------------------------------------------------- */

/* Scope widths per channel count (indexed by [numChannels/2-1][channel]) */
const uint16_t scopeLenTab[16][32] =
{
	{285,285},  /* 2 ch */
	{141,141,141,141},  /* 4 ch */
	{93,93,93,93,93,93},  /* 6 ch */
	{69,69,69,69,69,69,69,69},  /* 8 ch */
	{55,55,55,54,54,55,55,55,54,54},  /* 10 ch */
	{45,45,45,45,45,45,45,45,45,45,45,45},  /* 12 ch */
	{39,38,38,38,38,38,38,39,38,38,38,38,38,38},  /* 14 ch */
	{33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},  /* 16 ch */
	{29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29},  /* 18 ch */
	{26,26,26,26,26,26,26,26,25,25,26,26,26,26,26,26,26,26,25,25},  /* 20 ch */
	{24,24,23,23,23,23,23,23,23,23,23,24,24,23,23,23,23,23,23,23,23,23},  /* 22 ch */
	{21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21},  /* 24 ch */
	{20,20,19,19,19,19,19,19,19,19,19,19,19,20,20,19,19,19,19,19,19,19,19,19,19,19},  /* 26 ch */
	{18,18,18,18,18,18,18,18,17,17,17,17,17,17,18,18,18,18,18,18,18,18,17,17,17,17,17,17},  /* 28 ch */
	{17,17,17,16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,16,16,16,16,16,16,16,16,16,16,16,16},  /* 30 ch */
	{15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15}  /* 32 ch */
};

/* Mute overlay bitmap dimensions per channel count */
const uint8_t scopeMuteBMP_Widths[16] = { 162,111,76,56,42,35,28,24,21,21,17,17,12,12,9,9 };
const uint8_t scopeMuteBMP_Heights[16] = { 27,27,26,25,25,25,24,24,24,24,24,24,24,24,24,24 };
const uint16_t scopeMuteBMP_Offs[16] = {
	0*(162*27), 1*(162*27), 2*(162*27), 3*(162*27),
	4*(162*27), 5*(162*27), 6*(162*27), 7*(162*27),
	8*(162*27), 8*(162*27), 9*(162*27), 9*(162*27),
	10*(162*27),10*(162*27),11*(162*27),11*(162*27)
};

/* ------------------------------------------------------------------------- */
/*                       INTERPOLATION LUT                                   */
/* ------------------------------------------------------------------------- */

/* Builds 4-point cubic B-spline LUT for scope interpolation (no overshoot) */
static bool calcScopeIntrpLUT(ft2_scopes_t *scopes)
{
	scopes->scopeIntrpLUT = (int16_t *)malloc(SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES * sizeof(int16_t));
	if (!scopes->scopeIntrpLUT) return false;

	int16_t *ptr16 = scopes->scopeIntrpLUT;
	for (int32_t i = 0; i < SCOPE_INTRP_PHASES; i++)
	{
		const float x1 = (float)i * (1.0f / SCOPE_INTRP_PHASES);
		const float x2 = x1 * x1, x3 = x2 * x1;

		*ptr16++ = (int16_t)((x1 * -0.5f + x2 * 0.5f - x3 / 6.0f + 1.0f/6.0f) * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)((x2 * -1.0f + x3 * 0.5f + 2.0f/3.0f) * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)((x1 * 0.5f + x2 * 0.5f - x3 * 0.5f + 1.0f/6.0f) * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)((x3 / 6.0f) * SCOPE_INTRP_SCALE);
	}
	return true;
}

/* ------------------------------------------------------------------------- */
/*                      INTERPOLATION MACROS                                 */
/* ------------------------------------------------------------------------- */

#define INTERP_DISABLED 0
#define INTERP_LINEAR 1
#define INTERP_CUBIC 3

/* 8-bit sample access macros */
#define SCOPE_SMP8_NEAREST(s, pos, sample) sample = s->base8[pos] << 8;
#define SCOPE_SMP8_LINEAR(s, pos, frac, sample) { \
	const int8_t *p = s->base8 + pos; \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS - 15); \
	sample = (p[0] << 8) + ((((p[1] - p[0]) << 8) * f) >> 15); }
#define SCOPE_SMP8_CUBIC(s, pos, frac, lut, sample) { \
	const int8_t *p = s->base8 + pos; \
	const int16_t *t = lut + (((frac) >> (SCOPE_FRAC_BITS - SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	sample = ((p[-1] * t[0]) + (p[0] * t[1]) + (p[1] * t[2]) + (p[2] * t[3])) >> (SCOPE_INTRP_SCALE_BITS - 8); }

/* 16-bit sample access macros */
#define SCOPE_SMP16_NEAREST(s, pos, sample) sample = s->base16[pos];
#define SCOPE_SMP16_LINEAR(s, pos, frac, sample) { \
	const int16_t *p = s->base16 + pos; \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS - 15); \
	sample = p[0] + (((p[1] - p[0]) * f) >> 15); }
#define SCOPE_SMP16_CUBIC(s, pos, frac, lut, sample) { \
	const int16_t *p = s->base16 + pos; \
	const int16_t *t = lut + (((frac) >> (SCOPE_FRAC_BITS - SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	sample = ((p[-1] * t[0]) + (p[0] * t[1]) + (p[1] * t[2]) + (p[2] * t[3])) >> SCOPE_INTRP_SCALE_BITS; }

/* Interpolated sample fetch - 8-bit */
static inline int32_t getScopeSample8(scope_t *s, int32_t pos, uint64_t frac, uint8_t interpMode, int16_t *lut)
{
	int32_t sample;
	if (interpMode == INTERP_DISABLED) { SCOPE_SMP8_NEAREST(s, pos, sample); }
	else if (interpMode == INTERP_LINEAR) { SCOPE_SMP8_LINEAR(s, pos, frac, sample); }
	else { SCOPE_SMP8_CUBIC(s, pos, frac, lut, sample); }
	return sample;
}

/* Interpolated sample fetch - 16-bit */
static inline int32_t getScopeSample16(scope_t *s, int32_t pos, uint64_t frac, uint8_t interpMode, int16_t *lut)
{
	int32_t sample;
	if (interpMode == INTERP_DISABLED) { SCOPE_SMP16_NEAREST(s, pos, sample); }
	else if (interpMode == INTERP_LINEAR) { SCOPE_SMP16_LINEAR(s, pos, frac, sample); }
	else { SCOPE_SMP16_CUBIC(s, pos, frac, lut, sample); }
	return sample;
}


/* ------------------------------------------------------------------------- */
/*                     PERIOD TO DELTA CONVERSION                            */
/* ------------------------------------------------------------------------- */

/* Converts period to scope position delta (for 64 Hz update) */
uint64_t ft2_period_to_scope_delta(ft2_instance_t *inst, uint32_t period)
{
	period &= 0xFFFF;
	if (period == 0) return 0;

	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		return inst->replayer.scopeLogTab[invPeriod % (12 * 16 * 4)] >> ((14 - invPeriod / (12 * 16 * 4)) & 31);
	}
	return inst->replayer.scopeAmigaPeriodDiv / period;
}

/* Converts period to scope draw delta (for display rate) */
uint64_t ft2_period_to_scope_draw_delta(ft2_instance_t *inst, uint32_t period)
{
	period &= 0xFFFF;
	if (period == 0) return 0;

	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		return inst->replayer.scopeDrawLogTab[invPeriod % (12 * 16 * 4)] >> ((14 - invPeriod / (12 * 16 * 4)) & 31);
	}
	return inst->replayer.scopeDrawAmigaPeriodDiv / period;
}

/* ------------------------------------------------------------------------- */
/*                         LINE DRAWING                                      */
/* ------------------------------------------------------------------------- */

/* Draws vertical line segment connecting two Y coordinates at same X */
static void scopeLine(ft2_video_t *video, int32_t x1, int32_t y1, int32_t y2, const uint32_t color)
{
	if (x1 < 0 || x1 >= SCREEN_W || y1 < 0 || y1 >= SCREEN_H || y2 < 0 || y2 >= SCREEN_H)
		return;

	uint32_t *dst32 = &video->frameBuffer[(y1 * SCREEN_W) + x1];
	*dst32 = color;

	const int32_t dy = y2 - y1;
	if (dy == 0) { dst32[1] = color; return; }

	int32_t ay = (dy < 0 ? -dy : dy) << 1;
	int32_t d = 1 - (ay >> 1);

	if (y1 > y2)
		for (; y1 != y2; y1--) { if (d >= 0) { d -= ay; dst32++; } d += 2; dst32 -= SCREEN_W; *dst32 = color; }
	else
		for (; y1 != y2; y1++) { if (d >= 0) { d -= ay; dst32++; } d += 2; dst32 += SCREEN_W; *dst32 = color; }
}

/* ------------------------------------------------------------------------- */
/*                     DOTTED SCOPE ROUTINES                                 */
/* ------------------------------------------------------------------------- */

static void scopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (s->base8[position] * s->volume) >> (8+2);
		else
			sample = 0;

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
			s->active = false;
	}
}

static void scopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (s->base8[position] * s->volume) >> (8+2);
		else
			sample = 0;

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
				position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
			else
				position = s->loopStart;
			s->hasLooped = true;
		}
	}
}

static void scopeDrawBidiLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t actualPos, position = s->position;
	uint64_t positionFrac = s->positionFrac;
	bool samplingBackwards = s->samplingBackwards;

	for (; x < width; x++)
	{
		if (s->active)
		{
			if (samplingBackwards)
				actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
			else
				actualPos = position;
			sample = (s->base8[actualPos] * s->volume) >> (8+2);
		}
		else
		{
			sample = 0;
		}

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
			{
				const uint32_t overflow = position - s->sampleEnd;
				const uint32_t cycles = overflow / (uint32_t)s->loopLength;
				const uint32_t phase = overflow % (uint32_t)s->loopLength;
				position = s->loopStart + phase;
				samplingBackwards ^= !(cycles & 1);
			}
			else
			{
				position = s->loopStart;
			}
			s->hasLooped = true;
		}
	}
}

static void scopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (s->base16[position] * s->volume) >> (16+2);
		else
			sample = 0;

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
			s->active = false;
	}
}

static void scopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (s->base16[position] * s->volume) >> (16+2);
		else
			sample = 0;

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
				position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
			else
				position = s->loopStart;
			s->hasLooped = true;
		}
	}
}

static void scopeDrawBidiLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	(void)scopes;
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w;
	int32_t sample;
	int32_t actualPos, position = s->position;
	uint64_t positionFrac = s->positionFrac;
	bool samplingBackwards = s->samplingBackwards;

	for (; x < width; x++)
	{
		if (s->active)
		{
			if (samplingBackwards)
				actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
			else
				actualPos = position;
			sample = (s->base16[actualPos] * s->volume) >> (16+2);
		}
		else
		{
			sample = 0;
		}

		video->frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
			{
				const uint32_t overflow = position - s->sampleEnd;
				const uint32_t cycles = overflow / (uint32_t)s->loopLength;
				const uint32_t phase = overflow % (uint32_t)s->loopLength;
				position = s->loopStart + phase;
				samplingBackwards ^= !(cycles & 1);
			}
			else
			{
				position = s->loopStart;
			}
			s->hasLooped = true;
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                      LINED SCOPE ROUTINES                                 */
/* ------------------------------------------------------------------------- */

static void linedScopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
		sample = (getScopeSample8(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
	else
		sample = 0;
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
		s->active = false;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (getScopeSample8(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
		else
			sample = 0;

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
			s->active = false;
	}
}

static void linedScopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
		sample = (getScopeSample8(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
	else
		sample = 0;
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
	{
		if (s->loopLength >= 2)
			position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
		else
			position = s->loopStart;
		s->hasLooped = true;
	}

	for (; x < width; x++)
	{
		if (s->active)
			sample = (getScopeSample8(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
		else
			sample = 0;

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
				position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
			else
				position = s->loopStart;
			s->hasLooped = true;
		}
	}
}

static void linedScopeDrawBidiLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t actualPos, position = s->position;
	uint64_t positionFrac = s->positionFrac;
	bool samplingBackwards = s->samplingBackwards;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
	{
		if (samplingBackwards)
			actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
		else
			actualPos = position;
		sample = (getScopeSample8(s, actualPos, positionFrac, interp, lut) * s->volume) >> (16+2);
	}
	else
	{
		sample = 0;
	}
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
	{
		if (s->loopLength >= 2)
		{
			const uint32_t overflow = position - s->sampleEnd;
			const uint32_t cycles = overflow / (uint32_t)s->loopLength;
			const uint32_t phase = overflow % (uint32_t)s->loopLength;
			position = s->loopStart + phase;
			samplingBackwards ^= !(cycles & 1);
		}
		else
		{
			position = s->loopStart;
		}
		s->hasLooped = true;
	}

	for (; x < width; x++)
	{
		if (s->active)
		{
			if (samplingBackwards)
				actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
			else
				actualPos = position;
			sample = (getScopeSample8(s, actualPos, positionFrac, interp, lut) * s->volume) >> (16+2);
		}
		else
		{
			sample = 0;
		}

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
			{
				const uint32_t overflow = position - s->sampleEnd;
				const uint32_t cycles = overflow / (uint32_t)s->loopLength;
				const uint32_t phase = overflow % (uint32_t)s->loopLength;
				position = s->loopStart + phase;
				samplingBackwards ^= !(cycles & 1);
			}
			else
			{
				position = s->loopStart;
			}
			s->hasLooped = true;
		}
	}
}

static void linedScopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
		sample = (getScopeSample16(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
	else
		sample = 0;
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
		s->active = false;

	for (; x < width; x++)
	{
		if (s->active)
			sample = (getScopeSample16(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
		else
			sample = 0;

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
			s->active = false;
	}
}

static void linedScopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t position = s->position;
	uint64_t positionFrac = s->positionFrac;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
		sample = (getScopeSample16(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
	else
		sample = 0;
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
	{
		if (s->loopLength >= 2)
			position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
		else
			position = s->loopStart;
		s->hasLooped = true;
	}

	for (; x < width; x++)
	{
		if (s->active)
			sample = (getScopeSample16(s, position, positionFrac, interp, lut) * s->volume) >> (16+2);
		else
			sample = 0;

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
				position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength);
			else
				position = s->loopStart;
			s->hasLooped = true;
		}
	}
}

static void linedScopeDrawBidiLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, ft2_video_t *video, ft2_scopes_t *scopes)
{
	const uint32_t color = video->palette[PAL_PATTEXT];
	uint32_t width = x + w - 1;
	int32_t sample;
	int32_t actualPos, position = s->position;
	uint64_t positionFrac = s->positionFrac;
	bool samplingBackwards = s->samplingBackwards;
	int32_t smpY1, smpY2;
	const uint8_t interp = scopes->interpolation;
	int16_t *lut = scopes->scopeIntrpLUT;

	/* Get first sample */
	if (s->active)
	{
		if (samplingBackwards)
			actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
		else
			actualPos = position;
		sample = (getScopeSample16(s, actualPos, positionFrac, interp, lut) * s->volume) >> (16+2);
	}
	else
	{
		sample = 0;
	}
	smpY1 = lineY - sample;
	positionFrac += s->drawDelta;
	position += positionFrac >> 32;
	positionFrac &= 0xFFFFFFFF;
	if (position >= s->sampleEnd)
	{
		if (s->loopLength >= 2)
		{
			const uint32_t overflow = position - s->sampleEnd;
			const uint32_t cycles = overflow / (uint32_t)s->loopLength;
			const uint32_t phase = overflow % (uint32_t)s->loopLength;
			position = s->loopStart + phase;
			samplingBackwards ^= !(cycles & 1);
		}
		else
		{
			position = s->loopStart;
		}
		s->hasLooped = true;
	}

	for (; x < width; x++)
	{
		if (s->active)
		{
			if (samplingBackwards)
				actualPos = (s->sampleEnd - 1) - (position - s->loopStart);
			else
				actualPos = position;
			sample = (getScopeSample16(s, actualPos, positionFrac, interp, lut) * s->volume) >> (16+2);
		}
		else
		{
			sample = 0;
		}

		smpY2 = lineY - sample;
		scopeLine(video, x, smpY1, smpY2, color);
		smpY1 = smpY2;

		positionFrac += s->drawDelta;
		position += positionFrac >> 32;
		positionFrac &= 0xFFFFFFFF;

		if (position >= s->sampleEnd)
		{
			if (s->loopLength >= 2)
			{
				const uint32_t overflow = position - s->sampleEnd;
				const uint32_t cycles = overflow / (uint32_t)s->loopLength;
				const uint32_t phase = overflow % (uint32_t)s->loopLength;
				position = s->loopStart + phase;
				samplingBackwards ^= !(cycles & 1);
			}
			else
			{
				position = s->loopStart;
			}
			s->hasLooped = true;
		}
	}
}

/* Routine table: [0-5]=dotted, [6-11]=lined; within: [0-2]=8bit, [3-5]=16bit; looptype */
static const scopeDrawRoutine scopeDrawRoutineTable[12] = {
	scopeDrawNoLoop_8bit, scopeDrawLoop_8bit, scopeDrawBidiLoop_8bit,
	scopeDrawNoLoop_16bit, scopeDrawLoop_16bit, scopeDrawBidiLoop_16bit,
	linedScopeDrawNoLoop_8bit, linedScopeDrawLoop_8bit, linedScopeDrawBidiLoop_8bit,
	linedScopeDrawNoLoop_16bit, linedScopeDrawLoop_16bit, linedScopeDrawBidiLoop_16bit
};

/* ------------------------------------------------------------------------- */
/*                          HELPER FUNCTIONS                                 */
/* ------------------------------------------------------------------------- */

/* Draws channel number (1-32) in scope corner */
static void drawScopeNumber(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t chNr, bool outline)
{
	x++; y++; chNr++;
	if (outline)
	{
		if (chNr < 10) charOutOutlined(video, bmp, x, y, PAL_MOUSEPT, '0' + chNr);
		else { charOutOutlined(video, bmp, x, y, PAL_MOUSEPT, '0' + (chNr / 10)); charOutOutlined(video, bmp, x + 7, y, PAL_MOUSEPT, '0' + (chNr % 10)); }
	}
	else
	{
		if (chNr < 10) charOut(video, bmp, x, y, PAL_MOUSEPT, '0' + chNr);
		else { charOut(video, bmp, x, y, PAL_MOUSEPT, '0' + (chNr / 10)); charOut(video, bmp, x + 7, y, PAL_MOUSEPT, '0' + (chNr % 10)); }
	}
}

/* Redraws single scope's framework and mute overlay */
static void redrawScope(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp, int32_t ch)
{
	int32_t chansPerRow = (uint32_t)scopes->numChannels >> 1;
	int32_t chanLookup = chansPerRow - 1;
	if (chanLookup < 0 || chanLookup >= 16) return;

	const uint16_t *scopeLens = scopeLenTab[chanLookup];
	uint16_t x = 2, y = 94, scopeLen = 0;

	for (int32_t i = 0; i < scopes->numChannels; i++)
	{
		scopeLen = scopeLens[i];
		if (i == chansPerRow) { x = 2; y += 39; }
		if (i == ch) break;
		x += scopeLen + 3;
	}

	drawFramework(video, x, y, scopeLen + 2, 38, FRAMEWORK_TYPE2);

	if (scopes->channelMuted[ch])
	{
		const uint16_t muteGfxLen = scopeMuteBMP_Widths[chanLookup];
		const uint16_t muteGfxX = x + ((scopeLen - muteGfxLen) >> 1);
		if (bmp && bmp->scopeMute)
			blitFastClipX(video, muteGfxX, y + 6, bmp->scopeMute + scopeMuteBMP_Offs[chanLookup],
			              162, scopeMuteBMP_Heights[chanLookup], muteGfxLen);
		if (scopes->ptnChnNumbers)
			drawScopeNumber(video, bmp, x + 1, y + 1, (uint8_t)ch, true);
	}
	scopes->scopes[ch].wasCleared = false;
}

/* ------------------------------------------------------------------------- */
/*                          PUBLIC API                                       */
/* ------------------------------------------------------------------------- */

void ft2_scopes_init(ft2_scopes_t *scopes)
{
	if (!scopes) return;
	memset(scopes, 0, sizeof(ft2_scopes_t));
	scopes->numChannels = 8;
	scopes->linedScopes = true;
	scopes->ptnChnNumbers = true;
	calcScopeIntrpLUT(scopes);
}

void ft2_scopes_free(ft2_scopes_t *scopes)
{
	if (!scopes) return;
	if (scopes->scopeIntrpLUT) { free(scopes->scopeIntrpLUT); scopes->scopeIntrpLUT = NULL; }
}

/* Triggers scope playback from sync queue entry */
static void scopeTriggerFromEntry(scope_t *s, const ft2_scope_sync_entry_t *entry)
{
	if ((!entry->base8 && !entry->base16) || entry->length < 1)
	{
		s->active = false;
		return;
	}

	uint8_t loopType = (entry->loopLength < 1) ? LOOP_OFF : entry->loopType;

	s->base8 = entry->base8;
	s->base16 = entry->base16;
	s->sample16Bit = entry->sample16Bit;
	s->loopType = loopType;
	s->hasLooped = false;
	s->samplingBackwards = false;
	s->sampleEnd = (loopType == LOOP_OFF) ? entry->length : (entry->loopStart + entry->loopLength);
	s->loopStart = entry->loopStart;
	s->loopLength = entry->loopLength;
	s->loopEnd = entry->loopStart + entry->loopLength;
	s->position = entry->smpStartPos;
	s->positionFrac = 0;
	s->wasCleared = false;

	s->active = (s->position < s->sampleEnd);
}

/* Advances scope position by one 64 Hz tick */
static void updateScopePosition(scope_t *s)
{
	if (!s->active) return;

	s->positionFrac += s->delta;
	s->position += s->positionFrac >> SCOPE_FRAC_BITS;
	s->positionFrac &= SCOPE_FRAC_MASK;

	if (s->position >= s->sampleEnd)
	{
		if (s->loopType == LOOP_BIDI)
		{
			if (s->loopLength >= 2)
			{
				uint32_t overflow = s->position - s->sampleEnd;
				s->position = s->loopStart + (overflow % s->loopLength);
				s->samplingBackwards ^= !((overflow / s->loopLength) & 1);
			}
			else
				s->position = s->loopStart;
			s->hasLooped = true;
		}
		else if (s->loopType == LOOP_FORWARD)
		{
			s->position = s->loopStart + (s->loopLength >= 2 ? (s->position - s->sampleEnd) % s->loopLength : 0);
			s->hasLooped = true;
		}
		else
			s->active = false;
	}
}

/*
** Main scope update: processes sync queue and advances positions at 64 Hz.
** Called from UI thread; catches up ticks if frame rate drops.
*/
void ft2_scopes_update(ft2_scopes_t *scopes, struct ft2_instance_t *inst)
{
	if (!scopes || !inst) return;

	if (inst->scopesClearRequested)
	{
		inst->scopesClearRequested = false;
		for (int32_t i = 0; i < MAX_CHANNELS; i++)
			scopes->scopes[i].active = false;
	}

	scopes->linedScopes = inst->config.linedScopes;
	scopes->interpolation = inst->audio.interpolationType;
	scopes->ptnChnNumbers = inst->config.ptnChnNumbers;

	uint8_t songChannels = inst->replayer.song.numChannels;
	if (songChannels < 2) songChannels = 2;
	if (songChannels > 32) songChannels = 32;
	if (scopes->numChannels != songChannels)
	{
		scopes->numChannels = songChannels;
		scopes->needsFrameworkRedraw = true;
	}

	/* Process sync queue from audio thread */
	ft2_scope_sync_entry_t entry;
	while (ft2_scope_sync_queue_pop(inst, &entry))
	{
		if (entry.channel >= MAX_CHANNELS) continue;
		scope_t *s = &scopes->scopes[entry.channel];

		if (entry.status & FT2_SCOPE_UPDATE_VOL) s->volume = entry.scopeVolume;
		if (entry.status & FT2_SCOPE_UPDATE_PERIOD)
		{
			s->delta = ft2_period_to_scope_delta(inst, entry.period);
			s->drawDelta = ft2_period_to_scope_draw_delta(inst, entry.period);
		}
		if (entry.status & FT2_SCOPE_TRIGGER_VOICE) scopeTriggerFromEntry(s, &entry);
	}

	/* Advance positions at 64 Hz, capped at 8 ticks to avoid spiral */
	uint64_t currentTick = getTickNs();
	if (scopes->lastUpdateTick == 0) scopes->lastUpdateTick = currentTick;

	int32_t ticksToRun = (int32_t)((currentTick - scopes->lastUpdateTick) / NS_PER_SCOPE_TICK);
	if (ticksToRun > 8) ticksToRun = 8;

	if (ticksToRun > 0)
	{
		scopes->lastUpdateTick += (uint64_t)ticksToRun * NS_PER_SCOPE_TICK;
		for (int32_t t = 0; t < ticksToRun; t++)
			for (int32_t i = 0; i < scopes->numChannels; i++)
				updateScopePosition(&scopes->scopes[i]);
	}
}

void ft2_scope_stop(ft2_scopes_t *scopes, int channel)
{
	if (!scopes || channel < 0 || channel >= MAX_CHANNELS) return;
	scopes->scopes[channel].active = false;
}

void ft2_scopes_stop_all(ft2_scopes_t *scopes)
{
	if (!scopes) return;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		scopes->scopes[i].active = false;
}

void ft2_scopes_draw(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!scopes || !video) return;

	int32_t chansPerRow = (uint32_t)scopes->numChannels >> 1;
	if (chansPerRow < 1)
		chansPerRow = 1;

	int32_t chanLookup = chansPerRow - 1;
	if (chanLookup >= 16)
		chanLookup = 15;

	const uint16_t *scopeLens = scopeLenTab[chanLookup];
	uint16_t scopeXOffs = 3;
	uint16_t scopeYOffs = 95;
	int16_t scopeLineY = 112;

	for (int32_t i = 0; i < scopes->numChannels; i++)
	{
		if (i == chansPerRow)
		{
			scopeXOffs = 3;
			scopeYOffs = 134;
			scopeLineY = 151;
		}

		const uint16_t scopeDrawLen = scopeLens[i];

		if (scopes->channelMuted[i])
		{
			scopeXOffs += scopeDrawLen + 3;
			continue;
		}

		scope_t *s = &scopes->scopes[i];
		if (s->active && s->volume > 0)
		{
			s->wasCleared = false;

			/* Clear scope background */
			clearRect(video, scopeXOffs, scopeYOffs, scopeDrawLen, SCOPE_HEIGHT);

			/* Draw scope */
			int routineIdx = (scopes->linedScopes ? 6 : 0) + (s->sample16Bit ? 3 : 0) + s->loopType;
			scopeDrawRoutineTable[routineIdx](s, scopeXOffs, scopeLineY, scopeDrawLen, video, scopes);
		}
		else
		{
			if (!s->wasCleared)
			{
				clearRect(video, scopeXOffs, scopeYOffs, scopeDrawLen, SCOPE_HEIGHT);
				hLine(video, scopeXOffs, scopeLineY, scopeDrawLen, PAL_PATTEXT);
				s->wasCleared = true;
			}
		}

		if (scopes->ptnChnNumbers)
			drawScopeNumber(video, bmp, scopeXOffs, scopeYOffs, (uint8_t)i, false);

		if (scopes->multiRecChn[i] && bmp && bmp->scopeRec)
			blit(video, scopeXOffs + 1, scopeYOffs + 31, bmp->scopeRec, 13, 4);

		scopeXOffs += scopeDrawLen + 3;
	}
}

void ft2_scopes_draw_framework(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!scopes || !video) return;
	drawFramework(video, 0, 92, 291, 81, FRAMEWORK_TYPE1);
	for (int32_t i = 0; i < scopes->numChannels; i++)
		redrawScope(scopes, video, bmp, i);
}

/* Handles mouse click on scopes: left=mute, right=multi-rec, both=solo */
bool ft2_scopes_mouse_down(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, bool leftButton, bool rightButton)
{
	if (!scopes) return false;
	if (mouseY < 95 || mouseY > 169 || mouseX < 3 || mouseX > 288) return false;
	if (mouseY > 130 && mouseY < 134) return true;  /* Gap between rows */

	int32_t chansPerRow = (uint32_t)scopes->numChannels >> 1;
	if (chansPerRow < 1) chansPerRow = 1;
	int32_t chanLookup = (chansPerRow - 1 < 16) ? chansPerRow - 1 : 15;
	const uint16_t *scopeLens = scopeLenTab[chanLookup];

	uint16_t x = 3;
	int32_t i;
	for (i = 0; i < chansPerRow && !(mouseX >= x && mouseX < x + scopeLens[i]); i++)
		x += scopeLens[i] + 3;
	if (i == chansPerRow) return true;

	int32_t chanToToggle = (mouseY >= 134) ? i + chansPerRow : i;
	if (chanToToggle >= scopes->numChannels) return true;

	if (leftButton && rightButton)
	{
		/* Solo: unmute all if others muted, else mute all except clicked */
		bool othersMuted = false;
		for (int32_t j = 0; j < scopes->numChannels; j++)
			if (j != chanToToggle && scopes->channelMuted[j]) othersMuted = true;

		for (int32_t j = 0; j < scopes->numChannels; j++)
			scopes->channelMuted[j] = othersMuted ? false : (j != chanToToggle);
		for (int32_t j = 0; j < scopes->numChannels; j++)
			redrawScope(scopes, video, bmp, j);
	}
	else if (leftButton)
	{
		scopes->channelMuted[chanToToggle] ^= 1;
		redrawScope(scopes, video, bmp, chanToToggle);
	}
	else if (rightButton)
	{
		if (!scopes->channelMuted[chanToToggle])
		{
			scopes->multiRecChn[chanToToggle] ^= 1;
			scopes->scopes[chanToToggle].wasCleared = false;
		}
		else
		{
			scopes->multiRecChn[chanToToggle] = true;
			scopes->channelMuted[chanToToggle] = false;
			redrawScope(scopes, video, bmp, chanToToggle);
		}
	}
	return true;
}

void ft2_scopes_set_mute(ft2_scopes_t *scopes, int channel, bool muted)
{
	if (!scopes || channel < 0 || channel >= MAX_CHANNELS) return;
	scopes->channelMuted[channel] = muted;
}

bool ft2_scopes_get_mute(ft2_scopes_t *scopes, int channel)
{
	if (!scopes || channel < 0 || channel >= MAX_CHANNELS) return false;
	return scopes->channelMuted[channel];
}
