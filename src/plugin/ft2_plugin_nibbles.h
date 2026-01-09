/**
 * @file ft2_plugin_nibbles.h
 * @brief Nibbles snake game (FT2 easter egg).
 *
 * Two-player snake with 30 levels, high scores, wrap/surround/grid modes.
 * P1: arrows, P2: WASD. Cheats: Shift+Ctrl+Alt + "skip" or "triton".
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

extern const uint8_t nibblesSpeedTable[4]; /* Frame delay: 12/8/6/4 at 70Hz */

#define NIBBLES_MAX_LEVEL 30
#define NIBBLES_SCREEN_W 51
#define NIBBLES_SCREEN_H 23
#define NIBBLES_STAGES_BMP_WIDTH 530

/* Init/state */
void ft2_nibbles_init(struct ft2_instance_t *inst);
void ft2_nibbles_load_default_highscores(struct ft2_instance_t *inst);

/* Screen management */
void ft2_nibbles_show(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_nibbles_hide(struct ft2_instance_t *inst);
void ft2_nibbles_exit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Game control */
void ft2_nibbles_play(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_nibbles_tick(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_nibbles_redraw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Overlays */
void ft2_nibbles_show_highscores(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_nibbles_show_help(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Input: direction 0=right, 1=up, 2=left, 3=down */
void ft2_nibbles_add_input(struct ft2_instance_t *inst, int16_t playerNum, uint8_t direction);
bool ft2_nibbles_handle_key(struct ft2_instance_t *inst, int32_t keyCode);
bool ft2_nibbles_test_cheat(struct ft2_instance_t *inst, int32_t keyCode,
	bool shiftPressed, bool ctrlPressed, bool altPressed,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Widget callbacks */
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

