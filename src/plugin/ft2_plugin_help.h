/**
 * @file ft2_plugin_help.h
 * @brief Help screen: scrollable formatted text with subject selection.
 *
 * Subjects: Features, Effects, Keybindings, How to use FT2, Plugin.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

#define HELP_LINE_HEIGHT 11
#define HELP_WINDOW_HEIGHT 164
#define HELP_TEXT_BUFFER_W 472

/* Help screen state */
typedef struct help_state_t {
	uint8_t currentSubject;   /* Current help subject index (0-4) */
	int16_t scrollLine;       /* Current scroll position */
} help_state_t;

/* Scrolling */
void helpScrollUp(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void helpScrollDown(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void helpScrollSetPos(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint32_t pos);

/* Visibility */
void showHelpScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideHelpScreen(struct ft2_instance_t *inst);
void exitHelpScreen(struct ft2_instance_t *inst);

/* Drawing */
void drawHelpScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Subject selection (radio button callbacks) */
void rbHelpFeatures(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpEffects(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpKeybindings(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpHowToUseFT2(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpPlugin(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Initialization/cleanup */
void initFTHelp(void);
void windUpFTHelp(void);

#ifdef __cplusplus
}
#endif
