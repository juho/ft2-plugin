/*
** FT2 Plugin - Unified Widget Management
** Integrates pushbuttons, scrollbars, checkboxes, and radiobuttons.
** Implements mouse tracking, continuous hold behavior, and hit testing.
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

/* Object types for mouse tracking */
enum {
	OBJECT_NONE = 0, OBJECT_PUSHBUTTON, OBJECT_SCROLLBAR, OBJECT_CHECKBOX, OBJECT_RADIOBUTTON,
	OBJECT_TEXTBOX, OBJECT_INSTRSWITCH, OBJECT_PATTERNMARK, OBJECT_DISKOPLIST,
	OBJECT_SMPDATA, OBJECT_PIANO, OBJECT_INSVOLENV, OBJECT_INSPANENV, OBJECT_SCOPE
};
#define OBJECT_ID_NONE -1

/* Mouse state (matches FT2 mouse_t structure) */
static struct {
	int32_t x, y, lastX, lastY, absX, absY, rawX, rawY;
	bool leftButtonPressed, rightButtonPressed, leftButtonReleased, rightButtonReleased;
	bool rightClickTrackedObject;
	int16_t lastUsedObjectID;
	int8_t lastUsedObjectType;
	bool firstTimePressingButton;
	uint8_t buttonCounter, mode;
	int8_t xBias, yBias;
	bool mouseOverTextBox;
} mouse = {0};

enum { MOUSE_MODE_NORMAL = 0, MOUSE_MODE_DELETE, MOUSE_MODE_RENAME };

/* ------------------------------------------------------------------------- */
/*                         INITIALIZATION                                    */
/* ------------------------------------------------------------------------- */

void ft2_widgets_init(ft2_widgets_t *widgets)
{
	if (!widgets) return;

	/* Copy templates to per-instance arrays */
	memcpy(widgets->pushButtons, pushButtonsTemplate, sizeof(pushButtonsTemplate));
	memcpy(widgets->scrollBars, scrollBarsTemplate, sizeof(scrollBarsTemplate));

	/* Initialize per-instance state arrays */
	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
		{ widgets->pushButtonVisible[i] = false; widgets->pushButtonDisabled[i] = false; widgets->pushButtonState[i] = 0; }
	for (int i = 0; i < NUM_CHECKBOXES; i++)
		{ widgets->checkBoxVisible[i] = false; widgets->checkBoxDisabled[i] = false; widgets->checkBoxChecked[i] = false; widgets->checkBoxState[i] = 0; }
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
		{ widgets->radioButtonVisible[i] = false; widgets->radioButtonDisabled[i] = false; widgets->radioButtonState[i] = 0; }
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		widgets->scrollBarDisabled[i] = false;
		ft2_scrollbar_state_t *ss = &widgets->scrollBarState[i];
		ss->visible = false; ss->state = 0; ss->pos = 0; ss->page = 1; ss->end = 1;
		ss->thumbX = ss->thumbY = ss->thumbW = ss->thumbH = 0;
	}

	/* Initialize subsystems */
	initPushButtons(widgets);
	initScrollBars(widgets);
	initCheckBoxes();
	initRadioButtons();
	initCallbacks(widgets);
	initFTHelp();

	/* Reset mouse state */
	memset(&mouse, 0, sizeof(mouse));
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;

	widgets->mouseX = widgets->mouseY = 0;
	widgets->mouseDown = false;
	widgets->activeButton = -1;
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

void ft2_widgets_draw(ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!video || !widgets) return;

	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
		if (widgets->pushButtonVisible[i]) drawPushButton(widgets, video, bmp, i);
	for (int i = 0; i < NUM_SCROLLBARS; i++)
		if (widgets->scrollBarState[i].visible) drawScrollBar(widgets, video, i);
	for (int i = 0; i < NUM_CHECKBOXES; i++)
		if (widgets->checkBoxVisible[i]) drawCheckBox(widgets, video, bmp, i);
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
		if (widgets->radioButtonVisible[i]) drawRadioButton(widgets, video, bmp, i);
}

/* ------------------------------------------------------------------------- */
/*                        MOUSE HANDLING                                     */
/* ------------------------------------------------------------------------- */

void ft2_widgets_mouse_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, int x, int y, bool sysReqShown)
{
	if (!widgets) return;

	mouse.x = x; mouse.y = y;
	mouse.leftButtonPressed = true;
	mouse.leftButtonReleased = mouse.rightButtonReleased = false;

	if (mouse.lastUsedObjectType != OBJECT_NONE) return;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.firstTimePressingButton = true;
	mouse.buttonCounter = 0;

	int16_t id;
	if ((id = testPushButtonMouseDown(widgets, inst, x, y, sysReqShown)) >= 0)
		{ mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = OBJECT_PUSHBUTTON; return; }
	if ((id = testScrollBarMouseDown(widgets, inst, video, x, y, sysReqShown)) >= 0)
		{ mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = OBJECT_SCROLLBAR; return; }
	if (sysReqShown) return;
	if ((id = testCheckBoxMouseDown(widgets, x, y, false)) >= 0)
		{ mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = OBJECT_CHECKBOX; return; }
	if ((id = testRadioButtonMouseDown(widgets, x, y, false)) >= 0)
		{ mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = OBJECT_RADIOBUTTON; return; }
	testInstrSwitcherMouseDown(inst, x, y);
}

void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst)
{
	mouse.x = x; mouse.y = y;
	mouse.rightButtonPressed = true;
	mouse.leftButtonReleased = mouse.rightButtonReleased = false;

	if (mouse.lastUsedObjectType != OBJECT_NONE || !widgets) return;

	/* Right-click only tests pushbuttons (for predef envelope save) */
	int16_t pbID = testPushButtonMouseDown(widgets, inst, x, y, false);
	if (pbID >= 0)
	{
		mouse.lastUsedObjectID = pbID;
		mouse.lastUsedObjectType = OBJECT_PUSHBUTTON;
		mouse.rightClickTrackedObject = true;
	}
}

void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!widgets) return;

	mouse.x = x; mouse.y = y;
	mouse.leftButtonPressed = false;
	mouse.leftButtonReleased = true;
	mouse.firstTimePressingButton = false;
	mouse.buttonCounter = 0;

	if (mouse.rightButtonPressed) return;

	switch (mouse.lastUsedObjectType)
	{
		case OBJECT_PUSHBUTTON:  testPushButtonMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID, true); break;
		case OBJECT_SCROLLBAR:   testScrollBarMouseRelease(widgets, video, mouse.lastUsedObjectID); break;
		case OBJECT_CHECKBOX:    testCheckBoxMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID); break;
		case OBJECT_RADIOBUTTON: testRadioButtonMouseRelease(widgets, inst, video, bmp, x, y, mouse.lastUsedObjectID); break;
		default: break;
	}

	if (mouse.mode != MOUSE_MODE_NORMAL) mouse.mode = MOUSE_MODE_NORMAL;

	mouse.lastX = mouse.lastY = 0;
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
}

void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	mouse.x = x; mouse.y = y;
	mouse.rightButtonPressed = false;
	mouse.rightButtonReleased = true;

	if (mouse.leftButtonPressed) return;

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
	mouse.lastX = mouse.x; mouse.lastY = mouse.y;
	mouse.x = x; mouse.y = y;
}

/* Handle continuous hold behavior (repeat for arrows, drag for scrollbars) */
void ft2_widgets_handle_held_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!widgets || mouse.lastUsedObjectType == OBJECT_NONE) return;
	if (!mouse.leftButtonPressed && !mouse.rightButtonPressed) return;
	if (mouse.lastUsedObjectID == OBJECT_ID_NONE) return;

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
		default: break;
	}
}

void ft2_widgets_key_press(ft2_widgets_t *widgets, int key) { (void)widgets; (void)key; }

/* ------------------------------------------------------------------------- */
/*                      MOUSE STATE ACCESSORS                                */
/* ------------------------------------------------------------------------- */

int32_t getWidgetMouseX(void)      { return mouse.x; }
int32_t getWidgetMouseY(void)      { return mouse.y; }
int32_t getWidgetMouseLastX(void)  { return mouse.lastX; }
int32_t getWidgetMouseLastY(void)  { return mouse.lastY; }
bool isWidgetMouseDown(void)       { return mouse.leftButtonPressed; }
bool isWidgetMouseRightDown(void)  { return mouse.rightButtonPressed; }
bool isMouseLeftButtonReleased(void)  { return mouse.leftButtonReleased; }
bool isMouseRightButtonReleased(void) { return mouse.rightButtonReleased; }
int16_t getLastUsedWidget(void)    { return mouse.lastUsedObjectID; }
int8_t getLastUsedWidgetType(void) { return mouse.lastUsedObjectType; }
void setLastUsedWidget(int16_t id, int8_t type) { mouse.lastUsedObjectID = id; mouse.lastUsedObjectType = type; }
uint8_t getMouseMode(void)         { return mouse.mode; }
void setMouseMode(uint8_t mode)    { mouse.mode = mode; }
