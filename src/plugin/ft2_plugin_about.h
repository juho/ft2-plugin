/**
 * @file ft2_plugin_about.h
 * @brief About screen with animated 3D starfield and credits.
 *
 * Two display modes:
 *   New mode: float-precision starfield, waving FT2 logo, blue-tinted stars
 *   Classic mode: integer starfield (galaxy/spiral/stars), original FT2 logo
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_widgets_t;
struct ft2_instance_t;

/* Starfield constants */
#define ABOUT_OLD_NUM_STARS 1000
#define ABOUT_NUM_STARS 1500
#define ABOUT_SINUS_PHASES 1024

/* Old starfield types (classic FT2) */
typedef struct { int16_t x, y, z; } about_old_vector_t;
typedef struct { uint16_t x, y, z; } about_old_rotate_t;
typedef struct { about_old_vector_t x, y, z; } about_old_matrix_t;

/* New starfield types */
typedef struct { float x, y, z; } about_vector_t;
typedef struct { about_vector_t x, y, z; } about_matrix_t;

/* About screen state */
typedef struct about_state_t {
	bool initialized;
	bool useNewMode;
	uint32_t randSeed;
	uint32_t sinp1, sinp2;
	int16_t zSpeed;
	int16_t textPosX[5], textPosY[5];
	int32_t lastStarScreenPos[ABOUT_OLD_NUM_STARS];
	about_old_vector_t oldStarPoints[ABOUT_OLD_NUM_STARS];
	about_old_rotate_t oldStarRotation;
	about_old_matrix_t oldStarMatrix;
	about_vector_t starPoints[ABOUT_NUM_STARS];
	about_vector_t starRotation;
	about_matrix_t starMatrix;
} about_state_t;

/* Initialize static lookup tables. Call once at startup. */
void ft2_about_init(void);

/* Initialize per-instance about state */
void ft2_about_init_state(about_state_t *state);

/* Display the about screen. Draws framework and initializes starfield pattern. */
void ft2_about_show(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Render one animation frame (starfield rotation, logo wave). Call per UI frame. */
void ft2_about_render_frame(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Wrapper for render_frame (legacy interface). */
void ft2_about_draw(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Set display mode: true = new (waving logo), false = classic FT2. */
void ft2_about_set_mode(struct ft2_instance_t *inst, bool newMode);

/* Get current display mode. */
bool ft2_about_get_mode(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
