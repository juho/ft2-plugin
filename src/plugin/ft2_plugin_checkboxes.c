/**
 * @file ft2_plugin_checkboxes.c
 * @brief Checkbox implementation for the FT2 plugin UI.
 * 
 * Ported from ft2_checkboxes.c - exact coordinates preserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_trim.h"

checkBox_t checkBoxes[NUM_CHECKBOXES] =
{
	/* Reserved */
	{ 0 },

	/* Nibbles */
	/*x, y,   w,  h,  callback */
	{ 3, 133, 70, 12, NULL },
	{ 3, 146, 40, 12, NULL },
	{ 3, 159, 45, 12, NULL },

	/* Advanced edit */
	/*x,   y,   w,   h,  callback */
	{ 113,  94, 105, 12, cbEnableMasking },
	{ 237, 107,  13, 12, cbCopyMask0 },
	{ 237, 120,  13, 12, cbCopyMask1 },
	{ 237, 133,  13, 12, cbCopyMask2 },
	{ 237, 146,  13, 12, cbCopyMask3 },
	{ 237, 159,  13, 12, cbCopyMask4 },
	{ 256, 107,  13, 12, cbPasteMask0 },
	{ 256, 120,  13, 12, cbPasteMask1 },
	{ 256, 133,  13, 12, cbPasteMask2 },
	{ 256, 146,  13, 12, cbPasteMask3 },
	{ 256, 159,  13, 12, cbPasteMask4 },
	{ 275, 107,  13, 12, cbTranspMask0 },
	{ 275, 120,  13, 12, cbTranspMask1 },
	{ 275, 133,  13, 12, cbTranspMask2 },
	{ 275, 146,  13, 12, cbTranspMask3 },
	{ 275, 159,  13, 12, cbTranspMask4 },

	/* Instrument editor */
	/*x,   y,   w,   h,  callback */
	{   3, 175, 118, 12, NULL },
	{ 341, 192,  64, 12, NULL },
	{ 341, 217,  70, 12, NULL },
	{   3, 262, 123, 12, NULL },
	{ 341, 279,  64, 12, NULL },
	{ 341, 304,  70, 12, NULL },

	/* Instrument editor extension */
	/*x,   y,   w,   h,  callback */
	{   3, 112, 148, 12, NULL },
	{ 172, 112, 103, 12, NULL },

	/* Sample effects */
	/*x,   y,   w,  h,  callback */
	{ 119, 384, 95, 12, NULL },

	/* Trim */
	/*x,   y,   w,   h,  callback */
	{   3, 107, 113, 12, cbTrimUnusedPatt },
	{   3, 120, 132, 12, cbTrimUnusedInst },
	{   3, 133, 110, 12, cbTrimUnusedSamp },
	{   3, 146, 115, 12, cbTrimUnusedChans },
	{   3, 159, 130, 12, cbTrimUnusedSmpData },
	{ 139,  94, 149, 12, cbTrimSmpsTo8Bit },

	/* Config */
	/*x,   y,   w,   h,  callback */
	{   3,  91,  77, 12, NULL },
	{ 512, 158, 107, 12, NULL },
	{ 113,  14, 108, 12, NULL },
	{ 113,  27, 117, 12, NULL },
	{ 113,  40,  81, 12, NULL },
	{ 113,  53,  92, 12, NULL },
	{ 113,  66,  81, 12, NULL },
	{ 113,  79, 128, 12, NULL },
	{ 113,  92, 126, 12, NULL },
	{ 255,  14, 136, 12, NULL },
	{ 237, 108,  13, 12, NULL },
	{ 255, 158, 111, 12, NULL },
	{ 212,   2, 150, 12, NULL },
	{ 212,  15, 153, 12, NULL },
	{ 212,  28, 159, 12, NULL },
	{ 212,  41, 149, 12, NULL },
	{ 212,  68, 130, 12, NULL },
	{ 212,  81, 157, 12, NULL },
	{ 212,  94, 114, 12, NULL },
	{ 212, 107, 143, 12, NULL },
	{ 212, 120,  89, 12, NULL },
	{ 212, 133, 180, 24, NULL },
	{ 212, 159, 169, 12, NULL },
	{ 411,  93,  83, 12, NULL },
	{ 530, 106,  29, 12, NULL },
	{ 411, 119, 121, 12, NULL },
	{ 411, 132, 109, 12, NULL },
	{ 411, 145, 124, 12, NULL },
	{ 113, 115,  75, 12, NULL },
	{ 113, 128,  78, 12, NULL },
	{ 113, 141,  75, 12, NULL },
	{ 113, 154,  78, 12, NULL },

	/* DAW sync (in audio config, replacing unused device sections) */
	/*x,   y,   w,   h,  callback */
	{ 114,  20, 100, 12, NULL },  /* CB_CONF_SYNC_BPM */
	{ 114,  36, 150, 12, NULL },  /* CB_CONF_SYNC_TRANSPORT */
	{ 114,  52, 150, 12, NULL },  /* CB_CONF_SYNC_POSITION */
	{ 114,  68, 180, 12, NULL },  /* CB_CONF_ALLOW_FXX_SPEED */

	/* WAV renderer */
	/*x,  y,   w,   h,  callback */
	{ 62, 157, 159, 24, NULL }
};

void initCheckBoxes(void)
{
	for (int i = 0; i < NUM_CHECKBOXES; i++)
	{
		checkBoxes[i].visible = false;
		checkBoxes[i].checked = false;
		checkBoxes[i].state = CHECKBOX_UNPRESSED;
	}

	/* Wire up volume ramping callback */
	checkBoxes[CB_CONF_VOLRAMP].callbackFunc = cbConfigVolRamp;

	/* Wire up pattern layout checkboxes */
	checkBoxes[CB_CONF_PATTSTRETCH].callbackFunc = cbConfigPattStretch;
	checkBoxes[CB_CONF_HEXCOUNT].callbackFunc = cbConfigHexCount;
	checkBoxes[CB_CONF_ACCIDENTAL].callbackFunc = cbConfigAccidential;
	checkBoxes[CB_CONF_SHOWZEROS].callbackFunc = cbConfigShowZeroes;
	checkBoxes[CB_CONF_FRAMEWORK].callbackFunc = cbConfigFramework;
	checkBoxes[CB_CONF_LINECOLORS].callbackFunc = cbConfigLineColors;
	checkBoxes[CB_CONF_CHANNUMS].callbackFunc = cbConfigChanNums;
	checkBoxes[CB_CONF_SHOWVOLCOL].callbackFunc = cbConfigShowVolCol;

	/* Wire up miscellaneous checkboxes */
	checkBoxes[CB_CONF_SAMPCUTBUF].callbackFunc = cbSampCutToBuff;
	checkBoxes[CB_CONF_PATTCUTBUF].callbackFunc = cbPattCutToBuff;
	checkBoxes[CB_CONF_KILLNOTES].callbackFunc = cbKillNotesAtStop;
	checkBoxes[CB_CONF_MULTICHAN_REC].callbackFunc = cbMultiChanRec;
	checkBoxes[CB_CONF_MULTICHAN_KEYJAZZ].callbackFunc = cbMultiChanKeyJazz;
	checkBoxes[CB_CONF_MULTICHAN_EDIT].callbackFunc = cbMultiChanEdit;
	checkBoxes[CB_CONF_REC_KEYOFF].callbackFunc = cbRecKeyOff;
	checkBoxes[CB_CONF_QUANTIZE].callbackFunc = cbQuantize;
	checkBoxes[CB_CONF_CHANGE_PATTLEN].callbackFunc = cbChangePattLen;

	/* DAW sync checkboxes */
	checkBoxes[CB_CONF_SYNC_BPM].callbackFunc = cbSyncBpmFromDAW;
	checkBoxes[CB_CONF_SYNC_TRANSPORT].callbackFunc = cbSyncTransportFromDAW;
	checkBoxes[CB_CONF_SYNC_POSITION].callbackFunc = cbSyncPositionFromDAW;
	checkBoxes[CB_CONF_ALLOW_FXX_SPEED].callbackFunc = cbAllowFxxSpeedChanges;
}

void drawCheckBox(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID)
{
	if (checkBoxID >= NUM_CHECKBOXES)
		return;

	checkBox_t *cb = &checkBoxes[checkBoxID];
	if (!cb->visible)
		return;

	if (bmp == NULL || bmp->checkboxGfx == NULL)
	{
		/* Fallback: draw simple checkbox */
		fillRect(video, cb->x, cb->y, CHECKBOX_W, CHECKBOX_H, PAL_BUTTONS);
		hLine(video, cb->x, cb->y, CHECKBOX_W, PAL_BUTTON2);
		vLine(video, cb->x, cb->y, CHECKBOX_H, PAL_BUTTON2);
		hLine(video, cb->x, cb->y + CHECKBOX_H - 1, CHECKBOX_W, PAL_BUTTON1);
		vLine(video, cb->x + CHECKBOX_W - 1, cb->y, CHECKBOX_H, PAL_BUTTON1);

		if (cb->checked)
		{
			/* Draw X */
			line(video, cb->x + 2, cb->y + 2, cb->x + CHECKBOX_W - 3, cb->y + CHECKBOX_H - 3, PAL_FORGRND);
			line(video, cb->x + CHECKBOX_W - 3, cb->y + 2, cb->x + 2, cb->y + CHECKBOX_H - 3, PAL_FORGRND);
		}
		return;
	}

	/* Use bitmap graphics */
	const uint8_t *gfxPtr = bmp->checkboxGfx;

	/* Offset for special accidental checkbox (CB_CONF_ACCIDENTAL) */
	if (checkBoxID == CB_CONF_ACCIDENTAL)
		gfxPtr += 4 * (CHECKBOX_W * CHECKBOX_H);

	if (cb->checked)
		gfxPtr += 2 * (CHECKBOX_W * CHECKBOX_H);

	if (cb->state == CHECKBOX_PRESSED)
		gfxPtr += 1 * (CHECKBOX_W * CHECKBOX_H);

	blitFast(video, cb->x, cb->y, gfxPtr, CHECKBOX_W, CHECKBOX_H);
}

void showCheckBox(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID)
{
	if (checkBoxID >= NUM_CHECKBOXES)
		return;

	checkBoxes[checkBoxID].visible = true;
	drawCheckBox(video, bmp, checkBoxID);
}

void hideCheckBox(uint16_t checkBoxID)
{
	if (checkBoxID >= NUM_CHECKBOXES)
		return;

	checkBoxes[checkBoxID].state = CHECKBOX_UNPRESSED;
	checkBoxes[checkBoxID].visible = false;
}

void handleCheckBoxesWhileMouseDown(ft2_video_t *video, const ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastCheckBoxID)
{
	if (lastCheckBoxID < 0 || lastCheckBoxID >= NUM_CHECKBOXES)
		return;

	checkBox_t *cb = &checkBoxes[lastCheckBoxID];
	if (!cb->visible)
		return;

	cb->state = CHECKBOX_UNPRESSED;
	if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
	    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
	{
		cb->state = CHECKBOX_PRESSED;
	}

	if (lastMouseX != mouseX || lastMouseY != mouseY)
	{
		drawCheckBox(video, bmp, lastCheckBoxID);
	}
}

int16_t testCheckBoxMouseDown(int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	uint16_t start, end;

	if (sysReqShown)
	{
		start = 0;
		end = 1;
	}
	else
	{
		start = 1;
		end = NUM_CHECKBOXES;
	}

	for (uint16_t i = start; i < end; i++)
	{
		checkBox_t *cb = &checkBoxes[i];
		if (!cb->visible)
			continue;

		if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
		    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
		{
			cb->state = CHECKBOX_PRESSED;
			return (int16_t)i;
		}
	}

	return -1;
}

void testCheckBoxMouseRelease(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int16_t lastCheckBoxID)
{
	if (lastCheckBoxID < 0 || lastCheckBoxID >= NUM_CHECKBOXES)
		return;

	checkBox_t *cb = &checkBoxes[lastCheckBoxID];
	if (!cb->visible)
		return;

	if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
	    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
	{
		cb->checked = !cb->checked;
		cb->state = CHECKBOX_UNPRESSED;
		drawCheckBox(video, bmp, lastCheckBoxID);

		if (cb->callbackFunc != NULL)
			cb->callbackFunc(inst);
	}
}

