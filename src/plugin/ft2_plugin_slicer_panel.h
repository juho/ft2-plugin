/**
 * @file ft2_plugin_slicer_panel.h
 * @brief Sample slicer panel for the sample editor.
 *
 * Provides transient-based sample slicing with automatic detection,
 * manual marker adjustment, preview, and processing to create
 * chromatically-mapped instrument key zones.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Screen visibility */
void showSlicerScreen(struct ft2_instance_t *inst);
void hideSlicerScreen(struct ft2_instance_t *inst);
void drawSlicerScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Controls */
void pbSlicerSlicesUp(struct ft2_instance_t *inst);
void pbSlicerSlicesDown(struct ft2_instance_t *inst);
void pbSlicerSensUp(struct ft2_instance_t *inst);
void pbSlicerSensDown(struct ft2_instance_t *inst);
void pbSlicerRootUp(struct ft2_instance_t *inst);
void pbSlicerRootDown(struct ft2_instance_t *inst);
void pbSlicerDetect(struct ft2_instance_t *inst);
void pbSlicerProcess(struct ft2_instance_t *inst);
void pbSlicerBack(struct ft2_instance_t *inst);

/* Transient detection */
void slicerDetectTransients(struct ft2_instance_t *inst);

/* Marker rendering (called from sample editor) */
void drawSliceMarkers(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Marker interaction */
int8_t slicerHitTestMarker(struct ft2_instance_t *inst, int32_t screenX);
void slicerStartMarkerDrag(struct ft2_instance_t *inst, int8_t markerIdx);
void slicerDragMarker(struct ft2_instance_t *inst, int32_t screenX);
void slicerEndMarkerDrag(struct ft2_instance_t *inst);

/* Slice preview */
void slicerPreviewSlice(struct ft2_instance_t *inst, int32_t screenX);

/* State accessors */
uint8_t getSlicerNumSlices(struct ft2_instance_t *inst);
uint8_t getSlicerSensitivity(struct ft2_instance_t *inst);
uint8_t getSlicerRootNote(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
