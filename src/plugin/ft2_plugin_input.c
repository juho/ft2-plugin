/**
 * @file ft2_plugin_input.c
 * @brief Keyboard/mouse input: note entry, navigation, shortcuts, MIDI recording.
 *
 * FT2 keyboard layout: Z-M=C-B, Q-P=C-E (+1 octave), sharps on home/number rows.
 * Numpad: instrument bank selection, 0-8=quick select, Enter=swap bank.
 * Pattern editing: Insert/Delete/Backspace for line operations.
 */

#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_input.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_instance.h"

/* FT2 keyboard layout: lower row = octave N, upper row = octave N+1 */
static const int8_t keyToNote[256] = {
	['Z'] = 1,  /* C */
	['S'] = 2,  /* C# */
	['X'] = 3,  /* D */
	['D'] = 4,  /* D# */
	['C'] = 5,  /* E */
	['V'] = 6,  /* F */
	['G'] = 7,  /* F# */
	['B'] = 8,  /* G */
	['H'] = 9,  /* G# */
	['N'] = 10, /* A */
	['J'] = 11, /* A# */
	['M'] = 12, /* B */
	[','] = 13, /* C (next octave) */
	['L'] = 14, /* C# */
	['.'] = 15, /* D */
	[';'] = 16, /* D# */
	['/'] = 17, /* E */
	
	/* Upper row - one octave higher */
	['Q'] = 13, /* C */
	['2'] = 14, /* C# */
	['W'] = 15, /* D */
	['3'] = 16, /* D# */
	['E'] = 17, /* E */
	['R'] = 18, /* F */
	['5'] = 19, /* F# */
	['T'] = 20, /* G */
	['6'] = 21, /* G# */
	['Y'] = 22, /* A */
	['7'] = 23, /* A# */
	['U'] = 24, /* B */
	['I'] = 25, /* C (next octave) */
	['9'] = 26, /* C# */
	['O'] = 27, /* D */
	['0'] = 28, /* D# */
	['P'] = 29, /* E */

	/* Lowercase */
	['z'] = 1, ['s'] = 2, ['x'] = 3, ['d'] = 4, ['c'] = 5,
	['v'] = 6, ['g'] = 7, ['b'] = 8, ['h'] = 9, ['n'] = 10,
	['j'] = 11, ['m'] = 12, ['l'] = 14,
	['q'] = 13, ['w'] = 15, ['e'] = 17, ['r'] = 18, ['t'] = 20,
	['y'] = 22, ['u'] = 24, ['i'] = 25, ['o'] = 27, ['p'] = 29,
};

void ft2_input_init(ft2_input_state_t *input)
{
	if (input == NULL) return;
	memset(input, 0, sizeof(ft2_input_state_t));
	input->octave = 4;
}

/* Convert key to note number: note + octave*12, clamped to 1-96 */
int8_t ft2_key_to_note(int key, int8_t octave)
{
	if (key < 0 || key > 255) return 0;
	int8_t note = keyToNote[key];
	if (note == 0) return 0;
	int8_t adjustedNote = note + (octave * 12);
	if (adjustedNote < 1) adjustedNote = 1;
	if (adjustedNote > 96) adjustedNote = 96;
	return adjustedNote;
}

/* ---------- Playback control ---------- */

static void handlePlaybackKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL) return;

	switch (keyCode) {
		case FT2_KEY_SPACE:
			/* Space: toggle edit mode (idle) or stop (playing) */
			if (inst->replayer.playMode == FT2_PLAYMODE_IDLE) {
				memset(inst->editor.keyOnTab, 0, sizeof(inst->editor.keyOnTab));
				inst->replayer.playMode = FT2_PLAYMODE_EDIT;
			} else {
				ft2_instance_stop(inst);
			}
			inst->uiState.updatePosSections = true;
			break;

		case FT2_KEY_RETURN:
		case '\n':
			/* Enter: play song, Ctrl+Enter: play pattern */
			if (modifiers & FT2_MOD_CTRL)
				ft2_instance_play(inst, FT2_PLAYMODE_PATT, 0);
			else
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			break;

		case FT2_KEY_F5:
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT))) {
				inst->replayer.song.songPos = 0;
				inst->replayer.song.row = 0;
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			}
			break;

		case FT2_KEY_F6:
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			break;

		case FT2_KEY_F7:
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT))) {
				inst->replayer.song.row = 0;
				ft2_instance_play(inst, FT2_PLAYMODE_PATT, 0);
			}
			break;

		case FT2_KEY_F8:
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
				ft2_instance_stop(inst);
			break;

		default:
			break;
	}
}

/* ---------- Pattern navigation ---------- */

static void handleNavigationKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL) return;

	switch (keyCode) {
		case FT2_KEY_UP:
			if (modifiers & FT2_MOD_SHIFT) {
				if (inst->editor.curInstr > 0) inst->editor.curInstr--;
			} else if (modifiers & FT2_MOD_ALT) {
				keybPattMarkUp(inst);
			} else {
				uint16_t numRows = inst->replayer.song.currNumRows;
				if (numRows == 0) numRows = 64;
				if (inst->replayer.song.row > 0)
					inst->replayer.song.row--;
				else
					inst->replayer.song.row = numRows - 1;
				if (!inst->replayer.songPlaying)
					inst->editor.row = (uint8_t)inst->replayer.song.row;
			}
			inst->uiState.updatePatternEditor = true;
			break;

		case FT2_KEY_DOWN:
			if (modifiers & FT2_MOD_SHIFT) {
				if (inst->editor.curInstr < 127) inst->editor.curInstr++;
			} else if (modifiers & FT2_MOD_ALT) {
				keybPattMarkDown(inst);
			} else {
				uint16_t numRows = inst->replayer.song.currNumRows;
				if (numRows == 0) numRows = 64;
				if (inst->replayer.song.row < numRows - 1)
					inst->replayer.song.row++;
				else
					inst->replayer.song.row = 0;
				if (!inst->replayer.songPlaying)
					inst->editor.row = (uint8_t)inst->replayer.song.row;
			}
			inst->uiState.updatePatternEditor = true;
			break;

		case FT2_KEY_LEFT:
			if (modifiers & FT2_MOD_SHIFT) {
				if (inst->replayer.song.songPos > 0) {
					inst->replayer.song.songPos--;
					inst->editor.editPattern = inst->replayer.song.orders[inst->replayer.song.songPos];
				}
			} else if (modifiers & FT2_MOD_CTRL) {
				if (inst->editor.editPattern > 0) inst->editor.editPattern--;
			} else if (modifiers & FT2_MOD_ALT) {
				keybPattMarkLeft(inst);
			} else {
				if (inst->cursor.object > 0)
					inst->cursor.object--;
				else if (inst->cursor.ch > 0) {
					inst->cursor.ch--;
					inst->cursor.object = CURSOR_EFX2;
				}
			}
			inst->uiState.updatePatternEditor = true;
			break;

		case FT2_KEY_RIGHT:
			if (modifiers & FT2_MOD_SHIFT) {
				if (inst->replayer.song.songPos < inst->replayer.song.songLength - 1) {
					inst->replayer.song.songPos++;
					inst->editor.editPattern = inst->replayer.song.orders[inst->replayer.song.songPos];
				}
			} else if (modifiers & FT2_MOD_CTRL) {
				if (inst->editor.editPattern < 255) inst->editor.editPattern++;
			} else if (modifiers & FT2_MOD_ALT) {
				keybPattMarkRight(inst);
			} else {
				if (inst->cursor.object < CURSOR_EFX2)
					inst->cursor.object++;
				else if (inst->cursor.ch < inst->replayer.song.numChannels - 1) {
					inst->cursor.ch++;
					inst->cursor.object = CURSOR_NOTE;
				}
			}
			inst->uiState.updatePatternEditor = true;
			break;
		
		case FT2_KEY_PAGEUP:
		{
			uint16_t numRows = inst->replayer.song.currNumRows;
			if (numRows == 0) numRows = 64;
			int16_t step = 16;
			if (inst->replayer.song.row >= step)
				inst->replayer.song.row -= step;
			else
				inst->replayer.song.row = 0;
			if (!inst->replayer.songPlaying)
				inst->editor.row = (uint8_t)inst->replayer.song.row;
			inst->uiState.updatePatternEditor = true;
		}
		break;
		
		case FT2_KEY_PAGEDOWN:
		{
			uint16_t numRows = inst->replayer.song.currNumRows;
			if (numRows == 0) numRows = 64;
			int16_t step = 16;
			if (inst->replayer.song.row + step < numRows)
				inst->replayer.song.row += step;
			else
				inst->replayer.song.row = numRows - 1;
			if (!inst->replayer.songPlaying)
				inst->editor.row = (uint8_t)inst->replayer.song.row;
			inst->uiState.updatePatternEditor = true;
		}
		break;
		
		case FT2_KEY_HOME:
			inst->replayer.song.row = 0;
			if (!inst->replayer.songPlaying)
				inst->editor.row = 0;
			inst->uiState.updatePatternEditor = true;
			break;
		
		case FT2_KEY_END:
		{
			uint16_t numRows = inst->replayer.song.currNumRows;
			if (numRows == 0) numRows = 64;
			inst->replayer.song.row = numRows - 1;
			if (!inst->replayer.songPlaying)
				inst->editor.row = (uint8_t)inst->replayer.song.row;
			inst->uiState.updatePatternEditor = true;
		}
		break;
		
		case FT2_KEY_TAB:
			if (modifiers & FT2_MOD_SHIFT)
				cursorTabLeft(inst);
			else
				cursorTabRight(inst);
			break;

		default:
			break;
	}
}

/* ---------- Octave control ---------- */

/* F1/F2 = octave down/up (without modifiers) */
static void handleOctaveKeys(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (inst == NULL || input == NULL) return;
	if (modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)) return;

	switch (keyCode) {
		case FT2_KEY_F1:
			if (input->octave > 0) { input->octave--; inst->editor.curOctave = input->octave; }
			break;
		case FT2_KEY_F2:
			if (input->octave < 7) { input->octave++; inst->editor.curOctave = input->octave; }
			break;
		default: break;
	}
}

/* ---------- Numpad instrument selection ---------- */

/*
 * Numpad layout for instrument bank selection:
 *   NumLk / * -   = Banks 1-4 (or 5-8 with + held)
 *   0-8          = Quick select within current bank
 *   Enter        = Toggle bank 1-64 vs 65-128
 *   .            = Clear instrument (Shift+. = clear sample)
 */
static bool handleNumpadInstrumentKeys(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (inst == NULL || input == NULL) return false;

	/* With + held, only bank selection keys work */
	if (input->numPadPlusPressed && !(modifiers & FT2_MOD_CTRL)) {
		if (keyCode != FT2_KEY_NUMLOCK && keyCode != FT2_KEY_NUMPAD_DIVIDE &&
			keyCode != FT2_KEY_NUMPAD_MULTIPLY && keyCode != FT2_KEY_NUMPAD_MINUS)
			return false;
	}

	switch (keyCode) {
		case FT2_KEY_NUMPAD_ENTER:
			inst->editor.instrBankSwapped ^= 1;
			if (inst->editor.instrBankSwapped)
				inst->editor.instrBankOffset += 8 * 8;
			else
				inst->editor.instrBankOffset -= 8 * 8;
			inst->uiState.updateInstrSwitcher = true;
			inst->uiState.instrBankSwapPending = true;
			return true;
		
		case FT2_KEY_NUMLOCK:
			if (input->numPadPlusPressed)
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 12*8 : 4*8;
			else
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 8*8 : 0;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD_DIVIDE:
			if (input->numPadPlusPressed)
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 13*8 : 5*8;
			else
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 9*8 : 1*8;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD_MULTIPLY:
			if (input->numPadPlusPressed)
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 14*8 : 6*8;
			else
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 10*8 : 2*8;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD_MINUS:
			if (input->numPadPlusPressed)
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 15*8 : 7*8;
			else
				inst->editor.instrBankOffset = inst->editor.instrBankSwapped ? 11*8 : 3*8;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD_PLUS:
			input->numPadPlusPressed = true;
			return true;
		
		case FT2_KEY_NUMPAD_PERIOD:
			if (inst->editor.curInstr > 0)
			{
				if (input->modifiers & FT2_MOD_SHIFT)
					clearSample(inst);
				else
					clearInstr(inst);
			}
			return true;
		
		case FT2_KEY_NUMPAD0:
			inst->editor.curInstr = 0;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD1:
			inst->editor.curInstr = inst->editor.instrBankOffset + 1;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD2:
			inst->editor.curInstr = inst->editor.instrBankOffset + 2;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD3:
			inst->editor.curInstr = inst->editor.instrBankOffset + 3;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD4:
			inst->editor.curInstr = inst->editor.instrBankOffset + 4;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD5:
			inst->editor.curInstr = inst->editor.instrBankOffset + 5;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD6:
			inst->editor.curInstr = inst->editor.instrBankOffset + 6;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD7:
			inst->editor.curInstr = inst->editor.instrBankOffset + 7;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;
		
		case FT2_KEY_NUMPAD8:
			inst->editor.curInstr = inst->editor.instrBankOffset + 8;
			if (inst->editor.curInstr > 127) inst->editor.curInstr = 127;
			inst->uiState.updateInstrSwitcher = true;
			return true;

		default:
			return false;
	}
}

/* ---------- Edit skip ---------- */

/* Backtick: cycle edit row skip 0-16 (Shift = decrease) */
static void handleEditSkipKey(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL) return;
	if (keyCode == '`' || keyCode == '~') {
		if (modifiers & FT2_MOD_SHIFT) {
			if (inst->editor.editRowSkip == 0) inst->editor.editRowSkip = 16;
			else inst->editor.editRowSkip--;
		} else {
			if (inst->editor.editRowSkip == 16) inst->editor.editRowSkip = 0;
			else inst->editor.editRowSkip++;
		}
	}
}

/* ---------- Modified key shortcuts ---------- */

static bool handleModifiedKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL) return false;

	bool ctrlDown = (modifiers & FT2_MOD_CTRL) != 0;
	bool altDown = (modifiers & FT2_MOD_ALT) != 0;
	bool shiftDown = (modifiers & FT2_MOD_SHIFT) != 0;

	/* Ctrl+key: screen toggles */
	if (ctrlDown && !altDown && !shiftDown) {
		switch (keyCode) {
			case 'a': case 'A': inst->uiState.advEditShown = !inst->uiState.advEditShown; return true;
			case 'b': case 'B':
				if (!inst->uiState.aboutScreenShown) {
					inst->uiState.aboutScreenShown = true;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
				}
				return true;
			case 'c': case 'C':
				if (!inst->uiState.sampleEditorShown) {
					inst->uiState.configScreenShown = !inst->uiState.configScreenShown;
					if (inst->uiState.configScreenShown) {
						inst->uiState.aboutScreenShown = false;
						inst->uiState.helpScreenShown = false;
					}
				}
				return true;
			case 'd': case 'D': inst->uiState.diskOpShown = !inst->uiState.diskOpShown; return true;
			case 'e': case 'E':
				if (!inst->uiState.sampleEditorExtShown) {
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
					inst->uiState.sampleEditorExtShown = true;
				}
				return true;
			case 'h': case 'H':
				inst->uiState.helpScreenShown = !inst->uiState.helpScreenShown;
				if (inst->uiState.helpScreenShown) {
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
				}
				return true;
			case 'i': case 'I':
				if (!inst->uiState.instEditorShown) {
					inst->uiState.patternEditorShown = false;
					inst->uiState.sampleEditorShown = false;
					inst->uiState.instEditorShown = true;
				}
				return true;
			case 'm': case 'M':
				if (!inst->uiState.instEditorExtShown) {
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
					inst->uiState.instEditorExtShown = true;
				}
				return true;
			case 'p': case 'P':
				if (!inst->uiState.patternEditorShown) {
					inst->uiState.sampleEditorShown = false;
					inst->uiState.sampleEditorExtShown = false;
					inst->uiState.instEditorShown = false;
					inst->uiState.patternEditorShown = true;
				}
				return true;
			case 's': case 'S':
				if (!inst->uiState.sampleEditorShown) {
					inst->uiState.patternEditorShown = false;
					inst->uiState.instEditorShown = false;
					inst->uiState.sampleEditorShown = true;
				}
				return true;
			case 't': case 'T': inst->uiState.transposeShown = !inst->uiState.transposeShown; return true;
			case 'x': case 'X':
				/* Restore default view */
				inst->uiState.sampleEditorShown = false;
				inst->uiState.sampleEditorExtShown = false;
				inst->uiState.instEditorShown = false;
				inst->uiState.instEditorExtShown = false;
				inst->uiState.transposeShown = false;
				inst->uiState.aboutScreenShown = false;
				inst->uiState.configScreenShown = false;
				inst->uiState.helpScreenShown = false;
				inst->uiState.diskOpShown = false;
				inst->uiState.advEditShown = false;
				inst->uiState.extendedPatternEditor = false;
				inst->uiState.patternEditorShown = true;
				return true;
			case 'z': case 'Z': inst->uiState.extendedPatternEditor = !inst->uiState.extendedPatternEditor; return true;
			default: break;
		}
	}

	/* Alt+key: block ops and channel jumping */
	if (altDown && !ctrlDown && !shiftDown) {
		switch (keyCode) {
			case FT2_KEY_F3: cutBlock(inst); return true;
			case FT2_KEY_F4: copyBlock(inst); return true;
			case FT2_KEY_F5: pasteBlock(inst); return true;
			default: break;
		}

		/* Alt+QWERTY/ASDF = jump to channel 0-15 */
		int channel = -1;
		switch (keyCode) {
			case 'q': case 'Q': channel = 0; break;
			case 'w': case 'W': channel = 1; break;
			case 'e': case 'E': channel = 2; break;
			case 'r': case 'R': channel = 3; break;
			case 't': case 'T': channel = 4; break;
			case 'y': case 'Y': channel = 5; break;
			case 'u': case 'U': channel = 6; break;
			case 'i': case 'I': channel = 7; break;
			case 'a': case 'A': channel = 8; break;
			case 's': case 'S': channel = 9; break;
			case 'd': case 'D': channel = 10; break;
			case 'f': case 'F': channel = 11; break;
			case 'g': case 'G': channel = 12; break;
			case 'h': case 'H': channel = 13; break;
			case 'j': case 'J': channel = 14; break;
			case 'k': case 'K': channel = 15; break;
			default: break;
		}
		if (channel >= 0 && channel < inst->replayer.song.numChannels) {
			inst->cursor.ch = (uint8_t)channel;
			inst->cursor.object = CURSOR_NOTE;
			inst->uiState.updatePatternEditor = true;
			return true;
		}
	}

	return false;
}

/* ---------- Pattern insert/delete ---------- */

/*
 * Insert: shift data down (Shift = all channels, else current channel)
 * Backspace: delete previous row, shift up
 * Delete: clear current cell (Shift=all, Ctrl=vol+efx, Alt=efx only)
 */
static void handlePatternInsertDelete(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL) return;
	if (inst->replayer.playMode != FT2_PLAYMODE_EDIT) return;

	uint16_t patt = inst->editor.editPattern;
	if (patt >= 256) return;

	uint16_t numRows = inst->replayer.patternNumRows[patt];
	uint16_t numCh = inst->replayer.song.numChannels;
	ft2_note_t *pattern = inst->replayer.pattern[patt];
	int16_t curRow = inst->replayer.song.row;
	if (curRow < 0) curRow = 0;

	switch (keyCode) {
		case FT2_KEY_INSERT:
			if (pattern != NULL) {
				if (modifiers & FT2_MOD_SHIFT) {
					/* Insert line - all channels */
					for (int16_t row = numRows - 2; row >= curRow; row--)
						for (uint8_t ch = 0; ch < numCh; ch++)
							pattern[(row + 1) * numCh + ch] = pattern[row * numCh + ch];
					for (uint8_t ch = 0; ch < numCh; ch++)
						memset(&pattern[curRow * numCh + ch], 0, sizeof(ft2_note_t));
				} else {
					/* Insert note - current channel */
					for (int16_t row = numRows - 2; row >= curRow; row--)
						pattern[(row + 1) * numCh + inst->cursor.ch] = pattern[row * numCh + inst->cursor.ch];
					memset(&pattern[curRow * numCh + inst->cursor.ch], 0, sizeof(ft2_note_t));
				}
				ft2_song_mark_modified(inst);
				inst->uiState.updatePatternEditor = true;
			}
			break;

		case FT2_KEY_BACKSPACE:
			if (pattern != NULL && curRow > 0) {
				inst->replayer.song.row--;
				curRow = inst->replayer.song.row;
				if (!inst->replayer.songPlaying) inst->editor.row = (uint8_t)curRow;

				if (modifiers & FT2_MOD_SHIFT) {
					for (uint16_t row = curRow; row < numRows - 1; row++)
						for (uint8_t ch = 0; ch < numCh; ch++)
							pattern[row * numCh + ch] = pattern[(row + 1) * numCh + ch];
					for (uint8_t ch = 0; ch < numCh; ch++)
						memset(&pattern[(numRows - 1) * numCh + ch], 0, sizeof(ft2_note_t));
				} else {
					for (uint16_t row = curRow; row < numRows - 1; row++)
						pattern[row * numCh + inst->cursor.ch] = pattern[(row + 1) * numCh + inst->cursor.ch];
					memset(&pattern[(numRows - 1) * numCh + inst->cursor.ch], 0, sizeof(ft2_note_t));
				}
				ft2_song_mark_modified(inst);
				inst->uiState.updatePatternEditor = true;
			}
			break;

		case FT2_KEY_DELETE:
			if (pattern != NULL) {
				ft2_note_t *n = &pattern[curRow * numCh + inst->cursor.ch];
				if (modifiers & FT2_MOD_SHIFT)
					memset(n, 0, sizeof(ft2_note_t));
				else if (modifiers & FT2_MOD_CTRL)
					{ n->vol = 0; n->efx = 0; n->efxData = 0; }
				else if (modifiers & FT2_MOD_ALT)
					{ n->efx = 0; n->efxData = 0; }
				else if (inst->cursor.object == CURSOR_VOL1 || inst->cursor.object == CURSOR_VOL2)
					n->vol = 0;
				else
					{ n->note = 0; n->instr = 0; }

				if (inst->editor.editRowSkip > 0) {
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying) inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				ft2_song_mark_modified(inst);
				inst->uiState.updatePatternEditor = true;
			}
			break;

		default:
			break;
	}
}

/* ---------- Unified note recording ---------- */

/*
 * Record/play a note from keyboard or MIDI.
 * Handles channel allocation (multiEdit/multiRec/multiKeyJazz), playback,
 * and pattern recording with optional MIDI velocity/pitch/mod wheel.
 * Returns channel used or -1 on failure.
 */
int8_t ft2_plugin_record_note(ft2_instance_t *inst, ft2_input_state_t *input,
                              uint8_t noteNum, int8_t vol,
                              uint16_t midiVibDepth, int16_t midiPitch)
{
	if (inst == NULL || input == NULL) return -1;
	if (noteNum == FT2_KEY_NOTE_OFF) return -1;
	if (noteNum < 1 || noteNum > 96) return -1;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	bool *multiRecChn = (ui != NULL) ? ui->scopes.multiRecChn : NULL;
	bool *channelMuted = (ui != NULL) ? ui->scopes.channelMuted : NULL;

	bool editMode = inst->uiState.patternEditorShown &&
	                (inst->replayer.playMode == FT2_PLAYMODE_EDIT);
	bool recMode = (inst->replayer.playMode == FT2_PLAYMODE_RECSONG) ||
	               (inst->replayer.playMode == FT2_PLAYMODE_RECPATT);

	uint16_t numChannels = inst->replayer.song.numChannels;
	if (numChannels > 32) numChannels = 32;

	int8_t c = -1;  /* Channel to use */
	int8_t k = -1;  /* Channel where note already held */
	int32_t time;
	
	if (editMode || recMode)
	{
		/* Edit/record mode: find suitable channel */
		if ((inst->config.multiEdit && editMode) || (inst->config.multiRec && recMode))
		{
			/* Multi-edit/rec: find available channel from multiRecChn */
			time = 0x7FFFFFFF;
			for (int8_t i = 0; i < (int8_t)numChannels; i++)
			{
				bool muted = channelMuted ? channelMuted[i] : false;
				bool recChn = multiRecChn ? multiRecChn[i] : false;
				if (!muted && recChn && input->keyOffTime[i] < time && input->keyOnTab[i] == 0)
				{
					c = i;
					time = input->keyOffTime[i];
				}
			}
		}
		else
		{
			/* Single channel: use cursor channel */
			c = inst->cursor.ch;
		}
		
		/* Find if this note is already held on a multiRecChn channel */
		for (int8_t i = 0; i < (int8_t)numChannels; i++)
		{
			bool recChn = multiRecChn ? multiRecChn[i] : false;
			if (noteNum == input->keyOnTab[i] && recChn)
				k = i;
		}
	}
	else
	{
		/* Idle/play mode (jamming) */
		if (inst->config.multiKeyJazz)
		{
			time = 0x7FFFFFFF;
			c = 0;
		
			/* If song is playing, first try to find channels with multiRecChn set */
			if (inst->replayer.songPlaying && multiRecChn)
			{
				for (int8_t i = 0; i < (int8_t)numChannels; i++)
				{
					if (input->keyOffTime[i] < time && input->keyOnTab[i] == 0 && multiRecChn[i])
		{
						c = i;
						time = input->keyOffTime[i];
					}
				}
			}
			
			/* Fallback: if no multiRecChn channel found, use any available */
			if (time == 0x7FFFFFFF)
			{
				for (int8_t i = 0; i < (int8_t)numChannels; i++)
			{
					if (input->keyOffTime[i] < time && input->keyOnTab[i] == 0)
					{
						c = i;
						time = input->keyOffTime[i];
					}
				}
			}
		}
		else
		{
			/* Single channel: use cursor channel */
			c = inst->cursor.ch;
		}
		
		/* Find if this note is already held on any channel */
		for (int8_t i = 0; i < (int8_t)numChannels; i++)
		{
			if (noteNum == input->keyOnTab[i])
				k = i;
				}
			}
			
	/* Validate channel and check if note already held (matching standalone logic) */
	if (c < 0 || (k >= 0 && (inst->config.multiEdit || (recMode || !editMode))))
		return -1;
	
	/* Track key-on on selected channel */
	input->keyOnTab[c] = noteNum;
		
	/* Trigger the note for playback */
	ft2_instance_trigger_note(inst, noteNum, inst->editor.curInstr, c, vol, midiVibDepth, midiPitch);
		
	/* Record note to pattern in edit/record mode */
	if (editMode || recMode)
		{
			uint16_t patt = inst->editor.editPattern;
			if (!allocatePattern(inst, patt))
			return c;
			
			int16_t row = inst->replayer.song.row;
		if (c < (int8_t)numChannels && row >= 0 && row < inst->replayer.patternNumRows[patt])
			{
			ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + c];
			n->note = noteNum;
			if (inst->editor.curInstr > 0)
				n->instr = inst->editor.curInstr;
				
			/* Record volume if specified (vol >= 0 means record it) */
			if (vol >= 0 && inst->config.midiRecordVelocity)
				n->vol = 0x10 + (uint8_t)vol;  /* Volume column format: 0x10-0x50 = vol 0-64 */
			
			/* Determine which MIDI controllers get effect column vs volume column */
			/* Priority setting: 0 = pitch bend to effect, mod wheel to volume */
			/*                   1 = mod wheel to effect, pitch bend to volume */
			bool pitchToEffect = (inst->config.midiRecordPriority == 0);
			bool modToEffect = (inst->config.midiRecordPriority == 1);
			
			/* Record mod wheel vibrato if enabled */
			if (inst->config.midiRecordModWheel && midiVibDepth > 0)
			{
				/* Apply mod range scaling: midiVibDepth is 0-1024, midiModRange is 1-15 */
				/* Scale to 0-F with modRange affecting the depth */
				uint8_t depth = ((midiVibDepth * inst->config.midiModRange) >> 12) & 0x0F;
				if (depth > 0)
				{
					if (modToEffect && n->efx == 0)
					{
						/* Effect column: 04Ax = vibrato with depth */
						n->efx = 0x04;
						n->efxData = 0xA0 | depth;
					}
					else if (!modToEffect && n->vol == 0)
					{
						/* Volume column: Vx = vibrato */
						n->vol = 0xB0 | depth;
					}
				}
			}
			
			/* Record pitch bend as portamento if enabled */
			if (inst->config.midiRecordPitchBend && midiPitch != 0)
			{
				/* Calculate portamento speed based on bend range */
				/* bendRange = semitones for full bend, midiPitch = -128..127 */
				/* We want to convert to a portamento speed value (0-FF) */
				int absPitch = midiPitch < 0 ? -midiPitch : midiPitch;
				/* Scale: full bend (127) at range 2 = ~0x40, at range 12 = ~0xFF */
				uint8_t speed = (uint8_t)((absPitch * inst->config.midiBendRange * 2) / 127);
				if (speed > 0xFF) speed = 0xFF;
				if (speed == 0 && absPitch > 0) speed = 1;
				
				if (speed > 0)
				{
					if (pitchToEffect && n->efx == 0)
					{
						/* Effect column: 1xx = portamento up, 2xx = portamento down */
						n->efx = (midiPitch > 0) ? 0x01 : 0x02;
						n->efxData = speed;
					}
					else if (!pitchToEffect && n->vol == 0)
					{
						/* Volume column: Mx = tone portamento (simplified) */
						uint8_t volSpeed = speed >> 4;
						if (volSpeed == 0 && speed > 0) volSpeed = 1;
						n->vol = 0xF0 | volSpeed;
					}
				}
			}
			
			/* Record aftertouch as volume slide if enabled and volume column not already used */
			if (inst->config.midiRecordAftertouch && n->vol == 0)
			{
				uint8_t at = inst->editor.currAftertouch;
				uint8_t lastAt = inst->editor.lastRecordedAT;
				
				if (at != lastAt)
				{
					/* Convert aftertouch change to volume slide */
					int8_t delta = (int8_t)at - (int8_t)lastAt;
					if (delta > 0)
					{
						/* Aftertouch increased = volume slide up */
						uint8_t slideUp = (uint8_t)(delta >> 3);  /* Scale 0-127 to 0-15 */
						if (slideUp > 15) slideUp = 15;
						if (slideUp == 0) slideUp = 1;
						n->vol = 0x70 | slideUp;  /* +x = volume slide up */
					}
					else
					{
						/* Aftertouch decreased = volume slide down */
						uint8_t slideDown = (uint8_t)((-delta) >> 3);
						if (slideDown > 15) slideDown = 15;
						if (slideDown == 0) slideDown = 1;
						n->vol = 0x60 | slideDown;  /* -x = volume slide down */
					}
					inst->editor.lastRecordedAT = at;
				}
			}
			
			/* In edit mode (not record), advance row */
			if (!recMode && inst->editor.editRowSkip > 0)
				{
					uint16_t numRows = inst->replayer.patternNumRows[patt];
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				
				ft2_song_mark_modified(inst);
				inst->uiState.updatePatternEditor = true;
		}
	}
	
	return c;
}

/* Record note-off on specific channel. Used for MIDI release tracking. */
void ft2_plugin_record_note_off(ft2_instance_t *inst, ft2_input_state_t *input, int8_t channel)
{
	if (inst == NULL || input == NULL || channel < 0 || channel >= 32) return;

	input->keyOffNr++;
	input->keyOnTab[channel] = 0;
	input->keyOffTime[channel] = input->keyOffNr;
	ft2_instance_release_note(inst, (uint8_t)channel);

	/* Record note-off in record mode if recRelease enabled */
	bool recMode = (inst->replayer.playMode == FT2_PLAYMODE_RECSONG) ||
	               (inst->replayer.playMode == FT2_PLAYMODE_RECPATT);

	if (recMode && inst->config.recRelease) {
		uint16_t patt = inst->editor.editPattern;
		if (!allocatePattern(inst, patt)) return;

		int16_t row = inst->replayer.song.row;
		uint16_t numRows = inst->replayer.patternNumRows[patt];

		if (row >= 0 && row < numRows) {
			ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + channel];
			if (n->note != 0) {
				row = (row + 1) % numRows;
				n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + channel];
			}
			n->note = FT2_KEY_NOTE_OFF;
			ft2_song_mark_modified(inst);
			inst->uiState.updatePatternEditor = true;
		}
	}
}

/* ---------- Note input ---------- */

/* Backtick = note-off in edit/record mode */
static void handleNoteInput(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode)
{
	if (inst == NULL || input == NULL) return;

	/* Note-off key */
	if (keyCode == '`' || keyCode == '~') {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		bool *multiRecChn = (ui != NULL) ? ui->scopes.multiRecChn : NULL;
		bool *channelMuted = (ui != NULL) ? ui->scopes.channelMuted : NULL;

		bool editMode = inst->uiState.patternEditorShown &&
		                (inst->replayer.playMode == FT2_PLAYMODE_EDIT);
		bool recMode = (inst->replayer.playMode == FT2_PLAYMODE_RECSONG) ||
		               (inst->replayer.playMode == FT2_PLAYMODE_RECPATT);

		if ((editMode || recMode) && inst->uiState.patternEditorShown) {
			uint16_t numChannels = inst->replayer.song.numChannels;
			if (numChannels > 32) numChannels = 32;

			int8_t c = inst->cursor.ch;
			if (((inst->config.multiEdit && editMode) || (inst->config.multiRec && recMode)) && multiRecChn) {
				int32_t time = 0x7FFFFFFF;
				for (int8_t i = 0; i < (int8_t)numChannels; i++) {
					if ((!channelMuted || !channelMuted[i]) && multiRecChn[i] &&
					    input->keyOffTime[i] < time && input->keyOnTab[i] == 0)
						{ c = i; time = input->keyOffTime[i]; }
				}
			}
			if (c < 0 || c >= (int8_t)numChannels) return;

			uint16_t patt = inst->editor.editPattern;
			if (!allocatePattern(inst, patt)) return;

			int16_t row = inst->replayer.song.row;
			if (row >= 0 && row < inst->replayer.patternNumRows[patt]) {
				ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + c];
				n->note = FT2_KEY_NOTE_OFF;
				n->instr = 0;

				if (!recMode && inst->editor.editRowSkip > 0) {
					uint16_t numRows = inst->replayer.patternNumRows[patt];
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying) inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				ft2_song_mark_modified(inst);
				inst->uiState.updatePatternEditor = true;
			}
		}
		return;
	}

	int8_t noteNum = ft2_key_to_note(keyCode, input->octave);
	if (noteNum <= 0) return;
	ft2_plugin_record_note(inst, input, (uint8_t)noteNum, -1, 0, 0);
}

/* ---------- Effect column input ---------- */

static int8_t hexCharToValue(int keyCode)
{
	if (keyCode >= '0' && keyCode <= '9') return (int8_t)(keyCode - '0');
	if (keyCode >= 'a' && keyCode <= 'f') return (int8_t)(10 + keyCode - 'a');
	if (keyCode >= 'A' && keyCode <= 'F') return (int8_t)(10 + keyCode - 'A');
	return -1;
}

/* Volume column keys: 0-4=vol, -/+=slide, d/u=fine, s=vib speed, v=vib, p=pan, l/r=pan slide, m=porta */
static int8_t volKeyToValue(int keyCode)
{
	switch (keyCode) {
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '-': return 5;
		case '+': case '=': return 6;
		case 'd': case 'D': return 7;
		case 'u': case 'U': return 8;
		case 's': case 'S': return 9;
		case 'v': case 'V': return 10;
		case 'p': case 'P': return 11;
		case 'l': case 'L': return 12;
		case 'r': case 'R': return 13;
		case 'm': case 'M': return 14;
		default: return -1;
	}
}

/* Effect type: 0-9 + A-Z (maps to effect 0-35) */
static int8_t efxKeyToValue(int keyCode)
{
	if (keyCode >= '0' && keyCode <= '9') return (int8_t)(keyCode - '0');
	if (keyCode >= 'a' && keyCode <= 'z') return (int8_t)(10 + keyCode - 'a');
	if (keyCode >= 'A' && keyCode <= 'Z') return (int8_t)(10 + keyCode - 'A');
	return -1;
}

static void handleEffectInput(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode)
{
	if (inst == NULL || input == NULL) return;
	if (inst->replayer.playMode != FT2_PLAYMODE_EDIT &&
	    inst->replayer.playMode != FT2_PLAYMODE_RECSONG &&
	    inst->replayer.playMode != FT2_PLAYMODE_RECPATT)
		return;
	if (!inst->uiState.patternEditorShown) return;

	int8_t val;
	if (inst->cursor.object == CURSOR_VOL1)
		val = volKeyToValue(keyCode);
	else if (inst->cursor.object == CURSOR_EFX0)
		val = efxKeyToValue(keyCode);
	else
		val = hexCharToValue(keyCode);
	if (val < 0) return;
	
	uint16_t patt = inst->editor.editPattern;
	
	/* Allocate pattern if needed */
	if (!allocatePattern(inst, patt))
		return;
	
	/* Use song.row as source of truth for pattern editor position */
	int16_t row = inst->replayer.song.row;
	uint8_t ch = inst->cursor.ch;
	uint16_t numCh = inst->replayer.song.numChannels;
	
	if (ch >= numCh || row < 0 || row >= inst->replayer.patternNumRows[patt])
		return;
	
	ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + ch];
	
	switch (inst->cursor.object) {
		case CURSOR_NOTE: break;
		case CURSOR_INST1:
			n->instr = (n->instr & 0x0F) | (uint8_t)(val << 4);
			if (n->instr > 127) n->instr = 127;
			break;
		case CURSOR_INST2:
			n->instr = (n->instr & 0xF0) | (uint8_t)val;
			if (n->instr > 127) n->instr = 127;
			break;
		case CURSOR_VOL1:
			n->vol = (n->vol & 0x0F) | (uint8_t)((val + 1) << 4);
			if (n->vol >= 0x51 && n->vol <= 0x5F) n->vol = 0x50;
			break;
		case CURSOR_VOL2:
			if (n->vol < 0x10) n->vol = 0x10 + (uint8_t)val;
			else n->vol = (n->vol & 0xF0) | (uint8_t)val;
			if (n->vol >= 0x51 && n->vol <= 0x5F) n->vol = 0x50;
			break;
		case CURSOR_EFX0: n->efx = (uint8_t)val; break;
		case CURSOR_EFX1: n->efxData = (n->efxData & 0x0F) | (uint8_t)(val << 4); break;
		case CURSOR_EFX2: n->efxData = (n->efxData & 0xF0) | (uint8_t)val; break;
		default: return;
	}

	uint16_t numRows = inst->replayer.patternNumRows[patt];
	if (numRows >= 1 && inst->editor.editRowSkip > 0) {
		inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
		if (!inst->replayer.songPlaying) inst->editor.row = (uint8_t)inst->replayer.song.row;
	}

	ft2_song_mark_modified(inst);
	inst->uiState.updatePatternEditor = true;
}

/* ---------- Main key handlers ---------- */

void ft2_input_key_down(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (input == NULL) return;
	if (keyCode >= 0 && keyCode < 512) input->keyDown[keyCode] = true;
	input->lastKeyPressed = keyCode;
	input->modifiers = (uint8_t)modifiers;
	if (inst == NULL) return;

	/* Nibbles consumes all input when playing */
	if (inst->uiState.nibblesShown && inst->nibbles.playing) {
		ft2_nibbles_handle_key(inst, keyCode);
		return;
	}

	if (keyCode == FT2_KEY_NUMPAD_PLUS) input->numPadPlusPressed = true;

	if (modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT))
		if (handleModifiedKeys(inst, keyCode, modifiers)) return;

	handlePlaybackKeys(inst, keyCode, modifiers);
	if (handleNumpadInstrumentKeys(inst, input, keyCode, modifiers)) return;
	handleOctaveKeys(inst, input, keyCode, modifiers);
	handleNavigationKeys(inst, keyCode, modifiers);
	handleEditSkipKey(inst, keyCode, modifiers);
	handlePatternInsertDelete(inst, keyCode, modifiers);

	if (!(modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT | FT2_MOD_CMD))) {
		if (inst->cursor.object == CURSOR_NOTE)
			handleNoteInput(inst, input, keyCode);
		else
			handleEffectInput(inst, input, keyCode);
	}
}

void ft2_input_key_up(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (input == NULL) return;
	if (keyCode >= 0 && keyCode < 512) input->keyDown[keyCode] = false;
	input->modifiers = (uint8_t)modifiers;

	if (keyCode == FT2_KEY_NUMPAD_PLUS) input->numPadPlusPressed = false;
	if (input->ignoreCurrKeyUp) { input->ignoreCurrKeyUp = false; return; }

	/* Release note on key up for proper jamming */
	if (inst != NULL && !(modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT | FT2_MOD_CMD))) {
		int8_t note = ft2_key_to_note(keyCode, input->octave);
		if (note > 0) {
			for (uint8_t ch = 0; ch < 32; ch++) {
				if (input->keyOnTab[ch] == note) {
					input->keyOnTab[ch] = 0;
					input->keyOffNr++;
					input->keyOffTime[ch] = (int32_t)input->keyOffNr;
					ft2_instance_release_note(inst, ch);
					break;
				}
			}
		}
	}
	input->keyRepeat = false;
}

/* ---------- Mouse input ---------- */

void ft2_input_mouse_down(ft2_input_state_t *input, int x, int y, int button)
{
	if (input == NULL) return;
	input->mouseX = x;
	input->mouseY = y;
	input->mouseButtons |= (uint8_t)(1 << button);
	input->mouseDragging = true;
}

void ft2_input_mouse_up(ft2_input_state_t *input, int x, int y, int button)
{
	if (input == NULL) return;
	input->mouseX = x;
	input->mouseY = y;
	input->mouseButtons &= ~(uint8_t)(1 << button);
	input->mouseDragging = false;
}

void ft2_input_mouse_move(ft2_input_state_t *input, int x, int y)
{
	if (input == NULL) return;
	input->mouseX = x;
	input->mouseY = y;
}

void ft2_input_mouse_wheel(ft2_input_state_t *input, int delta)
{
	(void)input;
	(void)delta;
}

void ft2_input_update(ft2_input_state_t *input)
{
	(void)input;
}
