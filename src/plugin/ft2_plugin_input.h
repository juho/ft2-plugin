/**
 * @file ft2_plugin_input.h
 * @brief Input handling: keyboard, mouse, MIDI note recording.
 *
 * Keyboard layout matches FT2: Z-M=C-B, Q-P=C-E (+1 octave).
 * Numpad for instrument banks, F1-F8 for playback/octave.
 * Supports multi-channel edit/record and MIDI controller recording.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/* Key codes - remapped to fit within keyDown[512] for proper state tracking */
enum {
	/* ASCII keys */
	FT2_KEY_SPACE = ' ',
	FT2_KEY_RETURN = '\r',
	FT2_KEY_ESCAPE = 27,
	FT2_KEY_BACKSPACE = 8,
	FT2_KEY_TAB = '\t',
	FT2_KEY_DELETE = 127,

	/* Navigation keys (0x80-0x8F) */
	FT2_KEY_LEFT = 0x80,
	FT2_KEY_RIGHT = 0x81,
	FT2_KEY_UP = 0x82,
	FT2_KEY_DOWN = 0x83,
	FT2_KEY_PAGEUP = 0x84,
	FT2_KEY_PAGEDOWN = 0x85,
	FT2_KEY_HOME = 0x86,
	FT2_KEY_END = 0x87,
	FT2_KEY_INSERT = 0x88,

	/* F-keys (0x90-0x9F) */
	FT2_KEY_F1 = 0x90,
	FT2_KEY_F2 = 0x91,
	FT2_KEY_F3 = 0x92,
	FT2_KEY_F4 = 0x93,
	FT2_KEY_F5 = 0x94,
	FT2_KEY_F6 = 0x95,
	FT2_KEY_F7 = 0x96,
	FT2_KEY_F8 = 0x97,
	FT2_KEY_F9 = 0x98,
	FT2_KEY_F10 = 0x99,
	FT2_KEY_F11 = 0x9A,
	FT2_KEY_F12 = 0x9B,

	/* Numpad keys (0xA0-0xBF) */
	FT2_KEY_NUMPAD0 = 0xA0,
	FT2_KEY_NUMPAD1 = 0xA1,
	FT2_KEY_NUMPAD2 = 0xA2,
	FT2_KEY_NUMPAD3 = 0xA3,
	FT2_KEY_NUMPAD4 = 0xA4,
	FT2_KEY_NUMPAD5 = 0xA5,
	FT2_KEY_NUMPAD6 = 0xA6,
	FT2_KEY_NUMPAD7 = 0xA7,
	FT2_KEY_NUMPAD8 = 0xA8,
	FT2_KEY_NUMPAD9 = 0xA9,
	FT2_KEY_NUMPAD_ENTER = 0xAA,
	FT2_KEY_NUMPAD_PLUS = 0xAB,
	FT2_KEY_NUMPAD_MINUS = 0xAC,
	FT2_KEY_NUMPAD_MULTIPLY = 0xAD,
	FT2_KEY_NUMPAD_DIVIDE = 0xAE,
	FT2_KEY_NUMPAD_PERIOD = 0xAF,
	FT2_KEY_NUMLOCK = 0xB0
};

/* Modifier flags */
enum {
	FT2_MOD_SHIFT = 1,
	FT2_MOD_CTRL  = 2,
	FT2_MOD_ALT   = 4,
	FT2_MOD_CMD   = 8
};

/* Mouse buttons */
enum {
	MOUSE_BUTTON_LEFT   = 1,
	MOUSE_BUTTON_RIGHT  = 2,
	MOUSE_BUTTON_MIDDLE = 3
};

/* Cursor position within a channel (8 columns per channel) */
enum {
	CURSOR_NOTE  = 0,  /* Note column */
	CURSOR_INST1 = 1,  /* Instrument high nibble */
	CURSOR_INST2 = 2,  /* Instrument low nibble */
	CURSOR_VOL1  = 3,  /* Volume column effect type */
	CURSOR_VOL2  = 4,  /* Volume column param */
	CURSOR_EFX0  = 5,  /* Effect type */
	CURSOR_EFX1  = 6,  /* Effect param high nibble */
	CURSOR_EFX2  = 7   /* Effect param low nibble */
};

#define FT2_KEY_NOTE_OFF  97
#define FT2_KEY_NOTE_NONE 0

typedef struct ft2_input_state_t {
	bool keyDown[512];          /* Key state array (includes numpad+ at 0xAB) */
	int32_t lastKeyPressed;     /* Last key code */
	uint8_t modifiers;          /* Current modifier flags */
	int32_t mouseX, mouseY;     /* Mouse position */
	uint8_t mouseButtons;       /* Mouse button state bitmask */
	bool mouseDragging;         /* Mouse drag in progress */
	bool pattMarkDragging;      /* Pattern marking drag */
	int8_t octave;              /* Current keyboard octave (0-7) */
	bool editMode;              /* Edit mode active */
	bool keyRepeat;             /* Key repeat in progress */
	bool ignoreCurrKeyUp;       /* Skip next key-up event */
	uint32_t keyOffNr;          /* Note-off sequence counter */
	int32_t keyOffTime[32];     /* Per-channel note-off timestamps */
	uint8_t keyOnTab[32];       /* Per-channel held note numbers */
} ft2_input_state_t;

/* Initialization */
void ft2_input_init(ft2_input_state_t *input);

/* Keyboard */
void ft2_input_key_down(struct ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers);
void ft2_input_key_up(struct ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers);

/* Mouse */
void ft2_input_mouse_down(ft2_input_state_t *input, int x, int y, int button);
void ft2_input_mouse_up(ft2_input_state_t *input, int x, int y, int button);
void ft2_input_mouse_move(ft2_input_state_t *input, int x, int y);
void ft2_input_mouse_wheel(ft2_input_state_t *input, int delta);
void ft2_input_update(ft2_input_state_t *input);

/* Note conversion: key + octave -> note 1-96, or 0 if not a note key */
int8_t ft2_key_to_note(int key, int8_t octave);

/*
 * Record/play a note from keyboard or MIDI.
 * Handles channel allocation, playback, and pattern recording.
 * vol: -1 = default, 0-64 = record to pattern
 * midiVibDepth: mod wheel depth (0 if keyboard)
 * midiPitch: pitch bend -128..127 (0 if keyboard)
 * Returns channel used (0-31), or -1 on failure.
 */
int8_t ft2_plugin_record_note(struct ft2_instance_t *inst, ft2_input_state_t *input,
                              uint8_t noteNum, int8_t vol,
                              uint16_t midiVibDepth, int16_t midiPitch);

/* Release a note on a specific channel (for MIDI note-off tracking) */
void ft2_plugin_record_note_off(struct ft2_instance_t *inst, ft2_input_state_t *input, int8_t channel);

#ifdef __cplusplus
}
#endif
