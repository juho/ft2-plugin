/**
 * @file ft2_plugin_radiobuttons.c
 * @brief Radio button implementation for the FT2 plugin UI.
 * 
 * Ported from ft2_radiobuttons.c - exact coordinates preserved.
 * Modified for multi-instance support: visibility/state stored per-instance.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_callbacks.h"
#include "ft2_plugin_palette.h"

radioButton_t radioButtons[NUM_RADIOBUTTONS] =
{
	/* Help screen */
	/*x, y,   w,  group,         callback */
	{ 5, 18,  69, RB_GROUP_HELP, NULL },
	{ 5, 34,  60, RB_GROUP_HELP, NULL },
	{ 5, 50,  86, RB_GROUP_HELP, NULL },
	{ 5, 66, 109, RB_GROUP_HELP, NULL },
	{ 5, 82, 101, RB_GROUP_HELP, NULL },
	{ 5, 98,  86, RB_GROUP_HELP, NULL },

	/* Nibbles */
	/*x,  y,   w,  group,                       callback */
	{  4, 105, 61, RB_GROUP_NIBBLES_PLAYERS,    NULL },
	{  4, 119, 68, RB_GROUP_NIBBLES_PLAYERS,    NULL },
	{ 79, 117, 55, RB_GROUP_NIBBLES_DIFFICULTY, NULL },
	{ 79, 131, 63, RB_GROUP_NIBBLES_DIFFICULTY, NULL },
	{ 79, 145, 34, RB_GROUP_NIBBLES_DIFFICULTY, NULL },
	{ 79, 159, 50, RB_GROUP_NIBBLES_DIFFICULTY, NULL },

	/* Sample editor */
	/*x,   y,   w,  group,                 callback */
	{ 357, 351, 58, RB_GROUP_SAMPLE_LOOP,  NULL },
	{ 357, 368, 62, RB_GROUP_SAMPLE_LOOP,  NULL },
	{ 357, 385, 67, RB_GROUP_SAMPLE_LOOP,  NULL },
	{ 431, 368, 44, RB_GROUP_SAMPLE_DEPTH, NULL },
	{ 431, 383, 50, RB_GROUP_SAMPLE_DEPTH, NULL },

	/* Instrument editor */
	/*x,   y,   w,  group,                  callback */
	{ 442, 279, 25, RB_GROUP_INST_WAVEFORM, NULL },
	{ 472, 279, 25, RB_GROUP_INST_WAVEFORM, NULL },
	{ 502, 279, 25, RB_GROUP_INST_WAVEFORM, NULL },
	{ 532, 279, 25, RB_GROUP_INST_WAVEFORM, NULL },

	/* Config screen select */
	/*x, y,  w,  group,                  callback */
	{ 5, 18, 48, RB_GROUP_CONFIG_SELECT, NULL },
	{ 5, 34, 57, RB_GROUP_CONFIG_SELECT, NULL },
	{ 5, 50, 97, RB_GROUP_CONFIG_SELECT, NULL },
	{ 5, 66, 72, RB_GROUP_CONFIG_SELECT, NULL },
	{ 5, 82, 80, RB_GROUP_CONFIG_SELECT, NULL },

	/* Config audio buffer size */
	/*x,   y,  w,   group,                           callback */
	{ 390, 16,  45, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, NULL },
	{ 390, 30, 112, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, NULL },
	{ 390, 44,  49, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, NULL },

	/* Config audio bit depth */
	/*x,   y,  w,  group,                           callback */
	{ 390, 73, 51, RB_GROUP_CONFIG_AUDIO_BIT_DEPTH, NULL },
	{ 453, 73, 51, RB_GROUP_CONFIG_AUDIO_BIT_DEPTH, NULL },

	/* Config audio interpolation */
	/*x,   y,   w,   group,                               callback */
	{ 390,  90, 108, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },
	{ 390, 104,  90, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },
	{ 390, 118, 109, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },
	{ 390, 132,  85, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },
	{ 390, 146,  94, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },
	{ 390, 160, 101, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, NULL },

	/* Config audio frequency */
	/*x,   y,  w,  group,                      callback */
	{ 513, 16, 65, RB_GROUP_CONFIG_AUDIO_FREQ, NULL },
	{ 513, 30, 65, RB_GROUP_CONFIG_AUDIO_FREQ, NULL },
	{ 513, 44, 65, RB_GROUP_CONFIG_AUDIO_FREQ, NULL },

	/* Config audio input frequency */
	/*x,   y,   w,  group,                            callback */
	{ 180, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, NULL },
	{ 251, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, NULL },
	{ 322, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, NULL },

	/* Config frequency slides */
	/*x,   y,  w,   group,                       callback */
	{ 513, 74,  49, RB_GROUP_CONFIG_FREQ_SLIDES, NULL },
	{ 513, 88, 107, RB_GROUP_CONFIG_FREQ_SLIDES, NULL },

	/* Config mouse */
	/*x,   y,   w,  group,                 callback */
	{ 115, 120, 41, RB_GROUP_CONFIG_MOUSE, NULL },
	{ 178, 120, 41, RB_GROUP_CONFIG_MOUSE, NULL },
	{ 115, 134, 47, RB_GROUP_CONFIG_MOUSE, NULL },
	{ 178, 134, 55, RB_GROUP_CONFIG_MOUSE, NULL },

	/* Config mouse busy */
	/*x,   y,   w,  group,                      callback */
	{ 115, 159, 51, RB_GROUP_CONFIG_MOUSE_BUSY, NULL },
	{ 178, 159, 45, RB_GROUP_CONFIG_MOUSE_BUSY, NULL },

	/* Config scope */
	/*x,   y,   w,  group,                 callback */
	{ 305, 145, 38, RB_GROUP_CONFIG_SCOPE, NULL },
	{ 346, 145, 46, RB_GROUP_CONFIG_SCOPE, NULL },

	/* Config pattern channels */
	/*x,   y,  w,  group,                         callback */
	{ 257, 42, 78, RB_GROUP_CONFIG_PATTERN_CHANS, NULL },
	{ 257, 56, 78, RB_GROUP_CONFIG_PATTERN_CHANS, NULL },
	{ 257, 70, 78, RB_GROUP_CONFIG_PATTERN_CHANS, NULL },
	{ 257, 84, 85, RB_GROUP_CONFIG_PATTERN_CHANS, NULL },

	/* Config font */
	/*x,   y,   w,  group,                callback */
	{ 257, 114, 62, RB_GROUP_CONFIG_FONT, NULL },
	{ 323, 114, 68, RB_GROUP_CONFIG_FONT, NULL },
	{ 257, 129, 54, RB_GROUP_CONFIG_FONT, NULL },
	{ 323, 129, 40, RB_GROUP_CONFIG_FONT, NULL },

	/* Config palette entries */
	/*x,   y,  w,  group,                       callback */
	{ 399,  2, 88, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },
	{ 399, 16, 79, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },
	{ 399, 30, 97, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },
	{ 399, 44, 52, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },
	{ 399, 58, 63, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },
	{ 399, 72, 61, RB_GROUP_CONFIG_PAL_ENTRIES, NULL },

	/* Config palette presets */
	/*x,   y,   w,   group,                      callback */
	{ 399,  89,  50, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512,  89,  81, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 399, 103, 105, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512, 103,  45, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 399, 117,  47, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512, 117,  77, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 399, 131,  40, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512, 131,  56, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 399, 145,  87, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512, 145,  87, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 399, 159,  54, RB_GROUP_CONFIG_PAL_PRESET, NULL },
	{ 512, 159,  90, RB_GROUP_CONFIG_PAL_PRESET, NULL },

	/* Config filesort */
	/*x,   y,  w,  group,                    callback */
	{ 114, 15, 40, RB_GROUP_CONFIG_FILESORT, NULL },
	{ 114, 29, 48, RB_GROUP_CONFIG_FILESORT, NULL },

	/* Config window size */
	/*x,   y,  w,  group,                    callback */
	{ 114, 58, 60, RB_GROUP_CONFIG_WIN_SIZE, NULL },
	{ 114, 72, 31, RB_GROUP_CONFIG_WIN_SIZE, NULL },
	{ 156, 72, 31, RB_GROUP_CONFIG_WIN_SIZE, NULL },
	{ 114, 86, 31, RB_GROUP_CONFIG_WIN_SIZE, NULL },
	{ 156, 86, 31, RB_GROUP_CONFIG_WIN_SIZE, NULL },

	/* Disk op item */
	/*x, y,  w,  group,                callback */
	{ 4, 16, 55, RB_GROUP_DISKOP_ITEM, NULL },
	{ 4, 30, 45, RB_GROUP_DISKOP_ITEM, NULL },
	{ 4, 44, 56, RB_GROUP_DISKOP_ITEM, NULL },
	{ 4, 58, 59, RB_GROUP_DISKOP_ITEM, NULL },
	{ 4, 72, 50, RB_GROUP_DISKOP_ITEM, NULL },

	/* Disk op module save as (MOD and XM only - no WAV export for plugins) */
	/*x, y,  w,   group,                      callback */
	{ 4, 100, 40, RB_GROUP_DISKOP_MOD_SAVEAS, NULL },
	{ 4, 114, 33, RB_GROUP_DISKOP_MOD_SAVEAS, NULL },
	{ 0,   0,  0, NUM_RB_GROUPS, NULL },  /* RB_DISKOP_MOD_WAV - disabled in plugin */

	/* Disk op instrument save as */
	/*x, y,   w,  group,                      callback */
	{ 4, 100, 29, RB_GROUP_DISKOP_INS_SAVEAS, NULL },

	/* Disk op sample save as */
	/*x, y,   w,  group,                      callback */
	{ 4, 100, 40, RB_GROUP_DISKOP_SMP_SAVEAS, NULL },
	{ 4, 114, 34, RB_GROUP_DISKOP_SMP_SAVEAS, NULL },
	{ 4, 128, 40, RB_GROUP_DISKOP_SMP_SAVEAS, NULL },

	/* Disk op pattern save as */
	/*x, y,   w,  group,                      callback */
	{ 4, 100, 33, RB_GROUP_DISKOP_PAT_SAVEAS, NULL },

	/* Disk op track save as */
	/*x, y,   w,  group,                      callback */
	{ 4, 100, 31, RB_GROUP_DISKOP_TRK_SAVEAS, NULL },

	/* WAV render bitdepth */
	/*x,   y,  w,  group,                        callback */
	{ 130, 95, 52, RB_GROUP_WAV_RENDER_BITDEPTH, NULL },
	{ 195, 95, 93, RB_GROUP_WAV_RENDER_BITDEPTH, NULL },

	/* Config MIDI trigger mode */
	/*x,   y,    w,  group,                         callback */
	{ 182, 120, 48, RB_GROUP_CONFIG_MIDI_TRIGGER, NULL },  /* RB_CONFIG_MIDI_NOTES */
	{ 245, 120, 65, RB_GROUP_CONFIG_MIDI_TRIGGER, NULL }   /* RB_CONFIG_MIDI_PATTERNS */
};

void initRadioButtons(void)
{
	/* Initialize callbacks only (visibility/state now per-instance in ft2_widgets_t) */

	/* Wire up config screen tab callbacks */
	radioButtons[RB_CONFIG_AUDIO].callbackFunc = rbConfigAudio;
	radioButtons[RB_CONFIG_LAYOUT].callbackFunc = rbConfigLayout;
	radioButtons[RB_CONFIG_MISC].callbackFunc = rbConfigMiscellaneous;
	radioButtons[RB_CONFIG_IO_ROUTING].callbackFunc = rbConfigIORouting;
	radioButtons[RB_CONFIG_MIDI].callbackFunc = rbConfigMidiInput;

	/* Wire up interpolation callbacks */
	radioButtons[RB_CONFIG_AUDIO_INTRP_NONE].callbackFunc = rbConfigIntrpNone;
	radioButtons[RB_CONFIG_AUDIO_INTRP_LINEAR].callbackFunc = rbConfigIntrpLinear;
	radioButtons[RB_CONFIG_AUDIO_INTRP_QUADRATIC].callbackFunc = rbConfigIntrpQuadratic;
	radioButtons[RB_CONFIG_AUDIO_INTRP_CUBIC].callbackFunc = rbConfigIntrpCubic;
	radioButtons[RB_CONFIG_AUDIO_INTRP_SINC8].callbackFunc = rbConfigIntrpSinc8;
	radioButtons[RB_CONFIG_AUDIO_INTRP_SINC16].callbackFunc = rbConfigIntrpSinc16;

	/* Wire up scope style callbacks */
	radioButtons[RB_CONFIG_SCOPE_STANDARD].callbackFunc = rbConfigScopeFT2;
	radioButtons[RB_CONFIG_SCOPE_LINED].callbackFunc = rbConfigScopeLined;

	/* Wire up pattern channel callbacks */
	radioButtons[RB_CONFIG_PATT_4CHANS].callbackFunc = rbConfigPatt4Chans;
	radioButtons[RB_CONFIG_PATT_6CHANS].callbackFunc = rbConfigPatt6Chans;
	radioButtons[RB_CONFIG_PATT_8CHANS].callbackFunc = rbConfigPatt8Chans;
	radioButtons[RB_CONFIG_PATT_12CHANS].callbackFunc = rbConfigPatt12Chans;

	/* Wire up font callbacks */
	radioButtons[RB_CONFIG_FONT_CAPITALS].callbackFunc = rbConfigFontCapitals;
	radioButtons[RB_CONFIG_FONT_LOWERCASE].callbackFunc = rbConfigFontLowerCase;
	radioButtons[RB_CONFIG_FONT_FUTURE].callbackFunc = rbConfigFontFuture;
	radioButtons[RB_CONFIG_FONT_BOLD].callbackFunc = rbConfigFontBold;

	/* Wire up help screen subject callbacks */
	radioButtons[RB_HELP_FEATURES].callbackFunc = cbHelpFeatures;
	radioButtons[RB_HELP_EFFECTS].callbackFunc = cbHelpEffects;
	radioButtons[RB_HELP_KEYBINDINGS].callbackFunc = cbHelpKeybindings;
	radioButtons[RB_HELP_HOWTO].callbackFunc = cbHelpHowToUseFT2;
	radioButtons[RB_HELP_FAQ].callbackFunc = cbHelpFAQ;
	radioButtons[RB_HELP_BUGS].callbackFunc = cbHelpKnownBugs;

	/* Wire up palette entry callbacks */
	radioButtons[RB_CONFIG_PAL_PATTEXT].callbackFunc = rbConfigPalPatternText;
	radioButtons[RB_CONFIG_PAL_BLOCKMARK].callbackFunc = rbConfigPalBlockMark;
	radioButtons[RB_CONFIG_PAL_TEXTONBLOCK].callbackFunc = rbConfigPalTextOnBlock;
	radioButtons[RB_CONFIG_PAL_MOUSE].callbackFunc = rbConfigPalMouse;
	radioButtons[RB_CONFIG_PAL_DESKTOP].callbackFunc = rbConfigPalDesktop;
	radioButtons[RB_CONFIG_PAL_BUTTONS].callbackFunc = rbConfigPalButtons;

	/* Wire up palette preset callbacks */
	radioButtons[RB_CONFIG_PAL_ARCTIC].callbackFunc = rbConfigPalArctic;
	radioButtons[RB_CONFIG_PAL_LITHE_DARK].callbackFunc = rbConfigPalLitheDark;
	radioButtons[RB_CONFIG_PAL_AURORA_BOREALIS].callbackFunc = rbConfigPalAuroraBorealis;
	radioButtons[RB_CONFIG_PAL_ROSE].callbackFunc = rbConfigPalRose;
	radioButtons[RB_CONFIG_PAL_BLUES].callbackFunc = rbConfigPalBlues;
	radioButtons[RB_CONFIG_PAL_DARK_MODE].callbackFunc = rbConfigPalDarkMode;
	radioButtons[RB_CONFIG_PAL_GOLD].callbackFunc = rbConfigPalGold;
	radioButtons[RB_CONFIG_PAL_VIOLENT].callbackFunc = rbConfigPalViolent;
	radioButtons[RB_CONFIG_PAL_HEAVY_METAL].callbackFunc = rbConfigPalHeavyMetal;
	radioButtons[RB_CONFIG_PAL_WHY_COLORS].callbackFunc = rbConfigPalWhyColors;
	radioButtons[RB_CONFIG_PAL_JUNGLE].callbackFunc = rbConfigPalJungle;
	radioButtons[RB_CONFIG_PAL_USER].callbackFunc = rbConfigPalUserDefined;

	/* Wire up MIDI trigger mode callbacks */
	radioButtons[RB_CONFIG_MIDI_NOTES].callbackFunc = rbConfigMidiTriggerNotes;
	radioButtons[RB_CONFIG_MIDI_PATTERNS].callbackFunc = rbConfigMidiTriggerPatterns;
}

void drawRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID)
{
	if (widgets == NULL || radioButtonID >= NUM_RADIOBUTTONS)
		return;

	if (!widgets->radioButtonVisible[radioButtonID])
		return;

	radioButton_t *rb = &radioButtons[radioButtonID];
	uint8_t state = widgets->radioButtonState[radioButtonID];

	if (bmp == NULL || bmp->radiobuttonGfx == NULL)
	{
		/* Fallback: draw simple radio button */
		fillRect(video, rb->x, rb->y, RADIOBUTTON_W, RADIOBUTTON_H, PAL_BUTTONS);

		/* Draw circle-ish border */
		hLine(video, rb->x + 2, rb->y, RADIOBUTTON_W - 4, PAL_BUTTON2);
		hLine(video, rb->x + 2, rb->y + RADIOBUTTON_H - 1, RADIOBUTTON_W - 4, PAL_BUTTON1);
		vLine(video, rb->x, rb->y + 2, RADIOBUTTON_H - 4, PAL_BUTTON2);
		vLine(video, rb->x + RADIOBUTTON_W - 1, rb->y + 2, RADIOBUTTON_H - 4, PAL_BUTTON1);

		if (state == RADIOBUTTON_CHECKED)
		{
			/* Draw dot in center */
			fillRect(video, rb->x + 3, rb->y + 3, 5, 5, PAL_FORGRND);
		}
		return;
	}

	/* Use bitmap graphics */
	const uint8_t *gfxPtr = &bmp->radiobuttonGfx[state * (RADIOBUTTON_W * RADIOBUTTON_H)];
	blitFast(video, rb->x, rb->y, gfxPtr, RADIOBUTTON_W, RADIOBUTTON_H);
}

void showRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID)
{
	if (widgets == NULL || radioButtonID >= NUM_RADIOBUTTONS)
		return;

	widgets->radioButtonVisible[radioButtonID] = true;
	drawRadioButton(widgets, video, bmp, radioButtonID);
}

void hideRadioButton(struct ft2_widgets_t *widgets, uint16_t radioButtonID)
{
	if (widgets == NULL || radioButtonID >= NUM_RADIOBUTTONS)
		return;

	widgets->radioButtonState[radioButtonID] = RADIOBUTTON_UNCHECKED;
	widgets->radioButtonVisible[radioButtonID] = false;
}

void checkRadioButton(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t radioButtonID)
{
	if (widgets == NULL || radioButtonID >= NUM_RADIOBUTTONS)
		return;

	uint16_t group = radioButtons[radioButtonID].group;

	/* Uncheck all in same group */
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].group == group && widgets->radioButtonState[i] == RADIOBUTTON_CHECKED)
		{
			widgets->radioButtonState[i] = RADIOBUTTON_UNCHECKED;
			drawRadioButton(widgets, video, bmp, i);
			break;
		}
	}

	widgets->radioButtonState[radioButtonID] = RADIOBUTTON_CHECKED;
	drawRadioButton(widgets, video, bmp, radioButtonID);
}

void checkRadioButtonNoRedraw(struct ft2_widgets_t *widgets, uint16_t radioButtonID)
{
	if (widgets == NULL || radioButtonID >= NUM_RADIOBUTTONS)
		return;

	uint16_t group = radioButtons[radioButtonID].group;

	/* Uncheck all in same group */
	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].group == group && widgets->radioButtonState[i] == RADIOBUTTON_CHECKED)
		{
			widgets->radioButtonState[i] = RADIOBUTTON_UNCHECKED;
			break;
		}
	}

	widgets->radioButtonState[radioButtonID] = RADIOBUTTON_CHECKED;
}

void uncheckRadioButtonGroup(struct ft2_widgets_t *widgets, uint16_t group)
{
	if (widgets == NULL)
		return;

	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].group == group)
			widgets->radioButtonState[i] = RADIOBUTTON_UNCHECKED;
	}
}

void showRadioButtonGroup(struct ft2_widgets_t *widgets, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, uint16_t group)
{
	if (widgets == NULL)
		return;

	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].group == group)
			showRadioButton(widgets, video, bmp, i);
	}
}

void hideRadioButtonGroup(struct ft2_widgets_t *widgets, uint16_t group)
{
	if (widgets == NULL)
		return;

	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (radioButtons[i].group == group)
			hideRadioButton(widgets, i);
	}
}

void handleRadioButtonsWhileMouseDown(struct ft2_widgets_t *widgets, ft2_video_t *video, const ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int32_t lastMouseX, int32_t lastMouseY, int16_t lastRadioButtonID)
{
	if (widgets == NULL || lastRadioButtonID < 0 || lastRadioButtonID >= NUM_RADIOBUTTONS)
		return;

	if (!widgets->radioButtonVisible[lastRadioButtonID] || widgets->radioButtonState[lastRadioButtonID] == RADIOBUTTON_CHECKED)
		return;

	radioButton_t *rb = &radioButtons[lastRadioButtonID];

	widgets->radioButtonState[lastRadioButtonID] = RADIOBUTTON_UNCHECKED;
	if (mouseX >= rb->x && mouseX < rb->x + rb->clickAreaWidth &&
	    mouseY >= rb->y && mouseY < rb->y + RADIOBUTTON_H + 1)
	{
		widgets->radioButtonState[lastRadioButtonID] = RADIOBUTTON_PRESSED;
	}

	if (lastMouseX != mouseX || lastMouseY != mouseY)
	{
		drawRadioButton(widgets, video, bmp, lastRadioButtonID);
	}
}

int16_t testRadioButtonMouseDown(struct ft2_widgets_t *widgets, int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	if (widgets == NULL || sysReqShown)
		return -1;

	for (int i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		if (!widgets->radioButtonVisible[i] || widgets->radioButtonState[i] == RADIOBUTTON_CHECKED)
			continue;

		radioButton_t *rb = &radioButtons[i];
		if (mouseX >= rb->x && mouseX < rb->x + rb->clickAreaWidth &&
		    mouseY >= rb->y && mouseY < rb->y + RADIOBUTTON_H + 1)
		{
			return (int16_t)i;
		}
	}

	return -1;
}

void testRadioButtonMouseRelease(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp,
	int32_t mouseX, int32_t mouseY, int16_t lastRadioButtonID)
{
	if (widgets == NULL || lastRadioButtonID < 0 || lastRadioButtonID >= NUM_RADIOBUTTONS)
		return;

	if (!widgets->radioButtonVisible[lastRadioButtonID] || widgets->radioButtonState[lastRadioButtonID] == RADIOBUTTON_CHECKED)
		return;

	radioButton_t *rb = &radioButtons[lastRadioButtonID];

	if (mouseX >= rb->x && mouseX < rb->x + rb->clickAreaWidth &&
	    mouseY >= rb->y && mouseY < rb->y + RADIOBUTTON_H + 1)
	{
		if (rb->callbackFunc != NULL)
			rb->callbackFunc(inst);
		else
			checkRadioButton(widgets, video, bmp, lastRadioButtonID);
	}
}

