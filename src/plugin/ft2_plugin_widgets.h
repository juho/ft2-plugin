/**
 * @file ft2_plugin_widgets.h
 * @brief Unified widget management for the FT2 plugin UI.
 * 
 * Integrates pushbuttons, scrollbars, checkboxes, and radiobuttons.
 * Static global arrays contain constant widget definitions (position, callbacks).
 * Per-instance state (visibility, checked, pressed) is stored in ft2_widgets_t.
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

/* Include widget headers for NUM_* constants */
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_scrollbars.h"

/**
 * Per-scrollbar runtime state (position, page, thumb geometry).
 * The static arrays store the constant definition (x, y, w, h, callbacks).
 */
typedef struct ft2_scrollbar_state_t
{
	bool visible;
	uint8_t state;
	uint32_t pos, page, end;
	uint16_t thumbX, thumbY, thumbW, thumbH;
} ft2_scrollbar_state_t;

/**
 * Widget state container with per-instance widget definitions and state.
 * Template arrays (pushButtonsTemplate[], scrollBarsTemplate[]) contain
 * read-only default definitions. Per-instance copies enable independent
 * widget positions for extended mode and modal panels.
 */
typedef struct ft2_widgets_t
{
	int mouseX, mouseY;
	bool mouseDown;
	int activeButton;

	/* Per-instance widget definitions (copied from templates at init) */
	pushButton_t pushButtons[NUM_PUSHBUTTONS];
	scrollBar_t scrollBars[NUM_SCROLLBARS];

	/* Per-instance push button state */
	bool pushButtonVisible[NUM_PUSHBUTTONS];
	bool pushButtonDisabled[NUM_PUSHBUTTONS];
	uint8_t pushButtonState[NUM_PUSHBUTTONS];

	/* Per-instance checkbox state */
	bool checkBoxVisible[NUM_CHECKBOXES];
	bool checkBoxDisabled[NUM_CHECKBOXES];
	bool checkBoxChecked[NUM_CHECKBOXES];
	uint8_t checkBoxState[NUM_CHECKBOXES];

	/* Per-instance radio button state */
	bool radioButtonVisible[NUM_RADIOBUTTONS];
	bool radioButtonDisabled[NUM_RADIOBUTTONS];
	uint8_t radioButtonState[NUM_RADIOBUTTONS];

	/* Per-instance scrollbar state */
	bool scrollBarDisabled[NUM_SCROLLBARS];
	ft2_scrollbar_state_t scrollBarState[NUM_SCROLLBARS];
} ft2_widgets_t;

/**
 * Initialize all widget systems.
 * @param widgets Widget state container
 */
void ft2_widgets_init(ft2_widgets_t *widgets);

/**
 * Draw all visible widgets.
 * @param widgets Widget state container
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_widgets_draw(ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Handle left mouse button press.
 * @param widgets Widget state container
 * @param inst FT2 instance for context
 * @param video Video context
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param sysReqShown If true, only test reserved dialog widgets (slots 0-7 for buttons, 0-2 for scrollbars)
 */
void ft2_widgets_mouse_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, int x, int y, bool sysReqShown);

/**
 * Handle right mouse button press.
 * @param widgets Widget state container
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param inst FT2 instance for callbacks
 */
void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst);

/**
 * Handle left mouse button release.
 * @param widgets Widget state container
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param inst FT2 instance for callbacks
 * @param video Video context for redrawing
 * @param bmp Bitmap assets for redrawing
 */
void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst, 
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Handle right mouse button release.
 * @param widgets Widget state container
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param inst FT2 instance for callbacks
 * @param video Video context for redrawing
 * @param bmp Bitmap assets for redrawing
 */
void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Handle mouse movement.
 * @param widgets Widget state container
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 */
void ft2_widgets_mouse_move(ft2_widgets_t *widgets, int x, int y);

/**
 * Handle continuous held-down interaction (repeat behavior).
 * @param widgets Widget state container
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 */
void ft2_widgets_handle_held_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst, 
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Handle key press.
 * @param widgets Widget state container
 * @param key Key code
 */
void ft2_widgets_key_press(ft2_widgets_t *widgets, int key);

/* Mouse state access functions */
int32_t getWidgetMouseX(void);
int32_t getWidgetMouseY(void);
int32_t getWidgetMouseLastX(void);
int32_t getWidgetMouseLastY(void);
bool isWidgetMouseDown(void);
bool isWidgetMouseRightDown(void);
bool isMouseLeftButtonReleased(void);
bool isMouseRightButtonReleased(void);
int16_t getLastUsedWidget(void);
int8_t getLastUsedWidgetType(void);
void setLastUsedWidget(int16_t id, int8_t type);
uint8_t getMouseMode(void);
void setMouseMode(uint8_t mode);

#ifdef __cplusplus
}
#endif
