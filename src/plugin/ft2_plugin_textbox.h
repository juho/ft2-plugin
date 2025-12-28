/**
 * @file ft2_plugin_textbox.h
 * @brief Text box input system for the FT2 plugin.
 * 
 * Handles text input for instrument/sample names and other text fields.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

/* Textbox IDs */
enum
{
	TB_INST1,
	TB_INST2,
	TB_INST3,
	TB_INST4,
	TB_INST5,
	TB_INST6,
	TB_INST7,
	TB_INST8,
	TB_SAMP1,
	TB_SAMP2,
	TB_SAMP3,
	TB_SAMP4,
	TB_SAMP5,
	TB_SONG_NAME,
	TB_DISKOP_FILENAME,
	TB_DIALOG_INPUT,
	NUM_TEXTBOXES
};

typedef struct textBox_t
{
	uint16_t x, y, w, h;
	uint8_t tx, ty;
	uint16_t maxChars;
	bool rightMouseButton;
	char *textPtr;
	bool visible;
	bool active;
	int16_t cursorPos;
} textBox_t;

/**
 * Initialize the textbox system.
 */
void ft2_textbox_init(void);

/**
 * Update textbox pointers for current instrument/sample.
 * @param inst FT2 instance
 */
void ft2_textbox_update_pointers(struct ft2_instance_t *inst);

/**
 * Test if a mouse click is on a textbox.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param rightButton true if right mouse button
 * @return Textbox ID if hit, -1 otherwise
 */
int16_t ft2_textbox_test_mouse_down(int32_t x, int32_t y, bool rightButton);

/**
 * Handle a character input while editing a textbox.
 * @param c Character to input
 */
void ft2_textbox_input_char(char c);

/**
 * Handle special key input (backspace, delete, arrows, etc.)
 * @param keyCode Key code
 * @param modifiers Modifier flags
 */
void ft2_textbox_handle_key(int32_t keyCode, int32_t modifiers);

/**
 * Exit text editing mode.
 */
void ft2_textbox_exit_editing(void);

/**
 * Get textbox ID that needs redraw after editing exited (clears the flag).
 * @return Textbox ID or -1 if none
 */
int16_t ft2_textbox_get_needs_redraw(void);

/**
 * Check if text editing is active.
 * @return true if editing a textbox
 */
bool ft2_textbox_is_editing(void);

/**
 * Draw a textbox.
 * @param video Video context
 * @param bmp Bitmap assets
 * @param textBoxID Textbox ID
 * @param inst FT2 instance (for determining correct background color)
 */
void ft2_textbox_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t textBoxID, const struct ft2_instance_t *inst);

/**
 * Draw a textbox with optional cursor display.
 * @param video Video context
 * @param bmp Bitmap assets
 * @param textBoxID Textbox ID
 * @param showCursor Whether to show the cursor (for blinking effect)
 * @param inst FT2 instance (for determining correct background color based on selection)
 */
void ft2_textbox_draw_with_cursor(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, 
                                  uint16_t textBoxID, bool showCursor, 
                                  const struct ft2_instance_t *inst);

/**
 * Get the currently active textbox ID.
 * @return Active textbox ID or -1
 */
int16_t ft2_textbox_get_active(void);

/**
 * Show a textbox.
 * @param textBoxID Textbox ID
 */
void ft2_textbox_show(uint16_t textBoxID);

/**
 * Hide a textbox.
 * @param textBoxID Textbox ID
 */
void ft2_textbox_hide(uint16_t textBoxID);

/**
 * Check if text is currently marked/selected.
 * @return true if text is marked
 */
bool ft2_textbox_is_marked(void);

/**
 * Get the cursor X position.
 * @param textBoxID Textbox ID
 * @return Cursor X position in pixels
 */
int16_t ft2_textbox_get_cursor_x(uint16_t textBoxID);

/**
 * Set cursor to end of text.
 * @param textBoxID Textbox ID
 */
void ft2_textbox_set_cursor_end(uint16_t textBoxID);

/**
 * Handle textbox while mouse is held down (for selection).
 * @param x Mouse X position
 * @param y Mouse Y position
 */
void ft2_textbox_mouse_drag(int32_t x, int32_t y);

/**
 * Configure the dialog input textbox for use in input dialogs.
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param textPtr Pointer to text buffer
 * @param maxChars Maximum characters allowed
 */
void ft2_textbox_configure_dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                  char *textPtr, uint16_t maxChars);

/**
 * Activate the dialog textbox for editing.
 */
void ft2_textbox_activate_dialog(void);

/**
 * Deactivate the dialog textbox.
 */
void ft2_textbox_deactivate_dialog(void);

#ifdef __cplusplus
}
#endif

