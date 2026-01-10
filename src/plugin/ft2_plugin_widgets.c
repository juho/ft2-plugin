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

	/* Reset mouse state (per-instance) */
	memset(&widgets->mouse, 0, sizeof(widgets->mouse));
	widgets->mouse.lastUsedObjectID = OBJECT_ID_NONE;
	widgets->mouse.lastUsedObjectType = OBJECT_NONE;

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
	widget_mouse_state_t *m = &widgets->mouse;

	m->x = x; m->y = y;
	m->leftButtonPressed = true;
	m->leftButtonReleased = m->rightButtonReleased = false;

	if (m->lastUsedObjectType != OBJECT_NONE) return;

	m->lastUsedObjectID = OBJECT_ID_NONE;
	m->lastUsedObjectType = OBJECT_NONE;
	m->firstTimePressingButton = true;
	m->buttonCounter = 0;

	int16_t id;
	if ((id = testPushButtonMouseDown(widgets, inst, x, y, sysReqShown)) >= 0)
		{ m->lastUsedObjectID = id; m->lastUsedObjectType = OBJECT_PUSHBUTTON; return; }
	if ((id = testScrollBarMouseDown(widgets, inst, video, x, y, sysReqShown)) >= 0)
		{ m->lastUsedObjectID = id; m->lastUsedObjectType = OBJECT_SCROLLBAR; return; }
	if (sysReqShown) return;
	if ((id = testCheckBoxMouseDown(widgets, x, y, false)) >= 0)
		{ m->lastUsedObjectID = id; m->lastUsedObjectType = OBJECT_CHECKBOX; return; }
	if ((id = testRadioButtonMouseDown(widgets, x, y, false)) >= 0)
		{ m->lastUsedObjectID = id; m->lastUsedObjectType = OBJECT_RADIOBUTTON; return; }
	testInstrSwitcherMouseDown(inst, x, y);
}

void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst)
{
	if (!widgets) return;
	widget_mouse_state_t *m = &widgets->mouse;

	m->x = x; m->y = y;
	m->rightButtonPressed = true;
	m->leftButtonReleased = m->rightButtonReleased = false;

	if (m->lastUsedObjectType != OBJECT_NONE) return;

	/* Right-click only tests pushbuttons (for predef envelope save) */
	int16_t pbID = testPushButtonMouseDown(widgets, inst, x, y, false);
	if (pbID >= 0)
	{
		m->lastUsedObjectID = pbID;
		m->lastUsedObjectType = OBJECT_PUSHBUTTON;
		m->rightClickTrackedObject = true;
	}
}

void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!widgets) return;
	widget_mouse_state_t *m = &widgets->mouse;

	m->x = x; m->y = y;
	m->leftButtonPressed = false;
	m->leftButtonReleased = true;
	m->firstTimePressingButton = false;
	m->buttonCounter = 0;

	if (m->rightButtonPressed) return;

	switch (m->lastUsedObjectType)
	{
		case OBJECT_PUSHBUTTON:  testPushButtonMouseRelease(widgets, inst, video, bmp, x, y, m->lastUsedObjectID, true); break;
		case OBJECT_SCROLLBAR:   testScrollBarMouseRelease(widgets, inst, video, m->lastUsedObjectID); break;
		case OBJECT_CHECKBOX:    testCheckBoxMouseRelease(widgets, inst, video, bmp, x, y, m->lastUsedObjectID); break;
		case OBJECT_RADIOBUTTON: testRadioButtonMouseRelease(widgets, inst, video, bmp, x, y, m->lastUsedObjectID); break;
		default: break;
	}

	if (m->mode != MOUSE_MODE_NORMAL) m->mode = MOUSE_MODE_NORMAL;

	m->lastX = m->lastY = 0;
	m->lastUsedObjectID = OBJECT_ID_NONE;
	m->lastUsedObjectType = OBJECT_NONE;
}

void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!widgets) return;
	widget_mouse_state_t *m = &widgets->mouse;

	m->x = x; m->y = y;
	m->rightButtonPressed = false;
	m->rightButtonReleased = true;

	if (m->leftButtonPressed) return;

	if (m->rightClickTrackedObject && m->lastUsedObjectType == OBJECT_PUSHBUTTON)
	{
		testPushButtonMouseRelease(widgets, inst, video, bmp, x, y, m->lastUsedObjectID, true);
		m->lastUsedObjectID = OBJECT_ID_NONE;
		m->lastUsedObjectType = OBJECT_NONE;
		m->rightClickTrackedObject = false;
	}
}

void ft2_widgets_mouse_move(ft2_widgets_t *widgets, int x, int y)
{
	if (!widgets) return;
	widget_mouse_state_t *m = &widgets->mouse;
	m->lastX = m->x; m->lastY = m->y;
	m->x = x; m->y = y;
}

/* Handle continuous hold behavior (repeat for arrows, drag for scrollbars) */
void ft2_widgets_handle_held_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	if (!widgets) return;
	widget_mouse_state_t *m = &widgets->mouse;

	if (m->lastUsedObjectType == OBJECT_NONE) return;
	if (!m->leftButtonPressed && !m->rightButtonPressed) return;
	if (m->lastUsedObjectID == OBJECT_ID_NONE) return;

	switch (m->lastUsedObjectType)
	{
		case OBJECT_PUSHBUTTON:
			handlePushButtonWhileMouseDown(widgets, inst, video, bmp, m->x, m->y, m->lastUsedObjectID,
				&m->firstTimePressingButton, &m->buttonCounter);
			break;
		case OBJECT_SCROLLBAR:
			handleScrollBarWhileMouseDown(widgets, inst, video, m->x, m->y, m->lastUsedObjectID);
			break;
		case OBJECT_CHECKBOX:
			handleCheckBoxesWhileMouseDown(widgets, video, bmp, m->x, m->y, m->lastX, m->lastY, m->lastUsedObjectID);
			break;
		case OBJECT_RADIOBUTTON:
			handleRadioButtonsWhileMouseDown(widgets, video, bmp, m->x, m->y, m->lastX, m->lastY, m->lastUsedObjectID);
			break;
		default: break;
	}
}

void ft2_widgets_key_press(ft2_widgets_t *widgets, int key) { (void)widgets; (void)key; }

/* ------------------------------------------------------------------------- */
/*                      MOUSE STATE ACCESSORS                                */
/* ------------------------------------------------------------------------- */

int32_t getWidgetMouseX(ft2_widgets_t *w)      { return w ? w->mouse.x : 0; }
int32_t getWidgetMouseY(ft2_widgets_t *w)      { return w ? w->mouse.y : 0; }
int32_t getWidgetMouseLastX(ft2_widgets_t *w)  { return w ? w->mouse.lastX : 0; }
int32_t getWidgetMouseLastY(ft2_widgets_t *w)  { return w ? w->mouse.lastY : 0; }
bool isWidgetMouseDown(ft2_widgets_t *w)       { return w ? w->mouse.leftButtonPressed : false; }
bool isWidgetMouseRightDown(ft2_widgets_t *w)  { return w ? w->mouse.rightButtonPressed : false; }
bool isMouseLeftButtonReleased(ft2_widgets_t *w)  { return w ? w->mouse.leftButtonReleased : false; }
bool isMouseRightButtonReleased(ft2_widgets_t *w) { return w ? w->mouse.rightButtonReleased : false; }
int16_t getLastUsedWidget(ft2_widgets_t *w)    { return w ? w->mouse.lastUsedObjectID : -1; }
int8_t getLastUsedWidgetType(ft2_widgets_t *w) { return w ? w->mouse.lastUsedObjectType : OBJECT_NONE; }
void setLastUsedWidget(ft2_widgets_t *w, int16_t id, int8_t type) { if (w) { w->mouse.lastUsedObjectID = id; w->mouse.lastUsedObjectType = type; } }
uint8_t getMouseMode(ft2_widgets_t *w)         { return w ? w->mouse.mode : 0; }
void setMouseMode(ft2_widgets_t *w, uint8_t mode) { if (w) w->mouse.mode = mode; }
