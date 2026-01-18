/**
 * @file ft2_plugin_loader.c
 * @brief XM loader with auto-detect wrapper for XM/MOD/S3M.
 *
 * XM format support:
 * - v1.02/v1.03/v1.04 (different header layouts)
 * - Stereo samples downmixed to mono
 * - ModPlug ADPCM samples
 * - >128 instruments (extras discarded)
 * - >16 samples per instrument (extras discarded)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_plugin_loader.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_mem_reader.h"
#include "ft2_plugin_load_mod.h"
#include "ft2_plugin_load_s3m.h"
#include "ft2_plugin_timemap.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_xm2_convert.h"

#define SAMPLE_16BIT  16
#define SAMPLE_STEREO 32
#define SAMPLE_ADPCM  64
#define INSTR_HEADER_SIZE 263

/* ---------- XM file structures ---------- */

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct xm_header_t {
	char id[17];
	char name[20];
	uint8_t x1A;
	char progName[20];
	uint16_t version;
	uint32_t headerSize;
	uint16_t numOrders, songLoopStart, numChannels, numPatterns, numInstr;
	uint16_t flags, speed, BPM;
	uint8_t orders[256];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_header_t;

typedef struct xm_pattern_header_t {
	uint32_t headerSize;
	uint8_t type;
	uint16_t numRows, dataSize;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_pattern_header_t;

typedef struct xm_sample_header_t {
	uint32_t length, loopStart, loopLength;
	uint8_t volume;
	int8_t finetune;
	uint8_t flags, panning;
	int8_t relativeNote;
	uint8_t nameLength;
	char name[22];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_sample_header_t;

typedef struct xm_instr_header_t {
	uint32_t instrSize;
	char name[22];
	uint8_t type;
	int16_t numSamples;
	int32_t sampleSize;
	uint8_t note2SampleLUT[96];
	int16_t volEnvPoints[12][2];
	int16_t panEnvPoints[12][2];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t vibType, vibSweep, vibDepth, vibRate;
	uint16_t fadeout;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	int8_t mute;
	uint8_t junk[15];
	xm_sample_header_t smp[16];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_instr_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct xm_loader_state_t {
	mem_reader_t reader;
	ft2_instance_t *inst;
	xm_header_t header;
	bool linearPeriodsFlag;
	bool isXm2;  /* True for XM2 format (stereo samples preserved) */
	bool allowXm2; /* XM2 features enabled in config */
	int32_t maxSamples; /* Runtime max sample slots (16 or 96) */
	int32_t extraSampleCount;
	uint32_t *extraSampleLengths;
} xm_loader_state_t;

/* ---------- Pattern unpacking ---------- */

/* Unpack XM pattern data (RLE-like compression).
 * Byte with bit 7 set = compressed, bits 0-4 indicate which fields follow. */
static void unpack_pattern(ft2_note_t *dst, const uint8_t *src, uint32_t srcLen, int32_t numChannels, int32_t numRows)
{
	if (dst == NULL) return;

	const int32_t srcEnd = numRows * (sizeof(ft2_note_t) * numChannels);
	int32_t srcIdx = 0;
	int32_t actualChannels = (numChannels > FT2_MAX_CHANNELS) ? FT2_MAX_CHANNELS : numChannels;

	for (int32_t i = 0; i < numRows; i++) {
		ft2_note_t *p = &dst[i * FT2_MAX_CHANNELS];
		for (int32_t j = 0; j < actualChannels; j++) {
			if (srcIdx >= srcEnd) return;
			const uint8_t note = *src++;
			if (note & 0x80) {
				p->note = (note & 0x01) ? *src++ : 0;
				p->instr = (note & 0x02) ? *src++ : 0;
				p->vol = (note & 0x04) ? *src++ : 0;
				p->efx = (note & 0x08) ? *src++ : 0;
				p->efxData = (note & 0x10) ? *src++ : 0;
			} else {
				p->note = note;
				p->instr = *src++;
				p->vol = *src++;
				p->efx = *src++;
				p->efxData = *src++;
			}
			srcIdx += sizeof(ft2_note_t);
			p++;
		}
		/* Skip channels beyond FT2_MAX_CHANNELS */
		for (int32_t j = actualChannels; j < numChannels; j++) {
			if (srcIdx >= srcEnd) return;
			const uint8_t note = *src++;
			if (note & 0x80) {
				if (note & 0x01) src++;
				if (note & 0x02) src++;
				if (note & 0x04) src++;
				if (note & 0x08) src++;
				if (note & 0x10) src++;
			} else {
				src += 4;
			}
			srcIdx += sizeof(ft2_note_t);
		}
	}
}

/* ---------- Sample decoding ---------- */

/* Delta decode sample data, downmixing stereo to mono unless keepStereo is true.
 * XM samples are stored as deltas from previous sample value.
 * For XM2 format, stereo samples are preserved with L and R data in separate buffers. */
static void delta_to_sample(int8_t *p, int8_t *pR, int32_t length, uint8_t smpFlags, bool keepStereo)
{
	bool sample16Bit = !!(smpFlags & SAMPLE_16BIT);
	bool stereo = !!(smpFlags & SAMPLE_STEREO);

	if (stereo) {
		int32_t halfLength = length >> 1;
		if (keepStereo && pR != NULL) {
			/* XM2: Keep stereo - decode L and R channels to separate buffers */
			if (sample16Bit) {
				int16_t *p16L = (int16_t *)p;
				int16_t *srcR = (int16_t *)p + halfLength;
				int16_t *p16R = (int16_t *)pR;
				int16_t olds16L = 0, olds16R = 0;
				for (int32_t i = 0; i < halfLength; i++) {
					olds16L = p16L[i] += olds16L;
					olds16R = srcR[i] += olds16R;
					p16R[i] = olds16R;
				}
			} else {
				int8_t *p8L = p;
				int8_t *srcR = p + halfLength;
				int8_t *p8R = pR;
				int8_t olds8L = 0, olds8R = 0;
				for (int32_t i = 0; i < halfLength; i++) {
					olds8L = p8L[i] += olds8L;
					olds8R = srcR[i] += olds8R;
					p8R[i] = olds8R;
				}
			}
		} else {
			/* Standard XM: Downmix stereo to mono */
			if (sample16Bit) {
				int16_t *p16L = (int16_t *)p, *p16R = (int16_t *)p + halfLength;
				int16_t olds16L = 0, olds16R = 0;
				for (int32_t i = 0; i < halfLength; i++) {
					olds16L = p16L[i] += olds16L;
					olds16R = p16R[i] += olds16R;
					p16L[i] = (int16_t)((olds16L + olds16R) >> 1);
				}
			} else {
				int8_t *p8L = p, *p8R = p + halfLength;
				int8_t olds8L = 0, olds8R = 0;
				for (int32_t i = 0; i < halfLength; i++) {
					olds8L = p8L[i] += olds8L;
					olds8R = p8R[i] += olds8R;
					p8L[i] = (int8_t)((olds8L + olds8R) >> 1);
				}
			}
		}
	} else {
		if (sample16Bit) {
			int16_t *p16 = (int16_t *)p;
			int16_t olds16 = 0;
			for (int32_t i = 0; i < length; i++) olds16 = p16[i] += olds16;
		} else {
			int8_t olds8 = 0;
			for (int32_t i = 0; i < length; i++) olds8 = p[i] += olds8;
		}
	}
}

static uint32_t xm2_file_bytes_per_sample(ft2_sample_format_t format)
{
	switch (format)
	{
		case FT2_SMP_FORMAT_16BIT: return 2;
		case FT2_SMP_FORMAT_24BIT: return 3;
		case FT2_SMP_FORMAT_FLOAT32: return 4;
		case FT2_SMP_FORMAT_8BIT:
		default: return 1;
	}
}

static void delta_decode_8bit(int8_t *p, int32_t length)
{
	int8_t old = 0;
	for (int32_t i = 0; i < length; i++)
		old = p[i] += old;
}

static void delta_decode_16bit(int16_t *p, int32_t length)
{
	int16_t old = 0;
	for (int32_t i = 0; i < length; i++)
		old = p[i] += old;
}

static void clear_extra_sample_lengths(xm_loader_state_t *state)
{
	if (state == NULL)
		return;

	if (state->extraSampleLengths != NULL)
	{
		free(state->extraSampleLengths);
		state->extraSampleLengths = NULL;
	}

	state->extraSampleCount = 0;
}

/* ModPlug ADPCM: 4-bit delta with 16-byte LUT per sample */
static bool load_adpcm_sample(mem_reader_t *r, ft2_sample_t *s)
{
	int8_t deltaLUT[16];
	if (!mem_read(r, deltaLUT, 16)) return false;

	const int32_t dataLength = (s->length + 1) / 2;
	int8_t *dataPtr = s->dataPtr;
	int8_t currSample = 0;

	for (int32_t i = 0; i < dataLength; i++) {
		uint8_t nibbles;
		if (!mem_read(r, &nibbles, 1)) return false;
		currSample += deltaLUT[nibbles & 0x0F];
		*dataPtr++ = currSample;
		currSample += deltaLUT[nibbles >> 4];
		*dataPtr++ = currSample;
	}
	return true;
}

/* ---------- Instrument loading ---------- */

/* Load instrument header. Separated from sample loading for v1.02/v1.03 compat. */
static bool load_instr_header(xm_loader_state_t *state, uint16_t instrNum)
{
	mem_reader_t *r = &state->reader;
	ft2_instance_t *inst = state->inst;
	ft2_replayer_state_t *rep = &inst->replayer;
	ft2_song_t *song = &rep->song;

	clear_extra_sample_lengths(state);

	xm_instr_header_t ih;
	memset(&ih, 0, sizeof(ih));

	/* Read instrSize, then seek back to read full header */
	uint32_t readSize;
	if (!mem_read(r, &readSize, 4)) return false;
	r->pos -= 4;

	if (readSize == 0 || readSize > INSTR_HEADER_SIZE) readSize = INSTR_HEADER_SIZE;
	if ((int32_t)readSize < 0) return false;
	if (!mem_read(r, &ih, readSize)) return false;

	/* Skip extended header data */
	ft2_xm2_ins_ext_t xm2Ext;
	bool xm2ExtValid = false;
	uint32_t extraBytesToSkip = 0;
	if (ih.instrSize > INSTR_HEADER_SIZE)
	{
		extraBytesToSkip = ih.instrSize - INSTR_HEADER_SIZE;
		if (state->isXm2 && extraBytesToSkip >= sizeof(ft2_xm2_ins_ext_t))
		{
			if (!mem_read(r, &xm2Ext, sizeof(ft2_xm2_ins_ext_t))) return false;
			if (xm2Ext.id == FT2_XM2_INS_EXT_ID && xm2Ext.version >= 1)
				xm2ExtValid = true;
			extraBytesToSkip -= (uint32_t)sizeof(ft2_xm2_ins_ext_t);
		}
		if (extraBytesToSkip > 0)
			if (!mem_skip(r, extraBytesToSkip)) return false;
	}

	const int32_t fileMaxSamples = state->isXm2 ? FT2_XM2_MAX_SMP_PER_INST : 32;
	if (ih.numSamples < 0 || ih.numSamples > fileMaxSamples) return false;

	if (instrNum <= FT2_MAX_INST)
		memcpy(song->instrName[instrNum], ih.name, 22);

	if (ih.numSamples > 0) {
		if (!ft2_instance_alloc_instr(inst, instrNum)) return false;

		ft2_instr_t *ins = rep->instr[instrNum];
		memcpy(ins->note2SampleLUT, ih.note2SampleLUT, 96);
		memcpy(ins->volEnvPoints, ih.volEnvPoints, sizeof(ins->volEnvPoints));
		memcpy(ins->panEnvPoints, ih.panEnvPoints, sizeof(ins->panEnvPoints));
		ins->volEnvLength = ih.volEnvLength;
		ins->panEnvLength = ih.panEnvLength;
		ins->volEnvSustain = ih.volEnvSustain;
		ins->volEnvLoopStart = ih.volEnvLoopStart;
		ins->volEnvLoopEnd = ih.volEnvLoopEnd;
		ins->panEnvSustain = ih.panEnvSustain;
		ins->panEnvLoopStart = ih.panEnvLoopStart;
		ins->panEnvLoopEnd = ih.panEnvLoopEnd;
		ins->volEnvFlags = ih.volEnvFlags;
		ins->panEnvFlags = ih.panEnvFlags;
		ins->autoVibType = ih.vibType;
		ins->autoVibSweep = ih.vibSweep;
		ins->autoVibDepth = ih.vibDepth;
		ins->autoVibRate = ih.vibRate;
		ins->fadeout = ih.fadeout;
		ins->midiOn = (ih.midiOn == 1);
		ins->midiChannel = ih.midiChannel;
		ins->midiProgram = ih.midiProgram;
		ins->midiBend = ih.midiBend;
		ins->mute = (ih.mute == 1);
		ins->numSamples = (ih.numSamples > state->maxSamples) ? state->maxSamples : ih.numSamples;
		ins->volEnvSpeed = 1;
		ins->panEnvSpeed = 1;
		if (xm2ExtValid)
		{
			memcpy(ins->volEnvCurve, xm2Ext.volEnvCurve, sizeof(ins->volEnvCurve));
			memcpy(ins->panEnvCurve, xm2Ext.panEnvCurve, sizeof(ins->panEnvCurve));
			if (xm2Ext.version >= 2)
			{
				ins->volEnvSpeed = (xm2Ext.volEnvSpeed == 0) ? 1 : xm2Ext.volEnvSpeed;
				ins->panEnvSpeed = (xm2Ext.panEnvSpeed == 0) ? 1 : xm2Ext.panEnvSpeed;
			}
		}
		ft2_sanitize_instrument(ins);
		if (state->maxSamples < FT2_MAX_SMP_PER_INST)
		{
			for (int32_t i = 0; i < 96; i++)
				if (ins->note2SampleLUT[i] >= state->maxSamples)
					ins->note2SampleLUT[i] = (uint8_t)(state->maxSamples - 1);
		}

		int32_t sampleHeadersToRead = ih.numSamples;
		if (sampleHeadersToRead > state->maxSamples)
			sampleHeadersToRead = state->maxSamples;

		xm_sample_header_t *smpHdrs = NULL;
		ft2_xm2_smp_ext_t *xm2Ext = NULL;
		bool *xm2ExtValid = NULL;
		bool ok = false;
		if (sampleHeadersToRead > 0)
		{
			smpHdrs = (xm_sample_header_t *)calloc((size_t)sampleHeadersToRead, sizeof(xm_sample_header_t));
			if (smpHdrs == NULL)
				return false;
			if (!mem_read(r, smpHdrs, sampleHeadersToRead * sizeof(xm_sample_header_t)))
				goto load_fail;

			xm2Ext = (ft2_xm2_smp_ext_t *)calloc((size_t)sampleHeadersToRead, sizeof(ft2_xm2_smp_ext_t));
			xm2ExtValid = (bool *)calloc((size_t)sampleHeadersToRead, sizeof(bool));
			if (xm2Ext == NULL || xm2ExtValid == NULL)
				goto load_fail;
		}

		uint32_t sampleHeaderSize = ih.sampleSize;
		if (sampleHeaderSize < sizeof(xm_sample_header_t))
			sampleHeaderSize = (uint32_t)sizeof(xm_sample_header_t);

		uint32_t extraBytes = 0;
		if (sampleHeaderSize > sizeof(xm_sample_header_t))
			extraBytes = sampleHeaderSize - (uint32_t)sizeof(xm_sample_header_t);

		if (extraBytes > 0 && sampleHeadersToRead > 0)
		{
			for (int32_t j = 0; j < sampleHeadersToRead; j++)
			{
				if (state->isXm2 && extraBytes >= sizeof(ft2_xm2_smp_ext_t))
				{
					if (!mem_read(r, &xm2Ext[j], sizeof(ft2_xm2_smp_ext_t))) goto load_fail;
					if (xm2Ext[j].id == FT2_XM2_SMP_EXT_ID && xm2Ext[j].version >= FT2_XM2_SMP_EXT_VERSION)
						xm2ExtValid[j] = true;
					if (extraBytes > sizeof(ft2_xm2_smp_ext_t))
						if (!mem_skip(r, extraBytes - sizeof(ft2_xm2_smp_ext_t))) goto load_fail;
				}
				else
				{
					if (!mem_skip(r, extraBytes)) goto load_fail;
				}
			}
		}

		/* Store lengths of extra samples (beyond runtime max) for skipping later */
		if (ih.numSamples > sampleHeadersToRead)
		{
			const int32_t samplesToSkip = ih.numSamples - sampleHeadersToRead;
			state->extraSampleLengths = (uint32_t *)calloc((size_t)samplesToSkip, sizeof(uint32_t));
			if (state->extraSampleLengths == NULL) goto load_fail;
			state->extraSampleCount = samplesToSkip;

			for (int32_t j = 0; j < samplesToSkip; j++)
			{
				if (!mem_read(r, &state->extraSampleLengths[j], 4)) goto load_fail;
				if (sampleHeaderSize > 4)
					if (!mem_skip(r, sampleHeaderSize - 4)) goto load_fail;
			}
		}

		for (int32_t j = 0; j < sampleHeadersToRead; j++) {
			ft2_sample_t *s = &ins->smp[j];
			xm_sample_header_t *src = &smpHdrs[j];
			s->length = src->length;
			s->loopStart = src->loopStart;
			s->loopLength = src->loopLength;
			s->volume = src->volume;
			s->finetune = src->finetune;
			s->flags = src->flags;
			s->panning = src->panning;
			s->relativeNote = src->relativeNote;
			memcpy(s->name, src->name, 22);
			s->format = (s->flags & SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
			s->sampleRate = 0;

			if (state->isXm2 && xm2ExtValid[j])
			{
				switch ((ft2_sample_format_t)xm2Ext[j].format)
				{
					case FT2_SMP_FORMAT_8BIT:
					case FT2_SMP_FORMAT_16BIT:
					case FT2_SMP_FORMAT_24BIT:
					case FT2_SMP_FORMAT_FLOAT32:
						s->format = (ft2_sample_format_t)xm2Ext[j].format;
						break;
					default:
						break;
				}

				s->sampleRate = xm2Ext[j].sampleRate;
			}

			if (s->format == FT2_SMP_FORMAT_16BIT)
				s->flags |= SAMPLE_16BIT;
			else
				s->flags &= ~SAMPLE_16BIT;
			/* ModPlug ADPCM: nameLength=0xAD, must be 8-bit mono */
			if (src->nameLength == 0xAD && !(src->flags & (SAMPLE_16BIT | SAMPLE_STEREO)))
				s->flags |= SAMPLE_ADPCM;
		}

		ok = true;
load_fail:
		free(smpHdrs);
		free(xm2Ext);
		free(xm2ExtValid);
		if (!ok)
			clear_extra_sample_lengths(state);
		if (!ok)
			return false;
	}
	return true;
}

/* Load sample data for an instrument. Separated from header for v1.02/v1.03 compat. */
static bool load_instr_sample(xm_loader_state_t *state, uint16_t instrNum)
{
	mem_reader_t *r = &state->reader;
	ft2_instance_t *inst = state->inst;
	ft2_replayer_state_t *rep = &inst->replayer;
	const bool isXm2 = state->isXm2;

	if (rep->instr[instrNum] == NULL) return true;

	ft2_instr_t *ins = rep->instr[instrNum];
	uint16_t k = ins->numSamples;
	if (k > state->maxSamples) k = (uint16_t)state->maxSamples;

	/* Instruments >128 loaded but samples skipped */
	if (instrNum > FT2_MAX_INST) {
		for (uint16_t j = 0; j < k; j++)
			if (ins->smp[j].length > 0)
				if (!mem_skip(r, ins->smp[j].length)) return false;
		return true;
	}

	for (uint16_t j = 0; j < k; j++) {
		ft2_sample_t *s = &ins->smp[j];
		if (s->length <= 0) { s->length = 0; s->loopStart = 0; s->loopLength = 0; s->flags = 0; continue; }

		const int32_t lengthInFile = s->length;
		const ft2_sample_format_t format = s->format;
		const bool stereoSample = !!(s->flags & SAMPLE_STEREO);
		const bool adpcmSample = !!(s->flags & SAMPLE_ADPCM);
		const bool keepStereo = isXm2 && stereoSample;
		const uint32_t fileBytesPerSample = xm2_file_bytes_per_sample(format);
		const uint32_t internalBytesPerSample = ft2_sample_format_bytes(format);

		/* Length in file is bytes; convert to samples */
		if (fileBytesPerSample == 0) return false;
		s->length /= (int32_t)fileBytesPerSample;
		s->loopStart /= (int32_t)fileBytesPerSample;
		s->loopLength /= (int32_t)fileBytesPerSample;
		if (s->length > FT2_MAX_SAMPLE_LEN) s->length = FT2_MAX_SAMPLE_LEN;

		int32_t frameLength = s->length;
		if (stereoSample)
			frameLength >>= 1;

		const int32_t padding = (int32_t)(FT2_MAX_TAPS * internalBytesPerSample);
		const size_t allocSize = (size_t)padding + ((size_t)frameLength * internalBytesPerSample) + (size_t)padding;

		/* Allocate left channel */
		if (format == FT2_SMP_FORMAT_24BIT)
		{
			s->origDataPtr24 = (int32_t *)calloc(allocSize, 1);
			if (!s->origDataPtr24) return false;
			s->dataPtr24 = (int32_t *)((uint8_t *)s->origDataPtr24 + padding);
		}
		else if (format == FT2_SMP_FORMAT_FLOAT32)
		{
			s->origDataPtrF = (float *)calloc(allocSize, 1);
			if (!s->origDataPtrF) return false;
			s->dataPtrF = (float *)((uint8_t *)s->origDataPtrF + padding);
		}
		else
		{
			s->origDataPtr = (int8_t *)calloc(allocSize, 1);
			if (!s->origDataPtr) return false;
			s->dataPtr = s->origDataPtr + padding;
		}

		/* Allocate right channel for XM2 stereo */
		if (keepStereo)
		{
			if (format == FT2_SMP_FORMAT_24BIT)
			{
				s->origDataPtr24R = (int32_t *)calloc(allocSize, 1);
				if (!s->origDataPtr24R) return false;
				s->dataPtr24R = (int32_t *)((uint8_t *)s->origDataPtr24R + padding);
			}
			else if (format == FT2_SMP_FORMAT_FLOAT32)
			{
				s->origDataPtrFR = (float *)calloc(allocSize, 1);
				if (!s->origDataPtrFR) return false;
				s->dataPtrFR = (float *)((uint8_t *)s->origDataPtrFR + padding);
			}
			else
			{
				s->origDataPtrR = (int8_t *)calloc(allocSize, 1);
				if (!s->origDataPtrR) return false;
				s->dataPtrR = s->origDataPtrR + padding;
			}
		}
		else
		{
			s->origDataPtrR = NULL;
			s->dataPtrR = NULL;
			s->origDataPtr24R = NULL;
			s->dataPtr24R = NULL;
			s->origDataPtrFR = NULL;
			s->dataPtrFR = NULL;
		}

		if (adpcmSample)
		{
			if (!load_adpcm_sample(r, s)) return false;
		}
		else if (format == FT2_SMP_FORMAT_24BIT)
		{
			const int32_t bytesPerChan = frameLength * (int32_t)fileBytesPerSample;
			const int32_t totalBytes = s->length * (int32_t)fileBytesPerSample;
			uint8_t *tmp = (uint8_t *)malloc(bytesPerChan);
			if (tmp == NULL) return false;

			if (stereoSample && !keepStereo)
			{
				uint8_t *tmpR = (uint8_t *)malloc(bytesPerChan);
				if (tmpR == NULL) { free(tmp); return false; }

				if (!mem_read(r, tmp, bytesPerChan)) { free(tmpR); free(tmp); return false; }
				if (!mem_read(r, tmpR, bytesPerChan)) { free(tmpR); free(tmp); return false; }

				int32_t *dst = s->dataPtr24;
				for (int32_t i = 0; i < frameLength; i++)
				{
					uint32_t vL = (uint32_t)tmp[i * 3] | ((uint32_t)tmp[i * 3 + 1] << 8) | ((uint32_t)tmp[i * 3 + 2] << 16);
					uint32_t vR = (uint32_t)tmpR[i * 3] | ((uint32_t)tmpR[i * 3 + 1] << 8) | ((uint32_t)tmpR[i * 3 + 2] << 16);
					int32_t sL = (vL & 0x800000) ? (int32_t)(vL | 0xFF000000) : (int32_t)vL;
					int32_t sR = (vR & 0x800000) ? (int32_t)(vR | 0xFF000000) : (int32_t)vR;
					dst[i] = (sL + sR) >> 1;
				}

				free(tmpR);
			}
			else
			{
				if (!mem_read(r, tmp, bytesPerChan)) { free(tmp); return false; }
				int32_t *dst = s->dataPtr24;
				for (int32_t i = 0; i < frameLength; i++)
				{
					uint32_t v = (uint32_t)tmp[i * 3] | ((uint32_t)tmp[i * 3 + 1] << 8) | ((uint32_t)tmp[i * 3 + 2] << 16);
					dst[i] = (v & 0x800000) ? (int32_t)(v | 0xFF000000) : (int32_t)v;
				}
				if (keepStereo && s->dataPtr24R != NULL)
				{
					if (!mem_read(r, tmp, bytesPerChan)) { free(tmp); return false; }
					int32_t *dstR = s->dataPtr24R;
					for (int32_t i = 0; i < frameLength; i++)
					{
						uint32_t v = (uint32_t)tmp[i * 3] | ((uint32_t)tmp[i * 3 + 1] << 8) | ((uint32_t)tmp[i * 3 + 2] << 16);
						dstR[i] = (v & 0x800000) ? (int32_t)(v | 0xFF000000) : (int32_t)v;
					}
				}
			}

			free(tmp);
			if (totalBytes < lengthInFile)
				if (!mem_skip(r, lengthInFile - totalBytes)) return false;
		}
		else if (format == FT2_SMP_FORMAT_FLOAT32)
		{
			const int32_t bytesPerChan = frameLength * (int32_t)fileBytesPerSample;
			const int32_t totalBytes = s->length * (int32_t)fileBytesPerSample;

			if (stereoSample && !keepStereo)
			{
				float *tmp = (float *)malloc(bytesPerChan);
				float *tmpR = (float *)malloc(bytesPerChan);
				if (tmp == NULL || tmpR == NULL) { free(tmp); free(tmpR); return false; }
				if (!mem_read(r, tmp, bytesPerChan)) { free(tmp); free(tmpR); return false; }
				if (!mem_read(r, tmpR, bytesPerChan)) { free(tmp); free(tmpR); return false; }
				for (int32_t i = 0; i < frameLength; i++)
					s->dataPtrF[i] = (tmp[i] + tmpR[i]) * 0.5f;
				free(tmp);
				free(tmpR);
			}
			else
			{
				if (!mem_read(r, s->dataPtrF, bytesPerChan)) return false;
				if (keepStereo && s->dataPtrFR != NULL)
					if (!mem_read(r, s->dataPtrFR, bytesPerChan)) return false;
			}

			if (totalBytes < lengthInFile)
				if (!mem_skip(r, lengthInFile - totalBytes)) return false;
		}
		else
		{
			const int32_t bytesPerChan = frameLength * (int32_t)fileBytesPerSample;
			const int32_t totalBytes = s->length * (int32_t)fileBytesPerSample;

			if (stereoSample && !keepStereo)
			{
				int8_t *tmp = (int8_t *)malloc(totalBytes);
				if (tmp == NULL) return false;
				if (!mem_read(r, tmp, totalBytes)) { free(tmp); return false; }
				delta_to_sample(tmp, NULL, s->length, s->flags, false);
				memcpy(s->dataPtr, tmp, bytesPerChan);
				free(tmp);
			}
			else
			{
				if (!mem_read(r, s->dataPtr, bytesPerChan)) return false;
				if (keepStereo && s->dataPtrR != NULL)
					if (!mem_read(r, s->dataPtrR, bytesPerChan)) return false;

				if (format == FT2_SMP_FORMAT_16BIT)
				{
					delta_decode_16bit((int16_t *)s->dataPtr, frameLength);
					if (keepStereo && s->dataPtrR != NULL)
						delta_decode_16bit((int16_t *)s->dataPtrR, frameLength);
				}
				else
				{
					delta_decode_8bit(s->dataPtr, frameLength);
					if (keepStereo && s->dataPtrR != NULL)
						delta_decode_8bit(s->dataPtrR, frameLength);
				}
			}

			if (totalBytes < lengthInFile)
				if (!mem_skip(r, lengthInFile - totalBytes)) return false;
		}

		if (stereoSample) { s->length >>= 1; s->loopStart >>= 1; s->loopLength >>= 1; }

		/* Clear stereo flag for standard XM (downmixed), keep for XM2 */
		if (!keepStereo && (s->flags & SAMPLE_STEREO))
			s->flags &= ~SAMPLE_STEREO;

		ft2_sanitize_sample(s);
		ft2_fix_sample(s);
	}

	/* Skip extra sample data (beyond runtime max) */
	if (state->extraSampleCount > 0 && state->extraSampleLengths != NULL)
	{
		for (int32_t j = 0; j < state->extraSampleCount; j++)
		{
			if (state->extraSampleLengths[j] > 0)
				if (!mem_skip(r, state->extraSampleLengths[j])) return false;
		}
	}
	clear_extra_sample_lengths(state);
	return true;
}

/* ---------- Pattern loading ---------- */

static bool load_patterns(xm_loader_state_t *state, uint16_t numPatterns)
{
	mem_reader_t *r = &state->reader;
	ft2_instance_t *inst = state->inst;
	ft2_replayer_state_t *rep = &inst->replayer;
	uint16_t version = state->header.version;
	uint16_t numChannels = state->header.numChannels;

	for (uint16_t i = 0; i < numPatterns; i++) {
		uint32_t headerSize;
		if (!mem_read(r, &headerSize, 4)) return false;

		uint8_t type;
		if (!mem_read(r, &type, 1)) return false;

		uint16_t numRows = 0, dataSize = 0;
		if (version == 0x0102) {
			/* v1.02: numRows stored as uint8+1 */
			uint8_t tmpLen;
			if (!mem_read(r, &tmpLen, 1)) return false;
			if (!mem_read(r, &dataSize, 2)) return false;
			numRows = tmpLen + 1;
			if (headerSize > 8) if (!mem_skip(r, headerSize - 8)) return false;
		} else {
			if (!mem_read(r, &numRows, 2)) return false;
			if (!mem_read(r, &dataSize, 2)) return false;
			if (headerSize > 9) if (!mem_skip(r, headerSize - 9)) return false;
		}

		if (numRows > FT2_MAX_PATT_LEN) numRows = FT2_MAX_PATT_LEN;
		if (numRows == 0) numRows = 64;
		rep->patternNumRows[i] = numRows;

		if (dataSize > 0) {
			rep->pattern[i] = (ft2_note_t *)calloc(numRows * FT2_MAX_CHANNELS, sizeof(ft2_note_t));
			if (rep->pattern[i] == NULL) return false;

			uint8_t *packedData = (uint8_t *)malloc(dataSize);
			if (packedData == NULL) return false;
			if (!mem_read(r, packedData, dataSize)) { free(packedData); return false; }

			unpack_pattern(rep->pattern[i], packedData, dataSize, numChannels, numRows);
			free(packedData);
		}
	}
	return true;
}

/* ---------- XM loader entry point ---------- */

bool ft2_load_xm_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL || dataSize < sizeof(xm_header_t)) return false;

	xm_loader_state_t state;
	memset(&state, 0, sizeof(state));
	mem_init(&state.reader, data, dataSize);
	state.inst = inst;

	if (!mem_read(&state.reader, &state.header, sizeof(xm_header_t))) return false;
	xm_header_t *h = &state.header;

	/* Detect XM2 format: "Extended Module2:" or version 0x0200+ */
	state.isXm2 = (memcmp(h->id, "Extended Module2:", 17) == 0) || (h->version >= 0x0200);
	state.allowXm2 = state.isXm2 && inst->config.enableXm2Features;
	state.maxSamples = state.allowXm2 ? FT2_XM2_MAX_SMP_PER_INST : FT2_XM_MAX_SMP_PER_INST;
	state.extraSampleCount = 0;
	state.extraSampleLengths = NULL;

	/* Accept both standard XM and XM2 headers */
	if (memcmp(h->id, "Extended Module: ", 17) != 0 && memcmp(h->id, "Extended Module2:", 17) != 0)
		return false;

	/* Standard XM: v1.02-v1.04, XM2: v2.00+ */
	if (!state.isXm2 && (h->version < 0x0102 || h->version > 0x0104))
		return false;
	if (h->numOrders > FT2_MAX_ORDERS) return false;
	if (h->numPatterns > FT2_MAX_PATTERNS) return false;
	if (h->numChannels == 0) return false;
	if (h->numInstr > 256) return false; /* Load >128, discard extras later */

	if (!mem_seek(&state.reader, 60 + h->headerSize)) return false;

	ft2_instance_reset(inst);
	ft2_replayer_state_t *rep = &inst->replayer;
	ft2_song_t *song = &rep->song;

	memcpy(song->name, h->name, 20);
	song->name[20] = '\0';
	song->songLength = h->numOrders;
	if (song->songLength == 0) song->songLength = 1;
	song->songLoopStart = h->songLoopStart;
	if (song->songLoopStart >= song->songLength) song->songLoopStart = 0;

	song->numChannels = (uint8_t)h->numChannels;
	if (song->numChannels > FT2_MAX_CHANNELS) song->numChannels = FT2_MAX_CHANNELS;
	if (song->numChannels & 1) { /* Round up to even */
		song->numChannels++;
		if (song->numChannels > FT2_MAX_CHANNELS) song->numChannels = FT2_MAX_CHANNELS;
	}

	song->BPM = h->BPM;
	song->speed = h->speed;
	song->initialSpeed = h->speed;
	song->globalVolume = 64;
	song->tick = 1;
	memcpy(song->orders, h->orders, FT2_MAX_ORDERS);

	/* Trim 0xFF padding from order list */
	for (int16_t j = 255; j >= 0; j--) {
		if (song->orders[j] != 0xFF) break;
		if (song->songLength > j) song->songLength = j;
	}
	if (song->songLength > 255) song->songLength = 255;

	inst->audio.linearPeriodsFlag = !!(h->flags & 1);

	/* v1.02/v1.03: headers, patterns, samples
	 * v1.04: patterns, then instruments with samples interleaved */
	if (h->version < 0x0104) {
		for (uint16_t i = 1; i <= h->numInstr; i++)
			if (!load_instr_header(&state, i)) { clear_extra_sample_lengths(&state); return false; }
		if (!load_patterns(&state, h->numPatterns)) return false;
		for (uint16_t i = 1; i <= h->numInstr; i++)
			if (!load_instr_sample(&state, i)) { clear_extra_sample_lengths(&state); return false; }
	} else {
		if (!load_patterns(&state, h->numPatterns)) return false;
		for (uint16_t i = 1; i <= h->numInstr; i++) {
			if (!load_instr_header(&state, i)) { clear_extra_sample_lengths(&state); return false; }
			if (!load_instr_sample(&state, i)) { clear_extra_sample_lengths(&state); return false; }
		}
	}

	/* Discard instruments >128 */
	if (h->numInstr > FT2_MAX_INST) {
		for (int32_t i = FT2_MAX_INST + 1; i <= h->numInstr; i++) {
			free(rep->instr[i]);
			rep->instr[i] = NULL;
		}
	}

	/* Clamp sample counts to runtime limit */
	for (int32_t i = 1; i <= FT2_MAX_INST; i++)
		if (rep->instr[i] != NULL && rep->instr[i]->numSamples > state.maxSamples)
			rep->instr[i]->numSamples = state.maxSamples;

	song->pattNum = song->orders[0];
	if (song->pattNum >= FT2_MAX_PATTERNS) song->pattNum = 0;
	song->currNumRows = rep->patternNumRows[song->pattNum];

	ft2_set_bpm(inst, song->BPM);

	song->songPos = 0;
	song->row = 0;

	ft2_xm2_update_usage(inst);

	inst->uiState.updatePosEdScrollBar = true;
	inst->uiState.updatePosSections = true;
	hideAllTopLeftPanelOverlays(inst);
	inst->uiState.needsFullRedraw = true;
	return true;
}

/* ---------- Auto-detect loader ---------- */

enum { FORMAT_UNKNOWN = 0, FORMAT_XM, FORMAT_MOD, FORMAT_S3M };

static int detect_module_format(const uint8_t *data, uint32_t dataSize)
{
	if (data == NULL || dataSize < 48) return FORMAT_UNKNOWN;
	if (dataSize >= 17 && memcmp(data, "Extended Module: ", 17) == 0) return FORMAT_XM;
	if (dataSize >= 17 && memcmp(data, "Extended Module2:", 17) == 0) return FORMAT_XM;  /* XM2 uses same loader */
	if (dataSize >= 48 && memcmp(&data[0x2C], "SCRM", 4) == 0 && data[0x1D] == 16) return FORMAT_S3M;
	if (dataSize >= 1084 && detect_mod_format(data, dataSize)) return FORMAT_MOD;
	return FORMAT_UNKNOWN;
}

bool ft2_load_module(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL || dataSize == 0) return false;

	int format = detect_module_format(data, dataSize);
	bool loaded = false;

	switch (format) {
		case FORMAT_XM:  loaded = ft2_load_xm_from_memory(inst, data, dataSize); break;
		case FORMAT_MOD: loaded = load_mod_from_memory(inst, data, dataSize); break;
		case FORMAT_S3M: loaded = load_s3m_from_memory(inst, data, dataSize); break;
		default: return false;
	}

	if (loaded) {
		inst->uiState.channelOffset = 0;
		inst->uiState.updateChanScrollPos = true;

		/* Apply locked speed if Fxx changes disabled */
		if (!inst->config.allowFxxSpeedChanges) {
			inst->config.savedSpeed = inst->replayer.song.speed;
			inst->config.lockedSpeed = (inst->config.savedSpeed == 3) ? 3 : 6;
			inst->replayer.song.speed = inst->config.lockedSpeed;
		}

		/* Save module BPM for restore when DAW sync disabled */
		if (inst->config.syncBpmFromDAW)
			inst->config.savedBpm = inst->replayer.song.BPM;

		ft2_timemap_invalidate(inst);
	}
	return loaded;
}
