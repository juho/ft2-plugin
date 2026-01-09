/*
** FT2 Plugin - Unified Widget Management API
** Static templates contain widget definitions (position, callbacks).
** Per-instance state (visibility, checked, pressed) stored in ft2_widgets_t.
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

#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_scrollbars.h"

/* Per-scrollbar runtime state (position, page, thumb geometry) */
typedef struct ft2_scrollbar_state_t {
	bool visible;
	uint8_t state;
	uint32_t pos, page, end;
	uint16_t thumbX, thumbY, thumbW, thumbH;
} ft2_scrollbar_state_t;

/* Widget state container */
typedef struct ft2_widgets_t {
	int mouseX, mouseY;
	bool mouseDown;
	int activeButton;

	pushButton_t pushButtons[NUM_PUSHBUTTONS];
	scrollBar_t scrollBars[NUM_SCROLLBARS];

	bool pushButtonVisible[NUM_PUSHBUTTONS];
	bool pushButtonDisabled[NUM_PUSHBUTTONS];
	uint8_t pushButtonState[NUM_PUSHBUTTONS];
	bool pushButtonLocked[NUM_PUSHBUTTONS];

	bool checkBoxVisible[NUM_CHECKBOXES];
	bool checkBoxDisabled[NUM_CHECKBOXES];
	bool checkBoxChecked[NUM_CHECKBOXES];
	uint8_t checkBoxState[NUM_CHECKBOXES];

	bool radioButtonVisible[NUM_RADIOBUTTONS];
	bool radioButtonDisabled[NUM_RADIOBUTTONS];
	uint8_t radioButtonState[NUM_RADIOBUTTONS];

	bool scrollBarDisabled[NUM_SCROLLBARS];
	ft2_scrollbar_state_t scrollBarState[NUM_SCROLLBARS];
} ft2_widgets_t;

/* Initialization */
void ft2_widgets_init(ft2_widgets_t *widgets);

/* Drawing */
void ft2_widgets_draw(ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Mouse input (sysReqShown limits hit testing to reserved dialog widget slots) */
void ft2_widgets_mouse_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, int x, int y, bool sysReqShown);
void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst);
void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_widgets_mouse_move(ft2_widgets_t *widgets, int x, int y);
void ft2_widgets_handle_held_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_widgets_key_press(ft2_widgets_t *widgets, int key);

/* Mouse state accessors */
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
