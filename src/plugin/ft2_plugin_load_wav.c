/**
 * @file ft2_plugin_load_wav.c
 * @brief WAV sample loading and saving functions.
 *
 * Handles loading WAV files into sample slots, saving samples as WAV,
 * and WAV file format parsing/creation utilities.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_plugin_load_wav.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_diskop.h"

#define NOTE_C4 48
#define C4_FREQ 8363.0

/* Set sample's relative note and finetune based on C-4 playback rate.
 * Matches standalone setSampleC4Hz() in ft2_replayer.c */
void ft2_set_sample_c4hz(ft2_sample_t *s, double dC4Hz)
{
	const double dC4PeriodOffset = (NOTE_C4 * 16) + 16;
	int32_t period = (int32_t)round(dC4PeriodOffset + (log2(dC4Hz / C4_FREQ) * 12.0 * 16.0));

	if (period < 0) period = 0;
	if (period > (12 * 16 * 10) - 1) period = (12 * 16 * 10) - 1;

	s->finetune = ((period & 31) - 16) << 3;
	s->relativeNote = (int8_t)(((period & ~31) >> 4) - NOTE_C4);
}

/* ---------- WAV helpers ---------- */

/* Parse WAV file: find fmt and data chunks, extract audio info */
bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                    const uint8_t **outAudioData, uint32_t *outAudioSize,
                    int16_t *outChannels, int16_t *outBitsPerSample,
                    uint32_t *outSampleRate)
{
	if (data == NULL || dataSize < 44) return false;
	if (memcmp(data, "RIFF", 4) != 0) return false;
	if (memcmp(data + 8, "WAVE", 4) != 0) return false;
	uint32_t pos = 12;
	bool foundFmt = false;
	bool foundData = false;

	int16_t channels = 0;
	int16_t bitsPerSample = 0;
	uint32_t sampleRate = 0;
	const uint8_t *audioData = NULL;
	uint32_t audioSize = 0;

	while (pos + 8 <= dataSize)
	{
		const char *chunkId = (const char *)(data + pos);
		uint32_t chunkSize;
		memcpy(&chunkSize, data + pos + 4, 4);

		if (memcmp(chunkId, "fmt ", 4) == 0)
		{
			if (pos + 8 + chunkSize > dataSize)
				return false;

			/* Audio format (1 = PCM) */
			uint16_t audioFormat;
			memcpy(&audioFormat, data + pos + 8, 2);
			if (audioFormat != 1)
				return false;  /* Only PCM supported */

			memcpy(&channels, data + pos + 10, 2);
			memcpy(&sampleRate, data + pos + 12, 4);
			memcpy(&bitsPerSample, data + pos + 22, 2);

			foundFmt = true;
		}
		else if (memcmp(chunkId, "data", 4) == 0)
		{
			if (pos + 8 + chunkSize > dataSize)
				chunkSize = dataSize - pos - 8;

			audioData = data + pos + 8;
			audioSize = chunkSize;
			foundData = true;
		}

		pos += 8 + chunkSize;
		if (chunkSize & 1)
			pos++;  /* Pad to word boundary */
	}

	if (!foundFmt || !foundData)
		return false;

	*outAudioData = audioData;
	*outAudioSize = audioSize;
	*outChannels = channels;
	*outBitsPerSample = bitsPerSample;
	*outSampleRate = sampleRate;

	return true;
}

bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                     int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                     uint8_t **outData, uint32_t *outSize)
{
	if (sampleData == NULL || sampleLength <= 0 || outData == NULL || outSize == NULL)
		return false;

	int32_t bytesPerSample = (bitsPerSample / 8) * channels;
	int32_t dataSize = sampleLength * bytesPerSample;
	int32_t fileSize = 44 + dataSize;

	*outData = (uint8_t *)malloc(fileSize);
	if (*outData == NULL)
		return false;

	uint8_t *p = *outData;

	/* RIFF header */
	memcpy(p, "RIFF", 4);
	p += 4;

	uint32_t riffSize = fileSize - 8;
	memcpy(p, &riffSize, 4);
	p += 4;

	memcpy(p, "WAVE", 4);
	p += 4;

	/* fmt chunk */
	memcpy(p, "fmt ", 4);
	p += 4;

	uint32_t fmtSize = 16;
	memcpy(p, &fmtSize, 4);
	p += 4;

	uint16_t audioFormat = 1;  /* PCM */
	memcpy(p, &audioFormat, 2);
	p += 2;

	memcpy(p, &channels, 2);
	p += 2;

	memcpy(p, &sampleRate, 4);
	p += 4;

	uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
	memcpy(p, &byteRate, 4);
	p += 4;

	uint16_t blockAlign = channels * (bitsPerSample / 8);
	memcpy(p, &blockAlign, 2);
	p += 2;

	memcpy(p, &bitsPerSample, 2);
	p += 2;

	/* data chunk */
	memcpy(p, "data", 4);
	p += 4;

	memcpy(p, &dataSize, 4);
	p += 4;

	/* Audio data */
	if (bitsPerSample == 8)
	{
		/* Convert signed to unsigned for WAV */
		for (int32_t i = 0; i < sampleLength; i++)
			p[i] = (uint8_t)(sampleData[i] + 128);
	}
	else
	{
		memcpy(p, sampleData, dataSize);
	}

	*outSize = fileSize;
	return true;
}

/* ---------- Sample load/save ---------- */

bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL || dataSize == 0)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	if (sampleNum < 0 || sampleNum >= 16)
		return false;

	ft2_file_format format = ft2_detect_format_by_header(data, dataSize);

	/* Ensure instrument exists */
	if (inst->replayer.instr[instrNum] == NULL)
	{
		if (!ft2_instance_alloc_instr(inst, instrNum))
			return false;
	}

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	ft2_sample_t *smp = &instr->smp[sampleNum];

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, smp);

	/* Free existing sample data */
	if (smp->origDataPtr != NULL)
	{
		free(smp->origDataPtr);
		smp->origDataPtr = NULL;
		smp->dataPtr = NULL;
	}

	if (format == FT2_FORMAT_WAV)
	{
		const uint8_t *audioData;
		uint32_t audioSize;
		int16_t channels, bitsPerSample;
		uint32_t sampleRate;

		if (!ft2_parse_wav(data, dataSize, &audioData, &audioSize,
		                   &channels, &bitsPerSample, &sampleRate))
			return false;

		/* Calculate sample length */
		int32_t srcBytesPerSample = (bitsPerSample == 16) ? 2 : 1;
		if (channels == 2)
			srcBytesPerSample *= 2;

		int32_t numSamples = audioSize / srcBytesPerSample;
		bool is16Bit = (bitsPerSample == 16);

		/* Allocate sample data with proper padding for interpolation */
		int32_t dstBytesPerSample = is16Bit ? 2 : 1;
		int32_t leftPadding = FT2_MAX_TAPS * dstBytesPerSample;
		int32_t rightPadding = FT2_MAX_TAPS * dstBytesPerSample;
		int32_t dataLen = numSamples * dstBytesPerSample;
		int32_t allocSize = leftPadding + dataLen + rightPadding;

		smp->origDataPtr = (int8_t *)calloc(allocSize, 1);
		if (smp->origDataPtr == NULL)
			return false;

		smp->dataPtr = smp->origDataPtr + leftPadding;

		/* Convert audio data */
		if (channels == 1)
		{
			/* Mono */
			memcpy(smp->dataPtr, audioData, dataLen);
		}
		else
		{
			/* Stereo - mix to mono */
			if (is16Bit)
			{
				int16_t *dst = (int16_t *)smp->dataPtr;
				const int16_t *src = (const int16_t *)audioData;
				for (int32_t i = 0; i < numSamples; i++)
				{
					int32_t left = src[i * 2];
					int32_t right = src[i * 2 + 1];
					dst[i] = (int16_t)((left + right) / 2);
				}
			}
			else
			{
				int8_t *dst = smp->dataPtr;
				const int8_t *src = (const int8_t *)audioData;
				for (int32_t i = 0; i < numSamples; i++)
				{
					int32_t left = src[i * 2];
					int32_t right = src[i * 2 + 1];
					dst[i] = (int8_t)((left + right) / 2);
				}
			}
		}

		smp->length = numSamples;
		smp->flags = is16Bit ? FT2_SAMPLE_16BIT : 0;
		smp->volume = 64;
		smp->panning = 128;
		smp->loopStart = 0;
		smp->loopLength = 0;

		/* Set relative note and finetune based on WAV sample rate */
		ft2_set_sample_c4hz(smp, (double)sampleRate);

		if (sampleNum >= instr->numSamples)
			instr->numSamples = sampleNum + 1;

		/* Set up note-to-sample mapping: all notes map to this sample */
		for (int i = 0; i < 96; i++)
			instr->note2SampleLUT[i] = (uint8_t)sampleNum;

		/* Sanitize and fix sample for interpolation */
		ft2_sanitize_sample(smp);
		ft2_fix_sample(smp);

		return true;
	}

	return false;
}

void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                        int16_t sampleNum, const char *filename)
{
	if (inst == NULL || filename == NULL)
		return;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return;

	if (sampleNum < 0 || sampleNum >= 16)
		return;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return;

	ft2_sample_t *smp = &instr->smp[sampleNum];

	/* Find last path separator */
	const char *nameStart = filename;
	const char *p = filename;
	while (*p)
	{
		if (*p == '/' || *p == '\\')
			nameStart = p + 1;
		p++;
	}

	/* Find extension dot */
	const char *extDot = NULL;
	p = nameStart;
	while (*p)
	{
		if (*p == '.')
			extDot = p;
		p++;
	}

	/* Calculate name length (without extension) */
	int32_t nameLen;
	if (extDot != NULL && extDot > nameStart)
		nameLen = (int32_t)(extDot - nameStart);
	else
		nameLen = (int32_t)strlen(nameStart);

	/* Copy name (max 22 chars) */
	if (nameLen > 22)
		nameLen = 22;

	memset(smp->name, 0, sizeof(smp->name));
	memcpy(smp->name, nameStart, nameLen);
}

bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	if (sampleNum < 0 || sampleNum >= 16)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return false;

	ft2_sample_t *smp = &instr->smp[sampleNum];
	if (smp->dataPtr == NULL || smp->length == 0)
		return false;

	int16_t bitsPerSample = (smp->flags & FT2_SAMPLE_16BIT) ? 16 : 8;

	return ft2_create_wav(smp->dataPtr, smp->length, 1, bitsPerSample, 44100,
	                       outData, outSize);
}
