/**
 * @file ft2_plugin_input.h
 * @brief Input handling for the FT2 plugin.
 * 
 * Handles keyboard input for note entry, pattern navigation, and shortcuts.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/* Key codes (matching JUCE KeyPress codes) */
enum
{
	FT2_KEY_SPACE = ' ',
	FT2_KEY_RETURN = '\r',
	FT2_KEY_ESCAPE = 27,
	FT2_KEY_BACKSPACE = 8,
	FT2_KEY_DELETE = 127,
	FT2_KEY_INSERT = 0x1008,
	FT2_KEY_LEFT = 0x1000,
	FT2_KEY_RIGHT = 0x1001,
	FT2_KEY_UP = 0x1002,
	FT2_KEY_DOWN = 0x1003,
	FT2_KEY_PAGEUP = 0x1004,
	FT2_KEY_PAGEDOWN = 0x1005,
	FT2_KEY_HOME = 0x1006,
	FT2_KEY_END = 0x1007,
	FT2_KEY_TAB = '\t',
	FT2_KEY_F1 = 0x2001,
	FT2_KEY_F2 = 0x2002,
	FT2_KEY_F3 = 0x2003,
	FT2_KEY_F4 = 0x2004,
	FT2_KEY_F5 = 0x2005,
	FT2_KEY_F6 = 0x2006,
	FT2_KEY_F7 = 0x2007,
	FT2_KEY_F8 = 0x2008,
	FT2_KEY_F9 = 0x2009,
	FT2_KEY_F10 = 0x200A,
	FT2_KEY_F11 = 0x200B,
	FT2_KEY_F12 = 0x200C,
	
	/* Numpad keys */
	FT2_KEY_NUMPAD0 = 0x3000,
	FT2_KEY_NUMPAD1 = 0x3001,
	FT2_KEY_NUMPAD2 = 0x3002,
	FT2_KEY_NUMPAD3 = 0x3003,
	FT2_KEY_NUMPAD4 = 0x3004,
	FT2_KEY_NUMPAD5 = 0x3005,
	FT2_KEY_NUMPAD6 = 0x3006,
	FT2_KEY_NUMPAD7 = 0x3007,
	FT2_KEY_NUMPAD8 = 0x3008,
	FT2_KEY_NUMPAD9 = 0x3009,
	FT2_KEY_NUMPAD_ENTER = 0x300A,
	FT2_KEY_NUMPAD_PLUS = 0x300B,
	FT2_KEY_NUMPAD_MINUS = 0x300C,
	FT2_KEY_NUMPAD_MULTIPLY = 0x300D,
	FT2_KEY_NUMPAD_DIVIDE = 0x300E,
	FT2_KEY_NUMPAD_PERIOD = 0x300F,
	FT2_KEY_NUMLOCK = 0x3010
};

/* Modifier flags */
enum
{
	FT2_MOD_SHIFT = 1,
	FT2_MOD_CTRL = 2,
	FT2_MOD_ALT = 4,
	FT2_MOD_CMD = 8
};

/* Mouse button constants */
enum
{
	MOUSE_BUTTON_LEFT = 1,
	MOUSE_BUTTON_RIGHT = 2,
	MOUSE_BUTTON_MIDDLE = 3
};

/* Cursor object positions within a channel */
enum
{
	CURSOR_NOTE = 0,
	CURSOR_INST1 = 1,
	CURSOR_INST2 = 2,
	CURSOR_VOL1 = 3,
	CURSOR_VOL2 = 4,
	CURSOR_EFX0 = 5,
	CURSOR_EFX1 = 6,
	CURSOR_EFX2 = 7
};

/* Note values */
#define FT2_KEY_NOTE_OFF 97
#define FT2_KEY_NOTE_NONE 0

typedef struct ft2_input_state_t
{
	bool keyDown[512];
	int32_t lastKeyPressed;
	uint8_t modifiers;
	int32_t mouseX, mouseY;
	uint8_t mouseButtons;
	bool mouseDragging;
	bool pattMarkDragging;
	int8_t octave;
	bool editMode;
	bool keyRepeat;
	bool numPadPlusPressed;
	bool ignoreCurrKeyUp;
	uint32_t keyOffNr;
	int32_t keyOffTime[32];
	uint8_t keyOnTab[32];
} ft2_input_state_t;

/**
 * Initialize input state.
 * @param input Input state
 */
void ft2_input_init(ft2_input_state_t *input);

/**
 * Handle key down event.
 * @param inst FT2 instance
 * @param input Input state
 * @param keyCode Key code
 * @param modifiers Modifier flags
 */
void ft2_input_key_down(struct ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers);

/**
 * Handle key up event.
 * @param inst FT2 instance
 * @param input Input state
 * @param keyCode Key code
 * @param modifiers Modifier flags
 */
void ft2_input_key_up(struct ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers);

/**
 * Handle mouse down event.
 * @param input Input state
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param button Button number
 */
void ft2_input_mouse_down(ft2_input_state_t *input, int x, int y, int button);

/**
 * Handle mouse up event.
 * @param input Input state
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 * @param button Button number
 */
void ft2_input_mouse_up(ft2_input_state_t *input, int x, int y, int button);

/**
 * Handle mouse move event.
 * @param input Input state
 * @param x Mouse X coordinate
 * @param y Mouse Y coordinate
 */
void ft2_input_mouse_move(ft2_input_state_t *input, int x, int y);

/**
 * Handle mouse wheel event.
 * @param input Input state
 * @param delta Wheel delta
 */
void ft2_input_mouse_wheel(ft2_input_state_t *input, int delta);

/**
 * Update input state.
 * @param input Input state
 */
void ft2_input_update(ft2_input_state_t *input);

/**
 * Convert a keyboard character to an FT2 note number.
 * @param key Character code (ASCII)
 * @param octave Current octave
 * @return Note number (1-96), NOTE_OFF (97), or 0 if not a note key
 */
int8_t ft2_key_to_note(int key, int8_t octave);

/**
 * Record/play a note (unified path for keyboard and MIDI input).
 * Handles channel allocation, playback triggering, and pattern recording.
 * 
 * @param inst Instance
 * @param input Input state (for keyOnTab/keyOffTime tracking)
 * @param noteNum Note number (1-96) or NOTE_OFF (97)
 * @param vol Volume (-1 = use sample default and don't record vol, 0-64 = record to pattern)
 * @param midiVibDepth MIDI vibrato depth from mod wheel (0 if not MIDI)
 * @param midiPitch MIDI pitch bend value (-128 to 127, 0 if not MIDI)
 * @return Channel used (0-31), or -1 if failed/note-off
 */
int8_t ft2_plugin_record_note(struct ft2_instance_t *inst, ft2_input_state_t *input,
                              uint8_t noteNum, int8_t vol,
                              uint16_t midiVibDepth, int16_t midiPitch);

/**
 * Record a note-off on a specific channel.
 * Used when releasing a MIDI note that was tracked to a specific channel.
 * 
 * @param inst Instance
 * @param input Input state
 * @param channel Channel to release (0-31)
 */
void ft2_plugin_record_note_off(struct ft2_instance_t *inst, ft2_input_state_t *input, int8_t channel);

#ifdef __cplusplus
}
#endif
