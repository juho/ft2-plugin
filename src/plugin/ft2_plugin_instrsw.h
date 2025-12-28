/**
 * @file ft2_plugin_instrsw.h
 * @brief Instrument switcher for the FT2 plugin.
 * 
 * Ported from ft2_pattern_ed.c - handles the instrument list display
 * and sample list on the top right of the main screen.
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

/**
 * Draw the instrument switcher.
 * Shows the list of instruments and samples in the instrument bank.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Show the instrument switcher and all its widgets.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void showInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Hide the instrument switcher and all its widgets.
 * 
 * @param inst The FT2 instance
 */
void hideInstrumentSwitcher(struct ft2_instance_t *inst);

/**
 * Update the instrument switcher display.
 * Called when instruments change or selection moves.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void updateInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Test if mouse click hit the instrument switcher.
 * Handles clicking on instruments/samples in the list (exact match to original).
 * 
 * @param inst The FT2 instance
 * @param mouseX Mouse X position
 * @param mouseY Mouse Y position
 * @return true if the click was handled
 */
bool testInstrSwitcherMouseDown(struct ft2_instance_t *inst, int32_t mouseX, int32_t mouseY);

#ifdef __cplusplus
}
#endif

