/*
** FT2 Plugin - Sample Effects
** Port of ft2_smpfx.c: wave generation, filters, EQ, amplitude.
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

enum { REMOVE_SAMPLE_MARK = 0, KEEP_SAMPLE_MARK = 1 };
enum { FILTER_LOWPASS = 0, FILTER_HIGHPASS = 1 };

/* 2nd-order IIR biquad filter state */
typedef struct {
	double a1, a2, a3, b1, b2;  /* Coefficients */
	double inTmp[2], outTmp[2]; /* Delay line */
} resoFilter_t;

#define UNDO_STATE(inst) (&FT2_SAMPLE_ED(inst)->undo)
static bool normalization = false;
static uint8_t lastFilterType = FILTER_LOWPASS;
static int32_t lastLpCutoff = 2000, lastHpCutoff = 200, filterResonance = 0;
static int32_t smpCycles = 1, lastWaveLength = 64, lastAmp = 75;

/* ------------------------------------------------------------------------- */
/*                           HELPERS                                         */
/* ------------------------------------------------------------------------- */

static ft2_sample_t *getSmpFxCurSample(ft2_instance_t *inst)
{
	if (!inst || inst->editor.curInstr == 0 || inst->editor.curInstr >= 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr || inst->editor.curSmp >= 16) return NULL;
	return &instr->smp[inst->editor.curSmp];
}

/* Gets sample editor range; if no selection, returns whole sample */
static void getSmpFxRange(ft2_instance_t *inst, ft2_sample_t *s, int32_t *x1, int32_t *x2)
{
	ft2_sample_editor_t *ed = (inst && inst->ui) ? FT2_SAMPLE_ED(inst) : NULL;

	if (ed && ed->hasRange && ed->rangeEnd > ed->rangeStart)
	{
		*x1 = (ed->rangeStart < 0) ? 0 : ed->rangeStart;
		*x2 = (ed->rangeEnd > s->length) ? s->length : ed->rangeEnd;
		if (*x2 <= *x1) { *x1 = 0; *x2 = s->length; }
	}
	else
	{
		*x1 = 0;
		*x2 = s->length;
	}
}

/* ------------------------------------------------------------------------- */
/*                            UNDO                                           */
/* ------------------------------------------------------------------------- */

void clearSampleUndo(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smp_undo_t *undo = UNDO_STATE(inst);
	if (undo->smpData8) { free(undo->smpData8); undo->smpData8 = NULL; }
	if (undo->smpData16) { free(undo->smpData16); undo->smpData16 = NULL; }
	undo->filled = false;
	undo->keepSampleMark = false;
}

void fillSampleUndo(ft2_instance_t *inst, bool keepSampleMark)
{
	if (!inst || !inst->ui) return;
	smp_undo_t *undo = UNDO_STATE(inst);
	undo->filled = false;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || s->length == 0 || !s->dataPtr) return;

	ft2_unfix_sample(s);
	clearSampleUndo(inst);

	undo->undoInstr = inst->editor.curInstr;
	undo->undoSmp = inst->editor.curSmp;
	undo->flags = s->flags;
	undo->length = s->length;
	undo->loopStart = s->loopStart;
	undo->loopLength = s->loopLength;
	undo->keepSampleMark = keepSampleMark;

	if (s->flags & SAMPLE_16BIT)
	{
		undo->smpData16 = (int16_t *)malloc(s->length * sizeof(int16_t));
		if (undo->smpData16) { memcpy(undo->smpData16, s->dataPtr, s->length * sizeof(int16_t)); undo->filled = true; }
	}
	else
	{
		undo->smpData8 = (int8_t *)malloc(s->length * sizeof(int8_t));
		if (undo->smpData8) { memcpy(undo->smpData8, s->dataPtr, s->length * sizeof(int8_t)); undo->filled = true; }
	}

	ft2_fix_sample(s);
}

/* Allocates new 16-bit sample, stopping any playing voices */
static ft2_sample_t *setupNewSample(ft2_instance_t *inst, uint32_t length)
{
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (!instr)
	{
		instr = (ft2_instr_t *)calloc(1, sizeof(ft2_instr_t));
		if (!instr) return NULL;
		inst->replayer.instr[inst->editor.curInstr] = instr;
	}

	ft2_sample_t *s = &instr->smp[inst->editor.curSmp];
	ft2_stop_sample_voices(inst, s);

	if (!allocateSmpData(inst, inst->editor.curInstr, inst->editor.curSmp, length, true))
		return NULL;

	s->isFixed = false;
	s->length = length;
	s->loopLength = s->loopStart = 0;
	s->flags = SAMPLE_16BIT;
	return s;
}

/* ------------------------------------------------------------------------- */
/*                         STATE ACCESSORS                                   */
/* ------------------------------------------------------------------------- */

void cbSfxNormalization(ft2_instance_t *inst) { (void)inst; normalization = !normalization; }
bool getSfxNormalization(ft2_instance_t *inst) { (void)inst; return normalization; }
int32_t getSfxCycles(ft2_instance_t *inst) { (void)inst; return smpCycles; }
int32_t getSfxResonance(ft2_instance_t *inst) { (void)inst; return filterResonance; }

void pbSfxCyclesUp(ft2_instance_t *inst)
{
	if (smpCycles < 256) { smpCycles++; inst->uiState.updateSampleEditor = true; }
}

void pbSfxCyclesDown(ft2_instance_t *inst)
{
	if (smpCycles > 1) { smpCycles--; inst->uiState.updateSampleEditor = true; }
}

/* ------------------------------------------------------------------------- */
/*                       WAVE GENERATION                                     */
/* ------------------------------------------------------------------------- */

static void generateTriangle(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double delta = 4.0 / lastWaveLength;
	double phase = 0.0;
	int16_t *ptr16 = (int16_t *)s->dataPtr;

	for (int32_t i = 0; i < newLength; i++)
	{
		double t = (phase > 3.0) ? phase - 4.0 : (phase >= 1.0) ? 2.0 - phase : phase;
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
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	uint64_t point64 = 0, delta64 = ((uint64_t)(INT16_MAX * 2) << 32ULL) / lastWaveLength;
	int16_t *ptr16 = (int16_t *)s->dataPtr;

	for (int32_t i = 0; i < newLength; i++) { *ptr16++ = (int16_t)(point64 >> 32); point64 += delta64; }

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

static void generateSine(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double dMul = (2.0 * M_PI) / lastWaveLength;
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++) *ptr16++ = (int16_t)(INT16_MAX * sin(i * dMul));

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

static void generateSquare(ft2_instance_t *inst)
{
	if (inst->editor.curInstr == 0 || lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	uint32_t newLength = lastWaveLength * smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const uint32_t halfWaveLength = lastWaveLength / 2;
	int16_t currValue = INT16_MAX;
	uint32_t counter = 0;
	int16_t *ptr16 = (int16_t *)s->dataPtr;

	for (uint32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = currValue;
		if (++counter >= halfWaveLength) { counter = 0; currValue = -currValue; }
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void pbSfxTriangle(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_TRIANGLE); }
void pbSfxSaw(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SAW); }
void pbSfxSine(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SINE); }
void pbSfxSquare(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SQUARE); }

/* ------------------------------------------------------------------------- */
/*                           FILTERS                                         */
/* ------------------------------------------------------------------------- */

void pbSfxResoUp(ft2_instance_t *inst) { if (filterResonance < RESONANCE_RANGE) { filterResonance++; inst->uiState.updateSampleEditor = true; } }
void pbSfxResoDown(ft2_instance_t *inst) { if (filterResonance > 0) { filterResonance--; inst->uiState.updateSampleEditor = true; } }

/* Calculates sample rate at C-4 from relativeNote and finetune */
static double getSampleC4Rate(ft2_sample_t *s)
{
	double period = (10 * 12 * 16 * 4) - (s->relativeNote * 16 * 4) - (s->finetune / 2);
	return 8363.0 * pow(2.0, (4608.0 - period) / 768.0);
}

#define CUTOFF_EPSILON (1E-4)

/* 2nd-order Butterworth lowpass with resonance (Q controlled by resonance param) */
static void setupResoLpFilter(ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq / 2.0) cutoff = (sampleFreq / 2.0) - CUTOFF_EPSILON;
		cutoff /= sampleFreq;
	}

	double r = (resonance > 0) ? pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0)) : sqrt(2.0);
	if (r < RESONANCE_MIN) r = RESONANCE_MIN;

	const double c = 1.0 / tan(M_PI * cutoff);
	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = 2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (1.0 - c * c) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;
	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0;
}

/* 2nd-order Butterworth highpass with resonance */
static void setupResoHpFilter(ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq / 2.0) cutoff = (sampleFreq / 2.0) - CUTOFF_EPSILON;
		cutoff /= sampleFreq;
	}

	double r = (resonance > 0) ? pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0)) : sqrt(2.0);
	if (r < RESONANCE_MIN) r = RESONANCE_MIN;

	const double c = tan(M_PI * cutoff);
	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = -2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (c * c - 1.0) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;
	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0;
}

/* Applies biquad filter to sample range; optionally normalizes output */
static bool applyResoFilter(ft2_instance_t *inst, ft2_sample_t *s, resoFilter_t *f, int32_t x1, int32_t x2)
{
	if (x1 < 0) x1 = 0;
	if (x2 > s->length) x2 = s->length;
	if (x2 <= x1) return true;

	const int32_t len = x2 - x1;
	ft2_stop_sample_voices(inst, s);

	if (!normalization)
	{
		ft2_unfix_sample(s);
		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = f->a1 * ptr16[i] + f->a2 * f->inTmp[0] + f->a3 * f->inTmp[1] - f->b1 * f->outTmp[0] - f->b2 * f->outTmp[1];
				f->inTmp[1] = f->inTmp[0]; f->inTmp[0] = ptr16[i];
				f->outTmp[1] = f->outTmp[0]; f->outTmp[0] = out;
				ptr16[i] = (out < INT16_MIN) ? INT16_MIN : (out > INT16_MAX) ? INT16_MAX : (int16_t)out;
			}
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = f->a1 * ptr8[i] + f->a2 * f->inTmp[0] + f->a3 * f->inTmp[1] - f->b1 * f->outTmp[0] - f->b2 * f->outTmp[1];
				f->inTmp[1] = f->inTmp[0]; f->inTmp[0] = ptr8[i];
				f->outTmp[1] = f->outTmp[0]; f->outTmp[0] = out;
				ptr8[i] = (out < INT8_MIN) ? INT8_MIN : (out > INT8_MAX) ? INT8_MAX : (int8_t)out;
			}
		}
		ft2_fix_sample(s);
	}
	else
	{
		double *dSmp = (double *)malloc(len * sizeof(double));
		if (!dSmp) return false;

		ft2_unfix_sample(s);

		if (s->flags & SAMPLE_16BIT)
			for (int32_t i = 0; i < len; i++) dSmp[i] = (double)((int16_t *)s->dataPtr + x1)[i];
		else
			for (int32_t i = 0; i < len; i++) dSmp[i] = (double)((int8_t *)s->dataPtr + x1)[i];

		double peak = 0.0;
		for (int32_t i = 0; i < len; i++)
		{
			double out = f->a1 * dSmp[i] + f->a2 * f->inTmp[0] + f->a3 * f->inTmp[1] - f->b1 * f->outTmp[0] - f->b2 * f->outTmp[1];
			f->inTmp[1] = f->inTmp[0]; f->inTmp[0] = dSmp[i];
			f->outTmp[1] = f->outTmp[0]; f->outTmp[0] = out;
			dSmp[i] = out;
			if (fabs(out) > peak) peak = fabs(out);
		}

		if (peak > 0.0)
		{
			double scale = (s->flags & SAMPLE_16BIT) ? INT16_MAX / peak : INT8_MAX / peak;
			if (s->flags & SAMPLE_16BIT)
				for (int32_t i = 0; i < len; i++) ((int16_t *)s->dataPtr + x1)[i] = (int16_t)(dSmp[i] * scale);
			else
				for (int32_t i = 0; i < len; i++) ((int8_t *)s->dataPtr + x1)[i] = (int8_t)(dSmp[i] * scale);
		}

		free(dSmp);
		ft2_fix_sample(s);
	}
	return true;
}

static void applyLowPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	lastFilterType = FILTER_LOWPASS;
	lastLpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoLpFilter(s, &f, cutoff, filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	inst->uiState.updateSampleEditor = true;
}

static void applyHighPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	lastFilterType = FILTER_HIGHPASS;
	lastHpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoHpFilter(s, &f, cutoff, filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	inst->uiState.updateSampleEditor = true;
}

void pbSfxLowPass(ft2_instance_t *inst) { ft2_filter_panel_show(inst, FILTER_TYPE_LOWPASS); }
void pbSfxHighPass(ft2_instance_t *inst) { ft2_filter_panel_show(inst, FILTER_TYPE_HIGHPASS); }

void smpfx_apply_filter(ft2_instance_t *inst, int filterType, int32_t cutoff)
{
	(filterType == 0) ? applyLowPassFilter(inst, cutoff) : applyHighPassFilter(inst, cutoff);
}

/* ------------------------------------------------------------------------- */
/*                             EQ                                            */
/* ------------------------------------------------------------------------- */

/* Removes sub-bass via HP at normalized 0.001 */
void pbSfxSubBass(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoHpFilter(s, &f, 0.001, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	inst->uiState.updateSampleEditor = true;
}

/* Adds bass by mixing in LP-filtered signal at 25% */
void pbSfxAddBass(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	double *dSmp = (double *)malloc(len * sizeof(double));
	if (!dSmp) return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (s->flags & SAMPLE_16BIT)
		for (int32_t i = 0; i < len; i++) dSmp[i] = ((int16_t *)s->dataPtr + x1)[i];
	else
		for (int32_t i = 0; i < len; i++) dSmp[i] = ((int8_t *)s->dataPtr + x1)[i];

	resoFilter_t f;
	setupResoLpFilter(s, &f, 0.015, 0, true);
	for (int32_t i = 0; i < len; i++)
	{
		double out = f.a1 * dSmp[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
		f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = dSmp[i];
		f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
		dSmp[i] = out;
	}

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr16[i] + dSmp[i] * 0.25;
			ptr16[i] = (out < INT16_MIN) ? INT16_MIN : (out > INT16_MAX) ? INT16_MAX : (int16_t)out;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr8[i] + dSmp[i] * 0.25;
			ptr8[i] = (out < INT8_MIN) ? INT8_MIN : (out > INT8_MAX) ? INT8_MAX : (int8_t)out;
		}
	}

	free(dSmp);
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Removes treble via LP at normalized 0.33 */
void pbSfxSubTreble(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoLpFilter(s, &f, 0.33, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	inst->uiState.updateSampleEditor = true;
}

/* Adds treble by subtracting HP-filtered signal at 25% (shelf boost) */
void pbSfxAddTreble(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	double *dSmp = (double *)malloc(len * sizeof(double));
	if (!dSmp) return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (s->flags & SAMPLE_16BIT)
		for (int32_t i = 0; i < len; i++) dSmp[i] = ((int16_t *)s->dataPtr + x1)[i];
	else
		for (int32_t i = 0; i < len; i++) dSmp[i] = ((int8_t *)s->dataPtr + x1)[i];

	resoFilter_t f;
	setupResoHpFilter(s, &f, 0.27, 0, true);
	for (int32_t i = 0; i < len; i++)
	{
		double out = f.a1 * dSmp[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
		f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = dSmp[i];
		f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
		dSmp[i] = out;
	}

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr16[i] - dSmp[i] * 0.25;
			ptr16[i] = (out < INT16_MIN) ? INT16_MIN : (out > INT16_MAX) ? INT16_MAX : (int16_t)out;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			double out = ptr8[i] - dSmp[i] * 0.25;
			ptr8[i] = (out < INT8_MIN) ? INT8_MIN : (out > INT8_MAX) ? INT8_MAX : (int8_t)out;
		}
	}

	free(dSmp);
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* ------------------------------------------------------------------------- */
/*                          AMPLITUDE                                        */
/* ------------------------------------------------------------------------- */

void pbSfxSetAmp(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s || !s->dataPtr) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	const int32_t mul = (int32_t)round((1 << 22UL) * (lastAmp / 100.0));

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr16[i] * mul) >> 22;
			ptr16[i] = (sample < INT16_MIN) ? INT16_MIN : (sample > INT16_MAX) ? INT16_MAX : (int16_t)sample;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr8[i] * mul) >> 22;
			ptr8[i] = (sample < INT8_MIN) ? INT8_MIN : (sample > INT8_MAX) ? INT8_MAX : (int8_t)sample;
		}
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

void pbSfxUndo(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smp_undo_t *undo = UNDO_STATE(inst);
	if (!undo->filled || undo->undoInstr != inst->editor.curInstr || undo->undoSmp != inst->editor.curSmp)
		return;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	if (!s) return;

	ft2_stop_sample_voices(inst, s);
	freeSmpData(inst, inst->editor.curInstr, inst->editor.curSmp);

	s->flags = undo->flags;
	s->length = undo->length;
	s->loopStart = undo->loopStart;
	s->loopLength = undo->loopLength;

	if (allocateSmpData(inst, inst->editor.curInstr, inst->editor.curSmp, s->length, !!(s->flags & SAMPLE_16BIT)))
	{
		memcpy(s->dataPtr, (s->flags & SAMPLE_16BIT) ? (void *)undo->smpData16 : (void *)undo->smpData8,
		       s->length * ((s->flags & SAMPLE_16BIT) ? sizeof(int16_t) : sizeof(int8_t)));
		ft2_fix_sample(s);
	}

	undo->keepSampleMark = false;
	undo->filled = false;
	inst->uiState.updateSampleEditor = true;
}

/* ------------------------------------------------------------------------- */
/*                        SCREEN VISIBILITY                                  */
/* ------------------------------------------------------------------------- */

/* Hide sample editor buttons, show effects panel buttons */
void showSampleEffectsScreen(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	inst->uiState.sampleEditorEffectsShown = true;

	/* Hide sample editor buttons */
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_STOP] = widgets->pushButtonVisible[PB_SAMP_PWAVE] = false;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = widgets->pushButtonVisible[PB_SAMP_SAVE_RNG] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;

	/* Show effects widgets */
	widgets->checkBoxChecked[CB_SAMPFX_NORM] = normalization;
	widgets->checkBoxVisible[CB_SAMPFX_NORM] = true;

	widgets->pushButtonVisible[PB_SAMPFX_CYCLES_UP] = widgets->pushButtonVisible[PB_SAMPFX_CYCLES_DOWN] = true;
	widgets->pushButtonVisible[PB_SAMPFX_TRIANGLE] = widgets->pushButtonVisible[PB_SAMPFX_SAW] = true;
	widgets->pushButtonVisible[PB_SAMPFX_SINE] = widgets->pushButtonVisible[PB_SAMPFX_SQUARE] = true;
	widgets->pushButtonVisible[PB_SAMPFX_RESO_UP] = widgets->pushButtonVisible[PB_SAMPFX_RESO_DOWN] = true;
	widgets->pushButtonVisible[PB_SAMPFX_LOWPASS] = widgets->pushButtonVisible[PB_SAMPFX_HIGHPASS] = true;
	widgets->pushButtonVisible[PB_SAMPFX_SUB_BASS] = widgets->pushButtonVisible[PB_SAMPFX_ADD_BASS] = true;
	widgets->pushButtonVisible[PB_SAMPFX_SUB_TREBLE] = widgets->pushButtonVisible[PB_SAMPFX_ADD_TREBLE] = true;
	widgets->pushButtonVisible[PB_SAMPFX_SET_AMP] = widgets->pushButtonVisible[PB_SAMPFX_UNDO] = true;
	widgets->pushButtonVisible[PB_SAMPFX_XFADE] = widgets->pushButtonVisible[PB_SAMPFX_BACK] = true;

	inst->uiState.needsFullRedraw = true;
}

/* Hide effects panel buttons, show sample editor buttons */
void hideSampleEffectsScreen(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	inst->uiState.sampleEditorEffectsShown = false;

	/* Hide effects widgets */
	widgets->checkBoxVisible[CB_SAMPFX_NORM] = false;
	widgets->pushButtonVisible[PB_SAMPFX_CYCLES_UP] = widgets->pushButtonVisible[PB_SAMPFX_CYCLES_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMPFX_TRIANGLE] = widgets->pushButtonVisible[PB_SAMPFX_SAW] = false;
	widgets->pushButtonVisible[PB_SAMPFX_SINE] = widgets->pushButtonVisible[PB_SAMPFX_SQUARE] = false;
	widgets->pushButtonVisible[PB_SAMPFX_RESO_UP] = widgets->pushButtonVisible[PB_SAMPFX_RESO_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMPFX_LOWPASS] = widgets->pushButtonVisible[PB_SAMPFX_HIGHPASS] = false;
	widgets->pushButtonVisible[PB_SAMPFX_SUB_BASS] = widgets->pushButtonVisible[PB_SAMPFX_ADD_BASS] = false;
	widgets->pushButtonVisible[PB_SAMPFX_SUB_TREBLE] = widgets->pushButtonVisible[PB_SAMPFX_ADD_TREBLE] = false;
	widgets->pushButtonVisible[PB_SAMPFX_SET_AMP] = widgets->pushButtonVisible[PB_SAMPFX_UNDO] = false;
	widgets->pushButtonVisible[PB_SAMPFX_XFADE] = widgets->pushButtonVisible[PB_SAMPFX_BACK] = false;

	/* Show sample editor buttons */
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = true;
	widgets->pushButtonVisible[PB_SAMP_STOP] = widgets->pushButtonVisible[PB_SAMP_PWAVE] = true;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = true;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = true;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = true;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = true;
	/* widgets->pushButtonVisible[PB_SAMP_SAVE_RNG] = true; */
	widgets->pushButtonVisible[PB_SAMP_CUT] = widgets->pushButtonVisible[PB_SAMP_COPY] = true;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = widgets->pushButtonVisible[PB_SAMP_CROP] = true;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = widgets->pushButtonVisible[PB_SAMP_EFFECTS] = true;

	inst->uiState.needsFullRedraw = true;
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

void drawSampleEffectsScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp) return;

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
		textOut(video, bmp, 172, 352, PAL_FORGRND, "off");
	else
		{ sprintf(str, "%02d", filterResonance); textOut(video, bmp, 175, 352, PAL_FORGRND, str); }

	textOutShadow(video, bmp, 135, 386, PAL_FORGRND, PAL_DSKTOP2, "Normalization");
	textOutShadow(video, bmp, 235, 352, PAL_FORGRND, PAL_DSKTOP2, "Bass");
	textOutShadow(video, bmp, 235, 369, PAL_FORGRND, PAL_DSKTOP2, "Treb.");
}
