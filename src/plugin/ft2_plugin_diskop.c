/**
 * @file ft2_plugin_diskop.c
 * @brief Disk operations implementation for the plugin.
 *
 * Provides FT2-style file browser with JUCE-backed directory operations.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_loader.h"
#include "ft2_plugin_load_mod.h"
#include "ft2_plugin_load_s3m.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_layout.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_pattern_ed.h"

/* File list display constants */
#define FILENAME_TEXT_X 170
#define FILESIZE_TEXT_X 295
#define DISKOP_LIST_Y 4
#define DISKOP_LIST_H 164

/* Count digits in a number (for right-aligning file sizes) */
static int numDigits(int32_t n)
{
	if (n <= 0) return 1;
	int count = 0;
	while (n != 0) { n /= 10; count++; }
	return count;
}

/* Find last extension offset in filename (returns -1 if no extension) */
static int32_t getExtOffset(const char *s, int32_t nameLen)
{
	if (s == NULL || nameLen < 1)
		return -1;

	for (int32_t i = nameLen - 1; i >= 0; i--)
	{
		if (s[i] == '.')
			return i;
	}

	return -1;
}

/* Truncate entry name to fit in file list (matching standalone) */
static void trimEntryName(char *name, bool isDir)
{
	char extBuffer[24];

	int32_t j = (int32_t)strlen(name);
	const int32_t extOffset = getExtOffset(name, j);
	int32_t extLen = (extOffset != -1) ? (int32_t)strlen(&name[extOffset]) : 0;
	j--;

	if (isDir)
	{
		/* Directory: truncate with ".." at end to fit 160-8 pixels */
		while (textWidth(name) > 160 - 8 && j >= 2)
		{
			name[j - 2] = '.';
			name[j - 1] = '.';
			name[j - 0] = '\0';
			j--;
		}
		return;
	}

	if (extOffset != -1 && extLen <= 4)
	{
		/* Has extension: preserve extension with ".. .ext" format */
		snprintf(extBuffer, sizeof(extBuffer), ".. %s", &name[extOffset]);

		extLen = (int32_t)strlen(extBuffer);
		while (textWidth(name) >= FILESIZE_TEXT_X - FILENAME_TEXT_X && j >= extLen + 1)
		{
			memcpy(&name[j - extLen], extBuffer, extLen + 1);
			j--;
		}
	}
	else
	{
		/* No extension: truncate with ".." at end */
		while (textWidth(name) >= FILESIZE_TEXT_X - FILENAME_TEXT_X && j >= 2)
		{
			name[j - 2] = '.';
			name[j - 1] = '.';
			name[j - 0] = '\0';
			j--;
		}
	}
}

/* ============ HELPER FUNCTIONS ============ */

static void drawSaveAsElements(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	/* Clear the save-as area with desktop color */
	fillRect(video, 5, 99, 60, 42, PAL_DESKTOP);

	switch (inst->diskop.itemType)
	{
		default:
		case FT2_DISKOP_ITEM_MODULE:
			textOutShadow(video, bmp, 19, 101, PAL_FORGRND, PAL_DSKTOP2, "MOD");
			textOutShadow(video, bmp, 19, 115, PAL_FORGRND, PAL_DSKTOP2, "XM");
			break;

		case FT2_DISKOP_ITEM_INSTR:
			textOutShadow(video, bmp, 19, 101, PAL_FORGRND, PAL_DSKTOP2, "XI");
			break;

		case FT2_DISKOP_ITEM_SAMPLE:
			textOutShadow(video, bmp, 19, 101, PAL_FORGRND, PAL_DSKTOP2, "RAW");
			textOutShadow(video, bmp, 19, 115, PAL_FORGRND, PAL_DSKTOP2, "IFF");
			textOutShadow(video, bmp, 19, 129, PAL_FORGRND, PAL_DSKTOP2, "WAV");
			break;

		case FT2_DISKOP_ITEM_PATTERN:
			textOutShadow(video, bmp, 19, 101, PAL_FORGRND, PAL_DSKTOP2, "XP");
			break;

		case FT2_DISKOP_ITEM_TRACK:
			textOutShadow(video, bmp, 19, 101, PAL_FORGRND, PAL_DSKTOP2, "XT");
			break;
	}
}

static void setDiskOpItemRadioButtons(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Uncheck all save format groups */
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_MOD_SAVEAS);
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_INS_SAVEAS);
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_SMP_SAVEAS);
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_PAT_SAVEAS);
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_TRK_SAVEAS);

	/* Hide all save format groups */
	hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_MOD_SAVEAS);
	hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_INS_SAVEAS);
	hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_SMP_SAVEAS);
	hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_PAT_SAVEAS);
	hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_TRK_SAVEAS);

	/* Set checked state for each save format */
	widgets->radioButtonState[RB_DISKOP_MOD_MOD + inst->diskop.saveFormat[FT2_DISKOP_ITEM_MODULE]] = RADIOBUTTON_CHECKED;
	widgets->radioButtonState[RB_DISKOP_SMP_RAW + inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE]] = RADIOBUTTON_CHECKED;
	widgets->radioButtonState[RB_DISKOP_INS_XI] = RADIOBUTTON_CHECKED;
	widgets->radioButtonState[RB_DISKOP_PAT_XP] = RADIOBUTTON_CHECKED;
	widgets->radioButtonState[RB_DISKOP_TRK_XT] = RADIOBUTTON_CHECKED;

	/* Show the appropriate group based on current item type */
	if (inst->uiState.diskOpShown && video != NULL && bmp != NULL)
	{
		switch (inst->diskop.itemType)
		{
			default:
			case FT2_DISKOP_ITEM_MODULE:
				showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_MOD_SAVEAS);
				break;
			case FT2_DISKOP_ITEM_INSTR:
				showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_INS_SAVEAS);
				break;
			case FT2_DISKOP_ITEM_SAMPLE:
				showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_SMP_SAVEAS);
				break;
			case FT2_DISKOP_ITEM_PATTERN:
				showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_PAT_SAVEAS);
				break;
			case FT2_DISKOP_ITEM_TRACK:
				showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_TRK_SAVEAS);
				break;
		}
	}
}

static void displayCurrPath(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	/* Clear path area with desktop color */
	fillRect(video, 4, 145, 162, FONT1_CHAR_H, PAL_DESKTOP);

	/* Display truncated path */
	char pathBuf[256];
	strncpy(pathBuf, inst->diskop.currentPath, sizeof(pathBuf) - 1);
	pathBuf[sizeof(pathBuf) - 1] = '\0';

	/* Truncate if too long */
	int32_t len = (int32_t)strlen(pathBuf);
	while (textWidth(pathBuf) > 160 - 8 && len >= 3)
	{
		pathBuf[len - 3] = '.';
		pathBuf[len - 2] = '.';
		pathBuf[len - 1] = '\0';
		len--;
	}

	textOutClipX(video, bmp, 4, 145, PAL_FORGRND, pathBuf, 165);
}

/* ============ VISIBILITY ============ */

void showDiskOpScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	/* Hide other top screens first */
	hideTopScreen(inst);

	inst->uiState.diskOpShown = true;
	inst->uiState.scopesShown = false;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;

	/* Show disk op buttons */
	if (video != NULL && bmp != NULL && widgets != NULL)
	{
		showPushButton(widgets, video, bmp, PB_DISKOP_SAVE);
		showPushButton(widgets, video, bmp, PB_DISKOP_MAKEDIR);
		showPushButton(widgets, video, bmp, PB_DISKOP_REFRESH);
		showPushButton(widgets, video, bmp, PB_DISKOP_SET_PATH);
		showPushButton(widgets, video, bmp, PB_DISKOP_SHOW_ALL);
		showPushButton(widgets, video, bmp, PB_DISKOP_EXIT);
		showPushButton(widgets, video, bmp, PB_DISKOP_PARENT);
		showPushButton(widgets, video, bmp, PB_DISKOP_ROOT);
		showPushButton(widgets, video, bmp, PB_DISKOP_HOME);
		showPushButton(widgets, video, bmp, PB_DISKOP_LIST_UP);
		showPushButton(widgets, video, bmp, PB_DISKOP_LIST_DOWN);

		showScrollBar(widgets, video, SB_DISKOP_LIST);

		/* Show item type radio buttons */
		uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_ITEM);
		widgets->radioButtonState[RB_DISKOP_MODULE + inst->diskop.itemType] = RADIOBUTTON_CHECKED;
		showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_ITEM);

		/* Set up save format radio buttons */
		setDiskOpItemRadioButtons(inst, video, bmp);
	}

	/* Initialize directory on first open */
	if (inst->diskop.firstOpen)
	{
		inst->diskop.firstOpen = false;
		inst->diskop.requestGoHome = true; /* Start at home directory */
	}

#ifdef _WIN32
	/* Request drive enumeration */
	inst->diskop.requestEnumerateDrives = true;
	inst->diskop.requestDriveIndex = -1;
#endif

	inst->uiState.needsFullRedraw = true;
}

void hideDiskOpScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		/* Hide disk op buttons */
		hidePushButton(widgets, PB_DISKOP_SAVE);
		hidePushButton(widgets, PB_DISKOP_MAKEDIR);
		hidePushButton(widgets, PB_DISKOP_REFRESH);
		hidePushButton(widgets, PB_DISKOP_SET_PATH);
		hidePushButton(widgets, PB_DISKOP_SHOW_ALL);
		hidePushButton(widgets, PB_DISKOP_EXIT);
		hidePushButton(widgets, PB_DISKOP_PARENT);
		hidePushButton(widgets, PB_DISKOP_ROOT);
		hidePushButton(widgets, PB_DISKOP_HOME);
#ifdef _WIN32
		hidePushButton(widgets, PB_DISKOP_DRIVE1);
		hidePushButton(widgets, PB_DISKOP_DRIVE2);
		hidePushButton(widgets, PB_DISKOP_DRIVE3);
		hidePushButton(widgets, PB_DISKOP_DRIVE4);
		hidePushButton(widgets, PB_DISKOP_DRIVE5);
		hidePushButton(widgets, PB_DISKOP_DRIVE6);
		hidePushButton(widgets, PB_DISKOP_DRIVE7);
#endif
		hidePushButton(widgets, PB_DISKOP_LIST_UP);
		hidePushButton(widgets, PB_DISKOP_LIST_DOWN);

		hideScrollBar(widgets, SB_DISKOP_LIST);

		/* Hide radio buttons */
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_ITEM);
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_MOD_SAVEAS);
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_INS_SAVEAS);
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_SMP_SAVEAS);
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_PAT_SAVEAS);
		hideRadioButtonGroup(widgets, RB_GROUP_DISKOP_TRK_SAVEAS);
	}

	/* Hide filename textbox */
	ft2_textbox_hide(TB_DISKOP_FILENAME);

	inst->uiState.diskOpShown = false;
	inst->uiState.scopesShown = true;
}

void toggleDiskOpScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	if (inst->uiState.diskOpShown)
	{
		hideDiskOpScreen(inst);
		/* Restore scopes */
		inst->uiState.scopesShown = true;
	}
	else
	{
		showDiskOpScreen(inst, video, bmp);
	}

	inst->uiState.needsFullRedraw = true;
}

void drawDiskOpScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	/* Check for error flags and show appropriate dialogs */
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (inst->diskop.pathSetFailed)
	{
		inst->diskop.pathSetFailed = false;
		if (ui != NULL)
			ft2_dialog_show_message(&ui->dialog, "System message", "Couldn't set directory path!");
	}
	if (inst->diskop.makeDirFailed)
	{
		inst->diskop.makeDirFailed = false;
		if (ui != NULL)
			ft2_dialog_show_message(&ui->dialog, "System message", 
				"Couldn't create directory: Access denied, or a dir with the same name already exists!");
	}

	/* Draw frameworks - matching standalone exactly */
	drawFramework(video, 0,     0,  67,  86, FRAMEWORK_TYPE1);
	drawFramework(video, 67,    0,  64, 142, FRAMEWORK_TYPE1);
	drawFramework(video, 131,   0,  37, 142, FRAMEWORK_TYPE1);
	drawFramework(video, 0,    86,  67,  56, FRAMEWORK_TYPE1);
	drawFramework(video, 0,   142, 168,  31, FRAMEWORK_TYPE1);
	drawFramework(video, 168,   0, 164,   3, FRAMEWORK_TYPE1);
	drawFramework(video, 168, 170, 164,   3, FRAMEWORK_TYPE1);
	drawFramework(video, 332,   0,  24, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 30,  157, 136,  14, FRAMEWORK_TYPE2);

	/* Clear file list area */
	clearRect(video, 168, 2, 164, 168);

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Show buttons - matching standalone order (Delete and Rename removed for plugin) */
	showPushButton(widgets, video, bmp, PB_DISKOP_SAVE);
	showPushButton(widgets, video, bmp, PB_DISKOP_MAKEDIR);
	showPushButton(widgets, video, bmp, PB_DISKOP_REFRESH);
	showPushButton(widgets, video, bmp, PB_DISKOP_EXIT);
	showPushButton(widgets, video, bmp, PB_DISKOP_PARENT);
	showPushButton(widgets, video, bmp, PB_DISKOP_ROOT);
	showPushButton(widgets, video, bmp, PB_DISKOP_HOME);
#ifdef _WIN32
	/* Show and update drive buttons based on enumerated drives */
	{
		const int driveButtons[FT2_DISKOP_MAX_DRIVES] = {
			PB_DISKOP_DRIVE1, PB_DISKOP_DRIVE2, PB_DISKOP_DRIVE3, PB_DISKOP_DRIVE4,
			PB_DISKOP_DRIVE5, PB_DISKOP_DRIVE6, PB_DISKOP_DRIVE7
		};
		for (int i = 0; i < FT2_DISKOP_MAX_DRIVES; i++)
		{
			if (i < inst->diskop.numDrives && inst->diskop.driveNames[i][0] != '\0')
			{
				/* Set button caption to drive name */
				widgets->pushButtons[driveButtons[i]].caption = inst->diskop.driveNames[i];
				showPushButton(widgets, video, bmp, driveButtons[i]);
			}
			else
			{
				hidePushButton(widgets, driveButtons[i]);
			}
		}
	}
#endif
	showPushButton(widgets, video, bmp, PB_DISKOP_SHOW_ALL);
	showPushButton(widgets, video, bmp, PB_DISKOP_SET_PATH);
	showPushButton(widgets, video, bmp, PB_DISKOP_LIST_UP);
	showPushButton(widgets, video, bmp, PB_DISKOP_LIST_DOWN);

	/* Show scrollbar */
	showScrollBar(widgets, video, SB_DISKOP_LIST);

	/* Show filename textbox */
	ft2_textbox_show(TB_DISKOP_FILENAME);

	/* Show item type radio buttons */
	if (inst->diskop.itemType > 4)
		inst->diskop.itemType = 0;
	uncheckRadioButtonGroup(widgets, RB_GROUP_DISKOP_ITEM);
	widgets->radioButtonState[RB_DISKOP_MODULE + inst->diskop.itemType] = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_DISKOP_ITEM);

	/* Draw labels */
	textOutShadow(video, bmp, 5,   3, PAL_FORGRND, PAL_DSKTOP2, "Item:");
	textOutShadow(video, bmp, 19, 17, PAL_FORGRND, PAL_DSKTOP2, "Module");
	textOutShadow(video, bmp, 19, 31, PAL_FORGRND, PAL_DSKTOP2, "Instr.");
	textOutShadow(video, bmp, 19, 45, PAL_FORGRND, PAL_DSKTOP2, "Sample");
	textOutShadow(video, bmp, 19, 59, PAL_FORGRND, PAL_DSKTOP2, "Pattern");
	textOutShadow(video, bmp, 19, 73, PAL_FORGRND, PAL_DSKTOP2, "Track");

	textOutShadow(video, bmp, 5,  89, PAL_FORGRND, PAL_DSKTOP2, "Save as:");
	drawSaveAsElements(inst, video, bmp);
	setDiskOpItemRadioButtons(inst, video, bmp);

	textOutShadow(video, bmp, 4, 159, PAL_FORGRND, PAL_DSKTOP2, "File:");

	/* Display current path */
	displayCurrPath(inst, video, bmp);

	/* Draw file list */
	diskOpDrawFilelist(inst, video, bmp);

	/* Update scrollbar */
	setScrollBarEnd(inst, widgets, video, SB_DISKOP_LIST, inst->diskop.fileCount);
	setScrollBarPos(inst, widgets, video, SB_DISKOP_LIST, inst->diskop.dirPos, false);

	/* Draw filename textbox */
	ft2_textbox_draw(video, bmp, TB_DISKOP_FILENAME, inst);
}

/* ============ FILE LIST DISPLAY ============ */

void diskOpDrawFilelist(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	char nameBuf[FT2_PATH_MAX];

	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	/* Clear file list area */
	clearRect(video, FILENAME_TEXT_X - 1, DISKOP_LIST_Y, 162, DISKOP_LIST_H);

	if (inst->diskop.fileCount == 0)
		return;

	/* Draw selected file highlight */
	if (inst->diskop.selectedEntry >= 0 && inst->diskop.selectedEntry < FT2_DISKOP_ENTRY_NUM)
	{
		uint16_t y = DISKOP_LIST_Y + (uint16_t)((FONT1_CHAR_H + 1) * inst->diskop.selectedEntry);
		fillRect(video, FILENAME_TEXT_X - 1, y, 162, FONT1_CHAR_H, PAL_PATTEXT);
	}

	/* Draw each visible entry */
	for (uint16_t i = 0; i < FT2_DISKOP_ENTRY_NUM; i++)
	{
		int32_t bufEntry = inst->diskop.dirPos + i;
		if (bufEntry >= inst->diskop.fileCount)
			break;

		if (inst->diskop.entries == NULL)
			break;

		ft2_diskop_entry_t *entry = &inst->diskop.entries[bufEntry];
		if (entry->name[0] == '\0')
			continue;

		uint16_t y = DISKOP_LIST_Y + (i * (FONT1_CHAR_H + 1));

		/* Copy name to temp buffer and truncate to fit */
		strncpy(nameBuf, entry->name, sizeof(nameBuf) - 1);
		nameBuf[sizeof(nameBuf) - 1] = '\0';
		trimEntryName(nameBuf, entry->isDir);

		if (entry->isDir)
		{
			/* Directory - prefix with slash */
			charOut(video, bmp, FILENAME_TEXT_X, y, PAL_BLCKTXT, '/');
			textOut(video, bmp, FILENAME_TEXT_X + FONT1_CHAR_W, y, PAL_BLCKTXT, nameBuf);
		}
		else
		{
			/* File */
			textOut(video, bmp, FILENAME_TEXT_X, y, PAL_BLCKTXT, nameBuf);

			/* Draw file size (right-aligned like standalone) */
			if (entry->filesize == -1)
			{
				textOut(video, bmp, FILESIZE_TEXT_X + 6, y, PAL_BLCKTXT, ">2GB");
			}
			else if (entry->filesize > 0)
			{
				char sizeBuf[16];
				int32_t printFilesize;
				int sizeX = FILESIZE_TEXT_X;
				
				if (entry->filesize >= 1024 * 1024 * 10)  /* >= 10MB */
				{
					printFilesize = (entry->filesize + 1024 * 1024 - 1) / (1024 * 1024);
					sizeX += (4 - numDigits(printFilesize)) * (FONT1_CHAR_W - 1);
					snprintf(sizeBuf, sizeof(sizeBuf), "%dM", printFilesize);
				}
				else if (entry->filesize >= 1024 * 10)  /* >= 10kB */
				{
					printFilesize = (entry->filesize + 1024 - 1) / 1024;
					if (printFilesize > 9999)
					{
						printFilesize = (entry->filesize + 1024 * 1024 - 1) / (1024 * 1024);
						sizeX += (4 - numDigits(printFilesize)) * (FONT1_CHAR_W - 1);
						snprintf(sizeBuf, sizeof(sizeBuf), "%dM", printFilesize);
					}
				else
					{
						sizeX += (4 - numDigits(printFilesize)) * (FONT1_CHAR_W - 1);
						snprintf(sizeBuf, sizeof(sizeBuf), "%dk", printFilesize);
					}
				}
				else  /* bytes */
				{
					printFilesize = entry->filesize;
					sizeX += (5 - numDigits(printFilesize)) * (FONT1_CHAR_W - 1);
					snprintf(sizeBuf, sizeof(sizeBuf), "%d", printFilesize);
				}
				textOut(video, bmp, sizeX, y, PAL_BLCKTXT, sizeBuf);
			}
		}
	}
}

void diskOpDrawDirectory(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL || bmp == NULL)
		return;

	displayCurrPath(inst, video, bmp);

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
	{
		setScrollBarEnd(inst, widgets, video, SB_DISKOP_LIST, inst->diskop.fileCount);
		setScrollBarPos(inst, widgets, video, SB_DISKOP_LIST, inst->diskop.dirPos, false);
	}

	diskOpDrawFilelist(inst, video, bmp);
}

/* ============ CALLBACKS ============ */

void pbDiskOpParent(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->diskop.requestGoParent = true;
	inst->uiState.needsFullRedraw = true;
}

void pbDiskOpRoot(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->diskop.requestGoRoot = true;
	inst->uiState.needsFullRedraw = true;
}

void pbDiskOpHome(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->diskop.requestGoHome = true;
	inst->uiState.needsFullRedraw = true;
}

#ifdef _WIN32
void pbDiskOpDrive1(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 0; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive2(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 1; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive3(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 2; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive4(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 3; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive5(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 4; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive6(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 5; inst->uiState.needsFullRedraw = true; } }
void pbDiskOpDrive7(ft2_instance_t *inst) { if (inst) { inst->diskop.requestDriveIndex = 6; inst->uiState.needsFullRedraw = true; } }
#endif

void pbDiskOpRefresh(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->diskop.requestReadDir = true;
	inst->uiState.needsFullRedraw = true;
}

void pbDiskOpShowAll(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->diskop.showAllFiles = !inst->diskop.showAllFiles;
	inst->diskop.requestReadDir = true;
	inst->uiState.needsFullRedraw = true;
}

/* Callback for set path input dialog */
static void onSetPathCallback(ft2_instance_t *inst, ft2_dialog_result_t result,
                              const char *inputText, void *userData)
{
	(void)userData;

	if (result == DIALOG_RESULT_OK && inputText != NULL && inputText[0] != '\0')
	{
		/* Store the path and request validation/change (JUCE will verify it exists) */
		strncpy(inst->diskop.newPath, inputText, FT2_PATH_MAX - 1);
		inst->diskop.newPath[FT2_PATH_MAX - 1] = '\0';
		inst->diskop.requestSetPath = true;
		inst->uiState.needsFullRedraw = true;
	}
}

void pbDiskOpSetPath(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		/* Single text like standalone: "Enter new directory path:" */
		ft2_dialog_show_input_cb(&ui->dialog,
			"Enter new directory path:", "",
			"", 255, inst, onSetPathCallback, NULL);
	}
}

void pbDiskOpExit(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideDiskOpScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void pbDiskOpSave(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Request save via JUCE */
	inst->diskop.requestSave = true;
}

void pbDiskOpDelete(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Request delete of selected entry */
	if (inst->diskop.selectedEntry >= 0)
		inst->diskop.requestDelete = true;
}

void pbDiskOpRename(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Request rename of selected entry */
	if (inst->diskop.selectedEntry >= 0)
		inst->diskop.requestRename = true;
}

/* Callback for make directory input dialog */
static void onMakeDirCallback(ft2_instance_t *inst, ft2_dialog_result_t result,
                              const char *inputText, void *userData)
{
	(void)userData;

	if (result == DIALOG_RESULT_OK && inputText != NULL && inputText[0] != '\0')
	{
		strncpy(inst->diskop.newDirName, inputText, sizeof(inst->diskop.newDirName) - 1);
		inst->diskop.newDirName[sizeof(inst->diskop.newDirName) - 1] = '\0';
		inst->diskop.requestMakeDir = true;
		inst->uiState.needsFullRedraw = true;
	}
}

void pbDiskOpMakeDir(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		/* Single text like standalone: "Enter directory name:" */
		ft2_dialog_show_input_cb(&ui->dialog,
			"Enter directory name:", "",
			"", 64, inst, onMakeDirCallback, NULL);
	}
}

void pbDiskOpListUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	if (inst->diskop.dirPos > 0)
	{
		inst->diskop.dirPos--;
		/* Scrollbar will be updated on redraw */
		inst->uiState.needsFullRedraw = true;
	}
}

void pbDiskOpListDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	if (inst->diskop.dirPos < inst->diskop.fileCount - FT2_DISKOP_ENTRY_NUM)
	{
		inst->diskop.dirPos++;
		/* Scrollbar will be updated on redraw */
		inst->uiState.needsFullRedraw = true;
	}
}

void sbDiskOpSetPos(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;
	inst->diskop.dirPos = (int32_t)pos;
	inst->uiState.needsFullRedraw = true;
}

/* Item type selection */
static void setDiskOpItem(ft2_instance_t *inst, uint8_t item)
{
	if (inst == NULL || item > 4)
		return;

	inst->diskop.itemType = item;

	/* Switch to saved path for this item type */
	char *sourcePath = NULL;
	switch (item)
	{
		case FT2_DISKOP_ITEM_MODULE:  sourcePath = inst->diskop.modulePath; break;
		case FT2_DISKOP_ITEM_INSTR:   sourcePath = inst->diskop.instrPath; break;
		case FT2_DISKOP_ITEM_SAMPLE:  sourcePath = inst->diskop.samplePath; break;
		case FT2_DISKOP_ITEM_PATTERN: sourcePath = inst->diskop.patternPath; break;
		case FT2_DISKOP_ITEM_TRACK:   sourcePath = inst->diskop.trackPath; break;
		default: return;
	}

	if (sourcePath[0] != '\0')
	{
		strncpy(inst->diskop.currentPath, sourcePath, FT2_PATH_MAX - 1);
		inst->diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
	}

	/* Request directory refresh via JUCE */
	inst->diskop.requestReadDir = true;
	inst->uiState.needsFullRedraw = true;
}

void rbDiskOpModule(ft2_instance_t *inst)  { setDiskOpItem(inst, FT2_DISKOP_ITEM_MODULE); }
void rbDiskOpInstr(ft2_instance_t *inst)   { setDiskOpItem(inst, FT2_DISKOP_ITEM_INSTR); }
void rbDiskOpSample(ft2_instance_t *inst)  { setDiskOpItem(inst, FT2_DISKOP_ITEM_SAMPLE); }
void rbDiskOpPattern(ft2_instance_t *inst) { setDiskOpItem(inst, FT2_DISKOP_ITEM_PATTERN); }
void rbDiskOpTrack(ft2_instance_t *inst)   { setDiskOpItem(inst, FT2_DISKOP_ITEM_TRACK); }

/* Save format selection */
void rbDiskOpModSaveMod(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_MODULE] = FT2_MOD_SAVE_MOD;
}

void rbDiskOpModSaveXm(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_MODULE] = FT2_MOD_SAVE_XM;
}

void rbDiskOpModSaveWav(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_MODULE] = FT2_MOD_SAVE_WAV;
}

void rbDiskOpSmpSaveRaw(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE] = FT2_SMP_SAVE_RAW;
}

void rbDiskOpSmpSaveIff(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE] = FT2_SMP_SAVE_IFF;
}

void rbDiskOpSmpSaveWav(ft2_instance_t *inst)
{
	if (inst) inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE] = FT2_SMP_SAVE_WAV;
}

/* Directory reading - stub, implemented via JUCE callback */
void diskOpReadDirectory(ft2_instance_t *inst)
{
	(void)inst;
	/* This will be implemented via JUCE callback to populate inst->diskop.entries */
}

/* Mouse handling for file list */
bool diskOpTestMouseDown(ft2_instance_t *inst, int32_t mouseX, int32_t mouseY)
{
	if (inst == NULL || !inst->uiState.diskOpShown)
		return false;

	/* Check if click is in file list area */
	if (mouseX >= FILENAME_TEXT_X - 1 && mouseX < FILENAME_TEXT_X + 162 &&
	    mouseY >= DISKOP_LIST_Y && mouseY < DISKOP_LIST_Y + DISKOP_LIST_H)
	{
		int32_t entryIndex = (mouseY - DISKOP_LIST_Y) / (FONT1_CHAR_H + 1);
		if (entryIndex >= 0 && entryIndex < FT2_DISKOP_ENTRY_NUM)
		{
			int32_t absIndex = inst->diskop.dirPos + entryIndex;
			if (absIndex < inst->diskop.fileCount)
			{
				inst->diskop.selectedEntry = entryIndex;
				diskOpHandleItemClick(inst, absIndex);
				return true;
			}
		}
	}

	return false;
}

/* Track last click for double-click detection */
static int32_t lastClickedEntry = -1;
static uint32_t lastClickTime = 0;

/* Callback for unsaved changes confirmation when loading a module */
static void unsavedChangesLoadCallback(ft2_instance_t *inst, ft2_dialog_result_t result,
                                       const char *inputText, void *userData)
{
	(void)inputText;
	int32_t entryIndex = (int32_t)(intptr_t)userData;

	if (result == DIALOG_RESULT_YES)
	{
		inst->diskop.requestLoadEntry = entryIndex;
		inst->uiState.needsFullRedraw = true;
	}
}

void diskOpHandleItemClick(ft2_instance_t *inst, int32_t entryIndex)
{
	if (inst == NULL || inst->diskop.entries == NULL)
		return;

	if (entryIndex < 0 || entryIndex >= inst->diskop.fileCount)
		return;

	ft2_diskop_entry_t *entry = &inst->diskop.entries[entryIndex];

	/* Check for double-click (within ~500ms) */
	uint32_t currentTime = inst->editor.framesPassed;
	bool isDoubleClick = (entryIndex == lastClickedEntry && 
	                      (currentTime - lastClickTime) < 30); /* ~500ms at 60fps */

	lastClickedEntry = entryIndex;
	lastClickTime = currentTime;

	if (entry->isDir)
	{
		/* Directory - navigate on single click */
		inst->diskop.requestOpenEntry = entryIndex;
	}
	else
	{
		/* File - set filename on single click, load on double click */
		strncpy(inst->diskop.filename, entry->name, FT2_PATH_MAX - 1);
		inst->diskop.filename[FT2_PATH_MAX - 1] = '\0';

		if (isDoubleClick)
		{
			/* Double-click - check for unsaved changes when loading modules */
			if (inst->diskop.itemType == FT2_DISKOP_ITEM_MODULE && inst->replayer.song.isModified)
			{
				/* Show confirmation dialog */
				ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
				if (ui != NULL)
				{
					ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
						"You have unsaved changes in your song. Load new song and lose ALL changes?",
						inst, unsavedChangesLoadCallback, (void *)(intptr_t)entryIndex);
				}
			}
			else
			{
				/* No unsaved changes or not a module - load directly */
			inst->diskop.requestLoadEntry = entryIndex;
			}
		}
	}

	inst->uiState.needsFullRedraw = true;
}

/* Free disk op resources */
void freeDiskOp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->diskop.entries != NULL)
	{
		free(inst->diskop.entries);
		inst->diskop.entries = NULL;
	}

	inst->diskop.fileCount = 0;
}

/* ============ FILE FORMAT DETECTION ============ */

ft2_file_format ft2_detect_format_by_ext(const char *filename)
{
	if (filename == NULL)
		return FT2_FORMAT_UNKNOWN;

	/* Find last dot */
	const char *ext = NULL;
	const char *p = filename;
	while (*p)
	{
		if (*p == '.')
			ext = p + 1;
		p++;
	}

	if (ext == NULL)
		return FT2_FORMAT_UNKNOWN;

	/* Compare extension (case-insensitive) */
	if ((ext[0] == 'x' || ext[0] == 'X') && (ext[1] == 'm' || ext[1] == 'M') && ext[2] == '\0')
		return FT2_FORMAT_XM;

	if ((ext[0] == 'm' || ext[0] == 'M') && (ext[1] == 'o' || ext[1] == 'O') && 
	    (ext[2] == 'd' || ext[2] == 'D') && ext[3] == '\0')
		return FT2_FORMAT_MOD;

	if ((ext[0] == 's' || ext[0] == 'S') && (ext[1] == '3' || ext[1] == '3') && 
	    (ext[2] == 'm' || ext[2] == 'M') && ext[3] == '\0')
		return FT2_FORMAT_S3M;

	if ((ext[0] == 'x' || ext[0] == 'X') && (ext[1] == 'i' || ext[1] == 'I') && ext[2] == '\0')
		return FT2_FORMAT_XI;

	if ((ext[0] == 'w' || ext[0] == 'W') && (ext[1] == 'a' || ext[1] == 'A') && 
	    (ext[2] == 'v' || ext[2] == 'V') && ext[3] == '\0')
		return FT2_FORMAT_WAV;

	if ((ext[0] == 'a' || ext[0] == 'A') && (ext[1] == 'i' || ext[1] == 'I') &&
	    (ext[2] == 'f' || ext[2] == 'F') && (ext[3] == 'f' || ext[3] == 'F' || ext[3] == '\0'))
		return FT2_FORMAT_AIFF;

	if ((ext[0] == 'r' || ext[0] == 'R') && (ext[1] == 'a' || ext[1] == 'A') &&
	    (ext[2] == 'w' || ext[2] == 'W') && ext[3] == '\0')
		return FT2_FORMAT_RAW;

	if ((ext[0] == 'p' || ext[0] == 'P') && (ext[1] == 'a' || ext[1] == 'A') &&
	    (ext[2] == 't' || ext[2] == 'T') && ext[3] == '\0')
		return FT2_FORMAT_PAT;

	return FT2_FORMAT_UNKNOWN;
}

ft2_file_format ft2_detect_format_by_header(const uint8_t *data, uint32_t dataSize)
{
	if (data == NULL || dataSize < 4)
		return FT2_FORMAT_UNKNOWN;

	/* XM signature */
	if (dataSize >= 17 && memcmp(data, "Extended Module: ", 17) == 0)
		return FT2_FORMAT_XM;

	/* XI signature */
	if (dataSize >= 21 && memcmp(data, "Extended Instrument: ", 21) == 0)
		return FT2_FORMAT_XI;

	/* WAV signature */
	if (dataSize >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0)
		return FT2_FORMAT_WAV;

	/* AIFF signature */
	if (dataSize >= 12 && memcmp(data, "FORM", 4) == 0 && 
	    (memcmp(data + 8, "AIFF", 4) == 0 || memcmp(data + 8, "AIFC", 4) == 0))
		return FT2_FORMAT_AIFF;

	/* S3M signature ("SCRM" at offset 0x2C) */
	if (dataSize >= 48 && memcmp(data + 0x2C, "SCRM", 4) == 0 && data[0x1D] == 16)
		return FT2_FORMAT_S3M;

	/* MOD signatures (M.K., M!K!, FLT4, FLT8, 4CHN, 6CHN, 8CHN, etc.) */
	if (dataSize >= 1084)
	{
		const uint8_t *sig = data + 1080;
		if (memcmp(sig, "M.K.", 4) == 0 || memcmp(sig, "M!K!", 4) == 0 ||
		    memcmp(sig, "FLT4", 4) == 0 || memcmp(sig, "FLT8", 4) == 0 ||
		    memcmp(sig, "4CHN", 4) == 0 || memcmp(sig, "6CHN", 4) == 0 ||
		    memcmp(sig, "8CHN", 4) == 0)
			return FT2_FORMAT_MOD;
	}

	return FT2_FORMAT_UNKNOWN;
}

/* ft2_load_module moved to ft2_plugin_loader.c - use ft2_load_module() from ft2_plugin_loader.h */

/**
 * Helper: Count used samples in an instrument (ported from getUsedSamples).
 */
static int16_t countUsedSamples(ft2_instr_t *ins)
{
	if (ins == NULL)
		return 0;

	int16_t i = 16 - 1;
	while (i >= 0 && ins->smp[i].dataPtr == NULL && ins->smp[i].name[0] == '\0')
		i--;

	/* Check note2SampleLUT for higher sample indices */
	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->note2SampleLUT[j] > i)
			i = ins->note2SampleLUT[j];
	}

	return i + 1;
}

/* XM instrument header size */
#define XM_INSTR_HEADER_SIZE 263

/* XM pattern header structure */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct xm_patt_hdr_t
{
	int32_t headerSize;
	uint8_t type;
	int16_t numRows;
	uint16_t dataSize;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_patt_hdr_t;

/* XM sample header structure (40 bytes) */
typedef struct xm_smp_hdr_t
{
	uint32_t length, loopStart, loopLength;
	uint8_t volume;
	int8_t finetune;
	uint8_t flags, panning;
	int8_t relativeNote;
	uint8_t nameLength;
	char name[22];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_smp_hdr_t;

/* XM instrument header structure (263 bytes + sample headers) */
typedef struct xm_ins_hdr_t
{
	uint32_t instrSize;
	char name[22];
	uint8_t type;
	int16_t numSamples;
	int32_t sampleSize;
	uint8_t note2SampleLUT[96];
	int16_t volEnvPoints[12][2];
	int16_t panEnvPoints[12][2];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t vibType, vibSweep, vibDepth, vibRate;
	uint16_t fadeout;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	int8_t mute;
	uint8_t reserved[15];
	xm_smp_hdr_t smp[16];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xm_ins_hdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/**
 * Convert sample data to delta encoding for XM/XI file saving.
 * Ported from standalone samp2Delta().
 */
static void sample_to_delta(int8_t *p, int32_t length, uint8_t smpFlags)
{
	if (smpFlags & FT2_SAMPLE_16BIT)
	{
		int16_t *p16 = (int16_t *)p;
		int16_t newS16 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int16_t oldS16 = p16[i];
			p16[i] -= newS16;
			newS16 = oldS16;
		}
	}
	else
	{
		int8_t newS8 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int8_t oldS8 = p[i];
			p[i] -= newS8;
			newS8 = oldS8;
		}
	}
}

/**
 * Convert delta-encoded sample data back to normal.
 * Ported from standalone delta2Samp() - mono-only version for save/restore.
 */
static void delta_to_sample(int8_t *p, int32_t length, uint8_t smpFlags)
{
	if (smpFlags & FT2_SAMPLE_16BIT)
	{
		int16_t *p16 = (int16_t *)p;
		int16_t olds16 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int16_t news16 = p16[i] + olds16;
			p16[i] = news16;
			olds16 = news16;
		}
	}
	else
	{
		int8_t olds8 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int8_t news8 = p[i] + olds8;
			p[i] = news8;
			olds8 = news8;
		}
	}
}

/**
 * Pack pattern data for XM file saving.
 * Ported from standalone packPatt().
 * @param writePtr Output buffer (must be large enough for packed data)
 * @param pattPtr Pattern data (32 channels * 5 bytes per note)
 * @param numRows Number of rows in pattern
 * @param numChannels Number of channels to pack
 * @return Number of bytes written
 */
static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows, uint16_t numChannels)
{
	uint8_t bytes[5];

	if (pattPtr == NULL)
		return 0;

	uint16_t totalPackLen = 0;

	/* Pitch = bytes to skip per row for unused channels (32 - numChannels) */
	const int32_t pitch = 5 * (FT2_MAX_CHANNELS - numChannels);
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < numChannels; chn++)
		{
			bytes[0] = *pattPtr++;  /* note */
			bytes[1] = *pattPtr++;  /* instrument */
			bytes[2] = *pattPtr++;  /* volume column */
			bytes[3] = *pattPtr++;  /* effect */
			bytes[4] = *pattPtr++;  /* effect parameter */

			uint8_t *firstBytePtr = writePtr++;

			uint8_t packBits = 0;
			if (bytes[0] > 0) { packBits |= 1; *writePtr++ = bytes[0]; }
			if (bytes[1] > 0) { packBits |= 2; *writePtr++ = bytes[1]; }
			if (bytes[2] > 0) { packBits |= 4; *writePtr++ = bytes[2]; }
			if (bytes[3] > 0) { packBits |= 8; *writePtr++ = bytes[3]; }

			if (packBits == 15)
			{
				/* All 4 fields set - no packing needed, write as-is */
				writePtr = firstBytePtr;

				*writePtr++ = bytes[0];
				*writePtr++ = bytes[1];
				*writePtr++ = bytes[2];
				*writePtr++ = bytes[3];
				*writePtr++ = bytes[4];

				totalPackLen += 5;
				continue;
			}

			if (bytes[4] > 0) { packBits |= 16; *writePtr++ = bytes[4]; }

			*firstBytePtr = packBits | 128;  /* Write pack bits byte with high bit set */
			totalPackLen += (uint16_t)(writePtr - firstBytePtr);
		}

		/* Skip unused channels (unpacked patterns always have 32 channels) */
		pattPtr += pitch;
	}

	return totalPackLen;
}

bool ft2_save_module(ft2_instance_t *inst, uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	ft2_replayer_state_t *rep = &inst->replayer;

	/* Count patterns - same as standalone: find last non-empty pattern */
	int32_t numPatterns = FT2_MAX_PATTERNS;
	do
	{
		if (patternEmpty(inst, numPatterns - 1))
			numPatterns--;
		else
			break;
	}
	while (numPatterns > 0);

	/* Count instruments - same as standalone: find last with samples or name */
	int32_t numInstruments = FT2_MAX_INST;
	while (numInstruments > 0 &&
	       countUsedSamples(rep->instr[numInstruments]) == 0 &&
	       rep->song.instrName[numInstruments][0] == '\0')
	{
		numInstruments--;
	}

	/* Allocate temporary buffer for packed pattern data */
	uint8_t *packedPattData = (uint8_t *)malloc(65536);
	if (packedPattData == NULL)
		return false;

	/* Calculate required size (estimate high, then shrink) */
	uint32_t headerSize = 336;  /* XM header (60 + 276) */
	uint32_t patternSize = 0;
	uint32_t instrumentSize = 0;

	/* Pattern sizes: 9-byte header each + worst case packed data */
	for (int32_t i = 0; i < numPatterns; i++)
	{
		int16_t numRows = rep->patternNumRows[i];
		if (numRows < 1) numRows = 64;
		patternSize += 9 + numRows * rep->song.numChannels * 5;
	}

	/* Instrument sizes: header + sample headers + sample data */
	for (int32_t i = 1; i <= numInstruments; i++)
	{
		ft2_instr_t *instr = rep->instr[i];
		int16_t numSamples = (instr != NULL) ? countUsedSamples(instr) : 0;

		if (numSamples > 0)
		{
			instrumentSize += XM_INSTR_HEADER_SIZE + (numSamples * sizeof(xm_smp_hdr_t));
			for (int32_t s = 0; s < numSamples; s++)
			{
				ft2_sample_t *smp = &instr->smp[s];
				if (smp->dataPtr != NULL && smp->length > 0)
				{
					int32_t smpBytes = smp->length;
					if (smp->flags & FT2_SAMPLE_16BIT)
						smpBytes *= 2;
					instrumentSize += smpBytes;
				}
			}
		}
		else
		{
			instrumentSize += 22 + 11;  /* Empty instrument: name (22) + minimal header (11) */
		}
	}

	uint32_t totalSize = headerSize + patternSize + instrumentSize;

	/* Allocate buffer */
	*outData = (uint8_t *)malloc(totalSize);
	if (*outData == NULL)
	{
		free(packedPattData);
		return false;
	}

	memset(*outData, 0, totalSize);
	uint8_t *p = *outData;

	/* ===== XM HEADER (60 bytes) ===== */
	memcpy(p, "Extended Module: ", 17);
	p += 17;

	/* Module name (20 chars, space-padded like FT2) */
	int32_t nameLen = (int32_t)strlen(rep->song.name);
	if (nameLen > 20) nameLen = 20;
	memset(p, ' ', 20);
	if (nameLen > 0) memcpy(p, rep->song.name, nameLen);
	p += 20;

	*p++ = 0x1A;

	/* Tracker name (20 chars) */
	memcpy(p, "FT2Clone Plugin     ", 20);
	p += 20;

	/* Version 1.04 (little-endian) */
	*p++ = 0x04;
	*p++ = 0x01;

	/* ===== XM HEADER DATA (276 bytes) ===== */
	uint32_t hdrSize = 20 + 256;  /* 276 bytes after this field */
	memcpy(p, &hdrSize, 4);
	p += 4;

	uint16_t songLen = rep->song.songLength;
	memcpy(p, &songLen, 2);
	p += 2;

	uint16_t restartPos = rep->song.songLoopStart;
	memcpy(p, &restartPos, 2);
	p += 2;

	uint16_t numCh = rep->song.numChannels;
	memcpy(p, &numCh, 2);
	p += 2;

	memcpy(p, &numPatterns, 2);
	p += 2;

	memcpy(p, &numInstruments, 2);
	p += 2;

	uint16_t flags = inst->audio.linearPeriodsFlag ? 1 : 0;
	memcpy(p, &flags, 2);
	p += 2;

	uint16_t tempo = rep->song.speed;
	memcpy(p, &tempo, 2);
	p += 2;

	uint16_t bpm = rep->song.BPM;
	memcpy(p, &bpm, 2);
	p += 2;

	memcpy(p, rep->song.orders, 256);
	p += 256;

	/* ===== PATTERNS ===== */
	for (int32_t i = 0; i < numPatterns; i++)
	{
		/* Free empty patterns and reset to 64 rows (matches standalone) */
		if (patternEmpty(inst, i))
		{
			if (rep->pattern[i] != NULL)
			{
				free(rep->pattern[i]);
				rep->pattern[i] = NULL;
			}
			rep->patternNumRows[i] = 64;
		}

		xm_patt_hdr_t ph;
		memset(&ph, 0, sizeof(ph));
		ph.headerSize = sizeof(xm_patt_hdr_t);
		ph.type = 0;
		ph.numRows = rep->patternNumRows[i];

		if (rep->pattern[i] == NULL)
		{
			ph.dataSize = 0;
			memcpy(p, &ph, sizeof(ph));
			p += sizeof(ph);
		}
		else
		{
			ph.dataSize = packPatt(packedPattData, (uint8_t *)rep->pattern[i],
			                        ph.numRows, rep->song.numChannels);

			memcpy(p, &ph, sizeof(ph));
			p += sizeof(ph);

			memcpy(p, packedPattData, ph.dataSize);
			p += ph.dataSize;
		}
	}

	free(packedPattData);

	/* ===== INSTRUMENTS ===== */
	for (int32_t i = 1; i <= numInstruments; i++)
	{
		ft2_instr_t *instr = rep->instr[i];
		int16_t numSamples = (instr != NULL) ? countUsedSamples(instr) : 0;

		xm_ins_hdr_t ih;
		memset(&ih, 0, sizeof(ih));

		/* Instrument name */
		nameLen = (int32_t)strlen(rep->song.instrName[i]);
		if (nameLen > 22) nameLen = 22;
		memset(ih.name, 0, 22);
		if (nameLen > 0) memcpy(ih.name, rep->song.instrName[i], nameLen);

		ih.type = 0;
		ih.numSamples = numSamples;
		ih.sampleSize = sizeof(xm_smp_hdr_t);

		if (numSamples > 0)
		{
			/* Copy instrument parameters */
			memcpy(ih.note2SampleLUT, instr->note2SampleLUT, 96);
			memcpy(ih.volEnvPoints, instr->volEnvPoints, sizeof(ih.volEnvPoints));
			memcpy(ih.panEnvPoints, instr->panEnvPoints, sizeof(ih.panEnvPoints));
			ih.volEnvLength = instr->volEnvLength;
			ih.panEnvLength = instr->panEnvLength;
			ih.volEnvSustain = instr->volEnvSustain;
			ih.volEnvLoopStart = instr->volEnvLoopStart;
			ih.volEnvLoopEnd = instr->volEnvLoopEnd;
			ih.panEnvSustain = instr->panEnvSustain;
			ih.panEnvLoopStart = instr->panEnvLoopStart;
			ih.panEnvLoopEnd = instr->panEnvLoopEnd;
			ih.volEnvFlags = instr->volEnvFlags;
			ih.panEnvFlags = instr->panEnvFlags;
			ih.vibType = instr->autoVibType;
			ih.vibSweep = instr->autoVibSweep;
			ih.vibDepth = instr->autoVibDepth;
			ih.vibRate = instr->autoVibRate;
			ih.fadeout = instr->fadeout;
			ih.midiOn = instr->midiOn ? 1 : 0;
			ih.midiChannel = instr->midiChannel;
			ih.midiProgram = instr->midiProgram;
			ih.midiBend = instr->midiBend;
			ih.mute = instr->mute ? 1 : 0;
			ih.instrSize = XM_INSTR_HEADER_SIZE;

			/* Build sample headers */
			for (int32_t s = 0; s < numSamples; s++)
			{
				ft2_sample_t *smp = &instr->smp[s];
				xm_smp_hdr_t *dst = &ih.smp[s];

				bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);

				dst->length = smp->length;
				dst->loopStart = smp->loopStart;
				dst->loopLength = smp->loopLength;

				if (sample16Bit)
				{
					dst->length *= 2;
					dst->loopStart *= 2;
					dst->loopLength *= 2;
				}

				dst->volume = smp->volume;
				dst->finetune = smp->finetune;
				dst->flags = smp->flags;
				dst->panning = smp->panning;
				dst->relativeNote = smp->relativeNote;

				nameLen = (int32_t)strlen(smp->name);
				if (nameLen > 22) nameLen = 22;
				dst->nameLength = (uint8_t)nameLen;
				memset(dst->name, ' ', 22);
				if (nameLen > 0) memcpy(dst->name, smp->name, nameLen);

				if (smp->dataPtr == NULL)
					dst->length = 0;
			}

			/* Write instrument header + sample headers */
			memcpy(p, &ih, XM_INSTR_HEADER_SIZE + (numSamples * sizeof(xm_smp_hdr_t)));
			p += XM_INSTR_HEADER_SIZE + (numSamples * sizeof(xm_smp_hdr_t));

			/* Write sample data (delta-encoded) */
			for (int32_t s = 0; s < numSamples; s++)
			{
				ft2_sample_t *smp = &instr->smp[s];
				if (smp->dataPtr != NULL && smp->length > 0)
				{
					int32_t smpBytes = smp->length;
					if (smp->flags & FT2_SAMPLE_16BIT)
						smpBytes *= 2;

					/* Unfix, delta encode, copy, undo */
					ft2_unfix_sample(smp);
					sample_to_delta(smp->dataPtr, smp->length, smp->flags);

					memcpy(p, smp->dataPtr, smpBytes);
					p += smpBytes;

					/* Restore sample for playback */
					delta_to_sample(smp->dataPtr, smp->length, smp->flags);
					ft2_fix_sample(smp);
				}
			}
		}
		else
		{
			/* Empty instrument: minimal header */
			ih.instrSize = 22 + 11;
			memcpy(p, &ih, ih.instrSize);
			p += ih.instrSize;
		}
	}

	*outSize = (uint32_t)(p - *outData);
	return true;
}

/* XI header size (without sample headers) */
#define XI_HEADER_SIZE 298

/* XI header structure */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct xi_header_t
{
	char id[21];
	char name[23];       /* 22 chars + 0x1A terminator */
	char progName[20];
	uint16_t version;
	uint8_t note2SampleLUT[96];
	int16_t volEnvPoints[12][2];
	int16_t panEnvPoints[12][2];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t vibType, vibSweep, vibDepth, vibRate;
	uint16_t fadeout;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	uint8_t mute, reserved[15];
	int16_t numSamples;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xi_header_t;

/* XI sample header structure */
typedef struct xi_sample_header_t
{
	uint32_t length, loopStart, loopLength;
	uint8_t volume;
	int8_t finetune;
	uint8_t flags, panning;
	int8_t relativeNote;
	uint8_t nameLength;
	char name[22];
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xi_sample_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* Delta decoding for XI samples */
static void xi_delta_to_sample_8bit(int8_t *p, int32_t length)
{
	int8_t sample = 0;
	for (int32_t i = 0; i < length; i++)
	{
		sample += p[i];
		p[i] = sample;
	}
}

static void xi_delta_to_sample_16bit(int16_t *p, int32_t length)
{
	int16_t sample = 0;
	for (int32_t i = 0; i < length; i++)
	{
		sample += p[i];
		p[i] = sample;
	}
}

bool ft2_load_instrument(ft2_instance_t *inst, int16_t instrNum,
                          const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL || dataSize < XI_HEADER_SIZE)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	/* Check XI signature */
	if (memcmp(data, "Extended Instrument: ", 21) != 0)
		return false;

	/* Read header */
	xi_header_t hdr;
	memcpy(&hdr, data, XI_HEADER_SIZE);

	/* Check version */
	if (hdr.version != 0x0101 && hdr.version != 0x0102)
		return false;

	/* Handle old v1.01 format (adjust positions) */
	uint32_t readPos = XI_HEADER_SIZE;
	if (hdr.version == 0x0101)
	{
		/* v1.01 has different header layout */
		hdr.numSamples = hdr.midiProgram;
		hdr.midiProgram = 0;
		hdr.midiBend = 0;
		hdr.mute = 0;
		readPos -= 20;  /* Adjust for shorter header */
	}

	if (hdr.numSamples < 0 || hdr.numSamples > 16)
		return false;

	/* Free existing instrument */
	ft2_instance_free_instr(inst, instrNum);

	/* Copy instrument name */
	memcpy(inst->replayer.song.instrName[instrNum], hdr.name, 22);
	inst->replayer.song.instrName[instrNum][22] = '\0';

	if (hdr.numSamples == 0)
		return true;  /* Empty instrument is valid */

	/* Allocate instrument */
	if (!ft2_instance_alloc_instr(inst, instrNum))
		return false;

	ft2_instr_t *ins = inst->replayer.instr[instrNum];

	/* Copy instrument parameters */
	memcpy(ins->note2SampleLUT, hdr.note2SampleLUT, 96);
	memcpy(ins->volEnvPoints, hdr.volEnvPoints, sizeof(hdr.volEnvPoints));
	memcpy(ins->panEnvPoints, hdr.panEnvPoints, sizeof(hdr.panEnvPoints));
	ins->volEnvLength = hdr.volEnvLength;
	ins->panEnvLength = hdr.panEnvLength;
	ins->volEnvSustain = hdr.volEnvSustain;
	ins->volEnvLoopStart = hdr.volEnvLoopStart;
	ins->volEnvLoopEnd = hdr.volEnvLoopEnd;
	ins->panEnvSustain = hdr.panEnvSustain;
	ins->panEnvLoopStart = hdr.panEnvLoopStart;
	ins->panEnvLoopEnd = hdr.panEnvLoopEnd;
	ins->volEnvFlags = hdr.volEnvFlags;
	ins->panEnvFlags = hdr.panEnvFlags;
	ins->autoVibType = hdr.vibType;
	ins->autoVibSweep = hdr.vibSweep;
	ins->autoVibDepth = hdr.vibDepth;
	ins->autoVibRate = hdr.vibRate;
	ins->fadeout = hdr.fadeout;
	ins->midiOn = (hdr.midiOn == 1);
	ins->midiChannel = hdr.midiChannel;
	ins->mute = (hdr.mute == 1);
	ins->numSamples = hdr.numSamples;
	ft2_sanitize_instrument(ins);

	/* Read sample headers */
	int32_t numSamplesToRead = hdr.numSamples;
	if (numSamplesToRead > FT2_MAX_SMP_PER_INST)
		numSamplesToRead = FT2_MAX_SMP_PER_INST;

	xi_sample_header_t smpHeaders[16];
	memset(smpHeaders, 0, sizeof(smpHeaders));

	if (readPos + numSamplesToRead * sizeof(xi_sample_header_t) > dataSize)
	{
		ft2_instance_free_instr(inst, instrNum);
		return false;
	}

	memcpy(smpHeaders, data + readPos, numSamplesToRead * sizeof(xi_sample_header_t));
	readPos += hdr.numSamples * sizeof(xi_sample_header_t);

	/* Copy sample headers to sample structures */
	for (int32_t i = 0; i < numSamplesToRead; i++)
	{
		ft2_sample_t *smp = &ins->smp[i];
		xi_sample_header_t *src = &smpHeaders[i];

		smp->length = src->length;
		smp->loopStart = src->loopStart;
		smp->loopLength = src->loopLength;
		smp->volume = src->volume;
		smp->finetune = src->finetune;
		smp->flags = src->flags;
		smp->panning = src->panning;
		smp->relativeNote = src->relativeNote;
		memcpy(smp->name, src->name, 22);
		smp->name[22] = '\0';
	}

	/* Read sample data */
	for (int32_t i = 0; i < hdr.numSamples; i++)
	{
		ft2_sample_t *smp = (i < FT2_MAX_SMP_PER_INST) ? &ins->smp[i] : NULL;
		xi_sample_header_t *src = &smpHeaders[i < FT2_MAX_SMP_PER_INST ? i : 0];

		int32_t lengthInFile = src->length;
		if (lengthInFile <= 0)
			continue;

		if (smp == NULL)
		{
			/* Skip samples beyond our limit */
			readPos += lengthInFile;
			continue;
		}

		bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);
		bool stereoSample = !!(smp->flags & 32);  /* SAMPLE_STEREO */

		/* Convert byte lengths to sample lengths for 16-bit */
		if (sample16Bit)
		{
			smp->length /= 2;
			smp->loopStart /= 2;
			smp->loopLength /= 2;
		}

		/* Clamp length */
		if (smp->length > 0x3FFFFFFF)
			smp->length = 0x3FFFFFFF;

		/* Allocate sample data with padding for interpolation */
		int32_t allocSize = smp->length * (sample16Bit ? 2 : 1) + FT2_MAX_TAPS * 4;
		smp->origDataPtr = (int8_t *)malloc(allocSize);
		if (smp->origDataPtr == NULL)
		{
			ft2_instance_free_instr(inst, instrNum);
			return false;
		}

		memset(smp->origDataPtr, 0, allocSize);
		smp->dataPtr = smp->origDataPtr + FT2_MAX_TAPS * (sample16Bit ? 2 : 1);

		/* Read sample data */
		int32_t sampleLengthBytes = smp->length * (sample16Bit ? 2 : 1);
		if (readPos + sampleLengthBytes > dataSize)
		{
			/* Truncate if file is shorter than expected */
			sampleLengthBytes = dataSize - readPos;
			if (sample16Bit)
				smp->length = sampleLengthBytes / 2;
			else
				smp->length = sampleLengthBytes;
		}

		memcpy(smp->dataPtr, data + readPos, sampleLengthBytes);

		/* Skip any remaining data (if lengthInFile > what we read) */
		if (lengthInFile > sampleLengthBytes)
			readPos += lengthInFile;
		else
			readPos += sampleLengthBytes;

		/* Convert from delta encoding */
		if (sample16Bit)
			xi_delta_to_sample_16bit((int16_t *)smp->dataPtr, smp->length);
		else
			xi_delta_to_sample_8bit(smp->dataPtr, smp->length);

		/* Handle stereo samples (convert to mono) */
		if (stereoSample)
		{
			smp->flags &= ~32;  /* Remove stereo flag */
			smp->length /= 2;
			smp->loopStart /= 2;
			smp->loopLength /= 2;

			/* Mix stereo to mono in place */
			if (sample16Bit)
			{
				int16_t *p = (int16_t *)smp->dataPtr;
				for (int32_t j = 0; j < smp->length; j++)
					p[j] = (p[j * 2] + p[j * 2 + 1]) / 2;
			}
			else
			{
				int8_t *p = smp->dataPtr;
				for (int32_t j = 0; j < smp->length; j++)
					p[j] = (p[j * 2] + p[j * 2 + 1]) / 2;
			}
		}

		/* Sanitize and fix sample for interpolation */
		ft2_sanitize_sample(smp);
		ft2_fix_sample(smp);
	}

	return true;
}

bool ft2_save_instrument(ft2_instance_t *inst, int16_t instrNum,
                          uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return false;

	int16_t numSamples = countUsedSamples(instr);
	if (numSamples == 0)
		return false;  /* Empty instrument */

	/* Calculate required size */
	uint32_t totalSize = XI_HEADER_SIZE + (numSamples * sizeof(xi_sample_header_t));

	for (int32_t i = 0; i < numSamples; i++)
	{
		ft2_sample_t *smp = &instr->smp[i];
		if (smp->dataPtr != NULL && smp->length > 0)
		{
			int32_t smpBytes = smp->length;
			if (smp->flags & FT2_SAMPLE_16BIT)
				smpBytes *= 2;
			totalSize += smpBytes;
		}
	}

	/* Allocate buffer */
	*outData = (uint8_t *)malloc(totalSize);
	if (*outData == NULL)
		return false;

	memset(*outData, 0, totalSize);

	/* Build XI header */
	xi_header_t ih;
	memset(&ih, 0, sizeof(ih));

	memcpy(ih.id, "Extended Instrument: ", 21);
	ih.version = 0x0102;

	/* Instrument name (22 chars, space-padded, with 0x1A terminator at pos 22) */
	int32_t nameLen = (int32_t)strlen(inst->replayer.song.instrName[instrNum]);
	if (nameLen > 22) nameLen = 22;
	memset(ih.name, ' ', 22);
	if (nameLen > 0)
		memcpy(ih.name, inst->replayer.song.instrName[instrNum], nameLen);
	ih.name[22] = 0x1A;

	/* Tracker name */
	memcpy(ih.progName, "FT2Clone Plugin     ", 20);

	/* Copy instrument parameters */
	memcpy(ih.note2SampleLUT, instr->note2SampleLUT, 96);
	memcpy(ih.volEnvPoints, instr->volEnvPoints, sizeof(ih.volEnvPoints));
	memcpy(ih.panEnvPoints, instr->panEnvPoints, sizeof(ih.panEnvPoints));
	ih.volEnvLength = instr->volEnvLength;
	ih.panEnvLength = instr->panEnvLength;
	ih.volEnvSustain = instr->volEnvSustain;
	ih.volEnvLoopStart = instr->volEnvLoopStart;
	ih.volEnvLoopEnd = instr->volEnvLoopEnd;
	ih.panEnvSustain = instr->panEnvSustain;
	ih.panEnvLoopStart = instr->panEnvLoopStart;
	ih.panEnvLoopEnd = instr->panEnvLoopEnd;
	ih.volEnvFlags = instr->volEnvFlags;
	ih.panEnvFlags = instr->panEnvFlags;
	ih.vibType = instr->autoVibType;
	ih.vibSweep = instr->autoVibSweep;
	ih.vibDepth = instr->autoVibDepth;
	ih.vibRate = instr->autoVibRate;
	ih.fadeout = instr->fadeout;
	ih.midiOn = instr->midiOn ? 1 : 0;
	ih.midiChannel = instr->midiChannel;
	ih.midiProgram = instr->midiProgram;
	ih.midiBend = instr->midiBend;
	ih.mute = instr->mute ? 1 : 0;
	ih.numSamples = numSamples;

	/* Build sample headers and write to buffer */
	uint8_t *p = *outData;

	/* Write XI header first */
	memcpy(p, &ih, XI_HEADER_SIZE);
	p += XI_HEADER_SIZE;

	/* Write sample headers */
	for (int32_t i = 0; i < numSamples; i++)
	{
		ft2_sample_t *smp = &instr->smp[i];
		xi_sample_header_t sh;
		memset(&sh, 0, sizeof(sh));

		bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);

		sh.length = smp->length;
		sh.loopStart = smp->loopStart;
		sh.loopLength = smp->loopLength;

		if (sample16Bit)
		{
			sh.length *= 2;
			sh.loopStart *= 2;
			sh.loopLength *= 2;
		}

		sh.volume = smp->volume;
		sh.finetune = smp->finetune;
		sh.flags = smp->flags;
		sh.panning = smp->panning;
		sh.relativeNote = smp->relativeNote;

		nameLen = (int32_t)strlen(smp->name);
		if (nameLen > 22) nameLen = 22;
		sh.nameLength = (uint8_t)nameLen;
		memset(sh.name, 0, 22);
		if (nameLen > 0)
			memcpy(sh.name, smp->name, nameLen);

		if (smp->dataPtr == NULL)
			sh.length = 0;

		memcpy(p, &sh, sizeof(sh));
		p += sizeof(sh);
	}

	/* Write sample data (delta-encoded) */
	for (int32_t i = 0; i < numSamples; i++)
	{
		ft2_sample_t *smp = &instr->smp[i];
		if (smp->dataPtr != NULL && smp->length > 0)
		{
			int32_t smpBytes = smp->length;
			if (smp->flags & FT2_SAMPLE_16BIT)
				smpBytes *= 2;

			/* Unfix, delta encode, copy, restore */
			ft2_unfix_sample(smp);
			sample_to_delta(smp->dataPtr, smp->length, smp->flags);

			memcpy(p, smp->dataPtr, smpBytes);
			p += smpBytes;

			/* Restore sample for playback */
			delta_to_sample(smp->dataPtr, smp->length, smp->flags);
			ft2_fix_sample(smp);
		}
	}

	*outSize = (uint32_t)(p - *outData);
	return true;
}

bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL || dataSize == 0)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	if (sampleNum < 0 || sampleNum >= 16)
		return false;

	ft2_file_format format = ft2_detect_format_by_header(data, dataSize);

	/* Ensure instrument exists */
	if (inst->replayer.instr[instrNum] == NULL)
	{
		if (!ft2_instance_alloc_instr(inst, instrNum))
			return false;
	}

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	ft2_sample_t *smp = &instr->smp[sampleNum];

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, smp);

	/* Free existing sample data */
	if (smp->origDataPtr != NULL)
	{
		free(smp->origDataPtr);
		smp->origDataPtr = NULL;
		smp->dataPtr = NULL;
	}

	if (format == FT2_FORMAT_WAV)
	{
		const uint8_t *audioData;
		uint32_t audioSize;
		int16_t channels, bitsPerSample;
		uint32_t sampleRate;

		if (!ft2_parse_wav(data, dataSize, &audioData, &audioSize,
		                   &channels, &bitsPerSample, &sampleRate))
			return false;

		/* Calculate sample length */
		int32_t srcBytesPerSample = (bitsPerSample == 16) ? 2 : 1;
		if (channels == 2)
			srcBytesPerSample *= 2;

		int32_t numSamples = audioSize / srcBytesPerSample;
		bool is16Bit = (bitsPerSample == 16);

		/* Allocate sample data with proper padding for interpolation */
		int32_t dstBytesPerSample = is16Bit ? 2 : 1;
		int32_t leftPadding = FT2_MAX_TAPS * dstBytesPerSample;
		int32_t rightPadding = FT2_MAX_TAPS * dstBytesPerSample;
		int32_t dataLen = numSamples * dstBytesPerSample;
		int32_t allocSize = leftPadding + dataLen + rightPadding;

		smp->origDataPtr = (int8_t *)calloc(allocSize, 1);
		if (smp->origDataPtr == NULL)
			return false;

		smp->dataPtr = smp->origDataPtr + leftPadding;

		/* Convert audio data */
		if (channels == 1)
		{
			/* Mono */
			memcpy(smp->dataPtr, audioData, dataLen);
		}
		else
		{
			/* Stereo - mix to mono */
			if (is16Bit)
			{
				int16_t *dst = (int16_t *)smp->dataPtr;
				const int16_t *src = (const int16_t *)audioData;
				for (int32_t i = 0; i < numSamples; i++)
				{
					int32_t left = src[i * 2];
					int32_t right = src[i * 2 + 1];
					dst[i] = (int16_t)((left + right) / 2);
				}
			}
			else
			{
				int8_t *dst = smp->dataPtr;
				const int8_t *src = (const int8_t *)audioData;
				for (int32_t i = 0; i < numSamples; i++)
				{
					int32_t left = src[i * 2];
					int32_t right = src[i * 2 + 1];
					dst[i] = (int8_t)((left + right) / 2);
				}
			}
		}

		smp->length = numSamples;
		smp->flags = is16Bit ? FT2_SAMPLE_16BIT : 0;
		smp->volume = 64;
		smp->panning = 128;
		smp->loopStart = 0;
		smp->loopLength = 0;

		if (sampleNum >= instr->numSamples)
			instr->numSamples = sampleNum + 1;

		/* Set up note-to-sample mapping: all notes map to this sample */
		for (int i = 0; i < 96; i++)
			instr->note2SampleLUT[i] = (uint8_t)sampleNum;

		/* Sanitize and fix sample for interpolation */
		ft2_sanitize_sample(smp);
		ft2_fix_sample(smp);

		return true;
	}

	return false;
}

void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                        int16_t sampleNum, const char *filename)
{
	if (inst == NULL || filename == NULL)
		return;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return;

	if (sampleNum < 0 || sampleNum >= 16)
		return;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return;

	ft2_sample_t *smp = &instr->smp[sampleNum];

	/* Find last path separator */
	const char *nameStart = filename;
	const char *p = filename;
	while (*p)
	{
		if (*p == '/' || *p == '\\')
			nameStart = p + 1;
		p++;
	}

	/* Find extension dot */
	const char *extDot = NULL;
	p = nameStart;
	while (*p)
	{
		if (*p == '.')
			extDot = p;
		p++;
	}

	/* Calculate name length (without extension) */
	int32_t nameLen;
	if (extDot != NULL && extDot > nameStart)
		nameLen = (int32_t)(extDot - nameStart);
	else
		nameLen = (int32_t)strlen(nameStart);

	/* Copy name (max 22 chars) */
	if (nameLen > 22)
		nameLen = 22;

	memset(smp->name, 0, sizeof(smp->name));
	memcpy(smp->name, nameStart, nameLen);
}

bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	if (instrNum < 1 || instrNum > FT2_MAX_INST)
		return false;

	if (sampleNum < 0 || sampleNum >= 16)
		return false;

	ft2_instr_t *instr = inst->replayer.instr[instrNum];
	if (instr == NULL)
		return false;

	ft2_sample_t *smp = &instr->smp[sampleNum];
	if (smp->dataPtr == NULL || smp->length == 0)
		return false;

	int16_t bitsPerSample = (smp->flags & FT2_SAMPLE_16BIT) ? 16 : 8;

	return ft2_create_wav(smp->dataPtr, smp->length, 1, bitsPerSample, 44100,
	                       outData, outSize);
}

/* XP pattern file header */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct xp_header_t
{
	uint16_t version;
	uint16_t numRows;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
xp_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* TRACK_WIDTH = 5 bytes per note * 32 channels */
#define XP_TRACK_WIDTH (5 * FT2_MAX_CHANNELS)

bool ft2_load_pattern(ft2_instance_t *inst, int16_t pattNum,
                       const uint8_t *data, uint32_t dataSize)
{
	if (inst == NULL || data == NULL)
		return false;

	if (pattNum < 0 || pattNum >= FT2_MAX_PATTERNS)
		return false;

	/* Check minimum size for header */
	if (dataSize < sizeof(xp_header_t))
		return false;

	/* Read header */
	xp_header_t hdr;
	memcpy(&hdr, data, sizeof(hdr));

	/* Validate version */
	if (hdr.version != 1)
		return false;

	/* Clamp numRows */
	if (hdr.numRows > FT2_MAX_PATT_LEN)
		hdr.numRows = FT2_MAX_PATT_LEN;

	/* Check data size */
	uint32_t expectedSize = sizeof(xp_header_t) + (hdr.numRows * XP_TRACK_WIDTH);
	if (dataSize < expectedSize)
		return false;

	/* Allocate pattern if needed */
	if (inst->replayer.pattern[pattNum] == NULL)
	{
		if (!allocatePattern(inst, pattNum))
			return false;
	}

	ft2_note_t *patt = inst->replayer.pattern[pattNum];

	/* Copy pattern data */
	memcpy(patt, data + sizeof(xp_header_t), hdr.numRows * XP_TRACK_WIDTH);

	/* Sanitize data */
	for (int32_t row = 0; row < hdr.numRows; row++)
	{
		for (int32_t ch = 0; ch < FT2_MAX_CHANNELS; ch++)
		{
			ft2_note_t *note = &patt[(row * FT2_MAX_CHANNELS) + ch];

			if (note->note > 97)
				note->note = 0;

			if (note->instr > 128)
				note->instr = 128;

			if (note->efx > 35)
			{
				note->efx = 0;
				note->efxData = 0;
			}
		}
	}

	/* Set pattern length */
	inst->replayer.patternNumRows[pattNum] = hdr.numRows;

	/* Update current song if this is the active pattern */
	if (inst->replayer.song.pattNum == pattNum)
		inst->replayer.song.currNumRows = hdr.numRows;

	return true;
}

bool ft2_save_pattern(ft2_instance_t *inst, int16_t pattNum,
                       uint8_t **outData, uint32_t *outSize)
{
	if (inst == NULL || outData == NULL || outSize == NULL)
		return false;

	if (pattNum < 0 || pattNum >= FT2_MAX_PATTERNS)
		return false;

	ft2_note_t *patt = inst->replayer.pattern[pattNum];
	if (patt == NULL)
		return false;

	int16_t numRows = inst->replayer.patternNumRows[pattNum];
	if (numRows < 1)
		numRows = 64;

	/* Calculate size: header + pattern data */
	uint32_t totalSize = sizeof(xp_header_t) + (numRows * XP_TRACK_WIDTH);

	/* Allocate buffer */
	*outData = (uint8_t *)malloc(totalSize);
	if (*outData == NULL)
		return false;

	/* Write header */
	xp_header_t hdr;
	hdr.version = 1;
	hdr.numRows = numRows;

	memcpy(*outData, &hdr, sizeof(hdr));

	/* Write pattern data */
	memcpy(*outData + sizeof(hdr), patt, numRows * XP_TRACK_WIDTH);

	*outSize = totalSize;
	return true;
}

bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                    const uint8_t **outAudioData, uint32_t *outAudioSize,
                    int16_t *outChannels, int16_t *outBitsPerSample,
                    uint32_t *outSampleRate)
{
	if (data == NULL || dataSize < 44)
		return false;

	/* Check RIFF header */
	if (memcmp(data, "RIFF", 4) != 0)
		return false;

	if (memcmp(data + 8, "WAVE", 4) != 0)
		return false;

	/* Parse chunks */
	uint32_t pos = 12;
	bool foundFmt = false;
	bool foundData = false;

	int16_t channels = 0;
	int16_t bitsPerSample = 0;
	uint32_t sampleRate = 0;
	const uint8_t *audioData = NULL;
	uint32_t audioSize = 0;

	while (pos + 8 <= dataSize)
	{
		const char *chunkId = (const char *)(data + pos);
		uint32_t chunkSize;
		memcpy(&chunkSize, data + pos + 4, 4);

		if (memcmp(chunkId, "fmt ", 4) == 0)
		{
			if (pos + 8 + chunkSize > dataSize)
				return false;

			/* Audio format (1 = PCM) */
			uint16_t audioFormat;
			memcpy(&audioFormat, data + pos + 8, 2);
			if (audioFormat != 1)
				return false;  /* Only PCM supported */

			memcpy(&channels, data + pos + 10, 2);
			memcpy(&sampleRate, data + pos + 12, 4);
			memcpy(&bitsPerSample, data + pos + 22, 2);

			foundFmt = true;
		}
		else if (memcmp(chunkId, "data", 4) == 0)
		{
			if (pos + 8 + chunkSize > dataSize)
				chunkSize = dataSize - pos - 8;

			audioData = data + pos + 8;
			audioSize = chunkSize;
			foundData = true;
		}

		pos += 8 + chunkSize;
		if (chunkSize & 1)
			pos++;  /* Pad to word boundary */
	}

	if (!foundFmt || !foundData)
		return false;

	*outAudioData = audioData;
	*outAudioSize = audioSize;
	*outChannels = channels;
	*outBitsPerSample = bitsPerSample;
	*outSampleRate = sampleRate;

	return true;
}

bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                     int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                     uint8_t **outData, uint32_t *outSize)
{
	if (sampleData == NULL || sampleLength <= 0 || outData == NULL || outSize == NULL)
		return false;

	int32_t bytesPerSample = (bitsPerSample / 8) * channels;
	int32_t dataSize = sampleLength * bytesPerSample;
	int32_t fileSize = 44 + dataSize;

	*outData = (uint8_t *)malloc(fileSize);
	if (*outData == NULL)
		return false;

	uint8_t *p = *outData;

	/* RIFF header */
	memcpy(p, "RIFF", 4);
	p += 4;

	uint32_t riffSize = fileSize - 8;
	memcpy(p, &riffSize, 4);
	p += 4;

	memcpy(p, "WAVE", 4);
	p += 4;

	/* fmt chunk */
	memcpy(p, "fmt ", 4);
	p += 4;

	uint32_t fmtSize = 16;
	memcpy(p, &fmtSize, 4);
	p += 4;

	uint16_t audioFormat = 1;  /* PCM */
	memcpy(p, &audioFormat, 2);
	p += 2;

	memcpy(p, &channels, 2);
	p += 2;

	memcpy(p, &sampleRate, 4);
	p += 4;

	uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
	memcpy(p, &byteRate, 4);
	p += 4;

	uint16_t blockAlign = channels * (bitsPerSample / 8);
	memcpy(p, &blockAlign, 2);
	p += 2;

	memcpy(p, &bitsPerSample, 2);
	p += 2;

	/* data chunk */
	memcpy(p, "data", 4);
	p += 4;

	memcpy(p, &dataSize, 4);
	p += 4;

	/* Audio data */
	if (bitsPerSample == 8)
	{
		/* Convert signed to unsigned for WAV */
		for (int32_t i = 0; i < sampleLength; i++)
			p[i] = (uint8_t)(sampleData[i] + 128);
	}
	else
	{
		memcpy(p, sampleData, dataSize);
	}

	*outSize = fileSize;
	return true;
}

/* Callback for unsaved changes confirmation when loading a dropped module */
static void unsavedChangesDropCallback(ft2_instance_t *inst, ft2_dialog_result_t result,
                                       const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result == DIALOG_RESULT_YES)
	{
		inst->diskop.requestDropLoad = true;
		inst->uiState.needsFullRedraw = true;
	}
}

void ft2_diskop_request_drop_load(ft2_instance_t *inst, const char *path)
{
	if (inst == NULL || path == NULL)
		return;

	/* Store the path for later loading */
	strncpy(inst->diskop.pendingDropPath, path, FT2_PATH_MAX - 1);
	inst->diskop.pendingDropPath[FT2_PATH_MAX - 1] = '\0';

	if (inst->replayer.song.isModified)
	{
		/* Show confirmation dialog */
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL)
		{
			ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
				"You have unsaved changes in your song. Load new song and lose ALL changes?",
				inst, unsavedChangesDropCallback, NULL);
		}
	}
	else
	{
		/* No unsaved changes - load directly */
		inst->diskop.requestDropLoad = true;
	}
}
