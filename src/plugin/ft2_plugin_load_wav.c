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
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_config.h"

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

/* ---------- Stereo sample loading helpers ---------- */

/* Forward declarations for dialog callbacks */
static void onStereoDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                  const char *inputText, void *userData);
static void onRadioDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                 const char *inputText, void *userData);

/* Clear pending stereo load state */
static void clearPendingStereoLoad(ft2_instance_t *inst)
{
	if (inst->diskop.pendingStereoData != NULL)
	{
		free(inst->diskop.pendingStereoData);
		inst->diskop.pendingStereoData = NULL;
	}
	inst->diskop.pendingStereoLoad = false;
	inst->diskop.pendingStereoDataSize = 0;
	inst->diskop.pendingStereoFilename[0] = '\0';
}

/* Cancel a pending stereo sample load */
void ft2_cancel_stereo_load(ft2_instance_t *inst)
{
	if (inst != NULL)
		clearPendingStereoLoad(inst);
}

/* Set filename for pending stereo load */
void ft2_set_pending_stereo_filename(ft2_instance_t *inst, const char *filename)
{
	if (inst == NULL || !inst->diskop.pendingStereoLoad || filename == NULL)
		return;

	strncpy(inst->diskop.pendingStereoFilename, filename, sizeof(inst->diskop.pendingStereoFilename) - 1);
	inst->diskop.pendingStereoFilename[sizeof(inst->diskop.pendingStereoFilename) - 1] = '\0';
}

/* Check if a stereo load is pending */
bool ft2_is_stereo_load_pending(ft2_instance_t *inst)
{
	return inst != NULL && inst->diskop.pendingStereoLoad;
}

/* Internal function to load a WAV sample with specified stereo mode */
static bool loadWavSampleWithMode(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                                   const uint8_t *data, uint32_t dataSize, stereo_load_mode_t mode)
{
	const uint8_t *audioData;
	uint32_t audioSize;
	int16_t channels, bitsPerSample;
	uint32_t sampleRate;

	if (!ft2_parse_wav(data, dataSize, &audioData, &audioSize,
	                   &channels, &bitsPerSample, &sampleRate))
		return false;

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
	if (smp->origDataPtrR != NULL)
	{
		free(smp->origDataPtrR);
		smp->origDataPtrR = NULL;
		smp->dataPtrR = NULL;
	}
	smp->flags &= ~FT2_SAMPLE_STEREO;

	/* Calculate sample length */
	int32_t srcBytesPerSample = (bitsPerSample == 16) ? 2 : 1;
	if (channels == 2)
		srcBytesPerSample *= 2;

	int32_t numSamples = audioSize / srcBytesPerSample;
	bool is16Bit = (bitsPerSample == 16);
	bool isStereo = (channels == 2);

	/* Determine output format based on mode */
	bool outputStereo = isStereo && (mode == STEREO_LOAD_AS_STEREO);

	/* Allocate sample data with proper padding for interpolation */
	int32_t dstBytesPerSample = is16Bit ? 2 : 1;
	int32_t leftPadding = FT2_MAX_TAPS * dstBytesPerSample;
	int32_t rightPadding = FT2_MAX_TAPS * dstBytesPerSample;
	int32_t dataLen = numSamples * dstBytesPerSample;
	int32_t allocSize = leftPadding + dataLen + rightPadding;

	/* Allocate left channel (or mono) */
	smp->origDataPtr = (int8_t *)calloc(allocSize, 1);
	if (smp->origDataPtr == NULL)
		return false;

	smp->dataPtr = smp->origDataPtr + leftPadding;

	/* Allocate right channel for stereo output */
	if (outputStereo)
	{
		smp->origDataPtrR = (int8_t *)calloc(allocSize, 1);
		if (smp->origDataPtrR == NULL)
		{
			free(smp->origDataPtr);
			smp->origDataPtr = smp->dataPtr = NULL;
			return false;
		}
		smp->dataPtrR = smp->origDataPtrR + leftPadding;
	}
	else
	{
		smp->origDataPtrR = NULL;
		smp->dataPtrR = NULL;
	}

	/* Convert audio data based on mode */
	if (!isStereo)
	{
		/* Mono input - just copy */
		memcpy(smp->dataPtr, audioData, dataLen);

		/* Convert 8-bit from unsigned (WAV) to signed (FT2 internal) */
		if (!is16Bit)
		{
			for (int32_t i = 0; i < numSamples; i++)
				smp->dataPtr[i] ^= 0x80;
		}
	}
	else
	{
		/* Stereo input - process based on mode */
		if (is16Bit)
		{
			int16_t *dstL = (int16_t *)smp->dataPtr;
			const int16_t *src = (const int16_t *)audioData;

			switch (mode)
			{
				case STEREO_LOAD_AS_STEREO:
				{
					int16_t *dstR = (int16_t *)smp->dataPtrR;
					for (int32_t i = 0; i < numSamples; i++)
					{
						dstL[i] = src[i * 2];
						dstR[i] = src[i * 2 + 1];
					}
					break;
				}
				case STEREO_LOAD_LEFT_ONLY:
					for (int32_t i = 0; i < numSamples; i++)
						dstL[i] = src[i * 2];
					break;
				case STEREO_LOAD_RIGHT_ONLY:
					for (int32_t i = 0; i < numSamples; i++)
						dstL[i] = src[i * 2 + 1];
					break;
				case STEREO_LOAD_CONVERT_MONO:
					for (int32_t i = 0; i < numSamples; i++)
						dstL[i] = (int16_t)((src[i * 2] + src[i * 2 + 1]) / 2);
					break;
			}
		}
		else
		{
			/* 8-bit: deinterleave and convert from unsigned to signed */
			const uint8_t *src = (const uint8_t *)audioData;

			switch (mode)
			{
				case STEREO_LOAD_AS_STEREO:
					for (int32_t i = 0; i < numSamples; i++)
					{
						smp->dataPtr[i] = src[i * 2] ^ 0x80;
						smp->dataPtrR[i] = src[i * 2 + 1] ^ 0x80;
					}
					break;
				case STEREO_LOAD_LEFT_ONLY:
					for (int32_t i = 0; i < numSamples; i++)
						smp->dataPtr[i] = src[i * 2] ^ 0x80;
					break;
				case STEREO_LOAD_RIGHT_ONLY:
					for (int32_t i = 0; i < numSamples; i++)
						smp->dataPtr[i] = src[i * 2 + 1] ^ 0x80;
					break;
				case STEREO_LOAD_CONVERT_MONO:
					for (int32_t i = 0; i < numSamples; i++)
					{
						int avg = ((src[i * 2] ^ 0x80) + (src[i * 2 + 1] ^ 0x80)) / 2;
						smp->dataPtr[i] = (int8_t)avg;
					}
					break;
			}
		}
	}

	smp->length = numSamples;
	smp->flags = is16Bit ? FT2_SAMPLE_16BIT : 0;
	if (outputStereo)
		smp->flags |= FT2_SAMPLE_STEREO;
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

	/* Mark song as using XM2 features if stereo sample loaded */
	if (outputStereo)
		inst->replayer.song.usesXm2Features = true;

	/* Sanitize and fix sample for interpolation */
	ft2_sanitize_sample(smp);
	ft2_fix_sample(smp);

	return true;
}

/* Complete a pending stereo sample load with specified mode */
bool ft2_complete_stereo_load(ft2_instance_t *inst, stereo_load_mode_t mode)
{
	if (inst == NULL || !inst->diskop.pendingStereoLoad)
		return false;

	bool result = loadWavSampleWithMode(inst,
	                                     inst->diskop.pendingStereoInstrNum,
	                                     inst->diskop.pendingStereoSampleNum,
	                                     inst->diskop.pendingStereoData,
	                                     inst->diskop.pendingStereoDataSize,
	                                     mode);

	/* Set sample name from filename if load succeeded */
	if (result && inst->diskop.pendingStereoFilename[0] != '\0')
	{
		ft2_set_sample_name_from_filename(inst,
		                                   inst->diskop.pendingStereoInstrNum,
		                                   inst->diskop.pendingStereoSampleNum,
		                                   inst->diskop.pendingStereoFilename);
	}

	clearPendingStereoLoad(inst);
	inst->uiState.updateSampleEditor = true;
	inst->uiState.updateInstrSwitcher = true;
	inst->uiState.needsFullRedraw = true;
	return result;
}

/* Stereo dialog callback (non-XM2 mode: 3 buttons) */
static void onStereoDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                  const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || !inst->diskop.pendingStereoLoad)
		return;

	stereo_load_mode_t mode;
	switch (result)
	{
		case DIALOG_RESULT_STEREO_LEFT:
			mode = STEREO_LOAD_LEFT_ONLY;
			break;
		case DIALOG_RESULT_STEREO_RIGHT:
			mode = STEREO_LOAD_RIGHT_ONLY;
			break;
		case DIALOG_RESULT_STEREO_CONVERT:
			mode = STEREO_LOAD_CONVERT_MONO;
			break;
		default:
			/* Should not happen - stereo dialog has no cancel */
			clearPendingStereoLoad(inst);
			return;
	}

	ft2_complete_stereo_load(inst, mode);
}

/* Radio dialog callback (XM2 mode: 4 options) */
static void onRadioDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                 const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || !inst->diskop.pendingStereoLoad)
		return;

	if (result == DIALOG_RESULT_CANCEL)
	{
		clearPendingStereoLoad(inst);
		return;
	}

	stereo_load_mode_t mode;
	switch (result)
	{
		case DIALOG_RESULT_RADIO_0:
			mode = STEREO_LOAD_AS_STEREO;
			break;
		case DIALOG_RESULT_RADIO_1:
			mode = STEREO_LOAD_LEFT_ONLY;
			break;
		case DIALOG_RESULT_RADIO_2:
			mode = STEREO_LOAD_RIGHT_ONLY;
			break;
		case DIALOG_RESULT_RADIO_3:
			mode = STEREO_LOAD_CONVERT_MONO;
			break;
		default:
			clearPendingStereoLoad(inst);
			return;
	}

	ft2_complete_stereo_load(inst, mode);
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

	if (format == FT2_FORMAT_WAV)
	{
		const uint8_t *audioData;
		uint32_t audioSize;
		int16_t channels, bitsPerSample;
		uint32_t sampleRate;

		if (!ft2_parse_wav(data, dataSize, &audioData, &audioSize,
		                   &channels, &bitsPerSample, &sampleRate))
			return false;

		bool isStereo = (channels == 2);

		/* Stereo sample detected - show dialog */
		if (isStereo)
		{
			ft2_ui_t *ui = (ft2_ui_t *)inst->ui;
			if (ui == NULL)
			{
				/* No UI available, default to convert mono */
				return loadWavSampleWithMode(inst, instrNum, sampleNum, data, dataSize,
				                              STEREO_LOAD_CONVERT_MONO);
			}

			/* Store pending load data */
			clearPendingStereoLoad(inst);
			inst->diskop.pendingStereoData = (uint8_t *)malloc(dataSize);
			if (inst->diskop.pendingStereoData == NULL)
				return false;

			memcpy(inst->diskop.pendingStereoData, data, dataSize);
			inst->diskop.pendingStereoDataSize = dataSize;
			inst->diskop.pendingStereoInstrNum = instrNum;
			inst->diskop.pendingStereoSampleNum = sampleNum;
			inst->diskop.pendingStereoLoad = true;
			/* Filename will be set separately by caller if needed */

			/* Show appropriate dialog based on XM2 config */
			if (inst->config.enableXm2Features)
			{
				/* XM2 enabled: show radio dialog with all 4 options */
				static const char *options[] = {
					"Load as stereo",
					"Load left channel only",
					"Load right channel only",
					"Convert to mono"
				};
				ft2_dialog_show_radio_cb(&ui->dialog, "Loading stereo sample",
				                          options, 4, 0, inst,
				                          onRadioDialogResult, NULL);
			}
			else
			{
				/* XM2 disabled: show 3-button dialog (matches standalone) */
				ft2_dialog_show_stereo_cb(&ui->dialog, "System request",
				                           "This is a stereo sample...",
				                           inst, onStereoDialogResult, NULL);
			}

			return true; /* Load is pending */
		}

		/* Mono sample - load directly */
		return loadWavSampleWithMode(inst, instrNum, sampleNum, data, dataSize,
		                              STEREO_LOAD_LEFT_ONLY);
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
	bool isStereo = (smp->flags & FT2_SAMPLE_STEREO) && smp->dataPtrR != NULL;
	int16_t numChannels = isStereo ? 2 : 1;

	if (!isStereo)
	{
		/* Mono: use original function */
		return ft2_create_wav(smp->dataPtr, smp->length, 1, bitsPerSample, 44100,
		                       outData, outSize);
	}

	/* Stereo: interleave L and R channels */
	int32_t bytesPerSample = (bitsPerSample / 8) * numChannels;
	int32_t dataSize = smp->length * bytesPerSample;
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

	memcpy(p, &numChannels, 2);
	p += 2;

	uint32_t sampleRate = 44100;
	memcpy(p, &sampleRate, 4);
	p += 4;

	uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
	memcpy(p, &byteRate, 4);
	p += 4;

	uint16_t blockAlign = numChannels * (bitsPerSample / 8);
	memcpy(p, &blockAlign, 2);
	p += 2;

	memcpy(p, &bitsPerSample, 2);
	p += 2;

	/* data chunk */
	memcpy(p, "data", 4);
	p += 4;

	memcpy(p, &dataSize, 4);
	p += 4;

	/* Interleave audio data (L,R,L,R,...) */
	if (bitsPerSample == 16)
	{
		int16_t *dst = (int16_t *)p;
		const int16_t *srcL = (const int16_t *)smp->dataPtr;
		const int16_t *srcR = (const int16_t *)smp->dataPtrR;
		for (int32_t i = 0; i < smp->length; i++)
		{
			dst[i * 2] = srcL[i];
			dst[i * 2 + 1] = srcR[i];
		}
	}
	else
	{
		/* Convert signed to unsigned for 8-bit WAV */
		uint8_t *dst = p;
		const int8_t *srcL = smp->dataPtr;
		const int8_t *srcR = smp->dataPtrR;
		for (int32_t i = 0; i < smp->length; i++)
		{
			dst[i * 2] = (uint8_t)(srcL[i] + 128);
			dst[i * 2 + 1] = (uint8_t)(srcR[i] + 128);
		}
	}

	*outSize = fileSize;
	return true;
}
