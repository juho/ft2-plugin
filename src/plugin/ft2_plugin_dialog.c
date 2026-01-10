/**
 * @file ft2_plugin_dialog.c
 * @brief Modal dialog system: message, confirm, input, zap dialogs.
 *
 * Simpler than standalone's okBox/inputBox system. Draws over the main UI,
 * consumes all input while active, and invokes a callback on close.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_textbox.h"

#define DIALOG_H 67         /* Fixed height (matches standalone) */
#define DIALOG_BUTTON_W 80
#define DIALOG_BUTTON_H 16
#define DIALOG_MIN_W 200
#define DIALOG_MAX_W 600
#define SCREEN_W 632
#define SCREEN_H 400
#define INPUT_BOX_W 250     /* Text input field width */

/*
 * Calculate dialog size and position based on content.
 * Width fits headline, text, input field, and buttons. Height is fixed.
 * Buttons are centered horizontally; dialog is centered horizontally at fixed Y.
 */
static void calculateDialogSize(ft2_dialog_t *dlg)
{
	int hlen = textWidth(dlg->headline);
	int tlen = textWidth(dlg->text);
	int wlen = (hlen > tlen) ? hlen : tlen;

	/* Input dialogs need room for text field */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
		if (INPUT_BOX_W > wlen) wlen = INPUT_BOX_W;

	/* Button count determines minimum width */
	int numButtons;
	switch (dlg->type) {
		case DIALOG_OK:            numButtons = 1; break;
		case DIALOG_INPUT_PREVIEW: numButtons = 3; break;
		case DIALOG_ZAP:           numButtons = 4; break;
		default:                   numButtons = 2; break;
	}

	int buttonWidth = numButtons * 100 - 20;
	if (buttonWidth > wlen) wlen = buttonWidth;
	wlen += 100; /* Padding */

	if (wlen < DIALOG_MIN_W) wlen = DIALOG_MIN_W;
	if (wlen > DIALOG_MAX_W) wlen = DIALOG_MAX_W;

	dlg->w = (int16_t)wlen;
	dlg->h = DIALOG_H;
	dlg->x = (int16_t)((SCREEN_W - wlen) / 2);
	dlg->y = 249; /* Fixed Y (matches standalone) */

	/* Center buttons */
	int buttonsWidth = numButtons * 100 - 20;
	int buttonsStartX = (SCREEN_W - buttonsWidth) / 2;
	dlg->button1X = (int16_t)buttonsStartX;
	dlg->button2X = (int16_t)(buttonsStartX + 100);
	dlg->button3X = (int16_t)(buttonsStartX + 200);
	dlg->button4X = (int16_t)(buttonsStartX + 300);
	dlg->buttonY = (int16_t)(dlg->y + 42);

	/* Center text */
	dlg->textX = (int16_t)((SCREEN_W - tlen) / 2);
	dlg->textY = (int16_t)(dlg->y + 24);

	/* Input dialogs use the textbox system */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW) {
		int inputX = (SCREEN_W - INPUT_BOX_W) / 2;
		int inputY = dlg->y + 24;
		ft2_textbox_configure_dialog(dlg->instance, (uint16_t)inputX, (uint16_t)inputY,
		                             INPUT_BOX_W, 12, dlg->inputBuffer, (uint16_t)dlg->inputMaxLen);
		ft2_textbox_activate_dialog(dlg->instance);
	}
}

void ft2_dialog_init(ft2_dialog_t *dlg)
{
	if (dlg == NULL) return;
	memset(dlg, 0, sizeof(ft2_dialog_t));
	dlg->type = DIALOG_NONE;
	dlg->result = DIALOG_RESULT_NONE;
}

/* Helper: copy text safely, handle NULL */
static void copyText(char *dst, size_t dstSize, const char *src)
{
	if (src != NULL)
		strncpy(dst, src, dstSize - 1);
	else
		dst[0] = '\0';
}

/* ---------- Show functions (no callback) ---------- */

void ft2_dialog_show_message(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_OK;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	calculateDialogSize(dlg);
}

void ft2_dialog_show_confirm(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_OK_CANCEL;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	calculateDialogSize(dlg);
}

void ft2_dialog_show_yesno(ft2_dialog_t *dlg, const char *headline, const char *text)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_YES_NO;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = false;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	calculateDialogSize(dlg);
}

/* ---------- Show functions (with callback) ---------- */

void ft2_dialog_show_yesno_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_YES_NO;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = dlg->button4Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	dlg->inputBuffer[0] = '\0';
	dlg->inputCursorPos = dlg->inputMaxLen = 0;
	calculateDialogSize(dlg);
}

/* Zap dialog: All/Song/Instrs/Cancel (4 buttons) */
void ft2_dialog_show_zap_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                            struct ft2_instance_t *inst,
                            ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_ZAP;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = dlg->button4Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	dlg->inputBuffer[0] = '\0';
	dlg->inputCursorPos = dlg->inputMaxLen = 0;
	calculateDialogSize(dlg);
}

/* ---------- Input dialogs ---------- */

/* Helper: initialize input buffer */
static void initInputBuffer(ft2_dialog_t *dlg, const char *defaultValue, int maxLen)
{
	dlg->inputMaxLen = (maxLen > 0 && maxLen < 256) ? maxLen : 255;
	if (defaultValue != NULL) {
		strncpy(dlg->inputBuffer, defaultValue, (size_t)dlg->inputMaxLen);
		dlg->inputCursorPos = (int)strlen(dlg->inputBuffer);
	} else {
		dlg->inputBuffer[0] = '\0';
		dlg->inputCursorPos = 0;
	}
}

void ft2_dialog_show_input(ft2_dialog_t *dlg, const char *headline, const char *text,
                           const char *defaultValue, int maxLen)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_INPUT;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = NULL;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	initInputBuffer(dlg, defaultValue, maxLen);
	calculateDialogSize(dlg);
}

/* Input with preview button (OK/Preview/Cancel) - used for sample effects like Resample */
void ft2_dialog_show_input_preview(ft2_dialog_t *dlg, const char *headline, const char *text,
                                   const char *defaultValue, int maxLen,
                                   struct ft2_instance_t *inst,
                                   ft2_dialog_preview_callback_t previewCallback)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_INPUT_PREVIEW;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = false;
	dlg->previewCallback = previewCallback;
	dlg->instance = inst;
	dlg->onComplete = NULL;
	dlg->userData = NULL;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	initInputBuffer(dlg, defaultValue, maxLen);
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              const char *defaultValue, int maxLen,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_INPUT;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = false;
	dlg->previewCallback = NULL;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	initInputBuffer(dlg, defaultValue, maxLen);
	calculateDialogSize(dlg);
}

void ft2_dialog_show_input_preview_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                                      const char *defaultValue, int maxLen,
                                      struct ft2_instance_t *inst,
                                      ft2_dialog_preview_callback_t previewCallback,
                                      ft2_dialog_callback_t onComplete, void *userData)
{
	if (dlg == NULL) return;
	dlg->active = true;
	dlg->type = DIALOG_INPUT_PREVIEW;
	dlg->result = DIALOG_RESULT_NONE;
	dlg->button1Pressed = dlg->button2Pressed = dlg->button3Pressed = false;
	dlg->previewCallback = previewCallback;
	dlg->instance = inst;
	dlg->onComplete = onComplete;
	dlg->userData = userData;
	copyText(dlg->headline, sizeof(dlg->headline), headline);
	copyText(dlg->text, sizeof(dlg->text), text);
	initInputBuffer(dlg, defaultValue, maxLen);
	calculateDialogSize(dlg);
}

/* ---------- Query functions ---------- */

bool ft2_dialog_is_active(const ft2_dialog_t *dlg) { return dlg != NULL && dlg->active; }
ft2_dialog_result_t ft2_dialog_get_result(const ft2_dialog_t *dlg) { return dlg ? dlg->result : DIALOG_RESULT_NONE; }
const char *ft2_dialog_get_input(const ft2_dialog_t *dlg) { return dlg ? dlg->inputBuffer : ""; }
void ft2_dialog_close(ft2_dialog_t *dlg) { if (dlg) dlg->active = false; }

/* ---------- Input handling helpers ---------- */

static bool pointInButton(int x, int y, int bx, int by)
{
	return (x >= bx && x < bx + DIALOG_BUTTON_W && y >= by && y < by + DIALOG_BUTTON_H);
}

/* Close dialog, deactivate textbox if needed, invoke callback */
static void closeDialogWithResult(ft2_dialog_t *dlg, ft2_dialog_result_t result)
{
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
		ft2_textbox_deactivate_dialog(dlg->instance);

	dlg->result = result;
	dlg->active = false;

	if (dlg->onComplete != NULL)
		dlg->onComplete(dlg->instance, result, dlg->inputBuffer, dlg->userData);
}

/* ---------- Mouse handling ---------- */

bool ft2_dialog_mouse_down(ft2_dialog_t *dlg, int x, int y, int button)
{
	if (dlg == NULL || !dlg->active || button != 1) return false;

	if (pointInButton(x, y, dlg->button1X, dlg->buttonY)) { dlg->button1Pressed = true; return true; }
	if (dlg->type != DIALOG_OK && pointInButton(x, y, dlg->button2X, dlg->buttonY)) { dlg->button2Pressed = true; return true; }
	if ((dlg->type == DIALOG_INPUT_PREVIEW || dlg->type == DIALOG_ZAP) && pointInButton(x, y, dlg->button3X, dlg->buttonY)) { dlg->button3Pressed = true; return true; }
	if (dlg->type == DIALOG_ZAP && pointInButton(x, y, dlg->button4X, dlg->buttonY)) { dlg->button4Pressed = true; return true; }

	return true; /* Consume click even if not on button */
}

bool ft2_dialog_mouse_up(ft2_dialog_t *dlg, int x, int y, int button)
{
	if (dlg == NULL || !dlg->active || button != 1) return false;

	/* Button 1: OK/Yes/All */
	if (dlg->button1Pressed) {
		if (pointInButton(x, y, dlg->button1X, dlg->buttonY))
			closeDialogWithResult(dlg, dlg->type == DIALOG_ZAP ? DIALOG_RESULT_ZAP_ALL : DIALOG_RESULT_OK);
		dlg->button1Pressed = false;
		return true;
	}

	/* Button 2: Cancel/No/Preview/Song (depends on dialog type) */
	if (dlg->button2Pressed) {
		if (pointInButton(x, y, dlg->button2X, dlg->buttonY)) {
			if (dlg->type == DIALOG_INPUT_PREVIEW) {
				/* Preview: invoke callback but keep dialog open */
				dlg->result = DIALOG_RESULT_PREVIEW;
				if (dlg->previewCallback != NULL && dlg->inputBuffer[0] != '\0')
					dlg->previewCallback(dlg->instance, (uint32_t)atoi(dlg->inputBuffer));
			} else if (dlg->type == DIALOG_ZAP) {
				closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_SONG);
			} else {
				closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
			}
		}
		dlg->button2Pressed = false;
		return true;
	}

	/* Button 3: Cancel (INPUT_PREVIEW) or Instrs (ZAP) */
	if (dlg->button3Pressed) {
		if (pointInButton(x, y, dlg->button3X, dlg->buttonY))
			closeDialogWithResult(dlg, dlg->type == DIALOG_ZAP ? DIALOG_RESULT_ZAP_INSTR : DIALOG_RESULT_CANCEL);
		dlg->button3Pressed = false;
		return true;
	}

	/* Button 4: Cancel (ZAP only) */
	if (dlg->button4Pressed) {
		if (pointInButton(x, y, dlg->button4X, dlg->buttonY))
			closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
		dlg->button4Pressed = false;
		return true;
	}

	return true;
}

/* ---------- Keyboard handling ---------- */

bool ft2_dialog_key_down(ft2_dialog_t *dlg, int keycode)
{
	if (dlg == NULL || !dlg->active) return false;

	if (keycode == FT2_KEY_ESCAPE) {
		closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL);
		return true;
	}

	/* Zap dialog: A/S/I/C keyboard shortcuts */
	if (dlg->type == DIALOG_ZAP) {
		switch (keycode) {
			case 'a': case 'A': closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_ALL); return true;
			case 's': case 'S': closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_SONG); return true;
			case 'i': case 'I': closeDialogWithResult(dlg, DIALOG_RESULT_ZAP_INSTR); return true;
			case 'c': case 'C': closeDialogWithResult(dlg, DIALOG_RESULT_CANCEL); return true;
		}
		return true;
	}

	if (keycode == FT2_KEY_RETURN) {
		closeDialogWithResult(dlg, DIALOG_RESULT_OK);
		return true;
	}

	/* Input dialogs: route editing keys to textbox */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW) {
		if (keycode == FT2_KEY_BACKSPACE || keycode == FT2_KEY_DELETE ||
		    keycode == FT2_KEY_LEFT || keycode == FT2_KEY_RIGHT ||
		    keycode == FT2_KEY_HOME || keycode == FT2_KEY_END) {
			ft2_textbox_handle_key(dlg->instance, keycode, 0);
			return true;
		}
	}

	return true; /* Consume all keys while dialog is open */
}

bool ft2_dialog_char_input(ft2_dialog_t *dlg, char c)
{
	if (dlg == NULL || !dlg->active) return false;
	if (dlg->type != DIALOG_INPUT && dlg->type != DIALOG_INPUT_PREVIEW) return false;
	ft2_textbox_input_char(dlg->instance, c);
	return true;
}

/* ---------- Drawing ---------- */

/* Draw 3D beveled button (matches standalone drawPushButton style) */
static void drawButton(ft2_video_t *video, const ft2_bmp_t *bmp, int x, int y, int w, int h,
                       const char *text, bool pressed)
{
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	/* Outer border (black) */
	hLine(video, x, y, w, PAL_BCKGRND);
	hLine(video, x, y + h - 1, w, PAL_BCKGRND);
	vLine(video, x, y, h, PAL_BCKGRND);
	vLine(video, x + w - 1, y, h, PAL_BCKGRND);

	/* 3D bevel: light top-left, dark bottom-right (reversed when pressed) */
	if (!pressed) {
		hLine(video, x + 1, y + 1, w - 3, PAL_BUTTON1);
		vLine(video, x + 1, y + 2, h - 4, PAL_BUTTON1);
		hLine(video, x + 1, y + h - 2, w - 2, PAL_BUTTON2);
		vLine(video, x + w - 2, y + 1, h - 3, PAL_BUTTON2);
	} else {
		hLine(video, x + 1, y + 1, w - 2, PAL_BUTTON2);
		vLine(video, x + 1, y + 2, h - 3, PAL_BUTTON2);
	}

	/* Centered text, offset by 1px when pressed */
	int textLen = textWidth(text);
	int tx = x + (w - textLen) / 2 + (pressed ? 1 : 0);
	int ty = y + (h - 8) / 2 + (pressed ? 1 : 0);
	textOut(video, bmp, tx, ty, PAL_BTNTEXT, text);
}

void ft2_dialog_draw(ft2_dialog_t *dlg, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (dlg == NULL || !dlg->active || video == NULL) return;

	int x = dlg->x, y = dlg->y, w = dlg->w, h = dlg->h;

	/* Background */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	/* 3D border: outer light top-left, dark bottom-right */
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);

	/* Inner border (inset) */
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	/* Title bar separator */
	hLine(video, x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(video, x + 3, y + 17, w - 6, PAL_BUTTON1);

	/* Headline (centered) */
	int hlen = textWidth(dlg->headline);
	textOutShadow(video, bmp, (SCREEN_W - hlen) / 2, y + 4, PAL_FORGRND, PAL_BUTTON2, dlg->headline);

	/* Body text (not shown for input dialogs - matches standalone) */
	if (dlg->type != DIALOG_INPUT && dlg->type != DIALOG_INPUT_PREVIEW)
		textOutShadow(video, bmp, dlg->textX, dlg->textY, PAL_FORGRND, PAL_BUTTON2, dlg->text);

	/* Button labels vary by dialog type */
	const char *btn1 = "OK", *btn2 = "Cancel", *btn3 = "", *btn4 = "";
	switch (dlg->type) {
		case DIALOG_OK:            btn1 = "OK"; break;
		case DIALOG_OK_CANCEL:     btn1 = "OK"; btn2 = "Cancel"; break;
		case DIALOG_YES_NO:        btn1 = "Yes"; btn2 = "No"; break;
		case DIALOG_INPUT:         btn1 = "OK"; btn2 = "Cancel"; break;
		case DIALOG_INPUT_PREVIEW: btn1 = "OK"; btn2 = "Preview"; btn3 = "Cancel"; break;
		case DIALOG_ZAP:           btn1 = "All"; btn2 = "Song"; btn3 = "Instrs."; btn4 = "Cancel"; break;
		default: break;
	}

	drawButton(video, bmp, dlg->button1X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H, btn1, dlg->button1Pressed);
	if (dlg->type != DIALOG_OK)
		drawButton(video, bmp, dlg->button2X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H, btn2, dlg->button2Pressed);
	if (dlg->type == DIALOG_INPUT_PREVIEW || dlg->type == DIALOG_ZAP)
		drawButton(video, bmp, dlg->button3X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H, btn3, dlg->button3Pressed);
	if (dlg->type == DIALOG_ZAP)
		drawButton(video, bmp, dlg->button4X, dlg->buttonY, DIALOG_BUTTON_W, DIALOG_BUTTON_H, btn4, dlg->button4Pressed);

	/* Input field uses textbox system */
	if (dlg->type == DIALOG_INPUT || dlg->type == DIALOG_INPUT_PREVIEW)
		ft2_textbox_draw(video, bmp, TB_DIALOG_INPUT, dlg->instance);
}

