/**
 * @file ft2_plugin_load_mod.c
 * @brief NoiseTracker/ProTracker MOD loader for plugin architecture.
 *
 * Ported from src/modloaders/ft2_load_mod.c to work with memory buffers
 * and instance-based state.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ft2_plugin_load_mod.h"
#include "ft2_plugin_mem_reader.h"
#include "ft2_plugin_sample_ed.h"
#include "../ft2_instance.h"

/* Supported 31-sample MOD types */
enum
{
	MOD_FORMAT_MK,     /* ProTracker or compatible */
	MOD_FORMAT_FLT4,   /* StarTrekker (4ch modules) */
	MOD_FORMAT_FLT8,   /* StarTrekker (8ch modules) */
	MOD_FORMAT_FT2,    /* FT2 or compatible (multi-channel) */
	MOD_FORMAT_NT,     /* NoiseTracker */
	MOD_FORMAT_HMNT,   /* His Master's NoiseTracker (special one) */
	MOD_FORMAT_UNKNOWN
};

/* MOD period table for period-to-note conversion */
static const uint16_t modPeriods[8 * 12] =
{
	6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3624,
	3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
	 856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
	 428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
	 214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
	 107,  101,   95,   90,   85,   80,   75,   71,   67,   63,   60,   56,
	  53,   50,   47,   45,   42,   40,   37,   35,   33,   31,   30,   28
};

/* Byte swap macros for big-endian MOD data */
#define SWAP16(x) ((((uint16_t)(x) & 0x00FF) << 8) | (((uint16_t)(x) & 0xFF00) >> 8))

/* Finetune conversion */
#define FINETUNE_MOD2XM(f) (((uint8_t)(f) & 0x0F) << 4)

/* Use existing loop type macros from ft2_plugin_sample_ed.h */

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

typedef struct modSmpHdr_t
{
	char name[22];
	uint16_t length;
	uint8_t finetune, volume;
	uint16_t loopStart, loopLength;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
modSmpHdr_t;

typedef struct modHdr_t
{
	char name[20];
	modSmpHdr_t smp[31];
	uint8_t numOrders, songLoopStart, orders[128];
	char ID[4];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
modHdr_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint8_t getModType(uint8_t *numChannels, const char *id)
{
#define IS_ID(s, b) (!strncmp(s, b, 4))

	uint8_t modFormat = MOD_FORMAT_UNKNOWN;
	*numChannels = 4;

	if (IS_ID("M.K.", id) || IS_ID("M!K!", id) || IS_ID("NSMS", id) || IS_ID("LARD", id) || IS_ID("PATT", id))
	{
		modFormat = MOD_FORMAT_MK;
	}
	else if (isdigit(id[0]) && id[1] == 'C' && id[2] == 'H' && id[3] == 'N')
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = id[0] - '0';
	}
	else if (isdigit(id[0]) && isdigit(id[1]) && id[2] == 'C' && id[3] == 'H')
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = ((id[0] - '0') * 10) + (id[1] - '0');
	}
	else if (isdigit(id[0]) && isdigit(id[1]) && id[2] == 'C' && id[3] == 'N')
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = ((id[0] - '0') * 10) + (id[1] - '0');
	}
	else if (IS_ID("CD61", id) || IS_ID("CD81", id))
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = id[2] - '0';
	}
	else if (id[0] == 'F' && id[1] == 'A' && id[2] == '0' && id[3] >= '4' && id[3] <= '8')
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = id[3] - '0';
	}
	else if (IS_ID("OKTA", id) || IS_ID("OCTA", id))
	{
		modFormat = MOD_FORMAT_FT2;
		*numChannels = 8;
	}
	else if (IS_ID("FLT4", id) || IS_ID("EXO4", id))
	{
		modFormat = MOD_FORMAT_FLT4;
	}
	else if (IS_ID("FLT8", id) || IS_ID("EXO8", id))
	{
		modFormat = MOD_FORMAT_FLT8;
		*numChannels = 8;
	}
	else if (IS_ID("N.T.", id))
	{
		modFormat = MOD_FORMAT_NT;
	}
	else if (IS_ID("M&K!", id) || IS_ID("FEST", id))
	{
		modFormat = MOD_FORMAT_HMNT;
	}

	return modFormat;
#undef IS_ID
}

bool detect_mod_format(const uint8_t *data, uint32_t dataSize)
{
	if (data == NULL || dataSize < 1084)
		return false;

	/* Check MOD ID at offset 1080 */
	const char *id = (const char *)&data[1080];
	uint8_t numChannels;
	return getModType(&numChannels, id) != MOD_FORMAT_UNKNOWN;
}

bool load_mod_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL)
		return false;

	mem_reader_t reader;
	mem_init(&reader, data, dataSize);

	modHdr_t hdr;
	if (dataSize < sizeof(hdr))
		return false;

	memset(&hdr, 0, sizeof(hdr));
	if (!mem_read(&reader, &hdr, sizeof(hdr)))
		return false;

	uint8_t numChannels;
	uint8_t modFormat = getModType(&numChannels, hdr.ID);
	if (modFormat == MOD_FORMAT_UNKNOWN)
		return false;

	if (modFormat == MOD_FORMAT_MK && hdr.numOrders == 129)
		hdr.numOrders = 127;

	if (numChannels == 0 || hdr.numOrders < 1 || hdr.numOrders > 128)
		return false;

	bool tooManyChannels = numChannels > FT2_MAX_CHANNELS;

	/* Reset instance */
	ft2_instance_reset(inst);

	ft2_replayer_state_t *rep = &inst->replayer;
	ft2_song_t *song = &rep->song;

	/* Use Amiga periods for MOD */
	inst->audio.linearPeriodsFlag = false;

	song->numChannels = tooManyChannels ? FT2_MAX_CHANNELS : numChannels;
	song->songLength = hdr.numOrders;
	song->songLoopStart = hdr.songLoopStart;
	if (song->songLoopStart >= song->songLength)
		song->songLoopStart = 0;
	song->BPM = 125;
	song->speed = 6;
	song->initialSpeed = 6;
	song->globalVolume = 64;
	song->tick = 1;

	memcpy(song->orders, hdr.orders, 128);

	/* Copy song name */
	memcpy(song->name, hdr.name, 20);
	song->name[20] = '\0';

	/* Copy instrument names */
	for (int a = 0; a < 31; a++)
	{
		if (modFormat != MOD_FORMAT_HMNT)
			memcpy(song->instrName[1 + a], hdr.smp[a].name, 22);
	}

	/* Count number of patterns */
	uint16_t numPatterns = 0;
	for (int a = 0; a < 128; a++)
	{
		if (modFormat == MOD_FORMAT_FLT8)
			song->orders[a] >>= 1;

		if (song->orders[a] > numPatterns)
			numPatterns = song->orders[a];
	}
	numPatterns++;

	/* Load pattern data */
	if (modFormat != MOD_FORMAT_FLT8)
	{
		/* Normal pattern loading */
		for (uint16_t a = 0; a < numPatterns; a++)
		{
			/* Allocate pattern */
			rep->pattern[a] = (ft2_note_t *)calloc(64 * FT2_MAX_CHANNELS, sizeof(ft2_note_t));
			if (rep->pattern[a] == NULL)
				return false;
			rep->patternNumRows[a] = 64;

			for (int j = 0; j < 64; j++)
			{
				for (int k = 0; k < song->numChannels; k++)
				{
					ft2_note_t *p = &rep->pattern[a][(j * FT2_MAX_CHANNELS) + k];
					uint8_t bytes[4];
					if (!mem_read(&reader, bytes, 4))
					{
						memset(p, 0, sizeof(ft2_note_t));
						continue;
					}

					/* Period to note */
					uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					p->note = 0;
					for (int i = 0; i < 8 * 12; i++)
					{
						if (period >= modPeriods[i])
						{
							p->note = (uint8_t)i + 1;
							break;
						}
					}

					p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					p->efx = bytes[2] & 0x0F;
					p->efxData = bytes[3];
				}

				if (tooManyChannels)
				{
					int32_t remainingChans = numChannels - song->numChannels;
					mem_skip(&reader, remainingChans * 4);
				}
			}
		}
	}
	else
	{
		/* FLT8 pattern loading */
		for (uint16_t a = 0; a < numPatterns; a++)
		{
			rep->pattern[a] = (ft2_note_t *)calloc(64 * FT2_MAX_CHANNELS, sizeof(ft2_note_t));
			if (rep->pattern[a] == NULL)
				return false;
			rep->patternNumRows[a] = 64;
		}

		for (uint16_t a = 0; a < numPatterns * 2; a++)
		{
			int32_t pattNum = a >> 1;
			int32_t chnOffset = (a & 1) * 4;

			for (int j = 0; j < 64; j++)
			{
				for (int k = 0; k < 4; k++)
				{
					ft2_note_t *p = &rep->pattern[pattNum][(j * FT2_MAX_CHANNELS) + (k + chnOffset)];
					uint8_t bytes[4];
					if (!mem_read(&reader, bytes, 4))
					{
						memset(p, 0, sizeof(ft2_note_t));
						continue;
					}

					/* Period to note */
					uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					p->note = 0;
					for (int i = 0; i < 8 * 12; i++)
					{
						if (period >= modPeriods[i])
						{
							p->note = (uint8_t)i + 1;
							break;
						}
					}

					p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					p->efx = bytes[2] & 0x0F;
					p->efxData = bytes[3];
				}
			}
		}
	}

	/* Pattern command conversion */
	for (uint16_t a = 0; a < numPatterns; a++)
	{
		if (rep->pattern[a] == NULL)
			continue;

		for (int j = 0; j < 64; j++)
		{
			for (int k = 0; k < song->numChannels; k++)
			{
				ft2_note_t *p = &rep->pattern[a][(j * FT2_MAX_CHANNELS) + k];

				if (p->efx == 0xC)
				{
					if (p->efxData > 64)
						p->efxData = 64;
				}
				else if (p->efx == 0x1)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0x2)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0x5)
				{
					if (p->efxData == 0)
						p->efx = 0x3;
				}
				else if (p->efx == 0x6)
				{
					if (p->efxData == 0)
						p->efx = 0x4;
				}
				else if (p->efx == 0xA)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0xE)
				{
					if (p->efxData == 0x10 || p->efxData == 0x20 || 
					    p->efxData == 0xA0 || p->efxData == 0xB0)
					{
						p->efx = 0;
						p->efxData = 0;
					}
				}

				if (modFormat == MOD_FORMAT_NT || modFormat == MOD_FORMAT_HMNT)
				{
					if (p->efx == 0xD)
						p->efxData = 0;
					if (p->efx == 0xF && p->efxData == 0)
						p->efx = 0;
				}
				else if (modFormat == MOD_FORMAT_FLT4 || modFormat == MOD_FORMAT_FLT8)
				{
					if (p->efx == 0xE)
					{
						p->efx = 0;
						p->efxData = 0;
					}
					if (p->efx == 0xF && p->efxData > 0x1F)
						p->efxData = 0x1F;
				}
			}
		}
	}

	/* Load samples */
	for (int a = 0; a < 31; a++)
	{
		if (hdr.smp[a].length == 0)
			continue;

		/* Allocate instrument */
		if (!ft2_instance_alloc_instr(inst, 1 + a))
			return false;

		ft2_instr_t *ins = rep->instr[1 + a];
		ft2_sample_t *s = &ins->smp[0];

		/* Copy sample name */
		if (modFormat != MOD_FORMAT_HMNT)
			memcpy(s->name, hdr.smp[a].name, 22);

		/* Finetune conversion for His Master's NoiseTracker */
		uint8_t finetune = hdr.smp[a].finetune;
		if (modFormat == MOD_FORMAT_HMNT)
			finetune = (uint8_t)((-finetune & 0x1F) >> 1);

		s->length = 2 * SWAP16(hdr.smp[a].length);
		s->finetune = FINETUNE_MOD2XM(finetune);
		s->volume = hdr.smp[a].volume;
		if (s->volume > 64)
			s->volume = 64;
		s->loopStart = 2 * SWAP16(hdr.smp[a].loopStart);
		s->loopLength = 2 * SWAP16(hdr.smp[a].loopLength);

		/* Fix for poorly converted STK -> PT/NT modules */
		if (s->loopLength > 2 && s->loopStart + s->loopLength > s->length)
		{
			if ((s->loopStart >> 1) + s->loopLength <= s->length)
				s->loopStart >>= 1;
		}

		/* Fix overflown loop */
		if (s->loopStart + s->loopLength > s->length)
		{
			if (s->loopStart >= s->length)
			{
				s->loopStart = 0;
				s->loopLength = 0;
			}
			else
			{
				s->loopLength = s->length - s->loopStart;
			}
		}

		if (s->loopStart + s->loopLength > 2)
			s->flags |= LOOP_FWD;

		/* Allocate and read sample data */
		if (!allocateSmpData(inst, 1 + a, 0, s->length, false))
			return false;

		uint32_t bytesToRead = s->length;
		if (mem_remaining(&reader) < bytesToRead)
			bytesToRead = mem_remaining(&reader);

		if (bytesToRead > 0)
		{
			memcpy(s->dataPtr, mem_ptr(&reader), bytesToRead);
			mem_skip(&reader, bytesToRead);
		}

		if (bytesToRead < (uint32_t)s->length)
			memset(&s->dataPtr[bytesToRead], 0, s->length - bytesToRead);

		if (GET_LOOPTYPE(s->flags) == LOOP_OFF)
		{
			s->loopLength = 0;
			s->loopStart = 0;
		}

		ft2_fix_sample(s);
	}

	/* Update UI flags */
	song->songPos = 0;
	song->row = 0;
	inst->uiState.updatePosEdScrollBar = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.needsFullRedraw = true;

	return true;
}

