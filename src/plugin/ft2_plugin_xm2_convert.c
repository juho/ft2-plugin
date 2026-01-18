/*
** FT2 Plugin - XM2 to XM conversion helpers
** Keeps usesXm2Features accurate and provides XM conversion utilities.
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_plugin_xm2_convert.h"
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_load_wav.h"
#include "ft2_plugin_replayer.h"
#include "../ft2_instance.h"

#define XM2_TO_XM_TARGET_RATE 44100.0

static bool sample_slot_used(const ft2_sample_t *s, ft2_sample_format_t format)
{
	if (s == NULL)
		return false;

	if (s->length > 0 && ft2_sample_has_data(s, format))
		return true;

	return s->name[0] != '\0';
}

static float sample_linear(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, double pos)
{
	if (s == NULL || s->length <= 0)
		return 0.0f;

	const int32_t lastIndex = s->length - 1;
	if (lastIndex <= 0 || pos <= 0.0)
		return ft2_sample_get_float(s, format, rightChannel, 0);
	if (pos >= (double)lastIndex)
		return ft2_sample_get_float(s, format, rightChannel, lastIndex);

	const int32_t idx = (int32_t)pos;
	int32_t idx2 = idx + 1;
	if (idx2 > lastIndex)
		idx2 = lastIndex;

	const float frac = (float)(pos - (double)idx);
	const float a = ft2_sample_get_float(s, format, rightChannel, idx);
	const float b = ft2_sample_get_float(s, format, rightChannel, idx2);
	return a + ((b - a) * frac);
}

static uint32_t scan_xm2_features(ft2_instance_t *inst)
{
	if (inst == NULL)
		return 0;

	uint32_t flags = 0;
	int32_t instrIdx = 1;
	while (instrIdx <= FT2_MAX_INST)
	{
		ft2_instr_t *instr = inst->replayer.instr[instrIdx];
		if (instr == NULL)
		{
			instrIdx++;
			continue;
		}

		if (instr->numSamples > FT2_XM_MAX_SMP_PER_INST)
			flags |= FT2_XM2_FEATURE_SAMPLE_SLOTS;

		int32_t note = 0;
		while (note < 96)
		{
			if (instr->note2SampleLUT[note] > (FT2_XM_MAX_SMP_PER_INST - 1))
			{
				flags |= FT2_XM2_FEATURE_SAMPLE_SLOTS;
				break;
			}
			note++;
		}

		int32_t smpIdx = 0;
		while (smpIdx < FT2_XM2_MAX_SMP_PER_INST)
		{
			ft2_sample_t *smp = &instr->smp[smpIdx];
			ft2_sample_format_t format = ft2_sample_get_format(smp);
			const bool usedSlot = sample_slot_used(smp, format);

			if (smpIdx >= FT2_XM_MAX_SMP_PER_INST && usedSlot)
				flags |= FT2_XM2_FEATURE_SAMPLE_SLOTS;

			if (smp->length > 0 && ft2_sample_has_data(smp, format))
			{
				if (format == FT2_SMP_FORMAT_24BIT || format == FT2_SMP_FORMAT_FLOAT32)
					flags |= FT2_XM2_FEATURE_FORMAT;
				if (ft2_sample_is_stereo(smp, format))
					flags |= FT2_XM2_FEATURE_STEREO;
				if (smp->sampleRate > 0)
					flags |= FT2_XM2_FEATURE_SAMPLE_RATE;

				const double rate = ft2_sample_get_playback_rate(inst, smp);
				if (rate > XM2_TO_XM_TARGET_RATE)
					flags |= FT2_XM2_FEATURE_HIGH_RATE;
			}

			smpIdx++;
		}

		for (int32_t i = 1; i < instr->volEnvLength; i++)
		{
			if (instr->volEnvCurve[i] != 0)
			{
				flags |= FT2_XM2_FEATURE_ENV_CURVE;
				break;
			}
		}
		if ((flags & FT2_XM2_FEATURE_ENV_CURVE) == 0)
		{
			for (int32_t i = 1; i < instr->panEnvLength; i++)
			{
				if (instr->panEnvCurve[i] != 0)
				{
					flags |= FT2_XM2_FEATURE_ENV_CURVE;
					break;
				}
			}
		}

		if (instr->volEnvSpeed != 1 || instr->panEnvSpeed != 1)
			flags |= FT2_XM2_FEATURE_ENV_LENGTH;

		instrIdx++;
	}

	return flags;
}

void ft2_xm2_update_usage(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->replayer.song.usesXm2Features = (scan_xm2_features(inst) != 0);
}

bool ft2_xm2_song_needs_conversion(ft2_instance_t *inst, uint32_t *outFlags)
{
	if (inst == NULL)
		return false;

	const uint32_t flags = scan_xm2_features(inst);
	if (outFlags != NULL)
		*outFlags = flags;

	return (flags != 0);
}

bool ft2_xm2_get_xm_sample_params(const ft2_instance_t *inst, const ft2_sample_t *src, ft2_xm2_sample_params_t *out)
{
	if (inst == NULL || src == NULL || out == NULL || src->length <= 0)
		return false;

	ft2_sample_format_t srcFormat = ft2_sample_get_format(src);
	if (!ft2_sample_has_data(src, srcFormat))
		return false;

	const double srcRate = ft2_sample_get_playback_rate(inst, src);
	double targetRate = srcRate;
	bool resample = false;
	if (srcRate > XM2_TO_XM_TARGET_RATE)
	{
		targetRate = XM2_TO_XM_TARGET_RATE;
		resample = true;
	}

	ft2_sample_format_t targetFormat = srcFormat;
	if (srcFormat == FT2_SMP_FORMAT_24BIT || srcFormat == FT2_SMP_FORMAT_FLOAT32)
		targetFormat = FT2_SMP_FORMAT_16BIT;

	const int32_t srcLen = src->length;
	int32_t newLen = srcLen;
	int32_t newLoopStart = src->loopStart;
	int32_t newLoopLength = src->loopLength;

	if (resample && srcRate > 0.0)
	{
		const double ratio = targetRate / srcRate;
		newLen = (int32_t)llround((double)srcLen * ratio);
		if (newLen < 1)
			newLen = 1;
		newLoopStart = (int32_t)llround((double)src->loopStart * ratio);
		newLoopLength = (int32_t)llround((double)src->loopLength * ratio);
	}

	if (newLen <= 0)
		return false;

	out->format = targetFormat;
	out->length = newLen;
	out->loopStart = newLoopStart;
	out->loopLength = newLoopLength;
	out->targetRate = targetRate;
	out->resample = resample;
	return true;
}

static bool build_converted_sample(const ft2_instance_t *inst, const ft2_sample_t *src, ft2_sample_t *dst)
{
	if (src == NULL || dst == NULL || src->length <= 0)
		return false;

	ft2_xm2_sample_params_t params;
	if (!ft2_xm2_get_xm_sample_params(inst, src, &params))
		return false;

	ft2_sample_format_t srcFormat = ft2_sample_get_format(src);
	const bool srcStereo = ft2_sample_is_stereo(src, srcFormat);
	const int32_t srcLen = src->length;
	const int32_t newLen = params.length;
	const int32_t newLoopStart = params.loopStart;
	const int32_t newLoopLength = params.loopLength;
	const ft2_sample_format_t targetFormat = params.format;

	const uint32_t bytesPerSample = ft2_sample_format_bytes(targetFormat);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)newLen * bytesPerSample) + (size_t)padding;

	void *newOrigL = calloc(allocSize, 1);
	if (newOrigL == NULL)
		return false;

	uint8_t *newDataL = (uint8_t *)newOrigL + padding;

	memset(dst, 0, sizeof(ft2_sample_t));
	memcpy(dst->name, src->name, sizeof(dst->name));
	dst->volume = src->volume;
	dst->panning = src->panning;
	dst->length = newLen;
	dst->loopStart = newLoopStart;
	dst->loopLength = newLoopLength;
	dst->format = targetFormat;
	dst->flags = src->flags;
	dst->flags &= ~FT2_SAMPLE_STEREO;
	if (targetFormat == FT2_SMP_FORMAT_16BIT)
		dst->flags |= FT2_SAMPLE_16BIT;
	else
		dst->flags &= ~FT2_SAMPLE_16BIT;
	dst->isFixed = false;
	dst->fixedPos = 0;

	if (targetFormat == FT2_SMP_FORMAT_24BIT)
	{
		dst->origDataPtr24 = (int32_t *)newOrigL;
		dst->dataPtr24 = (int32_t *)newDataL;
	}
	else if (targetFormat == FT2_SMP_FORMAT_FLOAT32)
	{
		dst->origDataPtrF = (float *)newOrigL;
		dst->dataPtrF = (float *)newDataL;
	}
	else
	{
		dst->origDataPtr = (int8_t *)newOrigL;
		dst->dataPtr = (int8_t *)newDataL;
	}

	int32_t i = 0;
	while (i < newLen)
	{
		const double pos = (newLen <= 1 || srcLen <= 1) ? 0.0 :
			((double)i * (double)(srcLen - 1)) / (double)(newLen - 1);
		float left = sample_linear(src, srcFormat, false, pos);
		float right = left;
		if (srcStereo)
			right = sample_linear(src, srcFormat, true, pos);

		const float v = srcStereo ? (0.5f * (left + right)) : left;
		ft2_sample_set_float(dst, targetFormat, false, i, v);
		i++;
	}

	dst->sampleRate = 0;
	if (params.resample)
		ft2_set_sample_c4hz(dst, params.targetRate);
	else if (src->sampleRate > 0)
		ft2_set_sample_c4hz(dst, (double)src->sampleRate);
	else
	{
		dst->relativeNote = src->relativeNote;
		dst->finetune = src->finetune;
	}

	return true;
}

bool ft2_xm2_conform_sample_inplace(ft2_instance_t *inst, ft2_sample_t *s)
{
	if (inst == NULL || s == NULL)
		return false;

	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!ft2_sample_has_data(s, format) || s->length <= 0)
		return false;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	ft2_sample_t dstSmp;
	if (!build_converted_sample(inst, s, &dstSmp))
	{
		ft2_fix_sample(s);
		return false;
	}

	ft2_sample_free_buffers(s);
	*s = dstSmp;
	ft2_sanitize_sample(s);
	ft2_fix_sample(s);
	return true;
}

bool ft2_xm2_conform_song_inplace(ft2_instance_t *inst)
{
	if (inst == NULL)
		return false;

	bool ok = true;
	bool didChange = false;
	int32_t instrIdx = 1;
	while (instrIdx <= FT2_MAX_INST)
	{
		ft2_instr_t *instr = inst->replayer.instr[instrIdx];
		if (instr == NULL)
		{
			instrIdx++;
			continue;
		}

		if (instr->numSamples > FT2_XM_MAX_SMP_PER_INST)
		{
			instr->numSamples = FT2_XM_MAX_SMP_PER_INST;
			didChange = true;
		}

		if (instr->volEnvSpeed != 1)
		{
			instr->volEnvSpeed = 1;
			didChange = true;
		}
		if (instr->panEnvSpeed != 1)
		{
			instr->panEnvSpeed = 1;
			didChange = true;
		}

		int32_t smpIdx = FT2_XM_MAX_SMP_PER_INST;
		while (smpIdx < FT2_XM2_MAX_SMP_PER_INST)
		{
			freeSmpData(inst, instrIdx, smpIdx);
			memset(instr->smp[smpIdx].name, 0, sizeof(instr->smp[smpIdx].name));
			didChange = true;
			smpIdx++;
		}

		int32_t note = 0;
		while (note < 96)
		{
			if (instr->note2SampleLUT[note] > (FT2_XM_MAX_SMP_PER_INST - 1))
			{
				instr->note2SampleLUT[note] = (FT2_XM_MAX_SMP_PER_INST - 1);
				didChange = true;
			}
			note++;
		}

		smpIdx = 0;
		while (smpIdx < FT2_XM_MAX_SMP_PER_INST)
		{
			ft2_sample_t *smp = &instr->smp[smpIdx];
			ft2_sample_format_t format = ft2_sample_get_format(smp);
			if (smp->length > 0 && ft2_sample_has_data(smp, format))
			{
				if (!ft2_xm2_conform_sample_inplace(inst, smp))
					ok = false;
				else
					didChange = true;
			}
			smpIdx++;
		}

		instrIdx++;
	}

	if (didChange)
	{
		ft2_song_mark_modified(inst);
		ft2_sample_ed_mark_waveform_dirty(inst);
		inst->uiState.updateInstrSwitcher = true;
		inst->uiState.updateInstEditor = true;
	}

	ft2_xm2_update_usage(inst);
	return ok;
}

bool ft2_xm2_build_xm_sample_copy(const ft2_instance_t *inst, const ft2_sample_t *src, ft2_sample_t *dst)
{
	if (inst == NULL || src == NULL || dst == NULL)
		return false;

	return build_converted_sample(inst, src, dst);
}

void ft2_xm2_free_sample_copy(ft2_sample_t *s)
{
	if (s == NULL)
		return;

	ft2_sample_free_buffers(s);
	memset(s, 0, sizeof(ft2_sample_t));
}
