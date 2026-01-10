/*
** FT2 Plugin - Textbox Input System API
** Port of ft2_textboxes.c: text editing for instrument/sample names, song name, etc.
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
enum {
	TB_INST1, TB_INST2, TB_INST3, TB_INST4, TB_INST5, TB_INST6, TB_INST7, TB_INST8,
	TB_SAMP1, TB_SAMP2, TB_SAMP3, TB_SAMP4, TB_SAMP5,
	TB_SONG_NAME, TB_DISKOP_FILENAME, TB_DIALOG_INPUT,
	NUM_TEXTBOXES
};

#define TEXT_SCROLL_VALUE 30 /* Pixels to scroll when cursor exceeds visible area */

typedef struct textBox_t {
	uint16_t x, y, w, h;    /* Position and size */
	uint8_t tx, ty;         /* Text inset */
	uint16_t maxChars;      /* Max character count */
	bool rightMouseButton;  /* Requires right-click to edit */
	char *textPtr;          /* Pointer to text buffer */
	bool visible, active;   /* State flags */
	int16_t cursorPos;      /* Cursor position in characters */
	uint8_t *renderBuf;     /* Offscreen buffer for clipped rendering */
	uint16_t renderW;       /* Visible width */
	uint16_t renderBufW, renderBufH; /* Buffer dimensions */
	int32_t bufOffset;      /* Horizontal scroll offset */
} textBox_t;

/* Per-instance textbox state (replaces former global variables) */
typedef struct ft2_textbox_state_t {
	textBox_t textBoxes[NUM_TEXTBOXES];
	int16_t activeTextBox;
	bool textEditActive;
	int16_t textBoxNeedsRedraw;
} ft2_textbox_state_t;

/* Initialization */
void ft2_textbox_init(ft2_textbox_state_t *state);
void ft2_textbox_update_pointers(struct ft2_instance_t *inst);
void ft2_textbox_free(ft2_textbox_state_t *state);

/* Mouse input */
int16_t ft2_textbox_test_mouse_down(struct ft2_instance_t *inst, int32_t x, int32_t y, bool rightButton);
void ft2_textbox_mouse_drag(struct ft2_instance_t *inst, int32_t x, int32_t y);

/* Keyboard input */
void ft2_textbox_input_char(struct ft2_instance_t *inst, char c);
void ft2_textbox_handle_key(struct ft2_instance_t *inst, int32_t keyCode, int32_t modifiers);

/* State control */
void ft2_textbox_exit_editing(struct ft2_instance_t *inst);
bool ft2_textbox_is_editing(struct ft2_instance_t *inst);
int16_t ft2_textbox_get_active(struct ft2_instance_t *inst);
int16_t ft2_textbox_get_needs_redraw(struct ft2_instance_t *inst);

/* Visibility */
void ft2_textbox_show(struct ft2_instance_t *inst, uint16_t textBoxID);
void ft2_textbox_hide(struct ft2_instance_t *inst, uint16_t textBoxID);

/* Position (for dynamic repositioning in extended/normal mode) */
void ft2_textbox_set_position(struct ft2_instance_t *inst, uint16_t textBoxID, uint16_t x, uint16_t y, uint16_t w);

/* Drawing */
void ft2_textbox_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t textBoxID, struct ft2_instance_t *inst);
void ft2_textbox_draw_with_cursor(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t textBoxID, bool showCursor, struct ft2_instance_t *inst);

/* Utilities */
bool ft2_textbox_is_marked(struct ft2_instance_t *inst);
int16_t ft2_textbox_get_cursor_x(struct ft2_instance_t *inst, uint16_t textBoxID);
void ft2_textbox_set_cursor_end(struct ft2_instance_t *inst, uint16_t textBoxID);

/* Dialog textbox */
void ft2_textbox_configure_dialog(struct ft2_instance_t *inst, uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *textPtr, uint16_t maxChars);
void ft2_textbox_activate_dialog(struct ft2_instance_t *inst);
void ft2_textbox_deactivate_dialog(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif

