/**
 * @file ft2_plugin_widgets.c
 * @brief Unified widget management for the FT2 plugin UI.
 * 
 * Integrates pushbuttons, scrollbars, checkboxes, and radiobuttons.
 * Implements continuous hold behavior and mouse wheel handling.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_callbacks.h"
#include "ft2_plugin_help.h"
#include "ft2_plugin_instrsw.h"
#include "ft2_instance.h"

/* Object types for tracking what the mouse is interacting with */
enum
{
	OBJECT_NONE = 0,
	OBJECT_PUSHBUTTON,
	OBJECT_SCROLLBAR,
	OBJECT_CHECKBOX,
	OBJECT_RADIOBUTTON,
	OBJECT_TEXTBOX,
	OBJECT_INSTRSWITCH,
	OBJECT_PATTERNMARK,
	OBJECT_DISKOPLIST,
	OBJECT_SMPDATA,
	OBJECT_PIANO,
	OBJECT_INSVOLENV,
	OBJECT_INSPANENV,
	OBJECT_SCOPE
};

#define OBJECT_ID_NONE -1

/* Mouse state for widget interaction - matches original FT2 mouse_t structure */
static struct {
	int32_t x, y;
	int32_t lastX, lastY;
	int32_t absX, absY;
	int32_t rawX, rawY;
	bool leftButtonPressed;
	bool rightButtonPressed;
	bool leftButtonReleased;
	bool rightButtonReleased;
	bool rightClickTrackedObject;
	int16_t lastUsedObjectID;
	int8_t lastUsedObjectType;
	bool firstTimePressingButton;
	uint8_t buttonCounter;
	uint8_t mode;
	int8_t xBias, yBias;
	bool mouseOverTextBox;
} mouse = {0};

/* Mouse modes (for disk op delete/rename) */
enum
{
	MOUSE_MODE_NORMAL = 0,
	MOUSE_MODE_DELETE,
	MOUSE_MODE_RENAME
};

void ft2_widgets_init(ft2_widgets_t *widgets)
{
	if (widgets == NULL)
		return;

	/* Copy widget definitions from templates to per-instance arrays */
	memcpy(widgets->pushButtons, pushButtonsTemplate, sizeof(pushButtonsTemplate));
	memcpy(widgets->scrollBars, scrollBarsTemplate, sizeof(scrollBarsTemplate));

	/* Initialize per-instance widget visibility/state arrays */
	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
	{
		widgets->pushButtonVisible[i] = false;
		widgets->pushButtonDisabled[i] = false;
		widgets->pushButtonState[i] = 0;
	}
	for (int i = 0; i < NUM_CHECKBOXES; i++)
	{
		widgets->checkBoxVisible[i] = false;
		widgets->checkBoxDisabled[i] = false;
		widgets->checkBoxChecked[i] = false;
		widgets->checkBoxState[i] = 0;
	}
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		widgets->radioButtonVisible[i] = false;
		widgets->radioButtonDisabled[i] = false;
		widgets->radioButtonState[i] = 0;
	}
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		widgets->scrollBarDisabled[i] = false;
		widgets->scrollBarState[i].visible = false;
		widgets->scrollBarState[i].state = 0;
		widgets->scrollBarState[i].pos = 0;
		widgets->scrollBarState[i].page = 1;
		widgets->scrollBarState[i].end = 1;
		widgets->scrollBarState[i].thumbX = 0;
		widgets->scrollBarState[i].thumbY = 0;
		widgets->scrollBarState[i].thumbW = 0;
		widgets->scrollBarState[i].thumbH = 0;
	}

	/* Initialize all widget subsystems (callbacks and constant data only) */
	initPushButtons(widgets);
	initScrollBars(widgets);
	initCheckBoxes();
	initRadioButtons();

	/* Initialize callbacks (operates on per-instance widget arrays) */
	initCallbacks(widgets);

	/* Initialize help system (parses help data) */
	initFTHelp();

	/* Reset mouse state */
	mouse.x = 0;
	mouse.y = 0;
	mouse.lastX = 0;
	mouse.lastY = 0;
	mouse.absX = 0;
	mouse.absY = 0;
	mouse.rawX = 0;
	mouse.rawY = 0;
	mouse.leftButtonPressed = false;
	mouse.rightButtonPressed = false;
	mouse.leftButtonReleased = false;
	mouse.rightButtonReleased = false;
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.firstTimePressingButton = false;
	mouse.buttonCounter = 0;
	mouse.mode = MOUSE_MODE_NORMAL;
	mouse.xBias = 0;
	mouse.yBias = 0;
	mouse.mouseOverTextBox = false;

	/* Initialize widget container state */
	widgets->mouseX = 0;
	widgets->mouseY = 0;
	widgets->mouseDown = false;
	widgets->activeButton = -1;
}

void ft2_widgets_draw(ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (video == NULL || widgets == NULL)
		return;

	/* Draw all visible pushbuttons */
	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
	{
		if (widgets->pushButtonVisible[i])
			drawPushButton(widgets, video, bmp, i);
	}

	/* Draw all visible scrollbars */
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		if (widgets->scrollBarState[i].visible)
			drawScrollBar(widgets, video, i);
	}

	/* Draw all visible checkboxes */
	for (int i = 0; i < NUM_CHECKBOXES; i++)
	{
		if (widgets->checkBoxVisible[i])
			drawCheckBox(widgets, video, bmp, i);
	}

	/* Draw all visible radiobuttons */
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (widgets->radioButtonVisible[i])
			drawRadioButton(widgets, video, bmp, i);
	}
}

void ft2_widgets_mouse_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, int x, int y, bool sysReqShown)
{
	if (widgets == NULL)
		return;

	mouse.x = x;
	mouse.y = y;
	mouse.leftButtonPressed = true;
	/* Clear both released flags on any button down - matches standalone behavior */
	mouse.leftButtonReleased = false;
	mouse.rightButtonReleased = false;

	/* Don't reset if already tracking an object */
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.firstTimePressingButton = true;
	mouse.buttonCounter = 0;

	/* Test pushbuttons */
	int16_t pbID = testPushButtonMouseDown(widgets, inst, x, y, sysReqShown);
	if (pbID >= 0)
	{
		mouse.lastUsedObjectID = pbID;
		mouse.lastUsedObjectType = OBJECT_PUSHBUTTON;
		return;
	}

	/* Test scrollbars */
	int16_t sbID = testScrollBarMouseDown(widgets, inst, video, x, y, sysReqShown);
	if (sbID >= 0)
	{
		mouse.lastUsedObjectID = sbID;
		mouse.lastUsedObjectType = OBJECT_SCROLLBAR;
		return;
	}

	/* When system request is shown, don't test other widget types */
	if (sysReqShown)
		return;

	/* Test checkboxes */
	int16_t cbID = testCheckBoxMouseDown(widgets, x, y, false);
	if (cbID >= 0)
	{
		mouse.lastUsedObjectID = cbID;
		mouse.lastUsedObjectType = OBJECT_CHECKBOX;
		return;
	}

	/* Test radiobuttons */
	int16_t rbID = testRadioButtonMouseDown(widgets, x, y, false);
	if (rbID >= 0)
	{
		mouse.lastUsedObjectID = rbID;
		mouse.lastUsedObjectType = OBJECT_RADIOBUTTON;
		return;
	}

	/* Test instrument switcher */
	if (testInstrSwitcherMouseDown(inst, x, y))
		return;
}

void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst)
{
	mouse.x = x;
	mouse.y = y;
	mouse.rightButtonPressed = true;
	/* Clear both released flags on any button down - matches standalone behavior */
	mouse.leftButtonReleased = false;
	mouse.rightButtonReleased = false;

	/* Don't track new object if left button is already tracking */
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return;

	if (widgets == NULL)
		return;

	/* Only test pushbuttons for right-click (for predef envelope save) */
	int16_t pbID = testPushButtonMouseDown(widgets, inst, x, y, false);
	if (pbID >= 0)
	{
		mouse.lastUsedObjectID = pbID;
		mouse.lastUsedObjectType = OBJECT_PUSHBUTTON;
		mouse.rightClickTrackedObject = true;
		return;
	}
}

void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (widgets == NULL)
		return;

	mouse.x = x;
	mouse.y = y;
	mouse.leftButtonPressed = false;
	mouse.leftButtonReleased = true;
	mouse.firstTimePressingButton = false;
	mouse.buttonCounter = 0;

	/* If we used both mouse buttons at the same time and released one, don't release GUI object */
	if (mouse.rightButtonPressed)
		return;

	/* Handle release based on object type */
	switch (mouse.lastUsedObjectType)
	{
		case OBJECT_PUSHBUTTON:
			testPushButtonMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID, true);
			break;

		case OBJECT_SCROLLBAR:
			testScrollBarMouseRelease(widgets, video, mouse.lastUsedObjectID);
			break;

		case OBJECT_CHECKBOX:
			testCheckBoxMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID);
			break;

		case OBJECT_RADIOBUTTON:
			testRadioButtonMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID);
			break;

		default:
			break;
	}

	/* Revert delete/rename mouse modes */
	if (mouse.mode != MOUSE_MODE_NORMAL)
	{
		/* Only reset mode if we didn't click on delete/rename buttons */
		mouse.mode = MOUSE_MODE_NORMAL;
	}

	mouse.lastX = 0;
	mouse.lastY = 0;
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
}

void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	mouse.x = x;
	mouse.y = y;
	mouse.rightButtonPressed = false;
	mouse.rightButtonReleased = true;

	/* If left button is still pressed, don't release the object */
	if (mouse.leftButtonPressed)
		return;

	/* Handle right-click release for pushbuttons (for predef envelope save) */
	if (mouse.rightClickTrackedObject && mouse.lastUsedObjectType == OBJECT_PUSHBUTTON)
	{
		testPushButtonMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID, true);
		mouse.lastUsedObjectID = OBJECT_ID_NONE;
		mouse.lastUsedObjectType = OBJECT_NONE;
		mouse.rightClickTrackedObject = false;
	}
}

void ft2_widgets_mouse_move(ft2_widgets_t *widgets, int x, int y)
{
	(void)widgets;

	mouse.lastX = mouse.x;
	mouse.lastY = mouse.y;
	mouse.x = x;
	mouse.y = y;
}

void ft2_widgets_handle_held_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (widgets == NULL)
		return;

	if (mouse.lastUsedObjectType == OBJECT_NONE)
		return;

	if (!mouse.leftButtonPressed && !mouse.rightButtonPressed)
		return;

	if (mouse.lastUsedObjectID == OBJECT_ID_NONE)
		return;

	/* Handle continuous interaction based on object type */
	switch (mouse.lastUsedObjectType)
	{
		case OBJECT_PUSHBUTTON:
			handlePushButtonWhileMouseDown(widgets, inst, video, bmp, mouse.x, mouse.y, mouse.lastUsedObjectID,
				&mouse.firstTimePressingButton, &mouse.buttonCounter);
			break;

		case OBJECT_SCROLLBAR:
			handleScrollBarWhileMouseDown(widgets, inst, video, mouse.x, mouse.y, mouse.lastUsedObjectID);
			break;

		case OBJECT_CHECKBOX:
			handleCheckBoxesWhileMouseDown(widgets, video, bmp, mouse.x, mouse.y, mouse.lastX, mouse.lastY, mouse.lastUsedObjectID);
			break;

		case OBJECT_RADIOBUTTON:
			handleRadioButtonsWhileMouseDown(widgets, video, bmp, mouse.x, mouse.y, mouse.lastX, mouse.lastY, mouse.lastUsedObjectID);
			break;

		default:
			break;
	}
}

void ft2_widgets_key_press(ft2_widgets_t *widgets, int key)
{
	(void)widgets;
	(void)key;
}

/* Access to mouse state for other modules */
int32_t getWidgetMouseX(void) { return mouse.x; }
int32_t getWidgetMouseY(void) { return mouse.y; }
int32_t getWidgetMouseLastX(void) { return mouse.lastX; }
int32_t getWidgetMouseLastY(void) { return mouse.lastY; }
bool isWidgetMouseDown(void) { return mouse.leftButtonPressed; }
bool isWidgetMouseRightDown(void) { return mouse.rightButtonPressed; }
bool isMouseLeftButtonReleased(void) { return mouse.leftButtonReleased; }
bool isMouseRightButtonReleased(void) { return mouse.rightButtonReleased; }
int16_t getLastUsedWidget(void) { return mouse.lastUsedObjectID; }
int8_t getLastUsedWidgetType(void) { return mouse.lastUsedObjectType; }
void setLastUsedWidget(int16_t id, int8_t type) { mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = type; }
uint8_t getMouseMode(void) { return mouse.mode; }
void setMouseMode(uint8_t mode) { mouse.mode = mode; }
