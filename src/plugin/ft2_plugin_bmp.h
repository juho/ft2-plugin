/**
 * @file ft2_plugin_bmp.h
 * @brief BMP asset loader - decodes RLE-compressed BMPs embedded in gfxdata/.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Decoded bitmap assets. Three storage formats:
 *   1-bit: font masks (0=transparent, 1=foreground color)
 *   4-bit: palette indices for themed UI elements
 *   32-bit: full ARGB for non-themed graphics
 */
typedef struct ft2_bmp_t
{
	/* 1-bit font masks: rendered with current foreground color */
	uint8_t *buttonGfx;    /* Button face character glyphs */
	uint8_t *font1;        /* Main UI font 8x10 */
	uint8_t *font2;        /* Large font 16x20 (about screen) */
	uint8_t *font3;        /* Tiny font 4x7 (pattern editor notes) */
	uint8_t *font4;        /* Medium font 8x8 */
	uint8_t *font6;        /* Scope font 7x8 */
	uint8_t *font7;        /* Small font 6x7 */
	uint8_t *font8;        /* Smallest font 5x7 */

	/* 4-bit palette indexed: adapt to user theme */
	uint8_t *ft2LogoBadges;        /* FT2/FT logo variants */
	uint8_t *ft2ByBadges;          /* "by" badge variants */
	uint8_t *radiobuttonGfx;       /* Radio button states */
	uint8_t *checkboxGfx;          /* Checkbox states */
	uint8_t *midiLogo;             /* MIDI config logo */
	uint8_t *nibblesLogo;          /* Nibbles game logo */
	uint8_t *nibblesStages;        /* Nibbles level backgrounds */
	uint8_t *loopPins;             /* Sample loop point markers */
	uint8_t *ft2OldAboutLogo;      /* Classic about screen logo */
	uint8_t *mouseCursors;         /* Standard cursor shapes */
	uint8_t *mouseCursorBusyClock; /* Busy cursor (clock) */
	uint8_t *mouseCursorBusyGlass; /* Busy cursor (hourglass) */
	uint8_t *whitePianoKeys;       /* Instrument editor piano */
	uint8_t *blackPianoKeys;
	uint8_t *vibratoWaveforms;     /* Vibrato type selector */
	uint8_t *scopeRec;             /* Scope record indicator */
	uint8_t *scopeMute;            /* Scope mute overlay */

	/* 32-bit ARGB: full color, not themed */
	uint32_t *ft2AboutLogo;        /* New about screen logo */
} ft2_bmp_t;

/* Decode all embedded BMPs into bmp struct. Returns false on allocation failure. */
bool ft2_bmp_load(ft2_bmp_t *bmp);

/* Free all allocated bitmap data. */
void ft2_bmp_free(ft2_bmp_t *bmp);

/*
 * Font dimensions (char width, height, bitmap width).
 * Must match original FT2 bitmaps in gfxdata/.
 */
#define FONT1_CHAR_W 8   /* Main UI font */
#define FONT1_CHAR_H 10
#define FONT1_WIDTH 1024

#define FONT2_CHAR_W 16  /* Large font (about screen) */
#define FONT2_CHAR_H 20
#define FONT2_WIDTH 2048

#define FONT3_CHAR_W 4   /* Tiny font (pattern editor) */
#define FONT3_CHAR_H 7
#define FONT3_WIDTH 172

#define FONT4_CHAR_W 8   /* Medium font */
#define FONT4_CHAR_H 8
#define FONT4_WIDTH 624

#define FONT5_CHAR_W 16  /* Double-width variant */
#define FONT5_CHAR_H 8
#define FONT5_WIDTH 624

#define FONT6_CHAR_W 7   /* Scope channel names */
#define FONT6_CHAR_H 8
#define FONT6_WIDTH 112

#define FONT7_CHAR_W 6   /* Small font */
#define FONT7_CHAR_H 7
#define FONT7_WIDTH 140

#define FONT8_CHAR_W 5   /* Smallest font */
#define FONT8_CHAR_H 7
#define FONT8_WIDTH 80

/* About screen logos */
#define ABOUT_LOGO_W 449
#define ABOUT_LOGO_H 75
#define ABOUT_OLD_LOGO_W 449
#define ABOUT_OLD_LOGO_H 111

/* Button face graphics (arrow glyphs, etc.) */
#define BUTTON_GFX_W 10
#define BUTTON_GFX_H 40

/* Widget state graphics (height = frames stacked vertically) */
#define CHECKBOX_BMP_W 13
#define CHECKBOX_BMP_H 96      /* 8 states * 12px each */
#define RADIOBUTTON_BMP_W 11
#define RADIOBUTTON_BMP_H 33   /* 3 states * 11px each */

/* Scope overlay graphics */
#define SCOPE_MUTE_W 162
#define SCOPE_MUTE_H 31
#define SCOPE_REC_W 10
#define SCOPE_REC_H 11

/* Instrument editor piano keyboard */
#define WHITE_KEY_W 10
#define WHITE_KEY_H 258        /* 7 octaves + states */
#define BLACK_KEY_W 10
#define BLACK_KEY_H 48

/* Sample editor loop markers */
#define LOOP_PIN_W 16
#define LOOP_PIN_H 391

