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
	CB_CONF_OLDABOUTLOGO,       /* unused - was Original FT2 About screen */
	CB_CONF_AUTO_UPDATE_CHECK,  /* Automatic update check on startup */
	CB_CONF_MIDI_ENABLE,
	CB_CONF_MIDI_ALLCHN,
	CB_CONF_MIDI_TRANSP,
	CB_CONF_MIDI_VELOCITY,
	CB_CONF_MIDI_AFTERTOUCH,
	CB_CONF_MIDI_MODWHEEL,
	CB_CONF_MIDI_PITCHBEND,
	CB_CONF_VSYNC_OFF,
	CB_CONF_FULLSCREEN,

	/* DAW sync */
	CB_CONF_SYNC_BPM,
	CB_CONF_SYNC_TRANSPORT,
	CB_CONF_SYNC_POSITION,
	CB_CONF_ALLOW_FXX_SPEED,

	/* WAV renderer */
	CB_WAV_TRACKS,

	/* I/O Routing - "To Main" checkboxes for 32 channels */
	CB_CONF_ROUTING_CH1_TOMAIN,
	CB_CONF_ROUTING_CH2_TOMAIN,
	CB_CONF_ROUTING_CH3_TOMAIN,
	CB_CONF_ROUTING_CH4_TOMAIN,
	CB_CONF_ROUTING_CH5_TOMAIN,
	CB_CONF_ROUTING_CH6_TOMAIN,
	CB_CONF_ROUTING_CH7_TOMAIN,
	CB_CONF_ROUTING_CH8_TOMAIN,
	CB_CONF_ROUTING_CH9_TOMAIN,
	CB_CONF_ROUTING_CH10_TOMAIN,
	CB_CONF_ROUTING_CH11_TOMAIN,
	CB_CONF_ROUTING_CH12_TOMAIN,
	CB_CONF_ROUTING_CH13_TOMAIN,
	CB_CONF_ROUTING_CH14_TOMAIN,
	CB_CONF_ROUTING_CH15_TOMAIN,
	CB_CONF_ROUTING_CH16_TOMAIN,
	CB_CONF_ROUTING_CH17_TOMAIN,
	CB_CONF_ROUTING_CH18_TOMAIN,
	CB_CONF_ROUTING_CH19_TOMAIN,
	CB_CONF_ROUTING_CH20_TOMAIN,
	CB_CONF_ROUTING_CH21_TOMAIN,
	CB_CONF_ROUTING_CH22_TOMAIN,
	CB_CONF_ROUTING_CH23_TOMAIN,
	CB_CONF_ROUTING_CH24_TOMAIN,
	CB_CONF_ROUTING_CH25_TOMAIN,
	CB_CONF_ROUTING_CH26_TOMAIN,
	CB_CONF_ROUTING_CH27_TOMAIN,
	CB_CONF_ROUTING_CH28_TOMAIN,
	CB_CONF_ROUTING_CH29_TOMAIN,
	CB_CONF_ROUTING_CH30_TOMAIN,
	CB_CONF_ROUTING_CH31_TOMAIN,
	CB_CONF_ROUTING_CH32_TOMAIN,

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

/* Forward declaration */
struct ft2_widgets_t;

/**
 * Checkbox definition (constant data).
 * Runtime state (visible, checked, pressed) is stored in ft2_widgets_t.
 */
typedef struct checkBox_t
{
	uint16_t x, y;
	uint16_t clickAreaWidth, clickAreaHeight;
	cbCallback_t callbackFunc;
} checkBox_t;

extern checkBox_t checkBoxes[NUM_CHECKBOXES];

/**
 * Initialize checkboxes array (constant data only).
 */
void initCheckBoxes(void);

/**
 * Draw a checkbox using per-instance state.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param checkBoxID Checkbox ID
 */
void drawCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/**
 * Show a checkbox.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param checkBoxID Checkbox ID
 */
void showCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/**
 * Hide a checkbox.
 * @param widgets Per-instance widget state
 * @param checkBoxID Checkbox ID
 */
void hideCheckBox(struct ft2_widgets_t *widgets, uint16_t checkBoxID);

/**
 * Test if mouse click is on a checkbox.
 * @param widgets Per-instance widget state
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param sysReqShown Whether system request is shown
 * @return Checkbox ID if clicked, -1 otherwise
 */
int16_t testCheckBoxMouseDown(struct ft2_widgets_t *widgets, int32_t mouseX, int32_t mouseY, bool sysReqShown);

/**
 * Handle checkbox while mouse is held down.
 * Shows PRESSED state when mouse is over the checkbox, UNPRESSED when moved away.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastMouseX Last mouse X coordinate (for change detection)
 * @param lastMouseY Last mouse Y coordinate (for change detection)
 * @param lastCheckBoxID Checkbox that was pressed
 */
void handleCheckBoxesWhileMouseDown(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastCheckBoxID);

/**
 * Handle checkbox mouse release.
 * @param widgets Per-instance widget state
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastCheckBoxID Last checkbox that was pressed
 */
void testCheckBoxMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int16_t lastCheckBoxID);

