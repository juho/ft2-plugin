/**
 * @file ft2_plugin_dialog.c
 * @brief Simple modal dialog system for the FT2 plugin
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_textbox.h"

/* Dialog constants */
#define DIALOG_H 67
#define DIALOG_BUTTON_W 80
#define DIALOG_BUTTON_H 16
#define DIALOG_MIN_W 200
#define DIALOG_MAX_W 600
#define SCREEN_W 632
#define SCREEN_H 400

/* Input box width matching standalone TEXTBOX_W */
#define INPUT_BOX_W 250

/* Calculate dialog dimensions based on content */
static void calculateDialogSize(ft2_dialog_t *dlg)
{
	int hlen = textWidth(dlg->headline);
	int tlen = textWidth(dlg->text);
	
	int wlen = (hlen > tlen) ? hlen : tlen;
	
	/* For input dialogs, also consider the input box width */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
	{
		if (INPUT_BOX_W > wlen)
			wlen = INPUT_BOX_W;
	}
	
	/* Account for buttons */
	int numButtons;
	if (dlg->type == DIALOG_OK)
		numButtons = 1;
	else if (dlg->type == DIALOG_INPUT_PREVIEW)
		numButtons = 3;
	else if (dlg->type == DIALOG_ZAP)
		numButtons = 4;
	else
		numButtons = 2;
	
	int buttonWidth = numButtons * 100 - 20;
	if (buttonWidth > wlen)
		wlen = buttonWidth;
	
	/* Add padding (matching standalone) */
	wlen += 100;
	
	/* Clamp width */
	if (wlen < DIALOG_MIN_W)
		wlen = DIALOG_MIN_W;
	if (wlen > DIALOG_MAX_W)
		wlen = DIALOG_MAX_W;
	
	dlg->w = (int16_t)wlen;
	dlg->h = DIALOG_H;  /* Same height for all dialog types (matching standalone) */
	
	dlg->x = (int16_t)((SCREEN_W - wlen) / 2);
	dlg->y = (int16_t)249;  /* Fixed Y position (matching standalone) */
	
	/* Calculate button positions */
	int buttonsWidth = numButtons * 100 - 20;
	int buttonsStartX = (SCREEN_W - buttonsWidth) / 2;
	dlg->button1X = (int16_t)buttonsStartX;
	dlg->button2X = (int16_t)(buttonsStartX + 100);
	dlg->button3X = (int16_t)(buttonsStartX + 200);
	dlg->button4X = (int16_t)(buttonsStartX + 300);
	
	/* Buttons at y+42 for all dialog types (matching standalone) */
	dlg->buttonY = (int16_t)(dlg->y + 42);
	
	/* Calculate text positions - only headline is drawn for input dialogs */
	dlg->textX = (int16_t)((SCREEN_W - tlen) / 2);
	dlg->textY = (int16_t)(dlg->y + 24);
	
	/* For input dialogs, configure and activate the textbox */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
	{
		int inputX = (SCREEN_W - INPUT_BOX_W) / 2;
		int inputY = dlg->y + 24;
		ft2_textbox_configure_dialog((uint16_t)inputX, (uint16_t)inputY, 
		                             INPUT_BOX_W, 12,
		                             dlg->inputBuffer, (uint16_t)dlg->inputMaxLen);
		ft2_textbox_activate_dialog();
	}
}

void ft2_dialog_init(ft2_dialog_t *dlg)
{
	if (dlg == NULL)
		return;
	
	memset(dlg, 0, sizeof(ft2_dialog_t));
	dlg->active = false;
	dlg->type = DIALOG_NONE;
	dlg->result = DIALOG_RESULT_NONE;
}

void ft2_dialog_show_message(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_OK;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_confirm(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_OK_CANCEL;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_yesno(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_YES_NO;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_yesno_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_YES_NO;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->button4Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputBuffer[0] = '\0';
	dlg->inputCursorPos = 0;
	dlg->inputMaxLen = 0;
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_zap_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                            struct ft2_instance_t *inst,
                            ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_ZAP;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->button4Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputBuffer[0] = '\0';
	dlg->inputCursorPos = 0;
	dlg->inputMaxLen = 0;
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input(ft2_dialog_t *dlg, const char *headline, const char *text,
                           const char *defaultValue, int maxLen)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_INPUT;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = NULL;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputMaxLen = (maxLen > 0 && maxLen < 256) ? maxLen : 255;
	if (defaultValue != NULL)
	{
		strncpy(dlg->inputBuffer, defaultValue, (size_t)dlg->inputMaxLen);
		dlg->inputCursorPos = (int)strlen(dlg->inputBuffer);
	}
	else
	{
		dlg->inputBuffer[0] = '\0';
		dlg->inputCursorPos = 0;
	}
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input_preview(ft2_dialog_t *dlg, const char *headline, const char *text,
                                   const char *defaultValue, int maxLen,
                                   struct ft2_instance_t *inst,
                                   ft2_dialog_preview_callback_t previewCallback)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_INPUT_PREVIEW;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->previewCallback = previewCallback;
	dlg->instance = inst;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputMaxLen = (maxLen > 0 && maxLen < 256) ? maxLen : 255;
	if (defaultValue != NULL)
	{
		strncpy(dlg->inputBuffer, defaultValue, (size_t)dlg->inputMaxLen);
		dlg->inputCursorPos = (int)strlen(dlg->inputBuffer);
	}
	else
	{
		dlg->inputBuffer[0] = '\0';
		dlg->inputCursorPos = 0;
	}
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              const char *defaultValue, int maxLen,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_INPUT;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputMaxLen = (maxLen > 0 && maxLen < 256) ? maxLen : 255;
	if (defaultValue != NULL)
	{
		strncpy(dlg->inputBuffer, defaultValue, (size_t)dlg->inputMaxLen);
		dlg->inputCursorPos = (int)strlen(dlg->inputBuffer);
	}
	else
	{
		dlg->inputBuffer[0] = '\0';
		dlg->inputCursorPos = 0;
	}
	
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input_preview_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                                      const char *defaultValue, int maxLen,
                                      struct ft2_instance_t *inst,
                                      ft2_dialog_preview_callback_t previewCallback,
                                      ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL)
		return;
	
	dlg->active = true;
	dlg->type = DIALOG_INPUT_PREVIEW;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = false;
	dlg->button2Pressed = false;
	dlg->button3Pressed = false;
	dlg->previewCallback = previewCallback;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	
	if (headline != NULL)
		strncpy(dlg->headline, headline, sizeof(dlg->headline) - 1);
	else
		dlg->headline[0] = '\0';
	
	if (text != NULL)
		strncpy(dlg->text, text, sizeof(dlg->text) - 1);
	else
		dlg->text[0] = '\0';
	
	dlg->inputMaxLen = (maxLen > 0 && maxLen < 256) ? maxLen : 255;
	if (defaultValue != NULL)
	{
		strncpy(dlg->inputBuffer, defaultValue, (size_t)dlg->inputMaxLen);
		dlg->inputCursorPos = (int)strlen(dlg->inputBuffer);
	}
	else
	{
		dlg->inputBuffer[0] = '\0';
		dlg->inputCursorPos = 0;
	}
	
	calculateDialogSize(dlg);
}

bool ft2_dialog_is_active(const ft2_dialog_t *dlg)
{
	if (dlg == NULL)
		return false;
	return dlg->active;
}

ft2_dialog_result_t ft2_dialog_get_result(const ft2_dialog_t *dlg)
{
	if (dlg == NULL)
		return DIALOG_RESULT_NONE;
	return dlg->result;
}

const char *ft2_dialog_get_input(const ft2_dialog_t *dlg)
{
	if (dlg == NULL)
		return "";
	return dlg->inputBuffer;
}

void ft2_dialog_close(ft2_dialog_t *dlg)
{
	if (dlg == NULL)
		return;
	
	dlg->active = false;
}

/* Check if point is within a button */
static bool pointInButton(int x, int y, int bx, int by)
{
	return (x >= bx && x < bx + DIALOG_BUTTON_W &&
	        y >= by && y < by + DIALOG_BUTTON_H);
}

/* Close dialog and invoke completion callback */
static void closeDialogWithResult(ft2_dialog_t *dlg, ft2_dialog_result_t result)
{
	/* Deactivate textbox if this was an input dialog */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
	{
		ft2_textbox_deactivate_dialog();
	}
	
	dlg->result = result;
	dlg->active = false;
	
	if (dlg->onComplete != NULL)
	{
		dlg->onComplete(dlg->instance, result, dlg->inputBuffer, dlg->userData);
	}
}

bool ft2_dialog_mouse_down(ft2_dialog_t *dlg, int x, int y, int button)
{
	if (dlg == NULL || !dlg->active)
		return false;
	
	if (button != 1) /* Left button only */
		return false;
	
	/* Check button 1 (OK/Yes/All) */
	if (pointInButton(x, y, dlg->button1X, dlg->buttonY))
	{
		dlg->button1Pressed = true;
		return true;
	}
	
	/* Check button 2 (Cancel/No/Preview/Song) - only if we have 2+ buttons */
	if (dlg->type != DIALOG_OK)
	{
		if (pointInButton(x, y, dlg->button2X, dlg->buttonY))
		{
			dlg->button2Pressed = true;
			return true;
		}
	}
	
	/* Check button 3 (Cancel/Instruments) - for 3+ button dialogs */
	if (dlg->type == DIALOG_INPUT_PREVIEW || dlg->type == DIALOG_ZAP)
	{
		if (pointInButton(x, y, dlg->button3X, dlg->buttonY))
		{
			dlg->button3Pressed = true;
			return true;
		}
	}
	
	/* Check button 4 (Cancel) - only for DIALOG_ZAP */
	if (dlg->type == DIALOG_ZAP)
	{
		if (pointInButton(x, y, dlg->button4X, dlg->buttonY))
		{
			dlg->button4Pressed = true;
			return true;
		}
	}
	
	return true; /* Consume click even if not on button */
}

bool ft2_dialog_mouse_up(ft2_dialog_t *dlg, int x, int y, int button)
{
	if (dlg == NULL || !dlg->active)
		return false;
	
	if (button != 1) /* Left button only */
		return false;
	
	/* Check button 1 (OK/All) */
	if (dlg->button1Pressed)
	{
		if (pointInButton(x, y, dlg->button1X, dlg->buttonY))
		{
			if (dlg->type == DIALOG_ZAP)
				closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_ALL);
			else
			closeDialogWithResult(dlg, DIALOG_RESULT_OK);
		}
		dlg->button1Pressed = false;
		return true;
	}
	
	/* Check button 2 (Cancel for 2-button, Preview for 3-button, Song for Zap) */
	if (dlg->button2Pressed)
	{
		if (pointInButton(x, y, dlg->button2X, dlg->buttonY))
		{
			if (dlg->type == DIALOG_INPUT_PREVIEW)
			{
				/* Preview button - trigger callback but keep dialog open */
				dlg->result = DIALOG_RESULT_PREVIEW;
				if (dlg->previewCallback != NULL && dlg->inputBuffer[0] != '\0')
				{
					uint32_t value = (uint32_t)atoi(dlg->inputBuffer);
					dlg->previewCallback(dlg->instance, value);
				}
				/* Don't close the dialog */
			}
			else if (dlg->type == DIALOG_ZAP)
			{
				closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_SONG);
			}
			else
			{
				closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
			}
		}
		dlg->button2Pressed = false;
		return true;
	}
	
	/* Check button 3 (Cancel for INPUT_PREVIEW, Instruments for Zap) */
	if (dlg->button3Pressed)
	{
		if (pointInButton(x, y, dlg->button3X, dlg->buttonY))
		{
			if (dlg->type == DIALOG_ZAP)
				closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_INSTR);
			else
			closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
		}
		dlg->button3Pressed = false;
		return true;
	}
	
	/* Check button 4 (Cancel for Zap) */
	if (dlg->button4Pressed)
	{
		if (pointInButton(x, y, dlg->button4X, dlg->buttonY))
		{
			closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
		}
		dlg->button4Pressed = false;
		return true;
	}
	
	return true;
}

bool ft2_dialog_key_down(ft2_dialog_t *dlg, int keycode)
{
	if (dlg == NULL || !dlg->active)
		return false;
	
	/* Escape closes with cancel */
	if (keycode == FT2_KEY_ESCAPE)
	{
		closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
		return true;
	}
	
	/* Handle Zap dialog keyboard shortcuts */
	if (dlg->type == DIALOG_ZAP)
	{
		if (keycode == 'a' || keycode == 'A')
		{
			closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_ALL);
			return true;
		}
		if (keycode == 's' || keycode == 'S')
		{
			closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_SONG);
			return true;
		}
		if (keycode == 'i' || keycode == 'I')
		{
			closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_INSTR);
			return true;
		}
		if (keycode == 'c' || keycode == 'C')
		{
			closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
			return true;
		}
		return true; /* Consume other keys */
	}
	
	/* Enter confirms */
	if (keycode == FT2_KEY_RETURN)
	{
		closeDialogWithResult(dlg, DIALOG_RESULT_OK);
		return true;
	}
	
	/* Handle input dialog keys - route to textbox system */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
	{
		if (keycode == FT2_KEY_BACKSPACE || keycode == FT2_KEY_DELETE ||
		    keycode == FT2_KEY_LEFT || keycode == FT2_KEY_RIGHT ||
		    keycode == FT2_KEY_HOME || keycode == FT2_KEY_END)
		{
			ft2_textbox_handle_key(keycode, 0);
			return true;
		}
	}
	
	return true; /* Consume all keys when dialog is open */
}

bool ft2_dialog_char_input(ft2_dialog_t *dlg, char c)
{
	if (dlg == NULL || !dlg->active || (dlg->type != DIALOG_INPUT && dlg->type != DIALOG_INPUT_PREVIEW))
		return false;
	
	/* Route to textbox system for proper handling */
	ft2_textbox_input_char(c);
	return true;
}

/* Draw a button (matching standalone FT2 drawPushButton style) */
static void drawButton(ft2_video_t *video, const ft2_bmp_t *bmp, int x, int y, int w, int h, 
                       const char *text, bool pressed)
{
	/* Fill button background */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	
	/* Draw outer border (black frame) */
	hLine(video, x, y, w, PAL_BCKGRND);
	hLine(video, x, y + h - 1, w, PAL_BCKGRND);
	vLine(video, x, y, h, PAL_BCKGRND);
	vLine(video, x + w - 1, y, h, PAL_BCKGRND);
	
	/* Draw inner borders (3D bevel effect) */
	if (!pressed)
	{
		/* Unpressed: light top-left, dark bottom-right */
		hLine(video, x + 1, y + 1, w - 3, PAL_BUTTON1);
		vLine(video, x + 1, y + 2, h - 4, PAL_BUTTON1);
		hLine(video, x + 1, y + h - 2, w - 2, PAL_BUTTON2);
		vLine(video, x + w - 2, y + 1, h - 3, PAL_BUTTON2);
	}
	else
	{
		/* Pressed: dark top-left only */
		hLine(video, x + 1, y + 1, w - 2, PAL_BUTTON2);
		vLine(video, x + 1, y + 2, h - 3, PAL_BUTTON2);
	}
	
	/* Text (centered, using PAL_BTNTEXT for proper theming) */
	int textLen = textWidth(text);
	int textX = x + (w - textLen) / 2;
	int textY = y + (h - 8) / 2;
	
	/* When pressed, offset text by 1 pixel for "pressed" effect */
	if (pressed)
	{
		textX++;
		textY++;
	}
	
	textOut(video, bmp, textX, textY, PAL_BTNTEXT, text);
}

void ft2_dialog_draw(ft2_dialog_t *dlg, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (dlg == NULL || !dlg->active || video == NULL)
		return;
	
	int x = dlg->x;
	int y = dlg->y;
	int w = dlg->w;
	int h = dlg->h;
	
	/* Main fill */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	
	/* Outer border */
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	
	/* Inner border */
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);
	
	/* Title bottom line */
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);
	
	/* Headline (with shadow, matching standalone) */
	int hlen = textWidth(dlg->headline);
	int headlineX = (SCREEN_W - hlen) / 2;
	textOutShadow(video, bmp, headlineX, y + 4, PAL_FORGRND, PAL_BUTTON2, dlg->headline);
	
	/* Text (with shadow) - NOT drawn for input dialogs (they only have headline, like standalone) */
	if (dlg->type != DIALOG_INPUT && dlg->type != DIALOG_INPUT_PREVIEW)
	{
	textOutShadow(video, bmp, dlg->textX, dlg->textY, PAL_FORGRND, PAL_BUTTON2, dlg->text);
	}
	
	/* Buttons */
	const char *btn1Text = "OK";
	const char *btn2Text = "Cancel";
	const char *btn3Text = "";
	const char *btn4Text = "";
	
	switch (dlg->type)
	{
		case DIALOG_OK:
			btn1Text = "OK";
			break;
		case DIALOG_OK_CANCEL:
			btn1Text = "OK";
			btn2Text = "Cancel";
			break;
		case DIALOG_YES_NO:
			btn1Text = "Yes";
			btn2Text = "No";
			break;
		case DIALOG_INPUT:
			btn1Text = "OK";
			btn2Text = "Cancel";
			break;
		case DIALOG_INPUT_PREVIEW:
			btn1Text = "OK";
			btn2Text = "Preview";
			btn3Text = "Cancel";
			break;
		case DIALOG_ZAP:
			btn1Text = "All";
			btn2Text = "Song";
			btn3Text = "Instrs.";
			btn4Text = "Cancel";
			break;
		default:
			break;
	}
	
	drawButton(video, bmp, dlg->button1X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H,
	           btn1Text, dlg->button1Pressed);
	
	if (dlg->type != DIALOG_OK)
	{
		drawButton(video, bmp, dlg->button2X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H,
		           btn2Text, dlg->button2Pressed);
	}
	
	if (dlg->type == DIALOG_INPUT_PREVIEW || dlg->type == DIALOG_ZAP)
	{
		drawButton(video, bmp, dlg->button3X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H,
		           btn3Text, dlg->button3Pressed);
	}
	
	if (dlg->type == DIALOG_ZAP)
	{
		drawButton(video, bmp, dlg->button4X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H,
		           btn4Text, dlg->button4Pressed);
	}
	
	/* Input field for input dialogs - use the textbox system for correct cursor positioning */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
	{
		ft2_textbox_draw(video, bmp, TB_DIALOG_INPUT, dlg->instance);
	}
}

