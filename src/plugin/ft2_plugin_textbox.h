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

/* Initialization */
void ft2_textbox_init(void);
void ft2_textbox_update_pointers(struct ft2_instance_t *inst);
void ft2_textbox_free(void);

/* Mouse input */
int16_t ft2_textbox_test_mouse_down(int32_t x, int32_t y, bool rightButton);
void ft2_textbox_mouse_drag(int32_t x, int32_t y);

/* Keyboard input */
void ft2_textbox_input_char(char c);
void ft2_textbox_handle_key(int32_t keyCode, int32_t modifiers);

/* State control */
void ft2_textbox_exit_editing(void);
bool ft2_textbox_is_editing(void);
int16_t ft2_textbox_get_active(void);
int16_t ft2_textbox_get_needs_redraw(void);

/* Visibility */
void ft2_textbox_show(uint16_t textBoxID);
void ft2_textbox_hide(uint16_t textBoxID);

/* Position (for dynamic repositioning in extended/normal mode) */
void ft2_textbox_set_position(uint16_t textBoxID, uint16_t x, uint16_t y, uint16_t w);

/* Drawing */
void ft2_textbox_draw(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t textBoxID, const struct ft2_instance_t *inst);
void ft2_textbox_draw_with_cursor(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t textBoxID, bool showCursor, const struct ft2_instance_t *inst);

/* Utilities */
bool ft2_textbox_is_marked(void);
int16_t ft2_textbox_get_cursor_x(uint16_t textBoxID);
void ft2_textbox_set_cursor_end(uint16_t textBoxID);

/* Dialog textbox */
void ft2_textbox_configure_dialog(uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *textPtr, uint16_t maxChars);
void ft2_textbox_activate_dialog(void);
void ft2_textbox_deactivate_dialog(void);

#ifdef __cplusplus
}
#endif

