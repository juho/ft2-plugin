/**
 * @file ft2_plugin_nibbles.h
 * @brief Nibbles snake game for the FT2 plugin.
 *
 * Port of the FT2 easter egg game. This is a complete port
 * matching the standalone behavior exactly.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Speed table - ticks at 70Hz: 12=Novice, 8=Average, 6=Pro, 4=Triton */
extern const uint8_t nibblesSpeedTable[4];

#define NIBBLES_MAX_LEVEL 30
#define NIBBLES_SCREEN_W 51
#define NIBBLES_SCREEN_H 23
#define NIBBLES_STAGES_BMP_WIDTH 530

/**
 * Initialize nibbles state to defaults.
 * @param inst FT2 instance
 */
void ft2_nibbles_init(struct ft2_instance_t *inst);

/**
 * Load default high scores.
 * @param inst FT2 instance
 */
void ft2_nibbles_load_default_highscores(struct ft2_instance_t *inst);

/**
 * Show the nibbles screen.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_show(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Hide the nibbles screen.
 * @param inst FT2 instance
 */
void ft2_nibbles_hide(struct ft2_instance_t *inst);

/**
 * Exit nibbles and return to top screen.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_exit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Start a new game.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_play(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Display high scores.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_show_highscores(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Display help screen.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_show_help(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Redraw the game screen (used when grid setting changes, etc.)
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_redraw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Game tick - called at ~60Hz.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_nibbles_tick(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Add a direction to the player's input buffer.
 * @param inst FT2 instance
 * @param playerNum Player number (0 or 1)
 * @param direction Direction (0=right, 1=up, 2=left, 3=down)
 */
void ft2_nibbles_add_input(struct ft2_instance_t *inst, int16_t playerNum, uint8_t direction);

/**
 * Handle key input during nibbles play.
 * @param inst FT2 instance
 * @param keyCode Key code
 * @return true if key was handled, false otherwise
 */
bool ft2_nibbles_handle_key(struct ft2_instance_t *inst, int32_t keyCode);

/**
 * Test for cheat codes (SHIFT+CTRL+ALT + code).
 * @param inst FT2 instance
 * @param keyCode Key code
 * @param shiftPressed Whether shift is pressed
 * @param ctrlPressed Whether ctrl is pressed
 * @param altPressed Whether alt is pressed
 * @param video Video context
 * @param bmp Bitmap assets
 * @return true if cheat code was handled, false otherwise
 */
bool ft2_nibbles_test_cheat(struct ft2_instance_t *inst, int32_t keyCode,
	bool shiftPressed, bool ctrlPressed, bool altPressed,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Callback functions for buttons/checkboxes/radiobuttons */
void pbNibblesPlay(struct ft2_instance_t *inst);
void pbNibblesHelp(struct ft2_instance_t *inst);
void pbNibblesHighScores(struct ft2_instance_t *inst);
void pbNibblesExit(struct ft2_instance_t *inst);

void rbNibbles1Player(struct ft2_instance_t *inst);
void rbNibbles2Players(struct ft2_instance_t *inst);
void rbNibblesNovice(struct ft2_instance_t *inst);
void rbNibblesAverage(struct ft2_instance_t *inst);
void rbNibblesPro(struct ft2_instance_t *inst);
void rbNibblesTriton(struct ft2_instance_t *inst);

void cbNibblesSurround(struct ft2_instance_t *inst);
void cbNibblesGrid(struct ft2_instance_t *inst);
void cbNibblesWrap(struct ft2_instance_t *inst);

