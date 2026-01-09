/**
 * @file ft2_plugin_checkboxes.c
 * @brief Checkbox widget implementation.
 *
 * Layout coordinates from ft2_checkboxes.c. Per-instance state in ft2_widgets_t.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_trim.h"
#include "ft2_plugin_instr_ed.h"
#include "../ft2_instance.h"

/* MIDI config callbacks (defined at bottom of file) */
static void cbMidiEnable(struct ft2_instance_t *inst);
static void cbMidiAllChannels(struct ft2_instance_t *inst);
static void cbMidiRecTranspose(struct ft2_instance_t *inst);
static void cbMidiRecVelocity(struct ft2_instance_t *inst);
static void cbMidiRecAftertouch(struct ft2_instance_t *inst);
static void cbMidiRecModWheel(struct ft2_instance_t *inst);
static void cbMidiRecPitchBend(struct ft2_instance_t *inst);

/*
 * Checkbox layout table: {x, y, clickWidth, clickHeight, callback}
 * Coordinates match standalone ft2_checkboxes.c exactly.
 * Callbacks wired in initCheckBoxes() or set to NULL for deferred wiring.
 */
checkBox_t checkBoxes[NUM_CHECKBOXES] =
{
	{ 0 }, /* CB_RES_1: reserved slot */

	/* Nibbles game options */
	{ 3, 133, 70, 12, NULL },  /* Surround */
	{ 3, 146, 40, 12, NULL },  /* Grid */
	{ 3, 159, 45, 12, NULL },  /* Wrap */

	/* Advanced edit: copy/paste/transpose masks (note, instr, vol, fx, fxdata) */
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

	/* Instrument editor: volume/pan envelope flags */
	{   3, 175, 118, 12, NULL },  /* Vol env on */
	{ 341, 192,  64, 12, NULL },  /* Vol env sustain */
	{ 341, 217,  70, 12, NULL },  /* Vol env loop */
	{   3, 262, 123, 12, NULL },  /* Pan env on */
	{ 341, 279,  64, 12, NULL },  /* Pan env sustain */
	{ 341, 304,  70, 12, NULL },  /* Pan env loop */

	/* Instrument editor extension: MIDI output */
	{   3, 112, 148, 12, cbInstExtMidi },
	{ 172, 112, 103, 12, cbInstExtMute },

	/* Sample effects: normalize after processing */
	{ 119, 384, 95, 12, NULL },

	/* Trim dialog: what to remove */
	{   3, 107, 113, 12, cbTrimUnusedPatt },
	{   3, 120, 132, 12, cbTrimUnusedInst },
	{   3, 133, 110, 12, cbTrimUnusedSamp },
	{   3, 146, 115, 12, cbTrimUnusedChans },
	{   3, 159, 130, 12, cbTrimUnusedSmpData },
	{ 139,  94, 149, 12, cbTrimSmpsTo8Bit },

	/* Config: I/O, layout, misc settings (callbacks wired in initCheckBoxes) */
	{   3,  91,  77, 12, NULL },  /* Autosave */
	{ 251, 145, 107, 12, NULL },  /* Volume ramp */
	{ 113,  14, 108, 12, NULL },  /* Pattern stretch */
	{ 113,  27, 117, 12, NULL },  /* Hex row numbers */
	{ 113,  40,  81, 12, NULL },  /* Accidentals (flat/sharp) */
	{ 113,  53,  92, 12, NULL },  /* Show zeros */
	{ 113,  66,  81, 12, NULL },  /* Framework lines */
	{ 113,  79, 128, 12, NULL },  /* Line highlight colors */
	{ 113,  92, 126, 12, NULL },  /* Channel numbers */
	{ 255,  14, 136, 12, NULL },  /* Show volume column */
	{ 237, 108,  13, 12, NULL },  /* Nice mouse pointer */
	{ 255, 158, 111, 12, NULL },  /* Software mouse */
	{ 112,   2, 150, 12, NULL },  /* Sample cut to buffer */
	{ 112,  15, 153, 12, NULL },  /* Pattern cut to buffer */
	{ 112,  28, 159, 12, NULL },  /* Kill notes on stop */
	{ 112,  41, 149, 12, NULL },  /* File overwrite warning */
	{ 112,  70, 130, 12, NULL },  /* Multichannel record */
	{ 112,  83, 157, 12, NULL },  /* Multichannel keyjazz */
	{ 112,  96, 114, 12, NULL },  /* Multichannel edit */
	{ 112, 109, 143, 12, NULL },  /* Record key-off */
	{ 112, 122,  89, 12, NULL },  /* Quantize */
	{ 112, 135, 180, 24, NULL },  /* Change pattern length on load */
	{   0,   0,   0,  0, NULL },  /* Old about logo (unused) */
	{ 112, 159, 155, 12, NULL },  /* Auto update check */
	{ 114,   2, 130, 12, cbMidiEnable },
	{ 231,  34,  30, 12, cbMidiAllChannels },
	{ 114,  50, 121, 12, cbMidiRecTranspose },
	{ 114,  66, 120, 12, cbMidiRecVelocity },
	{ 114,  82, 124, 12, cbMidiRecAftertouch },
	{ 114, 114, 130, 12, cbMidiRecModWheel },
	{ 114, 130, 130, 12, cbMidiRecPitchBend },
	{ 113, 141,  75, 12, NULL },  /* VSync off */
	{ 113, 154,  78, 12, NULL },  /* Fullscreen */

	/* DAW sync options (plugin-specific, not in standalone) */
	{ 114,  20, 100, 12, NULL },  /* Sync BPM from DAW */
	{ 114,  36, 150, 12, NULL },  /* Sync transport */
	{ 114,  52, 150, 12, NULL },  /* Sync position */
	{ 114,  68, 180, 12, NULL },  /* Allow Fxx speed changes */

	/* WAV renderer */
	{ 62, 157, 159, 24, NULL },

	/* I/O Routing: "To Main" per-channel checkboxes (3 columns of 11/11/10) */
	{ 208,  43, 13, 12, NULL }, { 208,  54, 13, 12, NULL }, { 208,  65, 13, 12, NULL },
	{ 208,  76, 13, 12, NULL }, { 208,  87, 13, 12, NULL }, { 208,  98, 13, 12, NULL },
	{ 208, 109, 13, 12, NULL }, { 208, 120, 13, 12, NULL }, { 208, 131, 13, 12, NULL },
	{ 208, 142, 13, 12, NULL }, { 208, 153, 13, 12, NULL },
	{ 368,  43, 13, 12, NULL }, { 368,  54, 13, 12, NULL }, { 368,  65, 13, 12, NULL },
	{ 368,  76, 13, 12, NULL }, { 368,  87, 13, 12, NULL }, { 368,  98, 13, 12, NULL },
	{ 368, 109, 13, 12, NULL }, { 368, 120, 13, 12, NULL }, { 368, 131, 13, 12, NULL },
	{ 368, 142, 13, 12, NULL }, { 368, 153, 13, 12, NULL },
	{ 528,  43, 13, 12, NULL }, { 528,  54, 13, 12, NULL }, { 528,  65, 13, 12, NULL },
	{ 528,  76, 13, 12, NULL }, { 528,  87, 13, 12, NULL }, { 528,  98, 13, 12, NULL },
	{ 528, 109, 13, 12, NULL }, { 528, 120, 13, 12, NULL }, { 528, 131, 13, 12, NULL },
	{ 528, 142, 13, 12, NULL }
};

/* Wire callbacks that couldn't be set in the static initializer. Call once at startup. */
void initCheckBoxes(void)
{
	/* Config: audio */
	checkBoxes[CB_CONF_VOLRAMP].callbackFunc = cbConfigVolRamp;

	/* Config: pattern layout */
	checkBoxes[CB_CONF_PATTSTRETCH].callbackFunc = cbConfigPattStretch;
	checkBoxes[CB_CONF_HEXCOUNT].callbackFunc = cbConfigHexCount;
	checkBoxes[CB_CONF_ACCIDENTAL].callbackFunc = cbConfigAccidential;
	checkBoxes[CB_CONF_SHOWZEROS].callbackFunc = cbConfigShowZeroes;
	checkBoxes[CB_CONF_FRAMEWORK].callbackFunc = cbConfigFramework;
	checkBoxes[CB_CONF_LINECOLORS].callbackFunc = cbConfigLineColors;
	checkBoxes[CB_CONF_CHANNUMS].callbackFunc = cbConfigChanNums;
	checkBoxes[CB_CONF_SHOWVOLCOL].callbackFunc = cbConfigShowVolCol;

	/* Config: miscellaneous */
	checkBoxes[CB_CONF_SAMPCUTBUF].callbackFunc = cbSampCutToBuff;
	checkBoxes[CB_CONF_PATTCUTBUF].callbackFunc = cbPattCutToBuff;
	checkBoxes[CB_CONF_KILLNOTES].callbackFunc = cbKillNotesAtStop;
	checkBoxes[CB_CONF_OVERWRITE_WARN].callbackFunc = cbFileOverwriteWarn;
	checkBoxes[CB_CONF_MULTICHAN_REC].callbackFunc = cbMultiChanRec;
	checkBoxes[CB_CONF_MULTICHAN_KEYJAZZ].callbackFunc = cbMultiChanKeyJazz;
	checkBoxes[CB_CONF_MULTICHAN_EDIT].callbackFunc = cbMultiChanEdit;
	checkBoxes[CB_CONF_REC_KEYOFF].callbackFunc = cbRecKeyOff;
	checkBoxes[CB_CONF_QUANTIZE].callbackFunc = cbQuantize;
	checkBoxes[CB_CONF_CHANGE_PATTLEN].callbackFunc = cbChangePattLen;
	checkBoxes[CB_CONF_AUTO_UPDATE_CHECK].callbackFunc = cbAutoUpdateCheck;

	/* Config: DAW sync (plugin-specific) */
	checkBoxes[CB_CONF_SYNC_BPM].callbackFunc = cbSyncBpmFromDAW;
	checkBoxes[CB_CONF_SYNC_TRANSPORT].callbackFunc = cbSyncTransportFromDAW;
	checkBoxes[CB_CONF_SYNC_POSITION].callbackFunc = cbSyncPositionFromDAW;
	checkBoxes[CB_CONF_ALLOW_FXX_SPEED].callbackFunc = cbAllowFxxSpeedChanges;

	/* Config: I/O routing (32 channels) */
	for (int i = 0; i < 32; i++)
		checkBoxes[CB_CONF_ROUTING_CH1_TOMAIN + i].callbackFunc = cbRoutingToMain;
}

/*
 * Draw checkbox using bitmap frames.
 * Frame layout: [unchecked][unchecked+pressed][checked][checked+pressed]
 * CB_CONF_ACCIDENTAL uses alternate frames (flat/sharp symbols instead of X).
 */
void drawCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID)
{
	if (widgets == NULL || checkBoxID >= NUM_CHECKBOXES)
		return;

	if (!widgets->checkBoxVisible[checkBoxID])
		return;

	checkBox_t *cb = &checkBoxes[checkBoxID];
	bool checked = widgets->checkBoxChecked[checkBoxID];
	uint8_t state = widgets->checkBoxState[checkBoxID];

	if (bmp == NULL || bmp->checkboxGfx == NULL)
	{
		/* Fallback: draw simple checkbox with X mark */
		fillRect(video, cb->x, cb->y, CHECKBOX_W, CHECKBOX_H, PAL_BUTTONS);
		hLine(video, cb->x, cb->y, CHECKBOX_W, PAL_BUTTON2);
		vLine(video, cb->x, cb->y, CHECKBOX_H, PAL_BUTTON2);
		hLine(video, cb->x, cb->y + CHECKBOX_H - 1, CHECKBOX_W, PAL_BUTTON1);
		vLine(video, cb->x + CHECKBOX_W - 1, cb->y, CHECKBOX_H, PAL_BUTTON1);

		if (checked)
		{
			line(video, cb->x + 2, cb->y + 2, cb->x + CHECKBOX_W - 3, cb->y + CHECKBOX_H - 3, PAL_FORGRND);
			line(video, cb->x + CHECKBOX_W - 3, cb->y + 2, cb->x + 2, cb->y + CHECKBOX_H - 3, PAL_FORGRND);
		}
		return;
	}

	const uint8_t *gfxPtr = bmp->checkboxGfx;

	/* Accidental checkbox uses frames 4-7 (flat/sharp symbols) */
	if (checkBoxID == CB_CONF_ACCIDENTAL)
		gfxPtr += 4 * (CHECKBOX_W * CHECKBOX_H);

	if (checked)
		gfxPtr += 2 * (CHECKBOX_W * CHECKBOX_H);

	if (state == CHECKBOX_PRESSED)
		gfxPtr += 1 * (CHECKBOX_W * CHECKBOX_H);

	blitFast(video, cb->x, cb->y, gfxPtr, CHECKBOX_W, CHECKBOX_H);
}

void showCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID)
{
	if (widgets == NULL || checkBoxID >= NUM_CHECKBOXES)
		return;

	widgets->checkBoxVisible[checkBoxID] = true;
	drawCheckBox(widgets, video, bmp, checkBoxID);
}

void hideCheckBox(struct ft2_widgets_t *widgets, uint16_t checkBoxID)
{
	if (widgets == NULL || checkBoxID >= NUM_CHECKBOXES)
		return;

	widgets->checkBoxState[checkBoxID] = CHECKBOX_UNPRESSED;
	widgets->checkBoxVisible[checkBoxID] = false;
}

/* Update pressed state while mouse is held. Redraws only if mouse moved. */
void handleCheckBoxesWhileMouseDown(struct ft2_widgets_t *widgets, ft2_video_t *video, const ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastCheckBoxID)
{
	if (widgets == NULL || lastCheckBoxID < 0 || lastCheckBoxID >= NUM_CHECKBOXES)
		return;

	if (!widgets->checkBoxVisible[lastCheckBoxID])
		return;

	checkBox_t *cb = &checkBoxes[lastCheckBoxID];

	widgets->checkBoxState[lastCheckBoxID] = CHECKBOX_UNPRESSED;
	if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
	    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
	{
		widgets->checkBoxState[lastCheckBoxID] = CHECKBOX_PRESSED;
	}

	if (lastMouseX != mouseX || lastMouseY != mouseY)
		drawCheckBox(widgets, video, bmp, lastCheckBoxID);
}

int16_t testCheckBoxMouseDown(struct ft2_widgets_t *widgets, int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	if (widgets == NULL)
		return -1;

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
		if (!widgets->checkBoxVisible[i])
			continue;

		if (widgets->checkBoxDisabled[i])
			continue;

		checkBox_t *cb = &checkBoxes[i];
		if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
		    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
		{
			widgets->checkBoxState[i] = CHECKBOX_PRESSED;
			return (int16_t)i;
		}
	}

	return -1;
}

void testCheckBoxMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int16_t lastCheckBoxID)
{
	if (widgets == NULL || lastCheckBoxID < 0 || lastCheckBoxID >= NUM_CHECKBOXES)
		return;

	if (!widgets->checkBoxVisible[lastCheckBoxID])
		return;

	checkBox_t *cb = &checkBoxes[lastCheckBoxID];

	if (mouseX >= cb->x && mouseX < cb->x + cb->clickAreaWidth &&
	    mouseY >= cb->y && mouseY < cb->y + cb->clickAreaHeight)
	{
		widgets->checkBoxChecked[lastCheckBoxID] = !widgets->checkBoxChecked[lastCheckBoxID];
		widgets->checkBoxState[lastCheckBoxID] = CHECKBOX_UNPRESSED;
		drawCheckBox(widgets, video, bmp, lastCheckBoxID);

		if (cb->callbackFunc != NULL)
			cb->callbackFunc(inst);
	}
}

/* MIDI config callbacks: sync checkbox state to config, redraw if dependent widgets change */

static void cbMidiEnable(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiEnabled = widgets->checkBoxChecked[CB_CONF_MIDI_ENABLE];
	inst->uiState.needsFullRedraw = true; /* Other MIDI widgets depend on this */
}

static void cbMidiAllChannels(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiAllChannels = widgets->checkBoxChecked[CB_CONF_MIDI_ALLCHN];
}

static void cbMidiRecTranspose(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiRecordTranspose = widgets->checkBoxChecked[CB_CONF_MIDI_TRANSP];
	inst->uiState.needsFullRedraw = true; /* Transpose value widget depends on this */
}

static void cbMidiRecVelocity(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiRecordVelocity = widgets->checkBoxChecked[CB_CONF_MIDI_VELOCITY];
}

static void cbMidiRecAftertouch(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiRecordAftertouch = widgets->checkBoxChecked[CB_CONF_MIDI_AFTERTOUCH];
}

static void cbMidiRecModWheel(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiRecordModWheel = widgets->checkBoxChecked[CB_CONF_MIDI_MODWHEEL];
	inst->uiState.needsFullRedraw = true; /* Range widget depends on this */
}

static void cbMidiRecPitchBend(struct ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->config.midiRecordPitchBend = widgets->checkBoxChecked[CB_CONF_MIDI_PITCHBEND];
	inst->uiState.needsFullRedraw = true; /* Range widget depends on this */
}

