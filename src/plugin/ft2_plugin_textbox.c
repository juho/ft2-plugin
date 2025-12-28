/**
 * @file ft2_plugin_textbox.c
 * @brief Text box input system for the FT2 plugin.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_instance.h"

static textBox_t textBoxes[NUM_TEXTBOXES];
static int16_t activeTextBox = -1;
static bool textEditActive = false;
static int16_t textBoxNeedsRedraw = -1;  /* Textbox that needs redraw after editing exits */

void ft2_textbox_init(void)
{
	memset(textBoxes, 0, sizeof(textBoxes));
	
	/* Instrument name textboxes (8 visible at once) - from original ft2_textboxes.c */
	/* {  446,   5, 140,  10, 1, 0, 22, true,  false }, etc. */
	static const uint16_t instY[8] = { 5, 16, 27, 38, 49, 60, 71, 82 };
	for (int i = 0; i < 8; i++)
	{
		textBoxes[TB_INST1 + i].x = 446;
		textBoxes[TB_INST1 + i].y = instY[i];
		textBoxes[TB_INST1 + i].w = 140;
		textBoxes[TB_INST1 + i].h = 10;
		textBoxes[TB_INST1 + i].tx = 1;
		textBoxes[TB_INST1 + i].ty = 0;
		textBoxes[TB_INST1 + i].maxChars = 22;
		textBoxes[TB_INST1 + i].rightMouseButton = true;
		textBoxes[TB_INST1 + i].visible = true;
	}
	
	/* Sample name textboxes (5 visible at once) */
	/* {  446,  99, 116,  10, 1, 0, 22, true,  false }, etc. */
	static const uint16_t sampY[5] = { 99, 110, 121, 132, 143 };
	for (int i = 0; i < 5; i++)
	{
		textBoxes[TB_SAMP1 + i].x = 446;
		textBoxes[TB_SAMP1 + i].y = sampY[i];
		textBoxes[TB_SAMP1 + i].w = 116;
		textBoxes[TB_SAMP1 + i].h = 10;
		textBoxes[TB_SAMP1 + i].tx = 1;
		textBoxes[TB_SAMP1 + i].ty = 0;
		textBoxes[TB_SAMP1 + i].maxChars = 22;
		textBoxes[TB_SAMP1 + i].rightMouseButton = true;
		textBoxes[TB_SAMP1 + i].visible = true;
	}
	
	/* Song name textbox */
	/* {  424, 158, 160,  12, 2, 1, 20, false, true  } */
	textBoxes[TB_SONG_NAME].x = 424;
	textBoxes[TB_SONG_NAME].y = 158;
	textBoxes[TB_SONG_NAME].w = 160;
	textBoxes[TB_SONG_NAME].h = 12;
	textBoxes[TB_SONG_NAME].tx = 2;
	textBoxes[TB_SONG_NAME].ty = 1;
	textBoxes[TB_SONG_NAME].maxChars = 20;
	textBoxes[TB_SONG_NAME].rightMouseButton = false;
	textBoxes[TB_SONG_NAME].visible = true;

	/* Disk op filename textbox */
	/* {  31, 158, 134, 12, 2, 1, PATH_MAX, false, true } */
	textBoxes[TB_DISKOP_FILENAME].x = 31;
	textBoxes[TB_DISKOP_FILENAME].y = 158;
	textBoxes[TB_DISKOP_FILENAME].w = 134;
	textBoxes[TB_DISKOP_FILENAME].h = 12;
	textBoxes[TB_DISKOP_FILENAME].tx = 2;
	textBoxes[TB_DISKOP_FILENAME].ty = 1;
	textBoxes[TB_DISKOP_FILENAME].maxChars = 255;
	textBoxes[TB_DISKOP_FILENAME].rightMouseButton = false;
	textBoxes[TB_DISKOP_FILENAME].visible = false;

	/* Dialog input textbox - position/size set dynamically */
	textBoxes[TB_DIALOG_INPUT].x = 0;
	textBoxes[TB_DIALOG_INPUT].y = 0;
	textBoxes[TB_DIALOG_INPUT].w = 250;
	textBoxes[TB_DIALOG_INPUT].h = 12;
	textBoxes[TB_DIALOG_INPUT].tx = 2;
	textBoxes[TB_DIALOG_INPUT].ty = 1;
	textBoxes[TB_DIALOG_INPUT].maxChars = 255;
	textBoxes[TB_DIALOG_INPUT].rightMouseButton = false;
	textBoxes[TB_DIALOG_INPUT].visible = false;
	textBoxes[TB_DIALOG_INPUT].textPtr = NULL;
	
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
			t->cursorPos = (int16_t)strlen(t->textPtr);
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
	
	int16_t len = (int16_t)strlen(t->textPtr);
	
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
}

void ft2_textbox_handle_key(int32_t keyCode, int32_t modifiers)
{
	if (!textEditActive || activeTextBox < 0 || activeTextBox >= NUM_TEXTBOXES)
		return;
	
	textBox_t *t = &textBoxes[activeTextBox];
	if (t->textPtr == NULL)
		return;
	
	int16_t len = (int16_t)strlen(t->textPtr);
	
	(void)modifiers;
	
	switch (keyCode)
	{
		case 8: /* Backspace */
			if (t->cursorPos > 0)
			{
				memmove(&t->textPtr[t->cursorPos - 1], &t->textPtr[t->cursorPos], len - t->cursorPos + 1);
				t->cursorPos--;
			}
			break;
		
		case 127: /* Delete */
			if (t->cursorPos < len)
			{
				memmove(&t->textPtr[t->cursorPos], &t->textPtr[t->cursorPos + 1], len - t->cursorPos);
			}
			break;
		
		case 0x1000: /* Left */
			if (t->cursorPos > 0)
				t->cursorPos--;
			break;
		
		case 0x1001: /* Right */
			if (t->cursorPos < len)
				t->cursorPos++;
			break;
		
		case 0x1006: /* Home */
			t->cursorPos = 0;
			break;
		
		case 0x1007: /* End */
			t->cursorPos = len;
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
	if (!t->visible || t->textPtr == NULL)
		return;
	
	uint16_t renderW = t->w - t->tx;
	uint8_t bgPal = getTextBoxBackgroundPal(textBoxID, inst);
	
	/* Fill area extended to cover cursor:
	 * - Cursor starts 1 pixel left (at t->x + t->tx - 1)
	 * - Cursor starts 1 pixel up (at t->y + t->ty - 1)  
	 * - Cursor is 12 pixels tall (vs 10 for text)
	 * So we extend fill by 1 left, 1 up, and use height 12 */
	fillRect(video, t->x + t->tx - 1, t->y + t->ty - 1, renderW + 1, 12, bgPal);
	
	if (bmp != NULL)
		textOut(video, bmp, t->x + t->tx, t->y + t->ty, PAL_FORGRND, t->textPtr);
	
	if (t->active && showCursor)
	{
		int16_t cursorX = t->x + t->tx - 1;
		for (int16_t i = 0; i < t->cursorPos && t->textPtr[i] != '\0'; i++)
			cursorX += charWidth(t->textPtr[i]);
		
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
	
	/* Calculate cursor position from mouse X */
	int32_t relX = x - t->x - 1;
	if (relX < 0)
		relX = 0;
	
	int16_t newPos = (int16_t)(relX / FONT1_CHAR_W);
	int16_t len = (int16_t)strlen(t->textPtr);
	
	if (newPos > len)
		newPos = len;
	
	t->cursorPos = newPos;
}

void ft2_textbox_configure_dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                  char *textPtr, uint16_t maxChars)
{
	textBox_t *t = &textBoxes[TB_DIALOG_INPUT];
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

