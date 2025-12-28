/**
 * @file ft2_plugin_dialog.h
 * @brief Simple modal dialog system for the FT2 plugin
 *
 * This provides a simpler dialog system than the standalone FT2,
 * designed for synchronous operation within the plugin environment.
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

/* Dialog types */
typedef enum ft2_dialog_type_t
{
	DIALOG_NONE = 0,
	DIALOG_OK,              /* Single OK button */
	DIALOG_OK_CANCEL,       /* OK and Cancel buttons */
	DIALOG_YES_NO,          /* Yes and No buttons */
	DIALOG_INPUT,           /* Text input with OK/Cancel */
	DIALOG_INPUT_PREVIEW,   /* Text input with OK/Preview/Cancel (for filters) */
	DIALOG_ZAP              /* 4 buttons: All, Song, Instruments, Cancel */
} ft2_dialog_type_t;

/* Dialog result */
typedef enum ft2_dialog_result_t
{
	DIALOG_RESULT_NONE = 0,
	DIALOG_RESULT_OK = 1,      /* OK, Yes, or first button */
	DIALOG_RESULT_CANCEL = 2,  /* Cancel, No, or second button */
	DIALOG_RESULT_PREVIEW = 3, /* Preview button (keeps dialog open) */
	DIALOG_RESULT_YES = 1,
	DIALOG_RESULT_NO = 2,
	/* Zap dialog results */
	DIALOG_RESULT_ZAP_ALL = 10,
	DIALOG_RESULT_ZAP_SONG = 11,
	DIALOG_RESULT_ZAP_INSTR = 12
} ft2_dialog_result_t;

/* Preview callback for filter dialogs */
typedef void (*ft2_dialog_preview_callback_t)(struct ft2_instance_t *inst, uint32_t value);

/* Completion callback - invoked when dialog closes with OK or Cancel */
typedef void (*ft2_dialog_callback_t)(
	struct ft2_instance_t *inst,
	ft2_dialog_result_t result,
	const char *inputText,
	void *userData
);

/* Dialog state */
typedef struct ft2_dialog_t
{
	bool active;
	ft2_dialog_type_t type;
	ft2_dialog_result_t result;
	
	char headline[64];
	char text[256];
	
	/* For input dialogs */
	char inputBuffer[256];
	int inputMaxLen;
	int inputCursorPos;
	
	/* Button states */
	bool button1Pressed;
	bool button2Pressed;
	bool button3Pressed;
	bool button4Pressed;
	
	/* For preview dialogs */
	ft2_dialog_preview_callback_t previewCallback;
	struct ft2_instance_t *instance;
	
	/* Completion callback - invoked when dialog closes */
	ft2_dialog_callback_t onComplete;
	void *userData;
	
	/* Calculated positions */
	int16_t x, y, w, h;
	int16_t button1X, button2X, button3X, button4X, buttonY;
	int16_t textX, textY;
} ft2_dialog_t;

/* Initialize dialog system */
void ft2_dialog_init(ft2_dialog_t *dlg);

/* Show a simple message dialog */
void ft2_dialog_show_message(ft2_dialog_t *dlg, const char *headline, const char *text);

/* Show an OK/Cancel dialog */
void ft2_dialog_show_confirm(ft2_dialog_t *dlg, const char *headline, const char *text);

/* Show a Yes/No dialog */
void ft2_dialog_show_yesno(ft2_dialog_t *dlg, const char *headline, const char *text);

/* Show an input dialog */
void ft2_dialog_show_input(ft2_dialog_t *dlg, const char *headline, const char *text, 
                           const char *defaultValue, int maxLen);

/* Show an input dialog with preview button (for filters) */
void ft2_dialog_show_input_preview(ft2_dialog_t *dlg, const char *headline, const char *text,
                                   const char *defaultValue, int maxLen,
                                   struct ft2_instance_t *inst,
                                   ft2_dialog_preview_callback_t previewCallback);

/* Show a Yes/No dialog with completion callback */
void ft2_dialog_show_yesno_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData);

/* Show a Zap dialog (4 buttons: All, Song, Instruments, Cancel) with completion callback */
void ft2_dialog_show_zap_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData);

/* Show an input dialog with completion callback */
void ft2_dialog_show_input_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                              const char *defaultValue, int maxLen,
                              struct ft2_instance_t *inst,
                              ft2_dialog_callback_t onComplete, void *userData);

/* Show an input dialog with preview button and completion callback */
void ft2_dialog_show_input_preview_cb(ft2_dialog_t *dlg, const char *headline, const char *text,
                                      const char *defaultValue, int maxLen,
                                      struct ft2_instance_t *inst,
                                      ft2_dialog_preview_callback_t previewCallback,
                                      ft2_dialog_callback_t onComplete, void *userData);

/* Check if dialog is active */
bool ft2_dialog_is_active(const ft2_dialog_t *dlg);

/* Get dialog result (only valid after dialog closes) */
ft2_dialog_result_t ft2_dialog_get_result(const ft2_dialog_t *dlg);

/* Get input text (for input dialogs) */
const char *ft2_dialog_get_input(const ft2_dialog_t *dlg);

/* Close the dialog */
void ft2_dialog_close(ft2_dialog_t *dlg);

/* Handle mouse click on dialog */
bool ft2_dialog_mouse_down(ft2_dialog_t *dlg, int x, int y, int button);

/* Handle mouse release on dialog */
bool ft2_dialog_mouse_up(ft2_dialog_t *dlg, int x, int y, int button);

/* Handle key press on dialog */
bool ft2_dialog_key_down(ft2_dialog_t *dlg, int keycode);

/* Handle character input on dialog */
bool ft2_dialog_char_input(ft2_dialog_t *dlg, char c);

/* Draw the dialog */
void ft2_dialog_draw(ft2_dialog_t *dlg, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

#ifdef __cplusplus
}
#endif

