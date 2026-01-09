/**
 * @file ft2_plugin_pattern_ed.c
 * @brief Pattern editor rendering and editing.
 *
 * Renders pattern data using multiple font sizes based on visible channel count.
 * Supports block marking, copy/paste, transpose, instrument remap, and extended mode.
 * Note/instrument/volume/effect display adapts to 4/6/8/12 channel layouts.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_instance.h"

#ifndef MAX_CHANNELS
#define MAX_CHANNELS FT2_MAX_CHANNELS
#endif

/* ============ LOOKUP TABLES ============ */

/* Channel width in pixels for 4/4/6/8/12/12 visible channels */
static const uint16_t chanWidths[6] = { 141, 141, 93, 69, 45, 45 };

/* Pattern coordinate tables [stretch][scroll][extended] - row Y positions and counts */
static const pattCoord_t pattCoordTable[2][2][2] = {
	/* stretch=0 */ {
		/* scroll=0 */ {{ 176, 292, 177, 283, 293, 13, 13 }, {  71, 236,  73, 227, 237, 19, 20 }},
		/* scroll=1 */ {{ 176, 285, 177, 276, 286, 12, 12 }, {  71, 236,  73, 227, 237, 19, 18 }}
	},
	/* stretch=1 */ {
		/* scroll=0 */ {{ 177, 286, 178, 277, 288,  9, 10 }, {  71, 240,  77, 231, 242, 14, 14 }},
		/* scroll=1 */ {{ 176, 285, 177, 276, 286,  9,  9 }, {  71, 238,  75, 229, 240, 14, 13 }}
	}
};

/* Framework heights [stretch][scroll][extended] */
static const pattCoord2_t pattCoord2Table[2][2][2] = {
	/* stretch=0 */ {
		/* scroll=0 */ {{ 175, 291, 107, 107 }, {  70, 235, 156, 163 }},
		/* scroll=1 */ {{ 175, 284, 100, 100 }, {  70, 235, 156, 149 }}
	},
	/* stretch=1 */ {
		/* scroll=0 */ {{ 175, 285, 101, 113 }, {  70, 239, 160, 159 }},
		/* scroll=1 */ {{ 175, 284, 100, 100 }, {  70, 237, 158, 148 }}
	}
};

/* Block mark Y coordinates [stretch][scroll][extended] */
static const markCoord_t markCoordTable[2][2][2] = {
	/* stretch=0 */ {
		/* scroll=0 */ {{ 177, 281, 293 }, {  73, 225, 237 }},
		/* scroll=1 */ {{ 177, 274, 286 }, {  73, 225, 237 }}
	},
	/* stretch=1 */ {
		/* scroll=0 */ {{ 176, 275, 286 }, {  75, 229, 240 }},
		/* scroll=1 */ {{ 175, 274, 284 }, {  73, 227, 238 }}
	}
};

/* Mouse Y to row conversion table [stretch][scroll][extended] */
typedef struct pattCoordsMouse_t { uint16_t upperRowsY, midRowY, lowerRowsY, numUpperRows; } pattCoordsMouse_t;
static const pattCoordsMouse_t pattCoordMouseTable[2][2][2] = {
	/* stretch=0 */ {
		/* scroll=0 */ {{ 177, 281, 293, 13 }, {  73, 225, 237, 19 }},
		/* scroll=1 */ {{ 177, 274, 286, 12 }, {  73, 225, 237, 19 }}
	},
	/* stretch=1 */ {
		/* scroll=0 */ {{ 176, 275, 286,  9 }, {  75, 229, 240, 14 }},
		/* scroll=1 */ {{ 175, 274, 283,  9 }, {  73, 227, 238, 14 }}
	}
};

/* Cursor X position [volCol*32 + channelMode*8 + cursorObject] */
static const uint8_t pattCursorXTab[2 * 4 * 8] = {
	/* No vol col: 4ch, 6ch, 8ch, 12ch */
	32, 88, 104, 0, 0, 120, 136, 152,
	32, 80,  88, 0, 0,  96, 104, 112,
	32, 56,  64, 0, 0,  72,  80,  88,
	32, 52,  56, 0, 0,  60,  64,  68,
	/* With vol col: 4ch, 6ch, 8ch, 12ch */
	32, 96, 104, 120, 128, 144, 152, 160,
	32, 56,  64,  80,  88,  96, 104, 112,
	32, 60,  64,  72,  76,  84,  88,  92,
	32, 60,  64,  72,  76,  84,  88,  92,
};

/* Cursor width [volCol*32 + channelMode*8 + cursorObject] */
static const uint8_t pattCursorWTab[2 * 4 * 8] = {
	/* No vol col: 4ch, 6ch, 8ch, 12ch */
	48, 16, 16, 0, 0, 16, 16, 16,
	48,  8,  8, 0, 0,  8,  8,  8,
	24,  8,  8, 0, 0,  8,  8,  8,
	20,  4,  4, 0, 0,  4,  4,  4,
	/* With vol col: 4ch, 6ch, 8ch, 12ch */
	48, 8, 16, 8, 8, 8, 8, 8,
	24, 8,  8, 8, 8, 8, 8, 8,
	24, 4,  8, 4, 4, 4, 4, 4,
	24, 4,  8, 4, 4, 4, 4, 4,
};

/* Maps numChannelsShown-1 to column mode index (0-3) */
static const uint8_t columnModeTab[12] = { 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3 };

/* Note name font offsets: [note % 12] -> char offset. Sharps vs flats notation. */
static const uint8_t sharpNote1Char_small[12] = {  8*6,  8*6,  9*6,  9*6, 10*6, 11*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6 };
static const uint8_t sharpNote2Char_small[12] = { 16*6, 15*6, 16*6, 15*6, 16*6, 16*6, 15*6, 16*6, 15*6, 16*6, 15*6, 16*6 };
static const uint8_t flatNote1Char_small[12]  = {  8*6,  9*6,  9*6, 10*6, 10*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6, 14*6 };
static const uint8_t flatNote2Char_small[12]  = { 16*6, 17*6, 16*6, 17*6, 16*6, 16*6, 17*6, 16*6, 17*6, 16*6, 17*6, 16*6 };
static const uint8_t sharpNote1Char_med[12]   = { 12*8, 12*8, 13*8, 13*8, 14*8, 15*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8 };
static const uint16_t sharpNote2Char_med[12]  = { 36*8, 37*8, 36*8, 37*8, 36*8, 36*8, 37*8, 36*8, 37*8, 36*8, 37*8, 36*8 };
static const uint8_t flatNote1Char_med[12]    = { 12*8, 13*8, 13*8, 14*8, 14*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8, 11*8 };
static const uint16_t flatNote2Char_med[12]   = { 36*8, 38*8, 36*8, 38*8, 36*8, 36*8, 38*8, 36*8, 38*8, 36*8, 38*8, 36*8 };
static const uint16_t sharpNote1Char_big[12]  = { 12*16, 12*16, 13*16, 13*16, 14*16, 15*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16 };
static const uint16_t sharpNote2Char_big[12]  = { 36*16, 37*16, 36*16, 37*16, 36*16, 36*16, 37*16, 36*16, 37*16, 36*16, 37*16, 36*16 };
static const uint16_t flatNote1Char_big[12]   = { 12*16, 13*16, 13*16, 14*16, 14*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16, 11*16 };
static const uint16_t flatNote2Char_big[12]   = { 36*16, 38*16, 36*16, 38*16, 36*16, 36*16, 38*16, 36*16, 38*16, 36*16, 38*16, 36*16 };

/* Volume column command chars: [vol >> 4] -> font char */
static const uint8_t vol2charTab1[16] = { 39, 0, 1, 2, 3, 4, 36, 52, 53, 54, 28, 31, 25, 58, 59, 22 };
static const uint8_t vol2charTab2[16] = { 42, 0, 1, 2, 3, 4, 36, 37, 38, 39, 28, 31, 25, 40, 41, 22 };

/* Note -> semitone (0-11) and octave (0-7) */
static const uint8_t noteTab1[96] = {
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11, 0,1,2,3,4,5,6,7,8,9,10,11,
};
static const uint8_t noteTab2[96] = {
	0,0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,3,3,3,3,
	4,4,4,4,4,4,4,4,4,4,4,4, 5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6, 7,7,7,7,7,7,7,7,7,7,7,7,
};

/* Row number hex->decimal display (0-99 maps to BCD) */
static const uint8_t hex2Dec[256] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16,17,18,19,20,21,22,23,24,25,
	32,33,34,35,36,37,38,39,40,41, 48,49,50,51,52,53,54,55,56,57,
	64,65,66,67,68,69,70,71,72,73, 80,81,82,83,84,85,86,87,88,89,
	96,97,98,99,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0
};

/* ============ BLOCK BUFFER ============ */

static bool blockCopied = false;
static int32_t markXSize = 0, markYSize = 0;
static ft2_note_t blkCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];

/* Mouse marking state */
static int32_t lastMouseX = 0, lastMouseY = 0;
static int8_t lastChMark = 0;
static int16_t lastRowMark = 0;
static int16_t lastMarkX1 = -1, lastMarkX2 = -1, lastMarkY1 = -1, lastMarkY2 = -1;

/* ============ DRAWING PRIMITIVES ============ */

/* Draw pattern font character (font3=small, font4=med, font5=big, font7=tiny) */
static void pattCharOut(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, 
                        uint8_t chr, uint8_t fontType, uint32_t color, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp) return;
	
	const uint8_t *srcPtr;
	int32_t charW, charH, width;

	switch (fontType) {
		case FONT_TYPE3:
			if (!bmp->font3) return;
			srcPtr = &bmp->font3[chr * FONT3_CHAR_W];
			charW = FONT3_CHAR_W; charH = FONT3_CHAR_H; width = FONT3_WIDTH;
			break;
		case FONT_TYPE4:
			if (!ed->font4Ptr) return;
			srcPtr = &ed->font4Ptr[chr * FONT4_CHAR_W];
			charW = FONT4_CHAR_W; charH = FONT4_CHAR_H; width = FONT4_WIDTH;
			break;
		case FONT_TYPE5:
			if (!ed->font5Ptr) return;
			srcPtr = &ed->font5Ptr[chr * FONT5_CHAR_W];
			charW = FONT5_CHAR_W; charH = FONT5_CHAR_H; width = FONT5_WIDTH;
			break;
		default:
			if (!bmp->font7) return;
			srcPtr = &bmp->font7[chr * FONT7_CHAR_W];
			charW = FONT7_CHAR_W; charH = FONT7_CHAR_H; width = FONT7_WIDTH;
			break;
	}

	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < charH; y++, srcPtr += width, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < charW; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

/* ============ NOTE DRAWING (size variants) ============ */

/* Small: 12-channel mode (font7) */
static void drawEmptyNoteSmall(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp || !bmp->font7) return;
	const uint8_t *srcPtr = &bmp->font7[18 * FONT7_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT7_CHAR_H; y++, srcPtr += FONT7_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT7_CHAR_W * 3; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawKeyOffSmall(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp || !bmp->font7) return;
	const uint8_t *srcPtr = &bmp->font7[21 * FONT7_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + (xPos + 2)];
	for (int32_t y = 0; y < FONT7_CHAR_H; y++, srcPtr += FONT7_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT7_CHAR_W * 2; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawNoteSmall(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp || !bmp->font7) return;
	noteNum--;
	const uint8_t note = noteTab1[noteNum];
	uint32_t char1 = ed->ptnAcc ? flatNote1Char_small[note] : sharpNote1Char_small[note];
	uint32_t char2 = ed->ptnAcc ? flatNote2Char_small[note] : sharpNote2Char_small[note];
	uint32_t char3 = noteTab2[noteNum] * FONT7_CHAR_W;

	const uint8_t *ch1Ptr = &bmp->font7[char1], *ch2Ptr = &bmp->font7[char2], *ch3Ptr = &bmp->font7[char3];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT7_CHAR_H; y++, ch1Ptr += FONT7_WIDTH, ch2Ptr += FONT7_WIDTH, ch3Ptr += FONT7_WIDTH, dstPtr += SCREEN_W) {
		for (int32_t x = 0; x < FONT7_CHAR_W; x++) {
			if (ch1Ptr[x]) dstPtr[x] = color;
			if (ch2Ptr[x]) dstPtr[FONT7_CHAR_W + x] = color;
			if (ch3Ptr[x]) dstPtr[((FONT7_CHAR_W * 2) - 2) + x] = color;
		}
	}
}

/* Medium: 6-8 channel mode (font4) */
static void drawEmptyNoteMedium(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	(void)bmp;
	if (!ed || !ed->video || !ed->font4Ptr) return;
	const uint8_t *srcPtr = &ed->font4Ptr[43 * FONT4_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT4_CHAR_H; y++, srcPtr += FONT4_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT4_CHAR_W * 3; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawKeyOffMedium(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	(void)bmp;
	if (!ed || !ed->video || !ed->font4Ptr) return;
	const uint8_t *srcPtr = &ed->font4Ptr[40 * FONT4_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT4_CHAR_H; y++, srcPtr += FONT4_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT4_CHAR_W * 3; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawNoteMedium(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color, const ft2_bmp_t *bmp)
{
	(void)bmp;
	if (!ed || !ed->video || !ed->font4Ptr) return;
	noteNum--;
	const uint8_t note = noteTab1[noteNum];
	uint32_t char1 = ed->ptnAcc ? flatNote1Char_med[note] : sharpNote1Char_med[note];
	uint32_t char2 = ed->ptnAcc ? flatNote2Char_med[note] : sharpNote2Char_med[note];
	uint32_t char3 = noteTab2[noteNum] * FONT4_CHAR_W;

	const uint8_t *ch1Ptr = &ed->font4Ptr[char1], *ch2Ptr = &ed->font4Ptr[char2], *ch3Ptr = &ed->font4Ptr[char3];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT4_CHAR_H; y++, ch1Ptr += FONT4_WIDTH, ch2Ptr += FONT4_WIDTH, ch3Ptr += FONT4_WIDTH, dstPtr += SCREEN_W) {
		for (int32_t x = 0; x < FONT4_CHAR_W; x++) {
			if (ch1Ptr[x]) dstPtr[x] = color;
			if (ch2Ptr[x]) dstPtr[FONT4_CHAR_W + x] = color;
			if (ch3Ptr[x]) dstPtr[(FONT4_CHAR_W * 2) + x] = color;
		}
	}
}

/* Big: 4-6 channel mode (font5 for notes, font4 for empty/keyoff) */
static void drawEmptyNoteBig(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	(void)bmp;
	if (!ed || !ed->video || !ed->font4Ptr) return;
	const uint8_t *srcPtr = &ed->font4Ptr[67 * FONT4_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT4_CHAR_H; y++, srcPtr += FONT4_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT4_CHAR_W * 6; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawKeyOffBig(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, uint32_t color, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp || !bmp->font4) return;
	const uint8_t *srcPtr = &bmp->font4[61 * FONT4_CHAR_W];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT4_CHAR_H; y++, srcPtr += FONT4_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT4_CHAR_W * 6; x++)
			if (srcPtr[x]) dstPtr[x] = color;
}

static void drawNoteBig(ft2_pattern_editor_t *ed, uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color, const ft2_bmp_t *bmp)
{
	(void)bmp;
	if (!ed || !ed->video || !ed->font5Ptr) return;
	noteNum--;
	const uint8_t note = noteTab1[noteNum];
	uint32_t char1 = ed->ptnAcc ? flatNote1Char_big[note] : sharpNote1Char_big[note];
	uint32_t char2 = ed->ptnAcc ? flatNote2Char_big[note] : sharpNote2Char_big[note];
	uint32_t char3 = noteTab2[noteNum] * FONT5_CHAR_W;

	const uint8_t *ch1Ptr = &ed->font5Ptr[char1], *ch2Ptr = &ed->font5Ptr[char2], *ch3Ptr = &ed->font5Ptr[char3];
	uint32_t *dstPtr = &ed->video->frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < FONT5_CHAR_H; y++, ch1Ptr += FONT5_WIDTH, ch2Ptr += FONT5_WIDTH, ch3Ptr += FONT5_WIDTH, dstPtr += SCREEN_W) {
		for (int32_t x = 0; x < FONT5_CHAR_W; x++) {
			if (ch1Ptr[x]) dstPtr[x] = color;
			if (ch2Ptr[x]) dstPtr[FONT5_CHAR_W + x] = color;
			if (ch3Ptr[x]) dstPtr[(FONT5_CHAR_W * 2) + x] = color;
		}
	}
}

/* Draw row numbers (left and right columns) */
static void drawRowNums(ft2_pattern_editor_t *ed, int32_t yPos, uint8_t row, bool selectedRowFlag, const ft2_bmp_t *bmp)
{
#define LEFT_ROW_XPOS 8
#define RIGHT_ROW_XPOS 608
	(void)bmp;
	if (!ed || !ed->video || !ed->font4Ptr) return;

	uint32_t pixVal = selectedRowFlag ? ed->video->palette[PAL_FORGRND] :
	                  (ed->ptnLineLight && !(row & 3)) ? ed->video->palette[PAL_BLCKTXT] :
	                  ed->video->palette[PAL_PATTEXT];

	if (!ed->ptnHex) row = hex2Dec[row];

	const uint8_t *src1Ptr = &ed->font4Ptr[(row >> 4) * FONT4_CHAR_W];
	const uint8_t *src2Ptr = &ed->font4Ptr[(row & 0x0F) * FONT4_CHAR_W];
	uint32_t *dst1Ptr = &ed->video->frameBuffer[(yPos * SCREEN_W) + LEFT_ROW_XPOS];
	uint32_t *dst2Ptr = dst1Ptr + (RIGHT_ROW_XPOS - LEFT_ROW_XPOS);

	for (int32_t y = 0; y < FONT4_CHAR_H; y++, src1Ptr += FONT4_WIDTH, src2Ptr += FONT4_WIDTH, dst1Ptr += SCREEN_W, dst2Ptr += SCREEN_W) {
		for (int32_t x = 0; x < FONT4_CHAR_W; x++) {
			if (src1Ptr[x]) { dst1Ptr[x] = pixVal; dst2Ptr[x] = pixVal; }
			if (src2Ptr[x]) { dst1Ptr[FONT4_CHAR_W + x] = pixVal; dst2Ptr[FONT4_CHAR_W + x] = pixVal; }
		}
	}
}

/* Draw channel numbers above pattern */
static void drawChannelNumbering(ft2_pattern_editor_t *ed, uint16_t yPos, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video || !bmp) return;
	uint16_t xPos = 30;
	for (uint8_t i = 0, ch = ed->channelOffset + 1; i < ed->numChannelsShown; i++, ch++, xPos += ed->patternChannelWidth) {
		if (ch < 10) {
			charOutOutlined(ed->video, bmp, xPos, yPos, PAL_MOUSEPT, '0' + ch);
		} else {
			charOutOutlined(ed->video, bmp, xPos, yPos, PAL_MOUSEPT, '0' + (ch / 10));
			charOutOutlined(ed->video, bmp, xPos + (FONT1_CHAR_W + 1), yPos, PAL_MOUSEPT, '0' + (ch % 10));
		}
	}
}

/* Draw block selection overlay (XOR palette indices to invert colors) */
static void writePatternBlockMark(ft2_pattern_editor_t *ed, ft2_instance_t *inst, 
                                   int32_t currRow, uint32_t rowHeight, const pattCoord_t *pattCoord)
{
	if (!ed || !ed->video || !inst) return;

	ft2_patt_mark_t *mark = &inst->editor.pattMark;
	if (mark->markY1 >= mark->markY2) return;

	const int32_t startCh = ed->channelOffset;
	const int32_t endCh = ed->channelOffset + (ed->numChannelsShown - 1);
	const int32_t startRow = currRow - pattCoord->numUpperRows;
	const int32_t endRow = currRow + pattCoord->numLowerRows;

	/* Test if pattern marking is outside of visible area */
	if (mark->markX1 > endCh || mark->markX2 < startCh || 
	    mark->markY1 > endRow || mark->markY2 < startRow)
		return;

	const markCoord_t *markCoord = &markCoordTable[ed->ptnStretch][ed->ptnChanScrollShown][ed->extendedPatternEditor];
	const int32_t pattYStart = markCoord->upperRowsY;

	/* X1 */
	int32_t x1 = 32 + ((mark->markX1 - ed->channelOffset) * ed->patternChannelWidth);
	if (x1 < 32)
		x1 = 32;

	/* X2 */
	int32_t x2 = (32 - 8) + (((mark->markX2 + 1) - ed->channelOffset) * ed->patternChannelWidth);
	if (x2 > 608)
		x2 = 608;

	/* Y1 */
	int32_t y1;
	if (mark->markY1 < currRow)
	{
		y1 = pattYStart + ((mark->markY1 - startRow) * rowHeight);
		if (y1 < pattYStart)
			y1 = pattYStart;
	}
	else if (mark->markY1 == currRow)
	{
		y1 = markCoord->midRowY;
	}
	else
	{
		y1 = markCoord->lowerRowsY + ((mark->markY1 - (currRow + 1)) * rowHeight);
	}

	/* Y2 */
	int32_t y2;
	if (mark->markY2 - 1 < currRow)
	{
		y2 = pattYStart + ((mark->markY2 - startRow) * rowHeight);
	}
	else if (mark->markY2 - 1 == currRow)
	{
		y2 = markCoord->midRowY + 11;
	}
	else
	{
		const int32_t pattYEnd = markCoord->lowerRowsY + (pattCoord->numLowerRows * rowHeight);
		y2 = markCoord->lowerRowsY + ((mark->markY2 - (currRow + 1)) * rowHeight);
		if (y2 > pattYEnd)
			y2 = pattYEnd;
	}

	/* Kludge for stretch + scroll */
	if (ed->ptnStretch && ed->ptnChanScrollShown)
	{
		if (y1 == (int32_t)pattCoord->upperRowsY - 1 || y1 == (int32_t)pattCoord->lowerRowsY - 1)
			y1++;

		if (y2 == 384)
			y2--;

		if (y1 >= y2)
			return;
	}

	/* Bounds check */
	if (x1 < 0 || x1 >= SCREEN_W || x2 < 0 || x2 >= SCREEN_W ||
	    y1 < 0 || y1 >= SCREEN_H || y2 < 0 || y2 >= SCREEN_H)
		return;

	/* Draw pattern mark */
	const int32_t w = x2 - x1;
	const int32_t h = y2 - y1;

	if (w <= 0 || h <= 0)
		return;

	if (x1 + w > SCREEN_W || y1 + h > SCREEN_H)
		return;

	uint32_t *ptr32 = &ed->video->frameBuffer[(y1 * SCREEN_W) + x1];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			ptr32[x] = ed->video->palette[(ptr32[x] >> 24) ^ 2]; /* XOR 2 to invert colors for mark */
		ptr32 += SCREEN_W;
	}
}

/* Draw cursor (XOR palette index ^4 for cursor color) */
static void writeCursor(ft2_pattern_editor_t *ed)
{
	if (!ed || !ed->video) return;

	const int32_t tabOffset = (ed->ptnShowVolColumn * 32) + (columnModeTab[ed->numChannelsShown - 1] * 8) + ed->cursor.object;
	int32_t xPos = pattCursorXTab[tabOffset];
	const int32_t width = pattCursorWTab[tabOffset];

	if (ed->ptnCursorY <= 0 || xPos <= 0 || width <= 0) return;
	xPos += ((ed->cursor.ch - ed->channelOffset) * ed->patternChannelWidth);
	if (xPos < 0 || xPos + width > SCREEN_W) return;

	uint32_t *dstPtr = &ed->video->frameBuffer[(ed->ptnCursorY * SCREEN_W) + xPos];
	for (int32_t y = 0; y < 9; y++, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < width; x++)
			dstPtr[x] = ed->video->palette[(dstPtr[x] >> 24) ^ 4];
}

/* ============ INITIALIZATION ============ */

void ft2_pattern_ed_init(ft2_pattern_editor_t *editor, ft2_video_t *video)
{
	if (!editor) return;
	memset(editor, 0, sizeof(ft2_pattern_editor_t));
	editor->video = video;
	editor->numChannelsShown = 8;
	editor->maxVisibleChannels = 8;
	editor->ptnCursorY = 283;
	editor->ptnShowVolColumn = true;
	editor->ptnHex = true;
	editor->ptnLineLight = true;
	editor->ptnChnNumbers = true;
	editor->ptnFrmWrk = true;
}

/* Set font pointers based on selected font style (0-3) */
void ft2_pattern_ed_update_font_ptrs(ft2_pattern_editor_t *editor, const ft2_bmp_t *bmp)
{
	if (!editor || !bmp || !bmp->font4) return;
	uint8_t fontIdx = (editor->ptnFont > 3) ? 0 : editor->ptnFont;
	editor->font4Ptr = &bmp->font4[fontIdx * (FONT4_WIDTH * FONT4_CHAR_H)];
	editor->font5Ptr = &bmp->font4[(4 + fontIdx) * (FONT4_WIDTH * FONT4_CHAR_H)];
}

/* ============ FRAMEWORK DRAWING ============ */

/* Draw pattern editor borders and row number boxes */
void ft2_pattern_ed_draw_borders(ft2_pattern_editor_t *ed, const ft2_bmp_t *bmp)
{
	if (!ed || !ed->video) return;
	ft2_video_t *video = ed->video;

	const pattCoord2_t *pattCoord = &pattCoord2Table[ed->ptnStretch][ed->ptnChanScrollShown][ed->extendedPatternEditor];
	ed->ptnCursorY = pattCoord->lowerRowsY - 9;

	/* Clamp and adjust channel count for proper framework widths */
	int32_t chans = ed->numChannelsShown;
	if (chans > ed->maxVisibleChannels) chans = ed->maxVisibleChannels;
	if (chans == 2) chans = 4;
	else if (chans == 10 && !ed->ptnShowVolColumn) chans = 12;
	if (chans < 2) chans = 2;
	if (chans > 12) chans = 12;
	const uint16_t chanWidth = chanWidths[(chans >> 1) - 1] + 2;

	if (ed->ptnChanScrollShown)
		drawFramework(video, 0, 383, 632, 17, FRAMEWORK_TYPE1);

	if (ed->ptnFrmWrk) {
	if (ed->extendedPatternEditor)
	{
		vLine(video, 0,   69, 330, PAL_DSKTOP1);
		vLine(video, 631, 68, 331, PAL_DSKTOP2);

		vLine(video, 1,   69, 330, PAL_DESKTOP);
		vLine(video, 630, 69, 330, PAL_DESKTOP);

		hLine(video, 0, 68, 631, PAL_DSKTOP1);
		hLine(video, 1, 69, 630, PAL_DESKTOP);

		if (!ed->ptnChanScrollShown)
		{
			hLine(video, 1, 398, 630, PAL_DESKTOP);
			hLine(video, 0, 399, 632, PAL_DSKTOP2);
		}
	}
	else
	{
		vLine(video, 0,   174, 225, PAL_DSKTOP1);
		vLine(video, 631, 173, 226, PAL_DSKTOP2);

		vLine(video, 1,   174, 225, PAL_DESKTOP);
		vLine(video, 630, 174, 225, PAL_DESKTOP);

		hLine(video, 0, 173, 631, PAL_DSKTOP1);
		hLine(video, 1, 174, 630, PAL_DESKTOP);

		if (!ed->ptnChanScrollShown)
		{
			hLine(video, 1, 398, 630, PAL_DESKTOP);
			hLine(video, 0, 399, 632, PAL_DSKTOP2);
		}
	}

	fillRect(video, 2, pattCoord->lowerRowsY - 9, 628, 9, PAL_DESKTOP);

	/* Row number boxes */
	drawFramework(video, 2, pattCoord->upperRowsY, 25, pattCoord->upperRowsH, FRAMEWORK_TYPE2);
	drawFramework(video, 604, pattCoord->upperRowsY, 26, pattCoord->upperRowsH, FRAMEWORK_TYPE2);
	drawFramework(video, 2, pattCoord->lowerRowsY, 25, pattCoord->lowerRowsH, FRAMEWORK_TYPE2);
	drawFramework(video, 604, pattCoord->lowerRowsY, 26, pattCoord->lowerRowsH, FRAMEWORK_TYPE2);

	/* Channel column frameworks */
	uint16_t xOffs = 28;
	for (int32_t i = 0; i < chans; i++)
	{
		vLine(video, xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
		vLine(video, xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);
		drawFramework(video, xOffs, pattCoord->upperRowsY, chanWidth, pattCoord->upperRowsH, FRAMEWORK_TYPE2);
		drawFramework(video, xOffs, pattCoord->lowerRowsY, chanWidth, pattCoord->lowerRowsH, FRAMEWORK_TYPE2);
		xOffs += chanWidth + 1;
	}
	vLine(video, xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
	vLine(video, xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);
	} else {
		/* No framework - clear to black, draw current row bar only */
		const int32_t clearY = ed->extendedPatternEditor ? 68 : 173;
		const int32_t clearHeight = ed->ptnChanScrollShown ? (ed->extendedPatternEditor ? 315 : 210) : (ed->extendedPatternEditor ? 332 : 227);
		fillRect(video, 0, clearY, SCREEN_W, clearHeight, PAL_BCKGRND);
		drawFramework(video, 0, pattCoord->lowerRowsY - 10, SCREEN_W, 11, FRAMEWORK_TYPE1);
	}
	(void)bmp;
}

/* ============ PATTERN DATA RENDERING ============ */

/* Draw all visible pattern rows */
void ft2_pattern_ed_write_pattern(ft2_pattern_editor_t *ed, const ft2_bmp_t *bmp, ft2_instance_t *inst)
{
	if (!ed || !ed->video || !inst) return;

	ft2_video_t *video = ed->video;
	int32_t currRow = ed->currRow, currPattern = ed->currPattern;

	uint32_t chans = ed->numChannelsShown;
	if (chans > ed->maxVisibleChannels) chans = ed->maxVisibleChannels;
	if (chans < 2) chans = 2;
	if (chans > 12) chans = 12;

	const uint32_t chanWidth = chanWidths[(chans / 2) - 1];
	ed->patternChannelWidth = (uint16_t)(chanWidth + 3);

	uint32_t rowHeight = ed->ptnStretch ? 11 : 8;
	const pattCoord_t *pattCoord = &pattCoordTable[ed->ptnStretch][ed->ptnChanScrollShown][ed->extendedPatternEditor];
	const pattCoord2_t *pattCoord2 = &pattCoord2Table[ed->ptnStretch][ed->ptnChanScrollShown][ed->extendedPatternEditor];
	const int32_t midRowTextY = pattCoord->midRowTextY;
	const int32_t lowerRowsTextY = pattCoord->lowerRowsTextY;
	int32_t row = currRow - pattCoord->numUpperRows;
	const int32_t rowsOnScreen = pattCoord->numUpperRows + 1 + pattCoord->numLowerRows;
	int32_t textY = pattCoord->upperRowsTextY;
	const int32_t afterCurrRow = currRow + 1;
	const int32_t numChannels = ed->numChannelsShown;

	ft2_note_t *pattPtr = NULL;
	int32_t numRows = 64;
	if (currPattern >= 0 && currPattern < FT2_MAX_PATTERNS) {
		pattPtr = inst->replayer.pattern[currPattern];
		numRows = inst->replayer.patternNumRows[currPattern];
		if (numRows <= 0) numRows = 64;
	}
	if (pattPtr) pattPtr += ed->channelOffset;

	uint32_t noteTextColors[2] = { video->palette[PAL_PATTEXT], video->palette[PAL_FORGRND] };
	for (int32_t i = 0; i < rowsOnScreen; i++)
	{
		if (row >= 0 && row < numRows)
		{
			const bool selectedRowFlag = (row == currRow);

			drawRowNums(ed, textY, (uint8_t)row, selectedRowFlag, bmp);

			const ft2_note_t *p = (pattPtr == NULL) ? &inst->replayer.nilPatternLine[0] : &pattPtr[(uint32_t)row * FT2_MAX_CHANNELS];
			const int32_t xWidth = ed->patternChannelWidth;
			const uint32_t color = noteTextColors[selectedRowFlag];

			int32_t xPos = 29;
			for (int32_t j = 0; j < numChannels; j++, p++, xPos += xWidth)
			{
				/* Draw note */
				int16_t note = p->note;
				if (ed->ptnShowVolColumn)
				{
					/* With volume column: <=4 Big, >4 Medium (no Small) */
				if (ed->numChannelsShown <= 4)
				{
					if (note <= 0 || note > 97)
						drawEmptyNoteBig(ed, xPos + 3, textY, color, bmp);
					else if (note == NOTE_OFF)
						drawKeyOffBig(ed, xPos + 3, textY, color, bmp);
					else
						drawNoteBig(ed, xPos + 3, textY, note, color, bmp);
				}
					else
				{
					if (note <= 0 || note > 97)
						drawEmptyNoteMedium(ed, xPos + 3, textY, color, bmp);
					else if (note == NOTE_OFF)
						drawKeyOffMedium(ed, xPos + 3, textY, color, bmp);
					else
						drawNoteMedium(ed, xPos + 3, textY, note, color, bmp);
				}
				}
				else
				{
					/* Without volume column: <=6 Big, <=8 Medium, >8 Small */
					if (ed->numChannelsShown <= 6)
					{
						if (note <= 0 || note > 97)
							drawEmptyNoteBig(ed, xPos + 3, textY, color, bmp);
						else if (note == NOTE_OFF)
							drawKeyOffBig(ed, xPos + 3, textY, color, bmp);
						else
							drawNoteBig(ed, xPos + 3, textY, note, color, bmp);
					}
					else if (ed->numChannelsShown <= 8)
					{
						if (note <= 0 || note > 97)
							drawEmptyNoteMedium(ed, xPos + 3, textY, color, bmp);
						else if (note == NOTE_OFF)
							drawKeyOffMedium(ed, xPos + 3, textY, color, bmp);
						else
							drawNoteMedium(ed, xPos + 3, textY, note, color, bmp);
				}
				else
				{
					if (note <= 0 || note > 97)
						drawEmptyNoteSmall(ed, xPos + 3, textY, color, bmp);
					else if (note == NOTE_OFF)
						drawKeyOffSmall(ed, xPos + 3, textY, color, bmp);
					else
						drawNoteSmall(ed, xPos + 3, textY, note, color, bmp);
					}
				}

				/* Draw instrument */
				uint8_t ins = p->instr;
				if (ins > 0 || ed->ptnInstrZero)
				{
					uint8_t fontType;
					uint8_t charW;
					int32_t instrX;

					if (ed->ptnShowVolColumn)
					{
						/* With volume column */
					if (ed->numChannelsShown <= 4)
					{
						fontType = FONT_TYPE4;
						charW = FONT4_CHAR_W;
						instrX = xPos + 67;
					}
					else if (ed->numChannelsShown <= 6)
					{
						fontType = FONT_TYPE4;
						charW = FONT4_CHAR_W;
						instrX = xPos + 27;
					}
					else
					{
						fontType = FONT_TYPE3;
						charW = FONT3_CHAR_W;
						instrX = xPos + 31;
						}
					}
					else
					{
						/* Without volume column */
						if (ed->numChannelsShown <= 4)
						{
							fontType = FONT_TYPE5;
							charW = FONT5_CHAR_W;
							instrX = xPos + 59;
						}
						else if (ed->numChannelsShown <= 6)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							instrX = xPos + 51;
						}
						else if (ed->numChannelsShown <= 8)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							instrX = xPos + 27;
						}
						else
						{
							fontType = FONT_TYPE3;
							charW = FONT3_CHAR_W;
							instrX = xPos + 23;
						}
					}

					if (ed->ptnInstrZero)
					{
						pattCharOut(ed, instrX, textY, ins >> 4, fontType, color, bmp);
						pattCharOut(ed, instrX + charW, textY, ins & 0x0F, fontType, color, bmp);
					}
					else
					{
						const uint8_t chr1 = ins >> 4;
						const uint8_t chr2 = ins & 0x0F;

						if (chr1 > 0)
							pattCharOut(ed, instrX, textY, chr1, fontType, color, bmp);

						if (chr1 > 0 || chr2 > 0)
							pattCharOut(ed, instrX + charW, textY, chr2, fontType, color, bmp);
					}
				}

				/* Draw volume column (if visible) - always draw, even when empty */
				if (ed->ptnShowVolColumn)
				{
					uint8_t vol = p->vol;
					uint8_t fontType;
					uint8_t charW;
					int32_t volX;
					uint8_t char1, char2;

					if (ed->numChannelsShown <= 4)
					{
						fontType = FONT_TYPE4;
						charW = FONT4_CHAR_W;
						volX = xPos + 91;
						char1 = vol2charTab1[vol >> 4];
						char2 = (vol < 0x10) ? 39 : (vol & 0x0F);
					}
					else if (ed->numChannelsShown <= 6)
					{
						fontType = FONT_TYPE4;
						charW = FONT4_CHAR_W;
						volX = xPos + 51;
						char1 = vol2charTab1[vol >> 4];
						char2 = (vol < 0x10) ? 39 : (vol & 0x0F);
					}
					else
					{
						fontType = FONT_TYPE3;
						charW = FONT3_CHAR_W;
						volX = xPos + 43;
						char1 = vol2charTab2[vol >> 4];
						char2 = (vol < 0x10) ? 42 : (vol & 0x0F);
					}

					pattCharOut(ed, volX, textY, char1, fontType, color, bmp);
					pattCharOut(ed, volX + charW, textY, char2, fontType, color, bmp);
				}

				/* Draw effect - always draw, even when empty */
				{
					uint8_t efx = p->efx;
					uint8_t efxData = p->efxData;
					uint8_t fontType;
					uint8_t charW;
					int32_t efxX;

					if (ed->ptnShowVolColumn)
					{
						if (ed->numChannelsShown <= 4)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							efxX = xPos + 115;
						}
						else if (ed->numChannelsShown <= 6)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							efxX = xPos + 67;
						}
						else
						{
							fontType = FONT_TYPE3;
							charW = FONT3_CHAR_W;
							efxX = xPos + 55;
						}
					}
					else
					{
						if (ed->numChannelsShown <= 4)
						{
							fontType = FONT_TYPE5;
							charW = FONT5_CHAR_W;
							efxX = xPos + 91;
						}
						else if (ed->numChannelsShown <= 6)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							efxX = xPos + 67;
						}
						else if (ed->numChannelsShown <= 8)
						{
							fontType = FONT_TYPE4;
							charW = FONT4_CHAR_W;
							efxX = xPos + 43;
						}
						else
						{
							fontType = FONT_TYPE3;
							charW = FONT3_CHAR_W;
							efxX = xPos + 31;
						}
					}

					pattCharOut(ed, efxX, textY, efx, fontType, color, bmp);
					pattCharOut(ed, efxX + charW, textY, efxData >> 4, fontType, color, bmp);
					pattCharOut(ed, efxX + (charW * 2), textY, efxData & 0x0F, fontType, color, bmp);
				}
			}
		}

		/* Next row */
		if (++row >= numRows)
			break;

		/* Adjust textY position */
		if (row == currRow)
			textY = midRowTextY;
		else if (row == afterCurrRow)
			textY = lowerRowsTextY;
		else
			textY += rowHeight;
	}

	/* Draw cursor */
	writeCursor(ed);

	/* Draw pattern marking (if anything is marked) */
	if (inst->editor.pattMark.markY1 != inst->editor.pattMark.markY2)
		writePatternBlockMark(ed, inst, currRow, rowHeight, pattCoord);

	/* Channel numbers must be drawn lastly */
	if (ed->ptnChnNumbers)
		drawChannelNumbering(ed, pattCoord2->upperRowsY + 2, bmp);
}

void ft2_pattern_ed_draw(ft2_pattern_editor_t *editor, const ft2_bmp_t *bmp, ft2_instance_t *instance)
{
	if (editor == NULL || editor->video == NULL)
		return;

	/* Update font pointers */
	ft2_pattern_ed_update_font_ptrs(editor, bmp);

	/* Sync current row/pattern from instance if available */
	if (instance != NULL)
	{
		editor->currRow = instance->replayer.song.row;
		editor->currPattern = instance->replayer.song.pattNum;
		
		/* Calculate visible channels properly - clamp to maxVisibleChannels */
		uint8_t maxVisible = getMaxVisibleChannels(instance);
		uint8_t songChannels = instance->replayer.song.numChannels;
		
		/* If song has more channels than we can display, show the max and enable scrollbar */
		if (songChannels > maxVisible)
		{
			editor->numChannelsShown = maxVisible;
			editor->ptnChanScrollShown = true;
			instance->uiState.pattChanScrollShown = true;
		}
		else
		{
			editor->numChannelsShown = songChannels;
			editor->ptnChanScrollShown = false;
			instance->uiState.pattChanScrollShown = false;
		}
		
		/* Ensure minimum 2 channels */
		if (editor->numChannelsShown < 2)
			editor->numChannelsShown = 2;
		
		/* Sync back to instance UI state */
		instance->uiState.numChannelsShown = editor->numChannelsShown;
		
		/* Get channel offset from instance */
		editor->channelOffset = instance->uiState.channelOffset;
		
		/* Clamp channel offset if needed */
		if (editor->ptnChanScrollShown)
		{
			if (editor->channelOffset > songChannels - editor->numChannelsShown)
				editor->channelOffset = songChannels - editor->numChannelsShown;
		}
		else
		{
			editor->channelOffset = 0;
		}
		
		/* Update instance with clamped offset */
		instance->uiState.channelOffset = editor->channelOffset;
		
		/* Calculate channel width */
		if (editor->numChannelsShown >= 2 && editor->numChannelsShown <= 12)
			editor->patternChannelWidth = chanWidths[(editor->numChannelsShown / 2) - 1] + 3;
		else
			editor->patternChannelWidth = 75; /* Default */
		
		instance->uiState.patternChannelWidth = editor->patternChannelWidth;
		
		/* Sync pattern display settings from uiState */
		editor->ptnShowVolColumn = instance->uiState.ptnShowVolColumn;
		editor->ptnStretch = instance->uiState.ptnStretch;
		editor->ptnHex = instance->uiState.ptnHex;
		editor->ptnLineLight = instance->uiState.ptnLineLight;
		editor->ptnChnNumbers = instance->uiState.ptnChnNumbers;
		editor->ptnInstrZero = instance->uiState.ptnInstrZero;
		editor->ptnAcc = instance->uiState.ptnAcc;
		editor->ptnFrmWrk = instance->uiState.ptnFrmWrk;
		editor->ptnFont = instance->uiState.ptnFont;
		editor->maxVisibleChannels = instance->uiState.maxVisibleChannels;
		editor->extendedPatternEditor = instance->uiState.extendedPatternEditor;
		
		/* Sync cursor position from instance */
		editor->cursor.ch = instance->cursor.ch;
		editor->cursor.object = instance->cursor.object;
	}

	/* Draw pattern borders framework */
	ft2_pattern_ed_draw_borders(editor, bmp);

	/* Draw the pattern data */
	ft2_pattern_ed_write_pattern(editor, bmp, instance);
	
	/* Show or hide channel scrollbar and buttons based on ptnChanScrollShown */
	if (editor->ptnChanScrollShown && instance != NULL && instance->ui != NULL)
	{
		ft2_widgets_t *widgets = &((ft2_ui_t *)instance->ui)->widgets;
		/* Scrollbar and buttons should be visible */
		showScrollBar(widgets, editor->video, SB_CHAN_SCROLL);
		showPushButton(widgets, editor->video, bmp, PB_CHAN_SCROLL_LEFT);
		showPushButton(widgets, editor->video, bmp, PB_CHAN_SCROLL_RIGHT);
		/* Update scrollbar range to reflect number of channels */
		setScrollBarEnd(instance, widgets, editor->video, SB_CHAN_SCROLL, instance->replayer.song.numChannels);
		setScrollBarPageLength(instance, widgets, editor->video, SB_CHAN_SCROLL, instance->uiState.numChannelsShown);
	}
	else if (instance != NULL && instance->ui != NULL)
	{
		ft2_widgets_t *widgets = &((ft2_ui_t *)instance->ui)->widgets;
		hideScrollBar(widgets, SB_CHAN_SCROLL);
		hidePushButton(widgets, PB_CHAN_SCROLL_LEFT);
		hidePushButton(widgets, PB_CHAN_SCROLL_RIGHT);
	}
}

/* ============ PATTERN MEMORY ============ */

/* Allocate pattern if not already allocated */
bool allocatePattern(ft2_instance_t *inst, uint16_t pattNum)
{
	if (!inst || pattNum >= FT2_MAX_PATTERNS) return false;
	if (inst->replayer.pattern[pattNum]) return true;

	inst->replayer.pattern[pattNum] = (ft2_note_t *)calloc((MAX_PATT_LEN * MAX_CHANNELS) + 16, sizeof(ft2_note_t));
	if (!inst->replayer.pattern[pattNum]) return false;
	inst->replayer.song.currNumRows = inst->replayer.patternNumRows[pattNum];
	return true;
}

/* Check if pattern contains no data */
bool patternEmpty(ft2_instance_t *inst, uint16_t pattNum)
{
	if (!inst || pattNum >= FT2_MAX_PATTERNS) return true;
	ft2_note_t *p = inst->replayer.pattern[pattNum];
	if (!p) return true;

	uint16_t numRows = inst->replayer.patternNumRows[pattNum];
	for (int32_t row = 0; row < numRows; row++) {
		for (int32_t ch = 0; ch < inst->replayer.song.numChannels; ch++) {
			ft2_note_t *n = &p[(row * MAX_CHANNELS) + ch];
			if (n->note || n->instr || n->vol || n->efx || n->efxData) return false;
		}
	}
	return true;
}

/* Free pattern memory if empty */
void killPatternIfUnused(ft2_instance_t *inst, uint16_t pattNum)
{
	if (!inst || pattNum >= FT2_MAX_PATTERNS) return;
	if (patternEmpty(inst, pattNum) && inst->replayer.pattern[pattNum]) {
		free(inst->replayer.pattern[pattNum]);
		inst->replayer.pattern[pattNum] = NULL;
	}
}

/* Max visible channels based on config and volume column visibility */
uint8_t getMaxVisibleChannels(ft2_instance_t *inst)
{
	if (!inst) return 8;
	static const uint8_t maxWithVol[4] = { 4, 6, 8, 8 };   /* With vol column: max 8 */
	static const uint8_t maxNoVol[4]   = { 4, 6, 8, 12 };  /* Without: max 12 */
	uint8_t idx = inst->config.ptnMaxChannels;
	if (idx > 3) idx = 2;
	return inst->uiState.ptnShowVolColumn ? maxWithVol[idx] : maxNoVol[idx];
}

/* Recalculate channel width from visible channel count */
void updatePatternWidth(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t maxVisible = getMaxVisibleChannels(inst);
	if (inst->uiState.numChannelsShown > maxVisible) inst->uiState.numChannelsShown = maxVisible;
	if (inst->uiState.numChannelsShown < 2) inst->uiState.numChannelsShown = 2;
	inst->uiState.patternChannelWidth = chanWidths[(inst->uiState.numChannelsShown / 2) - 1] + 3;
}

/* Update visible channels and scrollbar state after song channel count changes */
void updateChanNums(ft2_instance_t *inst)
{
	if (!inst) return;

	uint8_t songCh = inst->replayer.song.numChannels;
	uint8_t maxCh = getMaxVisibleChannels(inst);
	inst->uiState.numChannelsShown = (songCh > maxCh) ? maxCh : songCh;
	inst->uiState.pattChanScrollShown = (songCh > maxCh);

	if (inst->uiState.patternEditorShown && inst->uiState.channelOffset > songCh - inst->uiState.numChannelsShown)
		inst->uiState.channelOffset = 0;

	inst->uiState.patternChannelWidth = (inst->uiState.numChannelsShown >= 2 && inst->uiState.numChannelsShown <= 12)
		? chanWidths[(inst->uiState.numChannelsShown / 2) - 1] + 3 : 75;
	inst->uiState.updatePatternEditor = true;
}

/* ============ CURSOR NAVIGATION ============ */

/* Move cursor to previous channel (wraps around) */
void cursorChannelLeft(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->cursor.object = CURSOR_EFX2;

	if (inst->cursor.ch == 0) {
		inst->cursor.ch = inst->replayer.song.numChannels - 1;
		if (inst->uiState.pattChanScrollShown) {
			inst->uiState.channelOffset = inst->replayer.song.numChannels - inst->uiState.numChannelsShown;
			inst->uiState.updateChanScrollPos = true;
		}
	} else {
		inst->cursor.ch--;
		if (inst->uiState.pattChanScrollShown && inst->cursor.ch < inst->uiState.channelOffset) {
			inst->uiState.channelOffset--;
			inst->uiState.updateChanScrollPos = true;
		}
	}
	inst->uiState.updatePatternEditor = true;
}

/* Move cursor to next channel (wraps around) */
void cursorChannelRight(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->cursor.object = CURSOR_NOTE;

	if (inst->cursor.ch >= inst->replayer.song.numChannels - 1) {
		inst->cursor.ch = 0;
		if (inst->uiState.pattChanScrollShown) {
			inst->uiState.channelOffset = 0;
			inst->uiState.updateChanScrollPos = true;
		}
	} else {
		inst->cursor.ch++;
		if (inst->uiState.pattChanScrollShown && inst->cursor.ch >= inst->uiState.channelOffset + inst->uiState.numChannelsShown) {
			inst->uiState.channelOffset++;
			inst->uiState.updateChanScrollPos = true;
		}
	}
	inst->uiState.updatePatternEditor = true;
}

void cursorTabLeft(ft2_instance_t *inst)
{
	if (!inst) return;
	if (inst->cursor.object == CURSOR_NOTE) cursorChannelLeft(inst);
	inst->cursor.object = CURSOR_NOTE;
	inst->uiState.updatePatternEditor = true;
}

void cursorTabRight(ft2_instance_t *inst)
{
	if (!inst) return;
	cursorChannelRight(inst);
	inst->cursor.object = CURSOR_NOTE;
	inst->uiState.updatePatternEditor = true;
}

void chanLeft(ft2_instance_t *inst)  { if (inst) { cursorChannelLeft(inst); inst->cursor.object = CURSOR_NOTE; } }
void chanRight(ft2_instance_t *inst) { if (inst) { cursorChannelRight(inst); inst->cursor.object = CURSOR_NOTE; } }

/* Move cursor to previous field (skips vol column if hidden) */
void cursorLeft(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->cursor.object--;
	if (!inst->uiState.ptnShowVolColumn)
		while (inst->cursor.object == CURSOR_VOL1 || inst->cursor.object == CURSOR_VOL2)
			inst->cursor.object--;
	if (inst->cursor.object < 0) {
		inst->cursor.object = CURSOR_EFX2;
		cursorChannelLeft(inst);
	}
	inst->uiState.updatePatternEditor = true;
}

/* Move cursor to next field (skips vol column if hidden) */
void cursorRight(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->cursor.object++;
	if (!inst->uiState.ptnShowVolColumn)
		while (inst->cursor.object == CURSOR_VOL1 || inst->cursor.object == CURSOR_VOL2)
			inst->cursor.object++;
	if (inst->cursor.object > CURSOR_EFX2) {
		inst->cursor.object = CURSOR_NOTE;
		cursorChannelRight(inst);
	}
	inst->uiState.updatePatternEditor = true;
}

/* ============ ROW NAVIGATION ============ */

void rowOneUpWrap(ft2_instance_t *inst)
{
	if (!inst || inst->replayer.song.currNumRows <= 0) return;
	inst->replayer.song.row = (inst->replayer.song.row - 1 + inst->replayer.song.currNumRows) % inst->replayer.song.currNumRows;
	if (!inst->replayer.songPlaying) {
		inst->editor.row = (uint8_t)inst->replayer.song.row;
		inst->uiState.updatePatternEditor = true;
	}
}

void rowOneDownWrap(ft2_instance_t *inst)
{
	if (!inst) return;
	if (inst->replayer.songPlaying) {
		inst->replayer.song.tick = 2;
	} else if (inst->replayer.song.currNumRows > 0) {
		inst->replayer.song.row = (inst->replayer.song.row + 1) % inst->replayer.song.currNumRows;
		inst->editor.row = (uint8_t)inst->replayer.song.row;
		inst->uiState.updatePatternEditor = true;
	}
}

void rowUp(ft2_instance_t *inst, uint16_t amount)
{
	if (!inst) return;
	inst->replayer.song.row -= amount;
	if (inst->replayer.song.row < 0) inst->replayer.song.row = 0;
	if (!inst->replayer.songPlaying) {
		inst->editor.row = (uint8_t)inst->replayer.song.row;
		inst->uiState.updatePatternEditor = true;
	}
}

void rowDown(ft2_instance_t *inst, uint16_t amount)
{
	if (!inst) return;
	inst->replayer.song.row += amount;
	if (inst->replayer.song.row >= inst->replayer.song.currNumRows)
		inst->replayer.song.row = inst->replayer.song.currNumRows - 1;
	if (!inst->replayer.songPlaying) {
		inst->editor.row = (uint8_t)inst->replayer.song.row;
		inst->uiState.updatePatternEditor = true;
	}
}

/* ============ PATTERN MARKING ============ */

void clearPattMark(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->editor.pattMark.markX1 = inst->editor.pattMark.markX2 = 0;
	inst->editor.pattMark.markY1 = inst->editor.pattMark.markY2 = 0;
}

/* Clamp mark coordinates to pattern/channel bounds */
void checkMarkLimits(ft2_instance_t *inst)
{
	if (!inst) return;
	int16_t limitY = inst->replayer.patternNumRows[inst->editor.editPattern];
	int16_t limitX = inst->replayer.song.numChannels - 1;

	if (inst->editor.pattMark.markY1 < 0) inst->editor.pattMark.markY1 = 0;
	if (inst->editor.pattMark.markY1 > limitY) inst->editor.pattMark.markY1 = limitY;
	if (inst->editor.pattMark.markY2 < 0) inst->editor.pattMark.markY2 = 0;
	if (inst->editor.pattMark.markY2 > limitY) inst->editor.pattMark.markY2 = limitY;
	if (inst->editor.pattMark.markX1 < 0) inst->editor.pattMark.markX1 = 0;
	if (inst->editor.pattMark.markX1 > limitX) inst->editor.pattMark.markX1 = limitX;
	if (inst->editor.pattMark.markX2 < 0) inst->editor.pattMark.markX2 = 0;
	if (inst->editor.pattMark.markX2 > limitX) inst->editor.pattMark.markX2 = limitX;
	if (inst->editor.pattMark.markX1 > inst->editor.pattMark.markX2)
		inst->editor.pattMark.markX1 = inst->editor.pattMark.markX2;
}

/* Keyboard mark expansion helpers - start marking from cursor if not already */
#define START_MARK_Y(inst) do { \
	if (inst->editor.pattMark.markY1 == inst->editor.pattMark.markY2) { \
		inst->editor.pattMark.markY1 = inst->editor.pattMark.markY2 = inst->replayer.song.row; \
		inst->editor.pattMark.markX1 = inst->editor.pattMark.markX2 = inst->cursor.ch; \
	} \
} while(0)

#define START_MARK_X(inst) do { \
	if (inst->editor.pattMark.markX1 == inst->editor.pattMark.markX2) { \
		inst->editor.pattMark.markX1 = inst->editor.pattMark.markX2 = inst->cursor.ch; \
		inst->editor.pattMark.markY1 = inst->editor.pattMark.markY2 = inst->replayer.song.row; \
	} \
} while(0)

void keybPattMarkUp(ft2_instance_t *inst)
{
	if (!inst || inst->replayer.song.row == 0) return;
	START_MARK_Y(inst);
	inst->editor.pattMark.markY2--;
	rowOneUpWrap(inst);
	checkMarkLimits(inst);
	inst->uiState.updatePatternEditor = true;
}

void keybPattMarkDown(ft2_instance_t *inst)
{
	if (!inst) return;
	if (inst->replayer.song.row >= inst->replayer.patternNumRows[inst->editor.editPattern] - 1) return;
	START_MARK_Y(inst);
	inst->editor.pattMark.markY2++;
	rowOneDownWrap(inst);
	checkMarkLimits(inst);
	inst->uiState.updatePatternEditor = true;
}

void keybPattMarkLeft(ft2_instance_t *inst)
{
	if (!inst || inst->cursor.ch == 0) return;
	START_MARK_X(inst);
	inst->editor.pattMark.markX2--;
	chanLeft(inst);
	checkMarkLimits(inst);
	inst->uiState.updatePatternEditor = true;
}

void keybPattMarkRight(ft2_instance_t *inst)
{
	if (!inst || inst->cursor.ch >= inst->replayer.song.numChannels - 1) return;
	START_MARK_X(inst);
	inst->editor.pattMark.markX2++;
	chanRight(inst);
	checkMarkLimits(inst);
	inst->uiState.updatePatternEditor = true;
}

#undef START_MARK_Y
#undef START_MARK_X

/* ============ BLOCK OPERATIONS ============ */

/* Copy note applying copy mask (Adv.Edit feature) */
static void copyNoteWithMask(ft2_instance_t *inst, ft2_note_t *src, ft2_note_t *dst)
{
	if (!inst->editor.copyMaskEnable) { *dst = *src; return; }
	if (inst->editor.copyMask[0]) dst->note = src->note;
	if (inst->editor.copyMask[1]) dst->instr = src->instr;
	if (inst->editor.copyMask[2]) dst->vol = src->vol;
	if (inst->editor.copyMask[3]) dst->efx = src->efx;
	if (inst->editor.copyMask[4]) dst->efxData = src->efxData;
}

/* Paste note applying copy+transp masks (transp mask = skip if src is 0) */
static void pasteNoteWithMask(ft2_instance_t *inst, ft2_note_t *src, ft2_note_t *dst)
{
	if (!inst->editor.copyMaskEnable) { *dst = *src; return; }
	if (inst->editor.copyMask[0] && (src->note || !inst->editor.transpMask[0])) dst->note = src->note;
	if (inst->editor.copyMask[1] && (src->instr || !inst->editor.transpMask[1])) dst->instr = src->instr;
	if (inst->editor.copyMask[2] && (src->vol || !inst->editor.transpMask[2])) dst->vol = src->vol;
	if (inst->editor.copyMask[3] && (src->efx || !inst->editor.transpMask[3])) dst->efx = src->efx;
	if (inst->editor.copyMask[4] && (src->efxData || !inst->editor.transpMask[4])) dst->efxData = src->efxData;
}

void cutBlock(ft2_instance_t *inst)
{
	if (!inst) return;

	uint16_t curPattern = inst->editor.editPattern;
	int32_t markX1 = inst->editor.pattMark.markX1, markX2 = inst->editor.pattMark.markX2;
	int32_t markY1 = inst->editor.pattMark.markY1, markY2 = inst->editor.pattMark.markY2;
	const int16_t numRows = inst->replayer.patternNumRows[curPattern];
	const int16_t numCh = inst->replayer.song.numChannels;

	if (markY1 >= markY2) return;
	if (markX1 > numCh - 1) markX1 = numCh - 1;
	if (markX2 > numCh - 1) markX2 = numCh - 1;
	if (markX2 < markX1) markX2 = markX1;
	if (markY1 >= numRows) markY1 = numRows - 1;
	if (markY2 > numRows) markY2 = numRows - markY1;

	ft2_note_t *p = inst->replayer.pattern[curPattern];
	if (!p || markY1 < 0 || markX1 < 0) return;

	for (int32_t x = markX1; x <= markX2; x++) {
		for (int32_t y = markY1; y < markY2; y++) {
			ft2_note_t *n = &p[(y * FT2_MAX_CHANNELS) + x];
			if (inst->config.ptnCutToBuffer)
				copyNoteWithMask(inst, n, &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);
			memset(n, 0, sizeof(ft2_note_t));
		}
	}
	killPatternIfUnused(inst, curPattern);
	if (inst->config.ptnCutToBuffer) {
		markXSize = markX2 - markX1;
		markYSize = markY2 - markY1;
		blockCopied = true;
	}
	inst->uiState.updatePatternEditor = true;
}

void copyBlock(ft2_instance_t *inst)
{
	if (!inst) return;

	uint16_t curPattern = inst->editor.editPattern;
	int32_t markX1 = inst->editor.pattMark.markX1, markX2 = inst->editor.pattMark.markX2;
	int32_t markY1 = inst->editor.pattMark.markY1, markY2 = inst->editor.pattMark.markY2;
	const int16_t numRows = inst->replayer.patternNumRows[curPattern];
	const int16_t numCh = inst->replayer.song.numChannels;

	if (markY1 >= markY2) return;
	if (markX1 > numCh - 1) markX1 = numCh - 1;
	if (markX2 > numCh - 1) markX2 = numCh - 1;
	if (markX2 < markX1) markX2 = markX1;
	if (markY1 >= numRows) markY1 = numRows - 1;
	if (markY2 > numRows) markY2 = numRows - markY1;

	ft2_note_t *p = inst->replayer.pattern[curPattern];
	if (!p || markY1 < 0 || markX1 < 0) return;

	for (int32_t x = markX1; x <= markX2; x++)
		for (int32_t y = markY1; y < markY2; y++)
			copyNoteWithMask(inst, &p[(y * FT2_MAX_CHANNELS) + x], &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);

	markXSize = markX2 - markX1;
	markYSize = markY2 - markY1;
	blockCopied = true;
}

void pasteBlock(ft2_instance_t *inst)
{
	if (!inst) return;

	uint16_t curPattern = inst->editor.editPattern;
	if (!blockCopied || !allocatePattern(inst, curPattern)) return;

	int32_t chStart = inst->cursor.ch, rowStart = inst->replayer.song.row;
	const int16_t numRows = inst->replayer.patternNumRows[curPattern];
	const int16_t numCh = inst->replayer.song.numChannels;

	if (chStart >= numCh) chStart = numCh - 1;
	if (rowStart >= numRows) rowStart = numRows - 1;

	int32_t markedChannels = markXSize + 1, markedRows = markYSize;
	if (chStart + markedChannels > numCh) markedChannels = numCh - chStart;
	if (rowStart + markedRows > numRows) markedRows = numRows - rowStart;

	if (markedChannels > 0 && markedRows > 0) {
		ft2_note_t *p = inst->replayer.pattern[curPattern];
		for (int32_t x = chStart; x < chStart + markedChannels; x++)
			for (int32_t y = rowStart; y < rowStart + markedRows; y++)
				pasteNoteWithMask(inst, &blkCopyBuff[((y - rowStart) * MAX_CHANNELS) + (x - chStart)], &p[(y * FT2_MAX_CHANNELS) + x]);
	}
	killPatternIfUnused(inst, curPattern);
	inst->uiState.updatePatternEditor = true;
}

/* ============ MOUSE MARKING ============ */

static int8_t mouseXToCh(ft2_instance_t *inst, int32_t mouseX)
{
	if (!inst || !inst->uiState.patternChannelWidth) return 0;
	int32_t mx = mouseX - 29;
	if (mx < 0) mx = 0; if (mx > 573) mx = 573;
	int8_t ch = inst->uiState.channelOffset + (int8_t)(mx / inst->uiState.patternChannelWidth);
	int8_t chEnd = inst->uiState.channelOffset + inst->uiState.numChannelsShown - 1;
	if (ch < 0) ch = 0; if (ch > chEnd) ch = chEnd;
	if (ch >= inst->replayer.song.numChannels) ch = inst->replayer.song.numChannels - 1;
	return ch;
}

static int16_t mouseYToRow(ft2_instance_t *inst, int32_t mouseY)
{
	if (!inst) return 0;
	const pattCoordsMouse_t *pm = &pattCoordMouseTable[inst->uiState.ptnStretch][inst->uiState.pattChanScrollShown][inst->uiState.extendedPatternEditor];
	const int16_t maxY = inst->uiState.pattChanScrollShown ? 382 : 396;
	int16_t my = (int16_t)mouseY;
	if (my < (int16_t)pm->upperRowsY) my = pm->upperRowsY;
	if (my > maxY) my = maxY;

	const uint8_t charH = inst->uiState.ptnStretch ? 11 : 8;
	const int16_t currRow = inst->replayer.song.row;

	if (my < (int16_t)pm->midRowY) {
		int16_t row = currRow - (pm->numUpperRows - ((my - pm->upperRowsY) / charH));
		return (row < 0) ? 0 : row;
	} else if (my <= (int16_t)pm->midRowY + 10) {
		return currRow;
	} else {
		int16_t row = (currRow + 1) + ((my - pm->lowerRowsY) / charH);
		int16_t patLen = inst->replayer.patternNumRows[inst->editor.editPattern];
		return (row >= patLen) ? patLen - 1 : row;
	}
}

/* Handle mouse click/drag for block marking */
void handlePatternDataMouseDown(ft2_instance_t *inst, int32_t mouseX, int32_t mouseY, bool mouseButtonHeld, bool rightButton)
{
	if (!inst) return;

	/* Right-click clears marking (plugin extension, not in original FT2) */
	if (rightButton) { clearPattMark(inst); inst->uiState.updatePatternEditor = true; return; }

	if (!mouseButtonHeld) {
		/* First click - start marking */
		lastMouseX = mouseX; lastMouseY = mouseY;
		lastChMark = mouseXToCh(inst, mouseX);
		lastRowMark = mouseYToRow(inst, mouseY);
		inst->editor.pattMark.markX1 = inst->editor.pattMark.markX2 = lastChMark;
		inst->editor.pattMark.markY1 = lastRowMark;
		inst->editor.pattMark.markY2 = lastRowMark + 1;
		checkMarkLimits(inst);
		lastMarkX1 = inst->editor.pattMark.markX1; lastMarkX2 = inst->editor.pattMark.markX2;
		lastMarkY1 = inst->editor.pattMark.markY1; lastMarkY2 = inst->editor.pattMark.markY2;
		inst->uiState.updatePatternEditor = true;
		return;
	}

	/* Dragging */
	bool forceMarking = inst->replayer.songPlaying;
	if (inst->uiState.pattChanScrollShown) {
		if (mouseX < 29) { scrollChannelLeft(inst); forceMarking = true; }
		else if (mouseX > 604) { scrollChannelRight(inst); forceMarking = true; }
	}

	/* Mark channels */
	if (forceMarking || lastMouseX != mouseX)
	{
		lastMouseX = mouseX;

		int8_t chTmp = mouseXToCh(inst, mouseX);
		if (chTmp < lastChMark)
		{
			inst->editor.pattMark.markX1 = chTmp;
			inst->editor.pattMark.markX2 = lastChMark;
		}
		else
		{
			inst->editor.pattMark.markX2 = chTmp;
			inst->editor.pattMark.markX1 = lastChMark;
		}

		if (lastMarkX1 != inst->editor.pattMark.markX1 || lastMarkX2 != inst->editor.pattMark.markX2)
		{
			checkMarkLimits(inst);
			inst->uiState.updatePatternEditor = true;

			lastMarkX1 = inst->editor.pattMark.markX1;
			lastMarkX2 = inst->editor.pattMark.markX2;
		}
	}

	/* Scroll up/down with mouse near edges (only when not playing) */
	if (!inst->replayer.songPlaying)
	{
		const pattCoordsMouse_t *pattCoordsMouse = &pattCoordMouseTable
			[inst->uiState.ptnStretch]
			[inst->uiState.pattChanScrollShown]
			[inst->uiState.extendedPatternEditor];

		int16_t y1 = (int16_t)pattCoordsMouse->upperRowsY;
		int16_t y2 = inst->uiState.pattChanScrollShown ? 382 : 396;

		if (mouseY < y1)
		{
			if (inst->replayer.song.row > 0)
			{
				inst->replayer.song.row--;
				inst->editor.row = (uint8_t)inst->replayer.song.row;
			}

			forceMarking = true;
			inst->uiState.updatePatternEditor = true;
		}
		else if (mouseY > y2)
		{
			const int16_t numRows = inst->replayer.patternNumRows[inst->editor.editPattern];
			if (inst->replayer.song.row < numRows - 1)
			{
				inst->replayer.song.row++;
				inst->editor.row = (uint8_t)inst->replayer.song.row;
			}

			forceMarking = true;
			inst->uiState.updatePatternEditor = true;
		}
	}

	/* Mark rows */
	if (forceMarking || lastMouseY != mouseY)
	{
		lastMouseY = mouseY;

		const int16_t rowTmp = mouseYToRow(inst, mouseY);
		if (rowTmp < lastRowMark)
		{
			inst->editor.pattMark.markY1 = rowTmp;
			inst->editor.pattMark.markY2 = lastRowMark + 1;
		}
		else
		{
			inst->editor.pattMark.markY2 = rowTmp + 1;
			inst->editor.pattMark.markY1 = lastRowMark;
		}

		if (lastMarkY1 != inst->editor.pattMark.markY1 || lastMarkY2 != inst->editor.pattMark.markY2)
		{
			checkMarkLimits(inst);
			inst->uiState.updatePatternEditor = true;

			lastMarkY1 = inst->editor.pattMark.markY1;
			lastMarkY2 = inst->editor.pattMark.markY2;
		}
	}
}

/* ============ CHANNEL SCROLLING ============ */

void scrollChannelLeft(ft2_instance_t *inst)
{
	if (!inst || inst->uiState.channelOffset == 0) return;
	inst->uiState.channelOffset--;
	inst->uiState.updatePatternEditor = true;
}

void scrollChannelRight(ft2_instance_t *inst)
{
	if (!inst) return;
	uint8_t maxOffset = inst->replayer.song.numChannels - inst->uiState.numChannelsShown;
	if (inst->uiState.channelOffset < maxOffset) {
		inst->uiState.channelOffset++;
		inst->uiState.updatePatternEditor = true;
	}
}

void setChannelScrollPos(ft2_instance_t *inst, uint32_t pos)
{
	if (!inst) return;
	uint8_t maxOffset = inst->replayer.song.numChannels - inst->uiState.numChannelsShown;
	if (pos > maxOffset) pos = maxOffset;
	inst->uiState.channelOffset = (uint8_t)pos;
	inst->uiState.updatePatternEditor = true;
}

void jumpToChannel(ft2_instance_t *inst, uint8_t chNr)
{
	if (!inst) return;
	if (chNr >= inst->replayer.song.numChannels) chNr = inst->replayer.song.numChannels - 1;
	inst->cursor.ch = chNr;
	inst->cursor.object = CURSOR_NOTE;
	if (chNr < inst->uiState.channelOffset)
		inst->uiState.channelOffset = chNr;
	else if (chNr >= inst->uiState.channelOffset + inst->uiState.numChannelsShown)
		inst->uiState.channelOffset = chNr - inst->uiState.numChannelsShown + 1;
	inst->uiState.updatePatternEditor = true;
}

/* ============ VISIBILITY ============ */

void showPatternEditor(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->uiState.patternEditorShown = true;
	updatePatternWidth(inst);
	inst->uiState.updatePatternEditor = true;
}

void hidePatternEditor(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets) {
		hideScrollBar(widgets, SB_CHAN_SCROLL);
		hidePushButton(widgets, PB_CHAN_SCROLL_LEFT);
		hidePushButton(widgets, PB_CHAN_SCROLL_RIGHT);
	}
	inst->uiState.patternEditorShown = false;
}

/* ============ EXTENDED PATTERN EDITOR ============ */

/* Instrument switcher button layout tables */
static const uint8_t iSwitchY[8] = { 2, 19, 36, 53, 73, 90, 107, 124 };           /* Normal mode Y */
static const uint8_t iSwitchExtW[4] = { 40, 40, 40, 39 };                          /* Extended mode widths */
static const uint8_t iSwitchExtY[8] = { 2, 2, 2, 2, 19, 19, 19, 19 };              /* Extended mode Y */
static const uint16_t iSwitchExtX[4] = { 221, 262, 303, 344 };                     /* Extended mode X */

/* Reposition widgets for normal vs extended pattern editor mode */
void updatePatternEditorGUI(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

	if (inst->uiState.extendedPatternEditor) {
		/* Extended mode - compact layout at top */
		widgets->scrollBars[SB_POS_ED].h = 23;
		widgets->pushButtons[PB_POSED_POS_DOWN].y = 38;
		widgets->pushButtons[PB_POSED_PATT_UP].y = widgets->pushButtons[PB_POSED_PATT_DOWN].y = 20;
		widgets->pushButtons[PB_POSED_DEL].y = 35;
		widgets->pushButtons[PB_SWAP_BANK].caption = "Swap B.";
		widgets->pushButtons[PB_SWAP_BANK].caption2 = NULL;
		widgets->pushButtons[PB_SWAP_BANK].x = 162;
		widgets->pushButtons[PB_SWAP_BANK].y = 35;
		widgets->pushButtons[PB_SWAP_BANK].w = 53;
		widgets->pushButtons[PB_SWAP_BANK].h = 16;
		widgets->pushButtons[PB_POSED_LEN_UP].x = 180; widgets->pushButtons[PB_POSED_LEN_UP].y = 3;
		widgets->pushButtons[PB_POSED_LEN_DOWN].x = 197; widgets->pushButtons[PB_POSED_LEN_DOWN].y = 3;
		widgets->pushButtons[PB_POSED_REP_UP].x = 180; widgets->pushButtons[PB_POSED_REP_UP].y = 17;
		widgets->pushButtons[PB_POSED_REP_DOWN].x = 197; widgets->pushButtons[PB_POSED_REP_DOWN].y = 17;
		widgets->pushButtons[PB_PATT_UP].x = 267; widgets->pushButtons[PB_PATT_UP].y = 37;
		widgets->pushButtons[PB_PATT_DOWN].x = 284; widgets->pushButtons[PB_PATT_DOWN].y = 37;
		widgets->pushButtons[PB_PATTLEN_UP].x = 348; widgets->pushButtons[PB_PATTLEN_UP].y = 37;
		widgets->pushButtons[PB_PATTLEN_DOWN].x = 365; widgets->pushButtons[PB_PATTLEN_DOWN].y = 37;
		for (int i = 0; i < 16; i++) {
			widgets->pushButtons[PB_RANGE1 + i].w = iSwitchExtW[i & 3];
			widgets->pushButtons[PB_RANGE1 + i].x = iSwitchExtX[i & 3];
			widgets->pushButtons[PB_RANGE1 + i].y = iSwitchExtY[i & 7];
		}
	} else {
		/* Normal mode */
		widgets->scrollBars[SB_POS_ED].h = 21;
		widgets->pushButtons[PB_POSED_POS_DOWN].y = 36;
		widgets->pushButtons[PB_POSED_PATT_UP].y = widgets->pushButtons[PB_POSED_PATT_DOWN].y = 19;
		widgets->pushButtons[PB_POSED_DEL].y = 33;
		widgets->pushButtons[PB_SWAP_BANK].caption = "Swap";
		widgets->pushButtons[PB_SWAP_BANK].caption2 = "Bank";
		widgets->pushButtons[PB_SWAP_BANK].x = 590;
		widgets->pushButtons[PB_SWAP_BANK].y = 144;
		widgets->pushButtons[PB_SWAP_BANK].w = 39;
		widgets->pushButtons[PB_SWAP_BANK].h = 27;
		widgets->pushButtons[PB_POSED_LEN_UP].x = 74; widgets->pushButtons[PB_POSED_LEN_UP].y = 50;
		widgets->pushButtons[PB_POSED_LEN_DOWN].x = 91; widgets->pushButtons[PB_POSED_LEN_DOWN].y = 50;
		widgets->pushButtons[PB_POSED_REP_UP].x = 74; widgets->pushButtons[PB_POSED_REP_UP].y = 62;
		widgets->pushButtons[PB_POSED_REP_DOWN].x = 91; widgets->pushButtons[PB_POSED_REP_DOWN].y = 62;
		widgets->pushButtons[PB_PATT_UP].x = 253; widgets->pushButtons[PB_PATT_UP].y = 34;
		widgets->pushButtons[PB_PATT_DOWN].x = 270; widgets->pushButtons[PB_PATT_DOWN].y = 34;
		widgets->pushButtons[PB_PATTLEN_UP].x = 253; widgets->pushButtons[PB_PATTLEN_UP].y = 48;
		widgets->pushButtons[PB_PATTLEN_DOWN].x = 270; widgets->pushButtons[PB_PATTLEN_DOWN].y = 48;
		for (int i = 0; i < 16; i++) {
			widgets->pushButtons[PB_RANGE1 + i].w = 39;
			widgets->pushButtons[PB_RANGE1 + i].x = 590;
			widgets->pushButtons[PB_RANGE1 + i].y = iSwitchY[i & 7];
		}
	}
}

/* Enter extended pattern editor (full-height pattern view) */
void patternEditorExtended(ft2_instance_t *inst)
{
	if (!inst) return;

	/* Backup screen flags for restoration on exit */
	inst->uiState._aboutScreenShown = inst->uiState.aboutScreenShown;
	inst->uiState._helpScreenShown = inst->uiState.helpScreenShown;
	inst->uiState._configScreenShown = inst->uiState.configScreenShown;
	inst->uiState._diskOpShown = inst->uiState.diskOpShown;
	inst->uiState._transposeShown = inst->uiState.transposeShown;
	inst->uiState._instEditorShown = inst->uiState.instEditorShown;
	inst->uiState._instEditorExtShown = inst->uiState.instEditorExtShown;
	inst->uiState._sampleEditorShown = inst->uiState.sampleEditorShown;
	inst->uiState._sampleEditorExtShown = inst->uiState.sampleEditorExtShown;
	inst->uiState._advEditShown = inst->uiState.advEditShown;
	inst->uiState._trimScreenShown = inst->uiState.trimScreenShown;
	inst->uiState._nibblesShown = inst->uiState.nibblesShown;
	inst->uiState._patternEditorShown = inst->uiState.patternEditorShown;

	hideTopScreen(inst);
	hideSampleEditor(inst);
	hideInstEditor(inst);

	inst->uiState.extendedPatternEditor = true;
	inst->uiState.patternEditorShown = true;
	updatePatternEditorGUI(inst);
	inst->uiState.updatePatternEditor = true;
	inst->uiState.instrSwitcherShown = true;
	inst->uiState.needsFullRedraw = true;
}

/* Exit extended pattern editor, restore previous screen state */
void exitPatternEditorExtended(ft2_instance_t *inst)
{
	if (!inst) return;

	inst->uiState.extendedPatternEditor = false;
	updatePatternEditorGUI(inst);
	if (inst->ui) hidePushButton(&((ft2_ui_t *)inst->ui)->widgets, PB_EXIT_EXT_PATT);

	inst->uiState.aboutScreenShown = inst->uiState._aboutScreenShown;
	inst->uiState.helpScreenShown = inst->uiState._helpScreenShown;
	inst->uiState.configScreenShown = inst->uiState._configScreenShown;
	inst->uiState.diskOpShown = inst->uiState._diskOpShown;
	inst->uiState.nibblesShown = inst->uiState._nibblesShown;
	inst->uiState.transposeShown = inst->uiState._transposeShown;
	inst->uiState.instEditorShown = inst->uiState._instEditorShown;
	inst->uiState.instEditorExtShown = inst->uiState._instEditorExtShown;
	inst->uiState.sampleEditorShown = inst->uiState._sampleEditorShown;
	inst->uiState.sampleEditorExtShown = inst->uiState._sampleEditorExtShown;
	inst->uiState.advEditShown = inst->uiState._advEditShown;
	inst->uiState.trimScreenShown = inst->uiState._trimScreenShown;
	inst->uiState.patternEditorShown = inst->uiState._patternEditorShown;

	inst->uiState.updatePatternEditor = true;
	inst->uiState.needsFullRedraw = true;
}

void togglePatternEditorExtended(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->uiState.extendedPatternEditor ? exitPatternEditorExtended(inst) : patternEditorExtended(inst);
}

/* ============ ADVANCED EDIT DIALOG ============ */

/* Sync checkbox states from editor mask arrays and show them */
void setAdvEditCheckBoxes(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	widgets->checkBoxChecked[CB_ENABLE_MASKING] = inst->editor.copyMaskEnable;
	for (int i = 0; i < 5; i++) {
		widgets->checkBoxChecked[CB_COPY_MASK0 + i] = inst->editor.copyMask[i];
		widgets->checkBoxChecked[CB_PASTE_MASK0 + i] = inst->editor.pasteMask[i];
		widgets->checkBoxChecked[CB_TRANSP_MASK0 + i] = inst->editor.transpMask[i];
	}

	showCheckBox(widgets, video, bmp, CB_ENABLE_MASKING);
	for (int i = 0; i < 5; i++) {
		showCheckBox(widgets, video, bmp, CB_COPY_MASK0 + i);
		showCheckBox(widgets, video, bmp, CB_PASTE_MASK0 + i);
		showCheckBox(widgets, video, bmp, CB_TRANSP_MASK0 + i);
	}
}

void updateAdvEdit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video) return;
	hexOutBg(video, bmp, 92, 113, PAL_FORGRND, PAL_DESKTOP, inst->editor.srcInstr, 2);
	hexOutBg(video, bmp, 92, 126, PAL_FORGRND, PAL_DESKTOP, inst->editor.curInstr, 2);
}

void drawAdvEdit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video) return;

	/* Frameworks */
	drawFramework(video, 0, 92, 110, 17, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 109, 110, 64, FRAMEWORK_TYPE1);
	drawFramework(video, 110, 92, 124, 81, FRAMEWORK_TYPE1);
	drawFramework(video, 234, 92, 19, 81, FRAMEWORK_TYPE1);
	drawFramework(video, 253, 92, 19, 81, FRAMEWORK_TYPE1);
	drawFramework(video, 272, 92, 19, 81, FRAMEWORK_TYPE1);

	/* Labels */
	textOutShadow(video, bmp, 4, 96, PAL_FORGRND, PAL_DSKTOP2, "Instr. remap:");
	textOutShadow(video, bmp, 4, 113, PAL_FORGRND, PAL_DSKTOP2, "Old number");
	textOutShadow(video, bmp, 4, 126, PAL_FORGRND, PAL_DSKTOP2, "New number");
	textOutShadow(video, bmp, 129, 96, PAL_FORGRND, PAL_DSKTOP2, "Masking enable");
	textOutShadow(video, bmp, 114, 109, PAL_FORGRND, PAL_DSKTOP2, "Note");
	textOutShadow(video, bmp, 114, 122, PAL_FORGRND, PAL_DSKTOP2, "Instrument number");
	textOutShadow(video, bmp, 114, 135, PAL_FORGRND, PAL_DSKTOP2, "Volume column");
	textOutShadow(video, bmp, 114, 148, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 1");
	textOutShadow(video, bmp, 114, 161, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 2,3");
	charOutShadow(video, bmp, 239, 95, PAL_FORGRND, PAL_DSKTOP2, 'C');
	charOutShadow(video, bmp, 258, 95, PAL_FORGRND, PAL_DSKTOP2, 'P');
	charOutShadow(video, bmp, 277, 95, PAL_FORGRND, PAL_DSKTOP2, 'T');

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	showPushButton(widgets, video, bmp, PB_REMAP_TRACK);
	showPushButton(widgets, video, bmp, PB_REMAP_PATTERN);
	showPushButton(widgets, video, bmp, PB_REMAP_SONG);
	showPushButton(widgets, video, bmp, PB_REMAP_BLOCK);
	setAdvEditCheckBoxes(inst, video, bmp);
	updateAdvEdit(inst, video, bmp);
}

void showAdvEdit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);

	/* Hide all other top-left panel overlays (S.E.Ext, I.E.Ext, Transpose, Trim) */
	hideAllTopLeftPanelOverlays(inst);

	inst->uiState.advEditShown = true;
	inst->uiState.scopesShown = false;

	if (video != NULL)
		drawAdvEdit(inst, video, bmp);

	inst->uiState.needsFullRedraw = true;
}

void hideAdvEdit(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.advEditShown = false;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		/* Hide remap buttons */
		hidePushButton(widgets, PB_REMAP_TRACK);
		hidePushButton(widgets, PB_REMAP_PATTERN);
		hidePushButton(widgets, PB_REMAP_SONG);
		hidePushButton(widgets, PB_REMAP_BLOCK);

		/* Hide all masking checkboxes */
		hideCheckBox(widgets, CB_ENABLE_MASKING);
		hideCheckBox(widgets, CB_COPY_MASK0);
		hideCheckBox(widgets, CB_COPY_MASK1);
		hideCheckBox(widgets, CB_COPY_MASK2);
		hideCheckBox(widgets, CB_COPY_MASK3);
		hideCheckBox(widgets, CB_COPY_MASK4);
		hideCheckBox(widgets, CB_PASTE_MASK0);
		hideCheckBox(widgets, CB_PASTE_MASK1);
		hideCheckBox(widgets, CB_PASTE_MASK2);
		hideCheckBox(widgets, CB_PASTE_MASK3);
		hideCheckBox(widgets, CB_PASTE_MASK4);
		hideCheckBox(widgets, CB_TRANSP_MASK0);
		hideCheckBox(widgets, CB_TRANSP_MASK1);
		hideCheckBox(widgets, CB_TRANSP_MASK2);
		hideCheckBox(widgets, CB_TRANSP_MASK3);
		hideCheckBox(widgets, CB_TRANSP_MASK4);
	}

	inst->uiState.scopesShown = true;

	/* Trigger scope framework redraw */
	ft2_ui_t *ui = (inst->ui != NULL) ? (ft2_ui_t *)inst->ui : NULL;
	if (ui != NULL)
		ui->scopes.needsFrameworkRedraw = true;

	inst->uiState.needsFullRedraw = true;
}

void toggleAdvEdit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	if (inst->uiState.advEditShown)
		hideAdvEdit(inst);
	else
		showAdvEdit(inst, video, bmp);
}

/* ============ INSTRUMENT REMAP ============ */

/* Remap instrument src->dst in pattern area */
static void remapInstrXY(ft2_instance_t *inst, uint16_t pattNum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t src, uint8_t dst)
{
	if (!inst) return;
	ft2_note_t *pattPtr = inst->replayer.pattern[pattNum];
	if (!pattPtr) return;

	const int16_t numCh = inst->replayer.song.numChannels;
	if (x1 > numCh - 1) x1 = numCh - 1;
	if (x2 > numCh - 1) x2 = numCh - 1;
	if (x2 < x1) x2 = x1;
	const int16_t numRows = inst->replayer.patternNumRows[pattNum];
	if (y1 >= numRows) y1 = numRows - 1;
	if (y2 > numRows) y2 = numRows - y1;

	ft2_note_t *p = &pattPtr[(y1 * MAX_CHANNELS) + x1];
	const int32_t pitch = MAX_CHANNELS - ((x2 + 1) - x1);
	for (int32_t y = y1; y <= y2; y++, p += pitch)
		for (int32_t x = x1; x <= x2; x++, p++)
			if (p->instr == src) p->instr = dst;
}

void remapTrack(ft2_instance_t *inst)
{
	if (!inst || inst->editor.srcInstr == inst->editor.curInstr) return;
	uint16_t patt = inst->editor.editPattern;
	remapInstrXY(inst, patt, inst->cursor.ch, 0, inst->cursor.ch, inst->replayer.patternNumRows[patt] - 1, inst->editor.srcInstr, inst->editor.curInstr);
	inst->uiState.updatePatternEditor = true;
	ft2_song_mark_modified(inst);
}

void remapPattern(ft2_instance_t *inst)
{
	if (!inst || inst->editor.srcInstr == inst->editor.curInstr) return;
	uint16_t patt = inst->editor.editPattern;
	remapInstrXY(inst, patt, 0, 0, inst->replayer.song.numChannels - 1, inst->replayer.patternNumRows[patt] - 1, inst->editor.srcInstr, inst->editor.curInstr);
	inst->uiState.updatePatternEditor = true;
	ft2_song_mark_modified(inst);
}

void remapSong(ft2_instance_t *inst)
{
	if (!inst || inst->editor.srcInstr == inst->editor.curInstr) return;
	for (int32_t i = 0; i < FT2_MAX_PATTERNS; i++)
		remapInstrXY(inst, i, 0, 0, inst->replayer.song.numChannels - 1, inst->replayer.patternNumRows[i] - 1, inst->editor.srcInstr, inst->editor.curInstr);
	inst->uiState.updatePatternEditor = true;
	ft2_song_mark_modified(inst);
}

void remapBlock(ft2_instance_t *inst)
{
	if (!inst || inst->editor.srcInstr == inst->editor.curInstr) return;
	int32_t y1 = inst->editor.pattMark.markY1, y2 = inst->editor.pattMark.markY2;
	if (y1 >= y2) return;
	remapInstrXY(inst, inst->editor.editPattern, inst->editor.pattMark.markX1, y1, inst->editor.pattMark.markX2, y2 - 1, inst->editor.srcInstr, inst->editor.curInstr);
	inst->uiState.updatePatternEditor = true;
	ft2_song_mark_modified(inst);
}

/* ============ MASK TOGGLE CALLBACKS ============ */

void cbEnableMasking(ft2_instance_t *inst) { if (inst) inst->editor.copyMaskEnable ^= 1; }
#define MASK_CB(name, arr, idx) void name(ft2_instance_t *inst) { if (inst) inst->editor.arr[idx] ^= 1; }
MASK_CB(cbCopyMask0, copyMask, 0) MASK_CB(cbCopyMask1, copyMask, 1) MASK_CB(cbCopyMask2, copyMask, 2)
MASK_CB(cbCopyMask3, copyMask, 3) MASK_CB(cbCopyMask4, copyMask, 4)
MASK_CB(cbPasteMask0, pasteMask, 0) MASK_CB(cbPasteMask1, pasteMask, 1) MASK_CB(cbPasteMask2, pasteMask, 2)
MASK_CB(cbPasteMask3, pasteMask, 3) MASK_CB(cbPasteMask4, pasteMask, 4)
MASK_CB(cbTranspMask0, transpMask, 0) MASK_CB(cbTranspMask1, transpMask, 1) MASK_CB(cbTranspMask2, transpMask, 2)
MASK_CB(cbTranspMask3, transpMask, 3) MASK_CB(cbTranspMask4, transpMask, 4)
#undef MASK_CB

/* ============ TRANSPOSE DIALOG ============ */

void drawTranspose(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp) return;

	drawFramework(video, 0, 92, 53, 16, FRAMEWORK_TYPE1);
	drawFramework(video, 53, 92, 119, 16, FRAMEWORK_TYPE1);
	drawFramework(video, 172, 92, 119, 16, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 108, 53, 65, FRAMEWORK_TYPE1);
	drawFramework(video, 53, 108, 119, 65, FRAMEWORK_TYPE1);
	drawFramework(video, 172, 108, 119, 65, FRAMEWORK_TYPE1);

	textOutShadow(video, bmp, 4, 95, PAL_FORGRND, PAL_DSKTOP2, "Transp.");
	textOutShadow(video, bmp, 58, 95, PAL_FORGRND, PAL_DSKTOP2, "Current instrument");
	textOutShadow(video, bmp, 188, 95, PAL_FORGRND, PAL_DSKTOP2, "All instruments");
	textOutShadow(video, bmp, 4, 114, PAL_FORGRND, PAL_DSKTOP2, "Track");
	textOutShadow(video, bmp, 4, 129, PAL_FORGRND, PAL_DSKTOP2, "Pattern");
	textOutShadow(video, bmp, 4, 144, PAL_FORGRND, PAL_DSKTOP2, "Song");
	textOutShadow(video, bmp, 4, 159, PAL_FORGRND, PAL_DSKTOP2, "Block");

	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	/* 32 transpose buttons: 4 scopes (track/patt/song/block) x 2 instr modes x 4 amounts */
	static const uint16_t transpBtns[] = {
		PB_TRANSP_CUR_INS_TRK_UP, PB_TRANSP_CUR_INS_TRK_DN, PB_TRANSP_CUR_INS_TRK_12UP, PB_TRANSP_CUR_INS_TRK_12DN,
		PB_TRANSP_ALL_INS_TRK_UP, PB_TRANSP_ALL_INS_TRK_DN, PB_TRANSP_ALL_INS_TRK_12UP, PB_TRANSP_ALL_INS_TRK_12DN,
		PB_TRANSP_CUR_INS_PAT_UP, PB_TRANSP_CUR_INS_PAT_DN, PB_TRANSP_CUR_INS_PAT_12UP, PB_TRANSP_CUR_INS_PAT_12DN,
		PB_TRANSP_ALL_INS_PAT_UP, PB_TRANSP_ALL_INS_PAT_DN, PB_TRANSP_ALL_INS_PAT_12UP, PB_TRANSP_ALL_INS_PAT_12DN,
		PB_TRANSP_CUR_INS_SNG_UP, PB_TRANSP_CUR_INS_SNG_DN, PB_TRANSP_CUR_INS_SNG_12UP, PB_TRANSP_CUR_INS_SNG_12DN,
		PB_TRANSP_ALL_INS_SNG_UP, PB_TRANSP_ALL_INS_SNG_DN, PB_TRANSP_ALL_INS_SNG_12UP, PB_TRANSP_ALL_INS_SNG_12DN,
		PB_TRANSP_CUR_INS_BLK_UP, PB_TRANSP_CUR_INS_BLK_DN, PB_TRANSP_CUR_INS_BLK_12UP, PB_TRANSP_CUR_INS_BLK_12DN,
		PB_TRANSP_ALL_INS_BLK_UP, PB_TRANSP_ALL_INS_BLK_DN, PB_TRANSP_ALL_INS_BLK_12UP, PB_TRANSP_ALL_INS_BLK_12DN
	};
	for (int i = 0; i < 32; i++)
		showPushButton(widgets, video, bmp, transpBtns[i]);
}

void showTranspose(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);

	/* Hide all other top-left panel overlays (S.E.Ext, I.E.Ext, Adv.Edit, Trim) */
	hideAllTopLeftPanelOverlays(inst);

	inst->uiState.transposeShown = true;
	inst->uiState.scopesShown = false;
}

void hideTranspose(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		/* Hide all transpose pushbuttons */
		hidePushButton(widgets, PB_TRANSP_CUR_INS_TRK_UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_TRK_DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_TRK_12UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_TRK_12DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_TRK_UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_TRK_DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_TRK_12UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_TRK_12DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_PAT_UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_PAT_DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_PAT_12UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_PAT_12DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_PAT_UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_PAT_DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_PAT_12UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_PAT_12DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_SNG_UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_SNG_DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_SNG_12UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_SNG_12DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_SNG_UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_SNG_DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_SNG_12UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_SNG_12DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_BLK_UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_BLK_DN);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_BLK_12UP);
		hidePushButton(widgets, PB_TRANSP_CUR_INS_BLK_12DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_BLK_UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_BLK_DN);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_BLK_12UP);
		hidePushButton(widgets, PB_TRANSP_ALL_INS_BLK_12DN);
	}

	inst->uiState.transposeShown = false;
	inst->uiState.scopesShown = true;

	/* Trigger scope framework redraw to clear the transpose panel area */
	ft2_ui_t *ui = (inst->ui != NULL) ? (ft2_ui_t *)inst->ui : NULL;
	if (ui != NULL)
		ui->scopes.needsFrameworkRedraw = true;
}

void toggleTranspose(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.transposeShown)
		hideTranspose(inst);
	else
		showTranspose(inst);
}

/* ============ TRANSPOSE OPERATIONS ============ */

static uint32_t countOverflowingNotes(ft2_instance_t *inst, uint8_t mode, int8_t addValue, bool allInstrumentsFlag,
	uint16_t curPattern, int32_t numRows, int32_t markX1, int32_t markX2, int32_t markY1, int32_t markY2)
{
	uint32_t notesToDelete = 0;
	ft2_note_t **pattern = inst->replayer.pattern;
	int16_t *patternNumRows = inst->replayer.patternNumRows;
	int32_t numChannels = inst->replayer.song.numChannels;
	uint8_t cursorCh = inst->cursor.ch;
	uint8_t curInstr = inst->editor.curInstr;

	switch (mode)
	{
		case TRANSP_TRACK:
		{
			ft2_note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0;

			p += cursorCh;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
				{
					if ((int8_t)p->note + addValue > 96 || (int8_t)p->note + addValue <= 0)
						notesToDelete++;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			ft2_note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0;

			const int32_t pitch = MAX_CHANNELS - numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < numChannels; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
					{
						if ((int8_t)p->note + addValue > 96 || (int8_t)p->note + addValue <= 0)
							notesToDelete++;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS - numChannels;
			for (int32_t i = 0; i < FT2_MAX_PATTERNS; i++)
			{
				ft2_note_t *p = pattern[i];
				if (p == NULL)
					continue;

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < numChannels; ch++, p++)
					{
						if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
						{
							if ((int8_t)p->note + addValue > 96 || (int8_t)p->note + addValue <= 0)
								notesToDelete++;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (markY1 == markY2 || markY1 > markY2)
				return 0;

			if (markX1 > numChannels - 1)
				markX1 = numChannels - 1;

			if (markX2 > numChannels - 1)
				markX2 = numChannels - 1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows - 1;

			if (markY2 > numRows)
				markY2 = numRows - markY1;

			ft2_note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return 0;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
					{
						if ((int8_t)p->note + addValue > 96 || (int8_t)p->note + addValue <= 0)
							notesToDelete++;
					}
				}
			}
		}
		break;

		default: break;
	}

	return notesToDelete;
}

void doTranspose(ft2_instance_t *inst, uint8_t mode, int8_t addValue, bool allInstrumentsFlag)
{
	if (inst == NULL)
		return;

	const uint16_t curPattern = inst->editor.editPattern;
	const int32_t numRows = inst->replayer.patternNumRows[curPattern];
	int32_t markX1 = inst->editor.pattMark.markX1;
	int32_t markX2 = inst->editor.pattMark.markX2;
	int32_t markY1 = inst->editor.pattMark.markY1;
	int32_t markY2 = inst->editor.pattMark.markY2;

	ft2_note_t **pattern = inst->replayer.pattern;
	int16_t *patternNumRows = inst->replayer.patternNumRows;
	int32_t numChannels = inst->replayer.song.numChannels;
	uint8_t cursorCh = inst->cursor.ch;
	uint8_t curInstr = inst->editor.curInstr;

	(void)countOverflowingNotes; /* Unused in plugin - no confirmation dialogs */

	switch (mode)
	{
		case TRANSP_TRACK:
		{
			ft2_note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			p += cursorCh;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				uint8_t note = p->note;
				if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
				{
					note += addValue;
					if (note > 96)
						note = 0;

					p->note = note;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			ft2_note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			const int32_t pitch = MAX_CHANNELS - numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < numChannels; ch++, p++)
				{
					uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
					{
						note += addValue;
						if (note > 96)
							note = 0;

						p->note = note;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS - numChannels;
			for (int32_t i = 0; i < FT2_MAX_PATTERNS; i++)
			{
				ft2_note_t *p = pattern[i];
				if (p == NULL)
					continue;

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < numChannels; ch++, p++)
					{
						uint8_t note = p->note;
						if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
						{
							note += addValue;
							if (note > 96)
								note = 0;

							p->note = note;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (markY1 == markY2 || markY1 > markY2)
				return;

			if (markX1 > numChannels - 1)
				markX1 = numChannels - 1;

			if (markX2 > numChannels - 1)
				markX2 = numChannels - 1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows - 1;

			if (markY2 > numRows)
				markY2 = numRows - markY1;

			ft2_note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == curInstr))
					{
						note += addValue;
						if (note > 96)
							note = 0;

						p->note = note;
					}
				}
			}
		}
		break;

		default: break;
	}

	inst->uiState.updatePatternEditor = true;
}