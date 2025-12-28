/**
 * @file ft2_plugin_bmp.h
 * @brief BMP asset loader for the FT2 plugin - decodes compressed BMPs to usable pixel data.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Holds all decoded bitmap assets for the FT2 UI.
 */
typedef struct ft2_bmp_t
{
	/* 1-bit fonts (0 = transparent, 1 = draw foreground) */
	uint8_t *buttonGfx;
	uint8_t *font1;
	uint8_t *font2;
	uint8_t *font3;
	uint8_t *font4;
	uint8_t *font6;
	uint8_t *font7;
	uint8_t *font8;

	/* 4-bit palette indexed graphics */
	uint8_t *ft2LogoBadges;
	uint8_t *ft2ByBadges;
	uint8_t *radiobuttonGfx;
	uint8_t *checkboxGfx;
	uint8_t *midiLogo;
	uint8_t *nibblesLogo;
	uint8_t *nibblesStages;
	uint8_t *loopPins;
	uint8_t *ft2OldAboutLogo;
	uint8_t *mouseCursors;
	uint8_t *mouseCursorBusyClock;
	uint8_t *mouseCursorBusyGlass;
	uint8_t *whitePianoKeys;
	uint8_t *blackPianoKeys;
	uint8_t *vibratoWaveforms;
	uint8_t *scopeRec;
	uint8_t *scopeMute;

	/* 32-bit ARGB graphics */
	uint32_t *ft2AboutLogo;
} ft2_bmp_t;

/**
 * Load and decode all BMP assets.
 * @param bmp Pointer to the bmp structure to populate.
 * @return true on success, false if any asset failed to load.
 */
bool ft2_bmp_load(ft2_bmp_t *bmp);

/**
 * Free all loaded BMP assets.
 * @param bmp Pointer to the bmp structure to free.
 */
void ft2_bmp_free(ft2_bmp_t *bmp);

/* Font dimensions - must match the original FT2 */
#define FONT1_CHAR_W 8
#define FONT1_CHAR_H 10
#define FONT1_WIDTH 1024

#define FONT2_CHAR_W 16
#define FONT2_CHAR_H 20
#define FONT2_WIDTH 2048

#define FONT3_CHAR_W 4
#define FONT3_CHAR_H 7
#define FONT3_WIDTH 172

#define FONT4_CHAR_W 8
#define FONT4_CHAR_H 8
#define FONT4_WIDTH 624

#define FONT5_CHAR_W 16
#define FONT5_CHAR_H 8
#define FONT5_WIDTH 624

#define FONT6_CHAR_W 7
#define FONT6_CHAR_H 8
#define FONT6_WIDTH 112

#define FONT7_CHAR_W 6
#define FONT7_CHAR_H 7
#define FONT7_WIDTH 140

#define FONT8_CHAR_W 5
#define FONT8_CHAR_H 7
#define FONT8_WIDTH 80

/* About logo dimensions */
#define ABOUT_LOGO_W 449
#define ABOUT_LOGO_H 75
#define ABOUT_OLD_LOGO_W 449
#define ABOUT_OLD_LOGO_H 111

/* Button graphics dimensions */
#define BUTTON_GFX_W 10
#define BUTTON_GFX_H 40

/* Checkbox/Radiobutton bitmap dimensions (height is multiple frames) */
#define CHECKBOX_BMP_W 13
#define CHECKBOX_BMP_H 96
#define RADIOBUTTON_BMP_W 11
#define RADIOBUTTON_BMP_H 33

/* Scope graphics dimensions */
#define SCOPE_MUTE_W 162
#define SCOPE_MUTE_H 31
#define SCOPE_REC_W 10
#define SCOPE_REC_H 11

/* Piano key dimensions */
#define WHITE_KEY_W 10
#define WHITE_KEY_H 258
#define BLACK_KEY_W 10
#define BLACK_KEY_H 48

/* Loop pin dimensions */
#define LOOP_PIN_W 16
#define LOOP_PIN_H 391

