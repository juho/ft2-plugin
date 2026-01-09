/**
 * @file ft2_plugin_checkboxes.h
 * @brief Checkbox widget definitions and IDs.
 *
 * Layout coordinates from standalone ft2_checkboxes.h.
 * Runtime state (visible, checked, pressed) stored per-instance in ft2_widgets_t.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

/* Checkbox IDs grouped by screen/feature */
enum
{
	CB_RES_1, /* Reserved slot 0 */

	/* Nibbles game options */
	CB_NIBBLES_SURROUND,
	CB_NIBBLES_GRID,
	CB_NIBBLES_WRAP,

	/* Advanced edit: copy/paste/transpose column masks (0=note,1=instr,2=vol,3=fx,4=fxdata) */
	CB_ENABLE_MASKING,
	CB_COPY_MASK0, CB_COPY_MASK1, CB_COPY_MASK2, CB_COPY_MASK3, CB_COPY_MASK4,
	CB_PASTE_MASK0, CB_PASTE_MASK1, CB_PASTE_MASK2, CB_PASTE_MASK3, CB_PASTE_MASK4,
	CB_TRANSP_MASK0, CB_TRANSP_MASK1, CB_TRANSP_MASK2, CB_TRANSP_MASK3, CB_TRANSP_MASK4,

	/* Instrument editor: envelope enable/sustain/loop flags */
	CB_INST_VENV, CB_INST_VENV_SUS, CB_INST_VENV_LOOP,
	CB_INST_PENV, CB_INST_PENV_SUS, CB_INST_PENV_LOOP,

	/* Instrument editor extension: MIDI output */
	CB_INST_EXT_MIDI,
	CB_INST_EXT_MUTE,

	/* Sample effects: normalize after processing */
	CB_SAMPFX_NORM,

	/* Trim dialog: what to remove */
	CB_TRIM_PATT, CB_TRIM_INST, CB_TRIM_SAMP, CB_TRIM_CHAN, CB_TRIM_SMPD, CB_TRIM_CONV,

	/* Config: I/O, layout, misc */
	CB_CONF_AUTOSAVE,
	CB_CONF_VOLRAMP,
	CB_CONF_PATTSTRETCH,
	CB_CONF_HEXCOUNT,
	CB_CONF_ACCIDENTAL,      /* Flat/sharp display (uses alternate bitmap frames) */
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
	CB_CONF_OLDABOUTLOGO,      /* Unused (was classic about screen toggle) */
	CB_CONF_AUTO_UPDATE_CHECK,
	CB_CONF_MIDI_ENABLE,
	CB_CONF_MIDI_ALLCHN,
	CB_CONF_MIDI_TRANSP,
	CB_CONF_MIDI_VELOCITY,
	CB_CONF_MIDI_AFTERTOUCH,
	CB_CONF_MIDI_MODWHEEL,
	CB_CONF_MIDI_PITCHBEND,
	CB_CONF_VSYNC_OFF,
	CB_CONF_FULLSCREEN,

	/* DAW sync (plugin-specific, not in standalone) */
	CB_CONF_SYNC_BPM,
	CB_CONF_SYNC_TRANSPORT,
	CB_CONF_SYNC_POSITION,
	CB_CONF_ALLOW_FXX_SPEED,

	/* WAV renderer */
	CB_WAV_TRACKS,

	/* I/O Routing: per-channel "To Main" (sends to stereo output) */
	CB_CONF_ROUTING_CH1_TOMAIN, CB_CONF_ROUTING_CH2_TOMAIN, CB_CONF_ROUTING_CH3_TOMAIN,
	CB_CONF_ROUTING_CH4_TOMAIN, CB_CONF_ROUTING_CH5_TOMAIN, CB_CONF_ROUTING_CH6_TOMAIN,
	CB_CONF_ROUTING_CH7_TOMAIN, CB_CONF_ROUTING_CH8_TOMAIN, CB_CONF_ROUTING_CH9_TOMAIN,
	CB_CONF_ROUTING_CH10_TOMAIN, CB_CONF_ROUTING_CH11_TOMAIN, CB_CONF_ROUTING_CH12_TOMAIN,
	CB_CONF_ROUTING_CH13_TOMAIN, CB_CONF_ROUTING_CH14_TOMAIN, CB_CONF_ROUTING_CH15_TOMAIN,
	CB_CONF_ROUTING_CH16_TOMAIN, CB_CONF_ROUTING_CH17_TOMAIN, CB_CONF_ROUTING_CH18_TOMAIN,
	CB_CONF_ROUTING_CH19_TOMAIN, CB_CONF_ROUTING_CH20_TOMAIN, CB_CONF_ROUTING_CH21_TOMAIN,
	CB_CONF_ROUTING_CH22_TOMAIN, CB_CONF_ROUTING_CH23_TOMAIN, CB_CONF_ROUTING_CH24_TOMAIN,
	CB_CONF_ROUTING_CH25_TOMAIN, CB_CONF_ROUTING_CH26_TOMAIN, CB_CONF_ROUTING_CH27_TOMAIN,
	CB_CONF_ROUTING_CH28_TOMAIN, CB_CONF_ROUTING_CH29_TOMAIN, CB_CONF_ROUTING_CH30_TOMAIN,
	CB_CONF_ROUTING_CH31_TOMAIN, CB_CONF_ROUTING_CH32_TOMAIN,

	NUM_CHECKBOXES
};

enum { CHECKBOX_UNPRESSED = 0, CHECKBOX_PRESSED = 1 };

#define CHECKBOX_W 13
#define CHECKBOX_H 12

typedef void (*cbCallback_t)(struct ft2_instance_t *inst);

struct ft2_widgets_t;

/* Checkbox layout (constant). State stored in ft2_widgets_t arrays. */
typedef struct checkBox_t
{
	uint16_t x, y;
	uint16_t clickAreaWidth, clickAreaHeight;
	cbCallback_t callbackFunc;
} checkBox_t;

extern checkBox_t checkBoxes[NUM_CHECKBOXES];

/* Wire callbacks that use forward-declared functions. Call once at startup. */
void initCheckBoxes(void);

/* Draw checkbox at its position using current state. */
void drawCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/* Set visible and draw. */
void showCheckBox(struct ft2_widgets_t *widgets, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, uint16_t checkBoxID);

/* Set invisible and reset state. */
void hideCheckBox(struct ft2_widgets_t *widgets, uint16_t checkBoxID);

/* Hit test on mouse down. Returns checkbox ID or -1. */
int16_t testCheckBoxMouseDown(struct ft2_widgets_t *widgets, int32_t mouseX, int32_t mouseY,
	bool sysReqShown);

/* Update pressed state while mouse held. Redraws if mouse moved. */
void handleCheckBoxesWhileMouseDown(struct ft2_widgets_t *widgets, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int32_t lastMouseX,
	int32_t lastMouseY, int16_t lastCheckBoxID);

/* Toggle checked state on mouse release inside click area. Invokes callback. */
void testCheckBoxMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst,
	struct ft2_video_t *video, const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY,
	int16_t lastCheckBoxID);

