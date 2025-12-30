/**
 * @file ft2_plugin_video.h
 * @brief Exact port of FT2 video/drawing primitives for plugin use
 * 
 * This is a direct port of ft2_video.c and ft2_gui.c drawing functions,
 * modified only to use instance-based state instead of globals.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Screen dimensions - exact FT2 values */
#define SCREEN_W 632
#define SCREEN_H 400

/* Palette indices - exact FT2 order */
enum
{
	PAL_BCKGRND = 0,
	PAL_PATTEXT = 1,
	PAL_BLCKMRK = 2,
	PAL_BLCKTXT = 3,
	PAL_DESKTOP = 4,
	PAL_FORGRND = 5,
	PAL_BUTTONS = 6,
	PAL_BTNTEXT = 7,
	PAL_DSKTOP2 = 8,
	PAL_DSKTOP1 = 9,
	PAL_BUTTON2 = 10,
	PAL_BUTTON1 = 11,
	PAL_MOUSEPT = 12,
	PAL_PIANOXOR1 = 13,
	PAL_PIANOXOR2 = 14,
	PAL_PIANOXOR3 = 15,
	PAL_LOOPPIN = 16,
	PAL_TEXTMRK = 17,
	PAL_BOXSLCT = 18,
	PAL_CUSTOM = 19,
	PAL_NUM = 20
};

#define PAL_TRANSPR 127

/* RGB macros - exact FT2 */
#define RGB32_R(x) (((x) >> 16) & 0xFF)
#define RGB32_G(x) (((x) >> 8) & 0xFF)
#define RGB32_B(x) ((x) & 0xFF)
#define RGB32(r, g, b) (((r) << 16) | ((g) << 8) | (b))

/* Math macros - exact FT2 */
#define SGN(x) (((x) >= 0) ? 1 : -1)
#define ABS(a) (((a) < 0) ? -(a) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* Framework types - exact FT2 */
#define FRAMEWORK_TYPE1 0
#define FRAMEWORK_TYPE2 1

/* Font dimensions - exact FT2 */
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

/**
 * Video state for plugin instance
 */
typedef struct ft2_video_t
{
	uint32_t *frameBuffer;   /* Back buffer - UI draws here */
	uint32_t *displayBuffer; /* Front buffer - OpenGL reads from here */
	uint32_t palette[PAL_NUM];
} ft2_video_t;

/* Forward declaration for bitmap structure */
struct ft2_bmp_t;

/**
 * Initialize video state
 * @param video Video state to initialize
 * @return true on success
 */
bool ft2_video_init(ft2_video_t *video);

/**
 * Free video resources
 * @param video Video state to free
 */
void ft2_video_free(ft2_video_t *video);

/**
 * Swap front and back buffers (copy back to front for double buffering)
 * @param video Video state
 */
void ft2_video_swap_buffers(ft2_video_t *video);

/**
 * Set default FT2 palette
 * @param video Video state
 */
void ft2_video_set_default_palette(ft2_video_t *video);

/* ========== EXACT PORTS OF FT2 DRAWING PRIMITIVES ========== */

/* Line drawing - exact ft2_gui.c ports */
void hLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex);
void vLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex);
void hLineDouble(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex);
void vLineDouble(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex);
void line(ft2_video_t *video, int16_t x1, int16_t x2, int16_t y1, int16_t y2, uint8_t paletteIndex);

/* Rectangle drawing - exact ft2_gui.c ports */
void clearRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h);
void fillRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex);
void drawFramework(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t type);

/* Blit functions - exact ft2_gui.c ports */
void blit32(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint32_t *srcPtr, uint16_t w, uint16_t h);
void blit(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h);
void blitClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX);
void blitFast(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h);
void blitFastClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX);

/* Text width functions - exact ft2_gui.c ports */
uint8_t charWidth(char ch);
uint8_t charWidth16(char ch);
uint16_t textWidth(const char *textPtr);
uint16_t textNWidth(const char *textPtr, int32_t length);
uint16_t textWidth16(const char *textPtr);

/* Character output - exact ft2_gui.c ports */
void charOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr);
void charOutBg(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr);
void charOutShadow(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr);
void charOutClipX(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, uint16_t clipX);
void charOutOutlined(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, char chr);
void bigCharOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr);

/* Text output - exact ft2_gui.c ports */
void textOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr);
void textOutBorder(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t borderPaletteIndex, const char *textPtr);
void textOutFixed(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t fgPalette, uint8_t bgPalette, const char *textPtr);
void textOutShadow(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr);
void textOutClipX(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX);
void bigTextOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr);
void bigTextOutShadow(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr);

/* Tiny text - exact ft2_gui.c ports */
void textOutTiny(ft2_video_t *video, const struct ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str, uint32_t color);
void textOutTinyOutline(ft2_video_t *video, const struct ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str);

/* Hex output - exact ft2_gui.c ports */
void hexOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint32_t val, uint8_t numDigits);
void hexOutBg(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint32_t val, uint8_t numDigits);
void hexOutShadow(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, uint32_t val, uint8_t numDigits);

/* Pattern editor style hex output - uses font4 (smaller pattern font) */
void pattTwoHexOut(ft2_video_t *video, const struct ft2_bmp_t *bmp, uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color);

#ifdef __cplusplus
}
#endif
