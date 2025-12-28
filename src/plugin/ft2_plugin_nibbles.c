/**
 * @file ft2_plugin_nibbles.c
 * @brief Nibbles snake game for the FT2 plugin.
 *
 * Port of the FT2 easter egg game from ft2_nibbles.c.
 * Uses instance-based state and plugin video/UI systems.
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

/* Forward declarations for high score callbacks */
static void onP1HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData);
static void onP2HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData);

/* Show a message dialog using the plugin dialog system */
static void nibblesShowMessage(ft2_instance_t *inst, const char *headline, const char *text)
{
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
		ft2_dialog_show_message(&ui->dialog, headline, text);
}

/* Speed table - ticks at 70Hz: 12=Novice, 8=Average, 6=Pro, 4=Triton */
const uint8_t nibblesSpeedTable[4] = { 12, 8, 6, 4 };

/* Cheat codes */
static const char nibblesCheatCode1[] = "skip";   /* During game: skip to next level */
static const char nibblesCheatCode2[] = "triton"; /* Not during game: eternal lives */

/* Help text */
static const char *nibblesHelpText[] =
{
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

/* Default high scores (same as standalone) */
static const ft2_nibbles_highscore_t defaultHighScores[10] =
{
	/* nameLen, name, score, level */
	{ 5, "Vogue",  0x01500000, 23 },
	{ 4, "Mr.H",   0x01400000, 20 },
	{ 5, "Texel",  0x01250000, 18 },
	{ 4, "Tran",   0x01200000, 16 },
	{ 4, "Zolt",   0x01100000, 14 },
	{ 3, "Mag",    0x00750000, 10 },
	{ 2, "KC",     0x00500000, 7 },
	{ 5, "Raven",  0x00400000, 6 },
	{ 4, "Lone",   0x00200000, 3 },
	{ 3, "Mrd",    0x00100000, 1 }
};

/* Forward declarations */
static void nibblesRedrawScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesCreateLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp);
static void nibblesGenNewNumber(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesNewGame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void nibblesNewLevel(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);
static void drawScoresLives(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp);

/* Get RGB24 luminosity (0-255) */
static uint8_t rgb24ToLuminosity(uint32_t rgb24)
{
	const uint8_t R = (rgb24 >> 16) & 0xFF;
	const uint8_t G = (rgb24 >> 8) & 0xFF;
	const uint8_t B = rgb24 & 0xFF;

	uint8_t hi = 0;
	if (hi < R) hi = R;
	if (hi < G) hi = G;
	if (hi < B) hi = B;

	uint8_t lo = 255;
	if (lo > R) lo = R;
	if (lo > G) lo = G;
	if (lo > B) lo = B;

	return (hi + lo) >> 1;
}

/* Check if wall colors are too close to black to see */
static bool wallColorsAreCloseToBlack(ft2_video_t *video)
{
#define LUMINOSITY_THRESHOLD 4
	const uint8_t wallColor1L = rgb24ToLuminosity(video->palette[PAL_DESKTOP]);
	const uint8_t wallColor2L = rgb24ToLuminosity(video->palette[PAL_BUTTONS]);

	if (wallColor1L <= LUMINOSITY_THRESHOLD || wallColor2L <= LUMINOSITY_THRESHOLD)
		return true;

	return false;
}

/* Draw food number (0-9) at position */
static void drawNibblesFoodNumber(ft2_video_t *video, const ft2_bmp_t *bmp, int32_t xOut, int32_t yOut, int32_t number)
{
	if (number > 9 || !video->frameBuffer || !bmp->font8)
		return;

	uint32_t *dstPtr = &video->frameBuffer[(yOut * SCREEN_W) + xOut];
	uint8_t *srcPtr = &bmp->font8[number * FONT8_CHAR_W];

	for (int32_t y = 0; y < FONT8_CHAR_H; y++, srcPtr += FONT8_WIDTH, dstPtr += SCREEN_W)
	{
		for (int32_t x = 0; x < FONT8_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = video->palette[PAL_FORGRND];
		}
	}
}

/* Set nibble screen dot at coordinates */
static void setNibbleDot(ft2_instance_t *inst, ft2_video_t *video, uint8_t x, uint8_t y, uint8_t c)
{
	const uint16_t xs = 152 + (x * 8);
	const uint16_t ys = 7 + (y * 7);

	if (inst->nibbles.grid)
	{
		fillRect(video, xs + 0, ys + 0, 8, 7, PAL_BUTTON2);
		fillRect(video, xs + 1, ys + 1, 7, 6, c);
	}
	else
	{
		fillRect(video, xs, ys, 8, 7, c);
	}

	inst->nibbles.screen[x][y] = c;
}

/* Add direction to input buffer */
static void nibblesAddBuffer(ft2_instance_t *inst, int16_t bufNum, uint8_t value)
{
	ft2_nibbles_buffer_t *n = &inst->nibbles.inputBuffer[bufNum];
	if (n->length < 8)
	{
		n->data[n->length] = value;
		n->length++;
	}
}

/* Check if buffer has input */
static bool nibblesBufferFull(ft2_instance_t *inst, int16_t bufNum)
{
	return (inst->nibbles.inputBuffer[bufNum].length > 0);
}

/* Get next direction from buffer */
static int16_t nibblesGetBuffer(ft2_instance_t *inst, int16_t bufNum)
{
	ft2_nibbles_buffer_t *n = &inst->nibbles.inputBuffer[bufNum];
	if (n->length > 0)
	{
		const int16_t dataOut = n->data[0];
		memmove(&n->data[0], &n->data[1], 7);
		n->length--;
		return dataOut;
	}
	return -1;
}

/* Load level data from stage bitmap */
static void nibblesGetLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp)
{
	const int32_t readX = 1 + ((NIBBLES_SCREEN_W + 2) * (levelNum % 10));
	const int32_t readY = 1 + ((NIBBLES_SCREEN_H + 2) * (levelNum / 10));

	const uint8_t *stagePtr = &bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + readX];

	for (int32_t y = 0; y < NIBBLES_SCREEN_H; y++)
	{
		for (int32_t x = 0; x < NIBBLES_SCREEN_W; x++)
			inst->nibbles.screen[x][y] = stagePtr[x];

		stagePtr += NIBBLES_STAGES_BMP_WIDTH;
	}
}

/* Create a level */
static void nibblesCreateLevel(ft2_instance_t *inst, int16_t levelNum, const ft2_bmp_t *bmp)
{
	if (levelNum >= NIBBLES_MAX_LEVEL)
		levelNum = NIBBLES_MAX_LEVEL - 1;

	nibblesGetLevel(inst, levelNum, bmp);

	int32_t x1 = 0, x2 = 0, y1 = 0, y2 = 0;

	for (int32_t y = 0; y < NIBBLES_SCREEN_H; y++)
	{
		for (int32_t x = 0; x < NIBBLES_SCREEN_W; x++)
		{
			uint8_t c = inst->nibbles.screen[x][y];
			if (c == 1 || c == 3)
			{
				if (c == 3) { x1 = x; y1 = y; }
				if (c == 1) { x2 = x; y2 = y; }
				inst->nibbles.screen[x][y] = 0;
			}
		}
	}

	const int32_t readX = (NIBBLES_SCREEN_W + 2) * (levelNum % 10);
	const int32_t readY = (NIBBLES_SCREEN_H + 2) * (levelNum / 10);

	inst->nibbles.p1Dir = bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + (readX + 1)];
	inst->nibbles.p2Dir = bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + (readX + 0)];

	inst->nibbles.p1Len = 5;
	inst->nibbles.p2Len = 5;
	inst->nibbles.p1NoClear = 0;
	inst->nibbles.p2NoClear = 0;
	inst->nibbles.number = 0;
	inst->nibbles.inputBuffer[0].length = 0;
	inst->nibbles.inputBuffer[1].length = 0;

	for (int32_t i = 0; i < 256; i++)
	{
		inst->nibbles.p1[i].x = (uint8_t)x1;
		inst->nibbles.p1[i].y = (uint8_t)y1;
		inst->nibbles.p2[i].x = (uint8_t)x2;
		inst->nibbles.p2[i].y = (uint8_t)y2;
	}
}

/* Draw level sprite preview */
static void nibbleWriteLevelSprite(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t xOut, int16_t yOut, int16_t levelNum)
{
	const int32_t readX = (NIBBLES_SCREEN_W + 2) * (levelNum % 10);
	const int32_t readY = (NIBBLES_SCREEN_H + 2) * (levelNum / 10);

	const uint8_t *src = &bmp->nibblesStages[(readY * NIBBLES_STAGES_BMP_WIDTH) + readX];
	uint32_t *dst = &video->frameBuffer[(yOut * SCREEN_W) + xOut];

	for (int32_t y = 0; y < NIBBLES_SCREEN_H + 2; y++)
	{
		for (int32_t x = 0; x < NIBBLES_SCREEN_W + 2; x++)
			dst[x] = video->palette[src[x]];

		src += NIBBLES_STAGES_BMP_WIDTH;
		dst += SCREEN_W;
	}

	/* Overwrite start position pixels */
	video->frameBuffer[(yOut * SCREEN_W) + (xOut + 0)] = video->palette[PAL_FORGRND];
	video->frameBuffer[(yOut * SCREEN_W) + (xOut + 1)] = video->palette[PAL_FORGRND];
}

/* High score text output with clipping */
static void highScoreTextOutClipX(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t x, uint16_t y,
	uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr, uint16_t clipX)
{
	if (!textPtr) return;

	uint16_t currX = x;
	for (uint16_t i = 0; i < 22; i++)
	{
		const char ch = textPtr[i];
		if (ch == '\0')
			break;

		charOutClipX(video, bmp, currX + 1, y + 1, shadowPaletteIndex, ch, clipX);
		charOutClipX(video, bmp, currX, y, paletteIndex, ch, clipX);

		currX += charWidth(ch);
		if (currX >= clipX)
			break;
	}
}

/* Draw scores and lives */
static void drawScoresLives(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	/* Player 1 */
	hexOutBg(video, bmp, 89, 27, PAL_FORGRND, PAL_DESKTOP, inst->nibbles.p1Score, 8);
	char livesStr[4];
	snprintf(livesStr, sizeof(livesStr), "%02d", (int)(inst->nibbles.p1Lives > 99 ? 99 : inst->nibbles.p1Lives));
	textOutFixed(video, bmp, 131, 39, PAL_FORGRND, PAL_DESKTOP, livesStr);

	/* Player 2 */
	hexOutBg(video, bmp, 89, 75, PAL_FORGRND, PAL_DESKTOP, inst->nibbles.p2Score, 8);
	snprintf(livesStr, sizeof(livesStr), "%02d", (int)(inst->nibbles.p2Lives > 99 ? 99 : inst->nibbles.p2Lives));
	textOutFixed(video, bmp, 131, 87, PAL_FORGRND, PAL_DESKTOP, livesStr);
}

/* Redraw the entire nibbles screen */
static void nibblesRedrawScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst->nibbles.playing)
		return;

	for (int16_t x = 0; x < NIBBLES_SCREEN_W; x++)
	{
		for (int16_t y = 0; y < NIBBLES_SCREEN_H; y++)
		{
			const int16_t xs = 152 + (x * 8);
			const int16_t ys = 7 + (y * 7);

			const uint8_t c = inst->nibbles.screen[x][y];
			if (c < 16)
			{
				if (inst->nibbles.grid)
				{
					fillRect(video, xs, ys, 8, 7, PAL_BUTTON2);
					fillRect(video, xs + 1, ys + 1, 7, 6, c);
				}
				else
				{
					fillRect(video, xs, ys, 8, 7, c);
				}
			}
			else
			{
				drawNibblesFoodNumber(video, bmp, xs + 2, ys, inst->nibbles.number);
			}
		}
	}

	/* Fix wrongly rendered grid */
	if (inst->nibbles.grid)
	{
		vLine(video, 560, 7, 161, PAL_BUTTON2);
		hLine(video, 152, 168, 409, PAL_BUTTON2);
	}
	else
	{
		vLine(video, 560, 7, 161, PAL_BCKGRND);
		hLine(video, 152, 168, 409, PAL_BCKGRND);
	}
}

/* Public wrapper for redrawing the game screen */
void ft2_nibbles_redraw(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	nibblesRedrawScreen(inst, video, bmp);
}

/* Check if position is invalid (collision) */
static bool nibblesInvalid(ft2_instance_t *inst, int16_t x, int16_t y, int16_t d)
{
	if (!inst->nibbles.wrap)
	{
		if ((x == 0 && d == 0) || (x == 50 && d == 2) || (y == 0 && d == 3) || (y == 22 && d == 1))
			return true;
	}

	if (x >= 0 && x < NIBBLES_SCREEN_W && y >= 0 && y < NIBBLES_SCREEN_H)
		return (inst->nibbles.screen[x][y] >= 1 && inst->nibbles.screen[x][y] <= 15);

	return true;
}

/* Erase the current number */
static void nibblesEraseNumber(ft2_instance_t *inst, ft2_video_t *video)
{
	if (!inst->nibbles.surround)
		setNibbleDot(inst, video, (uint8_t)inst->nibbles.numberX, (uint8_t)inst->nibbles.numberY, 0);
}

/* Generate a new food number */
static void nibblesGenNewNumber(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	while (true)
	{
		int16_t x = rand() % NIBBLES_SCREEN_W;
		int16_t y = rand() % NIBBLES_SCREEN_H;

		bool blockIsSuitable;
		if (y < NIBBLES_SCREEN_H - 1)
			blockIsSuitable = inst->nibbles.screen[x][y] == 0 && inst->nibbles.screen[x][y + 1] == 0;
		else
			blockIsSuitable = inst->nibbles.screen[x][y] == 0;

		if (blockIsSuitable)
		{
			inst->nibbles.number++;
			inst->nibbles.screen[x][y] = (uint8_t)(16 + inst->nibbles.number);
			inst->nibbles.numberX = x;
			inst->nibbles.numberY = y;

			const int16_t xs = 152 + (x * 8);
			const int16_t ys = 7 + (y * 7);

			if (inst->nibbles.grid)
			{
				fillRect(video, xs, ys, 8, 7, PAL_BUTTON2);
				fillRect(video, xs + 1, ys + 1, 7, 6, PAL_BCKGRND);
			}
			else
			{
				fillRect(video, xs, ys, 8, 7, PAL_BCKGRND);
			}

			drawNibblesFoodNumber(video, bmp, (x * 8) + 154, (y * 7) + 7, inst->nibbles.number);
			break;
		}
	}
}

/* Start new game at current level */
static void nibblesNewGame(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	nibblesCreateLevel(inst, inst->nibbles.level, bmp);
	nibblesRedrawScreen(inst, video, bmp);

	setNibbleDot(inst, video, inst->nibbles.p1[0].x, inst->nibbles.p1[0].y, 6);
	if (inst->nibbles.numPlayers == 1)
		setNibbleDot(inst, video, inst->nibbles.p2[0].x, inst->nibbles.p2[0].y, 7);

	if (!inst->nibbles.surround)
		nibblesGenNewNumber(inst, video, bmp);
}

/* Advance to next level */
static void nibblesNewLevel(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	char text[32];
	snprintf(text, sizeof(text), "Level %d finished!", inst->nibbles.level + 1);
	nibblesShowMessage(inst, "Nibbles message", text);

	/* Calculate bonus (cast to int16_t to simulate FT2 bug) */
	inst->nibbles.p1Score += 0x10000 + (int16_t)((12 - inst->nibbles.curSpeed) * 0x2000);
	if (inst->nibbles.numPlayers == 1)
		inst->nibbles.p2Score += 0x10000;

	inst->nibbles.level++;

	if (inst->nibbles.p1Lives < 99)
		inst->nibbles.p1Lives++;

	if (inst->nibbles.numPlayers == 1)
	{
		if (inst->nibbles.p2Lives < 99)
			inst->nibbles.p2Lives++;
	}

	inst->nibbles.number = 0;
	nibblesCreateLevel(inst, inst->nibbles.level, bmp);
	nibblesRedrawScreen(inst, video, bmp);
	nibblesGenNewNumber(inst, video, bmp);
}

/* Handle player death */
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

			ft2_ui_t *ui = ft2_ui_get_current();
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

			ft2_ui_t *ui = ft2_ui_get_current();
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

/* Callback when Player 1 enters their high score name */
static void onP1HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)userData;

	if (inst->nibbles.pendingP1HighScore && inst->nibbles.pendingP1Slot >= 0)
	{
		ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[inst->nibbles.pendingP1Slot];

		if (result == DIALOG_RESULT_OK && inputText != NULL && inputText[0] != '\0')
		{
			size_t len = strlen(inputText);
			if (len > 21) len = 21;
			memset(h->name, 0, sizeof(h->name));
			memcpy(h->name, inputText, len);
			h->nameLen = (uint8_t)len;
		}
		/* else keep the default "Unknown" name */

		inst->nibbles.pendingP1HighScore = false;
		inst->nibbles.pendingP1Slot = -1;
	}

	/* Now check if Player 2 also has a high score */
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

		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
		{
			ft2_dialog_show_input_cb(&ui->dialog,
				"Player 2 - Enter your name:", "",
				"Unknown", 21, inst, onP2HighScoreNameEntered, NULL);
		}
		return;
	}

	/* No more high scores to enter, show the table */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
		ft2_nibbles_show_highscores(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
}

/* Callback when Player 2 enters their high score name */
static void onP2HighScoreNameEntered(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)userData;

	if (inst->nibbles.pendingP2HighScore && inst->nibbles.pendingP2Slot >= 0)
	{
		ft2_nibbles_highscore_t *h = &inst->nibbles.highScores[inst->nibbles.pendingP2Slot];

		if (result == DIALOG_RESULT_OK && inputText != NULL && inputText[0] != '\0')
		{
			size_t len = strlen(inputText);
			if (len > 21) len = 21;
			memset(h->name, 0, sizeof(h->name));
			memcpy(h->name, inputText, len);
			h->nameLen = (uint8_t)len;
		}

		inst->nibbles.pendingP2HighScore = false;
		inst->nibbles.pendingP2Slot = -1;
	}

	/* Show the high score table */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
		ft2_nibbles_show_highscores(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
}

/* Callback for "Restart game?" confirmation */
static void onRestartGameConfirm(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result == DIALOG_RESULT_YES)
	{
		/* User confirmed restart - stop current game and start new */
		inst->nibbles.playing = false;

		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
			ft2_nibbles_play(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
	}
}

/* Callback for "Quit game?" confirmation */
static void onQuitGameConfirm(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result == DIALOG_RESULT_YES)
	{
		inst->nibbles.playing = false;
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
			ft2_nibbles_exit(inst, &ui->video, ui->bmpLoaded ? &ui->bmp : NULL);
	}
}

/* Scale 70Hz timing to 60Hz (fewer frames = smaller countdown) */
#define SCALE_VBLANK_DELTA_REV(x) ((int32_t)round((x) * (60.0 / 70.0)))

/* Initialize nibbles state */
void ft2_nibbles_init(ft2_instance_t *inst)
{
	memset(&inst->nibbles, 0, sizeof(ft2_nibbles_state_t));
	inst->nibbles.speed = 0;  /* Novice */
	inst->nibbles.numPlayers = 0;  /* 1 player */
	inst->nibbles.grid = true;
	inst->nibbles.wrap = false;
	inst->nibbles.surround = false;
	ft2_nibbles_load_default_highscores(inst);
}

/* Load default high scores */
void ft2_nibbles_load_default_highscores(ft2_instance_t *inst)
{
	memcpy(inst->nibbles.highScores, defaultHighScores, sizeof(defaultHighScores));
}

/* Show nibbles screen */
void ft2_nibbles_show(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	/* Hide extended pattern editor if shown */
	if (inst->uiState.extendedPatternEditor)
	{
		inst->uiState.extendedPatternEditor = false;
		inst->uiState.needsFullRedraw = true;
	}

	hideTopScreen(inst);
	inst->uiState.nibblesShown = true;

	/* Draw framework */
	drawFramework(video, 0, 0, 632, 3, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 3, 148, 49, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 52, 148, 49, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 101, 148, 72, FRAMEWORK_TYPE1);
	drawFramework(video, 148, 3, 417, 170, FRAMEWORK_TYPE1);
	drawFramework(video, 150, 5, 413, 166, FRAMEWORK_TYPE2);
	drawFramework(video, 565, 3, 67, 170, FRAMEWORK_TYPE1);

	/* Draw labels */
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

	/* Draw initial scores/lives */
	drawScoresLives(inst, video, bmp);

	/* Draw nibbles logo */
	blitFast(video, 569, 7, bmp->nibblesLogo, 59, 91);

	/* Show buttons */
	showPushButton(video, bmp, PB_NIBBLES_PLAY);
	showPushButton(video, bmp, PB_NIBBLES_HELP);
	showPushButton(video, bmp, PB_NIBBLES_HIGHS);
	showPushButton(video, bmp, PB_NIBBLES_EXIT);

	/* Show checkboxes */
	checkBoxes[CB_NIBBLES_SURROUND].checked = inst->nibbles.surround;
	checkBoxes[CB_NIBBLES_GRID].checked = inst->nibbles.grid;
	checkBoxes[CB_NIBBLES_WRAP].checked = inst->nibbles.wrap;
	showCheckBox(video, bmp, CB_NIBBLES_SURROUND);
	showCheckBox(video, bmp, CB_NIBBLES_GRID);
	showCheckBox(video, bmp, CB_NIBBLES_WRAP);

	/* Show player radio buttons */
	uncheckRadioButtonGroup(RB_GROUP_NIBBLES_PLAYERS);
	if (inst->nibbles.numPlayers == 0)
		radioButtons[RB_NIBBLES_1PLAYER].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_NIBBLES_2PLAYER].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(video, bmp, RB_GROUP_NIBBLES_PLAYERS);

	/* Show difficulty radio buttons */
	uncheckRadioButtonGroup(RB_GROUP_NIBBLES_DIFFICULTY);
	switch (inst->nibbles.speed)
	{
		default:
		case 0: radioButtons[RB_NIBBLES_NOVICE].state = RADIOBUTTON_CHECKED; break;
		case 1: radioButtons[RB_NIBBLES_AVERAGE].state = RADIOBUTTON_CHECKED; break;
		case 2: radioButtons[RB_NIBBLES_PRO].state = RADIOBUTTON_CHECKED; break;
		case 3: radioButtons[RB_NIBBLES_TRITON].state = RADIOBUTTON_CHECKED; break;
	}
	showRadioButtonGroup(video, bmp, RB_GROUP_NIBBLES_DIFFICULTY);
}

/* Hide nibbles screen */
void ft2_nibbles_hide(ft2_instance_t *inst)
{
	hidePushButton(PB_NIBBLES_PLAY);
	hidePushButton(PB_NIBBLES_HELP);
	hidePushButton(PB_NIBBLES_HIGHS);
	hidePushButton(PB_NIBBLES_EXIT);

	hideRadioButtonGroup(RB_GROUP_NIBBLES_PLAYERS);
	hideRadioButtonGroup(RB_GROUP_NIBBLES_DIFFICULTY);

	hideCheckBox(CB_NIBBLES_SURROUND);
	hideCheckBox(CB_NIBBLES_GRID);
	hideCheckBox(CB_NIBBLES_WRAP);

	inst->uiState.nibblesShown = false;
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = false;
}

/* Exit nibbles screen */
void ft2_nibbles_exit(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	(void)video;
	(void)bmp;

	/* If game is playing, ask for confirmation */
	if (inst->nibbles.playing)
	{
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
		{
			ft2_dialog_show_yesno_cb(&ui->dialog,
				"System request", "Quit current game of Nibbles?",
				inst, onQuitGameConfirm, NULL);
		}
		return;
	}

	ft2_nibbles_hide(inst);
	
	/* Restore main screen state - matches pattern from pbExitAbout */
	inst->uiState.scopesShown = true;
	inst->uiState.instrSwitcherShown = true;
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.updateInstrSwitcher = true;
}

/* Start a game */
void ft2_nibbles_play(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing)
	{
		/* Already playing - ask to restart */
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
		{
			ft2_dialog_show_yesno_cb(&ui->dialog,
				"Nibbles request", "Restart current game of Nibbles?",
				inst, onRestartGameConfirm, NULL);
		}
		return;
	}

	if (inst->nibbles.surround && inst->nibbles.numPlayers == 0)
	{
		nibblesShowMessage(inst, "Nibbles message", "Surround mode is not appropriate in one-player mode.");
		return;
	}

	if (wallColorsAreCloseToBlack(video))
		nibblesShowMessage(inst, "Nibbles warning", "The Desktop/Button colors are set to values that make the walls hard to see!");

	inst->nibbles.curSpeed = nibblesSpeedTable[inst->nibbles.speed];

	/* Adjust for 70Hz -> 60Hz countdown frames */
	inst->nibbles.curSpeed60Hz = (uint8_t)SCALE_VBLANK_DELTA_REV(inst->nibbles.curSpeed);
	inst->nibbles.curTick60Hz = (uint8_t)SCALE_VBLANK_DELTA_REV(nibblesSpeedTable[2]);

	/* Clear help/highscore overlays when starting game */
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = false;

	inst->nibbles.playing = true;
	inst->nibbles.p1Score = 0;
	inst->nibbles.p2Score = 0;
	inst->nibbles.p1Lives = 5;
	inst->nibbles.p2Lives = 5;
	inst->nibbles.level = 0;

	nibblesNewGame(inst, video, bmp);
}

/* Show high scores */
void ft2_nibbles_show_highscores(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing)
	{
		nibblesShowMessage(inst, "Nibbles message", "The highscore table is not available during play.");
		return;
	}

	/* Mark high scores as shown (prevents framework redraw from overwriting) */
	inst->uiState.nibblesHelpShown = false;
	inst->uiState.nibblesHighScoresShown = true;

	clearRect(video, 152, 7, 409, 162);

	bigTextOut(video, bmp, 160, 10, PAL_FORGRND, "Fasttracker Nibbles Highscore");
	for (int16_t i = 0; i < 5; i++)
	{
		highScoreTextOutClipX(video, bmp, 160, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i].name, 160 + 70);
		hexOutShadow(video, bmp, 160 + 76, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i].score, 8);
		nibbleWriteLevelSprite(video, bmp, 160 + 136, (42 - 9) + (26 * i), inst->nibbles.highScores[i].level);

		highScoreTextOutClipX(video, bmp, 360, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i + 5].name, 360 + 70);
		hexOutShadow(video, bmp, 360 + 76, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, inst->nibbles.highScores[i + 5].score, 8);
		nibbleWriteLevelSprite(video, bmp, 360 + 136, (42 - 9) + (26 * i), inst->nibbles.highScores[i + 5].level);
	}
}

/* Show help */
void ft2_nibbles_show_help(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst->nibbles.playing)
	{
		nibblesShowMessage(inst, "System message", "Help is not available during play.");
		return;
	}

	/* Mark help as shown (prevents framework redraw from overwriting) */
	inst->uiState.nibblesHelpShown = true;
	inst->uiState.nibblesHighScoresShown = false;

	clearRect(video, 152, 7, 409, 162);

	bigTextOut(video, bmp, 160, 10, PAL_FORGRND, "Fasttracker Nibbles Help");
	for (uint16_t i = 0; i < NIBBLES_HELP_LINES; i++)
		textOut(video, bmp, 160, 36 + (11 * i), PAL_BUTTONS, nibblesHelpText[i]);
}

/* Game tick - called at ~60Hz */
void ft2_nibbles_tick(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst->nibbles.playing || inst->uiState.sysReqShown)
		return;

	/* Also pause if a dialog is active (player died, quit confirmation, etc.) */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL && ft2_dialog_is_active(&ui->dialog))
		return;

	if (--inst->nibbles.curTick60Hz != 0)
		return;

	/* Process input buffers */
	if (nibblesBufferFull(inst, 0))
	{
		switch (nibblesGetBuffer(inst, 0))
		{
			case 0: if (inst->nibbles.p1Dir != 2) inst->nibbles.p1Dir = 0; break;
			case 1: if (inst->nibbles.p1Dir != 3) inst->nibbles.p1Dir = 1; break;
			case 2: if (inst->nibbles.p1Dir != 0) inst->nibbles.p1Dir = 2; break;
			case 3: if (inst->nibbles.p1Dir != 1) inst->nibbles.p1Dir = 3; break;
			default: break;
		}
	}

	if (nibblesBufferFull(inst, 1))
	{
		switch (nibblesGetBuffer(inst, 1))
		{
			case 0: if (inst->nibbles.p2Dir != 2) inst->nibbles.p2Dir = 0; break;
			case 1: if (inst->nibbles.p2Dir != 3) inst->nibbles.p2Dir = 1; break;
			case 2: if (inst->nibbles.p2Dir != 0) inst->nibbles.p2Dir = 2; break;
			case 3: if (inst->nibbles.p2Dir != 1) inst->nibbles.p2Dir = 3; break;
			default: break;
		}
	}

	/* Move players */
	memmove(&inst->nibbles.p1[1], &inst->nibbles.p1[0], 255 * sizeof(ft2_nibbles_coord_t));
	if (inst->nibbles.numPlayers == 1)
		memmove(&inst->nibbles.p2[1], &inst->nibbles.p2[0], 255 * sizeof(ft2_nibbles_coord_t));

	switch (inst->nibbles.p1Dir)
	{
		case 0: inst->nibbles.p1[0].x++; break;
		case 1: inst->nibbles.p1[0].y--; break;
		case 2: inst->nibbles.p1[0].x--; break;
		case 3: inst->nibbles.p1[0].y++; break;
		default: break;
	}

	if (inst->nibbles.numPlayers == 1)
	{
		switch (inst->nibbles.p2Dir)
		{
			case 0: inst->nibbles.p2[0].x++; break;
			case 1: inst->nibbles.p2[0].y--; break;
			case 2: inst->nibbles.p2[0].x--; break;
			case 3: inst->nibbles.p2[0].y++; break;
			default: break;
		}
	}

	/* Handle wrapping */
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

/* Add input to buffer */
void ft2_nibbles_add_input(ft2_instance_t *inst, int16_t playerNum, uint8_t direction)
{
	nibblesAddBuffer(inst, playerNum, direction);
}

/* Handle key input during game */
bool ft2_nibbles_handle_key(ft2_instance_t *inst, int32_t keyCode)
{
	if (!inst->nibbles.playing)
		return false;

	/* Handle escape - ask to quit */
	if (keyCode == 27) /* Escape */
	{
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
		{
			ft2_dialog_show_yesno_cb(&ui->dialog,
				"System request", "Quit current game of Nibbles?",
				inst, onQuitGameConfirm, NULL);
		}
		return true;
	}

	/* Player 1: Arrow keys */
	switch (keyCode)
	{
		case FT2_KEY_RIGHT:
			nibblesAddBuffer(inst, 0, 0);
			return true;
		case FT2_KEY_UP:
			nibblesAddBuffer(inst, 0, 1);
			return true;
		case FT2_KEY_LEFT:
			nibblesAddBuffer(inst, 0, 2);
			return true;
		case FT2_KEY_DOWN:
			nibblesAddBuffer(inst, 0, 3);
			return true;
	}

	/* Player 2: WASD */
	if (keyCode == 'd' || keyCode == 'D')
	{
		nibblesAddBuffer(inst, 1, 0);
		return true;
	}
	if (keyCode == 'w' || keyCode == 'W')
	{
		nibblesAddBuffer(inst, 1, 1);
		return true;
	}
	if (keyCode == 'a' || keyCode == 'A')
	{
		nibblesAddBuffer(inst, 1, 2);
		return true;
	}
	if (keyCode == 's' || keyCode == 'S')
	{
		nibblesAddBuffer(inst, 1, 3);
		return true;
	}

	return false;
}

/* Test for cheat codes */
bool ft2_nibbles_test_cheat(ft2_instance_t *inst, int32_t keyCode,
	bool shiftPressed, bool ctrlPressed, bool altPressed,
	ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!shiftPressed || !ctrlPressed || !altPressed)
		return false;

	const char *codeStringPtr;
	uint8_t codeStringLen;

	if (inst->nibbles.playing)
	{
		/* During game: "skip" to next level */
		codeStringPtr = nibblesCheatCode1;
		codeStringLen = sizeof(nibblesCheatCode1) - 1;
	}
	else
	{
		/* Not during game: "triton" for eternal lives */
		codeStringPtr = nibblesCheatCode2;
		codeStringLen = sizeof(nibblesCheatCode2) - 1;
	}

	inst->nibbles.cheatBuffer[inst->nibbles.cheatIndex] = (char)keyCode;
	if (inst->nibbles.cheatBuffer[inst->nibbles.cheatIndex] != codeStringPtr[inst->nibbles.cheatIndex])
	{
		inst->nibbles.cheatIndex = 0;
		return true;
	}

	if (++inst->nibbles.cheatIndex == codeStringLen)
	{
		inst->nibbles.cheatIndex = 0;

		if (inst->nibbles.playing)
		{
			nibblesNewLevel(inst, video, bmp);
		}
		else
		{
			inst->nibbles.eternalLives = !inst->nibbles.eternalLives;
			if (inst->nibbles.eternalLives)
				nibblesShowMessage(inst, "Triton productions declares:", "Eternal lives activated!");
			else
				nibblesShowMessage(inst, "Triton productions declares:", "Eternal lives deactivated!");
		}
	}

	return true;
}

/* Button callbacks */
void pbNibblesPlay(ft2_instance_t *inst)
{
	/* Deferred - will be called with video/bmp */
	inst->uiState.nibblesPlayRequested = true;
}

void pbNibblesHelp(ft2_instance_t *inst)
{
	inst->uiState.nibblesHelpRequested = true;
}

void pbNibblesHighScores(ft2_instance_t *inst)
{
	inst->uiState.nibblesHighScoreRequested = true;
}

void pbNibblesExit(ft2_instance_t *inst)
{
	inst->uiState.nibblesExitRequested = true;
}

void rbNibbles1Player(ft2_instance_t *inst)
{
	inst->nibbles.numPlayers = 0;
	checkRadioButtonNoRedraw(RB_NIBBLES_1PLAYER);
}

void rbNibbles2Players(ft2_instance_t *inst)
{
	inst->nibbles.numPlayers = 1;
	checkRadioButtonNoRedraw(RB_NIBBLES_2PLAYER);
}

void rbNibblesNovice(ft2_instance_t *inst)
{
	inst->nibbles.speed = 0;
	checkRadioButtonNoRedraw(RB_NIBBLES_NOVICE);
}

void rbNibblesAverage(ft2_instance_t *inst)
{
	inst->nibbles.speed = 1;
	checkRadioButtonNoRedraw(RB_NIBBLES_AVERAGE);
}

void rbNibblesPro(ft2_instance_t *inst)
{
	inst->nibbles.speed = 2;
	checkRadioButtonNoRedraw(RB_NIBBLES_PRO);
}

void rbNibblesTriton(ft2_instance_t *inst)
{
	inst->nibbles.speed = 3;
	checkRadioButtonNoRedraw(RB_NIBBLES_TRITON);
}

void cbNibblesSurround(ft2_instance_t *inst)
{
	inst->nibbles.surround = !inst->nibbles.surround;
	checkBoxes[CB_NIBBLES_SURROUND].checked = inst->nibbles.surround;
}

void cbNibblesGrid(ft2_instance_t *inst)
{
	inst->nibbles.grid = !inst->nibbles.grid;
	checkBoxes[CB_NIBBLES_GRID].checked = inst->nibbles.grid;
	/* Redraw screen if playing */
	inst->uiState.nibblesRedrawRequested = inst->nibbles.playing;
}

void cbNibblesWrap(ft2_instance_t *inst)
{
	inst->nibbles.wrap = !inst->nibbles.wrap;
	checkBoxes[CB_NIBBLES_WRAP].checked = inst->nibbles.wrap;
}

