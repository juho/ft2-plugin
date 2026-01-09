/*
** FT2 Plugin - Textbox Input System
** Port of ft2_textboxes.c: text editing for instrument/sample names, song name, etc.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_instance.h"

static textBox_t textBoxes[NUM_TEXTBOXES];
static int16_t activeTextBox = -1;
static bool textEditActive = false;
static int16_t textBoxNeedsRedraw = -1;

/* ------------------------------------------------------------------------- */
/*                          TEXT RENDERING                                   */
/* ------------------------------------------------------------------------- */

/* Renders text to an offscreen buffer for clipped blitting */
static void textOutBuf(const ft2_bmp_t *bmp, uint8_t *dstBuffer, uint32_t dstWidth, 
                       uint8_t paletteIndex, const char *text, uint32_t maxTextLen)
{
	if (!text || !*text || !bmp || !bmp->font1) return;

	uint16_t currX = 0;
	for (uint32_t i = 0; i < maxTextLen; i++)
	{
		const char chr = *text++ & 0x7F;
		if (chr == '\0') break;

		if (chr != ' ')
		{
			const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
			uint8_t *dstPtr = &dstBuffer[currX];
			for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
			{
				for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
					if (srcPtr[x]) dstPtr[x] = paletteIndex;
				srcPtr += FONT1_WIDTH;
				dstPtr += dstWidth;
			}
		}
		currX += charWidth(chr);
	}
}

/* ------------------------------------------------------------------------- */
/*                             HELPERS                                       */
/* ------------------------------------------------------------------------- */

/* Returns on-screen X offset for cursor, accounting for scroll */
static int16_t cursorPosToX(textBox_t *t)
{
	if (!t->textPtr) return -1;
	int32_t x = -1;
	for (int16_t i = 0; i < t->cursorPos; i++) x += charWidth(t->textPtr[i]);
	return (int16_t)(x - t->bufOffset);
}

static void scrollTextBufferLeft(textBox_t *t)
{
	t->bufOffset -= TEXT_SCROLL_VALUE;
	if (t->bufOffset < 0) t->bufOffset = 0;
}

static void scrollTextBufferRight(textBox_t *t, uint16_t numCharsInText)
{
	if (!t->textPtr) return;
	int32_t textEnd = 0;
	for (uint16_t j = 0; j < numCharsInText; j++) textEnd += charWidth(t->textPtr[j]);
	textEnd -= t->renderW;
	if (textEnd < 0) textEnd = 0;
	t->bufOffset += TEXT_SCROLL_VALUE;
	if (t->bufOffset > textEnd) t->bufOffset = textEnd;
}

static int16_t getTextLength(textBox_t *t)
{
	return t->textPtr ? (int16_t)strlen(t->textPtr) : 0;
}

static void allocTextBoxRenderBuf(textBox_t *t)
{
	t->renderBufW = (9 + 1) * t->maxChars; /* 9 = max glyph width + 1 kerning */
	t->renderBufH = 10;
	t->renderW = t->w - (t->tx * 2);
	t->bufOffset = 0;
	t->renderBuf = (uint8_t *)malloc(t->renderBufW * t->renderBufH);
}

/* ------------------------------------------------------------------------- */
/*                         INITIALIZATION                                    */
/* ------------------------------------------------------------------------- */

void ft2_textbox_init(void)
{
	memset(textBoxes, 0, sizeof(textBoxes));

	/* Instrument name textboxes (8 visible, right-click to edit) */
	static const uint16_t instY[8] = { 5, 16, 27, 38, 49, 60, 71, 82 };
	for (int i = 0; i < 8; i++)
	{
		textBox_t *t = &textBoxes[TB_INST1 + i];
		t->x = 446; t->y = instY[i]; t->w = 140; t->h = 10;
		t->tx = 1; t->ty = 0; t->maxChars = 22;
		t->rightMouseButton = true; t->visible = true;
		allocTextBoxRenderBuf(t);
	}

	/* Sample name textboxes (5 visible, right-click to edit) */
	static const uint16_t sampY[5] = { 99, 110, 121, 132, 143 };
	for (int i = 0; i < 5; i++)
	{
		textBox_t *t = &textBoxes[TB_SAMP1 + i];
		t->x = 446; t->y = sampY[i]; t->w = 116; t->h = 10;
		t->tx = 1; t->ty = 0; t->maxChars = 22;
		t->rightMouseButton = true; t->visible = true;
		allocTextBoxRenderBuf(t);
	}

	/* Song name textbox (left-click to edit) */
	{
		textBox_t *t = &textBoxes[TB_SONG_NAME];
		t->x = 424; t->y = 158; t->w = 160; t->h = 12;
		t->tx = 2; t->ty = 1; t->maxChars = 20;
		t->rightMouseButton = false; t->visible = true;
		allocTextBoxRenderBuf(t);
	}

	/* Disk op filename textbox */
	{
		textBox_t *t = &textBoxes[TB_DISKOP_FILENAME];
		t->x = 31; t->y = 158; t->w = 134; t->h = 12;
		t->tx = 2; t->ty = 1; t->maxChars = 255;
		t->rightMouseButton = false; t->visible = false;
		allocTextBoxRenderBuf(t);
	}

	/* Dialog input textbox (configured dynamically) */
	{
		textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
		t->x = 0; t->y = 0; t->w = 250; t->h = 12;
		t->tx = 2; t->ty = 1; t->maxChars = 255;
		t->rightMouseButton = false; t->visible = false; t->textPtr = NULL;
		allocTextBoxRenderBuf(t);
	}

	activeTextBox = -1;
	textEditActive = false;
}

void ft2_textbox_update_pointers(ft2_instance_t *inst)
{
	if (!inst) return;

	/* Instrument name pointers (instrName is 1-indexed) */
	for (int i = 0; i < 8; i++)
	{
		uint8_t instrIdx = inst->editor.instrBankOffset + i + 1;
		textBoxes[TB_INST1 + i].textPtr = (instrIdx <= FT2_MAX_INST) ? inst->replayer.song.instrName[instrIdx] : NULL;
	}

	/* Sample name pointers */
	ft2_instr_t *curInstr = (inst->editor.curInstr > 0 && inst->editor.curInstr < 128) ? inst->replayer.instr[inst->editor.curInstr] : NULL;
	for (int i = 0; i < 5; i++)
	{
		uint8_t smpIdx = inst->editor.sampleBankOffset + i;
		textBoxes[TB_SAMP1 + i].textPtr = (curInstr && smpIdx < 16) ? curInstr->smp[smpIdx].name : NULL;
	}

	textBoxes[TB_SONG_NAME].textPtr = inst->replayer.song.name;
	textBoxes[TB_DISKOP_FILENAME].textPtr = inst->diskop.filename;
}

/* ------------------------------------------------------------------------- */
/*                          MOUSE INPUT                                      */
/* ------------------------------------------------------------------------- */

/* Positions cursor at mouse X, accounting for scroll offset */
static void moveTextCursorToMouseX(textBox_t *t, int32_t mouseX)
{
	if (!t->textPtr || !t->textPtr[0]) { t->cursorPos = 0; return; }
	if (mouseX == t->x && t->bufOffset == 0) { t->cursorPos = 0; return; }

	int16_t numChars = getTextLength(t);
	const int32_t mx = t->bufOffset + mouseX;
	int32_t tx = (t->x + t->tx) - 1, cw = -1;
	int16_t i;

	for (i = 0; i < numChars; i++)
	{
		cw = charWidth(t->textPtr[i]);
		if (mx >= tx && mx < tx + cw) { t->cursorPos = i; break; }
		tx += cw;
	}

	if (i == numChars && mx >= tx) t->cursorPos = numChars;

	if (cw != -1)
	{
		int16_t cursorX = cursorPosToX(t);
		if (cursorX + cw > t->renderW) scrollTextBufferRight(t, (uint16_t)numChars);
		else if (cursorX < -1) scrollTextBufferLeft(t);
	}
}

int16_t ft2_textbox_test_mouse_down(int32_t x, int32_t y, bool rightButton)
{
	for (int i = 0; i < NUM_TEXTBOXES; i++)
	{
		textBox_t *t = &textBoxes[i];
		if (!t->visible || !t->textPtr) continue;
		if (x < t->x || x >= t->x + t->w || y < t->y || y >= t->y + t->h) continue;

		/* Right-click textboxes require right button */
		if (!rightButton && t->rightMouseButton) continue;

		if (textEditActive && i != activeTextBox) ft2_textbox_exit_editing();

		activeTextBox = (int16_t)i;
		textEditActive = true;
		t->active = true;
		moveTextCursorToMouseX(t, x);
		return (int16_t)i;
	}

	if (textEditActive) ft2_textbox_exit_editing();
	return -1;
}

/* ------------------------------------------------------------------------- */
/*                       KEYBOARD INPUT                                      */
/* ------------------------------------------------------------------------- */

void ft2_textbox_input_char(char c)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES) return;

	textBox_t *t = &textBoxes[activeTextBox];
	if (!t->textPtr) return;

	int16_t len = getTextLength(t);
	if (len >= t->maxChars - 1) return;
	if (c < 32 || c > 126) return;

	if (t->cursorPos >= len)
	{
		t->textPtr[len] = c;
		t->textPtr[len + 1] = '\0';
		t->cursorPos = len + 1;
	}
	else
	{
		memmove(&t->textPtr[t->cursorPos + 1], &t->textPtr[t->cursorPos], len - t->cursorPos + 1);
		t->textPtr[t->cursorPos] = c;
		t->cursorPos++;
	}

	if (cursorPosToX(t) >= t->renderW) scrollTextBufferRight(t, (uint16_t)getTextLength(t));
}

void ft2_textbox_handle_key(int32_t keyCode, int32_t modifiers)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES) return;

	textBox_t *t = &textBoxes[activeTextBox];
	if (!t->textPtr) return;

	int16_t len = getTextLength(t);
	(void)modifiers;

	switch (keyCode)
	{
		case 8: /* Backspace */
			if (t->cursorPos > 0)
			{
				if (t->bufOffset > 0) { t->bufOffset -= charWidth(t->textPtr[t->cursorPos - 1]); if (t->bufOffset < 0) t->bufOffset = 0; }
				memmove(&t->textPtr[t->cursorPos - 1], &t->textPtr[t->cursorPos], len - t->cursorPos + 1);
				t->cursorPos--;
				if (cursorPosToX(t) < -1) scrollTextBufferLeft(t);
			}
			break;

		case 127: /* Delete */
			if (t->cursorPos < len)
			{
				if (t->bufOffset > 0) { t->bufOffset -= charWidth(t->textPtr[t->cursorPos]); if (t->bufOffset < 0) t->bufOffset = 0; }
				memmove(&t->textPtr[t->cursorPos], &t->textPtr[t->cursorPos + 1], len - t->cursorPos);
			}
			break;

		case 0x1000: /* Left */
			if (t->cursorPos > 0) { t->cursorPos--; if (cursorPosToX(t) < -1) scrollTextBufferLeft(t); }
			break;

		case 0x1001: /* Right */
			if (t->cursorPos < len) { t->cursorPos++; if (cursorPosToX(t) >= t->renderW) scrollTextBufferRight(t, (uint16_t)len); }
			break;

		case 0x1006: /* Home */
			t->cursorPos = 0; t->bufOffset = 0;
			break;

		case 0x1007: /* End */
		{
			uint32_t textWidth = 0;
			for (int16_t i = 0; i < len; i++) textWidth += charWidth(t->textPtr[i]);
			t->cursorPos = len;
			t->bufOffset = (textWidth > t->renderW) ? textWidth - t->renderW : 0;
		}
			break;

		case '\r': /* Enter */
		case 27:   /* Escape */
			ft2_textbox_exit_editing();
			break;

		default: break;
	}
}

/* ------------------------------------------------------------------------- */
/*                          STATE CONTROL                                    */
/* ------------------------------------------------------------------------- */

void ft2_textbox_exit_editing(void)
{
	if (activeTextBox >= 0 && activeTextBox < NUM_TEXTBOXES)
	{
		textBoxes[activeTextBox].active = false;
		textBoxes[activeTextBox].bufOffset = 0;
		textBoxNeedsRedraw = activeTextBox;
	}
	activeTextBox = -1;
	textEditActive = false;
}

int16_t ft2_textbox_get_needs_redraw(void)
{
	int16_t result = textBoxNeedsRedraw;
	textBoxNeedsRedraw = -1;
	return result;
}

bool ft2_textbox_is_editing(void) { return textEditActive; }
int16_t ft2_textbox_get_active(void) { return activeTextBox; }

void ft2_textbox_draw(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t textBoxID, const ft2_instance_t *inst)
{
	ft2_textbox_draw_with_cursor(video, bmp, textBoxID, false, inst);
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

/* Returns background palette based on textbox type and selection state */
static uint8_t getTextBoxBackgroundPal(uint16_t textBoxID, const ft2_instance_t *inst)
{
	if (!inst) return PAL_BCKGRND;

	if (textBoxID >= TB_INST1 && textBoxID <= TB_INST8)
	{
		uint8_t displayedInstr = inst->editor.instrBankOffset + 1 + (textBoxID - TB_INST1);
		return (displayedInstr == inst->editor.curInstr) ? PAL_BUTTONS : PAL_BCKGRND;
	}

	if (textBoxID >= TB_SAMP1 && textBoxID <= TB_SAMP5)
	{
		uint8_t displayedSmp = inst->editor.sampleBankOffset + (textBoxID - TB_SAMP1);
		return (displayedSmp == inst->editor.curSmp) ? PAL_BUTTONS : PAL_BCKGRND;
	}

	return PAL_BCKGRND;
}

void ft2_textbox_draw_with_cursor(ft2_video_t *video, const ft2_bmp_t *bmp, 
                                  uint16_t textBoxID, bool showCursor, 
                                  const ft2_instance_t *inst)
{
	if (textBoxID >= NUM_TEXTBOXES || !video) return;

	textBox_t *t = &textBoxes[textBoxID];
	if (!t->visible || !t->renderBuf) return;

	uint8_t bgPal = getTextBoxBackgroundPal(textBoxID, inst);

	/* Render text to offscreen buffer */
	memset(t->renderBuf, PAL_TRANSPR, t->renderBufW * t->renderBufH);
	if (t->textPtr && bmp) textOutBuf(bmp, t->renderBuf, t->renderBufW, PAL_FORGRND, t->textPtr, t->maxChars);

	/* Draw background and clear cursor extension rows */
	fillRect(video, t->x + t->tx - 1, t->y + t->ty, t->renderW + 1, 10, bgPal);
	hLine(video, t->x + t->tx - 1, t->y + t->ty - 1, t->renderW + 1, PAL_BCKGRND);
	hLine(video, t->x + t->tx - 1, t->y + t->ty + 10, t->renderW + 1, PAL_BCKGRND);

	/* Blit visible portion with clipping */
	blitClipX(video, t->x + t->tx, t->y + t->ty, &t->renderBuf[t->bufOffset], t->renderBufW, t->renderBufH, t->renderW);

	/* Draw cursor if active and visible */
	if (t->active && showCursor && t->textPtr)
	{
		int16_t cursorX = t->x + t->tx + cursorPosToX(t);
		if (cursorX >= t->x + t->tx - 1 && cursorX < t->x + t->tx + t->renderW)
			vLine(video, cursorX, t->y + t->ty - 1, 12, PAL_FORGRND);
	}
}

/* ------------------------------------------------------------------------- */
/*                         VISIBILITY                                        */
/* ------------------------------------------------------------------------- */

void ft2_textbox_show(uint16_t textBoxID)
{
	if (textBoxID < NUM_TEXTBOXES) textBoxes[textBoxID].visible = true;
}

void ft2_textbox_hide(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES) return;
	textBoxes[textBoxID].visible = false;
	if (activeTextBox == (int16_t)textBoxID) ft2_textbox_exit_editing();
}

/* ------------------------------------------------------------------------- */
/*                          UTILITIES                                        */
/* ------------------------------------------------------------------------- */

bool ft2_textbox_is_marked(void) { return false; /* Selection not implemented */ }

int16_t ft2_textbox_get_cursor_x(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES) return 0;
	textBox_t *t = &textBoxes[textBoxID];
	return t->x + 1 + (t->cursorPos * FONT1_CHAR_W);
}

void ft2_textbox_set_cursor_end(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES) return;
	textBox_t *t = &textBoxes[textBoxID];
	if (t->textPtr) t->cursorPos = (int16_t)strlen(t->textPtr);
}

void ft2_textbox_mouse_drag(int32_t x, int32_t y)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES) return;
	textBox_t *t = &textBoxes[activeTextBox];
	if (!t->textPtr) return;
	(void)y;
	moveTextCursorToMouseX(t, x);
}

/* ------------------------------------------------------------------------- */
/*                       DIALOG TEXTBOX                                      */
/* ------------------------------------------------------------------------- */

void ft2_textbox_configure_dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                  char *textPtr, uint16_t maxChars)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
	bool needRealloc = (maxChars != t->maxChars);

	t->x = x; t->y = y; t->w = w; t->h = h;
	t->tx = 2; t->ty = 1;
	t->textPtr = textPtr;
	t->maxChars = maxChars;
	t->cursorPos = textPtr ? (int16_t)strlen(textPtr) : 0;
	t->visible = true; t->active = false; t->bufOffset = 0;

	if (needRealloc)
	{
		if (t->renderBuf) free(t->renderBuf);
		allocTextBoxRenderBuf(t);
	}
	else
		t->renderW = t->w - (t->tx * 2);
}

void ft2_textbox_activate_dialog(void)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
	if (!t->textPtr) return;

	if (textEditActive && activeTextBox >= 0 && activeTextBox != TB_DIALOG_INPUT)
		ft2_textbox_exit_editing();

	activeTextBox = TB_DIALOG_INPUT;
	textEditActive = true;
	t->active = true;
	t->cursorPos = (int16_t)strlen(t->textPtr);
}

void ft2_textbox_deactivate_dialog(void)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
	if (activeTextBox == TB_DIALOG_INPUT) { activeTextBox = -1; textEditActive = false; }
	t->active = false; t->visible = false; t->textPtr = NULL;
}

/* ------------------------------------------------------------------------- */
/*                           CLEANUP                                         */
/* ------------------------------------------------------------------------- */

void ft2_textbox_free(void)
{
	for (int i = 0; i < NUM_TEXTBOXES; i++)
		if (textBoxes[i].renderBuf) { free(textBoxes[i].renderBuf); textBoxes[i].renderBuf = NULL; }
}

