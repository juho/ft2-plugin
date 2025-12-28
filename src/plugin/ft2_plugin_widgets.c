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

/* Forward declarations */
static void handlePatternWheelScroll(struct ft2_instance_t *inst, bool directionUp);

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

	/* Initialize all widget subsystems */
	initPushButtons();
	initScrollBars();
	initCheckBoxes();
	initRadioButtons();

	/* Initialize callbacks */
	initCallbacks();

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
	if (video == NULL)
		return;

	(void)widgets; /* Use global arrays now */

	/* Draw all visible pushbuttons */
	for (int i = 0; i < NUM_PUSHBUTTONS; i++)
	{
		if (pushButtons[i].visible)
			drawPushButton(video, bmp, i);
	}

	/* Draw all visible scrollbars */
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		if (scrollBars[i].visible)
			drawScrollBar(video, i);
	}

	/* Draw all visible checkboxes */
	for (int i = 0; i < NUM_CHECKBOXES; i++)
	{
		if (checkBoxes[i].visible)
			drawCheckBox(video, bmp, i);
	}

	/* Draw all visible radiobuttons */
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].visible)
			drawRadioButton(video, bmp, i);
	}
}

void ft2_widgets_mouse_down(ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, int x, int y, bool sysReqShown)
{
	(void)widgets;

	mouse.x = x;
	mouse.y = y;
	mouse.leftButtonPressed = true;
	mouse.leftButtonReleased = false;

	/* Don't reset if already tracking an object */
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.firstTimePressingButton = true;
	mouse.buttonCounter = 0;

	/* Test pushbuttons */
	int16_t pbID = testPushButtonMouseDown(inst, x, y, sysReqShown);
	if (pbID >= 0)
	{
		mouse.lastUsedObjectID = pbID;
		mouse.lastUsedObjectType = OBJECT_PUSHBUTTON;
		return;
	}

	/* Test scrollbars */
	int16_t sbID = testScrollBarMouseDown(inst, video, x, y, sysReqShown);
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
	int16_t cbID = testCheckBoxMouseDown(x, y, false);
	if (cbID >= 0)
	{
		mouse.lastUsedObjectID = cbID;
		mouse.lastUsedObjectType = OBJECT_CHECKBOX;
		return;
	}

	/* Test radiobuttons */
	int16_t rbID = testRadioButtonMouseDown(x, y, false);
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

void ft2_widgets_mouse_down_right(ft2_widgets_t *widgets, int x, int y)
{
	(void)widgets;

	mouse.x = x;
	mouse.y = y;
	mouse.rightButtonPressed = true;
	mouse.rightButtonReleased = false;
}

void ft2_widgets_mouse_up(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp)
{
	(void)widgets;

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
			testPushButtonMouseRelease(inst, video, bmp, x, y, mouse.lastUsedObjectID, true);
			break;

		case OBJECT_SCROLLBAR:
			testScrollBarMouseRelease(video, mouse.lastUsedObjectID);
			break;

		case OBJECT_CHECKBOX:
			testCheckBoxMouseRelease(inst, video, bmp, x, y, mouse.lastUsedObjectID);
			break;

		case OBJECT_RADIOBUTTON:
			testRadioButtonMouseRelease(inst, video, bmp, x, y, mouse.lastUsedObjectID);
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

void ft2_widgets_mouse_up_right(ft2_widgets_t *widgets, int x, int y, struct ft2_instance_t *inst)
{
	(void)widgets;
	(void)x;
	(void)y;
	(void)inst;

	mouse.rightButtonPressed = false;
	mouse.rightButtonReleased = true;
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
	(void)widgets;

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
			handlePushButtonWhileMouseDown(inst, video, bmp, mouse.x, mouse.y, mouse.lastUsedObjectID,
				&mouse.firstTimePressingButton, &mouse.buttonCounter);
			break;

		case OBJECT_SCROLLBAR:
			handleScrollBarWhileMouseDown(inst, video, mouse.x, mouse.y, mouse.lastUsedObjectID);
			break;

		case OBJECT_CHECKBOX:
			handleCheckBoxesWhileMouseDown(video, bmp, mouse.x, mouse.y, mouse.lastX, mouse.lastY, mouse.lastUsedObjectID);
			break;

		case OBJECT_RADIOBUTTON:
			handleRadioButtonsWhileMouseDown(video, bmp, mouse.x, mouse.y, mouse.lastX, mouse.lastY, mouse.lastUsedObjectID);
			break;

		default:
			break;
	}
}

void ft2_widgets_mouse_wheel(ft2_widgets_t *widgets, struct ft2_instance_t *inst, int delta, int x, int y)
{
	(void)widgets;

	if (inst == NULL)
		return;

	bool directionUp = (delta > 0);

	/* Handle mouse wheel based on screen area */
	if (inst->uiState.extendedPatternEditor)
	{
		/* Extended pattern editor */
		if (y <= 52)
		{
			if (x <= 111)
			{
				/* Song position area */
				bool posChanged = false;
				if (directionUp)
				{
					if (inst->replayer.song.songPos > 0)
					{
						inst->replayer.song.songPos--;
						posChanged = true;
					}
				}
				else
				{
					if (inst->replayer.song.songPos < inst->replayer.song.songLength - 1)
					{
						inst->replayer.song.songPos++;
						posChanged = true;
					}
				}
				
				if (posChanged)
				{
					/* Update pattern and reset row (matches standalone setNewSongPos) */
					inst->replayer.song.pattNum = inst->replayer.song.orders[inst->replayer.song.songPos];
					inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
					inst->replayer.song.row = 0;
					if (!inst->replayer.songPlaying)
					{
						inst->editor.row = 0;
						inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
					}
					inst->uiState.updatePosSections = true;
					inst->uiState.updatePosEdScrollBar = true;
					inst->uiState.updatePatternEditor = true;
				}
			}
			else if (x >= 386)
			{
				/* Instrument area */
				if (directionUp)
				{
					if (inst->editor.curInstr > 0)
						inst->editor.curInstr--;
				}
				else
				{
					if (inst->editor.curInstr < 127)
						inst->editor.curInstr++;
				}
			}
		}
		else
		{
			/* Pattern area */
			handlePatternWheelScroll(inst, directionUp);
		}
	}
	else if (y < 173)
	{
		/* Top screens */
		if (inst->uiState.helpScreenShown)
		{
			/* Help screen scroll */
			/* TODO: Implement help scrolling */
		}
		else if (inst->uiState.diskOpShown)
		{
			/* Disk op scroll - handled by scrollbar */
		}
		else if (inst->uiState.configScreenShown)
		{
			/* Config screen - device list scroll */
		}
		else
		{
			/* Main top screen */
			if (x >= 421 && y <= 173)
			{
				/* Instrument/sample area */
				if (y <= 93)
				{
					/* Instrument */
					if (directionUp)
					{
						if (inst->editor.curInstr > 0)
							inst->editor.curInstr--;
					}
					else
					{
						if (inst->editor.curInstr < 127)
							inst->editor.curInstr++;
					}
				}
				else
				{
					/* Sample */
					if (directionUp)
					{
						if (inst->editor.curSmp > 0)
							inst->editor.curSmp--;
					}
					else
					{
						if (inst->editor.curSmp < 15)
							inst->editor.curSmp++;
					}
				}
			}
			else if (x <= 111 && y <= 76)
			{
				/* Song position */
				if (directionUp)
				{
					if (inst->replayer.song.songPos > 0)
						inst->replayer.song.songPos--;
				}
				else
				{
					if (inst->replayer.song.songPos < inst->replayer.song.songLength - 1)
						inst->replayer.song.songPos++;
				}
				inst->editor.editPattern = inst->replayer.song.orders[inst->replayer.song.songPos];
			}
		}
	}
	else
	{
		/* Bottom screens */
		if (inst->uiState.sampleEditorShown)
		{
			/* Sample editor - zoom in/out */
			/* TODO: Implement sample zoom */
		}
		else if (inst->uiState.patternEditorShown)
		{
			/* Pattern editor - scroll rows */
			handlePatternWheelScroll(inst, directionUp);
		}
	}
}

static void handlePatternWheelScroll(struct ft2_instance_t *inst, bool directionUp)
{
	if (inst == NULL)
		return;

	if (inst->replayer.songPlaying)
		return;

	uint16_t numRows = inst->replayer.patternNumRows[inst->editor.editPattern];
	
	if (directionUp)
	{
		if (inst->editor.row > 0)
			inst->editor.row--;
		else
			inst->editor.row = numRows - 1;
	}
	else
	{
		if (inst->editor.row < numRows - 1)
			inst->editor.row++;
		else
			inst->editor.row = 0;
	}

	inst->uiState.updatePatternEditor = true;
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
