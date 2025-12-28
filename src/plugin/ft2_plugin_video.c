/**
 * @file ft2_plugin_video.c
 * @brief Exact port of FT2 video/drawing primitives for plugin use
 * 
 * Direct port of ft2_video.c and ft2_gui.c drawing functions.
 * Only modification: uses instance-based state instead of globals.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"

/* Font width tables - exact FT2 values from ft2_tables.c */
extern const uint8_t font1Widths[128];
extern const uint8_t font2Widths[128];

/* Default FT2 palette (Arctic theme) */
static const uint32_t defaultPalette[PAL_NUM] = {
	0x000000, /* PAL_BCKGRND */
	0xD2D2D2, /* PAL_PATTEXT */
	0x5454B2, /* PAL_BLCKMRK */
	0xD2D2D2, /* PAL_BLCKTXT */
	0x6E6E6E, /* PAL_DESKTOP */
	0xD2D2D2, /* PAL_FORGRND */
	0x555555, /* PAL_BUTTONS */
	0xD2D2D2, /* PAL_BTNTEXT */
	0x444444, /* PAL_DSKTOP2 */
	0x999999, /* PAL_DSKTOP1 */
	0x333333, /* PAL_BUTTON2 */
	0x777777, /* PAL_BUTTON1 */
	0xB2B2B2, /* PAL_MOUSEPT */
	0x000000, /* PAL_PIANOXOR1 */
	0x000000, /* PAL_PIANOXOR2 */
	0x000000, /* PAL_PIANOXOR3 */
	0x4444FF, /* PAL_LOOPPIN */
	0x5555AA, /* PAL_TEXTMRK */
	0x5555AA, /* PAL_BOXSLCT */
	0xFFFFFF  /* PAL_CUSTOM */
};

bool ft2_video_init(ft2_video_t *video)
{
	if (video == NULL)
		return false;

	video->frameBuffer = (uint32_t *)calloc(SCREEN_W * SCREEN_H, sizeof(uint32_t));
	if (video->frameBuffer == NULL)
		return false;

	ft2_video_set_default_palette(video);
	return true;
}

void ft2_video_free(ft2_video_t *video)
{
	if (video == NULL)
		return;

	if (video->frameBuffer != NULL)
	{
		free(video->frameBuffer);
		video->frameBuffer = NULL;
	}
}

void ft2_video_set_default_palette(ft2_video_t *video)
{
	if (video == NULL)
		return;

	/* Store palette index in high byte - required for XOR cursor drawing */
	for (int i = 0; i < PAL_NUM; i++)
		video->palette[i] = ((uint32_t)i << 24) | defaultPalette[i];
}

/* ========== LINE ROUTINES - EXACT PORTS ========== */

void hLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(x < SCREEN_W && y < SCREEN_H && (x + w) <= SCREEN_W);

	const uint32_t pixVal = video->palette[paletteIndex];

	uint32_t *dstPtr = &video->frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < w; i++)
		dstPtr[i] = pixVal;
}

void vLine(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(x < SCREEN_W && y < SCREEN_H && (y + h) <= SCREEN_H);

	const uint32_t pixVal = video->palette[paletteIndex];

	uint32_t *dstPtr = &video->frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < h; i++)
	{
		*dstPtr = pixVal;
		 dstPtr += SCREEN_W;
	}
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

void line(ft2_video_t *video, int16_t x1, int16_t x2, int16_t y1, int16_t y2, uint8_t paletteIndex)
{
	assert(video != NULL && video->frameBuffer != NULL);

	const int16_t dx = x2 - x1;
	const uint16_t ax = ABS(dx) * 2;
	const int16_t sx = SGN(dx);
	const int16_t dy = y2 - y1;
	const uint16_t ay = ABS(dy) * 2;
	const int16_t sy = SGN(dy);
	int16_t x = x1;
	int16_t y = y1;

	uint32_t pixVal = video->palette[paletteIndex];
	const int32_t pitch = sy * SCREEN_W;
	uint32_t *dst32 = &video->frameBuffer[(y * SCREEN_W) + x];

	/* draw line */
	if (ax > ay)
	{
		int16_t d = ay - (ax >> 1);
		while (true)
		{
			*dst32 = pixVal;
			if (x == x2)
				break;

			if (d >= 0)
			{
				d -= ax;
				dst32 += pitch;
			}

			x += sx;
			d += ay;
			dst32 += sx;
		}
	}
	else
	{
		int16_t d = ax - (ay >> 1);
		while (true)
		{
			*dst32 = pixVal;
			if (y == y2)
				break;

			if (d >= 0)
			{
				d -= ay;
				dst32 += sx;
			}

			y += sy;
			d += ax;
			dst32 += pitch;
		}
	}
}

/* ========== FILL ROUTINES - EXACT PORTS ========== */

void clearRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	const uint32_t pitch = w * sizeof(int32_t);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++, dstPtr += SCREEN_W)
		memset(dstPtr, 0, pitch);
}

void fillRect(ft2_video_t *video, uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			dstPtr[x] = pixVal;

		dstPtr += SCREEN_W;
	}
}

void drawFramework(ft2_video_t *video, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t type)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(x < SCREEN_W && y < SCREEN_H && w >= 2 && h >= 2);

	h--;
	w--;

	if (type == FRAMEWORK_TYPE1)
	{
		/* top left corner */
		hLine(video, x, y,     w,     PAL_DSKTOP1);
		vLine(video, x, y + 1, h - 1, PAL_DSKTOP1);

		/* bottom right corner */
		hLine(video, x,     y + h, w,     PAL_DSKTOP2);
		vLine(video, x + w, y,     h + 1, PAL_DSKTOP2);

		/* fill background */
		fillRect(video, x + 1, y + 1, w - 1, h - 1, PAL_DESKTOP);
	}
	else
	{
		/* top left corner */
		hLine(video, x, y,     w + 1, PAL_DSKTOP2);
		vLine(video, x, y + 1, h,     PAL_DSKTOP2);

		/* bottom right corner */
		hLine(video, x + 1, y + h, w,     PAL_DSKTOP1);
		vLine(video, x + w, y + 1, h - 1, PAL_DSKTOP1);

		/* clear background */
		clearRect(video, x + 1, y + 1, w - 1, h - 1);
	}
}

/* ========== BLIT ROUTINES - EXACT PORTS ========== */

void blit32(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint32_t *srcPtr, uint16_t w, uint16_t h)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
		{
			if (srcPtr[x] != 0x00FF00)
				dstPtr[x] = srcPtr[x] | 0xFF000000; /* most significant 8 bits = palette number. 0xFF because no true palette */
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blit(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
		{
			const uint32_t pixel = srcPtr[x];
			if (pixel != PAL_TRANSPR)
				dstPtr[x] = video->palette[pixel];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	if (clipX > w)
		clipX = w;

	assert(video != NULL && video->frameBuffer != NULL);
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < clipX; x++)
		{
			const uint32_t pixel = srcPtr[x];
			if (pixel != PAL_TRANSPR)
				dstPtr[x] = video->palette[pixel];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFast(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			dstPtr[x] = video->palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFastClipX(ft2_video_t *video, uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	if (clipX > w)
		clipX = w;

	assert(video != NULL && video->frameBuffer != NULL);
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < clipX; x++)
			dstPtr[x] = video->palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

/* ========== TEXT WIDTH ROUTINES - EXACT PORTS ========== */

uint8_t charWidth(char ch)
{
	return font1Widths[ch & 0x7F];
}

uint8_t charWidth16(char ch)
{
	return font2Widths[ch & 0x7F];
}

uint16_t textWidth(const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t w = 0;
	while (*textPtr != '\0')
		w += charWidth(*textPtr++);

	/* there will be a pixel spacer at the end of the last char/glyph, remove it */
	if (w > 0)
		w--;

	return w;
}

uint16_t textNWidth(const char *textPtr, int32_t length)
{
	assert(textPtr != NULL);

	uint16_t w = 0;
	for (int32_t i = 0; i < length; i++)
	{
		const char ch = textPtr[i];
		if (ch == '\0')
			break;

		w += charWidth(ch);
	}

	if (w > 0)
		w--;

	return w;
}

uint16_t textWidth16(const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t w = 0;
	while (*textPtr != '\0')
		w += charWidth16(*textPtr++);

	if (w > 0)
		w--;

	return w;
}

/* ========== CHARACTER OUTPUT - EXACT PORTS ========== */

void charOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font1 == NULL)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F; /* this is important to get the nordic glyphs in the font */
	if (chr == ' ')
		return;

	const uint32_t pixVal = video->palette[paletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void charOutBg(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font1 == NULL)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F;
	if (chr == ' ')
		return;

	const uint32_t fg = video->palette[fgPalette];
	const uint32_t bg = video->palette[bgPalette];

	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT1_CHAR_W-1; x++)
			dstPtr[x] = srcPtr[x] ? fg : bg;

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void charOutOutlined(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, char chr)
{
	charOut(video, bmp, x - 1, y,     PAL_BCKGRND, chr);
	charOut(video, bmp, x + 1, y,     PAL_BCKGRND, chr);
	charOut(video, bmp, x,     y - 1, PAL_BCKGRND, chr);
	charOut(video, bmp, x,     y + 1, PAL_BCKGRND, chr);

	charOut(video, bmp, x, y, paletteIndex, chr);
}

void charOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font1 == NULL)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F;
	if (chr == ' ')
		return;

	const uint32_t pixVal1 = video->palette[paletteIndex];
	const uint32_t pixVal2 = video->palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr1 = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT1_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
		}

		srcPtr += FONT1_WIDTH;
		dstPtr1 += SCREEN_W;
		dstPtr2 += SCREEN_W;
	}
}

void charOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, uint16_t clipX)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font1 == NULL)
		return;

	if (xPos > clipX)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F;
	if (chr == ' ')
		return;

	const uint32_t pixVal = video->palette[paletteIndex];
	const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	int32_t width = FONT1_CHAR_W;
	if (xPos+width > clipX)
		width = FONT1_CHAR_W - ((xPos + width) - clipX);

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < width; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void bigCharOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font2 == NULL)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F;
	if (chr == ' ')
		return;

	const uint8_t *srcPtr = &bmp->font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	const uint32_t pixVal = video->palette[paletteIndex];

	for (int32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT2_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void bigCharOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font2 == NULL)
		return;

	if ((uint8_t)chr > 127+31)
		chr = ' ';

	chr &= 0x7F;
	if (chr == ' ')
		return;

	const uint32_t pixVal1 = video->palette[paletteIndex];
	const uint32_t pixVal2 = video->palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp->font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr1 = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
		}

		srcPtr += FONT2_WIDTH;
		dstPtr1 += SCREEN_W;
		dstPtr2 += SCREEN_W;
	}
}

/* ========== TEXT OUTPUT - EXACT PORTS ========== */

void textOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOut(video, bmp, currX, y, paletteIndex, chr);
		currX += charWidth(chr);
	}
}

void textOutBorder(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t borderPaletteIndex, const char *textPtr)
{
	textOut(video, bmp, x,   y-1, borderPaletteIndex, textPtr); /* top */
	textOut(video, bmp, x+1, y,   borderPaletteIndex, textPtr); /* right */
	textOut(video, bmp, x,   y+1, borderPaletteIndex, textPtr); /* bottom */
	textOut(video, bmp, x-1, y,   borderPaletteIndex, textPtr); /* left */

	textOut(video, bmp, x, y, paletteIndex, textPtr);
}

void textOutFixed(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t fgPalette, uint8_t bgPalette, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutBg(video, bmp, currX, y, fgPalette, bgPalette, chr);
		currX += FONT1_CHAR_W-1;
	}
}

void textOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutShadow(video, bmp, currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth(chr);
	}
}

void bigTextOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOut(video, bmp, currX, y, paletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void bigTextOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOutShadow(video, bmp, currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void textOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutClipX(video, bmp, currX, y, paletteIndex, chr, clipX);

		currX += charWidth(chr);
		if (currX >= clipX)
			break;
	}
}

/* ========== TINY TEXT - EXACT PORTS ========== */

void textOutTiny(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str, uint32_t color)
{
	assert(video != NULL && video->frameBuffer != NULL);

	if (bmp == NULL || bmp->font3 == NULL)
		return;

	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];
	while (*str != '\0')
	{
		char chr = *str++;

		if (chr >= '0' && chr <= '9')
		{
			chr -= '0';
		}
		else if (chr >= 'a' && chr <= 'z')
		{
			chr -= 'a';
			chr += 10;
		}
		else if (chr >= 'A' && chr <= 'Z')
		{
			chr -= 'A';
			chr += 10;
		}
		else
		{
			dstPtr += FONT3_CHAR_W;
			continue;
		}

		const uint8_t *srcPtr = &bmp->font3[chr * FONT3_CHAR_W];
		for (int32_t y = 0; y < FONT3_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT3_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT3_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT3_CHAR_H) - FONT3_CHAR_W;
	}
}

void textOutTinyOutline(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xPos, int32_t yPos, char *str)
{
	const uint32_t bgColor = video->palette[PAL_BCKGRND];
	const uint32_t fgColor = video->palette[PAL_FORGRND];

	textOutTiny(video, bmp, xPos-1, yPos,   str, bgColor);
	textOutTiny(video, bmp, xPos,   yPos-1, str, bgColor);
	textOutTiny(video, bmp, xPos+1, yPos,   str, bgColor);
	textOutTiny(video, bmp, xPos,   yPos+1, str, bgColor);

	textOutTiny(video, bmp, xPos, yPos, str, fgColor);
}

/* ========== HEX OUTPUT - EXACT PORTS ========== */

void hexOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint32_t val, uint8_t numDigits)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font6 == NULL)
		return;

	const uint32_t pixVal = video->palette[paletteIndex];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		const uint8_t *srcPtr = &bmp->font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		/* render glyph */
		for (int32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = pixVal;
			}

			srcPtr += FONT6_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W; /* xpos += FONT6_CHAR_W */
	}
}

void hexOutBg(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint32_t val, uint8_t numDigits)
{
	assert(video != NULL && video->frameBuffer != NULL);
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (bmp == NULL || bmp->font6 == NULL)
		return;

	const uint32_t fg = video->palette[fgPalette];
	const uint32_t bg = video->palette[bgPalette];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		/* extract current nybble and set pointer to glyph */
		const uint8_t *srcPtr = &bmp->font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		/* render glyph */
		for (int32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
				dstPtr[x] = srcPtr[x] ? fg : bg;

			srcPtr += FONT6_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W;
	}
}

void hexOutShadow(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, uint32_t val, uint8_t numDigits)
{
	hexOut(video, bmp, xPos + 1, yPos + 1, shadowPaletteIndex, val, numDigits);
	hexOut(video, bmp, xPos + 0, yPos + 0, paletteIndex, val, numDigits);
}

/* Pattern editor style two-digit hex output using font4 - exact port of standalone pattTwoHexOut() */
void pattTwoHexOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color)
{
	if (video == NULL || video->frameBuffer == NULL || bmp == NULL || bmp->font4 == NULL)
		return;

	const uint8_t *ch1Ptr = &bmp->font4[(val >> 4) * FONT4_CHAR_W];
	const uint8_t *ch2Ptr = &bmp->font4[(val & 0x0F) * FONT4_CHAR_W];
	uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT4_CHAR_W + x] = color;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}
