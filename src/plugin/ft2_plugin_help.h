/**
 * @file ft2_plugin_help.h
 * @brief Exact port of help screen from ft2_help.h
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

/* Help screen constants */
#define HELP_LINE_HEIGHT 11
#define HELP_WINDOW_HEIGHT 164
#define HELP_TEXT_BUFFER_W 472

/* Scroll functions */
void helpScrollUp(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void helpScrollDown(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void helpScrollSetPos(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint32_t pos);

/* Show/Hide help screen */
void showHelpScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideHelpScreen(struct ft2_instance_t *inst);
void exitHelpScreen(struct ft2_instance_t *inst);

/* Per-frame drawing */
void drawHelpScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Subject selection (radio button callbacks) */
void rbHelpFeatures(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpEffects(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpKeybindings(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpHowToUseFT2(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpFAQ(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void rbHelpKnownBugs(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Initialization */
void initFTHelp(void);
void windUpFTHelp(void);

#ifdef __cplusplus
}
#endif
