/**
 * @file ft2_plugin_textbox.c
 * @brief Text box input system for the FT2 plugin.
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
static int16_t textBoxNeedsRedraw = -1;  /* Textbox that needs redraw after editing exits */

/**
 * Render text directly to a buffer (not to screen).
 * This is used for textbox rendering where we need clipping.
 * Port of standalone ft2_textboxes.c:textOutBuf()
 */
static void textOutBuf(const ft2_bmp_t *bmp, uint8_t *dstBuffer, uint32_t dstWidth, 
                       uint8_t paletteIndex, const char *text, uint32_t maxTextLen)
{
	if (text == NULL || *text == '\0' || bmp == NULL || bmp->font1 == NULL)
		return;
	
	uint16_t currX = 0;
	for (uint32_t i = 0; i < maxTextLen; i++)
	{
		const char chr = *text++ & 0x7F;
		if (chr == '\0')
			break;
		
		if (chr != ' ')
		{
			const uint8_t *srcPtr = &bmp->font1[chr * FONT1_CHAR_W];
			uint8_t *dstPtr = &dstBuffer[currX];
			
			for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
			{
				for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
				{
					if (srcPtr[x])
						dstPtr[x] = paletteIndex;
				}
				
				srcPtr += FONT1_WIDTH;
				dstPtr += dstWidth;
			}
		}
		
		currX += charWidth(chr);
	}
}

/**
 * Get cursor X position relative to buffer offset.
 * Returns the on-screen X offset where cursor should be drawn.
 */
static int16_t cursorPosToX(textBox_t *t)
{
	if (t->textPtr == NULL)
		return -1;
	
	int32_t x = -1;  /* Cursor starts one pixel before character */
	for (int16_t i = 0; i < t->cursorPos; i++)
		x += charWidth(t->textPtr[i]);
	
	x -= t->bufOffset;  /* Subtract buffer offset to get visible X position */
	return (int16_t)x;
}

/**
 * Scroll text buffer to the left by TEXT_SCROLL_VALUE pixels.
 */
static void scrollTextBufferLeft(textBox_t *t)
{
	t->bufOffset -= TEXT_SCROLL_VALUE;
	if (t->bufOffset < 0)
		t->bufOffset = 0;
}

/**
 * Scroll text buffer to the right by TEXT_SCROLL_VALUE pixels.
 * Clamps to prevent scrolling past end of text.
 */
static void scrollTextBufferRight(textBox_t *t, uint16_t numCharsInText)
{
	if (t->textPtr == NULL)
		return;
	
	/* Get end of text position in pixels */
	int32_t textEnd = 0;
	for (uint16_t j = 0; j < numCharsInText; j++)
		textEnd += charWidth(t->textPtr[j]);
	
	/* Maximum scroll offset is (textEnd - renderW), clamped to 0 */
	textEnd -= t->renderW;
	if (textEnd < 0)
		textEnd = 0;
	
	/* Scroll and clamp */
	t->bufOffset += TEXT_SCROLL_VALUE;
	if (t->bufOffset > textEnd)
		t->bufOffset = textEnd;
}

/**
 * Get the length of text in a textbox (number of characters).
 */
static int16_t getTextLength(textBox_t *t)
{
	if (t->textPtr == NULL)
		return 0;
	return (int16_t)strlen(t->textPtr);
}

/* Allocate render buffer for a textbox. Call after setting x, y, w, h, tx, ty, maxChars. */
static void allocTextBoxRenderBuf(textBox_t *t)
{
	t->renderBufW = (9 + 1) * t->maxChars;  /* 9 = max glyph width + 1 for kerning */
	t->renderBufH = 10;                      /* Max glyph height */
	t->renderW = t->w - (t->tx * 2);         /* Visible area */
	t->bufOffset = 0;
	
	t->renderBuf = (uint8_t *)malloc(t->renderBufW * t->renderBufH);
	/* If allocation fails, renderBuf will be NULL and drawing will be skipped */
}

void ft2_textbox_init(void)
{
	memset(textBoxes, 0, sizeof(textBoxes));
	
	/* Instrument name textboxes (8 visible at once) - from original ft2_textboxes.c */
	/* {  446,   5, 140,  10, 1, 0, 22, true,  false }, etc. */
	static const uint16_t instY[8] = { 5, 16, 27, 38, 49, 60, 71, 82 };
	for (int i = 0; i < 8; i++)
	{
		textBox_t *t = &textBoxes[TB_INST1 + i];
		t->x = 446;
		t->y = instY[i];
		t->w = 140;
		t->h = 10;
		t->tx = 1;
		t->ty = 0;
		t->maxChars = 22;
		t->rightMouseButton = true;
		t->visible = true;
		allocTextBoxRenderBuf(t);
	}
	
	/* Sample name textboxes (5 visible at once) */
	/* {  446,  99, 116,  10, 1, 0, 22, true,  false }, etc. */
	static const uint16_t sampY[5] = { 99, 110, 121, 132, 143 };
	for (int i = 0; i < 5; i++)
	{
		textBox_t *t = &textBoxes[TB_SAMP1 + i];
		t->x = 446;
		t->y = sampY[i];
		t->w = 116;
		t->h = 10;
		t->tx = 1;
		t->ty = 0;
		t->maxChars = 22;
		t->rightMouseButton = true;
		t->visible = true;
		allocTextBoxRenderBuf(t);
	}
	
	/* Song name textbox */
	/* {  424, 158, 160,  12, 2, 1, 20, false, true  } */
	{
		textBox_t *t = &textBoxes[TB_SONG_NAME];
		t->x = 424;
		t->y = 158;
		t->w = 160;
		t->h = 12;
		t->tx = 2;
		t->ty = 1;
		t->maxChars = 20;
		t->rightMouseButton = false;
		t->visible = true;
		allocTextBoxRenderBuf(t);
	}

	/* Disk op filename textbox */
	/* {  31, 158, 134, 12, 2, 1, PATH_MAX, false, true } */
	{
		textBox_t *t = &textBoxes[TB_DISKOP_FILENAME];
		t->x = 31;
		t->y = 158;
		t->w = 134;
		t->h = 12;
		t->tx = 2;
		t->ty = 1;
		t->maxChars = 255;
		t->rightMouseButton = false;
		t->visible = false;
		allocTextBoxRenderBuf(t);
	}

	/* Dialog input textbox - position/size set dynamically */
	{
		textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
		t->x = 0;
		t->y = 0;
		t->w = 250;
		t->h = 12;
		t->tx = 2;
		t->ty = 1;
		t->maxChars = 255;
		t->rightMouseButton = false;
		t->visible = false;
		t->textPtr = NULL;
		allocTextBoxRenderBuf(t);
	}
	
	activeTextBox = -1;
	textEditActive = false;
}

void ft2_textbox_update_pointers(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Update instrument name pointers (instrName is 1-indexed) */
	for (int i = 0; i < 8; i++)
	{
		uint8_t instrIdx = inst->editor.instrBankOffset + i + 1;
		if (instrIdx <= FT2_MAX_INST)
		{
			textBoxes[TB_INST1 + i].textPtr = inst->replayer.song.instrName[instrIdx];
		}
		else
		{
			textBoxes[TB_INST1 + i].textPtr = NULL;
		}
	}
	
	/* Update sample name pointers */
	ft2_instr_t *curInstr = NULL;
	if (inst->editor.curInstr > 0 && inst->editor.curInstr < 128)
		curInstr = inst->replayer.instr[inst->editor.curInstr];
	
	for (int i = 0; i < 5; i++)
	{
		uint8_t smpIdx = inst->editor.sampleBankOffset + i;
		if (curInstr != NULL && smpIdx < 16)
		{
			textBoxes[TB_SAMP1 + i].textPtr = curInstr->smp[smpIdx].name;
		}
		else
		{
			textBoxes[TB_SAMP1 + i].textPtr = NULL;
		}
	}
	
	/* Song name pointer */
	textBoxes[TB_SONG_NAME].textPtr = inst->replayer.song.name;

	/* Disk op filename pointer */
	textBoxes[TB_DISKOP_FILENAME].textPtr = inst->diskop.filename;
}

/**
 * Move cursor to the character at mouse X position, accounting for buffer offset.
 */
static void moveTextCursorToMouseX(textBox_t *t, int32_t mouseX)
{
	if (t->textPtr == NULL || t->textPtr[0] == '\0')
	{
		t->cursorPos = 0;
		return;
	}
	
	/* Early exit if clicking at start with no scroll */
	if (mouseX == t->x && t->bufOffset == 0)
	{
		t->cursorPos = 0;
		return;
	}
	
	int16_t numChars = getTextLength(t);
	
	/* Calculate the X position in the full (unscrolled) text buffer.
	 * mx = bufOffset + (mouseX - textStartX) */
	const int32_t mx = t->bufOffset + mouseX;
	int32_t tx = (t->x + t->tx) - 1;  /* Text starts at this X, cursor is 1px before first char */
	int32_t cw = -1;
	int16_t i;
	
	/* Find which character we're clicking on */
	for (i = 0; i < numChars; i++)
	{
		cw = charWidth(t->textPtr[i]);
		const int32_t tx2 = tx + cw;
		
		if (mx >= tx && mx < tx2)
		{
			t->cursorPos = i;
			break;
		}
		
		tx += cw;
	}
	
	/* Set to last character if we clicked past the end of text */
	if (i == numChars && mx >= tx)
		t->cursorPos = numChars;
	
	/* Scroll buffer if cursor is now out of visible area */
	if (cw != -1)
	{
		const int16_t cursorX = cursorPosToX(t);
		
		if (cursorX + cw > t->renderW)
			scrollTextBufferRight(t, (uint16_t)numChars);
		else if (cursorX < -1)
			scrollTextBufferLeft(t);
	}
}

int16_t ft2_textbox_test_mouse_down(int32_t x, int32_t y, bool rightButton)
{
	for (int i = 0; i < NUM_TEXTBOXES; i++)
	{
		textBox_t *t = &textBoxes[i];
		if (!t->visible || t->textPtr == NULL)
			continue;
		
		if (x >= t->x && x < t->x + t->w && y >= t->y && y < t->y + t->h)
		{
			/* Check if the click type matches what the textbox expects:
			 * - If textbox requires right-click (rightMouseButton=true), only activate on right-click
			 * - If textbox allows left-click (rightMouseButton=false), activate on any click */
			if (!rightButton && t->rightMouseButton)
				continue;
			
			/* If we were editing another text box and clicked on another one, exit that one first */
			if (textEditActive && i != activeTextBox)
				ft2_textbox_exit_editing();
			
			/* Activate text editing */
			activeTextBox = (int16_t)i;
			textEditActive = true;
			t->active = true;
			
			/* Move cursor to click position, accounting for buffer offset */
			moveTextCursorToMouseX(t, x);
			
			return (int16_t)i;
		}
	}
	
	/* If clicked outside while editing, exit editing */
	if (textEditActive)
	{
		ft2_textbox_exit_editing();
	}
	
	return -1;
}

void ft2_textbox_input_char(char c)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES)
		return;
	
	textBox_t *t = &textBoxes[activeTextBox];
	if (t->textPtr == NULL)
		return;
	
	int16_t len = getTextLength(t);
	
	/* Check if we can add more characters */
	if (len >= t->maxChars - 1)
		return;
	
	/* Only allow printable ASCII */
	if (c < 32 || c > 126)
		return;
	
	/* Insert character at cursor position */
	if (t->cursorPos >= len)
	{
		/* Append at end */
		t->textPtr[len] = c;
		t->textPtr[len + 1] = '\0';
		t->cursorPos = len + 1;
	}
	else
	{
		/* Insert in middle - shift characters right */
		memmove(&t->textPtr[t->cursorPos + 1], &t->textPtr[t->cursorPos], len - t->cursorPos + 1);
		t->textPtr[t->cursorPos] = c;
		t->cursorPos++;
	}
	
	/* Scroll buffer if cursor moved out of visible area */
	if (cursorPosToX(t) >= t->renderW)
		scrollTextBufferRight(t, (uint16_t)getTextLength(t));
}

void ft2_textbox_handle_key(int32_t keyCode, int32_t modifiers)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES)
		return;
	
	textBox_t *t = &textBoxes[activeTextBox];
	if (t->textPtr == NULL)
		return;
	
	int16_t len = getTextLength(t);
	
	(void)modifiers;
	
	switch (keyCode)
	{
		case 8: /* Backspace */
			if (t->cursorPos > 0)
			{
				/* Scroll buffer offset if we are scrolled */
				if (t->bufOffset > 0)
				{
					t->bufOffset -= charWidth(t->textPtr[t->cursorPos - 1]);
					if (t->bufOffset < 0)
						t->bufOffset = 0;
				}
				
				memmove(&t->textPtr[t->cursorPos - 1], &t->textPtr[t->cursorPos], len - t->cursorPos + 1);
				t->cursorPos--;
				
				/* Scroll if cursor moved out of view */
				if (cursorPosToX(t) < -1)
					scrollTextBufferLeft(t);
			}
			break;
		
		case 127: /* Delete */
			if (t->cursorPos < len)
			{
				/* Scroll buffer offset if we are scrolled */
				if (t->bufOffset > 0)
				{
					t->bufOffset -= charWidth(t->textPtr[t->cursorPos]);
					if (t->bufOffset < 0)
						t->bufOffset = 0;
				}
				
				memmove(&t->textPtr[t->cursorPos], &t->textPtr[t->cursorPos + 1], len - t->cursorPos);
			}
			break;
		
		case 0x1000: /* Left */
			if (t->cursorPos > 0)
			{
				t->cursorPos--;
				/* Scroll buffer if cursor moved out of visible area */
				if (cursorPosToX(t) < -1)
					scrollTextBufferLeft(t);
			}
			break;
		
		case 0x1001: /* Right */
			if (t->cursorPos < len)
			{
				t->cursorPos++;
				/* Scroll buffer if cursor moved out of visible area */
				if (cursorPosToX(t) >= t->renderW)
					scrollTextBufferRight(t, (uint16_t)len);
			}
			break;
		
		case 0x1006: /* Home */
			t->cursorPos = 0;
			t->bufOffset = 0;
			break;
		
		case 0x1007: /* End */
		{
			/* Count number of chars and get full text width */
			uint32_t textWidth = 0;
			for (int16_t i = 0; i < len; i++)
				textWidth += charWidth(t->textPtr[i]);
			
			t->cursorPos = len;
			
			/* Scroll to show end of text */
			t->bufOffset = 0;
			if (textWidth > t->renderW)
				t->bufOffset = textWidth - t->renderW;
		}
			break;
		
		case '\r': /* Enter */
		case 27: /* Escape */
			ft2_textbox_exit_editing();
			break;
		
		default:
			break;
	}
}

void ft2_textbox_exit_editing(void)
{
	if (activeTextBox >= 0 && activeTextBox < NUM_TEXTBOXES)
	{
		textBoxes[activeTextBox].active = false;
		textBoxes[activeTextBox].bufOffset = 0;  /* Reset scroll to start */
		textBoxNeedsRedraw = activeTextBox;  /* Mark for redraw to clear cursor */
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

bool ft2_textbox_is_editing(void)
{
	return textEditActive;
}

void ft2_textbox_draw(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t textBoxID, const ft2_instance_t *inst)
{
	ft2_textbox_draw_with_cursor(video, bmp, textBoxID, false, inst);
}

/**
 * Determine the correct background palette color for a textbox based on its type
 * and the current selection state. This is more reliable than reading from the
 * framebuffer, which can have stale values.
 *
 * @param textBoxID The ID of the textbox
 * @param inst The FT2 instance (for selection state)
 * @return The palette index to use for the background
 */
static uint8_t getTextBoxBackgroundPal(uint16_t textBoxID, const ft2_instance_t *inst)
{
	if (inst == NULL)
		return PAL_BCKGRND;
	
	/* Instrument name textboxes (TB_INST1 to TB_INST8) */
	if (textBoxID >= TB_INST1 && textBoxID <= TB_INST8)
	{
		/* Check if this textbox corresponds to the currently selected instrument */
		uint8_t textboxInstrIdx = (textBoxID - TB_INST1);  /* 0-7 */
		uint8_t displayedInstr = inst->editor.instrBankOffset + 1 + textboxInstrIdx;  /* 1-8, 9-16, etc. */
		
		if (displayedInstr == inst->editor.curInstr)
			return PAL_BUTTONS;  /* Selected instrument gets highlight */
		else
			return PAL_BCKGRND;  /* Non-selected gets black background */
	}
	
	/* Sample name textboxes (TB_SAMP1 to TB_SAMP5) */
	if (textBoxID >= TB_SAMP1 && textBoxID <= TB_SAMP5)
	{
		/* Check if this textbox corresponds to the currently selected sample */
		uint8_t textboxSmpIdx = (textBoxID - TB_SAMP1);  /* 0-4 */
		uint8_t displayedSmp = inst->editor.sampleBankOffset + textboxSmpIdx;  /* 0-4, 5-9, etc. */
		
		if (displayedSmp == inst->editor.curSmp)
			return PAL_BUTTONS;  /* Selected sample gets highlight */
		else
			return PAL_BCKGRND;  /* Non-selected gets black background */
	}
	
	/* Song name textbox - always black background (inside framework) */
	if (textBoxID == TB_SONG_NAME)
		return PAL_BCKGRND;
	
	/* Disk op filename textbox - black background */
	if (textBoxID == TB_DISKOP_FILENAME)
		return PAL_BCKGRND;

	/* Default to black for unknown textboxes */
	return PAL_BCKGRND;
}

void ft2_textbox_draw_with_cursor(ft2_video_t *video, const ft2_bmp_t *bmp, 
                                  uint16_t textBoxID, bool showCursor, 
                                  const ft2_instance_t *inst)
{
	if (textBoxID >= NUM_TEXTBOXES || video == NULL)
		return;
	
	textBox_t *t = &textBoxes[textBoxID];
	if (!t->visible)
		return;
	
	/* Skip if no render buffer allocated */
	if (t->renderBuf == NULL)
		return;
	
	uint8_t bgPal = getTextBoxBackgroundPal(textBoxID, inst);
	
	/* Fill render buffer with transparency key */
	memset(t->renderBuf, PAL_TRANSPR, t->renderBufW * t->renderBufH);
	
	/* Render text to buffer if we have text */
	if (t->textPtr != NULL && bmp != NULL)
		textOutBuf(bmp, t->renderBuf, t->renderBufW, PAL_FORGRND, t->textPtr, t->maxChars);
	
	/* Fill screen rect with background color (10 pixels, matching instrument switcher highlight) */
	fillRect(video, t->x + t->tx - 1, t->y + t->ty, t->renderW + 1, 10, bgPal);
	
	/* Clear the 1 pixel rows above and below that cursor may have extended into.
	 * This ensures consistent background size and cleans up after cursor exits. */
	hLine(video, t->x + t->tx - 1, t->y + t->ty - 1, t->renderW + 1, PAL_BCKGRND);
	hLine(video, t->x + t->tx - 1, t->y + t->ty + 10, t->renderW + 1, PAL_BCKGRND);
	
	/* Blit visible portion of render buffer to screen (with clipping) */
	blitClipX(video, t->x + t->tx, t->y + t->ty, &t->renderBuf[t->bufOffset], t->renderBufW, t->renderBufH, t->renderW);
	
	/* Draw cursor if active */
	if (t->active && showCursor && t->textPtr != NULL)
	{
		int16_t cursorX = t->x + t->tx + cursorPosToX(t);
		
		/* Only draw cursor if it's within visible area */
		if (cursorX >= t->x + t->tx - 1 && cursorX < t->x + t->tx + t->renderW)
			vLine(video, cursorX, t->y + t->ty - 1, 12, PAL_FORGRND);
	}
}

int16_t ft2_textbox_get_active(void)
{
	return activeTextBox;
}

void ft2_textbox_show(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES)
		return;
	
	textBoxes[textBoxID].visible = true;
}

void ft2_textbox_hide(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES)
		return;
	
	textBoxes[textBoxID].visible = false;
	
	/* If hiding the active textbox, exit editing */
	if (activeTextBox == (int16_t)textBoxID)
		ft2_textbox_exit_editing();
}

bool ft2_textbox_is_marked(void)
{
	/* TODO: Implement text selection/marking */
	return false;
}

int16_t ft2_textbox_get_cursor_x(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES)
		return 0;
	
	textBox_t *t = &textBoxes[textBoxID];
	return t->x + 1 + (t->cursorPos * FONT1_CHAR_W);
}

void ft2_textbox_set_cursor_end(uint16_t textBoxID)
{
	if (textBoxID >= NUM_TEXTBOXES)
		return;
	
	textBox_t *t = &textBoxes[textBoxID];
	if (t->textPtr != NULL)
		t->cursorPos = (int16_t)strlen(t->textPtr);
}

void ft2_textbox_mouse_drag(int32_t x, int32_t y)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES)
		return;
	
	textBox_t *t = &textBoxes[activeTextBox];
	if (t->textPtr == NULL)
		return;
	
	(void)y;
	
	/* Use the same logic as mouse down to properly handle buffer offset */
	moveTextCursorToMouseX(t, x);
}

void ft2_textbox_configure_dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                  char *textPtr, uint16_t maxChars)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
	
	/* Check if we need to reallocate the buffer (maxChars changed) */
	bool needRealloc = (maxChars != t->maxChars);
	
	t->x = x;
	t->y = y;
	t->w = w;
	t->h = h;
	t->tx = 2;
	t->ty = 1;
	t->textPtr = textPtr;
	t->maxChars = maxChars;
	t->cursorPos = (textPtr != NULL) ? (int16_t)strlen(textPtr) : 0;
	t->visible = true;
	t->active = false;
	t->bufOffset = 0;
	
	if (needRealloc)
	{
		if (t->renderBuf != NULL)
			free(t->renderBuf);
		allocTextBoxRenderBuf(t);
	}
	else
	{
		/* Just recalculate renderW since w might have changed */
		t->renderW = t->w - (t->tx * 2);
	}
}

void ft2_textbox_activate_dialog(void)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
	if (t->textPtr == NULL)
		return;
	
	/* Exit any existing editing first */
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
	
	if (activeTextBox == TB_DIALOG_INPUT)
	{
		activeTextBox = -1;
		textEditActive = false;
	}
	
	t->active = false;
	t->visible = false;
	t->textPtr = NULL;
}

void ft2_textbox_free(void)
{
	for (int i = 0; i < NUM_TEXTBOXES; i++)
	{
		if (textBoxes[i].renderBuf != NULL)
		{
			free(textBoxes[i].renderBuf);
			textBoxes[i].renderBuf = NULL;
		}
	}
}

