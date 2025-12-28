/**
 * @file ft2_plugin_checkboxes.h
 * @brief Checkbox definitions for the FT2 plugin UI.
 * 
 * Ported from ft2_checkboxes.h - all coordinates preserved exactly.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

enum
{
	/* Reserved checkbox */
	CB_RES_1,

	/* Nibbles */
	CB_NIBBLES_SURROUND,
	CB_NIBBLES_GRID,
	CB_NIBBLES_WRAP,

	/* Advanced edit */
	CB_ENABLE_MASKING,
	CB_COPY_MASK0,
	CB_COPY_MASK1,
	CB_COPY_MASK2,
	CB_COPY_MASK3,
	CB_COPY_MASK4,
	CB_PASTE_MASK0,
	CB_PASTE_MASK1,
	CB_PASTE_MASK2,
	CB_PASTE_MASK3,
	CB_PASTE_MASK4,
	CB_TRANSP_MASK0,
	CB_TRANSP_MASK1,
	CB_TRANSP_MASK2,
	CB_TRANSP_MASK3,
	CB_TRANSP_MASK4,

	/* Instrument editor */
	CB_INST_VENV,
	CB_INST_VENV_SUS,
	CB_INST_VENV_LOOP,
	CB_INST_PENV,
	CB_INST_PENV_SUS,
	CB_INST_PENV_LOOP,

	/* Instrument editor extension */
	CB_INST_EXT_MIDI,
	CB_INST_EXT_MUTE,

	/* Sample effects */
	CB_SAMPFX_NORM,

	/* Trim */
	CB_TRIM_PATT,
	CB_TRIM_INST,
	CB_TRIM_SAMP,
	CB_TRIM_CHAN,
	CB_TRIM_SMPD,
	CB_TRIM_CONV,

	/* Config */
	CB_CONF_AUTOSAVE,
	CB_CONF_VOLRAMP,
	CB_CONF_PATTSTRETCH,
	CB_CONF_HEXCOUNT,
	CB_CONF_ACCIDENTAL,
	CB_CONF_SHOWZEROS,
	CB_CONF_FRAMEWORK,
	CB_CONF_LINECOLORS,
	CB_CONF_CHANNUMS,
	CB_CONF_SHOWVOLCOL,
	CB_CONF_USENICEPTR,
	CB_CONF_SOFTMOUSE,
	CB_CONF_SAMPCUTBUF,
	CB_CONF_PATTCUTBUF,
	CB_CONF_KILLNOTES,
	CB_CONF_OVERWRITE_WARN,
	CB_CONF_MULTICHAN_REC,
	CB_CONF_MULTICHAN_KEYJAZZ,
	CB_CONF_MULTICHAN_EDIT,
	CB_CONF_REC_KEYOFF,
	CB_CONF_QUANTIZE,
	CB_CONF_CHANGE_PATTLEN,
	CB_CONF_OLDABOUTLOGO,
	CB_CONF_MIDI_ENABLE,
	CB_CONF_MIDI_ALLCHN,
	CB_CONF_MIDI_TRANSP,
	CB_CONF_MIDI_VELOCITY,
	CB_CONF_MIDI_AFTERTOUCH,
	CB_CONF_VSYNC_OFF,
	CB_CONF_FULLSCREEN,
	CB_CONF_STRETCH,
	CB_CONF_PIXELFILTER,

	/* DAW sync */
	CB_CONF_SYNC_BPM,
	CB_CONF_SYNC_TRANSPORT,
	CB_CONF_SYNC_POSITION,
	CB_CONF_ALLOW_FXX_SPEED,

	/* WAV renderer */
	CB_WAV_TRACKS,

	NUM_CHECKBOXES
};

enum
{
	CHECKBOX_UNPRESSED = 0,
	CHECKBOX_PRESSED = 1
};

#define CHECKBOX_W 13
#define CHECKBOX_H 12

typedef void (*cbCallback_t)(struct ft2_instance_t *inst);

typedef struct checkBox_t
{
	uint16_t x, y;
	uint16_t clickAreaWidth, clickAreaHeight;
	cbCallback_t callbackFunc;

	bool visible;
	bool checked;
	uint8_t state;
} checkBox_t;

extern checkBox_t checkBoxes[NUM_CHECKBOXES];

/**
 * Initialize checkboxes array.
 */
void initCheckBoxes(void);

/**
 * Draw a checkbox.
 * @param video Video context
 * @param bmp Bitmap assets
 * @param checkBoxID Checkbox ID
 */
void drawCheckBox(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/**
 * Show a checkbox.
 * @param video Video context
 * @param bmp Bitmap assets
 * @param checkBoxID Checkbox ID
 */
void showCheckBox(struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/**
 * Hide a checkbox.
 * @param checkBoxID Checkbox ID
 */
void hideCheckBox(uint16_t checkBoxID);

/**
 * Test if mouse click is on a checkbox.
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param sysReqShown Whether system request is shown
 * @return Checkbox ID if clicked, -1 otherwise
 */
int16_t testCheckBoxMouseDown(int32_t mouseX, int32_t mouseY, bool sysReqShown);

/**
 * Handle checkbox while mouse is held down.
 * Shows PRESSED state when mouse is over the checkbox, UNPRESSED when moved away.
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastMouseX Last mouse X coordinate (for change detection)
 * @param lastMouseY Last mouse Y coordinate (for change detection)
 * @param lastCheckBoxID Checkbox that was pressed
 */
void handleCheckBoxesWhileMouseDown(struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastCheckBoxID);

/**
 * Handle checkbox mouse release.
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastCheckBoxID Last checkbox that was pressed
 */
void testCheckBoxMouseRelease(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int16_t lastCheckBoxID);

