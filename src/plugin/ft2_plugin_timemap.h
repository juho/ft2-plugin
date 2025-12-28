/**
 * @file ft2_plugin_timemap.h
 * @brief PPQ-to-position mapping for DAW position sync.
 *
 * Builds a lookup table that maps PPQ (quarter notes) to FT2 song position,
 * enabling the plugin to sync its playhead with the DAW's transport.
 *
 * Key insight: FT2 tick timing in PPQ is BPM-independent.
 * 1 tick = 2.5/bpm seconds, 1 beat = 60/bpm seconds
 * Therefore: 1 tick = 2.5/60 = 1/24 PPQ (constant, BPM cancels!)
 * 1 row = speed/24 PPQ
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/**
 * Single entry in the PPQ map.
 * Maps a PPQ position to a specific (songPos, row) coordinate,
 * including pattern loop state for accurate seek behavior.
 */
typedef struct ft2_timemap_entry_t
{
	double ppqPosition;   /* Accumulated PPQ at this point */
	uint16_t songPos;     /* Order list position */
	uint16_t row;         /* Row within pattern */
	uint8_t loopCounter;  /* Remaining E6x loop iterations (0 = not in loop) */
	uint16_t loopStartRow;/* E60 loop start row */
} ft2_timemap_entry_t;

/**
 * PPQ map structure.
 * Contains a dynamically allocated array of PPQ-to-position mappings.
 */
typedef struct ft2_timemap_t
{
	ft2_timemap_entry_t *entries;
	uint32_t count;
	uint32_t capacity;
	bool valid;           /* False if song was edited since last build */
	double totalPpq;      /* Total song length in PPQ */
} ft2_timemap_t;

/**
 * Initialize the PPQ map structure.
 * @param timemap Pointer to PPQ map to initialize
 */
void ft2_timemap_init(ft2_timemap_t *timemap);

/**
 * Free the PPQ map's allocated memory.
 * @param timemap Pointer to PPQ map to free
 */
void ft2_timemap_free(ft2_timemap_t *timemap);

/**
 * Build/rebuild the PPQ map by scanning the song.
 * Uses PPQ-based timing (BPM-independent: 1 tick = 1/24 PPQ).
 * Processes speed changes, jumps, breaks, and pattern loops.
 * @param inst FT2 instance containing the song to scan
 */
void ft2_timemap_build(struct ft2_instance_t *inst);

/**
 * Invalidate the PPQ map (mark as needing rebuild).
 * Call this when the song is edited.
 * @param inst FT2 instance
 */
void ft2_timemap_invalidate(struct ft2_instance_t *inst);

/**
 * Look up the song position for a given PPQ position.
 * Uses binary search for O(log n) lookup.
 * Automatically rebuilds the map if invalid.
 * @param inst FT2 instance
 * @param ppqPosition PPQ position from DAW (quarter notes from song start)
 * @param outSongPos Output: order list position
 * @param outRow Output: row within pattern
 * @param outLoopCounter Output: remaining E6x loop iterations (can be NULL)
 * @param outLoopStartRow Output: E60 loop start row (can be NULL)
 * @return true if lookup succeeded, false if map invalid or position out of range
 */
bool ft2_timemap_lookup(struct ft2_instance_t *inst, double ppqPosition,
                        uint16_t *outSongPos, uint16_t *outRow,
                        uint8_t *outLoopCounter, uint16_t *outLoopStartRow);

#ifdef __cplusplus
}
#endif

