/**
 * @file ft2_plugin_trim.h
 * @brief Trim screen functionality for the FT2 plugin.
 *
 * Ported from ft2_trim.h - allows trimming unused patterns, instruments,
 * samples, channels, and converting samples to 8-bit.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/* UI functions */
void drawTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
void showTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
void hideTrimScreen(struct ft2_instance_t *inst);
void toggleTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);

/* Initialization */
void setInitialTrimFlags(void);
void resetTrimSizes(struct ft2_instance_t *inst);

/* Checkbox callbacks */
void cbTrimUnusedPatt(struct ft2_instance_t *inst);
void cbTrimUnusedInst(struct ft2_instance_t *inst);
void cbTrimUnusedSamp(struct ft2_instance_t *inst);
void cbTrimUnusedChans(struct ft2_instance_t *inst);
void cbTrimUnusedSmpData(struct ft2_instance_t *inst);
void cbTrimSmpsTo8Bit(struct ft2_instance_t *inst);

/* Button callbacks */
void pbTrimCalc(struct ft2_instance_t *inst);
void pbTrimDoTrim(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif

