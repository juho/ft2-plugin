/*
** FT2 Plugin - Sample Utilities
** Shared helpers for format/stereo/sample-rate aware sample access.
*/

#include <math.h>
#include <stdlib.h>
#include "ft2_plugin_sample_utils.h"

extern const uint16_t linearPeriodLUT[1936];
extern const uint16_t amigaPeriodLUT[1936];

static double period_to_hz(const ft2_instance_t *inst, uint32_t period)
{
	if (inst == NULL)
		return 0.0;

	period &= 0xFFFF;
	if (period == 0)
		return 0.0;

	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		const uint32_t quotient = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);
		return inst->replayer.dLogTab[remainder] * inst->replayer.dExp2MulTab[(14 - quotient) & 31];
	}

	return (8363.0 * 1712.0) / (int32_t)period;
}

ft2_sample_format_t ft2_sample_get_format(const ft2_sample_t *s)
{
	if (s == NULL)
		return FT2_SMP_FORMAT_8BIT;

	if (s->format == FT2_SMP_FORMAT_24BIT || s->format == FT2_SMP_FORMAT_FLOAT32)
		return s->format;

	return (s->flags & FT2_SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
}

bool ft2_sample_has_data(const ft2_sample_t *s, ft2_sample_format_t format)
{
	if (s == NULL)
		return false;

	if (format == FT2_SMP_FORMAT_24BIT)
		return (s->dataPtr24 != NULL);
	if (format == FT2_SMP_FORMAT_FLOAT32)
		return (s->dataPtrF != NULL);
	return (s->dataPtr != NULL);
}

bool ft2_sample_is_stereo(const ft2_sample_t *s, ft2_sample_format_t format)
{
	if (s == NULL || !(s->flags & FT2_SAMPLE_STEREO))
		return false;

	if (format == FT2_SMP_FORMAT_24BIT)
		return s->dataPtr24R != NULL;
	if (format == FT2_SMP_FORMAT_FLOAT32)
		return s->dataPtrFR != NULL;
	return s->dataPtrR != NULL;
}

void *ft2_sample_get_data_ptr(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel)
{
	if (s == NULL)
		return NULL;

	switch (format)
	{
		case FT2_SMP_FORMAT_24BIT:
			return rightChannel ? (void *)s->dataPtr24R : (void *)s->dataPtr24;
		case FT2_SMP_FORMAT_FLOAT32:
			return rightChannel ? (void *)s->dataPtrFR : (void *)s->dataPtrF;
		default:
			return rightChannel ? (void *)s->dataPtrR : (void *)s->dataPtr;
	}
}

const void *ft2_sample_get_data_ptr_const(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel)
{
	return ft2_sample_get_data_ptr((ft2_sample_t *)s, format, rightChannel);
}

float ft2_sample_get_float(const ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t pos)
{
	if (s == NULL || pos < 0 || pos >= s->length)
		return 0.0f;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		const int32_t *ptr = rightChannel ? s->dataPtr24R : s->dataPtr24;
		return (ptr != NULL) ? ((float)ptr[pos] * (1.0f / 8388608.0f)) : 0.0f;
	}

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		const float *ptr = rightChannel ? s->dataPtrFR : s->dataPtrF;
		return (ptr != NULL) ? ptr[pos] : 0.0f;
	}

	const int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
	if (ptr8 == NULL)
		ptr8 = s->dataPtr;
	if (ptr8 == NULL)
		return 0.0f;

	if (format == FT2_SMP_FORMAT_16BIT)
		return ((const int16_t *)ptr8)[pos] * (1.0f / 32768.0f);

	return ptr8[pos] * (1.0f / 128.0f);
}

void ft2_sample_set_float(ft2_sample_t *s, ft2_sample_format_t format, bool rightChannel, int32_t pos, float value)
{
	if (s == NULL || pos < 0 || pos >= s->length)
		return;

	if (value > 1.0f) value = 1.0f;
	if (value < -1.0f) value = -1.0f;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		int32_t *ptr = rightChannel ? s->dataPtr24R : s->dataPtr24;
		if (ptr == NULL) return;
		int32_t v = (int32_t)roundf(value * 8388607.0f);
		if (v > 8388607) v = 8388607;
		if (v < -8388608) v = -8388608;
		ptr[pos] = v;
		return;
	}

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		float *ptr = rightChannel ? s->dataPtrFR : s->dataPtrF;
		if (ptr == NULL) return;
		ptr[pos] = value;
		return;
	}

	int8_t *ptr8 = rightChannel ? s->dataPtrR : s->dataPtr;
	if (ptr8 == NULL)
		ptr8 = s->dataPtr;
	if (ptr8 == NULL)
		return;

	if (format == FT2_SMP_FORMAT_16BIT)
	{
		int32_t v = (int32_t)roundf(value * 32767.0f);
		if (v > 32767) v = 32767;
		if (v < -32768) v = -32768;
		((int16_t *)ptr8)[pos] = (int16_t)v;
	}
	else
	{
		int32_t v = (int32_t)roundf(value * 127.0f);
		if (v > 127) v = 127;
		if (v < -128) v = -128;
		ptr8[pos] = (int8_t)v;
	}
}

double ft2_sample_get_playback_rate(const ft2_instance_t *inst, const ft2_sample_t *s)
{
	if (s == NULL)
		return 0.0;

	if (s->sampleRate > 0)
		return (double)s->sampleRate;

	if (inst == NULL)
		return 0.0;

	const int32_t note = (4 * 12) + s->relativeNote;
	if (note < 0 || note >= (10 * 12) - 1)
		return 0.0;

	const int32_t c4Period = (note << 4) + (((int8_t)s->finetune >> 3) + 16);
	const uint32_t period = inst->audio.linearPeriodsFlag ? linearPeriodLUT[c4Period] : amigaPeriodLUT[c4Period];
	return period_to_hz(inst, period);
}

void ft2_sample_free_buffers(ft2_sample_t *s)
{
	if (s == NULL)
		return;

	if (s->origDataPtr) { free(s->origDataPtr); s->origDataPtr = NULL; }
	if (s->origDataPtrR) { free(s->origDataPtrR); s->origDataPtrR = NULL; }
	if (s->origDataPtr24) { free(s->origDataPtr24); s->origDataPtr24 = NULL; }
	if (s->origDataPtr24R) { free(s->origDataPtr24R); s->origDataPtr24R = NULL; }
	if (s->origDataPtrF) { free(s->origDataPtrF); s->origDataPtrF = NULL; }
	if (s->origDataPtrFR) { free(s->origDataPtrFR); s->origDataPtrFR = NULL; }

	s->dataPtr = NULL;
	s->dataPtrR = NULL;
	s->dataPtr24 = NULL;
	s->dataPtr24R = NULL;
	s->dataPtrF = NULL;
	s->dataPtrFR = NULL;
	s->isFixed = false;
	s->fixedPos = 0;
}

bool ft2_sample_alloc_buffers(ft2_sample_t *s, int32_t length, ft2_sample_format_t format, bool stereo)
{
	if (s == NULL)
		return false;

	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	const int32_t padding = (int32_t)(FT2_MAX_TAPS * bytesPerSample);
	const size_t allocSize = (size_t)padding + ((size_t)length * bytesPerSample) + (size_t)padding;

	void *origL = calloc(allocSize, 1);
	if (origL == NULL)
		return false;
	void *origR = NULL;

	if (stereo)
	{
		origR = calloc(allocSize, 1);
		if (origR == NULL)
		{
			free(origL);
			return false;
		}
	}

	uint8_t *dataL = (uint8_t *)origL + padding;
	uint8_t *dataR = stereo ? (uint8_t *)origR + padding : NULL;

	ft2_sample_free_buffers(s);

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		s->origDataPtr24 = (int32_t *)origL;
		s->dataPtr24 = (int32_t *)dataL;
		if (stereo)
		{
			s->origDataPtr24R = (int32_t *)origR;
			s->dataPtr24R = (int32_t *)dataR;
		}
	}
	else if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		s->origDataPtrF = (float *)origL;
		s->dataPtrF = (float *)dataL;
		if (stereo)
		{
			s->origDataPtrFR = (float *)origR;
			s->dataPtrFR = (float *)dataR;
		}
	}
	else
	{
		s->origDataPtr = (int8_t *)origL;
		s->dataPtr = (int8_t *)dataL;
		if (stereo)
		{
			s->origDataPtrR = (int8_t *)origR;
			s->dataPtrR = (int8_t *)dataR;
		}
	}

	s->format = format;
	if (format == FT2_SMP_FORMAT_16BIT)
		s->flags |= FT2_SAMPLE_16BIT;
	else
		s->flags &= ~FT2_SAMPLE_16BIT;

	if (stereo)
		s->flags |= FT2_SAMPLE_STEREO;
	else
		s->flags &= ~FT2_SAMPLE_STEREO;

	s->length = length;
	return true;
}

int32_t ft2_get_max_samples_per_instr(const ft2_instance_t *inst)
{
	if (inst != NULL && inst->config.enableXm2Features)
		return FT2_XM2_MAX_SMP_PER_INST;

	return FT2_XM_MAX_SMP_PER_INST;
}
