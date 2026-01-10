/*
** FT2 Plugin - Sample Effects API
** Wave generation, resonant filters, EQ, amplitude.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Filter types */
#define FILTER_LOWPASS  0
#define FILTER_HIGHPASS 1

/* Sample effects state (per-instance) */
typedef struct smpfx_state_t {
	bool normalization;
	uint8_t lastFilterType;
	int32_t lastLpCutoff, lastHpCutoff, filterResonance;
	int32_t smpCycles, lastWaveLength, lastAmp;
} smpfx_state_t;

/* Undo */
void clearSampleUndo(struct ft2_instance_t *inst);
void fillSampleUndo(struct ft2_instance_t *inst, bool keepSampleMark);
void pbSfxUndo(struct ft2_instance_t *inst);

/* State accessors */
void cbSfxNormalization(struct ft2_instance_t *inst);
bool getSfxNormalization(struct ft2_instance_t *inst);
int32_t getSfxCycles(struct ft2_instance_t *inst);
int32_t getSfxResonance(struct ft2_instance_t *inst);

/* Wave cycle controls */
void pbSfxCyclesUp(struct ft2_instance_t *inst);
void pbSfxCyclesDown(struct ft2_instance_t *inst);

/* Wave generators (show wave panel) */
void pbSfxTriangle(struct ft2_instance_t *inst);
void pbSfxSaw(struct ft2_instance_t *inst);
void pbSfxSine(struct ft2_instance_t *inst);
void pbSfxSquare(struct ft2_instance_t *inst);

/* Filter resonance controls */
void pbSfxResoUp(struct ft2_instance_t *inst);
void pbSfxResoDown(struct ft2_instance_t *inst);

/* Filters (show filter panel) */
void pbSfxLowPass(struct ft2_instance_t *inst);
void pbSfxHighPass(struct ft2_instance_t *inst);
void smpfx_apply_filter(struct ft2_instance_t *inst, int filterType, int32_t cutoff);

/* EQ buttons */
void pbSfxSubBass(struct ft2_instance_t *inst);
void pbSfxAddBass(struct ft2_instance_t *inst);
void pbSfxSubTreble(struct ft2_instance_t *inst);
void pbSfxAddTreble(struct ft2_instance_t *inst);

/* Amplitude */
void pbSfxSetAmp(struct ft2_instance_t *inst);

/* Screen visibility */
void showSampleEffectsScreen(struct ft2_instance_t *inst);
void hideSampleEffectsScreen(struct ft2_instance_t *inst);
void drawSampleEffectsScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

