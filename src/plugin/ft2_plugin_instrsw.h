/**
 * @file ft2_plugin_instrsw.h
 * @brief Instrument/sample list panel (top-right of main screen).
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

/* Draw framework and list content */
void drawInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/* Show panel and widgets */
void showInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/* Hide panel and widgets */
void hideInstrumentSwitcher(struct ft2_instance_t *inst);

/* Refresh instrument/sample names and highlights */
void updateInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/* Handle mouse click (returns true if handled) */
bool testInstrSwitcherMouseDown(struct ft2_instance_t *inst, int32_t mouseX, int32_t mouseY);

#ifdef __cplusplus
}
#endif

