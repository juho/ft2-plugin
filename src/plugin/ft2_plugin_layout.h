/**
 * @file ft2_plugin_layout.h
 * @brief Main screen layout functions for the FT2 plugin.
 * 
 * Ported from ft2_gui.c - handles drawing the main FT2 screen layout including:
 * - Position editor
 * - Instrument switcher
 * - Scopes
 * - Song/pattern controls
 * - Menu buttons
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
 * Draw the top-left main screen area.
 * Includes: position editor, logo, left menu, song/pattern controls, status bar.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 * @param restoreScreens Whether to restore previous overlay screen state
 */
void drawTopLeftMainScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, bool restoreScreens);

/**
 * Draw the top-right main screen area.
 * Includes: right menu buttons, instrument switcher, song name.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawTopRightMainScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw the complete top screen.
 * Calls drawTopLeftMainScreen and drawTopRightMainScreen.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 * @param restoreScreens Whether to restore previous overlay screen state
 */
void drawTopScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, bool restoreScreens);

/**
 * Draw the bottom screen based on current state.
 * Shows pattern editor, instrument editor, or sample editor.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawBottomScreen(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw the complete GUI layout.
 * Called once at startup and when screens change.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawGUILayout(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw position editor numbers.
 * Shows current song position and pattern numbers.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawPosEdNums(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw song length.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawSongLength(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw song loop start.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawSongLoopStart(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw song BPM.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawSongBPM(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw song speed.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawSongSpeed(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw global volume.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawGlobalVol(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw edit pattern number.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawEditPattern(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw pattern length.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawPatternLength(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw edit row add value.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawIDAdd(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw current octave.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawOctave(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw playback time.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawPlaybackTime(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

/**
 * Draw song name.
 * 
 * @param inst The FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void drawSongName(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp);

#ifdef __cplusplus
}
#endif

