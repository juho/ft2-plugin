#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/**
 * Clear sample undo buffer.
 */
void clearSampleUndo(struct ft2_instance_t *inst);

/**
 * Fill sample undo buffer with current sample data.
 * @param inst Plugin instance
 * @param keepSampleMark If true, restore sample mark after undo
 */
void fillSampleUndo(struct ft2_instance_t *inst, bool keepSampleMark);

/**
 * Normalization checkbox callback.
 */
void cbSfxNormalization(struct ft2_instance_t *inst);

/**
 * Get current normalization state.
 */
bool getSfxNormalization(struct ft2_instance_t *inst);

/**
 * Cycles up/down callbacks.
 */
void pbSfxCyclesUp(struct ft2_instance_t *inst);
void pbSfxCyclesDown(struct ft2_instance_t *inst);

/**
 * Wave generator callbacks.
 */
void pbSfxTriangle(struct ft2_instance_t *inst);
void pbSfxSaw(struct ft2_instance_t *inst);
void pbSfxSine(struct ft2_instance_t *inst);
void pbSfxSquare(struct ft2_instance_t *inst);

/**
 * Resonance up/down callbacks.
 */
void pbSfxResoUp(struct ft2_instance_t *inst);
void pbSfxResoDown(struct ft2_instance_t *inst);

/**
 * Filter callbacks.
 */
void pbSfxLowPass(struct ft2_instance_t *inst);
void pbSfxHighPass(struct ft2_instance_t *inst);

/**
 * Bass/Treble EQ callbacks.
 */
void pbSfxSubBass(struct ft2_instance_t *inst);
void pbSfxAddBass(struct ft2_instance_t *inst);
void pbSfxSubTreble(struct ft2_instance_t *inst);
void pbSfxAddTreble(struct ft2_instance_t *inst);

/**
 * Amplitude callback.
 */
void pbSfxSetAmp(struct ft2_instance_t *inst);

/**
 * Undo callback.
 */
void pbSfxUndo(struct ft2_instance_t *inst);

/**
 * Show/hide sample effects screen.
 */
void showSampleEffectsScreen(struct ft2_instance_t *inst);
void hideSampleEffectsScreen(struct ft2_instance_t *inst);

/**
 * Draw sample effects panel UI elements.
 */
void drawSampleEffectsScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Get current cycles value (for display).
 */
int32_t getSfxCycles(struct ft2_instance_t *inst);

/**
 * Get current filter resonance value (for display).
 */
int32_t getSfxResonance(struct ft2_instance_t *inst);

/**
 * Apply filter to current sample (called from filter panel).
 * @param inst Plugin instance
 * @param filterType 0 = lowpass, 1 = highpass
 * @param cutoff Cutoff frequency in Hz
 */
void smpfx_apply_filter(struct ft2_instance_t *inst, int filterType, int32_t cutoff);

