/*
** FT2 Plugin - Video/Drawing Primitives
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"

/* Bounds checking macros (assert disabled in Release, explicit checks prevent heap corruption) */
#define VIDEO_CHECK(v) do { if (!(v) || !(v)->frameBuffer) return; } while(0)
#define VIDEO_CHECK_RECT(x, y, w, h) do { \
    if (!(w) || !(h) || (x) >= SCREEN_W || (y) >= SCREEN_H || \
        (uint32_t)(x) + (w) > SCREEN_W || (uint32_t)(y) + (h) > SCREEN_H) return; \
} while(0)

extern const uint8_t font1Widths[128];
extern const uint8_t font2Widths[128];

/* Default FT2 palette (Arctic theme) */
static const uint32_t defaultPalette[PAL_NUM] = {
	0x000000, 0xD2D2D2, 0x5454B2, 0xD2D2D2, 0x6E6E6E, 0xD2D2D2, 0x555555, 0xD2D2D2,
	0x444444, 0x999999, 0x333333, 0x777777, 0xB2B2B2, 0x000000, 0x000000, 0x000000,
	0x4444FF, 0x5555AA, 0x5555AA, 0xFFFFFF
};

/* ------------------------------------------------------------------------- */
/*                            LIFECYCLE                                      */
/* ------------------------------------------------------------------------- */

bool ft2_video_init(ft2_video_t *video)
{
	if (!video) return false;

	video->frameBuffer = (uint32_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint32_t));
	if (!video->frameBuffer) return false;

	video->displayBuffer = (uint32_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint32_t));
	if (!video->displayBuffer) { free(video->frameBuffer); video->frameBuffer = NULL; return false; }

	ft2_video_set_default_palette(video);
	return true;
}

void ft2_video_free(ft2_video_t *video)
{
	if (!video) return;
	if (video->frameBuffer) { free(video->frameBuffer); video->frameBuffer = NULL; }
	if (video->displayBuffer) { free(video->displayBuffer); video->displayBuffer = NULL; }
}

void ft2_video_swap_buffers(ft2_video_t *video)
{
	if (!video || !video->frameBuffer || !video->displayBuffer) return;
	memcpy(video->displayBuffer, video->frameBuffer, SCREEN_W * SCREEN_H * sizeof(uint32_t));
}

void ft2_video_set_default_palette(ft2_video_t *video)
{
	if (!video) return;
	/* High byte stores palette index (needed for XOR cursor drawing) */
	for (int i = 0; i < PAL_NUM; i++)
		video->palette[i] = ((uint32_t)i << 24) | defaultPalette[i];
}

/* ------------------------------------------------------------------------- */
/*                          LINE ROUTINES                                    */
/* ------------------------------------------------------------------------- */

void hLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	VIDEO_CHECK(video);
	if (!w || x >= SCREEN_W || y >= SCREEN_H || (uint32_t)x + w > SCREEN_W) return;

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < w; i++) dstPtr[i] = pixVal;
}

void vLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	VIDEO_CHECK(video);
	if (!h || x >= SCREEN_W || y >= SCREEN_H || (uint32_t)y + h > SCREEN_H) return;

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < h; i++, dstPtr += SCREEN_W) *dstPtr = pixVal;
}

void hLineDouble(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	hLine(video, x, y, w, paletteIndex);
	hLine(video, x, y+1, w, paletteIndex);
}

void vLineDouble(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	vLine(video, x, y, h, paletteIndex);
	vLine(video, x+1, y, h, paletteIndex);
}

/* Bresenham line drawing with per-pixel bounds check */
void line(ft2_video_t *video, int16_t x1, int16_t x2, int16_t y1, int16_t y2, uint8_t paletteIndex)
{
	VIDEO_CHECK(video);

	const int32_t dx = (int32_t)x2 - (int32_t)x1, ax = ABS(dx) * 2, sx = SGN(dx);
	const int32_t dy = (int32_t)y2 - (int32_t)y1, ay = ABS(dy) * 2, sy = SGN(dy);
	int32_t x = x1, y = y1;
	const uint32_t pixVal = video->palette[paletteIndex];

	if (ax > ay)
	{
		int32_t d = ay - (ax / 2);
		while (true)
		{
			if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
				video->frameBuffer[(y * SCREEN_W) + x] = pixVal;
			if (x == x2) break;
			if (d >= 0) { y += sy; d -= ax; }
			x += sx; d += ay;
		}
	}
	else
	{
		int32_t d = ax - (ay / 2);
		while (true)
		{
			if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
				video->frameBuffer[(y * SCREEN_W) + x] = pixVal;
			if (y == y2) break;
			if (d >= 0) { x += sx; d -= ay; }
			y += sy; d += ax;
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                         FILL ROUTINES                                     */
/* ------------------------------------------------------------------------- */

void clearRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h)
{
	VIDEO_CHECK(video);
	VIDEO_CHECK_RECT(xPos, yPos, w, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, dstPtr += SCREEN_W)
		memset(dstPtr, 0, w * sizeof(int32_t));
}

void fillRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex)
{
	VIDEO_CHECK(video);
	VIDEO_CHECK_RECT(xPos, yPos, w, h);

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < w; x++) dstPtr[x] = pixVal;
}

/* Draws beveled frame with light/dark edges */
void drawFramework(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t type)
{
	VIDEO_CHECK(video);
	if (w < 2 || h < 2 || x >= SCREEN_W || y >= SCREEN_H) return;
	h--; w--;

	if (type == FRAMEWORK_TYPE1)
	{
		hLine(video, x, y, w, PAL_DSKTOP1);
		vLine(video, x, y + 1, h - 1, PAL_DSKTOP1);
		hLine(video, x, y + h, w, PAL_DSKTOP2);
		vLine(video, x + w, y, h + 1, PAL_DSKTOP2);
		fillRect(video, x + 1, y + 1, w - 1, h - 1, PAL_DESKTOP);
	}
	else
	{
		hLine(video, x, y, w + 1, PAL_DSKTOP2);
		vLine(video, x, y + 1, h, PAL_DSKTOP2);
		hLine(video, x + 1, y + h, w, PAL_DSKTOP1);
		vLine(video, x + w, y + 1, h - 1, PAL_DSKTOP1);
		clearRect(video, x + 1, y + 1, w - 1, h - 1);
	}
}

/* ------------------------------------------------------------------------- */
/*                         BLIT ROUTINES                                     */
/* ------------------------------------------------------------------------- */

/* 32-bit RGB blit with 0x00FF00 as transparent (logo bitmap) */
void blit32(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint32_t *srcPtr, uint16_t w, uint16_t h)
{
	VIDEO_CHECK(video);
	if (!srcPtr) return;
	VIDEO_CHECK_RECT(xPos, yPos, w, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, srcPtr += w, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < w; x++)
			if (srcPtr[x] != 0x00FF00) dstPtr[x] = srcPtr[x] | 0xFF000000;
}

/* 8-bit paletted blit with PAL_TRANSPR as transparent */
void blit(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	VIDEO_CHECK(video);
	if (!srcPtr) return;
	VIDEO_CHECK_RECT(xPos, yPos, w, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, srcPtr += w, dstPtr += SCREEN_W)
	{
		if ((size_t)(dstPtr - video->frameBuffer) >= SCREEN_W * SCREEN_H) return;
		for (int32_t x = 0; x < w; x++)
			if (srcPtr[x] != PAL_TRANSPR) dstPtr[x] = video->palette[srcPtr[x]];
	}
}

void blitClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	if (clipX > w) clipX = w;
	VIDEO_CHECK(video);
	if (!srcPtr) return;
	VIDEO_CHECK_RECT(xPos, yPos, clipX, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, srcPtr += w, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < clipX; x++)
			if (srcPtr[x] != PAL_TRANSPR) dstPtr[x] = video->palette[srcPtr[x]];
}

/* No transparency check (faster) */
void blitFast(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	VIDEO_CHECK(video);
	if (!srcPtr) return;
	VIDEO_CHECK_RECT(xPos, yPos, w, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, srcPtr += w, dstPtr += SCREEN_W)
	{
		if ((size_t)(dstPtr - video->frameBuffer) >= SCREEN_W * SCREEN_H) return;
		for (int32_t x = 0; x < w; x++) dstPtr[x] = video->palette[srcPtr[x]];
	}
}

void blitFastClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	if (clipX > w) clipX = w;
	VIDEO_CHECK(video);
	if (!srcPtr) return;
	VIDEO_CHECK_RECT(xPos, yPos, clipX, h);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, srcPtr += w, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < clipX; x++) dstPtr[x] = video->palette[srcPtr[x]];
}

/* ------------------------------------------------------------------------- */
/*                        TEXT WIDTH                                         */
/* ------------------------------------------------------------------------- */

uint8_t charWidth(char ch) { return font1Widths[ch & 0x7F]; }
uint8_t charWidth16(char ch) { return font2Widths[ch & 0x7F]; }

uint16_t textWidth(const char *textPtr)
{
	uint16_t w = 0;
	while (*textPtr) w += charWidth(*textPtr++);
	return w > 0 ? w - 1 : 0;
}

uint16_t textNWidth(const char *textPtr, int32_t length)
{
	uint16_t w = 0;
	for (int32_t i = 0; i < length && textPtr[i]; i++) w += charWidth(textPtr[i]);
	return w > 0 ? w - 1 : 0;
}

uint16_t textWidth16(const char *textPtr)
{
	uint16_t w = 0;
	while (*textPtr) w += charWidth16(*textPtr++);
	return w > 0 ? w - 1 : 0;
}

/* ------------------------------------------------------------------------- */
/*                      CHARACTER OUTPUT                                     */
/* ------------------------------------------------------------------------- */

/* Sanitizes char to valid font index (handles nordic chars via &0x7F) */
#define SANITIZE_CHAR(c) do { if ((uint8_t)(c) > 127+31) c = ' '; c &= 0x7F; } while(0)

void charOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	VIDEO_CHECK(video);
	if (xPos + FONT1_CHAR_W > SCREEN_W || yPos + FONT1_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font1) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint32_t pixVal = video->palette[paletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++, srcPtr += FONT1_WIDTH, dstPtr += SCREEN_W)
		for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
			if (srcPtr[x]) dstPtr[x] = pixVal;
}

void charOutBg(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr)
{
	VIDEO_CHECK(video);
	if (xPos + FONT1_CHAR_W > SCREEN_W || yPos + FONT1_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font1) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint32_t fg = video->palette[fgPalette], bg = video->palette[bgPalette];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT1_CHAR_H; y++, srcPtr += FONT1_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT1_CHAR_W-1; x++) dstPtr[x] = srcPtr[x] ? fg : bg;
}

void charOutOutlined(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, char chr)
{
	charOut(video, bmp, x - 1, y, PAL_BCKGRND, chr);
	charOut(video, bmp, x + 1, y, PAL_BCKGRND, chr);
	charOut(video, bmp, x, y - 1, PAL_BCKGRND, chr);
	charOut(video, bmp, x, y + 1, PAL_BCKGRND, chr);
	charOut(video, bmp, x, y, paletteIndex, chr);
}

void charOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	VIDEO_CHECK(video);
	if (xPos + FONT1_CHAR_W >= SCREEN_W || yPos + FONT1_CHAR_H >= SCREEN_H) return;
	if (!bmp || !bmp->font1) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint32_t pixVal1 = video->palette[paletteIndex], pixVal2 = video->palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr1 = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT1_CHAR_H; y++, srcPtr += FONT1_WIDTH, dstPtr1 += SCREEN_W, dstPtr2 += SCREEN_W)
		for (int32_t x = 0; x < FONT1_CHAR_W; x++)
			if (srcPtr[x]) { dstPtr2[x] = pixVal2; dstPtr1[x] = pixVal1; }
}

void charOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, uint16_t clipX)
{
	VIDEO_CHECK(video);
	if (xPos + FONT1_CHAR_W > SCREEN_W || yPos + FONT1_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font1) return;
	if (clipX > SCREEN_W) clipX = SCREEN_W;
	if (xPos > clipX) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint32_t pixVal = video->palette[paletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	int32_t width = (xPos + FONT1_CHAR_W > clipX) ? FONT1_CHAR_W - ((xPos + FONT1_CHAR_W) - clipX) : FONT1_CHAR_W;

	for (int32_t y = 0; y < FONT1_CHAR_H; y++, srcPtr += FONT1_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < width; x++)
			if (srcPtr[x]) dstPtr[x] = pixVal;
}

void bigCharOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	VIDEO_CHECK(video);
	if (xPos + FONT2_CHAR_W > SCREEN_W || yPos + FONT2_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font2) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint8_t *srcPtr = &bmp->font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	const uint32_t pixVal = video->palette[paletteIndex];

	for (int32_t y = 0; y < FONT2_CHAR_H; y++, srcPtr += FONT2_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
			if (srcPtr[x]) dstPtr[x] = pixVal;
}

static void bigCharOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	VIDEO_CHECK(video);
	if (xPos + FONT2_CHAR_W >= SCREEN_W || yPos + FONT2_CHAR_H >= SCREEN_H) return;
	if (!bmp || !bmp->font2) return;
	SANITIZE_CHAR(chr);
	if (chr == ' ') return;

	const uint32_t pixVal1 = video->palette[paletteIndex], pixVal2 = video->palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp->font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr1 = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT2_CHAR_H; y++, srcPtr += FONT2_WIDTH, dstPtr1 += SCREEN_W, dstPtr2 += SCREEN_W)
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
			if (srcPtr[x]) { dstPtr2[x] = pixVal2; dstPtr1[x] = pixVal1; }
}

/* ------------------------------------------------------------------------- */
/*                        TEXT OUTPUT                                        */
/* ------------------------------------------------------------------------- */

void textOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	uint16_t currX = x;
	while (*textPtr) { charOut(video, bmp, currX, y, paletteIndex, *textPtr); currX += charWidth(*textPtr++); }
}

void textOutBorder(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t borderPaletteIndex, const char *textPtr)
{
	textOut(video, bmp, x, y-1, borderPaletteIndex, textPtr);
	textOut(video, bmp, x+1, y, borderPaletteIndex, textPtr);
	textOut(video, bmp, x, y+1, borderPaletteIndex, textPtr);
	textOut(video, bmp, x-1, y, borderPaletteIndex, textPtr);
	textOut(video, bmp, x, y, paletteIndex, textPtr);
}

void textOutFixed(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t fgPalette, uint8_t bgPalette, const char *textPtr)
{
	uint16_t currX = x;
	while (*textPtr) { charOutBg(video, bmp, currX, y, fgPalette, bgPalette, *textPtr++); currX += FONT1_CHAR_W-1; }
}

void textOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	uint16_t currX = x;
	while (*textPtr) { charOutShadow(video, bmp, currX, y, paletteIndex, shadowPaletteIndex, *textPtr); currX += charWidth(*textPtr++); }
}

void bigTextOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	uint16_t currX = x;
	while (*textPtr) { bigCharOut(video, bmp, currX, y, paletteIndex, *textPtr); currX += charWidth16(*textPtr++); }
}

void bigTextOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	uint16_t currX = x;
	while (*textPtr) { bigCharOutShadow(video, bmp, currX, y, paletteIndex, shadowPaletteIndex, *textPtr); currX += charWidth16(*textPtr++); }
}

void textOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX)
{
	uint16_t currX = x;
	while (*textPtr && currX < clipX) { charOutClipX(video, bmp, currX, y, paletteIndex, *textPtr, clipX); currX += charWidth(*textPtr++); }
}

/* ------------------------------------------------------------------------- */
/*                         TINY TEXT                                         */
/* ------------------------------------------------------------------------- */

/* Font3: alphanumeric only (0-9, a-z mapped to 0-35) */
void textOutTiny(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str, uint32_t color)
{
	VIDEO_CHECK(video);
	if (!bmp || !bmp->font3 || !str || xPos < 0 || yPos < 0) return;
	if (xPos + (int32_t)(strlen(str) * FONT3_CHAR_W) > SCREEN_W || yPos + FONT3_CHAR_H > SCREEN_H) return;

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	while (*str)
	{
		char chr = *str++;
		if (chr >= '0' && chr <= '9') chr -= '0';
		else if (chr >= 'a' && chr <= 'z') chr = chr - 'a' + 10;
		else if (chr >= 'A' && chr <= 'Z') chr = chr - 'A' + 10;
		else { dstPtr += FONT3_CHAR_W; continue; }

		const uint8_t *srcPtr = &bmp->font3[chr * FONT3_CHAR_W];
		for (int32_t y = 0; y < FONT3_CHAR_H; y++, srcPtr += FONT3_WIDTH, dstPtr += SCREEN_W)
			for (int32_t x = 0; x < FONT3_CHAR_W; x++)
				if (srcPtr[x]) dstPtr[x] = color;
		dstPtr -= (SCREEN_W * FONT3_CHAR_H) - FONT3_CHAR_W;
	}
}

void textOutTinyOutline(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str)
{
	const uint32_t bg = video->palette[PAL_BCKGRND], fg = video->palette[PAL_FORGRND];
	textOutTiny(video, bmp, xPos-1, yPos, str, bg);
	textOutTiny(video, bmp, xPos, yPos-1, str, bg);
	textOutTiny(video, bmp, xPos+1, yPos, str, bg);
	textOutTiny(video, bmp, xPos, yPos+1, str, bg);
	textOutTiny(video, bmp, xPos, yPos, str, fg);
}

/* ------------------------------------------------------------------------- */
/*                          HEX OUTPUT                                       */
/* ------------------------------------------------------------------------- */

void hexOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint32_t val, uint8_t numDigits)
{
	VIDEO_CHECK(video);
	if (!numDigits || (uint32_t)xPos + (numDigits * FONT6_CHAR_W) > SCREEN_W || yPos + FONT6_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font6) return;

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		const uint8_t *srcPtr = &bmp->font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];
		for (int32_t y = 0; y < FONT6_CHAR_H; y++, srcPtr += FONT6_WIDTH, dstPtr += SCREEN_W)
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
				if (srcPtr[x]) dstPtr[x] = pixVal;
		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W;
	}
}

void hexOutBg(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint32_t val, uint8_t numDigits)
{
	VIDEO_CHECK(video);
	if (!numDigits || (uint32_t)xPos + (numDigits * FONT6_CHAR_W) > SCREEN_W || yPos + FONT6_CHAR_H > SCREEN_H) return;
	if (!bmp || !bmp->font6) return;

	const uint32_t fg = video->palette[fgPalette], bg = video->palette[bgPalette];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		const uint8_t *srcPtr = &bmp->font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];
		for (int32_t y = 0; y < FONT6_CHAR_H; y++, srcPtr += FONT6_WIDTH, dstPtr += SCREEN_W)
			for (int32_t x = 0; x < FONT6_CHAR_W; x++) dstPtr[x] = srcPtr[x] ? fg : bg;
		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W;
	}
}

void hexOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, uint32_t val, uint8_t numDigits)
{
	hexOut(video, bmp, xPos + 1, yPos + 1, shadowPaletteIndex, val, numDigits);
	hexOut(video, bmp, xPos, yPos, paletteIndex, val, numDigits);
}

/* Pattern editor 2-digit hex (font4 - smaller pattern font) */
void pattTwoHexOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color)
{
	if (!video || !video->frameBuffer || !bmp || !bmp->font4) return;
	if (xPos + (FONT4_CHAR_W * 2) > SCREEN_W || yPos + FONT4_CHAR_H > SCREEN_H) return;

	const uint8_t *ch1Ptr = &bmp->font4[(val >> 4) * FONT4_CHAR_W];
	const uint8_t *ch2Ptr = &bmp->font4[(val & 0x0F) * FONT4_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++, ch1Ptr += FONT4_WIDTH, ch2Ptr += FONT4_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x]) dstPtr[x] = color;
			if (ch2Ptr[x]) dstPtr[FONT4_CHAR_W + x] = color;
		}
}
