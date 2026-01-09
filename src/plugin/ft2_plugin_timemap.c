/*
** FT2 Plugin - PPQ-to-Position Timemap
** Maps DAW PPQ position to FT2 song position for transport sync.
**
** Key formula: 1 FT2 tick = 1/24 PPQ (BPM-independent)
** Derivation: tick = 2.5/bpm sec, beat = 60/bpm sec => tick = 2.5/60 PPQ
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_timemap.h"
#include "ft2_instance.h"

#define TIMEMAP_INITIAL_CAPACITY 1024  /* Initial entry allocation */
#define TIMEMAP_MAX_ENTRIES      65536 /* Limit to prevent runaway */
#define TIMEMAP_MAX_POSITIONS    512   /* Limit positions scanned (infinite Bxx protection) */
#define PPQ_PER_TICK (1.0 / 24.0)      /* BPM-independent: 2.5/60 */

/* ------------------------------------------------------------------------- */
/*                       INITIALIZATION                                      */
/* ------------------------------------------------------------------------- */

void ft2_timemap_init(ft2_timemap_t *timemap)
{
	if (!timemap) return;
	timemap->entries = NULL;
	timemap->count = timemap->capacity = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;
}

void ft2_timemap_free(ft2_timemap_t *timemap)
{
	if (!timemap) return;
	if (timemap->entries) { free(timemap->entries); timemap->entries = NULL; }
	timemap->count = timemap->capacity = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;
}

/* ------------------------------------------------------------------------- */
/*                            HELPERS                                        */
/* ------------------------------------------------------------------------- */

static bool timemap_ensure_capacity(ft2_timemap_t *timemap, uint32_t needed)
{
	if (timemap->capacity >= needed) return true;

	uint32_t newCapacity = timemap->capacity == 0 ? TIMEMAP_INITIAL_CAPACITY : timemap->capacity * 2;
	while (newCapacity < needed) newCapacity *= 2;
	if (newCapacity > TIMEMAP_MAX_ENTRIES) newCapacity = TIMEMAP_MAX_ENTRIES;

	ft2_timemap_entry_t *newEntries = (ft2_timemap_entry_t *)realloc(
		timemap->entries, newCapacity * sizeof(ft2_timemap_entry_t));
	if (!newEntries) return false;

	timemap->entries = newEntries;
	timemap->capacity = newCapacity;
	return true;
}

static bool timemap_add_entry(ft2_timemap_t *timemap, double ppq, uint16_t songPos, uint16_t row,
                              uint8_t loopCounter, uint16_t loopStartRow)
{
	if (timemap->count >= TIMEMAP_MAX_ENTRIES) return false;
	if (!timemap_ensure_capacity(timemap, timemap->count + 1)) return false;

	ft2_timemap_entry_t *entry = &timemap->entries[timemap->count++];
	entry->ppqPosition = ppq;
	entry->songPos = songPos;
	entry->row = row;
	entry->loopCounter = loopCounter;
	entry->loopStartRow = loopStartRow;
	return true;
}

/* ------------------------------------------------------------------------- */
/*                          MAP BUILDING                                     */
/* ------------------------------------------------------------------------- */

/*
** Scans the song and builds the PPQ->position lookup table.
** Processes: Fxx (speed), Bxx (jump), Dxx (break), E6x (loop), EEx (delay).
** BPM (Fxx >= 0x20) is ignored since PPQ timing is BPM-independent.
*/
void ft2_timemap_build(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_timemap_t *timemap = &inst->timemap;
	ft2_song_t *song = &inst->replayer.song;
	ft2_replayer_state_t *rep = &inst->replayer;

	timemap->count = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;

	if (!timemap_ensure_capacity(timemap, TIMEMAP_INITIAL_CAPACITY)) return;

	/* Speed: use lockedSpeed if Fxx disabled, else song's initial speed */
	uint16_t speed = !inst->config.allowFxxSpeedChanges ? inst->config.lockedSpeed
	               : (song->initialSpeed > 0 ? song->initialSpeed : 6);

	double currentPpq = 0.0;
	uint16_t nextRowStart = 0;
	uint32_t positionsScanned = 0;

	bool visitedPositions[256];
	memset(visitedPositions, 0, sizeof(visitedPositions));

	for (int16_t songPos = 0; songPos < song->songLength && positionsScanned < TIMEMAP_MAX_POSITIONS; songPos++)
	{
		positionsScanned++;

		uint8_t patternNum = song->orders[songPos];
		int16_t numRows = rep->patternNumRows[patternNum];
		if (numRows <= 0) numRows = 64;

		uint16_t startRow = nextRowStart;
		nextRowStart = 0;
		if (startRow >= (uint16_t)numRows) startRow = 0;

		bool positionJump = false, patternBreak = false;
		uint16_t jumpPos = 0, breakRow = 0;
		uint16_t loopStartRow = 0;
		uint8_t loopCounter = 0;

		for (uint16_t row = startRow; row < (uint16_t)numRows; row++)
		{
			if (!timemap_add_entry(timemap, currentPpq, (uint16_t)songPos, row, loopCounter, loopStartRow))
				goto done;

			bool patternDelayProcessed = false;
			ft2_note_t *patternPtr = rep->pattern[patternNum];

			if (patternPtr)
			{
				for (int32_t ch = 0; ch < song->numChannels; ch++)
				{
					ft2_note_t *note = &patternPtr[(row * FT2_MAX_CHANNELS) + ch];
					uint8_t efx = note->efx, efxData = note->efxData;

					switch (efx)
					{
						case 0x0F: /* Fxx: speed only (BPM >= 0x20 ignored) */
							if (efxData > 0 && efxData < 0x20 && inst->config.allowFxxSpeedChanges)
								speed = efxData;
							break;

						case 0x0B: /* Bxx: position jump */
							if (!positionJump) { positionJump = true; jumpPos = efxData; }
							break;

						case 0x0D: /* Dxx: pattern break (BCD parameter) */
							if (!patternBreak) { patternBreak = true; breakRow = ((efxData >> 4) * 10) + (efxData & 0x0F); }
							break;

						case 0x0E: /* Exx: extended effects */
						{
							uint8_t efxType = efxData >> 4, efxParam = efxData & 0x0F;
							if (efxType == 0x06) /* E6x: pattern loop */
							{
								if (efxParam == 0)
									loopStartRow = row;
								else if (loopCounter == 0)
									{ loopCounter = efxParam; row = loopStartRow - 1; }
								else if (--loopCounter > 0)
									row = loopStartRow - 1;
							}
							else if (efxType == 0x0E && !patternDelayProcessed && efxParam > 0) /* EEx: delay */
							{
								currentPpq += efxParam * speed * PPQ_PER_TICK;
								patternDelayProcessed = true;
							}
							break;
						}
						default: break;
					}
				}
			}

			currentPpq += speed * PPQ_PER_TICK;

			if (positionJump || patternBreak)
			{
				if (positionJump)
				{
					if (jumpPos < song->songLength)
					{
						if (visitedPositions[jumpPos] && !patternBreak) goto done;
						visitedPositions[songPos] = true;
						songPos = (int16_t)(jumpPos - 1);
					}
					else goto done;
				}
				if (patternBreak) nextRowStart = breakRow;
				break;
			}
		}
	}

done:
	timemap->totalPpq = currentPpq;
	timemap->valid = (timemap->count > 0);
}

void ft2_timemap_invalidate(ft2_instance_t *inst)
{
	if (inst) inst->timemap.valid = false;
}

/* ------------------------------------------------------------------------- */
/*                            LOOKUP                                         */
/* ------------------------------------------------------------------------- */

/* Binary search to find song position for a given PPQ. Rebuilds map if invalid. */
bool ft2_timemap_lookup(ft2_instance_t *inst, double ppqPosition,
                        uint16_t *outSongPos, uint16_t *outRow,
                        uint8_t *outLoopCounter, uint16_t *outLoopStartRow)
{
	if (!inst || !outSongPos || !outRow) return false;

	ft2_timemap_t *timemap = &inst->timemap;

	if (!timemap->valid) { ft2_timemap_build(inst); if (!timemap->valid) return false; }
	if (timemap->count == 0) return false;

	if (ppqPosition < 0.0) ppqPosition = 0.0;
	if (timemap->totalPpq > 0.0 && ppqPosition >= timemap->totalPpq)
		ppqPosition = fmod(ppqPosition, timemap->totalPpq);

	/* Binary search */
	uint32_t left = 0, right = timemap->count - 1, result = 0;
	while (left <= right)
	{
		uint32_t mid = left + (right - left) / 2;
		if (timemap->entries[mid].ppqPosition <= ppqPosition)
		{
			result = mid;
			if (mid == timemap->count - 1) break;
			left = mid + 1;
		}
		else
		{
			if (mid == 0) break;
			right = mid - 1;
		}
	}

	*outSongPos = timemap->entries[result].songPos;
	*outRow = timemap->entries[result].row;
	if (outLoopCounter) *outLoopCounter = timemap->entries[result].loopCounter;
	if (outLoopStartRow) *outLoopStartRow = timemap->entries[result].loopStartRow;
	return true;
}
