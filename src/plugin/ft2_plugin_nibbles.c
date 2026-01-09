/**
 * @file ft2_plugin_nibbles.c
 * @brief Nibbles snake game (FT2 easter egg).
 *
 * Two-player snake game with 30 levels. Features: wrap mode, surround mode,
 * grid display, high score table. Cheats: "skip" (during play) skips level,
 * "triton" (menu) toggles eternal lives.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_nibbles.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_input.h"
#include "../ft2_instance.h"

static void onP1HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData);
static void onP2HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData);

static void nibblesShowMessage(ft2_instance_t *inst, const char *headline, const char *text)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) ft2_dialog_show_message(&ui->dialog, headline, text);
}

/* Speed = frame delay at 70Hz. Lower = faster. */
const uint8_t nibblesSpeedTable[4] = { 12, 8, 6, 4 }; /* Novice, Average, Pro, Triton */

static const char nibblesCheatCode1[] = "skip";
static const char nibblesCheatCode2[] = "triton";

static const char *nibblesHelpText[] = {
	"Player 1 uses cursor keys to control movement.",
	"Player 2 uses the following keys:",
	"",
	"                  (W=Up)",
	"  (A=Left) (S=Down) (D=Right)",
	"",
	"The \"Wrap\" option controls whether it's possible to walk through",
	"the screen edges or not. Turn it on and use your brain to get",
	"the maximum out of this feature.",
	"The \"Surround\" option turns Nibbles into a completely different",
	"game. Don't change this option during play! (you'll see why)",
	"We wish you many hours of fun playing this game."
};
#define NIBBLES_HELP_LINES (sizeof(nibblesHelpText) / sizeof(char *))

static const ft2_nibbles_highscore_t defaultHighScores[10] = {
	{ 5, "Vogue",  0x01500000, 23 }, { 4, "Mr.H",   0x01400000, 20 },
	{ 5, "Texel",  0x01250000, 18 }, { 4, "Tran",   0x01200000, 16 },
	{ 4, "Zolt",   0x01100000, 14 }, { 3, "Mag",    0x00750000, 10 },
	{ 2, "KC",     0x00500000, 7 },  { 5, "Raven",  0x00400000, 6 },
	{ 4, "Lone",   0x00200000, 3 },  { 3, "Mrd",    0x00100000, 1 }
};

static void nibblesRedrawScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesCreateLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp);
static void nibblesGenNewNumber(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesNewGame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesNewLevel(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void drawScoresLives(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);

/* ---------- Helpers ---------- */

/* HSL-style luminosity: (max + min) / 2 */
static uint8_t rgb24ToLuminosity(uint32_t rgb24)
{
	const uint8_t R = (rgb24 >> 16) & 0xFF;
	const uint8_t G = (rgb24 >> 8) & 0xFF;
	const uint8_t B = rgb24 & 0xFF;
	uint8_t hi = (R > G) ? R : G; if (B > hi) hi = B;
	uint8_t lo = (R < G) ? R : G; if (B < lo) lo = B;
	return (hi + lo) >> 1;
}

/* Warn if wall colors are too dark to see */
static bool wallColorsAreCloseToBlack(ft2_video_t *video)
{
#define LUMINOSITY_THRESHOLD 4
	return rgb24ToLuminosity(video->palette[PAL_DESKTOP]) <= LUMINOSITY_THRESHOLD ||
	       rgb24ToLuminosity(video->palette[PAL_BUTTONS]) <= LUMINOSITY_THRESHOLD;
}

/* Draw food number (0-9) using font8 bitmap */
static void drawNibblesFoodNumber(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xOut, int32_t yOut, int32_t number)
{
	if (number > 9 || !video->frameBuffer || !bmp->font8) return;
	uint32_t *dstPtr = &video->frameBuffer[(yOut * SCREEN_W) + xOut];
	uint8_t *srcPtr = &bmp->font8[number * FONT8_CHAR_W];
	for (int32_t y = 0; y < FONT8_CHAR_H; y++, srcPtr += FONT8_WIDTH, dstPtr += SCREEN_W)
		for (int32_t x = 0; x < FONT8_CHAR_W; x++)
			if (srcPtr[x]) dstPtr[x] = video->palette[PAL_FORGRND];
}

/* Draw a game grid cell (8x7 pixels) */
static void setNibbleDot(ft2_instance_t *inst, ft2_video_t *video, uint8_t x, uint8_t y, uint8_t c)
{
	const uint16_t xs = 152 + (x * 8), ys = 7 + (y * 7);
	if (inst->nibbles.grid) {
		fillRect(video, xs, ys, 8, 7, PAL_BUTTON2);
		fillRect(video, xs + 1, ys + 1, 7, 6, c);
	} else {
		fillRect(video, xs, ys, 8, 7, c);
	}
	inst->nibbles.screen[x][y] = c;
}

/* ---------- Input buffer ---------- */

/* Queue direction input (up to 8 buffered per player) */
static void nibblesAddBuffer(ft2_instance_t *inst, int16_t bufNum, uint8_t value)
{
	ft2_nibbles_buffer_t *n = &inst->nibbles.inputBuffer[bufNum];
	if (n->length < 8) n->data[n->length++] = value;
}

static bool nibblesBufferFull(ft2_instance_t *inst, int16_t bufNum)
{
	return inst->nibbles.inputBuffer[bufNum].length > 0;
}

/* Dequeue direction (FIFO) */
static int16_t nibblesGetBuffer(ft2_instance_t *inst, int16_t bufNum)
{
	ft2_nibbles_buffer_t *n = &inst->nibbles.inputBuffer[bufNum];
	if (n->length == 0) return -1;
	int16_t dataOut = n->data[0];
	memmove(&n->data[0], &n->data[1], 7);
	n->length--;
	return dataOut;
}

/* ---------- Level loading ---------- */

/* Copy level data from stages bitmap (10x3 grid of levels) */
static void nibblesGetLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp)
{
	const int32_t readX = 1 + ((NIBBLES_SCREEN_W + 2) * (levelNum % 10));
	const int32_t readY = 1 + ((NIBBLES_SCREEN_H + 2) * (levelNum / 10));
	const uint8_t *stagePtr = &bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + readX];
	for (int32_t y = 0; y < NIBBLES_SCREEN_H; y++, stagePtr += NIBBLES_STAGES_BMP_WIDTH)
		for (int32_t x = 0; x < NIBBLES_SCREEN_W; x++)
			inst->nibbles.screen[x][y] = stagePtr[x];
}

/* Initialize level: load walls, find spawn points, reset snake positions */
static void nibblesCreateLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp)
{
	if (levelNum >= NIBBLES_MAX_LEVEL) levelNum = NIBBLES_MAX_LEVEL - 1;
	nibblesGetLevel(inst, levelNum, bmp);

	/* Find spawn points (color 1=P2, color 3=P1) and clear them */
	int32_t x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	for (int32_t y = 0; y < NIBBLES_SCREEN_H; y++) {
		for (int32_t x = 0; x < NIBBLES_SCREEN_W; x++) {
			uint8_t c = inst->nibbles.screen[x][y];
			if (c == 3) { x1 = x; y1 = y; inst->nibbles.screen[x][y] = 0; }
			else if (c == 1) { x2 = x; y2 = y; inst->nibbles.screen[x][y] = 0; }
		}
	}

	/* Read initial directions from stage header */
	const int32_t readX = (NIBBLES_SCREEN_W + 2) * (levelNum % 10);
	const int32_t readY = (NIBBLES_SCREEN_H + 2) * (levelNum / 10);
	inst->nibbles.p1Dir = bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + (readX + 1)];
	inst->nibbles.p2Dir = bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + (readX + 0)];

	inst->nibbles.p1Len = inst->nibbles.p2Len = 5;
	inst->nibbles.p1NoClear = inst->nibbles.p2NoClear = 0;
	inst->nibbles.number = 0;
	inst->nibbles.inputBuffer[0].length = inst->nibbles.inputBuffer[1].length = 0;

	for (int32_t i = 0; i < 256; i++) {
		inst->nibbles.p1[i].x = (uint8_t)x1; inst->nibbles.p1[i].y = (uint8_t)y1;
		inst->nibbles.p2[i].x = (uint8_t)x2; inst->nibbles.p2[i].y = (uint8_t)y2;
	}
}

/* ---------- Drawing ---------- */

/* Draw level thumbnail for high score table */
static void nibbleWriteLevelSprite(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t xOut, int16_t yOut, int16_t levelNum)
{
	const int32_t readX = (NIBBLES_SCREEN_W + 2) * (levelNum % 10);
	const int32_t readY = (NIBBLES_SCREEN_H + 2) * (levelNum / 10);
	const uint8_t *src = &bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + readX];
	uint32_t *dst = &video->frameBuffer[(yOut * SCREEN_W) + xOut];
	for (int32_t y = 0; y < NIBBLES_SCREEN_H + 2; y++, src += NIBBLES_STAGES_BMP_WIDTH, dst += SCREEN_W)
		for (int32_t x = 0; x < NIBBLES_SCREEN_W + 2; x++)
			dst[x] = video->palette[src[x]];
	/* Mark direction indicator pixels */
	video->frameBuffer[(yOut * SCREEN_W) + xOut] = video->palette[PAL_FORGRND];
	video->frameBuffer[(yOut * SCREEN_W) + xOut + 1] = video->palette[PAL_FORGRND];
}

/* Shadowed text with X clipping for high score names */
static void highScoreTextOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y,
	uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr, uint16_t clipX)
{
	if (!textPtr) return;
	uint16_t currX = x;
	for (uint16_t i = 0; i < 22 && textPtr[i] && currX < clipX; i++) {
		charOutClipX(video, bmp, currX + 1, y + 1, shadowPaletteIndex, textPtr[i], clipX);
		charOutClipX(video, bmp, currX, y, paletteIndex, textPtr[i], clipX);
		currX += charWidth(textPtr[i]);
	}
}

static void drawScoresLives(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	char livesStr[4];
	hexOutBg(video, bmp, 89, 27, PAL_FORGRND, PAL_DESKTOP, inst->nibbles.p1Score, 8);
	snprintf(livesStr, sizeof(livesStr), "%02d", (int)(inst->nibbles.p1Lives > 99 ? 99 : inst->nibbles.p1Lives));
	textOutFixed(video, bmp, 131, 39, PAL_FORGRND, PAL_DESKTOP, livesStr);

	hexOutBg(video, bmp, 89, 75, PAL_FORGRND, PAL_DESKTOP, inst->nibbles.p2Score, 8);
	snprintf(livesStr, sizeof(livesStr), "%02d", (int)(inst->nibbles.p2Lives > 99 ? 99 : inst->nibbles.p2Lives));
	textOutFixed(video, bmp, 131, 87, PAL_FORGRND, PAL_DESKTOP, livesStr);
}

/* Redraw entire game grid */
static void nibblesRedrawScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst->nibbles.playing) return;

	for (int16_t x = 0; x < NIBBLES_SCREEN_W; x++) {
		for (int16_t y = 0; y < NIBBLES_SCREEN_H; y++) {
			const int16_t xs = 152 + (x * 8), ys = 7 + (y * 7);
			const uint8_t c = inst->nibbles.screen[x][y];
			if (c < 16) {
				if (inst->nibbles.grid) {
					fillRect(video, xs, ys, 8, 7, PAL_BUTTON2);
					fillRect(video, xs + 1, ys + 1, 7, 6, c);
				} else {
					fillRect(video, xs, ys, 8, 7, c);
				}
			} else {
				drawNibblesFoodNumber(video, bmp, xs + 2, ys, inst->nibbles.number);
			}
		}
	}
	/* Fix grid border artifacts */
	uint8_t edgeColor = inst->nibbles.grid ? PAL_BUTTON2 : PAL_BCKGRND;
	vLine(video, 560, 7, 161, edgeColor);
	hLine(video, 152, 168, 409, edgeColor);
}

void ft2_nibbles_redraw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	nibblesRedrawScreen(inst, video, bmp);
}

/* ---------- Game logic ---------- */

/* Check collision: wall (1-15) or screen edge (if wrap disabled) */
static bool nibblesInvalid(ft2_instance_t *inst, int16_t x, int16_t y, int16_t d)
{
	if (!inst->nibbles.wrap)
		if ((x == 0 && d == 0) || (x == 50 && d == 2) || (y == 0 && d == 3) || (y == 22 && d == 1))
			return true;
	if (x >= 0 && x < NIBBLES_SCREEN_W && y >= 0 && y < NIBBLES_SCREEN_H)
		return inst->nibbles.screen[x][y] >= 1 && inst->nibbles.screen[x][y] <= 15;
	return true;
}

static void nibblesEraseNumber(ft2_instance_t *inst, ft2_video_t *video)
{
	if (!inst->nibbles.surround)
		setNibbleDot(inst, video, (uint8_t)inst->nibbles.numberX, (uint8_t)inst->nibbles.numberY, 0);
}

/* Place next food number (1-9) at random empty position */
static void nibblesGenNewNumber(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	while (true) {
		int16_t x = rand() % NIBBLES_SCREEN_W, y = rand() % NIBBLES_SCREEN_H;
		/* Need empty cell (and cell below for number rendering) */
		bool ok = inst->nibbles.screen[x][y] == 0;
		if (y < NIBBLES_SCREEN_H - 1) ok = ok && inst->nibbles.screen[x][y + 1] == 0;
		if (ok) {
			inst->nibbles.number++;
			inst->nibbles.screen[x][y] = (uint8_t)(16 + inst->nibbles.number);
			inst->nibbles.numberX = x; inst->nibbles.numberY = y;
			const int16_t xs = 152 + (x * 8), ys = 7 + (y * 7);
			if (inst->nibbles.grid) {
				fillRect(video, xs, ys, 8, 7, PAL_BUTTON2);
				fillRect(video, xs + 1, ys + 1, 7, 6, PAL_BCKGRND);
			} else {
				fillRect(video, xs, ys, 8, 7, PAL_BCKGRND);
			}
			drawNibblesFoodNumber(video, bmp, (x * 8) + 154, (y * 7) + 7, inst->nibbles.number);
			break;
		}
	}
}

static void nibblesNewGame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	nibblesCreateLevel(inst, inst->nibbles.level, bmp);
	nibblesRedrawScreen(inst, video, bmp);
	setNibbleDot(inst, video, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, 6);
	if (inst->nibbles.numPlayers == 1)
		setNibbleDot(inst, video, inst->nibbles.p2[0].x, inst->nibbles.p2[0].y, 7);
	if (!inst->nibbles.surround) nibblesGenNewNumber(inst, video, bmp);
}

static void nibblesNewLevel(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	char text[32];
	snprintf(text, sizeof(text), "Level %d finished!", inst->nibbles.level + 1);
	nibblesShowMessage(inst, "Nibbles message", text);

	/* Bonus: base + speed bonus. Cast to int16_t replicates original FT2 overflow bug. */
	inst->nibbles.p1Score += 0x10000 + (int16_t)((12 - inst->nibbles.curSpeed) * 0x2000);
	if (inst->nibbles.numPlayers == 1) inst->nibbles.p2Score += 0x10000;

	inst->nibbles.level++;
	if (inst->nibbles.p1Lives < 99) inst->nibbles.p1Lives++;
	if (inst->nibbles.numPlayers == 1 && inst->nibbles.p2Lives < 99) inst->nibbles.p2Lives++;

	inst->nibbles.number = 0;
	nibblesCreateLevel(inst, inst->nibbles.level, bmp);
	nibblesRedrawScreen(inst, video, bmp);
	nibblesGenNewNumber(inst, video, bmp);
}

/* ---------- Death / high scores ---------- */

static void nibblesDecLives(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp, int16_t l1, int16_t l2)
{
	if (!inst->nibbles.eternalLives)
	{
		inst->nibbles.p1Lives -= l1;
		inst->nibbles.p2Lives -= l2;
	}

	drawScoresLives(inst, video, bmp);

	if (l1 + l2 == 2)
		nibblesShowMessage(inst, "Nibbles message", "Both players died!");
	else if (l2 == 0)
		nibblesShowMessage(inst, "Nibbles message", "Player 1 died!");
	else
		nibblesShowMessage(inst, "Nibbles message", "Player 2 died!");

	if (inst->nibbles.p1Lives == 0 || inst->nibbles.p2Lives == 0)
	{
		inst->nibbles.playing = false;
		nibblesShowMessage(inst, "Nibbles message", "GAME OVER");

		/* Prevent highscore table from showing overflowing level graphics */
		if (inst->nibbles.level >= NIBBLES_MAX_LEVEL)
			inst->nibbles.level = NIBBLES_MAX_LEVEL - 1;

		/* Reset pending high score state */
		inst->nibbles.pendingP1HighScore = false;
		inst->nibbles.pendingP2HighScore = false;
		inst->nibbles.pendingP1Slot = -1;
		inst->nibbles.pendingP2Slot = -1;

		/* Check if Player 1 has a high score */
		if (inst->nibbles.p1Score > inst->nibbles.highScores[9].score)
		{
			int16_t i = 0;
			while (inst->nibbles.p1Score <= inst->nibbles.highScores[i].score)
				i++;

			/* Shift scores down to make room */
			for (int16_t k = 8; k >= i; k--)
				memcpy(&inst->nibbles.highScores[k + 1], &inst->nibbles.highScores[k], sizeof(ft2_nibbles_highscore_t));

			if (i == 0)
				nibblesShowMessage(inst, "Nibbles message", "You've probably cheated!");

			/* Store the slot for later, prefill with default name */
			ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[i];
			memset(h->name, 0, sizeof(h->name));
			strcpy(h->name, "Unknown");
			h->nameLen = 7;
			h->score = inst->nibbles.p1Score;
			h->level = inst->nibbles.level;

			/* Mark pending and show input dialog */
			inst->nibbles.pendingP1HighScore = true;
			inst->nibbles.pendingP1Slot = i;

			ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
			if (ui != NULL)
			{
				ft2_dialog_show_input_cb(&ui->dialog,
					"Player 1 - Enter your name:", "",
					"Unknown", 21, inst, onP1HighScoreNameEntered, NULL);
			}
			return;  /* Wait for callback to continue */
		}

		/* Check if Player 2 has a high score (only if P1 didn't) */
		if (inst->nibbles.p2Score > inst->nibbles.highScores[9].score)
		{
			int16_t i = 0;
			while (inst->nibbles.p2Score <= inst->nibbles.highScores[i].score)
				i++;

			for (int16_t k = 8; k >= i; k--)
				memcpy(&inst->nibbles.highScores[k + 1], &inst->nibbles.highScores[k], sizeof(ft2_nibbles_highscore_t));

			if (i == 0)
				nibblesShowMessage(inst, "Nibbles message", "You've probably cheated!");

			ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[i];
			memset(h->name, 0, sizeof(h->name));
			strcpy(h->name, "Unknown");
			h->nameLen = 7;
			h->score = inst->nibbles.p2Score;
			h->level = inst->nibbles.level;

			inst->nibbles.pendingP2HighScore = true;
			inst->nibbles.pendingP2Slot = i;

			ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
			if (ui != NULL)
			{
				ft2_dialog_show_input_cb(&ui->dialog,
					"Player 2 - Enter your name:", "",
					"Unknown", 21, inst, onP2HighScoreNameEntered, NULL);
			}
			return;  /* Wait for callback to continue */
		}

		/* No high scores, show high score table directly */
		ft2_nibbles_show_highscores(inst, video, bmp);
	}
	else
	{
		inst->nibbles.playing = true;
		nibblesNewGame(inst, video, bmp);
	}
}

/* Store entered name, then check if P2 also has a high score */
static void onP1HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)userData;
	if (inst->nibbles.pendingP1HighScore && inst->nibbles.pendingP1Slot >= 0) {
		ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[inst->nibbles.pendingP1Slot];
		if (result == DIALOG_RESULT_OK && inputText && inputText[0]) {
			size_t len = strlen(inputText); if (len > 21) len = 21;
			memset(h->name, 0, sizeof(h->name));
			memcpy(h->name, inputText, len);
			h->nameLen = (uint8_t)len;
		}
		inst->nibbles.pendingP1HighScore = false;
		inst->nibbles.pendingP1Slot = -1;
	}

	/* Check P2 high score */
	if (inst->nibbles.p2Score > inst->nibbles.highScores[9].score) {
		int16_t i = 0;
		while (inst->nibbles.p2Score <= inst->nibbles.highScores[i].score) i++;
		for (int16_t k = 8; k >= i; k--)
			memcpy(&inst->nibbles.highScores[k + 1], &inst->nibbles.highScores[k], sizeof(ft2_nibbles_highscore_t));
		if (i == 0) nibblesShowMessage(inst, "Nibbles message", "You've probably cheated!");
		ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[i];
		memset(h->name, 0, sizeof(h->name)); strcpy(h->name, "Unknown"); h->nameLen = 7;
		h->score = inst->nibbles.p2Score; h->level = inst->nibbles.level;
		inst->nibbles.pendingP2HighScore = true; inst->nibbles.pendingP2Slot = i;
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_input_cb(&ui->dialog, "Player 2 - Enter your name:", "", "Unknown", 21, inst, onP2HighScoreNameEntered, NULL);
		return;
	}
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ft2_nibbles_show_highscores(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
}

static void onP2HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)userData;
	if (inst->nibbles.pendingP2HighScore && inst->nibbles.pendingP2Slot >= 0) {
		ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[inst->nibbles.pendingP2Slot];
		if (result == DIALOG_RESULT_OK && inputText && inputText[0]) {
			size_t len = strlen(inputText); if (len > 21) len = 21;
			memset(h->name, 0, sizeof(h->name));
			memcpy(h->name, inputText, len);
			h->nameLen = (uint8_t)len;
		}
		inst->nibbles.pendingP2HighScore = false;
		inst->nibbles.pendingP2Slot = -1;
	}
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ft2_nibbles_show_highscores(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
}

static void onRestartGameConfirm(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (result == DIALOG_RESULT_YES) {
		inst->nibbles.playing = false;
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_nibbles_play(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
	}
}

static void onQuitGameConfirm(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (result == DIALOG_RESULT_YES) {
		inst->nibbles.playing = false;
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_nibbles_exit(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
	}
}

/* ---------- Public API ---------- */

/* Original FT2 ran at 70Hz; plugin runs at 60Hz */
#define SCALE_VBLANK_DELTA_REV(x) ((int32_t)round((x) * (60.0 / 70.0)))

void ft2_nibbles_init(ft2_instance_t *inst)
{
	memset(&inst->nibbles, 0, sizeof(ft2_nibbles_state_t));
	inst->nibbles.speed = 0;
	inst->nibbles.numPlayers = 0;
	inst->nibbles.grid = true;
	ft2_nibbles_load_default_highscores(inst);
}

void ft2_nibbles_load_default_highscores(ft2_instance_t *inst)
{
	memcpy(inst->nibbles.highScores, defaultHighScores, sizeof(defaultHighScores));
}

void ft2_nibbles_show(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;
	ft2_widgets_t *widgets = &ui->widgets;

	if (inst->uiState.extendedPatternEditor) {
		inst->uiState.extendedPatternEditor = false;
		inst->uiState.needsFullRedraw = true;
	}

	hideTopScreen(inst);
	inst->uiState.nibblesShown = true;

	/* Framework */
	drawFramework(video, 0, 0, 632, 3, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 3, 148, 49, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 52, 148, 49, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 101, 148, 72, FRAMEWORK_TYPE1);
	drawFramework(video, 148, 3, 417, 170, FRAMEWORK_TYPE1);
	drawFramework(video, 150, 5, 413, 166, FRAMEWORK_TYPE2);
	drawFramework(video, 565, 3, 67, 170, FRAMEWORK_TYPE1);

	/* Labels */
	bigTextOutShadow(video, bmp, 4, 6, PAL_FORGRND, PAL_DSKTOP2, "Player 1");
	bigTextOutShadow(video, bmp, 4, 55, PAL_FORGRND, PAL_DSKTOP2, "Player 2");
	textOutShadow(video, bmp, 4, 27, PAL_FORGRND, PAL_DSKTOP2, "Score");
	textOutShadow(video, bmp, 4, 75, PAL_FORGRND, PAL_DSKTOP2, "Score");
	textOutShadow(video, bmp, 4, 39, PAL_FORGRND, PAL_DSKTOP2, "Lives");
	textOutShadow(video, bmp, 4, 87, PAL_FORGRND, PAL_DSKTOP2, "Lives");
	textOutShadow(video, bmp, 18, 106, PAL_FORGRND, PAL_DSKTOP2, "1 player");
	textOutShadow(video, bmp, 18, 120, PAL_FORGRND, PAL_DSKTOP2, "2 players");
	textOutShadow(video, bmp, 20, 135, PAL_FORGRND, PAL_DSKTOP2, "Surround");
	textOutShadow(video, bmp, 20, 148, PAL_FORGRND, PAL_DSKTOP2, "Grid");
	textOutShadow(video, bmp, 20, 161, PAL_FORGRND, PAL_DSKTOP2, "Wrap");
	textOutShadow(video, bmp, 80, 105, PAL_FORGRND, PAL_DSKTOP2, "Difficulty:");
	textOutShadow(video, bmp, 93, 118, PAL_FORGRND, PAL_DSKTOP2, "Novice");
	textOutShadow(video, bmp, 93, 132, PAL_FORGRND, PAL_DSKTOP2, "Average");
	textOutShadow(video, bmp, 93, 146, PAL_FORGRND, PAL_DSKTOP2, "Pro");
	textOutShadow(video, bmp, 93, 160, PAL_FORGRND, PAL_DSKTOP2, "Triton");
	drawScoresLives(inst, video, bmp);
	blitFast(video, 569, 7, bmp->nibblesLogo, 59, 91);

	/* Widgets */
	showPushButton(widgets, video, bmp, PB_NIBBLES_PLAY);
	showPushButton(widgets, video, bmp, PB_NIBBLES_HELP);
	showPushButton(widgets, video, bmp, PB_NIBBLES_HIGHS);
	showPushButton(widgets, video, bmp, PB_NIBBLES_EXIT);

	widgets->checkBoxChecked[CB_NIBBLES_SURROUND] = inst->nibbles.surround;
	widgets->checkBoxChecked[CB_NIBBLES_GRID] = inst->nibbles.grid;
	widgets->checkBoxChecked[CB_NIBBLES_WRAP] = inst->nibbles.wrap;
	showCheckBox(widgets, video, bmp, CB_NIBBLES_SURROUND);
	showCheckBox(widgets, video, bmp, CB_NIBBLES_GRID);
	showCheckBox(widgets, video, bmp, CB_NIBBLES_WRAP);

	uncheckRadioButtonGroup(widgets, RB_GROUP_NIBBLES_PLAYERS);
	widgets->radioButtonState[inst->nibbles.numPlayers == 0 ? RB_NIBBLES_1PLAYER : RB_NIBBLES_2PLAYER] = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_NIBBLES_PLAYERS);

	uncheckRadioButtonGroup(widgets, RB_GROUP_NIBBLES_DIFFICULTY);
	int rb = RB_NIBBLES_NOVICE + inst->nibbles.speed;
	if (rb > RB_NIBBLES_TRITON) rb = RB_NIBBLES_NOVICE;
	widgets->radioButtonState[rb] = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_NIBBLES_DIFFICULTY);
}

void ft2_nibbles_hide(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;
	ft2_widgets_t *widgets = &ui->widgets;

	hidePushButton(widgets, PB_NIBBLES_PLAY);
	hidePushButton(widgets, PB_NIBBLES_HELP);
	hidePushButton(widgets, PB_NIBBLES_HIGHS);
	hidePushButton(widgets, PB_NIBBLES_EXIT);
	hideRadioButtonGroup(widgets, RB_GROUP_NIBBLES_PLAYERS);
	hideRadioButtonGroup(widgets, RB_GROUP_NIBBLES_DIFFICULTY);
	hideCheckBox(widgets, CB_NIBBLES_SURROUND);
	hideCheckBox(widgets, CB_NIBBLES_GRID);
	hideCheckBox(widgets, CB_NIBBLES_WRAP);

	inst->uiState.nibblesShown = false;
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = false;
}

void ft2_nibbles_exit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	(void)video; (void)bmp;
	if (inst->nibbles.playing) {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Quit current game of Nibbles?", inst, onQuitGameConfirm, NULL);
		return;
	}
	ft2_nibbles_hide(inst);
	inst->uiState.scopesShown = true;
	inst->uiState.instrSwitcherShown = true;
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.updateInstrSwitcher = true;
}

void ft2_nibbles_play(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing) {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "Nibbles request", "Restart current game of Nibbles?", inst, onRestartGameConfirm, NULL);
		return;
	}
	if (inst->nibbles.surround && inst->nibbles.numPlayers == 0) {
		nibblesShowMessage(inst, "Nibbles message", "Surround mode is not appropriate in one-player mode.");
		return;
	}
	if (wallColorsAreCloseToBlack(video))
		nibblesShowMessage(inst, "Nibbles warning", "The Desktop/Button colors are set to values that make the walls hard to see!");

	inst->nibbles.curSpeed = nibblesSpeedTable[inst->nibbles.speed];
	inst->nibbles.curSpeed60Hz = (uint8_t)SCALE_VBLANK_DELTA_REV(inst->nibbles.curSpeed);
	inst->nibbles.curTick60Hz = (uint8_t)SCALE_VBLANK_DELTA_REV(nibblesSpeedTable[2]);
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = false;

	inst->nibbles.playing = true;
	inst->nibbles.p1Score = inst->nibbles.p2Score = 0;
	inst->nibbles.p1Lives = inst->nibbles.p2Lives = 5;
	inst->nibbles.level = 0;
	nibblesNewGame(inst, video, bmp);
}

void ft2_nibbles_show_highscores(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing) {
		nibblesShowMessage(inst, "Nibbles message", "The highscore table is not available during play.");
		return;
	}
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = true;

	clearRect(video, 152, 7, 409, 162);
	bigTextOut(video, bmp, 160, 10, PAL_FORGRND, "Fasttracker Nibbles Highscore");
	for (int16_t i = 0; i < 5; i++) {
		highScoreTextOutClipX(video, bmp, 160, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i].name, 230);
		hexOutShadow(video, bmp, 236, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i].score, 8);
		nibbleWriteLevelSprite(video, bmp, 296, 33 + (26 * i), inst->nibbles.highScores[i].level);
		highScoreTextOutClipX(video, bmp, 360, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i + 5].name, 430);
		hexOutShadow(video, bmp, 436, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i + 5].score, 8);
		nibbleWriteLevelSprite(video, bmp, 496, 33 + (26 * i), inst->nibbles.highScores[i + 5].level);
	}
}

void ft2_nibbles_show_help(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing) {
		nibblesShowMessage(inst, "System message", "Help is not available during play.");
		return;
	}
	inst->uiState.nibblesHelpShown = true;
	inst->uiState.nibblesHighScoresShown = false;

	clearRect(video, 152, 7, 409, 162);
	bigTextOut(video, bmp, 160, 10, PAL_FORGRND, "Fasttracker Nibbles Help");
	for (uint16_t i = 0; i < NIBBLES_HELP_LINES; i++)
		textOut(video, bmp, 160, 36 + (11 * i), PAL_BUTTONS, nibblesHelpText[i]);
}

/* ---------- Game tick ---------- */

void ft2_nibbles_tick(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst->nibbles.playing || inst->uiState.sysReqShown) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui && ft2_dialog_is_active(&ui->dialog)) return;
	if (--inst->nibbles.curTick60Hz != 0) return;

	/* Process queued direction changes (can't reverse into self) */
	if (nibblesBufferFull(inst, 0)) {
		int16_t d = nibblesGetBuffer(inst, 0);
		if (d >= 0 && d != ((inst->nibbles.p1Dir + 2) & 3)) inst->nibbles.p1Dir = (uint8_t)d;
	}
	if (nibblesBufferFull(inst, 1)) {
		int16_t d = nibblesGetBuffer(inst, 1);
		if (d >= 0 && d != ((inst->nibbles.p2Dir + 2) & 3)) inst->nibbles.p2Dir = (uint8_t)d;
	}

	/* Shift snake body, move head */
	memmove(&inst->nibbles.p1[1], &inst->nibbles.p1[0], 255 * sizeof(ft2_nibbles_coord_t));
	if (inst->nibbles.numPlayers == 1)
		memmove(&inst->nibbles.p2[1], &inst->nibbles.p2[0], 255 * sizeof(ft2_nibbles_coord_t));

	switch (inst->nibbles.p1Dir) {
		case 0: inst->nibbles.p1[0].x++; break;
		case 1: inst->nibbles.p1[0].y--; break;
		case 2: inst->nibbles.p1[0].x--; break;
		case 3: inst->nibbles.p1[0].y++; break;
	}
	if (inst->nibbles.numPlayers == 1) {
		switch (inst->nibbles.p2Dir) {
			case 0: inst->nibbles.p2[0].x++; break;
			case 1: inst->nibbles.p2[0].y--; break;
			case 2: inst->nibbles.p2[0].x--; break;
			case 3: inst->nibbles.p2[0].y++; break;
		}
	}

	/* Wrap at edges (uint8_t underflow handled) */
	if (inst->nibbles.p1[0].x == 255) inst->nibbles.p1[0].x = 50;
	if (inst->nibbles.p2[0].x == 255) inst->nibbles.p2[0].x = 50;
	if (inst->nibbles.p1[0].y == 255) inst->nibbles.p1[0].y = 22;
	if (inst->nibbles.p2[0].y == 255) inst->nibbles.p2[0].y = 22;
	inst->nibbles.p1[0].x %= NIBBLES_SCREEN_W;
	inst->nibbles.p1[0].y %= NIBBLES_SCREEN_H;
	inst->nibbles.p2[0].x %= NIBBLES_SCREEN_W;
	inst->nibbles.p2[0].y %= NIBBLES_SCREEN_H;

	/* Check collisions */
	if (inst->nibbles.numPlayers == 1)
	{
		if (nibblesInvalid(inst, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, inst->nibbles.p1Dir) &&
		    nibblesInvalid(inst, inst->nibbles.p2[0].x, inst->nibbles.p2[0].y, inst->nibbles.p2Dir))
		{
			nibblesDecLives(inst, video, bmp, 1, 1);
			goto NoMove;
		}
		else if (nibblesInvalid(inst, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, inst->nibbles.p1Dir))
		{
			nibblesDecLives(inst, video, bmp, 1, 0);
			goto NoMove;
		}
		else if (nibblesInvalid(inst, inst->nibbles.p2[0].x, inst->nibbles.p2[0].y, inst->nibbles.p2Dir))
		{
			nibblesDecLives(inst, video, bmp, 0, 1);
			goto NoMove;
		}
		else if (inst->nibbles.p1[0].x == inst->nibbles.p2[0].x && inst->nibbles.p1[0].y == inst->nibbles.p2[0].y)
		{
			nibblesDecLives(inst, video, bmp, 1, 1);
			goto NoMove;
		}
	}
	else
	{
		if (nibblesInvalid(inst, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, inst->nibbles.p1Dir))
		{
			nibblesDecLives(inst, video, bmp, 1, 0);
			goto NoMove;
		}
	}

	/* Check for food pickup */
	int16_t j = 0;
	int16_t i = inst->nibbles.screen[inst->nibbles.p1[0].x][inst->nibbles.p1[0].y];
	if (i >= 16)
	{
		inst->nibbles.p1Score += (i & 15) * 999 * (inst->nibbles.level + 1);
		nibblesEraseNumber(inst, video);
		j = 1;
		inst->nibbles.p1NoClear = inst->nibbles.p1Len >> 1;
	}

	if (inst->nibbles.numPlayers == 1)
	{
		i = inst->nibbles.screen[inst->nibbles.p2[0].x][inst->nibbles.p2[0].y];
		if (i >= 16)
		{
			inst->nibbles.p2Score += (i & 15) * 999 * (inst->nibbles.level + 1);
			nibblesEraseNumber(inst, video);
			j = 1;
			inst->nibbles.p2NoClear = inst->nibbles.p2Len >> 1;
		}
	}

	/* Score decay */
	inst->nibbles.p1Score -= 17;
	if (inst->nibbles.numPlayers == 1)
		inst->nibbles.p2Score -= 17;

	if (inst->nibbles.p1Score < 0) inst->nibbles.p1Score = 0;
	if (inst->nibbles.p2Score < 0) inst->nibbles.p2Score = 0;

	/* Clear tail */
	if (!inst->nibbles.surround)
	{
		if (inst->nibbles.p1NoClear > 0 && inst->nibbles.p1Len < 255)
		{
			inst->nibbles.p1NoClear--;
			inst->nibbles.p1Len++;
		}
		else
		{
			setNibbleDot(inst, video, inst->nibbles.p1[inst->nibbles.p1Len].x, inst->nibbles.p1[inst->nibbles.p1Len].y, 0);
		}

		if (inst->nibbles.numPlayers == 1)
		{
			if (inst->nibbles.p2NoClear > 0 && inst->nibbles.p2Len < 255)
			{
				inst->nibbles.p2NoClear--;
				inst->nibbles.p2Len++;
			}
			else
			{
				setNibbleDot(inst, video, inst->nibbles.p2[inst->nibbles.p2Len].x, inst->nibbles.p2[inst->nibbles.p2Len].y, 0);
			}
		}
	}

	/* Draw heads */
	setNibbleDot(inst, video, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, 6);
	if (inst->nibbles.numPlayers == 1)
		setNibbleDot(inst, video, inst->nibbles.p2[0].x, inst->nibbles.p2[0].y, 5);

	/* Check for level complete */
	if (j == 1 && !inst->nibbles.surround)
	{
		if (inst->nibbles.number == 9)
		{
			nibblesNewLevel(inst, video, bmp);
			inst->nibbles.curTick60Hz = inst->nibbles.curSpeed60Hz;
			return;
		}

		nibblesGenNewNumber(inst, video, bmp);
	}

NoMove:
	inst->nibbles.curTick60Hz = inst->nibbles.curSpeed60Hz;
	drawScoresLives(inst, video, bmp);
}

/* ---------- Input handling ---------- */

void ft2_nibbles_add_input(ft2_instance_t *inst, int16_t playerNum, uint8_t direction)
{
	nibblesAddBuffer(inst, playerNum, direction);
}

bool ft2_nibbles_handle_key(ft2_instance_t *inst, int32_t keyCode)
{
	if (!inst->nibbles.playing) return false;

	if (keyCode == 27) {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Quit current game of Nibbles?", inst, onQuitGameConfirm, NULL);
		return true;
	}

	/* P1: Arrow keys (0=right, 1=up, 2=left, 3=down) */
	switch (keyCode) {
		case FT2_KEY_RIGHT: nibblesAddBuffer(inst, 0, 0); return true;
		case FT2_KEY_UP:    nibblesAddBuffer(inst, 0, 1); return true;
		case FT2_KEY_LEFT:  nibblesAddBuffer(inst, 0, 2); return true;
		case FT2_KEY_DOWN:  nibblesAddBuffer(inst, 0, 3); return true;
	}

	/* P2: WASD */
	if (keyCode == 'd' || keyCode == 'D') { nibblesAddBuffer(inst, 1, 0); return true; }
	if (keyCode == 'w' || keyCode == 'W') { nibblesAddBuffer(inst, 1, 1); return true; }
	if (keyCode == 'a' || keyCode == 'A') { nibblesAddBuffer(inst, 1, 2); return true; }
	if (keyCode == 's' || keyCode == 'S') { nibblesAddBuffer(inst, 1, 3); return true; }
	return false;
}

/* Cheat codes: Shift+Ctrl+Alt + "skip" (during game) or "triton" (menu) */
bool ft2_nibbles_test_cheat(ft2_instance_t *inst, int32_t keyCode,
	bool shiftPressed, bool ctrlPressed, bool altPressed,
	ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!shiftPressed || !ctrlPressed || !altPressed) return false;

	const char *code = inst->nibbles.playing ? nibblesCheatCode1 : nibblesCheatCode2;
	uint8_t codeLen = inst->nibbles.playing ? sizeof(nibblesCheatCode1) - 1 : sizeof(nibblesCheatCode2) - 1;

	inst->nibbles.cheatBuffer[inst->nibbles.cheatIndex] = (char)keyCode;
	if (inst->nibbles.cheatBuffer[inst->nibbles.cheatIndex] != code[inst->nibbles.cheatIndex]) {
		inst->nibbles.cheatIndex = 0;
		return true;
	}

	if (++inst->nibbles.cheatIndex == codeLen) {
		inst->nibbles.cheatIndex = 0;
		if (inst->nibbles.playing) {
			nibblesNewLevel(inst, video, bmp);
		} else {
			inst->nibbles.eternalLives = !inst->nibbles.eternalLives;
			nibblesShowMessage(inst, "Triton productions declares:",
				inst->nibbles.eternalLives ? "Eternal lives activated!" : "Eternal lives deactivated!");
		}
	}
	return true;
}

/* ---------- Widget callbacks ---------- */

void pbNibblesPlay(ft2_instance_t *inst) { inst->uiState.nibblesPlayRequested = true; }
void pbNibblesHelp(ft2_instance_t *inst) { inst->uiState.nibblesHelpRequested = true; }
void pbNibblesHighScores(ft2_instance_t *inst) { inst->uiState.nibblesHighScoreRequested = true; }
void pbNibblesExit(ft2_instance_t *inst) { inst->uiState.nibblesExitRequested = true; }

void rbNibbles1Player(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	inst->nibbles.numPlayers = 0;
	if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_1PLAYER);
}

void rbNibbles2Players(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	inst->nibbles.numPlayers = 1;
	if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_2PLAYER);
}

void rbNibblesNovice(ft2_instance_t *inst)  { if (!inst) return; inst->nibbles.speed = 0; ft2_ui_t *ui = (ft2_ui_t*)inst->ui; if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_NOVICE); }
void rbNibblesAverage(ft2_instance_t *inst) { if (!inst) return; inst->nibbles.speed = 1; ft2_ui_t *ui = (ft2_ui_t*)inst->ui; if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_AVERAGE); }
void rbNibblesPro(ft2_instance_t *inst)     { if (!inst) return; inst->nibbles.speed = 2; ft2_ui_t *ui = (ft2_ui_t*)inst->ui; if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_PRO); }
void rbNibblesTriton(ft2_instance_t *inst)  { if (!inst) return; inst->nibbles.speed = 3; ft2_ui_t *ui = (ft2_ui_t*)inst->ui; if (ui) checkRadioButtonNoRedraw(&ui->widgets, RB_NIBBLES_TRITON); }

void cbNibblesSurround(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->nibbles.surround = !inst->nibbles.surround;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ui->widgets.checkBoxChecked[CB_NIBBLES_SURROUND] = inst->nibbles.surround;
}

void cbNibblesGrid(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->nibbles.grid = !inst->nibbles.grid;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ui->widgets.checkBoxChecked[CB_NIBBLES_GRID] = inst->nibbles.grid;
	inst->uiState.nibblesRedrawRequested = inst->nibbles.playing;
}

void cbNibblesWrap(ft2_instance_t *inst)
{
	if (!inst) return;
	inst->nibbles.wrap = !inst->nibbles.wrap;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui) ui->widgets.checkBoxChecked[CB_NIBBLES_WRAP] = inst->nibbles.wrap;
}

