/**
 * @file ft2_plugin_scopes.c
 * @brief Exact port of FT2 scope rendering for the plugin.
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

/* Nanoseconds per scope update tick (1/64 second = 15625000 ns) */
#define NS_PER_SCOPE_TICK (1000000000ULL / SCOPE_HZ)

/* Platform-specific high-resolution tick counter */
#ifdef _WIN32
#include <windows.h>
static uint64_t getTickNs(void)
{
	static LARGE_INTEGER freq = {0};
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return (uint64_t)((double)count.QuadPart * 1000000000.0 / (double)freq.QuadPart);
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
static uint64_t getTickNs(void)
{
	static mach_timebase_info_data_t info = {0};
	if (info.denom == 0)
		mach_timebase_info(&info);
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

/* Scope length tables - exact from ft2_tables.c */
const uint16_t scopeLenTab[16][32] =
{
	/*  2 ch */ {285,285},
	/*  4 ch */ {141,141,141,141},
	/*  6 ch */ {93,93,93,93,93,93},
	/*  8 ch */ {69,69,69,69,69,69,69,69},
	/* 10 ch */ {55,55,55,54,54,55,55,55,54,54},
	/* 12 ch */ {45,45,45,45,45,45,45,45,45,45,45,45},
	/* 14 ch */ {39,38,38,38,38,38,38,39,38,38,38,38,38,38},
	/* 16 ch */ {33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},
	/* 18 ch */ {29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29},
	/* 20 ch */ {26,26,26,26,26,26,26,26,25,25,26,26,26,26,26,26,26,26,25,25},
	/* 22 ch */ {24,24,23,23,23,23,23,23,23,23,23,24,24,23,23,23,23,23,23,23,23,23},
	/* 24 ch */ {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21},
	/* 26 ch */ {20,20,19,19,19,19,19,19,19,19,19,19,19,20,20,19,19,19,19,19,19,19,19,19,19,19},
	/* 28 ch */ {18,18,18,18,18,18,18,18,17,17,17,17,17,17,18,18,18,18,18,18,18,18,17,17,17,17,17,17},
	/* 30 ch */ {17,17,17,16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,16,16,16,16,16,16,16,16,16,16,16,16},
	/* 32 ch */ {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15}
};

/* Scope mute bitmap tables - exact from ft2_tables.c */
const uint8_t scopeMuteBMP_Widths[16] =
{
	162,111, 76, 56, 42, 35, 28, 24,
	 21, 21, 17, 17, 12, 12,  9,  9
};

const uint8_t scopeMuteBMP_Heights[16] =
{
	27, 27, 26, 25, 25, 25, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24
};

const uint16_t scopeMuteBMP_Offs[16] =
{
	 0*(162*27), 1*(162*27), 2*(162*27), 3*(162*27),
	 4*(162*27), 5*(162*27), 6*(162*27), 7*(162*27),
	 8*(162*27), 8*(162*27), 9*(162*27), 9*(162*27),
	10*(162*27),10*(162*27),11*(162*27),11*(162*27)
};

/* --------------------------------------------------------------------- */
/*                       SCOPE INTERPOLATION LUT                         */
/* --------------------------------------------------------------------- */

static bool calcScopeIntrpLUT(ft2_scopes_t *scopes)
{
	scopes->scopeIntrpLUT = (int16_t *)malloc(SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES * sizeof(int16_t));
	if (scopes->scopeIntrpLUT == NULL)
		return false;

	/* 4-point cubic B-spline (no overshoot) */
	int16_t *ptr16 = scopes->scopeIntrpLUT;
	for (int32_t i = 0; i < SCOPE_INTRP_PHASES; i++)
	{
		const float x1 = (float)i * (1.0f / SCOPE_INTRP_PHASES);
		const float x2 = x1 * x1;
		const float x3 = x2 * x1;

		const float t1 = (x1 * -(1.0f/2.0f)) + (x2 * (1.0f/2.0f)) + (x3 * -(1.0f/6.0f)) + (1.0f/6.0f);
		const float t2 =                       (x2 *      -1.0f ) + (x3 *  (1.0f/2.0f)) + (2.0f/3.0f);
		const float t3 = (x1 *  (1.0f/2.0f)) + (x2 * (1.0f/2.0f)) + (x3 * -(1.0f/2.0f)) + (1.0f/6.0f);
		const float t4 =                                             x3 *  (1.0f/6.0f);

		*ptr16++ = (int16_t)(t1 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t2 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t3 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t4 * SCOPE_INTRP_SCALE);
	}

	return true;
}

/* --------------------------------------------------------------------- */
/*                    SCOPE INTERPOLATION MACROS                          */
/* --------------------------------------------------------------------- */

/* Interpolation mode constants (from ft2_plugin_config.h) */
#define INTERP_DISABLED 0
#define INTERP_LINEAR 1
#define INTERP_CUBIC 3

/* Nearest neighbor (no interpolation) - 8-bit */
#define SCOPE_SMP8_NEAREST(s, pos, sample) \
	sample = s->base8[pos] << 8;

/* Linear interpolation - 8-bit */
#define SCOPE_SMP8_LINEAR(s, pos, frac, sample) \
{ \
	const int8_t *p = s->base8 + pos; \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS - 15); \
	sample = (p[0] << 8) + ((((p[1] - p[0]) << 8) * f) >> 15); \
}

/* Cubic interpolation - 8-bit (uses scopeIntrpLUT) */
#define SCOPE_SMP8_CUBIC(s, pos, frac, lut, sample) \
{ \
	const int8_t *p = s->base8 + pos; \
	const int16_t *t = lut + (((frac) >> (SCOPE_FRAC_BITS - SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	sample = ((p[-1] * t[0]) + (p[0] * t[1]) + (p[1] * t[2]) + (p[2] * t[3])) >> (SCOPE_INTRP_SCALE_BITS - 8); \
}

/* Nearest neighbor (no interpolation) - 16-bit */
#define SCOPE_SMP16_NEAREST(s, pos, sample) \
	sample = s->base16[pos];

/* Linear interpolation - 16-bit */
#define SCOPE_SMP16_LINEAR(s, pos, frac, sample) \
{ \
	const int16_t *p = s->base16 + pos; \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS - 15); \
	sample = p[0] + (((p[1] - p[0]) * f) >> 15); \
}

/* Cubic interpolation - 16-bit (uses scopeIntrpLUT) */
#define SCOPE_SMP16_CUBIC(s, pos, frac, lut, sample) \
{ \
	const int16_t *p = s->base16 + pos; \
	const int16_t *t = lut + (((frac) >> (SCOPE_FRAC_BITS - SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	sample = ((p[-1] * t[0]) + (p[0] * t[1]) + (p[1] * t[2]) + (p[2] * t[3])) >> SCOPE_INTRP_SCALE_BITS; \
}

/* Get interpolated sample - 8-bit (selects method based on interpMode) */
static inline int32_t getScopeSample8(scope_t *s, int32_t pos, uint64_t frac, uint8_t interpMode, int16_t *lut)
{
	int32_t sample;
	if (interpMode == INTERP_DISABLED)
	{
		SCOPE_SMP8_NEAREST(s, pos, sample);
	}
	else if (interpMode == INTERP_LINEAR)
	{
		SCOPE_SMP8_LINEAR(s, pos, frac, sample);
	}
	else
	{
		SCOPE_SMP8_CUBIC(s, pos, frac, lut, sample);
	}
	return sample;
}

/* Get interpolated sample - 16-bit (selects method based on interpMode) */
static inline int32_t getScopeSample16(scope_t *s, int32_t pos, uint64_t frac, uint8_t interpMode, int16_t *lut)
{
	int32_t sample;
	if (interpMode == INTERP_DISABLED)
	{
		SCOPE_SMP16_NEAREST(s, pos, sample);
	}
	else if (interpMode == INTERP_LINEAR)
	{
		SCOPE_SMP16_LINEAR(s, pos, frac, sample);
	}
	else
	{
		SCOPE_SMP16_CUBIC(s, pos, frac, lut, sample);
	}
	return sample;
}


/* --------------------------------------------------------------------- */
/*                     PERIOD TO SCOPE DELTA                             */
/* --------------------------------------------------------------------- */

uint64_t ft2_period_to_scope_delta(ft2_instance_t *inst, uint32_t period)
{
	period &= 0xFFFF;
	
	if (period == 0)
		return 0;
	
	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		const uint32_t quotient = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);
		return inst->replayer.scopeLogTab[remainder] >> ((14 - quotient) & 31);
	}
	else
	{
		return inst->replayer.scopeAmigaPeriodDiv / period;
	}
}

uint64_t ft2_period_to_scope_draw_delta(ft2_instance_t *inst, uint32_t period)
{
	period &= 0xFFFF;
	
	if (period == 0)
		return 0;
	
	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		const uint32_t quotient = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);
		return inst->replayer.scopeDrawLogTab[remainder] >> ((14 - quotient) & 31);
	}
	else
	{
		return inst->replayer.scopeDrawAmigaPeriodDiv / period;
	}
}

/* --------------------------------------------------------------------- */
/*                         SCOPE LINE DRAWING                            */
/* --------------------------------------------------------------------- */

static void scopeLine(ft2_video_t *video, int32_t x1, int32_t y1, int32_t y2, const uint32_t color)
{
	if (x1 < 0 || x1 >= SCREEN_W || y1 < 0 || y1 >= SCREEN_H || y2 < 0 || y2 >= SCREEN_H)
		return;

	uint32_t *dst32 = &video->frameBuffer[(y1 * SCREEN_W) + x1];
	*dst32 = color;

	const int32_t dy = y2 - y1;
	if (dy == 0)
	{
		dst32[1] = color;
		return;
	}

	int32_t ay = dy < 0 ? -dy : dy;
	int32_t d = 1 - ay;
	ay <<= 1;

	if (y1 > y2)
	{
		for (; y1 != y2; y1--)
		{
			if (d >= 0)
			{
				d -= ay;
				dst32++;
			}
			d += 2;
			dst32 -= SCREEN_W;
			*dst32 = color;
		}
	}
	else
	{
		for (; y1 != y2; y1++)
		{
			if (d >= 0)
			{
				d -= ay;
				dst32++;
			}
			d += 2;
			dst32 += SCREEN_W;
			*dst32 = color;
		}
	}
}

/* --------------------------------------------------------------------- */
/*                      NON-LINED SCOPE ROUTINES                         */
/* --------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------- */
/*                        LINED SCOPE ROUTINES                           */
/* --------------------------------------------------------------------- */

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

/* Scope draw routine table */
static const scopeDrawRoutine scopeDrawRoutineTable[12] =
{
	scopeDrawNoLoop_8bit,
	scopeDrawLoop_8bit,
	scopeDrawBidiLoop_8bit,
	scopeDrawNoLoop_16bit,
	scopeDrawLoop_16bit,
	scopeDrawBidiLoop_16bit,
	linedScopeDrawNoLoop_8bit,
	linedScopeDrawLoop_8bit,
	linedScopeDrawBidiLoop_8bit,
	linedScopeDrawNoLoop_16bit,
	linedScopeDrawLoop_16bit,
	linedScopeDrawBidiLoop_16bit
};

/* --------------------------------------------------------------------- */
/*                        SCOPE HELPER FUNCTIONS                         */
/* --------------------------------------------------------------------- */

static void drawScopeNumber(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t chNr, bool outline)
{
	x++;
	y++;
	chNr++;

	if (outline)
	{
		if (chNr < 10)
		{
			charOutOutlined(video, bmp, x, y, PAL_MOUSEPT, '0' + chNr);
		}
		else
		{
			charOutOutlined(video, bmp, x, y, PAL_MOUSEPT, '0' + (chNr / 10));
			charOutOutlined(video, bmp, x + 7, y, PAL_MOUSEPT, '0' + (chNr % 10));
		}
	}
	else
	{
		if (chNr < 10)
		{
			charOut(video, bmp, x, y, PAL_MOUSEPT, '0' + chNr);
		}
		else
		{
			charOut(video, bmp, x, y, PAL_MOUSEPT, '0' + (chNr / 10));
			charOut(video, bmp, x + 7, y, PAL_MOUSEPT, '0' + (chNr % 10));
		}
	}
}

static void redrawScope(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp, int32_t ch)
{
	int32_t chansPerRow = (uint32_t)scopes->numChannels >> 1;
	int32_t chanLookup = chansPerRow - 1;
	if (chanLookup < 0 || chanLookup >= 16)
		return;

	const uint16_t *scopeLens = scopeLenTab[chanLookup];

	uint16_t x = 2;
	uint16_t y = 94;
	uint16_t scopeLen = 0;

	for (int32_t i = 0; i < scopes->numChannels; i++)
	{
		scopeLen = scopeLens[i];
		if (i == chansPerRow)
		{
			x = 2;
			y += 39;
		}
		if (i == ch)
			break;
		x += scopeLen + 3;
	}

	drawFramework(video, x, y, scopeLen + 2, 38, FRAMEWORK_TYPE2);

	if (scopes->channelMuted[ch])
	{
		const uint16_t muteGfxLen = scopeMuteBMP_Widths[chanLookup];
		const uint16_t muteGfxX = x + ((scopeLen - muteGfxLen) >> 1);

		if (bmp && bmp->scopeMute)
		{
			blitFastClipX(video, muteGfxX, y + 6,
			              bmp->scopeMute + scopeMuteBMP_Offs[chanLookup],
			              162, scopeMuteBMP_Heights[chanLookup], muteGfxLen);
		}

		if (scopes->ptnChnNumbers)
			drawScopeNumber(video, bmp, x + 1, y + 1, (uint8_t)ch, true);
	}

	scopes->scopes[ch].wasCleared = false;
}

/* --------------------------------------------------------------------- */
/*                          PUBLIC FUNCTIONS                             */
/* --------------------------------------------------------------------- */

void ft2_scopes_init(ft2_scopes_t *scopes)
{
	if (scopes == NULL)
		return;

	memset(scopes, 0, sizeof(ft2_scopes_t));
	scopes->numChannels = 8;
	scopes->linedScopes = true;
	scopes->ptnChnNumbers = true;
	scopes->lastUpdateTick = 0; /* Will be set on first update */

	calcScopeIntrpLUT(scopes);
}

void ft2_scopes_free(ft2_scopes_t *scopes)
{
	if (scopes == NULL)
		return;

	if (scopes->scopeIntrpLUT != NULL)
	{
		free(scopes->scopeIntrpLUT);
		scopes->scopeIntrpLUT = NULL;
	}
}

/* Trigger scope from sync entry (like standalone scopeTrigger) */
static void scopeTriggerFromEntry(scope_t *s, const ft2_scope_sync_entry_t *entry)
{
	if (entry->base8 == NULL && entry->base16 == NULL)
	{
		s->active = false;
		return;
	}

	if (entry->length < 1)
		{
			s->active = false;
		return;
		}
		
	uint8_t loopType = entry->loopType;
	if (entry->loopLength < 1)
		loopType = LOOP_OFF;

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

	if (s->position >= s->sampleEnd)
		{
			s->active = false;
		return;
		}

	s->active = true;
	}

/* Update position for one scope (one 64 Hz tick) */
static void updateScopePosition(scope_t *s)
	{
		if (!s->active)
		return;

		s->positionFrac += s->delta;
		s->position += s->positionFrac >> SCOPE_FRAC_BITS;
		s->positionFrac &= SCOPE_FRAC_MASK;

		if (s->position >= s->sampleEnd)
		{
			if (s->loopType == LOOP_BIDI)
			{
				if (s->loopLength >= 2)
				{
					const uint32_t overflow = s->position - s->sampleEnd;
					const uint32_t cycles = overflow / s->loopLength;
					const uint32_t phase = overflow % s->loopLength;
					s->position = s->loopStart + phase;
					s->samplingBackwards ^= !(cycles & 1);
				}
				else
				{
					s->position = s->loopStart;
				}
				s->hasLooped = true;
			}
			else if (s->loopType == LOOP_FORWARD)
			{
				if (s->loopLength >= 2)
					s->position = s->loopStart + ((s->position - s->sampleEnd) % s->loopLength);
				else
					s->position = s->loopStart;
				s->hasLooped = true;
			}
			else
			{
				s->active = false;
		}
	}
}

void ft2_scopes_update(ft2_scopes_t *scopes, struct ft2_instance_t *inst)
{
	if (scopes == NULL || inst == NULL)
		return;

	/* Check if scopes clear was requested (matches standalone stopVoices -> stopAllScopes) */
	if (inst->scopesClearRequested)
	{
		inst->scopesClearRequested = false;
		for (int32_t i = 0; i < MAX_CHANNELS; i++)
			scopes->scopes[i].active = false;
	}

	/* Sync config settings */
	scopes->linedScopes = inst->config.linedScopes;
	scopes->interpolation = inst->audio.interpolationType;
	scopes->ptnChnNumbers = inst->config.ptnChnNumbers;

	/* Synchronize scope channel count with song */
	uint8_t songChannels = inst->replayer.song.numChannels;
	if (songChannels < 2) songChannels = 2;
	if (songChannels > 32) songChannels = 32;
	
	if (scopes->numChannels != songChannels)
	{
		scopes->numChannels = songChannels;
		scopes->needsFrameworkRedraw = true;
	}

	/* Process sync queue entries from audio thread (like handleScopesFromChQueue) */
	ft2_scope_sync_entry_t entry;
	while (ft2_scope_sync_queue_pop(inst, &entry))
	{
		if (entry.channel >= MAX_CHANNELS)
			continue;

		scope_t *s = &scopes->scopes[entry.channel];

		if (entry.status & FT2_SCOPE_UPDATE_VOL)
			s->volume = entry.scopeVolume;

		if (entry.status & FT2_SCOPE_UPDATE_PERIOD)
	{
			s->delta = ft2_period_to_scope_delta(inst, entry.period);
			s->drawDelta = ft2_period_to_scope_draw_delta(inst, entry.period);
		}

		if (entry.status & FT2_SCOPE_TRIGGER_VOICE)
			scopeTriggerFromEntry(s, &entry);
	}

	/* Update scope positions at 64 Hz rate (like standalone scope thread).
	** Calculate how many 64 Hz ticks have elapsed and run that many updates.
	** This ensures scopes advance at the correct rate regardless of UI frame rate. */
	uint64_t currentTick = getTickNs();
	
	/* Initialize lastUpdateTick if this is first update */
	if (scopes->lastUpdateTick == 0)
		scopes->lastUpdateTick = currentTick;

	/* Calculate elapsed ticks (capped to avoid spiral on long pauses) */
	uint64_t elapsed = currentTick - scopes->lastUpdateTick;
	int32_t ticksToRun = (int32_t)(elapsed / NS_PER_SCOPE_TICK);
	if (ticksToRun > 8)
		ticksToRun = 8; /* Cap at 8 ticks (~125ms) to avoid lag spiral */

	if (ticksToRun > 0)
	{
		scopes->lastUpdateTick += (uint64_t)ticksToRun * NS_PER_SCOPE_TICK;

		/* Run position updates for each tick */
		for (int32_t t = 0; t < ticksToRun; t++)
		{
			for (int32_t i = 0; i < scopes->numChannels; i++)
				updateScopePosition(&scopes->scopes[i]);
		}
	}
}

void ft2_scope_stop(ft2_scopes_t *scopes, int channel)
{
	if (scopes == NULL || channel < 0 || channel >= MAX_CHANNELS)
		return;

	scopes->scopes[channel].active = false;
}

void ft2_scopes_stop_all(ft2_scopes_t *scopes)
{
	if (scopes == NULL)
		return;

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		scopes->scopes[i].active = false;
}

void ft2_scopes_draw(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (scopes == NULL || video == NULL)
		return;

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
	if (scopes == NULL || video == NULL)
		return;

	drawFramework(video, 0, 92, 291, 81, FRAMEWORK_TYPE1);

	/* Redraw each scope */
	for (int32_t i = 0; i < scopes->numChannels; i++)
		redrawScope(scopes, video, bmp, i);
}

bool ft2_scopes_mouse_down(ft2_scopes_t *scopes, ft2_video_t *video, const ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, bool leftButton, bool rightButton)
{
	if (scopes == NULL)
		return false;

	if (mouseY >= 95 && mouseY <= 169 && mouseX >= 3 && mouseX <= 288)
	{
		if (mouseY > 130 && mouseY < 134)
			return true;

		int32_t chansPerRow = (uint32_t)scopes->numChannels >> 1;
		if (chansPerRow < 1)
			chansPerRow = 1;

		int32_t chanLookup = chansPerRow - 1;
		if (chanLookup >= 16)
			chanLookup = 15;

		const uint16_t *scopeLens = scopeLenTab[chanLookup];

		uint16_t x = 3;
		int32_t i;
		for (i = 0; i < chansPerRow; i++)
		{
			if (mouseX >= x && mouseX < x + scopeLens[i])
				break;
			x += scopeLens[i] + 3;
		}

		if (i == chansPerRow)
			return true;

		int32_t chanToToggle = i;
		if (mouseY >= 134)
			chanToToggle += chansPerRow;

		if (chanToToggle >= scopes->numChannels)
			return true;

		/* Toggle mute */
		if (leftButton && rightButton)
		{
			/* Solo mode */
			bool test = false;
			for (int32_t j = 0; j < scopes->numChannels; j++)
			{
				if (j != chanToToggle && scopes->channelMuted[j])
					test = true;
			}

			if (test)
			{
				for (int32_t j = 0; j < scopes->numChannels; j++)
					scopes->channelMuted[j] = false;
			}
			else
			{
				for (int32_t j = 0; j < scopes->numChannels; j++)
					scopes->channelMuted[j] = (j != chanToToggle);
			}

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
			/* Force scope redraw to update REC indicator */
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

	return false;
}

void ft2_scopes_set_mute(ft2_scopes_t *scopes, int channel, bool muted)
{
	if (scopes == NULL || channel < 0 || channel >= MAX_CHANNELS)
		return;
	scopes->channelMuted[channel] = muted;
}

bool ft2_scopes_get_mute(ft2_scopes_t *scopes, int channel)
{
	if (scopes == NULL || channel < 0 || channel >= MAX_CHANNELS)
		return false;
	return scopes->channelMuted[channel];
}
