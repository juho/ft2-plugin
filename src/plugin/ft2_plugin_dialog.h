/**
 * @file ft2_plugin_dialog.h
 * @brief Modal dialog system: message, confirm, input, zap dialogs.
 *
 * Simpler than standalone's okBox/inputBox. Draws over the main UI,
 * consumes all input while active, and invokes a callback on close.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

typedef enum ft2_dialog_type_t {
	DIALOG_NONE = 0,
	DIALOG_OK,             /* [OK] */
	DIALOG_OK_CANCEL,      /* [OK] [Cancel] */
	DIALOG_YES_NO,         /* [Yes] [No] */
	DIALOG_INPUT,          /* Text input + [OK] [Cancel] */
	DIALOG_INPUT_PREVIEW,  /* Text input + [OK] [Preview] [Cancel] (sample effects) */
	DIALOG_ZAP             /* [All] [Song] [Instrs.] [Cancel] */
} ft2_dialog_type_t;

typedef enum ft2_dialog_result_t {
	DIALOG_RESULT_NONE = 0,
	DIALOG_RESULT_OK = 1,      /* OK/Yes/first button */
	DIALOG_RESULT_CANCEL = 2,  /* Cancel/No/last button */
	DIALOG_RESULT_PREVIEW = 3, /* Preview (keeps dialog open) */
	DIALOG_RESULT_YES = 1,     /* Alias for OK */
	DIALOG_RESULT_NO = 2,      /* Alias for Cancel */
	DIALOG_RESULT_ZAP_ALL = 10,
	DIALOG_RESULT_ZAP_SONG = 11,
	DIALOG_RESULT_ZAP_INSTR = 12
} ft2_dialog_result_t;

/* Preview callback: invoked when Preview button clicked (dialog stays open) */
typedef void (*ft2_dialog_preview_callback_t)(struct ft2_instance_t *inst, uint32_t value);

/* Completion callback: invoked when dialog closes */
typedef void (*ft2_dialog_callback_t)(
	struct ft2_instance_t *inst,
	ft2_dialog_result_t result,
	const char *inputText,
	void *userData
);

typedef struct ft2_dialog_t
{
	bool active;
	ft2_dialog_type_t type;
	ft2_dialog_result_t result;

	char headline[64];       /* Title bar text */
	char text[256];          /* Body text (not shown for input dialogs) */

	/* Input dialogs */
	char inputBuffer[256];
	int inputMaxLen;
	int inputCursorPos;

	/* Button press states (for visual feedback) */
	bool button1Pressed, button2Pressed, button3Pressed, button4Pressed;

	/* Callbacks */
	ft2_dialog_preview_callback_t previewCallback;
	ft2_dialog_callback_t onComplete;
	struct ft2_instance_t *instance;
	void *userData;

	/* Layout (calculated by calculateDialogSize) */
	int16_t x, y, w, h;
	int16_t button1X, button2X, button3X, button4X, buttonY;
	int16_t textX, textY;
} ft2_dialog_t;

void ft2_dialog_init(ft2_dialog_t *dlg);

/* Show dialogs (no callback) */
void ft2_dialog_show_message(ft2_dialog_t *dlg, const char *headline, const char *text);
void ft2_dialog_show_confirm(ft2_dialog_t *dlg, const char *headline, const char *text);
void ft2_dialog_show_yesno(ft2_dialog_t *dlg, const char *headline, const char *text);
void ft2_dialog_show_input(ft2_dialog_t *dlg, const char *headline, const char *text,
                           const char *defaultValue, int maxLen);
void ft2_dialog_show_input_preview(ft2_dialog_t *dlg, const char *headline, const char *text,
                                   const char *defaultValue, int maxLen,
                                   struct ft2_instance_t *inst,
                                   ft2_dialog_preview_callback_t previewCallback);

/* Show dialogs (with callback invoked on close) */
void ft2_dialog_show_yesno_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData);
void ft2_dialog_show_zap_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                            struct ft2_instance_t *inst,
                            ft2_dialog_callback_t onComplete, void *userData);
void ft2_dialog_show_input_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              const char *defaultValue, int maxLen,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData);
void ft2_dialog_show_input_preview_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                                      const char *defaultValue, int maxLen,
                                      struct ft2_instance_t *inst,
                                      ft2_dialog_preview_callback_t previewCallback,
                                      ft2_dialog_callback_t onComplete, void *userData);

/* Query */
bool ft2_dialog_is_active(const ft2_dialog_t *dlg);
ft2_dialog_result_t ft2_dialog_get_result(const ft2_dialog_t *dlg);
const char *ft2_dialog_get_input(const ft2_dialog_t *dlg);
void ft2_dialog_close(ft2_dialog_t *dlg);

/* Input handling (returns true if event was consumed) */
bool ft2_dialog_mouse_down(ft2_dialog_t *dlg, int x, int y, int button);
bool ft2_dialog_mouse_up(ft2_dialog_t *dlg, int x, int y, int button);
bool ft2_dialog_key_down(ft2_dialog_t *dlg, int keycode);
bool ft2_dialog_char_input(ft2_dialog_t *dlg, char c);

/* Render dialog over current framebuffer */
void ft2_dialog_draw(ft2_dialog_t *dlg, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

#ifdef __cplusplus
}
#endif

