/*
** FT2 Plugin - Trim Screen API
** Removes unused patterns/instruments/samples/channels, truncates sample
** data after loop, converts samples to 8-bit.
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

typedef struct ft2_trim_state_t {
	bool removePatt, removeInst, removeSamp;
	bool removeChans, removeSmpDataAfterLoop, convSmpsTo8Bit;
	int64_t xmSize64, xmAfterTrimSize64, spaceSaved64;
} ft2_trim_state_t;

/* Screen visibility */
void drawTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
void showTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
void hideTrimScreen(struct ft2_instance_t *inst);
void toggleTrimScreen(struct ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);

/* Initialization */
void setInitialTrimFlags(struct ft2_instance_t *inst);
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

