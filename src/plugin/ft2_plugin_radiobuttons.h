/**
 * @file ft2_plugin_radiobuttons.h
 * @brief Radio button definitions for the FT2 plugin UI.
 * 
 * Ported from ft2_radiobuttons.h - all coordinates preserved exactly.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

/* Radio button groups */
enum
{
	RB_GROUP_HELP,
	RB_GROUP_NIBBLES_PLAYERS,
	RB_GROUP_NIBBLES_DIFFICULTY,
	RB_GROUP_SAMPLE_LOOP,
	RB_GROUP_SAMPLE_DEPTH,
	RB_GROUP_INST_WAVEFORM,
	RB_GROUP_CONFIG_SELECT,
	RB_GROUP_CONFIG_SOUND_BUFF_SIZE,
	RB_GROUP_CONFIG_AUDIO_BIT_DEPTH,
	RB_GROUP_CONFIG_AUDIO_INTERPOLATION,
	RB_GROUP_CONFIG_AUDIO_FREQ,
	RB_GROUP_CONFIG_AUDIO_INPUT_FREQ,
	RB_GROUP_CONFIG_FREQ_SLIDES,
	RB_GROUP_CONFIG_MOUSE,
	RB_GROUP_CONFIG_MOUSE_BUSY,
	RB_GROUP_CONFIG_SCOPE,
	RB_GROUP_CONFIG_PATTERN_CHANS,
	RB_GROUP_CONFIG_FONT,
	RB_GROUP_CONFIG_PAL_ENTRIES,
	RB_GROUP_CONFIG_PAL_PRESET,
	RB_GROUP_CONFIG_FILESORT,
	RB_GROUP_CONFIG_WIN_SIZE,
	RB_GROUP_CONFIG_MIDI_TRIGGER,
	RB_GROUP_DISKOP_ITEM,
	RB_GROUP_DISKOP_MOD_SAVEAS,
	RB_GROUP_DISKOP_INS_SAVEAS,
	RB_GROUP_DISKOP_SMP_SAVEAS,
	RB_GROUP_DISKOP_PAT_SAVEAS,
	RB_GROUP_DISKOP_TRK_SAVEAS,
	RB_GROUP_WAV_RENDER_BITDEPTH,

	NUM_RB_GROUPS
};

enum
{
	/* Help screen */
	RB_HELP_FEATURES,
	RB_HELP_EFFECTS,
	RB_HELP_KEYBINDINGS,
	RB_HELP_HOWTO,
	RB_HELP_PLUGIN,

	/* Nibbles */
	RB_NIBBLES_1PLAYER,
	RB_NIBBLES_2PLAYER,
	RB_NIBBLES_NOVICE,
	RB_NIBBLES_AVERAGE,
	RB_NIBBLES_PRO,
	RB_NIBBLES_TRITON,

	/* Sample editor */
	RB_SAMPLE_NO_LOOP,
	RB_SAMPLE_FWD_LOOP,
	RB_SAMPLE_BIDI_LOOP,
	RB_SAMPLE_8BIT,
	RB_SAMPLE_16BIT,

	/* Instrument editor */
	RB_INST_WAVE_SINE,
	RB_INST_WAVE_SQUARE,
	RB_INST_WAVE_RAMPDN,
	RB_INST_WAVE_RAMPUP,

	/* Config screen select */
	RB_CONFIG_AUDIO,
	RB_CONFIG_LAYOUT,
	RB_CONFIG_MISC,
	RB_CONFIG_MIDI,
	RB_CONFIG_IO_ROUTING,

	/* Config audio buffer size */
	RB_CONFIG_AUDIO_BUFF_SMALL,
	RB_CONFIG_AUDIO_BUFF_MEDIUM,
	RB_CONFIG_AUDIO_BUFF_LARGE,

	/* Config audio bit depth */
	RB_CONFIG_AUDIO_16BIT,
	RB_CONFIG_AUDIO_32BIT,

	/* Config audio interpolation */
	RB_CONFIG_AUDIO_INTRP_NONE,
	RB_CONFIG_AUDIO_INTRP_LINEAR,
	RB_CONFIG_AUDIO_INTRP_QUADRATIC,
	RB_CONFIG_AUDIO_INTRP_CUBIC,
	RB_CONFIG_AUDIO_INTRP_SINC8,
	RB_CONFIG_AUDIO_INTRP_SINC16,

	/* Config audio frequency */
	RB_CONFIG_AUDIO_44KHZ,
	RB_CONFIG_AUDIO_48KHZ,
	RB_CONFIG_AUDIO_96KHZ,

	/* Config audio input frequency */
	RB_CONFIG_AUDIO_INPUT_44KHZ,
	RB_CONFIG_AUDIO_INPUT_48KHZ,
	RB_CONFIG_AUDIO_INPUT_96KHZ,

	/* Config frequency slides */
	RB_CONFIG_FREQ_AMIGA,
	RB_CONFIG_FREQ_LINEAR,

	/* Config mouse */
	RB_CONFIG_MOUSE_NICE,
	RB_CONFIG_MOUSE_UGLY,
	RB_CONFIG_MOUSE_AWFUL,
	RB_CONFIG_MOUSE_USABLE,

	/* Config mouse busy */
	RB_CONFIG_MOUSE_BUSY_VOGUE,
	RB_CONFIG_MOUSE_BUSY_MRH,

	/* Config scope */
	RB_CONFIG_SCOPE_STANDARD,
	RB_CONFIG_SCOPE_LINED,

	/* Config pattern channels */
	RB_CONFIG_PATT_4CHANS,
	RB_CONFIG_PATT_6CHANS,
	RB_CONFIG_PATT_8CHANS,
	RB_CONFIG_PATT_12CHANS,

	/* Config font */
	RB_CONFIG_FONT_CAPITALS,
	RB_CONFIG_FONT_LOWERCASE,
	RB_CONFIG_FONT_FUTURE,
	RB_CONFIG_FONT_BOLD,

	/* Config palette entries */
	RB_CONFIG_PAL_PATTEXT,
	RB_CONFIG_PAL_BLOCKMARK,
	RB_CONFIG_PAL_TEXTONBLOCK,
	RB_CONFIG_PAL_MOUSE,
	RB_CONFIG_PAL_DESKTOP,
	RB_CONFIG_PAL_BUTTONS,

	/* Config palette presets */
	RB_CONFIG_PAL_ARCTIC,
	RB_CONFIG_PAL_LITHE_DARK,
	RB_CONFIG_PAL_AURORA_BOREALIS,
	RB_CONFIG_PAL_ROSE,
	RB_CONFIG_PAL_BLUES,
	RB_CONFIG_PAL_DARK_MODE,
	RB_CONFIG_PAL_GOLD,
	RB_CONFIG_PAL_VIOLENT,
	RB_CONFIG_PAL_HEAVY_METAL,
	RB_CONFIG_PAL_WHY_COLORS,
	RB_CONFIG_PAL_JUNGLE,
	RB_CONFIG_PAL_USER,

	/* Config filesort */
	RB_CONFIG_FILESORT_EXT,
	RB_CONFIG_FILESORT_NAME,

	/* Config window size */
	RB_CONFIG_WIN_SIZE_AUTO,
	RB_CONFIG_WIN_SIZE_1X,
	RB_CONFIG_WIN_SIZE_3X,
	RB_CONFIG_WIN_SIZE_2X,
	RB_CONFIG_WIN_SIZE_4X,

	/* Disk op item */
	RB_DISKOP_MODULE,
	RB_DISKOP_INSTR,
	RB_DISKOP_SAMPLE,
	RB_DISKOP_PATTERN,
	RB_DISKOP_TRACK,

	/* Disk op module save as */
	RB_DISKOP_MOD_MOD,
	RB_DISKOP_MOD_XM,
	RB_DISKOP_MOD_WAV,

	/* Disk op instrument save as */
	RB_DISKOP_INS_XI,

	/* Disk op sample save as */
	RB_DISKOP_SMP_RAW,
	RB_DISKOP_SMP_IFF,
	RB_DISKOP_SMP_WAV,

	/* Disk op pattern save as */
	RB_DISKOP_PAT_XP,

	/* Disk op track save as */
	RB_DISKOP_TRK_XT,

	/* WAV render bitdepth */
	RB_WAV_RENDER_16BIT,
	RB_WAV_RENDER_32BIT,

	/* Config MIDI trigger mode */
	RB_CONFIG_MIDI_NOTES,
	RB_CONFIG_MIDI_PATTERNS,

	NUM_RADIOBUTTONS
};

enum
{
	RADIOBUTTON_UNCHECKED = 0,
	RADIOBUTTON_CHECKED = 1,
	RADIOBUTTON_PRESSED = 2
};

#define RADIOBUTTON_W 11
#define RADIOBUTTON_H 11

typedef void (*rbCallback_t)(struct ft2_instance_t *inst);

/* Forward declaration */
struct ft2_widgets_t;

/**
 * Radio button definition (constant data).
 * Runtime state (visible, checked/pressed) is stored in ft2_widgets_t.
 */
typedef struct radioButton_t
{
	uint16_t x, y;
	uint16_t clickAreaWidth;
	uint16_t group;
	rbCallback_t callbackFunc;
} radioButton_t;

extern radioButton_t radioButtons[NUM_RADIOBUTTONS];

/**
 * Initialize radio buttons array (constant data only).
 */
void initRadioButtons(void);

/**
 * Draw a radio button using per-instance state.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param radioButtonID Radio button ID
 */
void drawRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID);

/**
 * Show a radio button.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param radioButtonID Radio button ID
 */
void showRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID);

/**
 * Hide a radio button.
 * @param widgets Per-instance widget state
 * @param radioButtonID Radio button ID
 */
void hideRadioButton(struct ft2_widgets_t *widgets, uint16_t radioButtonID);

/**
 * Check a radio button (uncheck others in same group).
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param radioButtonID Radio button ID
 */
void checkRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID);

/**
 * Set a radio button as checked and uncheck others in its group without redrawing.
 * Use this in callbacks where video/bmp aren't available; next frame will draw the state.
 * @param widgets Per-instance widget state
 * @param radioButtonID Radio button ID to check
 */
void checkRadioButtonNoRedraw(struct ft2_widgets_t *widgets, uint16_t radioButtonID);

/**
 * Uncheck all radio buttons in a group.
 * @param widgets Per-instance widget state
 * @param group Radio button group
 */
void uncheckRadioButtonGroup(struct ft2_widgets_t *widgets, uint16_t group);

/**
 * Show all radio buttons in a group.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param group Radio button group
 */
void showRadioButtonGroup(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t group);

/**
 * Hide all radio buttons in a group.
 * @param widgets Per-instance widget state
 * @param group Radio button group
 */
void hideRadioButtonGroup(struct ft2_widgets_t *widgets, uint16_t group);

/**
 * Test if mouse click is on a radio button.
 * @param widgets Per-instance widget state
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param sysReqShown Whether system request is shown
 * @return Radio button ID if clicked, -1 otherwise
 */
int16_t testRadioButtonMouseDown(struct ft2_widgets_t *widgets, int32_t mouseX, int32_t mouseY, bool sysReqShown);

/**
 * Handle radio button while mouse is held down.
 * Shows PRESSED state when mouse is over the button, UNCHECKED when moved away.
 * @param widgets Per-instance widget state
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastMouseX Last mouse X coordinate (for change detection)
 * @param lastMouseY Last mouse Y coordinate (for change detection)
 * @param lastRadioButtonID Radio button that was pressed
 */
void handleRadioButtonsWhileMouseDown(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastRadioButtonID);

/**
 * Handle radio button mouse release.
 * @param widgets Per-instance widget state
 * @param inst FT2 instance
 * @param video Video context
 * @param bmp Bitmap assets
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param lastRadioButtonID Last radio button that was pressed
 */
void testRadioButtonMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, int16_t lastRadioButtonID);

