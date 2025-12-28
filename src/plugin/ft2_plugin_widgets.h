/**
 * @file ft2_plugin_widgets.h
 * @brief Unified widget management for the FT2 plugin UI.
 * 
 * Integrates pushbuttons, scrollbars, checkboxes, and radiobuttons.
 * Uses static global arrays matching the original FT2 implementation.
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

/**
 * Widget state container.
 * Note: Actual widgets use static global arrays (pushButtons, scrollBars, etc.)
 * This struct is kept for API compatibility but internal arrays are unused.
 */
typedef struct ft2_widgets_t
{
	int mouseX, mouseY;
	bool mouseDown;
	int activeButton;
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
 */
void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y);

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
 */
void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst);

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
 * Handle mouse wheel event.
 * @param widgets Widget state container
 * @param inst FT2 instance
 * @param delta Wheel delta (positive = up, negative = down)
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 */
void ft2_widgets_mouse_wheel(ft2_widgets_t *widgets, struct ft2_instance_t *inst, int delta, int x, int y);

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
