/**
 * @file ft2_plugin_timemap.c
 * @brief PPQ-to-position mapping implementation for DAW position sync.
 *
 * Uses PPQ (quarter notes) instead of seconds for BPM-independent timing.
 * Key formula: 1 FT2 tick = 1/24 PPQ (constant, derived from 2.5/60).
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_timemap.h"
#include "ft2_instance.h"

/* Initial capacity for PPQ map entries */
#define TIMEMAP_INITIAL_CAPACITY 1024

/* Maximum entries to prevent runaway in songs with infinite loops */
#define TIMEMAP_MAX_ENTRIES 65536

/* Maximum song positions to scan (prevent infinite Bxx loops) */
#define TIMEMAP_MAX_POSITIONS 512

/* PPQ per FT2 tick: 2.5/60 = 1/24
 * This is BPM-independent because:
 * - 1 tick = 2.5/bpm seconds
 * - 1 beat = 60/bpm seconds
 * - Therefore: 1 tick = 2.5/60 = 1/24 beats = 1/24 PPQ
 */
#define PPQ_PER_TICK (1.0 / 24.0)

void ft2_timemap_init(ft2_timemap_t *timemap)
{
	if (timemap == NULL)
		return;

	timemap->entries = NULL;
	timemap->count = 0;
	timemap->capacity = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;
}

void ft2_timemap_free(ft2_timemap_t *timemap)
{
	if (timemap == NULL)
		return;

	if (timemap->entries != NULL)
	{
		free(timemap->entries);
		timemap->entries = NULL;
	}

	timemap->count = 0;
	timemap->capacity = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;
}

static bool timemap_ensure_capacity(ft2_timemap_t *timemap, uint32_t needed)
{
	if (timemap->capacity >= needed)
		return true;

	uint32_t newCapacity = timemap->capacity == 0 ? TIMEMAP_INITIAL_CAPACITY : timemap->capacity * 2;
	while (newCapacity < needed)
		newCapacity *= 2;

	if (newCapacity > TIMEMAP_MAX_ENTRIES)
		newCapacity = TIMEMAP_MAX_ENTRIES;

	ft2_timemap_entry_t *newEntries = (ft2_timemap_entry_t *)realloc(
		timemap->entries, newCapacity * sizeof(ft2_timemap_entry_t));

	if (newEntries == NULL)
		return false;

	timemap->entries = newEntries;
	timemap->capacity = newCapacity;
	return true;
}

static bool timemap_add_entry(ft2_timemap_t *timemap, double ppq, uint16_t songPos, uint16_t row,
                              uint8_t loopCounter, uint16_t loopStartRow)
{
	if (timemap->count >= TIMEMAP_MAX_ENTRIES)
		return false;

	if (!timemap_ensure_capacity(timemap, timemap->count + 1))
		return false;

	ft2_timemap_entry_t *entry = &timemap->entries[timemap->count];
	entry->ppqPosition = ppq;
	entry->songPos = songPos;
	entry->row = row;
	entry->loopCounter = loopCounter;
	entry->loopStartRow = loopStartRow;
	timemap->count++;

	return true;
}

void ft2_timemap_build(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_timemap_t *timemap = &inst->timemap;
	ft2_song_t *song = &inst->replayer.song;
	ft2_replayer_state_t *rep = &inst->replayer;

	/* Reset the PPQ map */
	timemap->count = 0;
	timemap->valid = false;
	timemap->totalPpq = 0.0;

	/* Ensure we have capacity */
	if (!timemap_ensure_capacity(timemap, TIMEMAP_INITIAL_CAPACITY))
		return;

	/* Initialize speed (use lockedSpeed if Fxx changes disabled, else use song's initial speed) */
	uint16_t speed;
	if (!inst->config.allowFxxSpeedChanges)
		speed = inst->config.lockedSpeed;
	else
		speed = song->initialSpeed > 0 ? song->initialSpeed : 6;

	double currentPpq = 0.0;
	uint16_t nextRowStart = 0;  /* For pattern break (Dxx) */
	uint32_t positionsScanned = 0;

	/* Track visited positions to detect infinite loops */
	bool visitedPositions[256];
	memset(visitedPositions, 0, sizeof(visitedPositions));

	/* Scan through the song */
	for (int16_t songPos = 0; songPos < song->songLength && positionsScanned < TIMEMAP_MAX_POSITIONS; songPos++)
	{
		positionsScanned++;

		uint8_t patternNum = song->orders[songPos];
		int16_t numRows = rep->patternNumRows[patternNum];
		if (numRows <= 0)
			numRows = 64;

		/* Start row (may be set by previous pattern's Dxx) */
		uint16_t startRow = nextRowStart;
		nextRowStart = 0;

		if (startRow >= (uint16_t)numRows)
			startRow = 0;

		bool positionJump = false;
		bool patternBreak = false;
		uint16_t jumpPos = 0;
		uint16_t breakRow = 0;

		/* E6x pattern loop tracking (reset per pattern) */
		uint16_t loopStartRow = 0;
		uint8_t loopCounter = 0;

		for (uint16_t row = startRow; row < (uint16_t)numRows; row++)
		{
			/* Add entry for this position (with current loop state) */
			if (!timemap_add_entry(timemap, currentPpq, (uint16_t)songPos, row, loopCounter, loopStartRow))
				goto done;

			/* Track if pattern delay already processed this row (only first EEx counts) */
			bool patternDelayProcessed = false;

			/* Scan all channels for timing-relevant effects */
			ft2_note_t *patternPtr = rep->pattern[patternNum];
			if (patternPtr != NULL)
			{
				for (int32_t ch = 0; ch < song->numChannels; ch++)
				{
					ft2_note_t *note = &patternPtr[(row * FT2_MAX_CHANNELS) + ch];
					uint8_t efx = note->efx;
					uint8_t efxData = note->efxData;

					switch (efx)
					{
						case 0x0F: /* Fxx - Set speed (BPM changes don't affect PPQ timing) */
							if (efxData > 0 && efxData < 0x20 && inst->config.allowFxxSpeedChanges)
								speed = efxData;
							/* BPM changes (efxData >= 0x20) don't affect PPQ timing at all */
							break;

						case 0x0B: /* Bxx - Position jump */
							if (!positionJump)
							{
								positionJump = true;
								jumpPos = efxData;
							}
							break;

						case 0x0D: /* Dxx - Pattern break */
							if (!patternBreak)
							{
								patternBreak = true;
								/* Dxx param is BCD: high nibble * 10 + low nibble */
								breakRow = ((efxData >> 4) * 10) + (efxData & 0x0F);
							}
							break;

						case 0x0E: /* Exx extended effects */
						{
							uint8_t efxType = efxData >> 4;
							uint8_t efxParam = efxData & 0x0F;

							if (efxType == 0x06) /* E6x - Pattern loop */
							{
								if (efxParam == 0)
								{
									loopStartRow = row;  /* E60: set loop point */
								}
								else
								{
									if (loopCounter == 0)
									{
										loopCounter = efxParam;  /* First hit: set counter */
										row = loopStartRow - 1;  /* Jump back (-1 because loop increments) */
									}
									else if (--loopCounter > 0)
									{
										row = loopStartRow - 1;  /* Still looping */
									}
									/* else: counter exhausted, continue normally */
								}
							}
							else if (efxType == 0x0E && !patternDelayProcessed) /* EEx - Pattern delay */
							{
								if (efxParam > 0)
								{
									/* Pattern delay adds extra ticks: each tick = 1/24 PPQ */
									currentPpq += efxParam * speed * PPQ_PER_TICK;
									patternDelayProcessed = true;
								}
							}
							break;
						}

						default:
							break;
					}
				}
			}

			/* Add PPQ for this row: speed ticks, each tick = 1/24 PPQ */
			currentPpq += speed * PPQ_PER_TICK;

			/* Handle position jump / pattern break */
			if (positionJump || patternBreak)
			{
				if (positionJump)
				{
					/* Bxx: jump to position */
					if (jumpPos < song->songLength)
					{
						/* Check for infinite loop */
						if (visitedPositions[jumpPos] && !patternBreak)
						{
							/* We've been here before without a break - likely end of song loop */
							goto done;
						}
						visitedPositions[songPos] = true;
						songPos = (int16_t)(jumpPos - 1); /* -1 because loop will increment */
					}
					else
					{
						/* Invalid jump position - end scan */
						goto done;
					}
				}

				if (patternBreak)
				{
					nextRowStart = breakRow;
				}

				break; /* Exit row loop, move to next pattern */
			}
		}
	}

done:
	timemap->totalPpq = currentPpq;
	timemap->valid = (timemap->count > 0);
}

void ft2_timemap_invalidate(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->timemap.valid = false;
}

bool ft2_timemap_lookup(ft2_instance_t *inst, double ppqPosition,
                        uint16_t *outSongPos, uint16_t *outRow,
                        uint8_t *outLoopCounter, uint16_t *outLoopStartRow)
{
	if (inst == NULL || outSongPos == NULL || outRow == NULL)
		return false;

	ft2_timemap_t *timemap = &inst->timemap;

	/* Rebuild if invalid */
	if (!timemap->valid)
	{
		ft2_timemap_build(inst);
		if (!timemap->valid)
			return false;
	}

	if (timemap->count == 0)
		return false;

	/* Clamp PPQ to valid range */
	if (ppqPosition < 0.0)
		ppqPosition = 0.0;

	/* Handle song looping via modulo - wraps position to within one song iteration */
	if (timemap->totalPpq > 0.0 && ppqPosition >= timemap->totalPpq)
		ppqPosition = fmod(ppqPosition, timemap->totalPpq);

	/* Binary search for the entry */
	uint32_t left = 0;
	uint32_t right = timemap->count - 1;
	uint32_t result = 0;

	while (left <= right)
	{
		uint32_t mid = left + (right - left) / 2;

		if (timemap->entries[mid].ppqPosition <= ppqPosition)
		{
			result = mid;
			if (mid == timemap->count - 1)
				break;
			left = mid + 1;
		}
		else
		{
			if (mid == 0)
				break;
			right = mid - 1;
		}
	}

	*outSongPos = timemap->entries[result].songPos;
	*outRow = timemap->entries[result].row;

	if (outLoopCounter != NULL)
		*outLoopCounter = timemap->entries[result].loopCounter;
	if (outLoopStartRow != NULL)
		*outLoopStartRow = timemap->entries[result].loopStartRow;

	return true;
}
