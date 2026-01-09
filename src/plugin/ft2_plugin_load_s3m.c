/**
 * @file ft2_plugin_load_s3m.c
 * @brief S3M loader (Scream Tracker 3).
 *
 * Converts S3M effects to XM effects, handles unsigned samples,
 * stereo downmixing, and C4 frequency calculation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_load_s3m.h"
#include "ft2_plugin_mem_reader.h"
#include "ft2_plugin_sample_ed.h"
#include "../ft2_instance.h"

#define NOTE_C4 48
#define NOTE_OFF 97
#define C4_FREQ 8363.0
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct s3mSmpHdr_t {
	uint8_t type, junk1[12], offsetInFileH;
	uint16_t offsetInFile;
	int32_t length, loopStart, loopEnd;
	uint8_t volume, junk2, packFlag, flags;
	int32_t midCFreq, junk3;
	uint16_t junk4;
	uint8_t junk5[6];
	char name[28], ID[4];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
s3mSmpHdr_t;

typedef struct s3mHdr_t {
	char name[28];
	uint8_t junk1, type;
	uint16_t junk2;
	int16_t numOrders, numSamples, numPatterns;
	uint16_t flags, junk3, version;
	char ID[4];
	uint8_t junk4, speed, BPM, junk5, junk6[12], chnSettings[32];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
s3mHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* ---------- Sample conversion ---------- */

/* S3M samples are unsigned; convert to signed, downmix stereo to mono */
static void conv8BitSample(int8_t *p, int32_t length, bool stereo)
{
	if (stereo) {
		length >>= 1;
		int8_t *p2 = &p[length];
		for (int32_t i = 0; i < length; i++)
			p[i] = (int8_t)(((p[i] ^ 0x80) + (p2[i] ^ 0x80)) >> 1);
	} else {
		for (int32_t i = 0; i < length; i++) p[i] ^= 0x80;
	}
}

static void conv16BitSample(int8_t *p, int32_t length, bool stereo)
{
	int16_t *p16_1 = (int16_t *)p;
	if (stereo) {
		length >>= 1;
		int16_t *p16_2 = p16_1 + length;
		for (int32_t i = 0; i < length; i++)
			p16_1[i] = (int16_t)(((p16_1[i] ^ 0x8000) + (p16_2[i] ^ 0x8000)) >> 1);
	} else {
		for (int32_t i = 0; i < length; i++) p16_1[i] ^= 0x8000;
	}
}

/* Convert S3M C4 frequency to XM relative note + finetune */
static void setSampleC4Hz(ft2_sample_t *s, double dC4Hz)
{
	const double dC4PeriodOffset = (NOTE_C4 * 16) + 16;
	int32_t period = (int32_t)round(dC4PeriodOffset + (log2(dC4Hz / C4_FREQ) * 12.0 * 16.0));
	period = CLAMP(period, 0, (12 * 16 * 10) - 1);
	s->finetune = ((period & 31) - 16) << 3;
	s->relativeNote = (int8_t)(((period & ~31) >> 4) - NOTE_C4);
}

/* Find highest channel with note data */
static int32_t countS3MChannels(ft2_instance_t *inst, uint16_t numPatterns)
{
	int32_t channels = 0;
	ft2_replayer_state_t *rep = &inst->replayer;
	for (int32_t i = 0; i < numPatterns; i++) {
		if (rep->pattern[i] == NULL) continue;
		ft2_note_t *p = rep->pattern[i];
		for (int32_t j = 0; j < 64; j++) {
			for (int32_t k = 0; k < FT2_MAX_CHANNELS; k++, p++) {
				if (p->note || p->instr || p->vol || p->efx || p->efxData)
					if (k > channels) channels = k;
			}
		}
	}
	return channels + 1;
}

/* ---------- Format detection ---------- */

bool detect_s3m_format(const uint8_t *data, uint32_t dataSize)
{
	if (data == NULL || dataSize < 48) return false;
	return (memcmp(&data[0x2C], "SCRM", 4) == 0 && data[0x1D] == 16);
}

/* ---------- S3M loader ---------- */

bool load_s3m_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL) return false;

	mem_reader_t reader;
	mem_init(&reader, data, dataSize);

	s3mHdr_t hdr;
	if (dataSize < sizeof(hdr)) return false;
	memset(&hdr, 0, sizeof(hdr));
	if (!mem_read(&reader, &hdr, sizeof(hdr))) return false;

	if (hdr.numSamples > FT2_MAX_INST || hdr.numOrders > FT2_MAX_ORDERS ||
	    hdr.numPatterns > FT2_MAX_PATTERNS || hdr.type != 16 ||
	    hdr.version < 1 || hdr.version > 2)
		return false;

	ft2_instance_reset(inst);
	ft2_replayer_state_t *rep = &inst->replayer;
	ft2_song_t *song = &rep->song;

	inst->audio.linearPeriodsFlag = false; /* S3M uses Amiga periods */

	memset(song->orders, 255, 256);
	if (!mem_read(&reader, song->orders, hdr.numOrders)) return false;
	song->songLength = hdr.numOrders;

	/* Remove pattern separators (0xFE) and end markers (0xFF) */
	int32_t k = 0, j = 0;
	for (int32_t i = 0; i < song->songLength; i++) {
		if (song->orders[i] != 254) song->orders[j++] = song->orders[i];
		else k++;
	}
	song->songLength = (k <= song->songLength) ? song->songLength - (uint16_t)k : 1;

	for (int32_t i = 1; i < song->songLength; i++)
		if (song->orders[i] == 255) { song->songLength = (uint16_t)i; break; }

	if (song->songLength < 255) memset(&song->orders[song->songLength], 0, 255 - song->songLength);

	memcpy(song->name, hdr.name, 20);
	song->name[20] = '\0';
	song->BPM = hdr.BPM;
	song->speed = hdr.speed;
	song->initialSpeed = hdr.speed;
	song->globalVolume = 64;
	song->tick = 1;

	/* Offsets stored as parapointers (16-byte paragraphs) */
	int32_t sampleOffsets[256], patternOffsets[256];
	for (int32_t i = 0; i < hdr.numSamples; i++) {
		uint16_t offset;
		if (!mem_read(&reader, &offset, 2)) return false;
		sampleOffsets[i] = offset << 4;
	}
	for (int32_t i = 0; i < hdr.numPatterns; i++) {
		uint16_t offset;
		if (!mem_read(&reader, &offset, 2)) return false;
		patternOffsets[i] = offset << 4;
	}

	/* Effect memory for S3M->XM conversion */
	uint8_t alastnfo[32], alastefx[32], alastvibnfo[32], s3mLastGInstr[32];

	/* Load patterns - S3M uses packed format with channel mask byte */
	for (int32_t i = 0; i < hdr.numPatterns; i++) {
		if (patternOffsets[i] == 0) continue;

		memset(alastnfo, 0, sizeof(alastnfo));
		memset(alastefx, 0, sizeof(alastefx));
		memset(alastvibnfo, 0, sizeof(alastvibnfo));
		memset(s3mLastGInstr, 0, sizeof(s3mLastGInstr));

		if (!mem_seek(&reader, patternOffsets[i])) continue;
		uint16_t pattLen;
		if (!mem_read(&reader, &pattLen, 2)) continue;
		if (pattLen == 0 || pattLen > 12288) continue;

		rep->pattern[i] = (ft2_note_t *)calloc(64 * FT2_MAX_CHANNELS, sizeof(ft2_note_t));
		if (rep->pattern[i] == NULL) return false;
		rep->patternNumRows[i] = 64;

		uint8_t pattBuff[12288];
		if (!mem_read(&reader, pattBuff, pattLen)) continue;

		int32_t pos = 0, row = 0;
		while (pos < pattLen && row < 64) {
			uint8_t bits = pattBuff[pos++];
			if (bits == 0) { row++; continue; }

			int32_t chn = bits & 31;
			ft2_note_t tmpNote;
			memset(&tmpNote, 0, sizeof(tmpNote));

			/* Note/instrument (bit 5) */
			if (bits & 32) {
				tmpNote.note = pattBuff[pos++];
				tmpNote.instr = pattBuff[pos++];
				if (tmpNote.instr > FT2_MAX_INST) tmpNote.instr = 0;
				if (tmpNote.note == 254) tmpNote.note = NOTE_OFF;
				else if (tmpNote.note == 255) tmpNote.note = 0;
				else {
					tmpNote.note = 1 + (tmpNote.note & 0xF) + (tmpNote.note >> 4) * 12;
					if (tmpNote.note > 96) tmpNote.note = 0;
				}
			}

			/* Volume (bit 6) */
			if (bits & 64) {
				tmpNote.vol = pattBuff[pos++];
				tmpNote.vol = (tmpNote.vol <= 64) ? tmpNote.vol + 0x10 : 0;
			}

			/* Effect (bit 7) - convert S3M to XM */
			if (bits & 128) {
				tmpNote.efx = pattBuff[pos++];
				tmpNote.efxData = pattBuff[pos++];

				/* S3M effect memory */
				if (tmpNote.efxData > 0) {
					alastnfo[chn] = tmpNote.efxData;
					if (tmpNote.efx == 8 || tmpNote.efx == 21) alastvibnfo[chn] = tmpNote.efxData;
				}
				if (tmpNote.efxData == 0 && tmpNote.efx != 7) {
					uint8_t efx = tmpNote.efx;
					if (efx == 8 || efx == 21) tmpNote.efxData = alastvibnfo[chn];
					else if ((efx >= 4 && efx <= 12) || (efx >= 17 && efx <= 19)) tmpNote.efxData = alastnfo[chn];
					if (efx == alastefx[chn] && tmpNote.efx != 10 && tmpNote.efx != 19) {
						uint8_t nfo = tmpNote.efxData;
						bool extraFine = (efx == 5 || efx == 6) && ((nfo & 0xF0) == 0xE0);
						bool fineVol = (efx == 4 || efx == 11) && ((nfo > 0xF0) || (((nfo & 0xF) == 0xF) && ((nfo & 0xF0) > 0)));
						if (!extraFine && !fineVol) tmpNote.efxData = 0;
					}
				}
				if (tmpNote.efx > 0) alastefx[chn] = tmpNote.efx;

				/* S3M->XM effect conversion */
				int16_t tmp;
				switch (tmpNote.efx) {
					case 1: /* A: Set speed */
						tmpNote.efx = 0xF;
						if (tmpNote.efxData == 0 || tmpNote.efxData > 0x1F)
							tmpNote.efxData = (tmpNote.efxData == 0) ? (tmpNote.efx = 0, 0) : 0x1F;
						break;
					case 2: tmpNote.efx = 0xB; break; /* B: Position jump */
					case 3: tmpNote.efx = 0xD; break; /* C: Pattern break */
					case 4: /* D: Volume slide */
						if (tmpNote.efxData > 0xF0) { tmpNote.efx = 0xE; tmpNote.efxData = 0xB0 | (tmpNote.efxData & 0xF); }
						else if ((tmpNote.efxData & 0x0F) == 0x0F && (tmpNote.efxData & 0xF0) > 0) { tmpNote.efx = 0xE; tmpNote.efxData = 0xA0 | (tmpNote.efxData >> 4); }
						else { tmpNote.efx = 0xA; if (tmpNote.efxData & 0x0F) tmpNote.efxData &= 0x0F; }
						break;
					case 5: /* E: Portamento down */
					case 6: /* F: Portamento up */
						if ((tmpNote.efxData & 0xF0) >= 0xE0) {
							tmp = ((tmpNote.efxData & 0xF0) == 0xE0) ? 0x21 : 0xE;
							tmpNote.efxData = (tmpNote.efxData & 0x0F) | ((tmpNote.efx == 5) ? 0x20 : 0x10);
							tmpNote.efx = (uint8_t)tmp;
							if (tmpNote.efx == 0x21 && tmpNote.efxData == 0) tmpNote.efx = 0;
						} else tmpNote.efx = 7 - tmpNote.efx;
						break;
					case 7: /* G: Tone portamento - preserve last instrument */
						tmpNote.efx = 3;
						if (tmpNote.instr != 0 && tmpNote.instr != s3mLastGInstr[chn]) tmpNote.instr = s3mLastGInstr[chn];
						break;
					case 8: tmpNote.efx = 0x04; break; /* H: Vibrato */
					case 9: tmpNote.efx = 0x1D; break; /* I: Tremor */
					case 10: tmpNote.efx = 0x00; break; /* J: Arpeggio */
					case 11: /* K: Vibrato + vol slide */
						if (tmpNote.efxData > 0xF0) { tmpNote.efx = 0xE; tmpNote.efxData = 0xB0 | (tmpNote.efxData & 0xF); if (!tmpNote.vol) tmpNote.vol = 0xB0; }
						else if ((tmpNote.efxData & 0x0F) == 0x0F && (tmpNote.efxData & 0xF0) > 0) { tmpNote.efx = 0xE; tmpNote.efxData = 0xA0 | (tmpNote.efxData >> 4); if (!tmpNote.vol) tmpNote.vol = 0xB0; }
						else { tmpNote.efx = 6; if (tmpNote.efxData & 0x0F) tmpNote.efxData &= 0x0F; }
						break;
					case 12: tmpNote.efx = 0x05; break; /* L: Tone porta + vol slide */
					case 15: tmpNote.efx = 0x09; break; /* O: Sample offset */
					case 17: tmpNote.efx = 0x1B; break; /* Q: Retrig + vol slide */
					case 18: tmpNote.efx = 0x07; break; /* R: Tremolo */
					case 19: /* S: Extended effects */
						tmpNote.efx = 0xE;
						tmp = tmpNote.efxData >> 4;
						tmpNote.efxData &= 0x0F;
						if (tmp == 0x1) tmpNote.efxData |= 0x30;
						else if (tmp == 0x2) tmpNote.efxData |= 0x50;
						else if (tmp == 0x3) tmpNote.efxData |= 0x40;
						else if (tmp == 0x4) tmpNote.efxData |= 0x70;
						else if (tmp == 0x8) { tmpNote.efx = 8; tmpNote.efxData = ((tmpNote.efxData & 0x0F) << 4) | (tmpNote.efxData & 0x0F); }
						else if (tmp == 0xB) tmpNote.efxData |= 0x60;
						else if (tmp == 0xC) { tmpNote.efxData |= 0xC0; if (tmpNote.efxData == 0xC0) tmpNote.efx = tmpNote.efxData = 0; }
						else if (tmp == 0xD) { tmpNote.efxData |= 0xD0; if (!tmpNote.note || tmpNote.note == NOTE_OFF) tmpNote.efx = tmpNote.efxData = 0; else if (tmpNote.efxData == 0xD0) memset(&tmpNote, 0, sizeof(tmpNote)); }
						else if (tmp == 0xE) tmpNote.efxData |= 0xE0;
						else if (tmp == 0xF) tmpNote.efxData |= 0xF0;
						else tmpNote.efx = tmpNote.efxData = 0;
						break;
					case 20: /* T: Set tempo */
						tmpNote.efx = 0xF;
						if (tmpNote.efxData < 0x21) tmpNote.efx = tmpNote.efxData = 0;
						break;
					case 22: /* V: Set global volume */
						if (tmpNote.efxData > 0x40) tmpNote.efx = tmpNote.efxData = 0;
						else tmpNote.efx = 0x10;
						break;
					case 24: /* X: Set panning (0-80 -> 0-255) */
						if (tmpNote.efxData > 0x80) tmpNote.efx = tmpNote.efxData = 0;
						else { tmpNote.efx = 8; int32_t pan = tmpNote.efxData * 2; tmpNote.efxData = (pan > 255) ? 255 : (uint8_t)pan; }
						break;
					default: tmpNote.efx = tmpNote.efxData = 0; break;
				}
			}

			if (tmpNote.instr != 0 && tmpNote.efx != 3) s3mLastGInstr[chn] = tmpNote.instr;
			rep->pattern[i][(row * FT2_MAX_CHANNELS) + chn] = tmpNote;
		}
	}

	/* Load samples (type 1 = PCM, type 2 = AdLib which we skip) */
	for (int32_t i = 0; i < hdr.numSamples; i++) {
		if (sampleOffsets[i] == 0) continue;
		if (!mem_seek(&reader, sampleOffsets[i])) continue;

		s3mSmpHdr_t smpHdr;
		if (!mem_read(&reader, &smpHdr, sizeof(smpHdr))) continue;
		memcpy(song->instrName[1 + i], smpHdr.name, 22);

		if (smpHdr.type == 1) {
			int32_t offsetInFile = ((smpHdr.offsetInFileH << 16) | smpHdr.offsetInFile) << 4;
			if ((smpHdr.flags & (255 - 1 - 2 - 4)) != 0 || smpHdr.packFlag != 0) continue;

			if (offsetInFile > 0 && smpHdr.length > 0) {
				if (!ft2_instance_alloc_instr(inst, 1 + i)) return false;

				ft2_instr_t *ins = rep->instr[1 + i];
				ft2_sample_t *s = &ins->smp[0];

				if (smpHdr.midCFreq > 65535) smpHdr.midCFreq = 65535;
				memcpy(s->name, smpHdr.name, 22);

				if (offsetInFile + smpHdr.length > (int32_t)dataSize)
					smpHdr.length = dataSize - offsetInFile;

				bool hasLoop = !!(smpHdr.flags & 1);
				bool stereoSample = !!(smpHdr.flags & 2);
				bool sample16Bit = !!(smpHdr.flags & 4);

				if (stereoSample) smpHdr.length <<= 1;
				int32_t lengthInFile = smpHdr.length;

				s->length = smpHdr.length;
				s->volume = (smpHdr.volume > 64) ? 64 : smpHdr.volume;
				s->loopStart = smpHdr.loopStart;
				s->loopLength = smpHdr.loopEnd - smpHdr.loopStart;
				setSampleC4Hz(s, smpHdr.midCFreq);

				if (sample16Bit) { s->flags |= SAMPLE_16BIT; lengthInFile <<= 1; }
				if (!allocateSmpData(inst, 1 + i, 0, s->length, sample16Bit)) return false;

				if (s->loopLength <= 1 || s->loopStart + s->loopLength > s->length) {
					s->loopStart = 0; s->loopLength = 0; hasLoop = false;
				}
				if (hasLoop) s->flags |= LOOP_FWD;

				/* Version 1 samples use different format, not supported */
				if (hdr.version == 1) {
					memset(s->dataPtr, 0, s->length * (sample16Bit ? 2 : 1));
				} else {
					if (!mem_seek(&reader, offsetInFile)) {
						memset(s->dataPtr, 0, s->length * (sample16Bit ? 2 : 1));
					} else {
						uint32_t bytesToRead = s->length * (sample16Bit ? 2 : 1);
						if (mem_remaining(&reader) < bytesToRead) bytesToRead = mem_remaining(&reader);
						if (bytesToRead > 0) { memcpy(s->dataPtr, mem_ptr(&reader), bytesToRead); mem_skip(&reader, bytesToRead); }

						if (sample16Bit) conv16BitSample(s->dataPtr, s->length, stereoSample);
						else conv8BitSample(s->dataPtr, s->length, stereoSample);

						if (stereoSample) s->length >>= 1;
					}
				}
				ft2_fix_sample(s);
			}
		}
	}

	/* Determine channel count from pattern data */
	song->numChannels = countS3MChannels(inst, hdr.numPatterns);
	if (song->numChannels < 1) song->numChannels = 4;
	if (song->numChannels > FT2_MAX_CHANNELS) song->numChannels = FT2_MAX_CHANNELS;
	if (song->numChannels & 1) { /* Round up to even */
		song->numChannels++;
		if (song->numChannels > FT2_MAX_CHANNELS) song->numChannels = FT2_MAX_CHANNELS;
	}

	song->songPos = 0;
	song->row = 0;
	inst->uiState.updatePosEdScrollBar = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.needsFullRedraw = true;
	return true;
}

