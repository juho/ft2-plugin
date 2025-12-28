/**
 * Sample effects module for FT2 plugin.
 * Ported from standalone ft2_smpfx.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_filter_panel.h"
#include "ft2_plugin_replayer.h"
#include "../ft2_instance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RESONANCE_RANGE 99
#define RESONANCE_MIN 0.01

enum
{
	REMOVE_SAMPLE_MARK = 0,
	KEEP_SAMPLE_MARK   = 1
};

enum
{
	FILTER_LOWPASS  = 0,
	FILTER_HIGHPASS = 1
};

typedef struct
{
	double a1, a2, a3, b1, b2;
	double inTmp[2], outTmp[2];
} resoFilter_t;

typedef struct
{
	bool filled, keepSampleMark;
	uint8_t flags, undoInstr, undoSmp;
	uint32_t length, loopStart, loopLength;
	int8_t *smpData8;
	int16_t *smpData16;
} sampleUndo_t;

static sampleUndo_t sampleUndo;
static bool normalization = false;
static uint8_t lastFilterType = FILTER_LOWPASS;
static int32_t lastLpCutoff = 2000, lastHpCutoff = 200, filterResonance = 0;
static int32_t smpCycles = 1, lastWaveLength = 64, lastAmp = 75;

static ft2_sample_t *getSmpFxCurSample(ft2_instance_t *inst)
{
	if (inst == NULL || inst->editor.curInstr == 0 || inst->editor.curInstr >= 128)
		return NULL;

	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (instr == NULL || inst->editor.curSmp >= 16)
		return NULL;

	return &instr->smp[inst->editor.curSmp];
}

/* Get the sample editor range (x1, x2). If no range selected, returns whole sample. */
static void getSmpFxRange(ft2_sample_t *s, int32_t *x1, int32_t *x2)
{
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	
	if (ed != NULL && ed->hasRange && ed->rangeEnd > ed->rangeStart)
	{
		*x1 = ed->rangeStart;
		*x2 = ed->rangeEnd;
		
		/* Clamp to sample bounds */
		if (*x1 < 0) *x1 = 0;
		if (*x2 > s->length) *x2 = s->length;
		if (*x2 <= *x1)
		{
			/* Fallback to whole sample */
			*x1 = 0;
			*x2 = s->length;
		}
	}
	else
	{
		/* No range selected - use whole sample */
		*x1 = 0;
		*x2 = s->length;
	}
}

void clearSampleUndo(ft2_instance_t *inst)
{
	(void)inst;
	
	if (sampleUndo.smpData8 != NULL)
	{
		free(sampleUndo.smpData8);
		sampleUndo.smpData8 = NULL;
	}

	if (sampleUndo.smpData16 != NULL)
	{
		free(sampleUndo.smpData16);
		sampleUndo.smpData16 = NULL;
	}

	sampleUndo.filled = false;
	sampleUndo.keepSampleMark = false;
}

void fillSampleUndo(ft2_instance_t *inst, bool keepSampleMark)
{
	sampleUndo.filled = false;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s != NULL && s->length > 0 && s->dataPtr != NULL)
	{
		ft2_unfix_sample(s);

		clearSampleUndo(inst);

		sampleUndo.undoInstr = inst->editor.curInstr;
		sampleUndo.undoSmp = inst->editor.curSmp;
		sampleUndo.flags = s->flags;
		sampleUndo.length = s->length;
		sampleUndo.loopStart = s->loopStart;
		sampleUndo.loopLength = s->loopLength;
		sampleUndo.keepSampleMark = keepSampleMark;

		if (s->flags & SAMPLE_16BIT)
		{
			sampleUndo.smpData16 = (int16_t *)malloc(s->length * sizeof(int16_t));
			if (sampleUndo.smpData16 != NULL)
			{
				memcpy(sampleUndo.smpData16, s->dataPtr, s->length * sizeof(int16_t));
				sampleUndo.filled = true;
			}
		}
		else
		{
			sampleUndo.smpData8 = (int8_t *)malloc(s->length * sizeof(int8_t));
			if (sampleUndo.smpData8 != NULL)
			{
				memcpy(sampleUndo.smpData8, s->dataPtr, s->length * sizeof(int8_t));
				sampleUndo.filled = true;
			}
		}

		ft2_fix_sample(s);
	}
}

static ft2_sample_t *setupNewSample(ft2_instance_t *inst, uint32_t length)
{
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (instr == NULL)
	{
		/* Allocate instrument inline */
		instr = (ft2_instr_t *)calloc(1, sizeof(ft2_instr_t));
		if (instr == NULL)
			return NULL;
		inst->replayer.instr[inst->editor.curInstr] = instr;
	}

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];

	/* Stop voices playing this sample before replacing it */
	ft2_stop_sample_voices(inst, s);

	/* Allocate new sample data */
	if (!allocateSmpData(inst, inst->editor.curInstr, inst->editor.curSmp, length, true))
		return NULL;

	s->isFixed = false;
	s->length = length;
	s->loopLength = s->loopStart = 0;
	s->flags = SAMPLE_16BIT;

	return s;
}

void cbSfxNormalization(ft2_instance_t *inst)
{
	(void)inst;
	normalization = !normalization;
}

bool getSfxNormalization(ft2_instance_t *inst)
{
	(void)inst;
	return normalization;
}

int32_t getSfxCycles(ft2_instance_t *inst)
{
	(void)inst;
	return smpCycles;
}

int32_t getSfxResonance(ft2_instance_t *inst)
{
	(void)inst;
	return filterResonance;
}

void pbSfxCyclesUp(ft2_instance_t *inst)
{
	if (smpCycles < 256)
	{
		smpCycles++;
		inst->uiState.updateSampleEditor = true;
	}
}

void pbSfxCyclesDown(ft2_instance_t *inst)
{
	if (smpCycles > 1)
	{
		smpCycles--;
		inst->uiState.updateSampleEditor = true;
	}
}

/* Internal wave generation functions (called after dialog confirms) */
static void generateTriangle(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1)
		return;

	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;

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

	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;

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

	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;

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

	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	uint32_t newLength = lastWaveLength * smpCycles;

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

void pbSfxTriangle(ft2_instance_t *inst)
{
	ft2_wave_panel_show(inst, WAVE_TYPE_TRIANGLE);
}

void pbSfxSaw(ft2_instance_t *inst)
{
	ft2_wave_panel_show(inst, WAVE_TYPE_SAW);
}

void pbSfxSine(ft2_instance_t *inst)
{
	ft2_wave_panel_show(inst, WAVE_TYPE_SINE);
}

void pbSfxSquare(ft2_instance_t *inst)
{
	ft2_wave_panel_show(inst, WAVE_TYPE_SQUARE);
}

void pbSfxResoUp(ft2_instance_t *inst)
{
	if (filterResonance < RESONANCE_RANGE)
	{
		filterResonance++;
		inst->uiState.updateSampleEditor = true;
	}
}

void pbSfxResoDown(ft2_instance_t *inst)
{
	if (filterResonance > 0)
	{
		filterResonance--;
		inst->uiState.updateSampleEditor = true;
	}
}

static double getSampleC4Rate(ft2_sample_t *s)
{
	int32_t relTone = s->relativeNote;
	int32_t fineTune = s->finetune;

	double period = (10 * 12 * 16 * 4) - (relTone * 16 * 4) - (fineTune / 2);
	return 8363.0 * pow(2.0, (4608.0 - period) / 768.0);
}

#define CUTOFF_EPSILON (1E-4)

static void setupResoLpFilter(ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq / 2.0)
			cutoff = (sampleFreq / 2.0) - CUTOFF_EPSILON;

		cutoff /= sampleFreq;
	}

	double r = sqrt(2.0);
	if (resonance > 0)
	{
		r = pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0));
		if (r < RESONANCE_MIN)
			r = RESONANCE_MIN;
	}

	const double c = 1.0 / tan(M_PI * cutoff);

	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = 2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (1.0 - c * c) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;

	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0;
}

static void setupResoHpFilter(ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq / 2.0)
			cutoff = (sampleFreq / 2.0) - CUTOFF_EPSILON;

		cutoff /= sampleFreq;
	}

	double r = sqrt(2.0);
	if (resonance > 0)
	{
		r = pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0));
		if (r < RESONANCE_MIN)
			r = RESONANCE_MIN;
	}

	const double c = tan(M_PI * cutoff);

	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = -2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (c * c - 1.0) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;

	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0;
}

static bool applyResoFilter(ft2_instance_t *inst, ft2_sample_t *s, resoFilter_t *f, int32_t x1, int32_t x2)
{
	/* Clamp range to sample bounds */
	if (x1 < 0) x1 = 0;
	if (x2 > s->length) x2 = s->length;
	if (x2 <= x1) return true;
	
	const int32_t len = x2 - x1;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	if (!normalization)
	{
		ft2_unfix_sample(s);

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f->a1 * ptr16[i]) + (f->a2 * f->inTmp[0]) + (f->a3 * f->inTmp[1]) - (f->b1 * f->outTmp[0]) - (f->b2 * f->outTmp[1]);

				f->inTmp[1] = f->inTmp[0];
				f->inTmp[0] = ptr16[i];

				f->outTmp[1] = f->outTmp[0];
				f->outTmp[0] = out;

				if (out < INT16_MIN) out = INT16_MIN;
				if (out > INT16_MAX) out = INT16_MAX;
				ptr16[i] = (int16_t)out;
			}
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f->a1 * ptr8[i]) + (f->a2 * f->inTmp[0]) + (f->a3 * f->inTmp[1]) - (f->b1 * f->outTmp[0]) - (f->b2 * f->outTmp[1]);

				f->inTmp[1] = f->inTmp[0];
				f->inTmp[0] = ptr8[i];

				f->outTmp[1] = f->outTmp[0];
				f->outTmp[0] = out;

				if (out < INT8_MIN) out = INT8_MIN;
				if (out > INT8_MAX) out = INT8_MAX;
				ptr8[i] = (int8_t)out;
			}
		}

		ft2_fix_sample(s);
	}
	else
	{
		double *dSmp = (double *)malloc(len * sizeof(double));
		if (dSmp == NULL)
			return false;

		ft2_unfix_sample(s);

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				dSmp[i] = (double)ptr16[i];
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				dSmp[i] = (double)ptr8[i];
		}

		double peak = 0.0;
		for (int32_t i = 0; i < len; i++)
		{
			const double out = (f->a1 * dSmp[i]) + (f->a2 * f->inTmp[0]) + (f->a3 * f->inTmp[1]) - (f->b1 * f->outTmp[0]) - (f->b2 * f->outTmp[1]);

			f->inTmp[1] = f->inTmp[0];
			f->inTmp[0] = dSmp[i];

			f->outTmp[1] = f->outTmp[0];
			f->outTmp[0] = out;

			dSmp[i] = out;

			const double outAbs = fabs(out);
			if (outAbs > peak)
				peak = outAbs;
		}

		if (peak > 0.0)
		{
			if (s->flags & SAMPLE_16BIT)
			{
				const double scale = INT16_MAX / peak;

				int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
				for (int32_t i = 0; i < len; i++)
					ptr16[i] = (int16_t)(dSmp[i] * scale);
			}
			else
			{
				const double scale = INT8_MAX / peak;

				int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
				for (int32_t i = 0; i < len; i++)
					ptr8[i] = (int8_t)(dSmp[i] * scale);
			}
		}

		free(dSmp);
		ft2_fix_sample(s);
	}

	return true;
}

/* Internal filter apply functions (called after dialog confirms) */
static void applyLowPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	lastFilterType = FILTER_LOWPASS;
	lastLpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	
	setupResoLpFilter(s, &f, cutoff, filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);

	inst->uiState.updateSampleEditor = true;
}

static void applyHighPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	lastFilterType = FILTER_HIGHPASS;
	lastHpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	
	setupResoHpFilter(s, &f, cutoff, filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxLowPass(ft2_instance_t *inst)
{
	ft2_filter_panel_show(inst, FILTER_TYPE_LOWPASS);
}

void pbSfxHighPass(ft2_instance_t *inst)
{
	ft2_filter_panel_show(inst, FILTER_TYPE_HIGHPASS);
}

void smpfx_apply_filter(ft2_instance_t *inst, int filterType, int32_t cutoff)
{
	if (filterType == 0)
		applyLowPassFilter(inst, cutoff);
	else
		applyHighPassFilter(inst, cutoff);
}

void pbSfxSubBass(ft2_instance_t *inst)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	
	setupResoHpFilter(s, &f, 0.001, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxAddBass(ft2_instance_t *inst)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	setupResoLpFilter(s, &f, 0.015, 0, true);

	double *dSmp = (double *)malloc(len * sizeof(double));
	if (dSmp == NULL)
		return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr16[i];
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr8[i];
	}

	for (int32_t i = 0; i < len; i++)
	{
		double out = (f.a1 * dSmp[i]) + (f.a2 * f.inTmp[0]) + (f.a3 * f.inTmp[1]) - (f.b1 * f.outTmp[0]) - (f.b2 * f.outTmp[1]);

		f.inTmp[1] = f.inTmp[0];
		f.inTmp[0] = dSmp[i];

		f.outTmp[1] = f.outTmp[0];
		f.outTmp[0] = out;

		dSmp[i] = out;
	}

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr16[i] + (dSmp[i] * 0.25);
			if (out < INT16_MIN) out = INT16_MIN;
			if (out > INT16_MAX) out = INT16_MAX;
			ptr16[i] = (int16_t)out;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr8[i] + (dSmp[i] * 0.25);
			if (out < INT8_MIN) out = INT8_MIN;
			if (out > INT8_MAX) out = INT8_MAX;
			ptr8[i] = (int8_t)out;
		}
	}

	free(dSmp);
	ft2_fix_sample(s);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxSubTreble(ft2_instance_t *inst)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	
	setupResoLpFilter(s, &f, 0.33, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxAddTreble(ft2_instance_t *inst)
{
	resoFilter_t f;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	setupResoHpFilter(s, &f, 0.27, 0, true);

	double *dSmp = (double *)malloc(len * sizeof(double));
	if (dSmp == NULL)
		return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr16[i];
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr8[i];
	}

	for (int32_t i = 0; i < len; i++)
	{
		double out = (f.a1 * dSmp[i]) + (f.a2 * f.inTmp[0]) + (f.a3 * f.inTmp[1]) - (f.b1 * f.outTmp[0]) - (f.b2 * f.outTmp[1]);

		f.inTmp[1] = f.inTmp[0];
		f.inTmp[0] = dSmp[i];

		f.outTmp[1] = f.outTmp[0];
		f.outTmp[0] = out;

		dSmp[i] = out;
	}

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr16[i] - (dSmp[i] * 0.25);
			if (out < INT16_MIN) out = INT16_MIN;
			if (out > INT16_MAX) out = INT16_MAX;
			ptr16[i] = (int16_t)out;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr8[i] - (dSmp[i] * 0.25);
			if (out < INT8_MIN) out = INT8_MIN;
			if (out > INT8_MAX) out = INT8_MAX;
			ptr8[i] = (int8_t)out;
		}
	}

	free(dSmp);
	ft2_fix_sample(s);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxSetAmp(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	getSmpFxRange(s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	const int32_t mul = (int32_t)round((1 << 22UL) * (lastAmp / 100.0));

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr16[i] * (int32_t)mul) >> 22;
			if (sample < INT16_MIN) sample = INT16_MIN;
			if (sample > INT16_MAX) sample = INT16_MAX;
			ptr16[i] = (int16_t)sample;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr8[i] * (int32_t)mul) >> 22;
			if (sample < INT8_MIN) sample = INT8_MIN;
			if (sample > INT8_MAX) sample = INT8_MAX;
			ptr8[i] = (int8_t)sample;
		}
	}

	ft2_fix_sample(s);

	inst->uiState.updateSampleEditor = true;
}

void pbSfxUndo(ft2_instance_t *inst)
{
	if (!sampleUndo.filled || sampleUndo.undoInstr != inst->editor.curInstr || sampleUndo.undoSmp != inst->editor.curSmp)
		return;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (s == NULL)
		return;

	/* Stop voices playing this sample before restoring */
	ft2_stop_sample_voices(inst, s);

	freeSmpData(inst, inst->editor.curInstr, inst->editor.curSmp);
	s->flags = sampleUndo.flags;
	s->length = sampleUndo.length;
	s->loopStart = sampleUndo.loopStart;
	s->loopLength = sampleUndo.loopLength;

	if (allocateSmpData(inst, inst->editor.curInstr, inst->editor.curSmp, s->length, !!(s->flags & SAMPLE_16BIT)))
	{
		if (s->flags & SAMPLE_16BIT)
			memcpy(s->dataPtr, sampleUndo.smpData16, s->length * sizeof(int16_t));
		else
			memcpy(s->dataPtr, sampleUndo.smpData8, s->length * sizeof(int8_t));

		ft2_fix_sample(s);
	}

	sampleUndo.keepSampleMark = false;
	sampleUndo.filled = false;

	inst->uiState.updateSampleEditor = true;
}

void showSampleEffectsScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.sampleEditorEffectsShown = true;

	pushButtons[PB_SAMP_PNOTE_UP].visible = false;
	pushButtons[PB_SAMP_PNOTE_DOWN].visible = false;
	pushButtons[PB_SAMP_STOP].visible = false;
	pushButtons[PB_SAMP_PWAVE].visible = false;
	pushButtons[PB_SAMP_PRANGE].visible = false;
	pushButtons[PB_SAMP_PDISPLAY].visible = false;
	pushButtons[PB_SAMP_SHOW_RANGE].visible = false;
	pushButtons[PB_SAMP_RANGE_ALL].visible = false;
	pushButtons[PB_SAMP_CLR_RANGE].visible = false;
	pushButtons[PB_SAMP_ZOOM_OUT].visible = false;
	pushButtons[PB_SAMP_SHOW_ALL].visible = false;
	pushButtons[PB_SAMP_SAVE_RNG].visible = false;
	pushButtons[PB_SAMP_CUT].visible = false;
	pushButtons[PB_SAMP_COPY].visible = false;
	pushButtons[PB_SAMP_PASTE].visible = false;
	pushButtons[PB_SAMP_CROP].visible = false;
	pushButtons[PB_SAMP_VOLUME].visible = false;
	pushButtons[PB_SAMP_EFFECTS].visible = false;

	checkBoxes[CB_SAMPFX_NORM].checked = normalization;
	checkBoxes[CB_SAMPFX_NORM].visible = true;

	pushButtons[PB_SAMPFX_CYCLES_UP].visible = true;
	pushButtons[PB_SAMPFX_CYCLES_DOWN].visible = true;
	pushButtons[PB_SAMPFX_TRIANGLE].visible = true;
	pushButtons[PB_SAMPFX_SAW].visible = true;
	pushButtons[PB_SAMPFX_SINE].visible = true;
	pushButtons[PB_SAMPFX_SQUARE].visible = true;
	pushButtons[PB_SAMPFX_RESO_UP].visible = true;
	pushButtons[PB_SAMPFX_RESO_DOWN].visible = true;
	pushButtons[PB_SAMPFX_LOWPASS].visible = true;
	pushButtons[PB_SAMPFX_HIGHPASS].visible = true;
	pushButtons[PB_SAMPFX_SUB_BASS].visible = true;
	pushButtons[PB_SAMPFX_ADD_BASS].visible = true;
	pushButtons[PB_SAMPFX_SUB_TREBLE].visible = true;
	pushButtons[PB_SAMPFX_ADD_TREBLE].visible = true;
	pushButtons[PB_SAMPFX_SET_AMP].visible = true;
	pushButtons[PB_SAMPFX_UNDO].visible = true;
	pushButtons[PB_SAMPFX_XFADE].visible = true;
	pushButtons[PB_SAMPFX_BACK].visible = true;

	inst->uiState.needsFullRedraw = true;
}

void hideSampleEffectsScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.sampleEditorEffectsShown = false;

	checkBoxes[CB_SAMPFX_NORM].visible = false;

	pushButtons[PB_SAMPFX_CYCLES_UP].visible = false;
	pushButtons[PB_SAMPFX_CYCLES_DOWN].visible = false;
	pushButtons[PB_SAMPFX_TRIANGLE].visible = false;
	pushButtons[PB_SAMPFX_SAW].visible = false;
	pushButtons[PB_SAMPFX_SINE].visible = false;
	pushButtons[PB_SAMPFX_SQUARE].visible = false;
	pushButtons[PB_SAMPFX_RESO_UP].visible = false;
	pushButtons[PB_SAMPFX_RESO_DOWN].visible = false;
	pushButtons[PB_SAMPFX_LOWPASS].visible = false;
	pushButtons[PB_SAMPFX_HIGHPASS].visible = false;
	pushButtons[PB_SAMPFX_SUB_BASS].visible = false;
	pushButtons[PB_SAMPFX_ADD_BASS].visible = false;
	pushButtons[PB_SAMPFX_SUB_TREBLE].visible = false;
	pushButtons[PB_SAMPFX_ADD_TREBLE].visible = false;
	pushButtons[PB_SAMPFX_SET_AMP].visible = false;
	pushButtons[PB_SAMPFX_UNDO].visible = false;
	pushButtons[PB_SAMPFX_XFADE].visible = false;
	pushButtons[PB_SAMPFX_BACK].visible = false;

	pushButtons[PB_SAMP_PNOTE_UP].visible = true;
	pushButtons[PB_SAMP_PNOTE_DOWN].visible = true;
	pushButtons[PB_SAMP_STOP].visible = true;
	pushButtons[PB_SAMP_PWAVE].visible = true;
	pushButtons[PB_SAMP_PRANGE].visible = true;
	pushButtons[PB_SAMP_PDISPLAY].visible = true;
	pushButtons[PB_SAMP_SHOW_RANGE].visible = true;
	pushButtons[PB_SAMP_RANGE_ALL].visible = true;
	pushButtons[PB_SAMP_CLR_RANGE].visible = true;
	pushButtons[PB_SAMP_ZOOM_OUT].visible = true;
	pushButtons[PB_SAMP_SHOW_ALL].visible = true;
	/* pushButtons[PB_SAMP_SAVE_RNG].visible = true; */ /* TODO: Implement save range */
	pushButtons[PB_SAMP_CUT].visible = true;
	pushButtons[PB_SAMP_COPY].visible = true;
	pushButtons[PB_SAMP_PASTE].visible = true;
	pushButtons[PB_SAMP_CROP].visible = true;
	pushButtons[PB_SAMP_VOLUME].visible = true;
	pushButtons[PB_SAMP_EFFECTS].visible = true;

	inst->uiState.needsFullRedraw = true;
}

void drawSampleEffectsScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	drawFramework(video, 0, 346, 116, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 116, 346, 114, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 230, 346, 67, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 297, 346, 56, 54, FRAMEWORK_TYPE1);

	textOutShadow(video, bmp, 4, 352, PAL_FORGRND, PAL_DSKTOP2, "Cycles:");

	char str[16];
	sprintf(str, "%03d", smpCycles);
	textOut(video, bmp, 54, 352, PAL_FORGRND, str);

	textOutShadow(video, bmp, 121, 352, PAL_FORGRND, PAL_DSKTOP2, "Reson.:");

	if (filterResonance <= 0)
	{
		textOut(video, bmp, 172, 352, PAL_FORGRND, "off");
	}
	else
	{
		sprintf(str, "%02d", filterResonance);
		textOut(video, bmp, 175, 352, PAL_FORGRND, str);
	}

	textOutShadow(video, bmp, 135, 386, PAL_FORGRND, PAL_DSKTOP2, "Normalization");

	textOutShadow(video, bmp, 235, 352, PAL_FORGRND, PAL_DSKTOP2, "Bass");
	textOutShadow(video, bmp, 235, 369, PAL_FORGRND, PAL_DSKTOP2, "Treb.");
}
