/**
 * @file ft2_plugin_input.c
 * @brief Input handling for the FT2 plugin.
 */

#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_input.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_instance.h"

/* FT2 keyboard layout mapping for notes.
 * Lower row: Z-M = C-1 to B-1
 * Home row: A-L = C#1 to A#1
 * Upper row: Q-P = C-2 to E-2
 * Number row: 1-0 = C#2 to A#2
 */
static const int8_t keyToNote[256] = {
	/* Mapping ASCII characters to note numbers (relative to octave) */
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
	
	/* Lowercase versions */
	['z'] = 1, ['s'] = 2, ['x'] = 3, ['d'] = 4, ['c'] = 5,
	['v'] = 6, ['g'] = 7, ['b'] = 8, ['h'] = 9, ['n'] = 10,
	['j'] = 11, ['m'] = 12, ['l'] = 14,
	['q'] = 13, ['w'] = 15, ['e'] = 17, ['r'] = 18, ['t'] = 20,
	['y'] = 22, ['u'] = 24, ['i'] = 25, ['o'] = 27, ['p'] = 29,
};

void ft2_input_init(ft2_input_state_t *input)
{
	if (input == NULL)
		return;

	memset(input, 0, sizeof(ft2_input_state_t));
	input->octave = 4;
}

int8_t ft2_key_to_note(int key, int8_t octave)
{
	if (key < 0 || key > 255)
		return 0;
	
	int8_t note = keyToNote[key];
	if (note == 0)
		return 0;
	
	/* Adjust for octave (same formula as standalone: note + curOctave * 12) */
	int8_t adjustedNote = note + (octave * 12);
	
	/* Clamp to valid range 1-96 */
	if (adjustedNote < 1)
		adjustedNote = 1;
	if (adjustedNote > 96)
		adjustedNote = 96;
	
	return adjustedNote;
}

static void handlePlaybackKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL)
		return;
	
	switch (keyCode)
	{
		case FT2_KEY_SPACE:
			/* Space = toggle edit mode when stopped, or stop playback when playing */
			if (inst->replayer.playMode == FT2_PLAYMODE_IDLE)
			{
				/* Enter edit mode */
				memset(inst->editor.keyOnTab, 0, sizeof(inst->editor.keyOnTab));
				inst->replayer.playMode = FT2_PLAYMODE_EDIT;
				inst->uiState.updatePosSections = true; /* Update mode text */
			}
			else
			{
				ft2_instance_stop(inst);
				inst->uiState.updatePosSections = true; /* Update mode text */
			}
			break;
		
		case FT2_KEY_RETURN:
		case '\n':
			/* Enter = play from current position (Ctrl+Enter = play pattern) */
			if (modifiers & FT2_MOD_CTRL)
				ft2_instance_play(inst, FT2_PLAYMODE_PATT, 0);
			else
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			break;
		
		case FT2_KEY_F5:
			/* F5 = play song from start (unless modifier keys) */
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
			{
				inst->replayer.song.songPos = 0;
				inst->replayer.song.row = 0;
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			}
			break;
		
		case FT2_KEY_F6:
			/* F6 = play song from current position (unless modifier keys) */
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
				ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
			break;
		
		case FT2_KEY_F7:
			/* F7 = play pattern from start (unless modifier keys) */
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
			{
				inst->replayer.song.row = 0;
				ft2_instance_play(inst, FT2_PLAYMODE_PATT, 0);
			}
			break;
		
		case FT2_KEY_F8:
			/* F8 = stop (unless modifier keys) */
			if (!(modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT)))
				ft2_instance_stop(inst);
			break;
		
		default:
			break;
	}
}

static void handleNavigationKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL)
		return;
	
	/* Pattern editor navigation */
	switch (keyCode)
	{
		case FT2_KEY_UP:
			if (modifiers & FT2_MOD_SHIFT)
			{
				/* Shift+Up = decrease current instrument */
				if (inst->editor.curInstr > 0)
					inst->editor.curInstr--;
			}
			else if (modifiers & FT2_MOD_ALT)
			{
				/* Alt+Up = mark pattern upward */
				keybPattMarkUp(inst);
			}
			else
			{
				/* Row navigation - update song.row (source of truth for pattern editor) */
				uint16_t numRows = inst->replayer.song.currNumRows;
				if (numRows == 0) numRows = 64;
				
				if (inst->replayer.song.row > 0)
					inst->replayer.song.row--;
				else
					inst->replayer.song.row = numRows - 1; /* Wrap to bottom */
				
				/* Sync editor.row when not playing */
				if (!inst->replayer.songPlaying)
					inst->editor.row = (uint8_t)inst->replayer.song.row;
			}
			inst->uiState.updatePatternEditor = true;
			break;
		
		case FT2_KEY_DOWN:
			if (modifiers & FT2_MOD_SHIFT)
			{
				/* Shift+Down = increase current instrument */
				if (inst->editor.curInstr < 127)
					inst->editor.curInstr++;
			}
			else if (modifiers & FT2_MOD_ALT)
			{
				/* Alt+Down = mark pattern downward */
				keybPattMarkDown(inst);
			}
			else
			{
				/* Row navigation - update song.row (source of truth for pattern editor) */
				uint16_t numRows = inst->replayer.song.currNumRows;
				if (numRows == 0) numRows = 64;
				
				if (inst->replayer.song.row < numRows - 1)
					inst->replayer.song.row++;
				else
					inst->replayer.song.row = 0; /* Wrap to top */
				
				/* Sync editor.row when not playing */
				if (!inst->replayer.songPlaying)
					inst->editor.row = (uint8_t)inst->replayer.song.row;
			}
			inst->uiState.updatePatternEditor = true;
			break;
		
		case FT2_KEY_LEFT:
			if (modifiers & FT2_MOD_SHIFT)
			{
				/* Shift+Left = decrease song position */
				if (inst->replayer.song.songPos > 0)
				{
					inst->replayer.song.songPos--;
					inst->editor.editPattern = inst->replayer.song.orders[inst->replayer.song.songPos];
				}
			}
			else if (modifiers & FT2_MOD_CTRL)
			{
				/* Ctrl+Left = decrease pattern number */
				if (inst->editor.editPattern > 0)
					inst->editor.editPattern--;
			}
			else if (modifiers & FT2_MOD_ALT)
			{
				/* Alt+Left = mark pattern left */
				keybPattMarkLeft(inst);
			}
			else
			{
				/* Move cursor column left */
				if (inst->cursor.object > 0)
					inst->cursor.object--;
				else if (inst->cursor.ch > 0)
				{
					inst->cursor.ch--;
					inst->cursor.object = CURSOR_EFX2; /* Last column in channel */
				}
			}
			inst->uiState.updatePatternEditor = true;
			break;
		
		case FT2_KEY_RIGHT:
			if (modifiers & FT2_MOD_SHIFT)
			{
				/* Shift+Right = increase song position */
				if (inst->replayer.song.songPos < inst->replayer.song.songLength - 1)
				{
					inst->replayer.song.songPos++;
					inst->editor.editPattern = inst->replayer.song.orders[inst->replayer.song.songPos];
				}
			}
			else if (modifiers & FT2_MOD_CTRL)
			{
				/* Ctrl+Right = increase pattern number */
				if (inst->editor.editPattern < 255)
					inst->editor.editPattern++;
			}
			else if (modifiers & FT2_MOD_ALT)
			{
				/* Alt+Right = mark pattern right */
				keybPattMarkRight(inst);
			}
			else
			{
				/* Move cursor column right */
				if (inst->cursor.object < CURSOR_EFX2)
					inst->cursor.object++;
				else if (inst->cursor.ch < inst->replayer.song.numChannels - 1)
				{
					inst->cursor.ch++;
					inst->cursor.object = CURSOR_NOTE; /* First column in next channel */
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
			/* Tab = next channel, Shift+Tab = prev channel */
			if (modifiers & FT2_MOD_SHIFT)
				cursorTabLeft(inst);
			else
				cursorTabRight(inst);
			break;
		
		default:
			break;
	}
}

static void handleOctaveKeys(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (inst == NULL || input == NULL)
		return;
	
	/* F1/F2 for octave up/down, but not with modifier keys (those are for transpose) */
	if (modifiers & (FT2_MOD_SHIFT | FT2_MOD_CTRL | FT2_MOD_ALT))
		return;
	
	switch (keyCode)
	{
		case FT2_KEY_F1:
			if (input->octave > 0)
			{
				input->octave--;
				inst->editor.curOctave = input->octave;
			}
			break;
		
		case FT2_KEY_F2:
			if (input->octave < 7)
			{
				input->octave++;
				inst->editor.curOctave = input->octave;
			}
			break;
		
		default:
			break;
	}
}

/**
 * Handle numpad keys for instrument bank switching.
 * @return true if the key was handled (prevent further processing)
 */
static bool handleNumpadInstrumentKeys(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (inst == NULL || input == NULL)
		return false;
	
	/* Check for numpad plus held state - only allow bank selection keys */
	if (input->numPadPlusPressed && !(modifiers & FT2_MOD_CTRL))
	{
		if (keyCode != FT2_KEY_NUMLOCK && keyCode != FT2_KEY_NUMPAD_DIVIDE &&
			keyCode != FT2_KEY_NUMPAD_MULTIPLY && keyCode != FT2_KEY_NUMPAD_MINUS)
		{
			return false;
		}
	}
	
	switch (keyCode)
	{
		case FT2_KEY_NUMPAD_ENTER:
			/* Toggle instrument bank (1-64 vs 65-128) */
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
			/* Clear instrument/sample - TODO: Implement clear instrument dialog */
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

static void handleEditSkipKey(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL)
		return;
	
	/* Grave/backtick key for edit skip adjustment */
	if (keyCode == '`' || keyCode == '~')
	{
		/* Note off is handled elsewhere - this is for when in edit mode with no note col */
		if (modifiers & FT2_MOD_SHIFT)
		{
			/* Decrease edit skip */
			if (inst->editor.editRowSkip == 0)
				inst->editor.editRowSkip = 16;
			else
				inst->editor.editRowSkip--;
		}
		else
		{
			/* Increase edit skip */
			if (inst->editor.editRowSkip == 16)
				inst->editor.editRowSkip = 0;
			else
				inst->editor.editRowSkip++;
		}
	}
}

static bool handleModifiedKeys(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL)
		return false;
	
	bool ctrlDown = (modifiers & FT2_MOD_CTRL) != 0;
	bool altDown = (modifiers & FT2_MOD_ALT) != 0;
	bool shiftDown = (modifiers & FT2_MOD_SHIFT) != 0;
	
	/* Ctrl+key combinations */
	if (ctrlDown && !altDown && !shiftDown)
	{
		switch (keyCode)
		{
			case 'a':
			case 'A':
				/* Ctrl+A = show advanced edit */
				inst->uiState.advEditShown = !inst->uiState.advEditShown;
				return true;
			
			case 'b':
			case 'B':
				/* Ctrl+B = show about screen */
				if (!inst->uiState.aboutScreenShown)
				{
					inst->uiState.aboutScreenShown = true;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
				}
				return true;
			
			case 'c':
			case 'C':
				/* Ctrl+C = copy (pattern) or show config */
				if (inst->uiState.sampleEditorShown)
				{
					/* Sample copy - handled elsewhere */
				}
				else
				{
					inst->uiState.configScreenShown = !inst->uiState.configScreenShown;
					if (inst->uiState.configScreenShown)
					{
						inst->uiState.aboutScreenShown = false;
						inst->uiState.helpScreenShown = false;
					}
				}
				return true;
			
			case 'd':
			case 'D':
				/* Ctrl+D = disk operations */
				inst->uiState.diskOpShown = !inst->uiState.diskOpShown;
				return true;
			
			case 'e':
			case 'E':
				/* Ctrl+E = sample editor extended */
				if (!inst->uiState.sampleEditorExtShown)
				{
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
					inst->uiState.sampleEditorExtShown = true;
				}
				return true;
			
			case 'h':
			case 'H':
				/* Ctrl+H = help screen */
				inst->uiState.helpScreenShown = !inst->uiState.helpScreenShown;
				if (inst->uiState.helpScreenShown)
				{
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
				}
				return true;
			
			case 'i':
			case 'I':
				/* Ctrl+I = instrument editor */
				if (!inst->uiState.instEditorShown)
				{
					inst->uiState.patternEditorShown = false;
					inst->uiState.sampleEditorShown = false;
					inst->uiState.instEditorShown = true;
				}
				return true;
			
			case 'm':
			case 'M':
				/* Ctrl+M = instrument editor extended */
				if (!inst->uiState.instEditorExtShown)
				{
					inst->uiState.aboutScreenShown = false;
					inst->uiState.configScreenShown = false;
					inst->uiState.helpScreenShown = false;
					inst->uiState.instEditorExtShown = true;
				}
				return true;
			
			case 'p':
			case 'P':
				/* Ctrl+P = pattern editor */
				if (!inst->uiState.patternEditorShown)
				{
					inst->uiState.sampleEditorShown = false;
					inst->uiState.sampleEditorExtShown = false;
					inst->uiState.instEditorShown = false;
					inst->uiState.patternEditorShown = true;
				}
				return true;
			
			case 's':
			case 'S':
				/* Ctrl+S = sample editor */
				if (!inst->uiState.sampleEditorShown)
				{
					inst->uiState.patternEditorShown = false;
					inst->uiState.instEditorShown = false;
					inst->uiState.sampleEditorShown = true;
				}
				return true;
			
			case 't':
			case 'T':
				/* Ctrl+T = transpose */
				inst->uiState.transposeShown = !inst->uiState.transposeShown;
				return true;
			
			case 'x':
			case 'X':
				/* Ctrl+X = restore to default view */
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
			
			case 'z':
			case 'Z':
				/* Ctrl+Z = toggle extended pattern editor */
				inst->uiState.extendedPatternEditor = !inst->uiState.extendedPatternEditor;
				return true;
			
			default:
				break;
		}
	}
	
	/* Alt+key combinations for channel jumping */
	if (altDown && !ctrlDown && !shiftDown)
	{
		/* Alt+F3/F4/F5 for block operations */
		switch (keyCode)
		{
			case FT2_KEY_F3:
				cutBlock(inst);
				return true;
			
			case FT2_KEY_F4:
				copyBlock(inst);
				return true;
			
			case FT2_KEY_F5:
				pasteBlock(inst);
				return true;
			
			default:
				break;
		}
		
		/* Alt+letter for channel jumping */
		int channel = -1;
		switch (keyCode)
		{
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
		
		if (channel >= 0 && channel < inst->replayer.song.numChannels)
		{
			inst->cursor.ch = (uint8_t)channel;
			inst->cursor.object = CURSOR_NOTE;
			inst->uiState.updatePatternEditor = true;
			return true;
		}
	}
	
	return false;
}

static void handlePatternInsertDelete(ft2_instance_t *inst, int keyCode, int modifiers)
{
	if (inst == NULL)
		return;
	
	bool inEditMode = (inst->replayer.playMode == FT2_PLAYMODE_EDIT);
	if (!inEditMode)
		return;
	
	uint16_t patt = inst->editor.editPattern;
	if (patt >= 256)
		return;
	
	uint16_t numRows = inst->replayer.patternNumRows[patt];
	uint16_t numCh = inst->replayer.song.numChannels;
	ft2_note_t *pattern = inst->replayer.pattern[patt];
	
	int16_t curRow = inst->replayer.song.row;
	if (curRow < 0) curRow = 0;
	
	switch (keyCode)
	{
		case FT2_KEY_INSERT:
			if (pattern != NULL)
			{
				if (modifiers & FT2_MOD_SHIFT)
				{
					/* Insert line - shift all data down in all channels */
					for (int16_t row = numRows - 2; row >= curRow; row--)
					{
						for (uint8_t ch = 0; ch < numCh; ch++)
						{
							pattern[(row + 1) * numCh + ch] = pattern[row * numCh + ch];
						}
					}
					/* Clear current row */
					for (uint8_t ch = 0; ch < numCh; ch++)
					{
						memset(&pattern[curRow * numCh + ch], 0, sizeof(ft2_note_t));
					}
				}
				else
				{
					/* Insert note - shift only current channel */
					for (int16_t row = numRows - 2; row >= curRow; row--)
					{
						pattern[(row + 1) * numCh + inst->cursor.ch] = 
							pattern[row * numCh + inst->cursor.ch];
					}
					memset(&pattern[curRow * numCh + inst->cursor.ch], 0, sizeof(ft2_note_t));
				}
				inst->uiState.updatePatternEditor = true;
			}
			break;
		
		case FT2_KEY_BACKSPACE:
			if (pattern != NULL && curRow > 0)
			{
				if (modifiers & FT2_MOD_SHIFT)
				{
					/* Delete line - shift all data up in all channels */
					inst->replayer.song.row--;
					curRow = inst->replayer.song.row;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)curRow;
					
					for (uint16_t row = curRow; row < numRows - 1; row++)
					{
						for (uint8_t ch = 0; ch < numCh; ch++)
						{
							pattern[row * numCh + ch] = pattern[(row + 1) * numCh + ch];
						}
					}
					/* Clear last row */
					for (uint8_t ch = 0; ch < numCh; ch++)
					{
						memset(&pattern[(numRows - 1) * numCh + ch], 0, sizeof(ft2_note_t));
					}
				}
				else
				{
					/* Delete note - shift only current channel */
					inst->replayer.song.row--;
					curRow = inst->replayer.song.row;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)curRow;
					
					for (uint16_t row = curRow; row < numRows - 1; row++)
					{
						pattern[row * numCh + inst->cursor.ch] = 
							pattern[(row + 1) * numCh + inst->cursor.ch];
					}
					memset(&pattern[(numRows - 1) * numCh + inst->cursor.ch], 0, sizeof(ft2_note_t));
				}
				inst->uiState.updatePatternEditor = true;
			}
			break;
		
		case FT2_KEY_DELETE:
			if (pattern != NULL)
			{
				ft2_note_t *n = &pattern[curRow * numCh + inst->cursor.ch];
				
				if (modifiers & FT2_MOD_SHIFT)
				{
					/* Delete all columns */
					memset(n, 0, sizeof(ft2_note_t));
				}
				else if (modifiers & FT2_MOD_CTRL)
				{
					/* Delete volume + effect */
					n->vol = 0;
					n->efx = 0;
					n->efxData = 0;
				}
				else if (modifiers & FT2_MOD_ALT)
				{
					/* Delete effect only */
					n->efx = 0;
					n->efxData = 0;
				}
				else
				{
					/* Delete based on cursor position */
					if (inst->cursor.object == CURSOR_VOL1 || inst->cursor.object == CURSOR_VOL2)
					{
						n->vol = 0;
					}
					else
					{
						n->note = 0;
						n->instr = 0;
					}
				}
				
				/* Advance cursor after delete */
				if (inst->editor.editRowSkip > 0)
				{
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				
				inst->uiState.updatePatternEditor = true;
			}
			break;
		
		default:
			break;
	}
}

static void handleNoteInput(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode)
{
	if (inst == NULL || input == NULL)
		return;
	
	/* Get UI for access to scopes (multiRecChn, channelMuted) */
	ft2_ui_t *ui = ft2_ui_get_current();
	bool *multiRecChn = (ui != NULL) ? ui->scopes.multiRecChn : NULL;
	bool *channelMuted = (ui != NULL) ? ui->scopes.channelMuted : NULL;
	
	/* Determine play/edit mode */
	bool editMode = inst->uiState.patternEditorShown && 
	                (inst->replayer.playMode == FT2_PLAYMODE_EDIT);
	bool recMode = (inst->replayer.playMode == FT2_PLAYMODE_RECSONG) ||
	               (inst->replayer.playMode == FT2_PLAYMODE_RECPATT);
	
	uint16_t numChannels = inst->replayer.song.numChannels;
	if (numChannels > 32) numChannels = 32;
	
	/* Check for note off key (backtick) */
	if (keyCode == '`' || keyCode == '~')
	{
		if ((editMode || recMode) && inst->uiState.patternEditorShown)
		{
			/* Find channel to insert note off */
			int8_t c = inst->cursor.ch;
			
			/* In multi-edit/rec mode, find channel from multiRecChn */
			if (((inst->config.multiEdit && editMode) || (inst->config.multiRec && recMode)) && multiRecChn)
			{
				int32_t time = 0x7FFFFFFF;
				for (int8_t i = 0; i < (int8_t)numChannels; i++)
				{
					if ((!channelMuted || !channelMuted[i]) && multiRecChn[i] && 
					    input->keyOffTime[i] < time && input->keyOnTab[i] == 0)
					{
						c = i;
						time = input->keyOffTime[i];
					}
				}
			}
			
			if (c < 0 || c >= (int8_t)numChannels)
				return;
			
			uint16_t patt = inst->editor.editPattern;
			if (!allocatePattern(inst, patt))
				return;
			
			int16_t row = inst->replayer.song.row;
			if (row >= 0 && row < inst->replayer.patternNumRows[patt])
			{
				ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + c];
				n->note = 97; /* Note off */
				n->instr = 0;
				
				/* In edit mode (not record), advance row */
				if (!recMode && inst->editor.editRowSkip > 0)
				{
					uint16_t numRows = inst->replayer.patternNumRows[patt];
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				
				inst->uiState.updatePatternEditor = true;
			}
		}
		return;
	}
	
	int8_t noteNum = ft2_key_to_note(keyCode, input->octave);
	if (noteNum <= 0)
		return;
	
	int8_t c = -1; /* Channel to play/record on */
	int8_t k = -1; /* Channel where this note is already held */
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
		return;
	
	/* Track key-on on selected channel */
	input->keyOnTab[c] = noteNum;
		
	/* Trigger the note for playback */
	ft2_instance_trigger_note(inst, noteNum, inst->editor.curInstr, c, 64);
		
	/* Record note to pattern in edit/record mode */
	if (editMode || recMode)
		{
			uint16_t patt = inst->editor.editPattern;
			if (!allocatePattern(inst, patt))
				return;
			
			int16_t row = inst->replayer.song.row;
		if (c < (int8_t)numChannels && row >= 0 && row < inst->replayer.patternNumRows[patt])
			{
			ft2_note_t *n = &inst->replayer.pattern[patt][row * FT2_MAX_CHANNELS + c];
			n->note = noteNum;
			if (inst->editor.curInstr > 0)
				n->instr = inst->editor.curInstr;
				
			/* In edit mode (not record), advance row */
			if (!recMode && inst->editor.editRowSkip > 0)
				{
					uint16_t numRows = inst->replayer.patternNumRows[patt];
					inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
					if (!inst->replayer.songPlaying)
						inst->editor.row = (uint8_t)inst->replayer.song.row;
				}
				
				inst->uiState.updatePatternEditor = true;
		}
	}
}

static int8_t hexCharToValue(int keyCode)
{
	if (keyCode >= '0' && keyCode <= '9')
		return (int8_t)(keyCode - '0');
	if (keyCode >= 'a' && keyCode <= 'f')
		return (int8_t)(10 + keyCode - 'a');
	if (keyCode >= 'A' && keyCode <= 'F')
		return (int8_t)(10 + keyCode - 'A');
	return -1;
}

/* Volume column effect key mapping (matches FT2's key2VolTab):
 * 0-4 = volume 0x10-0x50, - = vol slide down, + = vol slide up
 * d = fine vol down, u = fine vol up, s = set vibrato speed
 * v = vibrato, p = set panning, l = pan slide left, r = pan slide right
 * m = tone portamento
 */
static int8_t volKeyToValue(int keyCode)
{
	switch (keyCode)
	{
		case '0': return 0;  /* Volume 00 (0x10) */
		case '1': return 1;  /* Volume 10 (0x20) */
		case '2': return 2;  /* Volume 20 (0x30) */
		case '3': return 3;  /* Volume 30 (0x40) */
		case '4': return 4;  /* Volume 40 (0x50) */
		case '-': return 5;  /* Volume slide down (0x60) */
		case '+':
		case '=': return 6;  /* Volume slide up (0x70) */
		case 'd':
		case 'D': return 7;  /* Fine volume down (0x80) */
		case 'u':
		case 'U': return 8;  /* Fine volume up (0x90) */
		case 's':
		case 'S': return 9;  /* Set vibrato speed (0xA0) */
		case 'v':
		case 'V': return 10; /* Vibrato (0xB0) */
		case 'p':
		case 'P': return 11; /* Set panning (0xC0) */
		case 'l':
		case 'L': return 12; /* Panning slide left (0xD0) */
		case 'r':
		case 'R': return 13; /* Panning slide right (0xE0) */
		case 'm':
		case 'M': return 14; /* Tone portamento (0xF0) */
		default: return -1;
	}
}

/* Effect type key mapping (matches FT2's key2EfxTab - A-Z plus 0-9) */
static int8_t efxKeyToValue(int keyCode)
{
	if (keyCode >= '0' && keyCode <= '9')
		return (int8_t)(keyCode - '0');
	if (keyCode >= 'a' && keyCode <= 'z')
		return (int8_t)(10 + keyCode - 'a');
	if (keyCode >= 'A' && keyCode <= 'Z')
		return (int8_t)(10 + keyCode - 'A');
	return -1;
}

static void handleEffectInput(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode)
{
	int8_t val;
	
	if (inst == NULL || input == NULL)
		return;
	
	if (inst->replayer.playMode != FT2_PLAYMODE_EDIT &&
	    inst->replayer.playMode != FT2_PLAYMODE_RECSONG &&
	    inst->replayer.playMode != FT2_PLAYMODE_RECPATT)
		return;
	
	if (!inst->uiState.patternEditorShown)
		return;
	
	/* Determine the key value based on cursor position */
	if (inst->cursor.object == CURSOR_VOL1)
	{
		/* Volume column effect type uses special key mapping */
		val = volKeyToValue(keyCode);
	}
	else if (inst->cursor.object == CURSOR_EFX0)
	{
		/* Effect type uses extended key mapping (0-9, A-Z) */
		val = efxKeyToValue(keyCode);
	}
	else
	{
		/* All other columns use hex input (0-9, A-F) */
		val = hexCharToValue(keyCode);
	}
	
	if (val < 0)
		return;
	
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
	
	/* Determine which column we're editing based on cursor.object */
	switch (inst->cursor.object)
	{
		case CURSOR_NOTE: /* Note column - handled by handleNoteInput */
			break;
		
		case CURSOR_INST1: /* Instrument high nibble */
			n->instr = (n->instr & 0x0F) | (uint8_t)(val << 4);
			if (n->instr > 127) n->instr = 127;
			break;
		
		case CURSOR_INST2: /* Instrument low nibble */
			n->instr = (n->instr & 0xF0) | (uint8_t)val;
			if (n->instr > 127) n->instr = 127;
			break;
		
		case CURSOR_VOL1: /* Volume column effect type */
			n->vol = (n->vol & 0x0F) | (uint8_t)((val + 1) << 4);
			if (n->vol >= 0x51 && n->vol <= 0x5F)
				n->vol = 0x50;
			break;
		
		case CURSOR_VOL2: /* Volume column param nibble */
			if (n->vol < 0x10)
				n->vol = 0x10 + (uint8_t)val;
			else
				n->vol = (n->vol & 0xF0) | (uint8_t)val;
			if (n->vol >= 0x51 && n->vol <= 0x5F)
				n->vol = 0x50;
			break;
		
		case CURSOR_EFX0: /* Effect type */
			n->efx = (uint8_t)val;
			break;
		
		case CURSOR_EFX1: /* Effect param high nibble */
			n->efxData = (n->efxData & 0x0F) | (uint8_t)(val << 4);
			break;
		
		case CURSOR_EFX2: /* Effect param low nibble */
			n->efxData = (n->efxData & 0xF0) | (uint8_t)val;
			break;
		
		default:
			return;
	}
	
	/* Advance row (matching original FT2 behavior) */
	uint16_t numRows = inst->replayer.patternNumRows[patt];
	if (numRows >= 1 && inst->editor.editRowSkip > 0)
	{
		inst->replayer.song.row = (inst->replayer.song.row + inst->editor.editRowSkip) % numRows;
		if (!inst->replayer.songPlaying)
			inst->editor.row = (uint8_t)inst->replayer.song.row;
	}
	
	inst->uiState.updatePatternEditor = true;
}

void ft2_input_key_down(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (input == NULL)
		return;

	if (keyCode >= 0 && keyCode < 512)
		input->keyDown[keyCode] = true;

	input->lastKeyPressed = keyCode;
	input->modifiers = (uint8_t)modifiers;
	
	if (inst == NULL)
		return;
	
	/* Handle nibbles input when playing - block ALL keys from other handlers */
	if (inst->uiState.nibblesShown && inst->nibbles.playing)
	{
		ft2_nibbles_handle_key(inst, keyCode);
		return; /* Always return - matches standalone behavior */
	}
	
	/* Track numpad plus state */
	if (keyCode == FT2_KEY_NUMPAD_PLUS)
		input->numPadPlusPressed = true;
	
	/* Handle modified keys first (Ctrl, Alt, Shift combos) */
	if (modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT))
	{
		if (handleModifiedKeys(inst, keyCode, modifiers))
			return;
	}
	
	/* Handle playback control keys */
	handlePlaybackKeys(inst, keyCode, modifiers);
	
	/* Handle numpad instrument bank keys - if handled, don't process as note/effect input */
	if (handleNumpadInstrumentKeys(inst, input, keyCode, modifiers))
		return;
	
	/* Handle octave keys (F1/F2 without modifiers) */
	handleOctaveKeys(inst, input, keyCode, modifiers);
	
	/* Handle pattern navigation */
	handleNavigationKeys(inst, keyCode, modifiers);
	
	/* Handle edit skip adjustment */
	handleEditSkipKey(inst, keyCode, modifiers);
	
	/* Handle pattern insert/delete keys */
	handlePatternInsertDelete(inst, keyCode, modifiers);
	
	/* Handle note input (only on note column) or effect input (on other columns) */
	if (!(modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT | FT2_MOD_CMD)))
	{
		if (inst->cursor.object == CURSOR_NOTE)
		{
			/* Note column - handle note input */
			handleNoteInput(inst, input, keyCode);
		}
		else
		{
			/* Effect/instrument/volume columns - handle hex input */
			handleEffectInput(inst, input, keyCode);
		}
	}
}

void ft2_input_key_up(ft2_instance_t *inst, ft2_input_state_t *input, int keyCode, int modifiers)
{
	if (input == NULL)
		return;

	if (keyCode >= 0 && keyCode < 512)
		input->keyDown[keyCode] = false;

	input->modifiers = (uint8_t)modifiers;
	
	/* Track numpad plus state */
	if (keyCode == FT2_KEY_NUMPAD_PLUS)
		input->numPadPlusPressed = false;
	
	/* Ignore key up if flagged */
	if (input->ignoreCurrKeyUp)
	{
		input->ignoreCurrKeyUp = false;
		return;
	}
	
	/* Release note on key up (for proper note-off in live play) */
	if (inst != NULL && !(modifiers & (FT2_MOD_CTRL | FT2_MOD_ALT | FT2_MOD_CMD)))
	{
		int8_t note = ft2_key_to_note(keyCode, input->octave);
		if (note > 0)
		{
			/* Find which channel has this note */
			for (uint8_t ch = 0; ch < 32; ch++)
			{
				if (input->keyOnTab[ch] == note)
				{
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

void ft2_input_mouse_down(ft2_input_state_t *input, int x, int y, int button)
{
	if (input == NULL)
		return;

	input->mouseX = x;
	input->mouseY = y;
	input->mouseButtons |= (uint8_t)(1 << button);
	input->mouseDragging = true;
}

void ft2_input_mouse_up(ft2_input_state_t *input, int x, int y, int button)
{
	if (input == NULL)
		return;

	input->mouseX = x;
	input->mouseY = y;
	input->mouseButtons &= ~(uint8_t)(1 << button);
	input->mouseDragging = false;
}

void ft2_input_mouse_move(ft2_input_state_t *input, int x, int y)
{
	if (input == NULL)
		return;

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
