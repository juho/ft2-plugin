/*
** FT2 Plugin - PPQ-to-Position Timemap API
** Maps DAW PPQ position to FT2 song position for transport sync.
**
** Key insight: 1 FT2 tick = 1/24 PPQ (BPM-independent)
** tick = 2.5/bpm sec, beat = 60/bpm sec => tick = 2.5/60 = 1/24 PPQ
** row = speed/24 PPQ
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/* Single entry mapping PPQ to song position */
typedef struct ft2_timemap_entry_t {
	double ppqPosition;    /* Accumulated PPQ at this point */
	uint16_t songPos;      /* Order list position */
	uint16_t row;          /* Row within pattern */
	uint8_t loopCounter;   /* E6x loop iterations remaining (0 = not looping) */
	uint16_t loopStartRow; /* E60 loop start row */
} ft2_timemap_entry_t;

/* PPQ map: dynamic array of PPQ->position mappings */
typedef struct ft2_timemap_t {
	ft2_timemap_entry_t *entries;
	uint32_t count, capacity;
	bool valid;       /* False after song edit */
	double totalPpq;  /* Total song length in PPQ */
} ft2_timemap_t;

/* Initialization */
void ft2_timemap_init(ft2_timemap_t *timemap);
void ft2_timemap_free(ft2_timemap_t *timemap);

/* Map building: scans song, processes Fxx/Bxx/Dxx/E6x/EEx effects */
void ft2_timemap_build(struct ft2_instance_t *inst);
void ft2_timemap_invalidate(struct ft2_instance_t *inst);

/* Lookup: O(log n) binary search, auto-rebuilds if invalid */
bool ft2_timemap_lookup(struct ft2_instance_t *inst, double ppqPosition,
                        uint16_t *outSongPos, uint16_t *outRow,
                        uint8_t *outLoopCounter, uint16_t *outLoopStartRow);

#ifdef __cplusplus
}
#endif

