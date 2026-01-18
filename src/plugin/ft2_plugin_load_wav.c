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
#include "ft2_plugin_common.h"
#include "ft2_plugin_sample_utils.h"
#ifdef HAS_LIBFLAC
#ifdef EXTERNAL_LIBFLAC
#include <FLAC/stream_decoder.h>
#else
#include "../libflac/FLAC/stream_decoder.h"
#endif
#endif
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_xm2_convert.h"

#define NOTE_C4 48
#define C4_FREQ 8363.0

/* Set sample's relative note and finetune based on C-4 playback rate. */
void ft2_set_sample_c4hz(ft2_sample_t *s, double dC4Hz)
{
	const double dC4PeriodOffset = (NOTE_C4 * 16) + 16;
	int32_t period = (int32_t)round(dC4PeriodOffset + (log2(dC4Hz / C4_FREQ) * 12.0 * 16.0));

	if (period < 0) period = 0;
	if (period > (12 * 16 * 10) - 1) period = (12 * 16 * 10) - 1;

	s->finetune = ((period & 31) - 16) << 3;
	s->relativeNote = (int8_t)(((period & ~31) >> 4) - NOTE_C4);
}

static ft2_sample_format_t resolve_sample_format(const ft2_sample_t *smp)
{
	if (smp->format == FT2_SMP_FORMAT_24BIT || smp->format == FT2_SMP_FORMAT_FLOAT32)
		return smp->format;

	return (smp->flags & FT2_SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
}

static bool sample_has_data(const ft2_sample_t *smp, ft2_sample_format_t format)
{
	switch (format)
	{
		case FT2_SMP_FORMAT_24BIT: return (smp->dataPtr24 != NULL);
		case FT2_SMP_FORMAT_FLOAT32: return (smp->dataPtrF != NULL);
		default: return (smp->dataPtr != NULL);
	}
}

static bool sample_has_stereo_data(const ft2_sample_t *smp, ft2_sample_format_t format)
{
	switch (format)
	{
		case FT2_SMP_FORMAT_24BIT: return (smp->dataPtr24R != NULL);
		case FT2_SMP_FORMAT_FLOAT32: return (smp->dataPtrFR != NULL);
		default: return (smp->dataPtrR != NULL);
	}
}

static float get_sample_float(const ft2_sample_t *smp, ft2_sample_format_t format, bool rightChannel, int32_t pos)
{
	if (smp == NULL || pos < 0 || pos >= smp->length)
		return 0.0f;

	if (format == FT2_SMP_FORMAT_24BIT)
	{
		const int32_t *ptr = rightChannel ? smp->dataPtr24R : smp->dataPtr24;
		if (ptr == NULL) ptr = smp->dataPtr24;
		if (ptr == NULL) return 0.0f;
		return ptr[pos] * (1.0f / 8388608.0f);
	}

	if (format == FT2_SMP_FORMAT_FLOAT32)
	{
		const float *ptr = rightChannel ? smp->dataPtrFR : smp->dataPtrF;
		if (ptr == NULL) ptr = smp->dataPtrF;
		return (ptr != NULL) ? ptr[pos] : 0.0f;
	}

	const int8_t *ptr8 = rightChannel ? smp->dataPtrR : smp->dataPtr;
	if (ptr8 == NULL) ptr8 = smp->dataPtr;
	if (ptr8 == NULL) return 0.0f;

	if (format == FT2_SMP_FORMAT_16BIT)
		return ((const int16_t *)ptr8)[pos] * (1.0f / 32768.0f);

	return ptr8[pos] * (1.0f / 128.0f);
}

/* ---------- Sample parsing helpers ---------- */

typedef struct ft2_pcm_info_t
{
	const uint8_t *data;
	uint32_t dataSize;
	double sampleRate;
	uint16_t channels;
	uint16_t bitsPerSample;
	bool isFloat;
	bool bigEndian;
	bool unsigned8;
	int32_t loopStart;
	int32_t loopLength;
	bool loopEnabled;
	bool hasVolume;
	uint8_t volume;
	bool hasPanning;
	uint8_t panning;
} ft2_pcm_info_t;

#ifdef HAS_LIBFLAC
typedef struct ft2_flac_ctx_t
{
	const uint8_t *data;
	uint32_t dataSize;
	uint32_t pos;

	uint32_t numChannels;
	uint32_t bitDepth;
	uint32_t sampleRate;
	uint32_t totalSamples;
	uint32_t samplesWritten;
	uint32_t bytesPerSample;
	uint32_t frameBytes;

	bool hasStreamInfo;
	bool hasError;

	int32_t loopStart;
	int32_t loopLength;
	bool loopEnabled;
	bool hasVolume;
	uint8_t volume;
	bool hasPanning;
	uint8_t panning;

	/* Decode output */
	uint8_t *pcmData;
	size_t pcmDataSize;
} ft2_flac_ctx_t;
#endif

static uint16_t read_be16(const uint8_t *p)
{
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t read_le16(const uint8_t *p)
{
	return (uint16_t)((p[1] << 8) | p[0]);
}

static uint32_t read_le32(const uint8_t *p)
{
	return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

static uint64_t read_be64(const uint8_t *p)
{
	return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
	       ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
}

static uint32_t abs32(int32_t v)
{
	return (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
}

#ifdef HAS_LIBFLAC
static bool flacAllocPcm(ft2_flac_ctx_t *ctx)
{
	if (ctx == NULL || ctx->pcmData != NULL || ctx->totalSamples == 0 || ctx->frameBytes == 0)
		return false;

	size_t allocSize = (size_t)ctx->totalSamples * (size_t)ctx->frameBytes;
	if (allocSize == 0 || allocSize / ctx->frameBytes != ctx->totalSamples)
		return false;

	ctx->pcmData = (uint8_t *)malloc(allocSize);
	if (ctx->pcmData == NULL)
		return false;

	ctx->pcmDataSize = allocSize;
	return true;
}

static FLAC__StreamDecoderReadStatus flacReadCb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || buffer == NULL || bytes == NULL)
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

	if (*bytes == 0)
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

	uint32_t remaining = (ctx->pos < ctx->dataSize) ? (ctx->dataSize - ctx->pos) : 0;
	if (remaining == 0)
	{
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	uint32_t toRead = (uint32_t)*bytes;
	if (toRead > remaining)
		toRead = remaining;

	memcpy(buffer, ctx->data + ctx->pos, toRead);
	ctx->pos += toRead;
	*bytes = toRead;
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus flacSeekCb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || absolute_byte_offset > ctx->dataSize)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	ctx->pos = (uint32_t)absolute_byte_offset;
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus flacTellCb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || absolute_byte_offset == NULL)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	*absolute_byte_offset = ctx->pos;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus flacLengthCb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || stream_length == NULL)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	*stream_length = ctx->dataSize;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flacEofCb(const FLAC__StreamDecoder *decoder, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	return (ctx == NULL || ctx->pos >= ctx->dataSize);
}

static void flacMetadataCb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || metadata == NULL)
		return;

	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && metadata->data.stream_info.total_samples != 0)
	{
		ctx->bitDepth = metadata->data.stream_info.bits_per_sample;
		ctx->numChannels = metadata->data.stream_info.channels;
		ctx->sampleRate = metadata->data.stream_info.sample_rate;
		ctx->totalSamples = (uint32_t)MIN(metadata->data.stream_info.total_samples, (FLAC__uint64)FT2_MAX_SAMPLE_LEN);
		ctx->hasStreamInfo = true;
	}
	else if (metadata->type == FLAC__METADATA_TYPE_APPLICATION && metadata->length >= 8 &&
	         !memcmp(metadata->data.application.id, "riff", 4))
	{
		const uint8_t *data = (const uint8_t *)metadata->data.application.data;
		uint32_t remaining = metadata->length;
		uint32_t chunkID = read_le32(data); data += 4; remaining -= 4;
		uint32_t chunkLen = read_le32(data); data += 4; remaining -= 4;
		if (chunkLen > remaining)
			chunkLen = remaining;

		if (chunkID == 0x61727478 && chunkLen >= 8) /* "xtra" */
		{
			uint32_t xtraFlags = read_le32(data); data += 4;
			if (xtraFlags & 0x20)
			{
				uint16_t tmpPan = read_le16(data);
				if (tmpPan > 255)
					tmpPan = 255;
				ctx->panning = (uint8_t)tmpPan;
				ctx->hasPanning = true;
			}
			data += 2;
			uint16_t tmpVol = read_le16(data);
			if (tmpVol > 256)
				tmpVol = 256;
			ctx->volume = (uint8_t)((tmpVol + 2) / 4);
			ctx->hasVolume = true;
		}
		else if (chunkID == 0x6C706D73 && chunkLen > 52) /* "smpl" */
		{
			data += 28;
			uint32_t numLoops = read_le32(data); data += 4;
			if (numLoops == 1)
			{
				data += 8;
				uint32_t loopType = read_le32(data); data += 4;
				uint32_t loopStart = read_le32(data); data += 4;
				uint32_t loopEnd = read_le32(data); data += 4;

				ctx->loopStart = (int32_t)loopStart;
				ctx->loopLength = (int32_t)((loopEnd + 1) - loopStart);
				ctx->loopEnabled = (loopType == 0) || (loopType == 1);
			}
		}
	}
	else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
	{
		uint32_t tmpSampleRate = 0;
		uint32_t loopStart = 0;
		uint32_t loopLength = 0;
		for (uint32_t i = 0; i < metadata->data.vorbis_comment.num_comments; i++)
		{
			const char *tag = (const char *)metadata->data.vorbis_comment.comments[i].entry;
			uint32_t length = metadata->data.vorbis_comment.comments[i].length;

			if (length > 11 && !memcmp(tag, "SAMPLERATE=", 11))
				tmpSampleRate = (uint32_t)atoi(&tag[11]);
			else if (length > 10 && !memcmp(tag, "LOOPSTART=", 10))
				loopStart = (uint32_t)atoi(&tag[10]);
			else if (length > 11 && !memcmp(tag, "LOOPLENGTH=", 11))
				loopLength = (uint32_t)atoi(&tag[11]);
		}

		if (loopLength > 0)
		{
			ctx->loopStart = (int32_t)loopStart;
			ctx->loopLength = (int32_t)loopLength;
			ctx->loopEnabled = true;
		}

		if (tmpSampleRate > 0)
			ctx->sampleRate = tmpSampleRate;
	}
}

static FLAC__StreamDecoderWriteStatus flacWriteCb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
                                                   const FLAC__int32 *const buffer[], void *client_data)
{
	(void)decoder;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx == NULL || frame == NULL || buffer == NULL)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (!ctx->hasStreamInfo || ctx->numChannels == 0 || ctx->totalSamples == 0 || ctx->pcmData == NULL)
	{
		ctx->hasError = true;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (ctx->numChannels > 2)
	{
		ctx->hasError = true;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (ctx->bitDepth != 8 && ctx->bitDepth != 16 && ctx->bitDepth != 24)
	{
		ctx->hasError = true;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	uint32_t blockSize = frame->header.blocksize;
	if (ctx->samplesWritten + blockSize > ctx->totalSamples)
		blockSize = ctx->totalSamples - ctx->samplesWritten;
	if (blockSize == 0)
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	const FLAC__int32 *src32L = buffer[0];
	const FLAC__int32 *src32R = (ctx->numChannels == 2) ? buffer[1] : NULL;
	uint8_t *dst = ctx->pcmData + ((size_t)ctx->samplesWritten * ctx->frameBytes);

	if (ctx->bytesPerSample == 1)
	{
		int8_t *dst8 = (int8_t *)dst;
		if (ctx->numChannels == 1)
		{
			for (uint32_t i = 0; i < blockSize; i++)
				dst8[i] = (int8_t)src32L[i];
		}
		else
		{
			for (uint32_t i = 0; i < blockSize; i++)
			{
				dst8[i * 2] = (int8_t)src32L[i];
				dst8[i * 2 + 1] = (int8_t)src32R[i];
			}
		}
	}
	else
	{
		int16_t *dst16 = (int16_t *)dst;
		const int shift = (ctx->bitDepth == 24) ? 8 : 0;

		if (ctx->numChannels == 1)
		{
			for (uint32_t i = 0; i < blockSize; i++)
				dst16[i] = (int16_t)(src32L[i] >> shift);
		}
		else
		{
			for (uint32_t i = 0; i < blockSize; i++)
			{
				dst16[i * 2] = (int16_t)(src32L[i] >> shift);
				dst16[i * 2 + 1] = (int16_t)(src32R[i] >> shift);
			}
		}
	}

	ctx->samplesWritten += blockSize;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flacErrorCb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder;
	(void)status;
	ft2_flac_ctx_t *ctx = (ft2_flac_ctx_t *)client_data;
	if (ctx != NULL)
		ctx->hasError = true;
}

static bool flacProbeInfo(const uint8_t *data, uint32_t dataSize, ft2_flac_ctx_t *ctx)
{
	if (data == NULL || dataSize < 4 || ctx == NULL)
		return false;

	if (memcmp(data, "fLaC", 4) != 0)
		return false;

	memset(ctx, 0, sizeof (*ctx));
	ctx->data = data;
	ctx->dataSize = dataSize;

	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
	if (decoder == NULL)
		return false;

	FLAC__stream_decoder_set_metadata_respond_all(decoder);
	FLAC__StreamDecoderInitStatus initStatus = FLAC__stream_decoder_init_stream(
		decoder,
		flacReadCb,
		flacSeekCb,
		flacTellCb,
		flacLengthCb,
		flacEofCb,
		flacWriteCb,
		flacMetadataCb,
		flacErrorCb,
		ctx);

	if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		FLAC__stream_decoder_delete(decoder);
		return false;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder))
		ctx->hasError = true;

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);

	return (!ctx->hasError && ctx->hasStreamInfo && ctx->totalSamples > 0 && ctx->numChannels > 0);
}

static bool decodeFlacToPcm(const uint8_t *data, uint32_t dataSize, ft2_pcm_info_t *out)
{
	if (data == NULL || dataSize < 4 || out == NULL)
		return false;

	if (memcmp(data, "fLaC", 4) != 0)
		return false;

	ft2_flac_ctx_t ctx;
	if (!flacProbeInfo(data, dataSize, &ctx))
		return false;

	if (ctx.numChannels > 2)
		return false;
	if (ctx.bitDepth != 8 && ctx.bitDepth != 16 && ctx.bitDepth != 24)
		return false;

	ctx.pos = 0;
	ctx.samplesWritten = 0;
	ctx.hasError = false;

	ctx.bytesPerSample = (ctx.bitDepth == 8) ? 1 : 2;
	ctx.frameBytes = ctx.bytesPerSample * ctx.numChannels;

	if (!flacAllocPcm(&ctx))
	{
		free(ctx.pcmData);
		return false;
	}

	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
	if (decoder == NULL)
	{
		free(ctx.pcmData);
		return false;
	}

	FLAC__stream_decoder_set_metadata_respond_all(decoder);
	FLAC__StreamDecoderInitStatus initStatus = FLAC__stream_decoder_init_stream(
		decoder,
		flacReadCb,
		flacSeekCb,
		flacTellCb,
		flacLengthCb,
		flacEofCb,
		flacWriteCb,
		flacMetadataCb,
		flacErrorCb,
		&ctx);

	if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		FLAC__stream_decoder_delete(decoder);
		free(ctx.pcmData);
		return false;
	}

	if (!FLAC__stream_decoder_process_until_end_of_stream(decoder))
		ctx.hasError = true;

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);

	if (ctx.hasError || ctx.samplesWritten == 0)
	{
		free(ctx.pcmData);
		return false;
	}

	out->sampleRate = (double)ctx.sampleRate;
	out->loopStart = ctx.loopStart;
	out->loopLength = ctx.loopLength;
	out->loopEnabled = ctx.loopEnabled;
	out->hasVolume = ctx.hasVolume;
	out->volume = ctx.volume;
	out->hasPanning = ctx.hasPanning;
	out->panning = ctx.panning;
	out->bigEndian = false;
	out->unsigned8 = false;
	out->isFloat = false;

	out->channels = (uint16_t)ctx.numChannels;
	out->bitsPerSample = (ctx.bytesPerSample == 1) ? 8 : 16;
	out->data = (const uint8_t *)ctx.pcmData;
	out->dataSize = ctx.samplesWritten * ctx.frameBytes;

	return true;
}
#endif

static int16_t read_s16(const uint8_t *p, bool bigEndian)
{
	if (bigEndian)
		return (int16_t)((p[0] << 8) | p[1]);
	return (int16_t)((p[1] << 8) | p[0]);
}

static int32_t read_s24(const uint8_t *p, bool bigEndian)
{
	uint32_t v;
	if (bigEndian)
		v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
	else
		v = ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];

	return (v & 0x800000) ? (int32_t)(v | 0xFF000000) : (int32_t)v;
}

static int32_t read_s32(const uint8_t *p, bool bigEndian)
{
	if (bigEndian)
		return (int32_t)read_be32(p);
	return (int32_t)((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static float read_f32(const uint8_t *p, bool bigEndian)
{
	uint32_t v = bigEndian ? read_be32(p) : ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
	float f;
	memcpy(&f, &v, sizeof (f));
	return f;
}

static double read_f64(const uint8_t *p, bool bigEndian)
{
	uint64_t v;
	if (bigEndian)
	{
		v = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
		    ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
	}
	else
	{
		v = ((uint64_t)p[7] << 56) | ((uint64_t)p[6] << 48) | ((uint64_t)p[5] << 40) | ((uint64_t)p[4] << 32) |
		    ((uint64_t)p[3] << 24) | ((uint64_t)p[2] << 16) | ((uint64_t)p[1] << 8) | p[0];
	}

	double d;
	memcpy(&d, &v, sizeof (d));
	return d;
}

static void normalizeSigned32Bit(int32_t *sampleData, uint32_t sampleLength)
{
	uint32_t sampleVolPeak = 0;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		const uint32_t sample = abs32(sampleData[i]);
		if (sampleVolPeak < sample)
			sampleVolPeak = sample;
	}

	if (sampleVolPeak == 0)
		return;

	const double dGain = (double)INT32_MAX / sampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
		sampleData[i] = (int32_t)(sampleData[i] * dGain);
}

static void normalizeSigned32BitStereo(int32_t *left, int32_t *right, uint32_t sampleLength)
{
	uint32_t sampleVolPeak = 0;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		uint32_t sample = abs32(left[i]);
		if (sampleVolPeak < sample)
			sampleVolPeak = sample;
		sample = abs32(right[i]);
		if (sampleVolPeak < sample)
			sampleVolPeak = sample;
	}

	if (sampleVolPeak == 0)
		return;

	const double dGain = (double)INT32_MAX / sampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		left[i] = (int32_t)(left[i] * dGain);
		right[i] = (int32_t)(right[i] * dGain);
	}
}

static void normalize32BitFloatToSigned16Bit(float *fSampleData, uint32_t sampleLength)
{
	float fSampleVolPeak = 0.0f;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		const float fSample = fabsf(fSampleData[i]);
		if (fSampleVolPeak < fSample)
			fSampleVolPeak = fSample;
	}

	if (fSampleVolPeak <= 0.0f)
		return;

	const float fGain = (float)INT16_MAX / fSampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
		fSampleData[i] *= fGain;
}

static void normalize64BitFloatToSigned16Bit(double *dSampleData, uint32_t sampleLength)
{
	double dSampleVolPeak = 0.0;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		const double dSample = fabs(dSampleData[i]);
		if (dSampleVolPeak < dSample)
			dSampleVolPeak = dSample;
	}

	if (dSampleVolPeak <= 0.0)
		return;

	const double dGain = (double)INT16_MAX / dSampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
		dSampleData[i] *= dGain;
}

static void normalize32BitFloatStereo(float *left, float *right, uint32_t sampleLength)
{
	float fSampleVolPeak = 0.0f;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		const float fSampleL = fabsf(left[i]);
		const float fSampleR = fabsf(right[i]);
		if (fSampleVolPeak < fSampleL)
			fSampleVolPeak = fSampleL;
		if (fSampleVolPeak < fSampleR)
			fSampleVolPeak = fSampleR;
	}

	if (fSampleVolPeak <= 0.0f)
		return;

	const float fGain = (float)INT16_MAX / fSampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		left[i] *= fGain;
		right[i] *= fGain;
	}
}

static void normalize64BitFloatStereo(double *left, double *right, uint32_t sampleLength)
{
	double dSampleVolPeak = 0.0;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		const double dSampleL = fabs(left[i]);
		const double dSampleR = fabs(right[i]);
		if (dSampleVolPeak < dSampleL)
			dSampleVolPeak = dSampleL;
		if (dSampleVolPeak < dSampleR)
			dSampleVolPeak = dSampleR;
	}

	if (dSampleVolPeak <= 0.0)
		return;

	const double dGain = (double)INT16_MAX / dSampleVolPeak;
	for (uint32_t i = 0; i < sampleLength; i++)
	{
		left[i] *= dGain;
		right[i] *= dGain;
	}
}

static double getAIFFSampleRate(const uint8_t *in)
{
	/* 80-bit IEEE-754 to double (sign bit ignored) */
#define EXP_BIAS 16383
	const uint16_t exp15 = (uint16_t)(((in[0] & 0x7F) << 8) | in[1]);
	const uint64_t mantissa63 = read_be64(&in[2]) & INT64_MAX;

	const double dExp = exp15 - EXP_BIAS;
	const double dMantissa = mantissa63 / (INT64_MAX + 1.0);
	return (1.0 + dMantissa) * exp2(dExp);
#undef EXP_BIAS
}

#define WAV_FORMAT_PCM 1
#define WAV_FORMAT_IEEE_FLOAT 3
#define WAV_FORMAT_EXTENSIBLE 0xFFFE

static const uint8_t wavSubFormatPcm[16] = {
	0x01, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x10, 0x00,
	0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

static const uint8_t wavSubFormatFloat[16] = {
	0x03, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x10, 0x00,
	0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
};

static bool parseWavToPcm(const uint8_t *data, uint32_t dataSize, ft2_pcm_info_t *out)
{
	const uint8_t *audioData = NULL;
	uint32_t audioSize = 0;
	int16_t channels = 0;
	int16_t bitsPerSample = 0;
	uint32_t sampleRate = 0;
	bool isFloat = false;

	if (!ft2_parse_wav(data, dataSize, &audioData, &audioSize, &channels, &bitsPerSample, &sampleRate, &isFloat))
		return false;

	out->data = audioData;
	out->dataSize = audioSize;
	out->sampleRate = (double)sampleRate;
	out->channels = (uint16_t)channels;
	out->bitsPerSample = (uint16_t)bitsPerSample;
	out->isFloat = isFloat;
	out->bigEndian = false;
	out->unsigned8 = (!isFloat && bitsPerSample == 8);
	out->loopStart = 0;
	out->loopLength = 0;
	out->loopEnabled = false;
	return true;
}

static bool parseAiffToPcm(const uint8_t *data, uint32_t dataSize, ft2_pcm_info_t *out)
{
	if (dataSize < 12) return false;
	if (memcmp(data, "FORM", 4) != 0) return false;
	if (memcmp(data + 8, "AIFF", 4) != 0 && memcmp(data + 8, "AIFC", 4) != 0) return false;

	uint32_t commPtr = 0, commLen = 0;
	uint32_t ssndPtr = 0, ssndLen = 0;
	uint32_t pos = 12;

	while (pos + 8 <= dataSize)
	{
		const uint32_t chunkId = read_be32(&data[pos]);
		const uint32_t chunkSize = read_be32(&data[pos + 4]);
		pos += 8;

		if (pos + chunkSize > dataSize)
			break;

		switch (chunkId)
		{
			case 0x434F4D4D: commPtr = pos; commLen = chunkSize; break; /* COMM */
			case 0x53534E44: ssndPtr = pos; ssndLen = chunkSize; break; /* SSND */
			default: break;
		}

		pos += chunkSize + (chunkSize & 1);
	}

	if (commPtr == 0 || commLen < 18 || ssndPtr == 0)
		return false;

	uint16_t numChannels = read_be16(&data[commPtr + 0]);
	uint16_t bitDepth = read_be16(&data[commPtr + 6]);
	double sampleRate = getAIFFSampleRate(&data[commPtr + 8]);

	if (numChannels != 1 && numChannels != 2)
		return false;

	bool signedSample = true;
	bool floatSample = false;
	if (commLen > 18)
	{
		if (commPtr + 18 + 4 > dataSize)
			return false;

		const uint8_t *compType = &data[commPtr + 18];
		if (memcmp(compType, "raw ", 4) == 0)
		{
			signedSample = false;
		}
		else if (memcmp(compType, "FL32", 4) == 0 || memcmp(compType, "fl32", 4) == 0 ||
		         memcmp(compType, "FL64", 4) == 0 || memcmp(compType, "fl64", 4) == 0)
		{
			floatSample = true;
		}
		else
		{
			return false;
		}
	}

	if (!floatSample && (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32))
		return false;
	if (floatSample && !(bitDepth == 32 || bitDepth == 64))
		return false;

	if (ssndLen == 0)
		ssndLen = dataSize - ssndPtr;
	if (ssndPtr + ssndLen > dataSize)
		ssndLen = dataSize - ssndPtr;

	if (ssndPtr + 8 > dataSize)
		return false;

	const uint32_t offset = read_be32(&data[ssndPtr + 0]);
	if (offset > 0)
		return false;

	const uint32_t audioOffset = ssndPtr + 8;
	uint32_t audioSize = (ssndLen >= 8) ? ssndLen - 8 : 0;
	if (audioOffset + audioSize > dataSize)
		audioSize = dataSize - audioOffset;

	out->data = &data[audioOffset];
	out->dataSize = audioSize;
	out->sampleRate = sampleRate;
	out->channels = numChannels;
	out->bitsPerSample = bitDepth;
	out->isFloat = floatSample;
	out->bigEndian = true;
	out->unsigned8 = !signedSample;
	out->loopStart = 0;
	out->loopLength = 0;
	out->loopEnabled = false;
	return true;
}

static bool parseIffToPcm(const uint8_t *data, uint32_t dataSize, ft2_pcm_info_t *out)
{
	if (dataSize < 12) return false;
	if (memcmp(data, "FORM", 4) != 0) return false;

	bool sample16Bit = false;
	if (memcmp(data + 8, "16SV", 4) == 0)
		sample16Bit = true;
	else if (memcmp(data + 8, "8SVX", 4) != 0)
		return false;

	uint32_t vhdrPtr = 0, vhdrLen = 0;
	uint32_t bodyPtr = 0, bodyLen = 0;

	uint32_t pos = 12;
	while (pos + 8 <= dataSize)
	{
		const uint32_t chunkId = read_be32(&data[pos]);
		const uint32_t chunkSize = read_be32(&data[pos + 4]);
		pos += 8;

		if (pos + chunkSize > dataSize)
			break;

		switch (chunkId)
		{
			case 0x56484452: vhdrPtr = pos; vhdrLen = chunkSize; break; /* VHDR */
			case 0x424F4459: bodyPtr = pos; bodyLen = chunkSize; break; /* BODY */
			default: break;
		}

		pos += chunkSize + (chunkSize & 1);
	}

	if (vhdrPtr == 0 || vhdrLen < 20 || bodyPtr == 0)
		return false;

	if (bodyLen == 0)
		bodyLen = dataSize - bodyPtr;
	if (bodyPtr + bodyLen > dataSize)
		bodyLen = dataSize - bodyPtr;

	uint32_t loopStart = read_be32(&data[vhdrPtr + 0]);
	uint32_t loopLength = read_be32(&data[vhdrPtr + 4]);
	uint32_t sampleRate = read_be16(&data[vhdrPtr + 12]);
	const uint8_t compression = data[vhdrPtr + 15];
	if (compression != 0)
		return false;

	uint32_t length = bodyLen;
	if (sample16Bit)
	{
		length >>= 1;
		loopStart >>= 1;
		loopLength >>= 1;
	}

	out->data = &data[bodyPtr];
	out->dataSize = bodyLen;
	out->sampleRate = (double)sampleRate;
	out->channels = 1;
	out->bitsPerSample = sample16Bit ? 16 : 8;
	out->isFloat = false;
	out->bigEndian = false;
	out->unsigned8 = false;
	out->loopStart = (int32_t)loopStart;
	out->loopLength = (int32_t)loopLength;
	out->loopEnabled = (loopLength > 0);
	return true;
}

/* ---------- WAV helpers ---------- */

/* Parse WAV file: find fmt and data chunks, extract audio info */
bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                    const uint8_t **outAudioData, uint32_t *outAudioSize,
                    int16_t *outChannels, int16_t *outBitsPerSample,
                    uint32_t *outSampleRate, bool *outIsFloat)
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
	uint16_t audioFormat = 0;
	bool isFloat = false;

	while (pos + 8 <= dataSize)
	{
		const char *chunkId = (const char *)(data + pos);
		uint32_t chunkSize;
		memcpy(&chunkSize, data + pos + 4, 4);

		if (memcmp(chunkId, "fmt ", 4) == 0)
		{
			if (pos + 8 + chunkSize > dataSize)
				return false;
			if (chunkSize < 16)
				return false;

			/* Audio format (1 = PCM) */
			memcpy(&audioFormat, data + pos + 8, 2);
			if (audioFormat != 1)
			{
				if (audioFormat != WAV_FORMAT_IEEE_FLOAT && audioFormat != WAV_FORMAT_EXTENSIBLE)
					return false;
			}

			memcpy(&channels, data + pos + 10, 2);
			memcpy(&sampleRate, data + pos + 12, 4);
			memcpy(&bitsPerSample, data + pos + 22, 2);

			if (audioFormat == WAV_FORMAT_EXTENSIBLE)
			{
				if (chunkSize < 40)
					return false;

				const uint8_t *subFormat = data + pos + 8 + 24;
				if (memcmp(subFormat, wavSubFormatPcm, sizeof(wavSubFormatPcm)) == 0)
				{
					audioFormat = WAV_FORMAT_PCM;
					isFloat = false;
				}
				else if (memcmp(subFormat, wavSubFormatFloat, sizeof(wavSubFormatFloat)) == 0)
				{
					audioFormat = WAV_FORMAT_IEEE_FLOAT;
					isFloat = true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				isFloat = (audioFormat == WAV_FORMAT_IEEE_FLOAT);
			}

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

	if (isFloat)
	{
		if (!(bitsPerSample == 32 || bitsPerSample == 64))
			return false;
	}
	else
	{
		if (!(bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32))
			return false;
	}

	*outAudioData = audioData;
	*outAudioSize = audioSize;
	*outChannels = channels;
	*outBitsPerSample = bitsPerSample;
	*outSampleRate = sampleRate;
	if (outIsFloat != NULL)
		*outIsFloat = isFloat;

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

static uint32_t getBytesPerSample(uint16_t bitsPerSample)
{
	return (bitsPerSample == 24) ? 3 : (uint32_t)(bitsPerSample / 8);
}

static void freeSampleData(ft2_sample_t *smp)
{
	if (smp->origDataPtr != NULL)
	{
		free(smp->origDataPtr);
		smp->origDataPtr = NULL;
		smp->dataPtr = NULL;
	}

	if (smp->origDataPtr24 != NULL)
	{
		free(smp->origDataPtr24);
		smp->origDataPtr24 = NULL;
		smp->dataPtr24 = NULL;
	}

	if (smp->origDataPtrF != NULL)
	{
		free(smp->origDataPtrF);
		smp->origDataPtrF = NULL;
		smp->dataPtrF = NULL;
	}

	if (smp->origDataPtrR != NULL)
	{
		free(smp->origDataPtrR);
		smp->origDataPtrR = NULL;
		smp->dataPtrR = NULL;
	}

	if (smp->origDataPtr24R != NULL)
	{
		free(smp->origDataPtr24R);
		smp->origDataPtr24R = NULL;
		smp->dataPtr24R = NULL;
	}

	if (smp->origDataPtrFR != NULL)
	{
		free(smp->origDataPtrFR);
		smp->origDataPtrFR = NULL;
		smp->dataPtrFR = NULL;
	}
}

static bool loadSampleFromPcm(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                               const ft2_pcm_info_t *pcm, stereo_load_mode_t mode)
{
	if (inst == NULL || pcm == NULL)
		return false;

	if (pcm->channels < 1 || pcm->channels > 2)
		return false;

	if (pcm->bitsPerSample != 8 && pcm->bitsPerSample != 16 &&
	    pcm->bitsPerSample != 24 && pcm->bitsPerSample != 32 && pcm->bitsPerSample != 64)
		return false;

	if (pcm->isFloat && (pcm->bitsPerSample != 32 && pcm->bitsPerSample != 64))
		return false;

	const uint32_t bytesPerSample = getBytesPerSample(pcm->bitsPerSample);
	if (bytesPerSample == 0)
		return false;

	const uint32_t frameBytes = bytesPerSample * pcm->channels;
	if (frameBytes == 0 || pcm->dataSize < frameBytes)
		return false;

	uint32_t sampleLength = pcm->dataSize / frameBytes;
	if (sampleLength > FT2_MAX_SAMPLE_LEN)
		sampleLength = FT2_MAX_SAMPLE_LEN;
	if (sampleLength == 0)
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
	freeSampleData(smp);
	smp->flags &= ~FT2_SAMPLE_STEREO;
	smp->flags &= ~FT2_SAMPLE_16BIT;

	const bool outputStereo = (pcm->channels == 2 && mode == STEREO_LOAD_AS_STEREO);
	const bool xm2Enabled = inst->config.enableXm2Features;
	ft2_sample_format_t dstFormat;
	if (xm2Enabled)
	{
		if (pcm->isFloat)
			dstFormat = FT2_SMP_FORMAT_FLOAT32;
		else if (pcm->bitsPerSample <= 8)
			dstFormat = FT2_SMP_FORMAT_8BIT;
		else if (pcm->bitsPerSample <= 16)
			dstFormat = FT2_SMP_FORMAT_16BIT;
		else
			dstFormat = FT2_SMP_FORMAT_24BIT;
	}
	else
	{
		dstFormat = (pcm->isFloat || pcm->bitsPerSample > 8) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
	}

	const uint32_t dstBytesPerSample = ft2_sample_format_bytes(dstFormat);
	const int32_t leftPadding = (int32_t)(FT2_MAX_TAPS * dstBytesPerSample);
	const int32_t rightPadding = (int32_t)(FT2_MAX_TAPS * dstBytesPerSample);
	const int32_t dataLen = (int32_t)sampleLength * (int32_t)dstBytesPerSample;
	const int32_t allocSize = leftPadding + dataLen + rightPadding;

	if (dstFormat == FT2_SMP_FORMAT_24BIT)
	{
		smp->origDataPtr24 = (int32_t *)calloc((size_t)allocSize, 1);
		if (smp->origDataPtr24 == NULL)
			return false;
		smp->dataPtr24 = (int32_t *)((uint8_t *)smp->origDataPtr24 + leftPadding);

		if (outputStereo)
		{
			smp->origDataPtr24R = (int32_t *)calloc((size_t)allocSize, 1);
			if (smp->origDataPtr24R == NULL)
			{
				freeSampleData(smp);
				return false;
			}
			smp->dataPtr24R = (int32_t *)((uint8_t *)smp->origDataPtr24R + leftPadding);
		}
	}
	else if (dstFormat == FT2_SMP_FORMAT_FLOAT32)
	{
		smp->origDataPtrF = (float *)calloc((size_t)allocSize, 1);
		if (smp->origDataPtrF == NULL)
			return false;
		smp->dataPtrF = (float *)((uint8_t *)smp->origDataPtrF + leftPadding);

		if (outputStereo)
		{
			smp->origDataPtrFR = (float *)calloc((size_t)allocSize, 1);
			if (smp->origDataPtrFR == NULL)
			{
				freeSampleData(smp);
				return false;
			}
			smp->dataPtrFR = (float *)((uint8_t *)smp->origDataPtrFR + leftPadding);
		}
	}
	else
	{
		smp->origDataPtr = (int8_t *)calloc((size_t)allocSize, 1);
		if (smp->origDataPtr == NULL)
			return false;
		smp->dataPtr = smp->origDataPtr + leftPadding;

		if (outputStereo)
		{
			smp->origDataPtrR = (int8_t *)calloc((size_t)allocSize, 1);
			if (smp->origDataPtrR == NULL)
			{
				freeSampleData(smp);
				return false;
			}
			smp->dataPtrR = smp->origDataPtrR + leftPadding;
		}
	}

	smp->format = dstFormat;
	smp->sampleRate = (xm2Enabled && pcm->sampleRate > 0.0) ? (uint32_t)round(pcm->sampleRate) : 0;

	const uint8_t *src = pcm->data;

	if (dstFormat == FT2_SMP_FORMAT_8BIT || dstFormat == FT2_SMP_FORMAT_16BIT)
	{
		const bool output16Bit = (dstFormat == FT2_SMP_FORMAT_16BIT);

		if (!pcm->isFloat && (pcm->bitsPerSample == 8 || pcm->bitsPerSample == 16))
		{
			if (pcm->bitsPerSample == 8)
			{
				int8_t *dstL = smp->dataPtr;
				int8_t *dstR = outputStereo ? smp->dataPtrR : NULL;

				for (uint32_t i = 0; i < sampleLength; i++)
				{
					const uint8_t *frame = src + (i * frameBytes);
					int8_t left = pcm->unsigned8 ? (int8_t)(frame[0] ^ 0x80) : (int8_t)frame[0];

					if (pcm->channels == 1)
					{
						dstL[i] = left;
						continue;
					}

					int8_t right = pcm->unsigned8 ? (int8_t)(frame[1] ^ 0x80) : (int8_t)frame[1];
					if (outputStereo)
					{
						dstL[i] = left;
						dstR[i] = right;
					}
					else if (mode == STEREO_LOAD_LEFT_ONLY)
					{
						dstL[i] = left;
					}
					else if (mode == STEREO_LOAD_RIGHT_ONLY)
					{
						dstL[i] = right;
					}
					else
					{
						int16_t mix = (int16_t)left + (int16_t)right;
						dstL[i] = (int8_t)(mix / 2);
					}
				}
			}
			else
			{
				int16_t *dstL = (int16_t *)smp->dataPtr;
				int16_t *dstR = outputStereo ? (int16_t *)smp->dataPtrR : NULL;

				for (uint32_t i = 0; i < sampleLength; i++)
				{
					const uint8_t *frame = src + (i * frameBytes);
					int16_t left = read_s16(frame, pcm->bigEndian);

					if (pcm->channels == 1)
					{
						dstL[i] = left;
						continue;
					}

					int16_t right = read_s16(frame + bytesPerSample, pcm->bigEndian);
					if (outputStereo)
					{
						dstL[i] = left;
						dstR[i] = right;
					}
					else if (mode == STEREO_LOAD_LEFT_ONLY)
					{
						dstL[i] = left;
					}
					else if (mode == STEREO_LOAD_RIGHT_ONLY)
					{
						dstL[i] = right;
					}
					else
					{
						int32_t mix = (int32_t)left + (int32_t)right;
						dstL[i] = (int16_t)(mix / 2);
					}
				}
			}
		}
		else if (!pcm->isFloat)
		{
			int32_t *tmpL = (int32_t *)malloc(sampleLength * sizeof (int32_t));
			int32_t *tmpR = outputStereo ? (int32_t *)malloc(sampleLength * sizeof (int32_t)) : NULL;
			if (tmpL == NULL || (outputStereo && tmpR == NULL))
			{
				free(tmpL);
				free(tmpR);
				freeSampleData(smp);
				return false;
			}

			for (uint32_t i = 0; i < sampleLength; i++)
			{
				const uint8_t *frame = src + (i * frameBytes);
				int32_t left = (pcm->bitsPerSample == 24) ? read_s24(frame, pcm->bigEndian) : read_s32(frame, pcm->bigEndian);

				if (pcm->channels == 1)
				{
					tmpL[i] = left;
					continue;
				}

				int32_t right = (pcm->bitsPerSample == 24) ? read_s24(frame + bytesPerSample, pcm->bigEndian) :
				                                             read_s32(frame + bytesPerSample, pcm->bigEndian);
				if (outputStereo)
				{
					tmpL[i] = left;
					tmpR[i] = right;
				}
				else if (mode == STEREO_LOAD_LEFT_ONLY)
				{
					tmpL[i] = left;
				}
				else if (mode == STEREO_LOAD_RIGHT_ONLY)
				{
					tmpL[i] = right;
				}
				else
				{
					int64_t mix = (int64_t)left + (int64_t)right;
					tmpL[i] = (int32_t)(mix / 2);
				}
			}

			if (outputStereo)
				normalizeSigned32BitStereo(tmpL, tmpR, sampleLength);
			else
				normalizeSigned32Bit(tmpL, sampleLength);

			int16_t *dstL = (int16_t *)smp->dataPtr;
			int16_t *dstR = outputStereo ? (int16_t *)smp->dataPtrR : NULL;
			for (uint32_t i = 0; i < sampleLength; i++)
			{
				dstL[i] = (int16_t)(tmpL[i] >> 16);
				if (outputStereo)
					dstR[i] = (int16_t)(tmpR[i] >> 16);
			}

			free(tmpL);
			free(tmpR);
		}
		else if (pcm->bitsPerSample == 32)
		{
			float *tmpL = (float *)malloc(sampleLength * sizeof (float));
			float *tmpR = outputStereo ? (float *)malloc(sampleLength * sizeof (float)) : NULL;
			if (tmpL == NULL || (outputStereo && tmpR == NULL))
			{
				free(tmpL);
				free(tmpR);
				freeSampleData(smp);
				return false;
			}

			for (uint32_t i = 0; i < sampleLength; i++)
			{
				const uint8_t *frame = src + (i * frameBytes);
				float left = read_f32(frame, pcm->bigEndian);

				if (pcm->channels == 1)
				{
					tmpL[i] = left;
					continue;
				}

				float right = read_f32(frame + bytesPerSample, pcm->bigEndian);
				if (outputStereo)
				{
					tmpL[i] = left;
					tmpR[i] = right;
				}
				else if (mode == STEREO_LOAD_LEFT_ONLY)
				{
					tmpL[i] = left;
				}
				else if (mode == STEREO_LOAD_RIGHT_ONLY)
				{
					tmpL[i] = right;
				}
				else
				{
					tmpL[i] = (left + right) * 0.5f;
				}
			}

			if (outputStereo)
				normalize32BitFloatStereo(tmpL, tmpR, sampleLength);
			else
				normalize32BitFloatToSigned16Bit(tmpL, sampleLength);

			int16_t *dstL = (int16_t *)smp->dataPtr;
			int16_t *dstR = outputStereo ? (int16_t *)smp->dataPtrR : NULL;
			for (uint32_t i = 0; i < sampleLength; i++)
			{
				dstL[i] = (int16_t)tmpL[i];
				if (outputStereo)
					dstR[i] = (int16_t)tmpR[i];
			}

			free(tmpL);
			free(tmpR);
		}
		else
		{
			double *tmpL = (double *)malloc(sampleLength * sizeof (double));
			double *tmpR = outputStereo ? (double *)malloc(sampleLength * sizeof (double)) : NULL;
			if (tmpL == NULL || (outputStereo && tmpR == NULL))
			{
				free(tmpL);
				free(tmpR);
				freeSampleData(smp);
				return false;
			}

			for (uint32_t i = 0; i < sampleLength; i++)
			{
				const uint8_t *frame = src + (i * frameBytes);
				double left = read_f64(frame, pcm->bigEndian);

				if (pcm->channels == 1)
				{
					tmpL[i] = left;
					continue;
				}

				double right = read_f64(frame + bytesPerSample, pcm->bigEndian);
				if (outputStereo)
				{
					tmpL[i] = left;
					tmpR[i] = right;
				}
				else if (mode == STEREO_LOAD_LEFT_ONLY)
				{
					tmpL[i] = left;
				}
				else if (mode == STEREO_LOAD_RIGHT_ONLY)
				{
					tmpL[i] = right;
				}
				else
				{
					tmpL[i] = (left + right) * 0.5;
				}
			}

			if (outputStereo)
				normalize64BitFloatStereo(tmpL, tmpR, sampleLength);
			else
				normalize64BitFloatToSigned16Bit(tmpL, sampleLength);

			int16_t *dstL = (int16_t *)smp->dataPtr;
			int16_t *dstR = outputStereo ? (int16_t *)smp->dataPtrR : NULL;
			for (uint32_t i = 0; i < sampleLength; i++)
			{
				dstL[i] = (int16_t)tmpL[i];
				if (outputStereo)
					dstR[i] = (int16_t)tmpR[i];
			}

			free(tmpL);
			free(tmpR);
		}
	}
	else if (dstFormat == FT2_SMP_FORMAT_24BIT)
	{
		int32_t *dstL = smp->dataPtr24;
		int32_t *dstR = outputStereo ? smp->dataPtr24R : NULL;

		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = src + (i * frameBytes);
			int32_t left = 0;
			int32_t right = 0;

			if (pcm->isFloat)
			{
				float lf = (pcm->bitsPerSample == 64) ? (float)read_f64(frame, pcm->bigEndian) : read_f32(frame, pcm->bigEndian);
				if (lf > 1.0f) lf = 1.0f;
				if (lf < -1.0f) lf = -1.0f;
				left = (int32_t)roundf(lf * 8388607.0f);
			}
			else
			{
				if (pcm->bitsPerSample == 8)
				{
					int8_t v = pcm->unsigned8 ? (int8_t)(frame[0] ^ 0x80) : (int8_t)frame[0];
					left = (int32_t)v << 16;
				}
				else if (pcm->bitsPerSample == 16)
				{
					left = (int32_t)read_s16(frame, pcm->bigEndian) << 8;
				}
				else if (pcm->bitsPerSample == 24)
				{
					left = read_s24(frame, pcm->bigEndian);
				}
				else
				{
					left = read_s32(frame, pcm->bigEndian) >> 8;
				}
			}

			if (pcm->channels == 1)
			{
				dstL[i] = left;
				continue;
			}

			if (pcm->isFloat)
			{
				float rf = (pcm->bitsPerSample == 64) ? (float)read_f64(frame + bytesPerSample, pcm->bigEndian) :
				                                        read_f32(frame + bytesPerSample, pcm->bigEndian);
				if (rf > 1.0f) rf = 1.0f;
				if (rf < -1.0f) rf = -1.0f;
				right = (int32_t)roundf(rf * 8388607.0f);
			}
			else
			{
				if (pcm->bitsPerSample == 8)
				{
					int8_t v = pcm->unsigned8 ? (int8_t)(frame[1] ^ 0x80) : (int8_t)frame[1];
					right = (int32_t)v << 16;
				}
				else if (pcm->bitsPerSample == 16)
				{
					right = (int32_t)read_s16(frame + bytesPerSample, pcm->bigEndian) << 8;
				}
				else if (pcm->bitsPerSample == 24)
				{
					right = read_s24(frame + bytesPerSample, pcm->bigEndian);
				}
				else
				{
					right = read_s32(frame + bytesPerSample, pcm->bigEndian) >> 8;
				}
			}

			if (outputStereo)
			{
				dstL[i] = left;
				dstR[i] = right;
			}
			else if (mode == STEREO_LOAD_LEFT_ONLY)
			{
				dstL[i] = left;
			}
			else if (mode == STEREO_LOAD_RIGHT_ONLY)
			{
				dstL[i] = right;
			}
			else
			{
				int64_t mix = (int64_t)left + (int64_t)right;
				dstL[i] = (int32_t)(mix / 2);
			}
		}
	}
	else
	{
		float *dstL = smp->dataPtrF;
		float *dstR = outputStereo ? smp->dataPtrFR : NULL;

		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = src + (i * frameBytes);
			float left = 0.0f;
			float right = 0.0f;

			if (pcm->isFloat)
			{
				left = (pcm->bitsPerSample == 64) ? (float)read_f64(frame, pcm->bigEndian) : read_f32(frame, pcm->bigEndian);
			}
			else
			{
				if (pcm->bitsPerSample == 8)
				{
					int8_t v = pcm->unsigned8 ? (int8_t)(frame[0] ^ 0x80) : (int8_t)frame[0];
					left = (float)v * (1.0f / 128.0f);
				}
				else if (pcm->bitsPerSample == 16)
				{
					left = (float)read_s16(frame, pcm->bigEndian) * (1.0f / 32768.0f);
				}
				else if (pcm->bitsPerSample == 24)
				{
					left = (float)read_s24(frame, pcm->bigEndian) * (1.0f / 8388608.0f);
				}
				else
				{
					left = (float)read_s32(frame, pcm->bigEndian) * (1.0f / 2147483648.0f);
				}
			}

			if (pcm->channels == 1)
			{
				dstL[i] = left;
				continue;
			}

			if (pcm->isFloat)
			{
				right = (pcm->bitsPerSample == 64) ? (float)read_f64(frame + bytesPerSample, pcm->bigEndian) :
				                                    read_f32(frame + bytesPerSample, pcm->bigEndian);
			}
			else
			{
				if (pcm->bitsPerSample == 8)
				{
					int8_t v = pcm->unsigned8 ? (int8_t)(frame[1] ^ 0x80) : (int8_t)frame[1];
					right = (float)v * (1.0f / 128.0f);
				}
				else if (pcm->bitsPerSample == 16)
				{
					right = (float)read_s16(frame + bytesPerSample, pcm->bigEndian) * (1.0f / 32768.0f);
				}
				else if (pcm->bitsPerSample == 24)
				{
					right = (float)read_s24(frame + bytesPerSample, pcm->bigEndian) * (1.0f / 8388608.0f);
				}
				else
				{
					right = (float)read_s32(frame + bytesPerSample, pcm->bigEndian) * (1.0f / 2147483648.0f);
				}
			}

			if (left > 1.0f) left = 1.0f;
			if (left < -1.0f) left = -1.0f;
			if (right > 1.0f) right = 1.0f;
			if (right < -1.0f) right = -1.0f;

			if (outputStereo)
			{
				dstL[i] = left;
				dstR[i] = right;
			}
			else if (mode == STEREO_LOAD_LEFT_ONLY)
			{
				dstL[i] = left;
			}
			else if (mode == STEREO_LOAD_RIGHT_ONLY)
			{
				dstL[i] = right;
			}
			else
			{
				dstL[i] = (left + right) * 0.5f;
			}
		}
	}

	smp->length = (int32_t)sampleLength;
	smp->flags = (dstFormat == FT2_SMP_FORMAT_16BIT) ? FT2_SAMPLE_16BIT : 0;
	if (outputStereo)
		smp->flags |= FT2_SAMPLE_STEREO;

	smp->loopStart = 0;
	smp->loopLength = 0;
	smp->flags &= ~(LOOP_FWD | LOOP_BIDI);
	if (pcm->loopEnabled && pcm->loopLength > 0)
	{
		smp->loopStart = pcm->loopStart;
		smp->loopLength = pcm->loopLength;
		smp->flags |= LOOP_FWD;
	}

	smp->volume = pcm->hasVolume ? (uint8_t)MIN(pcm->volume, 64) : 64;
	smp->panning = pcm->hasPanning ? (uint8_t)MIN(pcm->panning, 255) : 128;

	if (smp->sampleRate > 0)
	{
		smp->relativeNote = 0;
		smp->finetune = 0;
	}
	else if (pcm->sampleRate > 0.0)
	{
		ft2_set_sample_c4hz(smp, pcm->sampleRate);
	}
	else
	{
		smp->relativeNote = 0;
		smp->finetune = 0;
	}

	if (sampleNum >= instr->numSamples)
		instr->numSamples = sampleNum + 1;

	ft2_xm2_update_usage(inst);

	ft2_sanitize_sample(smp);
	ft2_fix_sample(smp);
	return true;
}

static bool loadSampleFromDataWithMode(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                                        const uint8_t *data, uint32_t dataSize, ft2_file_format format,
                                        stereo_load_mode_t mode)
{
	ft2_pcm_info_t pcm;
	memset(&pcm, 0, sizeof (pcm));

	switch (format)
	{
		case FT2_FORMAT_WAV:
			if (!parseWavToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_AIFF:
			if (!parseAiffToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_IFF:
			if (!parseIffToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_FLAC:
#ifdef HAS_LIBFLAC
			if (!decodeFlacToPcm(data, dataSize, &pcm))
				return false;
#else
			return false;
#endif
			break;
		case FT2_FORMAT_RAW:
			pcm.data = data;
			pcm.dataSize = dataSize;
			pcm.sampleRate = 0.0;
			pcm.channels = 1;
			pcm.bitsPerSample = 8;
			pcm.isFloat = false;
			pcm.bigEndian = false;
			pcm.unsigned8 = false;
			pcm.loopStart = 0;
			pcm.loopLength = 0;
			pcm.loopEnabled = false;
			break;
		default:
			return false;
	}

	bool result = loadSampleFromPcm(inst, instrNum, sampleNum, &pcm, mode);
#ifdef HAS_LIBFLAC
	if (format == FT2_FORMAT_FLAC)
	{
		free((void *)pcm.data);
	}
#endif
	return result;
}

/* ---------- Stereo sample loading helpers ---------- */

/* Forward declarations for dialog callbacks */
static void onStereoDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                  const char *inputText, void *userData);
static void onRadioDialogResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                 const char *inputText, void *userData);

static void buildStereoDialogHeadline(char *dst, size_t dstSize, const char *filename, const char *fallback)
{
	if (dst == NULL || dstSize == 0)
		return;

	const char *src = (filename != NULL && filename[0] != '\0') ? filename : fallback;
	if (src == NULL || src[0] == '\0')
	{
		dst[0] = '\0';
		return;
	}

	const size_t len = strlen(src);
	if (len < dstSize)
	{
		memcpy(dst, src, len + 1);
		return;
	}

	if (dstSize <= 4)
	{
		const size_t count = dstSize - 1;
		memset(dst, '.', count);
		dst[count] = '\0';
		return;
	}

	const size_t keep = dstSize - 4;
	memcpy(dst, src, keep);
	memcpy(dst + keep, "...", 3);
	dst[keep + 3] = '\0';
}

/* Clear pending stereo load state */
static void clearPendingStereoLoad(ft2_instance_t *inst)
{
	if (inst->diskop.pendingStereoData != NULL)
	{
		free(inst->diskop.pendingStereoData);
		inst->diskop.pendingStereoData = NULL;
	}
	inst->diskop.pendingStereoLoad = false;
	inst->diskop.pendingStereoFormat = FT2_FORMAT_UNKNOWN;
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
	return loadSampleFromDataWithMode(inst, instrNum, sampleNum, data, dataSize, FT2_FORMAT_WAV, mode);
}

/* Complete a pending stereo sample load with specified mode */
bool ft2_complete_stereo_load(ft2_instance_t *inst, stereo_load_mode_t mode)
{
	if (inst == NULL || !inst->diskop.pendingStereoLoad)
		return false;

	ft2_file_format format = (ft2_file_format)inst->diskop.pendingStereoFormat;
	if (format == FT2_FORMAT_UNKNOWN)
		format = FT2_FORMAT_WAV; /* Legacy default */

	bool result = loadSampleFromDataWithMode(inst,
	                                         inst->diskop.pendingStereoInstrNum,
	                                         inst->diskop.pendingStereoSampleNum,
	                                         inst->diskop.pendingStereoData,
	                                         inst->diskop.pendingStereoDataSize,
	                                         format,
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
	ft2_sample_ed_mark_waveform_dirty(inst);
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
                      const uint8_t *data, uint32_t dataSize, const char *filename)
{
	if (inst == NULL || data == NULL || dataSize == 0)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (sampleNum < 0 || sampleNum >= maxSamples)
		return false;

	ft2_file_format format = ft2_detect_format_by_header(data, dataSize);

#ifdef HAS_LIBFLAC
	if (format == FT2_FORMAT_FLAC)
	{
		ft2_flac_ctx_t ctx;
		if (!flacProbeInfo(data, dataSize, &ctx))
			return false;
		if (ctx.numChannels > 2 || (ctx.bitDepth != 8 && ctx.bitDepth != 16 && ctx.bitDepth != 24))
			return false;

		if (ctx.numChannels == 2)
		{
			ft2_ui_t *ui = (ft2_ui_t *)inst->ui;
			if (ui == NULL)
			{
				/* No UI available, default to convert mono */
				return loadSampleFromDataWithMode(inst, instrNum, sampleNum, data, dataSize,
				                                  format, STEREO_LOAD_CONVERT_MONO);
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
			inst->diskop.pendingStereoFormat = (uint8_t)format;
			inst->diskop.pendingStereoLoad = true;
			if (filename != NULL)
				ft2_set_pending_stereo_filename(inst, filename);

			char headline[64];
			if (inst->config.enableXm2Features)
			{
				static const char *options[] = {
					"Load as stereo",
					"Load left channel only",
					"Load right channel only",
					"Convert to mono"
				};
				buildStereoDialogHeadline(headline, sizeof(headline), filename, "Loading stereo sample");
				ft2_dialog_show_radio_cb(&ui->dialog, headline,
				                          options, 4, 0, inst,
				                          onRadioDialogResult, NULL);
			}
			else
			{
				buildStereoDialogHeadline(headline, sizeof(headline), filename, "System request");
				ft2_dialog_show_stereo_cb(&ui->dialog, headline,
				                           "This is a stereo sample...",
				                           inst, onStereoDialogResult, NULL);
			}

			return true; /* Load is pending */
		}

		return loadSampleFromDataWithMode(inst, instrNum, sampleNum, data, dataSize,
		                                  format, STEREO_LOAD_LEFT_ONLY);
	}
#endif

	if (format == FT2_FORMAT_UNKNOWN)
		format = FT2_FORMAT_RAW;

	ft2_pcm_info_t pcm;
	memset(&pcm, 0, sizeof (pcm));

	switch (format)
	{
		case FT2_FORMAT_WAV:
			if (!parseWavToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_AIFF:
			if (!parseAiffToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_IFF:
			if (!parseIffToPcm(data, dataSize, &pcm))
				return false;
			break;
		case FT2_FORMAT_RAW:
			pcm.data = data;
			pcm.dataSize = dataSize;
			pcm.sampleRate = 0.0;
			pcm.channels = 1;
			pcm.bitsPerSample = 8;
			pcm.isFloat = false;
			pcm.bigEndian = false;
			pcm.unsigned8 = false;
			pcm.loopStart = 0;
			pcm.loopLength = 0;
			pcm.loopEnabled = false;
			break;
		default:
			return false;
	}

	if (pcm.channels == 2)
	{
		ft2_ui_t *ui = (ft2_ui_t *)inst->ui;
		if (ui == NULL)
		{
			/* No UI available, default to convert mono */
			return loadSampleFromDataWithMode(inst, instrNum, sampleNum, data, dataSize,
			                                  format, STEREO_LOAD_CONVERT_MONO);
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
		inst->diskop.pendingStereoFormat = (uint8_t)format;
		inst->diskop.pendingStereoLoad = true;
		if (filename != NULL)
			ft2_set_pending_stereo_filename(inst, filename);

		/* Show appropriate dialog based on XM2 config */
		char headline[64];
		if (inst->config.enableXm2Features)
		{
			/* XM2 enabled: show radio dialog with all 4 options */
			static const char *options[] = {
				"Load as stereo",
				"Load left channel only",
				"Load right channel only",
				"Convert to mono"
			};
			buildStereoDialogHeadline(headline, sizeof(headline), filename, "Loading stereo sample");
			ft2_dialog_show_radio_cb(&ui->dialog, headline,
			                          options, 4, 0, inst,
			                          onRadioDialogResult, NULL);
		}
		else
		{
			/* XM2 disabled: show 3-button dialog */
			buildStereoDialogHeadline(headline, sizeof(headline), filename, "System request");
			ft2_dialog_show_stereo_cb(&ui->dialog, headline,
			                           "This is a stereo sample...",
			                           inst, onStereoDialogResult, NULL);
		}

		return true; /* Load is pending */
	}

	/* Mono sample - load directly */
	bool result = loadSampleFromPcm(inst, instrNum, sampleNum, &pcm, STEREO_LOAD_LEFT_ONLY);
#ifdef HAS_LIBFLAC
	if (format == FT2_FORMAT_FLAC)
		free((void *)pcm.data);
#endif
	return result;
}

void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                        int16_t sampleNum, const char *filename)
{
	if (inst == NULL || filename == NULL)
		return;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (sampleNum < 0 || sampleNum >= maxSamples)
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

static uint32_t getSampleMiddleCRate(const ft2_sample_t *s)
{
	double period = (10.0 * 12.0 * 16.0 * 4.0) - (s->relativeNote * 16.0 * 4.0) - (s->finetune / 2.0);
	double rate = 8363.0 * pow(2.0, (4608.0 - period) / 768.0);
	if (rate < 0.0)
		rate = 0.0;
	return (uint32_t)(rate + 0.5);
}

static void write_be16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v & 0xFF);
}

static void write_be32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v & 0xFF);
}

static uint32_t paddedChunkLen(uint32_t len)
{
	return len + (len & 1);
}

static bool createRawSample(const ft2_sample_t *smp, uint8_t **outData, uint32_t *outSize)
{
	if (smp == NULL || outData == NULL || outSize == NULL || smp->dataPtr == NULL || smp->length <= 0)
		return false;

	const bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);
	const bool stereo = (smp->flags & FT2_SAMPLE_STEREO) && smp->dataPtrR != NULL;
	const uint32_t sampleLen = (uint32_t)smp->length;
	const uint32_t dataSize = sampleLen * (sample16Bit ? 2 : 1);

	uint8_t *buf = (uint8_t *)malloc(dataSize);
	if (buf == NULL)
		return false;

	if (!stereo)
	{
		memcpy(buf, smp->dataPtr, dataSize);
	}
	else if (sample16Bit)
	{
		int16_t *dst = (int16_t *)buf;
		const int16_t *srcL = (const int16_t *)smp->dataPtr;
		const int16_t *srcR = (const int16_t *)smp->dataPtrR;
		for (uint32_t i = 0; i < sampleLen; i++)
		{
			int32_t mix = (int32_t)srcL[i] + (int32_t)srcR[i];
			dst[i] = (int16_t)(mix / 2);
		}
	}
	else
	{
		int8_t *dst = (int8_t *)buf;
		const int8_t *srcL = smp->dataPtr;
		const int8_t *srcR = smp->dataPtrR;
		for (uint32_t i = 0; i < sampleLen; i++)
		{
			int16_t mix = (int16_t)srcL[i] + (int16_t)srcR[i];
			dst[i] = (int8_t)(mix / 2);
		}
	}

	*outData = buf;
	*outSize = dataSize;
	return true;
}

static bool createIffSample(const ft2_sample_t *smp, uint8_t **outData, uint32_t *outSize)
{
	if (smp == NULL || outData == NULL || outSize == NULL || smp->dataPtr == NULL || smp->length <= 0)
		return false;

	const bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);
	const bool stereo = (smp->flags & FT2_SAMPLE_STEREO) && smp->dataPtrR != NULL;
	const uint32_t sampleLen = (uint32_t)smp->length;
	const uint32_t sampleDataBytes = sampleLen << (sample16Bit ? 1 : 0);

	const int8_t *samplePtr = smp->dataPtr;
	int8_t *monoBuffer = NULL;

	if (stereo)
	{
		monoBuffer = (int8_t *)malloc(sampleDataBytes);
		if (monoBuffer == NULL)
			return false;

		if (sample16Bit)
		{
			int16_t *dst = (int16_t *)monoBuffer;
			const int16_t *srcL = (const int16_t *)smp->dataPtr;
			const int16_t *srcR = (const int16_t *)smp->dataPtrR;
			for (uint32_t i = 0; i < sampleLen; i++)
			{
				int32_t mix = (int32_t)srcL[i] + (int32_t)srcR[i];
				dst[i] = (int16_t)(mix / 2);
			}
		}
		else
		{
			int8_t *dst = monoBuffer;
			const int8_t *srcL = smp->dataPtr;
			const int8_t *srcR = smp->dataPtrR;
			for (uint32_t i = 0; i < sampleLen; i++)
			{
				int16_t mix = (int16_t)srcL[i] + (int16_t)srcR[i];
				dst[i] = (int8_t)(mix / 2);
			}
		}

		samplePtr = monoBuffer;
	}

	uint32_t nameLen = 0;
	while (nameLen < 22 && smp->name[nameLen] != '\0')
		nameLen++;

	static const char annoStr[] = "FT2Clone Plugin";
	const uint32_t annoLen = (uint32_t)strlen(annoStr);

	const uint32_t nameChunkSize = (nameLen > 0) ? (8 + paddedChunkLen(nameLen)) : 0;
	const uint32_t annoChunkSize = 8 + paddedChunkLen(annoLen);
	const uint32_t bodyChunkSize = 8 + paddedChunkLen(sampleDataBytes);
	const uint32_t formSize = 4 + (8 + 20) + nameChunkSize + annoChunkSize + bodyChunkSize;
	const uint32_t totalSize = 8 + formSize;

	uint8_t *buf = (uint8_t *)malloc(totalSize);
	if (buf == NULL)
	{
		free(monoBuffer);
		return false;
	}

	uint8_t *p = buf;
	memcpy(p, "FORM", 4); p += 4;
	write_be32(p, formSize); p += 4;
	memcpy(p, sample16Bit ? "16SV" : "8SVX", 4); p += 4;

	memcpy(p, "VHDR", 4); p += 4;
	write_be32(p, 20); p += 4;

	uint32_t loopStart = 0;
	uint32_t loopLength = 0;
	if (GET_LOOPTYPE(smp->flags) != LOOP_OFF && smp->loopLength > 0)
	{
		loopStart = (uint32_t)smp->loopStart << (sample16Bit ? 1 : 0);
		loopLength = (uint32_t)smp->loopLength << (sample16Bit ? 1 : 0);
	}

	write_be32(p, loopStart); p += 4;
	write_be32(p, loopLength); p += 4;
	write_be32(p, 0); p += 4; /* samplesPerHiCycle */

	uint32_t rate = getSampleMiddleCRate(smp);
	if (rate == 0 || rate > 65535)
		rate = 16726;
	write_be16(p, (uint16_t)rate); p += 2;
	*p++ = 1; /* ctOctave */
	*p++ = 0; /* sCompression */
	write_be32(p, (uint32_t)smp->volume * 1024); p += 4;

	if (nameLen > 0)
	{
		memcpy(p, "NAME", 4); p += 4;
		write_be32(p, nameLen); p += 4;
		memcpy(p, smp->name, nameLen); p += nameLen;
		if (nameLen & 1) *p++ = 0;
	}

	memcpy(p, "ANNO", 4); p += 4;
	write_be32(p, annoLen); p += 4;
	memcpy(p, annoStr, annoLen); p += annoLen;
	if (annoLen & 1) *p++ = 0;

	memcpy(p, "BODY", 4); p += 4;
	write_be32(p, sampleDataBytes); p += 4;
	memcpy(p, samplePtr, sampleDataBytes); p += sampleDataBytes;
	if (sampleDataBytes & 1) *p++ = 0;

	*outData = buf;
	*outSize = (uint32_t)(p - buf);

	free(monoBuffer);
	return true;
}

bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (sampleNum < 0 || sampleNum >= maxSamples)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return false;

	ft2_sample_t *smp = &instr->smp[sampleNum];
	ft2_sample_format_t format = resolve_sample_format(smp);
	if (!sample_has_data(smp, format) || smp->length == 0)
		return false;

	const uint8_t saveFormat = inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE];
	const bool isStereo = (smp->flags & FT2_SAMPLE_STEREO) && sample_has_stereo_data(smp, format);

	ft2_sample_t tempSmp = *smp;
	const ft2_sample_t *src = smp;
	int16_t *convL = NULL;
	int16_t *convR = NULL;
	int16_t bitsPerSample = (format == FT2_SMP_FORMAT_16BIT) ? 16 : 8;

	if (format == FT2_SMP_FORMAT_24BIT || format == FT2_SMP_FORMAT_FLOAT32)
	{
		bitsPerSample = 16;
		convL = (int16_t *)malloc((size_t)smp->length * sizeof(int16_t));
		if (convL == NULL)
			return false;

		if (isStereo)
		{
			convR = (int16_t *)malloc((size_t)smp->length * sizeof(int16_t));
			if (convR == NULL)
			{
				free(convL);
				return false;
			}
		}

		for (int32_t i = 0; i < smp->length; i++)
		{
			float v = get_sample_float(smp, format, false, i);
			int32_t s16 = (int32_t)roundf(v * 32767.0f);
			if (s16 > 32767) s16 = 32767;
			if (s16 < -32768) s16 = -32768;
			convL[i] = (int16_t)s16;

			if (convR != NULL)
			{
				float vR = get_sample_float(smp, format, true, i);
				int32_t s16R = (int32_t)roundf(vR * 32767.0f);
				if (s16R > 32767) s16R = 32767;
				if (s16R < -32768) s16R = -32768;
				convR[i] = (int16_t)s16R;
			}
		}

		tempSmp.dataPtr = (int8_t *)convL;
		tempSmp.dataPtrR = (int8_t *)convR;
		tempSmp.flags |= FT2_SAMPLE_16BIT;
		if (!isStereo)
			tempSmp.flags &= ~FT2_SAMPLE_STEREO;
		src = &tempSmp;
	}

	if (saveFormat == FT2_SMP_SAVE_RAW)
	{
		bool ok = createRawSample(src, outData, outSize);
		free(convL);
		free(convR);
		return ok;
	}
	if (saveFormat == FT2_SMP_SAVE_IFF)
	{
		bool ok = createIffSample(src, outData, outSize);
		free(convL);
		free(convR);
		return ok;
	}

	int16_t numChannels = isStereo ? 2 : 1;

	if (!isStereo)
	{
		bool ok = ft2_create_wav(src->dataPtr, src->length, 1, bitsPerSample, 44100,
		                          outData, outSize);
		free(convL);
		free(convR);
		return ok;
	}

	/* Stereo: interleave L and R channels */
	int32_t bytesPerSample = (bitsPerSample / 8) * numChannels;
	int32_t dataSize = src->length * bytesPerSample;
	int32_t fileSize = 44 + dataSize;

	*outData = (uint8_t *)malloc(fileSize);
	if (*outData == NULL)
	{
		free(convL);
		free(convR);
		return false;
	}

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
		const int16_t *srcL = (const int16_t *)src->dataPtr;
		const int16_t *srcR = (const int16_t *)src->dataPtrR;
		for (int32_t i = 0; i < src->length; i++)
		{
			dst[i * 2] = srcL[i];
			dst[i * 2 + 1] = srcR[i];
		}
	}
	else
	{
		/* Convert signed to unsigned for 8-bit WAV */
		uint8_t *dst = p;
		const int8_t *srcL = src->dataPtr;
		const int8_t *srcR = src->dataPtrR;
		for (int32_t i = 0; i < src->length; i++)
		{
			dst[i * 2] = (uint8_t)(srcL[i] + 128);
			dst[i * 2 + 1] = (uint8_t)(srcR[i] + 128);
		}
	}

	*outSize = fileSize;
	free(convL);
	free(convR);
	return true;
}
