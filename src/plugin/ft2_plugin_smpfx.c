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
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_filter_panel.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_xm2_convert.h"
#include "../ft2_instance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RESONANCE_RANGE 99
#define RESONANCE_MIN 0.01

enum { REMOVE_SAMPLE_MARK = 0, KEEP_SAMPLE_MARK = 1 };

/* 2nd-order IIR biquad filter state */
typedef struct {
	double a1, a2, a3, b1, b2;  /* Coefficients */
	double inTmp[2], outTmp[2]; /* Delay line */
} resoFilter_t;

#define UNDO_STATE(inst) (&FT2_SAMPLE_ED(inst)->undo)
#define SMPFX_STATE(inst) (&FT2_SAMPLE_ED(inst)->smpfx)

/* ------------------------------------------------------------------------- */
/*                           HELPERS                                         */
/* ------------------------------------------------------------------------- */

static ft2_sample_t *getSmpFxCurSample(ft2_instance_t *inst)
{
	if (!inst || inst->editor.curInstr == 0 || inst->editor.curInstr >= 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (!instr || inst->editor.curSmp >= maxSamples) return NULL;
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
	if (undo->smpDataL) { free(undo->smpDataL); undo->smpDataL = NULL; }
	if (undo->smpDataR) { free(undo->smpDataR); undo->smpDataR = NULL; }
	undo->filled = false;
	undo->keepSampleMark = false;
}

void fillSampleUndo(ft2_instance_t *inst, bool keepSampleMark)
{
	if (!inst || !inst->ui) return;
	smp_undo_t *undo = UNDO_STATE(inst);
	undo->filled = false;

	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || s->length == 0 || !ft2_sample_has_data(s, format)) return;

	ft2_unfix_sample(s);
	clearSampleUndo(inst);

	undo->undoInstr = inst->editor.curInstr;
	undo->undoSmp = inst->editor.curSmp;
	undo->flags = s->flags;
	undo->length = s->length;
	undo->loopStart = s->loopStart;
	undo->loopLength = s->loopLength;
	undo->keepSampleMark = keepSampleMark;
	undo->format = format;
	undo->sampleRate = s->sampleRate;
	undo->bytesPerSample = ft2_sample_format_bytes(format);
	undo->isStereo = ft2_sample_is_stereo(s, format);

	size_t dataBytes = (size_t)s->length * undo->bytesPerSample;
	undo->smpDataL = (uint8_t *)malloc(dataBytes);
	if (undo->smpDataL == NULL)
	{
		ft2_fix_sample(s);
		return;
	}
	memcpy(undo->smpDataL, ft2_sample_get_data_ptr_const(s, format, false), dataBytes);

	if (undo->isStereo)
	{
		undo->smpDataR = (uint8_t *)malloc(dataBytes);
		if (undo->smpDataR == NULL)
		{
			free(undo->smpDataL);
			undo->smpDataL = NULL;
			ft2_fix_sample(s);
			return;
		}
		memcpy(undo->smpDataR, ft2_sample_get_data_ptr_const(s, format, true), dataBytes);
	}

	undo->filled = true;

	ft2_fix_sample(s);
}

/* Allocates new sample using current format/stereo, stopping any playing voices */
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

	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);
	const uint32_t sampleRate = s->sampleRate;

	if (!ft2_sample_alloc_buffers(s, (int32_t)length, format, isStereo))
		return NULL;

	s->isFixed = false;
	s->length = length;
	s->loopLength = s->loopStart = 0;
	s->flags &= ~(LOOP_FWD | LOOP_BIDI);
	if (format == FT2_SMP_FORMAT_16BIT)
		s->flags |= FT2_SAMPLE_16BIT;
	else
		s->flags &= ~FT2_SAMPLE_16BIT;
	if (isStereo)
		s->flags |= FT2_SAMPLE_STEREO;
	else
		s->flags &= ~FT2_SAMPLE_STEREO;
	s->format = format;
	s->sampleRate = sampleRate;
	ft2_xm2_update_usage(inst);
	return s;
}

/* ------------------------------------------------------------------------- */
/*                         STATE ACCESSORS                                   */
/* ------------------------------------------------------------------------- */

void cbSfxNormalization(ft2_instance_t *inst) {
	if (!inst || !inst->ui) return;
	SMPFX_STATE(inst)->normalization = !SMPFX_STATE(inst)->normalization;
}
bool getSfxNormalization(ft2_instance_t *inst) {
	return (inst && inst->ui) ? SMPFX_STATE(inst)->normalization : false;
}
int32_t getSfxCycles(ft2_instance_t *inst) {
	return (inst && inst->ui) ? SMPFX_STATE(inst)->smpCycles : 1;
}
int32_t getSfxResonance(ft2_instance_t *inst) {
	return (inst && inst->ui) ? SMPFX_STATE(inst)->filterResonance : 0;
}

void pbSfxCyclesUp(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (fx->smpCycles < 256) { fx->smpCycles++; inst->uiState.updateSampleEditor = true; }
}

void pbSfxCyclesDown(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (fx->smpCycles > 1) { fx->smpCycles--; inst->uiState.updateSampleEditor = true; }
}

/* ------------------------------------------------------------------------- */
/*                       WAVE GENERATION                                     */
/* ------------------------------------------------------------------------- */

static void generateTriangle(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (inst->editor.curInstr == 0 || fx->lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = fx->lastWaveLength * fx->smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double delta = 4.0 / fx->lastWaveLength;
	double phase = 0.0;
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);

	for (int32_t i = 0; i < newLength; i++)
	{
		double t = (phase > 3.0) ? phase - 4.0 : (phase >= 1.0) ? 2.0 - phase : phase;
		float v = (float)t;
		ft2_sample_set_float(s, format, false, i, v);
		if (isStereo)
			ft2_sample_set_float(s, format, true, i, v);
		phase = fmod(phase + delta, 4.0);
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

static void generateSaw(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (inst->editor.curInstr == 0 || fx->lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = fx->lastWaveLength * fx->smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	uint64_t point64 = 0, delta64 = ((uint64_t)(INT16_MAX * 2) << 32ULL) / fx->lastWaveLength;
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);

	for (int32_t i = 0; i < newLength; i++)
	{
		int16_t s16 = (int16_t)(point64 >> 32);
		float v = (float)s16 * (1.0f / 32768.0f);
		ft2_sample_set_float(s, format, false, i, v);
		if (isStereo)
			ft2_sample_set_float(s, format, true, i, v);
		point64 += delta64;
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

static void generateSine(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (inst->editor.curInstr == 0 || fx->lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	int32_t newLength = fx->lastWaveLength * fx->smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const double dMul = (2.0 * M_PI) / fx->lastWaveLength;
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);
	for (int32_t i = 0; i < newLength; i++)
	{
		float v = (float)sin(i * dMul);
		ft2_sample_set_float(s, format, false, i, v);
		if (isStereo)
			ft2_sample_set_float(s, format, true, i, v);
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

static void generateSquare(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (inst->editor.curInstr == 0 || fx->lastWaveLength <= 1) return;
	fillSampleUndo(inst, REMOVE_SAMPLE_MARK);

	uint32_t newLength = fx->lastWaveLength * fx->smpCycles;
	ft2_sample_t *s = setupNewSample(inst, newLength);
	if (!s) return;

	const uint32_t halfWaveLength = fx->lastWaveLength / 2;
	int16_t currValue = INT16_MAX;
	uint32_t counter = 0;
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);

	for (uint32_t i = 0; i < newLength; i++)
	{
		float v = (currValue >= 0) ? 1.0f : -1.0f;
		ft2_sample_set_float(s, format, false, (int32_t)i, v);
		if (isStereo)
			ft2_sample_set_float(s, format, true, (int32_t)i, v);
		if (++counter >= halfWaveLength) { counter = 0; currValue = -currValue; }
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FWD;
	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

void pbSfxTriangle(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_TRIANGLE); }
void pbSfxSaw(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SAW); }
void pbSfxSine(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SINE); }
void pbSfxSquare(ft2_instance_t *inst) { ft2_wave_panel_show(inst, WAVE_TYPE_SQUARE); }

/* ------------------------------------------------------------------------- */
/*                           FILTERS                                         */
/* ------------------------------------------------------------------------- */

void pbSfxResoUp(ft2_instance_t *inst) {
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (fx->filterResonance < RESONANCE_RANGE) { fx->filterResonance++; inst->uiState.updateSampleEditor = true; }
}
void pbSfxResoDown(ft2_instance_t *inst) {
	if (!inst || !inst->ui) return;
	smpfx_state_t *fx = SMPFX_STATE(inst);
	if (fx->filterResonance > 0) { fx->filterResonance--; inst->uiState.updateSampleEditor = true; }
}

#define CUTOFF_EPSILON (1E-4)

/* 2nd-order Butterworth lowpass with resonance (Q controlled by resonance param) */
static void setupResoLpFilter(ft2_instance_t *inst, ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = ft2_sample_get_playback_rate(inst, s);
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
static void setupResoHpFilter(ft2_instance_t *inst, ft2_sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	if (!absoluteCutoff)
	{
		const double sampleFreq = ft2_sample_get_playback_rate(inst, s);
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
	if (!s) return false;
	if (x1 < 0) x1 = 0;
	if (x2 > s->length) x2 = s->length;
	if (x2 <= x1) return true;

	const int32_t len = x2 - x1;
	const ft2_sample_format_t format = ft2_sample_get_format(s);
	const bool isStereo = ft2_sample_is_stereo(s, format);
	const bool doNormalize = (inst && inst->ui) ? SMPFX_STATE(inst)->normalization : false;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	if (!doNormalize)
	{
		/* Process in-place per channel */
		for (int ch = 0; ch < (isStereo ? 2 : 1); ch++)
		{
			const bool rightChannel = (ch == 1);
			resoFilter_t fc = *f;
			fc.inTmp[0] = fc.inTmp[1] = fc.outTmp[0] = fc.outTmp[1] = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double in = ft2_sample_get_float(s, format, rightChannel, x1 + i);
				double out = fc.a1 * in + fc.a2 * fc.inTmp[0] + fc.a3 * fc.inTmp[1] - fc.b1 * fc.outTmp[0] - fc.b2 * fc.outTmp[1];
				fc.inTmp[1] = fc.inTmp[0]; fc.inTmp[0] = in;
				fc.outTmp[1] = fc.outTmp[0]; fc.outTmp[0] = out;
				ft2_sample_set_float(s, format, rightChannel, x1 + i, (float)out);
			}
		}
	}
	else
	{
		/* Normalize across channels using temporary buffers */
		float *tmpL = (float *)malloc((size_t)len * sizeof(float));
		float *tmpR = isStereo ? (float *)malloc((size_t)len * sizeof(float)) : NULL;
		if (!tmpL || (isStereo && !tmpR))
		{
			if (tmpL) free(tmpL);
			if (tmpR) free(tmpR);
			ft2_fix_sample(s);
			return false;
		}

		double peak = 0.0;
		{
			resoFilter_t fc = *f;
			fc.inTmp[0] = fc.inTmp[1] = fc.outTmp[0] = fc.outTmp[1] = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double in = ft2_sample_get_float(s, format, false, x1 + i);
				double out = fc.a1 * in + fc.a2 * fc.inTmp[0] + fc.a3 * fc.inTmp[1] - fc.b1 * fc.outTmp[0] - fc.b2 * fc.outTmp[1];
				fc.inTmp[1] = fc.inTmp[0]; fc.inTmp[0] = in;
				fc.outTmp[1] = fc.outTmp[0]; fc.outTmp[0] = out;
				tmpL[i] = (float)out;
				if (fabs(out) > peak) peak = fabs(out);
			}
		}

		if (isStereo)
		{
			resoFilter_t fc = *f;
			fc.inTmp[0] = fc.inTmp[1] = fc.outTmp[0] = fc.outTmp[1] = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double in = ft2_sample_get_float(s, format, true, x1 + i);
				double out = fc.a1 * in + fc.a2 * fc.inTmp[0] + fc.a3 * fc.inTmp[1] - fc.b1 * fc.outTmp[0] - fc.b2 * fc.outTmp[1];
				fc.inTmp[1] = fc.inTmp[0]; fc.inTmp[0] = in;
				fc.outTmp[1] = fc.outTmp[0]; fc.outTmp[0] = out;
				tmpR[i] = (float)out;
				if (fabs(out) > peak) peak = fabs(out);
			}
		}

		float scale = (peak > 0.0) ? (float)(1.0 / peak) : 1.0f;
		for (int32_t i = 0; i < len; i++)
			ft2_sample_set_float(s, format, false, x1 + i, tmpL[i] * scale);
		if (isStereo)
			for (int32_t i = 0; i < len; i++)
				ft2_sample_set_float(s, format, true, x1 + i, tmpR[i] * scale);

		free(tmpL);
		if (tmpR) free(tmpR);
	}

	ft2_fix_sample(s);
	return true;
}

static void applyLowPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || !inst->ui) return;

	smpfx_state_t *fx = SMPFX_STATE(inst);
	fx->lastFilterType = FILTER_LOWPASS;
	fx->lastLpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoLpFilter(inst, s, &f, cutoff, fx->filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

static void applyHighPassFilter(ft2_instance_t *inst, int32_t cutoff)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || !inst->ui) return;

	smpfx_state_t *fx = SMPFX_STATE(inst);
	fx->lastFilterType = FILTER_HIGHPASS;
	fx->lastHpCutoff = cutoff;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoHpFilter(inst, s, &f, cutoff, fx->filterResonance, false);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	ft2_sample_ed_mark_waveform_dirty(inst);
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
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format)) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoHpFilter(inst, s, &f, 0.001, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Adds bass by mixing in LP-filtered signal at 25% */
void pbSfxAddBass(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format)) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	const bool isStereo = ft2_sample_is_stereo(s, format);
	float *origL = (float *)malloc((size_t)len * sizeof(float));
	float *filtL = (float *)malloc((size_t)len * sizeof(float));
	float *origR = isStereo ? (float *)malloc((size_t)len * sizeof(float)) : NULL;
	float *filtR = isStereo ? (float *)malloc((size_t)len * sizeof(float)) : NULL;
	if (!origL || !filtL || (isStereo && (!origR || !filtR)))
	{
		free(origL); free(filtL); free(origR); free(filtR);
		return;
	}

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	for (int32_t i = 0; i < len; i++)
		origL[i] = ft2_sample_get_float(s, format, false, x1 + i);

	resoFilter_t f;
	setupResoLpFilter(inst, s, &f, 0.015, 0, true);
	for (int32_t i = 0; i < len; i++)
	{
		double out = f.a1 * origL[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
		f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = origL[i];
		f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
		filtL[i] = (float)out;
	}

	if (isStereo)
	{
		for (int32_t i = 0; i < len; i++)
			origR[i] = ft2_sample_get_float(s, format, true, x1 + i);

		setupResoLpFilter(inst, s, &f, 0.015, 0, true);
		for (int32_t i = 0; i < len; i++)
		{
			double out = f.a1 * origR[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
			f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = origR[i];
			f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
			filtR[i] = (float)out;
		}
	}

	for (int32_t i = 0; i < len; i++)
		ft2_sample_set_float(s, format, false, x1 + i, origL[i] + (filtL[i] * 0.25f));
	if (isStereo)
		for (int32_t i = 0; i < len; i++)
			ft2_sample_set_float(s, format, true, x1 + i, origR[i] + (filtR[i] * 0.25f));

	ft2_fix_sample(s);
	free(origL); free(filtL); free(origR); free(filtR);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Removes treble via LP at normalized 0.33 */
void pbSfxSubTreble(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format)) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);

	resoFilter_t f;
	setupResoLpFilter(inst, s, &f, 0.33, 0, true);
	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	applyResoFilter(inst, s, &f, x1, x2);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* Adds treble by subtracting HP-filtered signal at 25% (shelf boost) */
void pbSfxAddTreble(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format)) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	const bool isStereo = ft2_sample_is_stereo(s, format);
	float *origL = (float *)malloc((size_t)len * sizeof(float));
	float *filtL = (float *)malloc((size_t)len * sizeof(float));
	float *origR = isStereo ? (float *)malloc((size_t)len * sizeof(float)) : NULL;
	float *filtR = isStereo ? (float *)malloc((size_t)len * sizeof(float)) : NULL;
	if (!origL || !filtL || (isStereo && (!origR || !filtR)))
	{
		free(origL); free(filtL); free(origR); free(filtR);
		return;
	}

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	for (int32_t i = 0; i < len; i++)
		origL[i] = ft2_sample_get_float(s, format, false, x1 + i);

	resoFilter_t f;
	setupResoHpFilter(inst, s, &f, 0.27, 0, true);
	for (int32_t i = 0; i < len; i++)
	{
		double out = f.a1 * origL[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
		f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = origL[i];
		f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
		filtL[i] = (float)out;
	}

	if (isStereo)
	{
		for (int32_t i = 0; i < len; i++)
			origR[i] = ft2_sample_get_float(s, format, true, x1 + i);

		setupResoHpFilter(inst, s, &f, 0.27, 0, true);
		for (int32_t i = 0; i < len; i++)
		{
			double out = f.a1 * origR[i] + f.a2 * f.inTmp[0] + f.a3 * f.inTmp[1] - f.b1 * f.outTmp[0] - f.b2 * f.outTmp[1];
			f.inTmp[1] = f.inTmp[0]; f.inTmp[0] = origR[i];
			f.outTmp[1] = f.outTmp[0]; f.outTmp[0] = out;
			filtR[i] = (float)out;
		}
	}

	for (int32_t i = 0; i < len; i++)
		ft2_sample_set_float(s, format, false, x1 + i, origL[i] - (filtL[i] * 0.25f));
	if (isStereo)
		for (int32_t i = 0; i < len; i++)
			ft2_sample_set_float(s, format, true, x1 + i, origR[i] - (filtR[i] * 0.25f));

	ft2_fix_sample(s);
	free(origL); free(filtL); free(origR); free(filtR);
	ft2_sample_ed_mark_waveform_dirty(inst);
}

/* ------------------------------------------------------------------------- */
/*                          AMPLITUDE                                        */
/* ------------------------------------------------------------------------- */

void pbSfxSetAmp(ft2_instance_t *inst)
{
	ft2_sample_t *s = getSmpFxCurSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || !inst->ui) return;

	int32_t x1, x2;
	getSmpFxRange(inst, s, &x1, &x2);
	const int32_t len = x2 - x1;
	if (len <= 0) return;

	fillSampleUndo(inst, KEEP_SAMPLE_MARK);
	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	const double mul = SMPFX_STATE(inst)->lastAmp / 100.0;
	const bool isStereo = ft2_sample_is_stereo(s, format);

	for (int32_t i = 0; i < len; i++)
	{
		float v = ft2_sample_get_float(s, format, false, x1 + i);
		ft2_sample_set_float(s, format, false, x1 + i, (float)(v * mul));
		if (isStereo)
		{
			v = ft2_sample_get_float(s, format, true, x1 + i);
			ft2_sample_set_float(s, format, true, x1 + i, (float)(v * mul));
		}
	}

	ft2_fix_sample(s);
	ft2_sample_ed_mark_waveform_dirty(inst);
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
	ft2_sample_free_buffers(s);

	s->flags = undo->flags;
	s->format = undo->format;
	s->sampleRate = undo->sampleRate;
	s->length = (int32_t)undo->length;
	s->loopStart = (int32_t)undo->loopStart;
	s->loopLength = (int32_t)undo->loopLength;

	if (ft2_sample_alloc_buffers(s, s->length, undo->format, undo->isStereo))
	{
		const size_t dataBytes = (size_t)undo->length * undo->bytesPerSample;
		uint8_t *dstL = (uint8_t *)ft2_sample_get_data_ptr(s, undo->format, false);
		memcpy(dstL, undo->smpDataL, dataBytes);

		if (undo->isStereo && undo->smpDataR != NULL)
		{
			uint8_t *dstR = (uint8_t *)ft2_sample_get_data_ptr(s, undo->format, true);
			memcpy(dstR, undo->smpDataR, dataBytes);
		}

		ft2_fix_sample(s);
	}

	undo->keepSampleMark = false;
	undo->filled = false;
	ft2_sample_ed_mark_waveform_dirty(inst);
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
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = widgets->pushButtonVisible[PB_SAMP_SLICER] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;

	/* Show effects widgets */
	smpfx_state_t *fx = SMPFX_STATE(inst);
	widgets->checkBoxChecked[CB_SAMPFX_NORM] = fx->normalization;
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

	/* Only restore sample editor buttons if sample editor is still visible */
	if (inst->uiState.sampleEditorShown)
	{
		widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = true;
		widgets->pushButtonVisible[PB_SAMP_STOP] = widgets->pushButtonVisible[PB_SAMP_PWAVE] = true;
		widgets->pushButtonVisible[PB_SAMP_PRANGE] = widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = true;
		widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = true;
		widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = true;
		widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = true;
		widgets->pushButtonVisible[PB_SAMP_SLICER] = true;
		widgets->pushButtonVisible[PB_SAMP_CUT] = widgets->pushButtonVisible[PB_SAMP_COPY] = true;
		widgets->pushButtonVisible[PB_SAMP_PASTE] = widgets->pushButtonVisible[PB_SAMP_CROP] = true;
		widgets->pushButtonVisible[PB_SAMP_VOLUME] = widgets->pushButtonVisible[PB_SAMP_EFFECTS] = true;
	}

	inst->uiState.needsFullRedraw = true;
}

void restoreSampleEffectsWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!inst->uiState.sampleEditorShown || !inst->uiState.sampleEditorEffectsShown) return;

	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

	/* Hide sample editor buttons */
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_STOP] = widgets->pushButtonVisible[PB_SAMP_PWAVE] = false;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = widgets->pushButtonVisible[PB_SAMP_SLICER] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;

	/* Show effects widgets */
	smpfx_state_t *fx = SMPFX_STATE(inst);
	widgets->checkBoxChecked[CB_SAMPFX_NORM] = fx->normalization;
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
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

void drawSampleEffectsScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp || !inst->ui) return;

	smpfx_state_t *fx = SMPFX_STATE(inst);

	drawFramework(video, 0, 346, 116, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 116, 346, 114, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 230, 346, 67, 54, FRAMEWORK_TYPE1);
	drawFramework(video, 297, 346, 56, 54, FRAMEWORK_TYPE1);

	textOutShadow(video, bmp, 4, 352, PAL_FORGRND, PAL_DSKTOP2, "Cycles:");
	char str[16];
	sprintf(str, "%03d", fx->smpCycles);
	textOut(video, bmp, 54, 352, PAL_FORGRND, str);

	textOutShadow(video, bmp, 121, 352, PAL_FORGRND, PAL_DSKTOP2, "Reson.:");
	if (fx->filterResonance <= 0)
		textOut(video, bmp, 172, 352, PAL_FORGRND, "off");
	else
		{ sprintf(str, "%02d", fx->filterResonance); textOut(video, bmp, 175, 352, PAL_FORGRND, str); }

	textOutShadow(video, bmp, 135, 386, PAL_FORGRND, PAL_DSKTOP2, "Normalization");
	textOutShadow(video, bmp, 235, 352, PAL_FORGRND, PAL_DSKTOP2, "Bass");
	textOutShadow(video, bmp, 235, 369, PAL_FORGRND, PAL_DSKTOP2, "Treb.");
}
