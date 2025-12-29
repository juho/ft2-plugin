/**
 * @file ft2_plugin_config.c
 * @brief Configuration screen implementation for the FT2 plugin.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "ft2_plugin_config.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_timemap.h"
#include "ft2_instance.h"

/* Default configuration values */
void ft2_config_init(ft2_plugin_config_t *config)
{
	if (config == NULL)
		return;

	memset(config, 0, sizeof(ft2_plugin_config_t));

	/* Pattern editor defaults */
	config->ptnStretch = false;
	config->ptnHex = true;
	config->ptnInstrZero = false;
	config->ptnFrmWrk = true;
	config->ptnLineLight = true;
	config->ptnShowVolColumn = true;
	config->ptnChnNumbers = true;
	config->ptnAcc = false;
	config->ptnFont = PATT_FONT_CAPITALS;
	config->ptnMaxChannels = MAX_CHANS_SHOWN_8;
	config->ptnLineLightStep = 4;

	/* Recording/Editing defaults (match standalone) */
	config->multiRec = false;
	config->multiKeyJazz = true;
	config->multiEdit = false;
	config->recRelease = false;
	config->recQuant = false;
	config->recQuantRes = 16;
	config->recTrueInsert = false;

	/* Audio/Mixer defaults (match standalone) */
	config->interpolation = INTERPOLATION_SINC8;
	config->boostLevel = 10;
	config->masterVol = 256;
	config->volumeRamp = true;

	/* Visual defaults */
	config->linedScopes = false;

	/* Sample editor defaults */
	config->smpEdNote = 48;

	/* Miscellaneous defaults */
	config->smpCutToBuffer = true;
	config->ptnCutToBuffer = true;
	config->killNotesOnStopPlay = true;

	/* DAW sync defaults (all enabled by default) */
	config->syncBpmFromDAW = true;
	config->syncTransportFromDAW = true;
	config->syncPositionFromDAW = true;
	config->allowFxxSpeedChanges = true;
	config->savedSpeed = 6;  /* Default speed */
	config->savedBpm = 125;  /* Default BPM */

	/* Palette */
	config->palettePreset = PAL_ARCTIC;

	/* Start on audio screen (matches standalone) */
	config->currConfigScreen = CONFIG_SCREEN_AUDIO;

	/* Initialize default envelope presets (matches standalone defConfigData exactly) */
	for (int i = 0; i < 6; i++)
	{
		/* Default volume envelope: 6 points matching FT2's classic shape */
		config->stdEnvPoints[i][0][0][0] = 0;    /* Point 0 X */
		config->stdEnvPoints[i][0][0][1] = 48;   /* Point 0 Y */
		config->stdEnvPoints[i][0][1][0] = 4;    /* Point 1 X */
		config->stdEnvPoints[i][0][1][1] = 64;   /* Point 1 Y (peak) */
		config->stdEnvPoints[i][0][2][0] = 8;    /* Point 2 X */
		config->stdEnvPoints[i][0][2][1] = 44;   /* Point 2 Y */
		config->stdEnvPoints[i][0][3][0] = 14;   /* Point 3 X */
		config->stdEnvPoints[i][0][3][1] = 8;    /* Point 3 Y (dip) */
		config->stdEnvPoints[i][0][4][0] = 24;   /* Point 4 X */
		config->stdEnvPoints[i][0][4][1] = 22;   /* Point 4 Y */
		config->stdEnvPoints[i][0][5][0] = 32;   /* Point 5 X */
		config->stdEnvPoints[i][0][5][1] = 8;    /* Point 5 Y (end low) */
		/* Initialize remaining points to zero */
		for (int p = 6; p < 12; p++)
		{
			config->stdEnvPoints[i][0][p][0] = 0;
			config->stdEnvPoints[i][0][p][1] = 0;
		}
		config->stdVolEnvLength[i] = 6;
		config->stdVolEnvSustain[i] = 2;
		config->stdVolEnvLoopStart[i] = 3;
		config->stdVolEnvLoopEnd[i] = 5;
		config->stdVolEnvFlags[i] = 0;

		/* Default panning envelope: 6 points with slight sweep settling to center */
		config->stdEnvPoints[i][1][0][0] = 0;    /* Point 0 X */
		config->stdEnvPoints[i][1][0][1] = 32;   /* Point 0 Y (center) */
		config->stdEnvPoints[i][1][1][0] = 10;   /* Point 1 X */
		config->stdEnvPoints[i][1][1][1] = 40;   /* Point 1 Y (slight right) */
		config->stdEnvPoints[i][1][2][0] = 30;   /* Point 2 X */
		config->stdEnvPoints[i][1][2][1] = 24;   /* Point 2 Y (slight left) */
		config->stdEnvPoints[i][1][3][0] = 50;   /* Point 3 X */
		config->stdEnvPoints[i][1][3][1] = 32;   /* Point 3 Y (center) */
		config->stdEnvPoints[i][1][4][0] = 60;   /* Point 4 X */
		config->stdEnvPoints[i][1][4][1] = 32;   /* Point 4 Y (center) */
		config->stdEnvPoints[i][1][5][0] = 70;   /* Point 5 X */
		config->stdEnvPoints[i][1][5][1] = 32;   /* Point 5 Y (center) */
		/* Initialize remaining points to zero */
		for (int p = 6; p < 12; p++)
		{
			config->stdEnvPoints[i][1][p][0] = 0;
			config->stdEnvPoints[i][1][p][1] = 0;
		}
		config->stdPanEnvLength[i] = 6;
		config->stdPanEnvSustain[i] = 2;
		config->stdPanEnvLoopStart[i] = 3;
		config->stdPanEnvLoopEnd[i] = 5;
		config->stdPanEnvFlags[i] = 0;

		/* Default vibrato and fadeout */
		config->stdFadeout[i] = 128;
		config->stdVibRate[i] = 0;
		config->stdVibDepth[i] = 0;
		config->stdVibSweep[i] = 0;
		config->stdVibType[i] = 0;
	}
}

void ft2_config_apply(ft2_instance_t *inst, ft2_plugin_config_t *config)
{
	if (inst == NULL || config == NULL)
		return;

	/* Apply pattern editor settings */
	inst->uiState.ptnStretch = config->ptnStretch;
	inst->uiState.ptnHex = config->ptnHex;
	inst->uiState.ptnInstrZero = config->ptnInstrZero;
	inst->uiState.ptnFrmWrk = config->ptnFrmWrk;
	inst->uiState.ptnLineLight = config->ptnLineLight;
	inst->uiState.ptnShowVolColumn = config->ptnShowVolColumn;
	inst->uiState.ptnChnNumbers = config->ptnChnNumbers;
	inst->uiState.ptnAcc = config->ptnAcc;
	inst->uiState.ptnFont = config->ptnFont;

	/* Apply audio/mixer settings */
	inst->audio.interpolationType = config->interpolation;
	inst->audio.volumeRampingFlag = config->volumeRamp;

	/* Update max visible channels based on config */
	switch (config->ptnMaxChannels)
	{
		case MAX_CHANS_SHOWN_4:  inst->uiState.maxVisibleChannels = 4;  break;
		case MAX_CHANS_SHOWN_6:  inst->uiState.maxVisibleChannels = 6;  break;
		case MAX_CHANS_SHOWN_8:  inst->uiState.maxVisibleChannels = 8;  break;
		case MAX_CHANS_SHOWN_12: inst->uiState.maxVisibleChannels = 12; break;
		default:                 inst->uiState.maxVisibleChannels = 8;  break;
	}
}

/* ============ SET RADIO BUTTON STATES ============ */

static void setConfigRadioButtonStates(ft2_plugin_config_t *config)
{
	uint16_t tmpID;

	uncheckRadioButtonGroup(RB_GROUP_CONFIG_SELECT);
	switch (config->currConfigScreen)
	{
		default:
		case CONFIG_SCREEN_AUDIO:         tmpID = RB_CONFIG_AUDIO; break;
		case CONFIG_SCREEN_LAYOUT:        tmpID = RB_CONFIG_LAYOUT; break;
		case CONFIG_SCREEN_MISCELLANEOUS: tmpID = RB_CONFIG_MISC; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;
}

static void setAudioConfigRadioButtonStates(ft2_plugin_config_t *config)
{
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	switch (config->interpolation)
	{
		case INTERPOLATION_DISABLED:  radioButtons[RB_CONFIG_AUDIO_INTRP_NONE].state = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_LINEAR:    radioButtons[RB_CONFIG_AUDIO_INTRP_LINEAR].state = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_QUADRATIC: radioButtons[RB_CONFIG_AUDIO_INTRP_QUADRATIC].state = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_CUBIC:     radioButtons[RB_CONFIG_AUDIO_INTRP_CUBIC].state = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_SINC8:     radioButtons[RB_CONFIG_AUDIO_INTRP_SINC8].state = RADIOBUTTON_CHECKED; break;
		default:
		case INTERPOLATION_SINC16:    radioButtons[RB_CONFIG_AUDIO_INTRP_SINC16].state = RADIOBUTTON_CHECKED; break;
	}
}

static void setLayoutConfigRadioButtonStates(ft2_plugin_config_t *config)
{
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_PATTERN_CHANS);
	switch (config->ptnMaxChannels)
	{
		case MAX_CHANS_SHOWN_4:  radioButtons[RB_CONFIG_PATT_4CHANS].state = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_6:  radioButtons[RB_CONFIG_PATT_6CHANS].state = RADIOBUTTON_CHECKED; break;
		default:
		case MAX_CHANS_SHOWN_8:  radioButtons[RB_CONFIG_PATT_8CHANS].state = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_12: radioButtons[RB_CONFIG_PATT_12CHANS].state = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(RB_GROUP_CONFIG_FONT);
	switch (config->ptnFont)
	{
		default:
		case PATT_FONT_CAPITALS:  radioButtons[RB_CONFIG_FONT_CAPITALS].state = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_LOWERCASE: radioButtons[RB_CONFIG_FONT_LOWERCASE].state = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_FUTURE:    radioButtons[RB_CONFIG_FONT_FUTURE].state = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_BOLD:      radioButtons[RB_CONFIG_FONT_BOLD].state = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(RB_GROUP_CONFIG_SCOPE);
	if (config->linedScopes)
		radioButtons[RB_CONFIG_SCOPE_LINED].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_CONFIG_SCOPE_STANDARD].state = RADIOBUTTON_CHECKED;
}

/* ============ HIDE CONFIG SCREEN ============ */

void hideConfigScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* CONFIG LEFT SIDE */
	hideRadioButtonGroup(RB_GROUP_CONFIG_SELECT);
	hideCheckBox(CB_CONF_AUTOSAVE);
	hidePushButton(PB_CONFIG_RESET);
	hidePushButton(PB_CONFIG_LOAD);
	hidePushButton(PB_CONFIG_SAVE);
	hidePushButton(PB_CONFIG_EXIT);

	/* CONFIG AUDIO */
	hideRadioButtonGroup(RB_GROUP_CONFIG_SOUND_BUFF_SIZE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_BIT_DEPTH);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_FREQ);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INPUT_FREQ);
	hideRadioButtonGroup(RB_GROUP_CONFIG_FREQ_SLIDES);
	hideCheckBox(CB_CONF_VOLRAMP);
	hideCheckBox(CB_CONF_SYNC_BPM);
	hideCheckBox(CB_CONF_SYNC_TRANSPORT);
	hideCheckBox(CB_CONF_SYNC_POSITION);
	hideCheckBox(CB_CONF_ALLOW_FXX_SPEED);
	hidePushButton(PB_CONFIG_AMP_DOWN);
	hidePushButton(PB_CONFIG_AMP_UP);
	hidePushButton(PB_CONFIG_MASTVOL_DOWN);
	hidePushButton(PB_CONFIG_MASTVOL_UP);
	hideScrollBar(SB_AMP_SCROLL);
	hideScrollBar(SB_MASTERVOL_SCROLL);

	/* CONFIG LAYOUT */
	hideCheckBox(CB_CONF_PATTSTRETCH);
	hideCheckBox(CB_CONF_HEXCOUNT);
	hideCheckBox(CB_CONF_ACCIDENTAL);
	hideCheckBox(CB_CONF_SHOWZEROS);
	hideCheckBox(CB_CONF_FRAMEWORK);
	hideCheckBox(CB_CONF_LINECOLORS);
	hideCheckBox(CB_CONF_CHANNUMS);
	hideCheckBox(CB_CONF_SHOWVOLCOL);
	hideCheckBox(CB_CONF_SOFTMOUSE);
	hideCheckBox(CB_CONF_USENICEPTR);
	hideRadioButtonGroup(RB_GROUP_CONFIG_MOUSE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_MOUSE_BUSY);
	hideRadioButtonGroup(RB_GROUP_CONFIG_SCOPE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PATTERN_CHANS);
	hideRadioButtonGroup(RB_GROUP_CONFIG_FONT);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PAL_ENTRIES);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PAL_PRESET);
	hideScrollBar(SB_PAL_R);
	hideScrollBar(SB_PAL_G);
	hideScrollBar(SB_PAL_B);
	hideScrollBar(SB_PAL_CONTRAST);
	hidePushButton(PB_CONFIG_PAL_R_DOWN);
	hidePushButton(PB_CONFIG_PAL_R_UP);
	hidePushButton(PB_CONFIG_PAL_G_DOWN);
	hidePushButton(PB_CONFIG_PAL_G_UP);
	hidePushButton(PB_CONFIG_PAL_B_DOWN);
	hidePushButton(PB_CONFIG_PAL_B_UP);
	hidePushButton(PB_CONFIG_PAL_CONT_DOWN);
	hidePushButton(PB_CONFIG_PAL_CONT_UP);

	/* CONFIG MISCELLANEOUS */
	hideRadioButtonGroup(RB_GROUP_CONFIG_FILESORT);
	hideRadioButtonGroup(RB_GROUP_CONFIG_WIN_SIZE);
	hideCheckBox(CB_CONF_SAMPCUTBUF);
	hideCheckBox(CB_CONF_PATTCUTBUF);
	hideCheckBox(CB_CONF_KILLNOTES);
	hideCheckBox(CB_CONF_OVERWRITE_WARN);
	hideCheckBox(CB_CONF_MULTICHAN_REC);
	hideCheckBox(CB_CONF_MULTICHAN_KEYJAZZ);
	hideCheckBox(CB_CONF_MULTICHAN_EDIT);
	hideCheckBox(CB_CONF_REC_KEYOFF);
	hideCheckBox(CB_CONF_QUANTIZE);
	hideCheckBox(CB_CONF_CHANGE_PATTLEN);
	hideCheckBox(CB_CONF_OLDABOUTLOGO);
	hideCheckBox(CB_CONF_MIDI_ENABLE);
	hideCheckBox(CB_CONF_MIDI_ALLCHN);
	hideCheckBox(CB_CONF_MIDI_TRANSP);
	hideCheckBox(CB_CONF_MIDI_VELOCITY);
	hideCheckBox(CB_CONF_MIDI_AFTERTOUCH);
	hideCheckBox(CB_CONF_VSYNC_OFF);
	hideCheckBox(CB_CONF_FULLSCREEN);
	hideCheckBox(CB_CONF_STRETCH);
	hideCheckBox(CB_CONF_PIXELFILTER);
	hidePushButton(PB_CONFIG_QUANTIZE_UP);
	hidePushButton(PB_CONFIG_QUANTIZE_DOWN);

	inst->uiState.configScreenShown = false;
}

/* ============ SHOW CONFIG SCREEN ============ */

void showConfigScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.configScreenShown = true;
	inst->uiState.scopesShown = false;
}

void exitConfigScreen(ft2_instance_t *inst)
{
	hideConfigScreen(inst);
	inst->uiState.scopesShown = true;
}

/* ============ DRAW CONFIG AUDIO TAB ============ */

static void showConfigAudio(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;

	/* Framework sections matching standalone layout */
	drawFramework(video, 110, 0, 276, 87, FRAMEWORK_TYPE1);   /* Audio output devices area */
	drawFramework(video, 110, 87, 276, 86, FRAMEWORK_TYPE1);  /* Audio input devices area */

	drawFramework(video, 386, 0, 123, 58, FRAMEWORK_TYPE1);   /* Audio buffer size */
	drawFramework(video, 386, 58, 123, 29, FRAMEWORK_TYPE1);  /* Audio bit depth */
	drawFramework(video, 386, 87, 123, 86, FRAMEWORK_TYPE1);  /* Interpolation */

	drawFramework(video, 509, 0, 123, 58, FRAMEWORK_TYPE1);   /* Audio output rate */
	drawFramework(video, 509, 58, 123, 44, FRAMEWORK_TYPE1);  /* Frequency slides */
	drawFramework(video, 509, 102, 123, 71, FRAMEWORK_TYPE1); /* Amp/Master vol/Vol ramp */

	/* DAW Sync section (replaces unused Audio output/input devices) */
	textOutShadow(video, bmp, 114, 4, PAL_FORGRND, PAL_DSKTOP2, "DAW Sync:");

	/* Sync BPM checkbox */
	checkBoxes[CB_CONF_SYNC_BPM].checked = cfg->syncBpmFromDAW;
	showCheckBox(video, bmp, CB_CONF_SYNC_BPM);
	textOutShadow(video, bmp, 131, 21, PAL_FORGRND, PAL_DSKTOP2, "Sync BPM");

	/* Sync transport checkbox */
	checkBoxes[CB_CONF_SYNC_TRANSPORT].checked = cfg->syncTransportFromDAW;
	showCheckBox(video, bmp, CB_CONF_SYNC_TRANSPORT);
	textOutShadow(video, bmp, 131, 37, PAL_FORGRND, PAL_DSKTOP2, "Sync transport (start/stop)");

	/* Sync position checkbox */
	checkBoxes[CB_CONF_SYNC_POSITION].checked = cfg->syncPositionFromDAW;
	showCheckBox(video, bmp, CB_CONF_SYNC_POSITION);
	textOutShadow(video, bmp, 131, 53, PAL_FORGRND, PAL_DSKTOP2, "Sync position (seek)");

	/* Allow Fxx speed changes checkbox */
	checkBoxes[CB_CONF_ALLOW_FXX_SPEED].checked = cfg->allowFxxSpeedChanges;
	showCheckBox(video, bmp, CB_CONF_ALLOW_FXX_SPEED);
	textOutShadow(video, bmp, 131, 69, PAL_FORGRND, PAL_DSKTOP2, "Allow Fxx speed changes");

	/* Audio buffer size - grayed out (DAW controls this) */
	textOutShadow(video, bmp, 390, 3, PAL_DSKTOP2, PAL_DSKTOP2, "Audio buffer size:");
	textOutShadow(video, bmp, 405, 17, PAL_DSKTOP2, PAL_DSKTOP2, "Small");
	textOutShadow(video, bmp, 405, 31, PAL_DSKTOP2, PAL_DSKTOP2, "Medium (default)");
	textOutShadow(video, bmp, 405, 45, PAL_DSKTOP2, PAL_DSKTOP2, "Large");

	/* Audio bit depth - grayed out (DAW controls this) */
	textOutShadow(video, bmp, 390, 61, PAL_DSKTOP2, PAL_DSKTOP2, "Audio bit depth:");
	textOutShadow(video, bmp, 405, 74, PAL_DSKTOP2, PAL_DSKTOP2, "16-bit");
	textOutShadow(video, bmp, 468, 74, PAL_DSKTOP2, PAL_DSKTOP2, "32-bit");

	/* Interpolation - ACTIVE */
	setAudioConfigRadioButtonStates(cfg);
	showRadioButtonGroup(video, bmp, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	textOutShadow(video, bmp, 405, 91, PAL_FORGRND, PAL_DSKTOP2, "No interpolation");
	textOutShadow(video, bmp, 405, 105, PAL_FORGRND, PAL_DSKTOP2, "Linear (FT2)");
	textOutShadow(video, bmp, 405, 119, PAL_FORGRND, PAL_DSKTOP2, "Quadratic spline");
	textOutShadow(video, bmp, 405, 133, PAL_FORGRND, PAL_DSKTOP2, "Cubic spline");
	textOutShadow(video, bmp, 405, 147, PAL_FORGRND, PAL_DSKTOP2, "Sinc (8 point)");
	textOutShadow(video, bmp, 405, 161, PAL_FORGRND, PAL_DSKTOP2, "Sinc (16 point)");

	/* Audio output rate - grayed out (DAW controls this) */
	textOutShadow(video, bmp, 513, 3, PAL_DSKTOP2, PAL_DSKTOP2, "Audio output rate:");
	textOutShadow(video, bmp, 528, 17, PAL_DSKTOP2, PAL_DSKTOP2, "44100Hz");
	textOutShadow(video, bmp, 528, 31, PAL_DSKTOP2, PAL_DSKTOP2, "48000Hz");
	textOutShadow(video, bmp, 528, 45, PAL_DSKTOP2, PAL_DSKTOP2, "96000Hz");

	/* Frequency slides - grayed out */
	textOutShadow(video, bmp, 513, 61, PAL_DSKTOP2, PAL_DSKTOP2, "Frequency slides:");
	textOutShadow(video, bmp, 528, 75, PAL_DSKTOP2, PAL_DSKTOP2, "Amiga");
	textOutShadow(video, bmp, 528, 89, PAL_DSKTOP2, PAL_DSKTOP2, "Linear (default)");

	/* Amplification - ACTIVE */
	textOutShadow(video, bmp, 513, 105, PAL_FORGRND, PAL_DSKTOP2, "Amplification:");
	char ampStr[8];
	snprintf(ampStr, sizeof(ampStr), "%2dx", cfg->boostLevel);
	textOutShadow(video, bmp, 601, 105, PAL_FORGRND, PAL_DSKTOP2, ampStr);
	setScrollBarPos(inst, video, SB_AMP_SCROLL, cfg->boostLevel - 1, false);
	showScrollBar(video, SB_AMP_SCROLL);
	showPushButton(video, bmp, PB_CONFIG_AMP_DOWN);
	showPushButton(video, bmp, PB_CONFIG_AMP_UP);

	/* Master volume - ACTIVE */
	textOutShadow(video, bmp, 513, 133, PAL_FORGRND, PAL_DSKTOP2, "Master volume:");
	char volStr[8];
	snprintf(volStr, sizeof(volStr), "%3d", cfg->masterVol);
	textOutShadow(video, bmp, 601, 133, PAL_FORGRND, PAL_DSKTOP2, volStr);
	setScrollBarPos(inst, video, SB_MASTERVOL_SCROLL, cfg->masterVol, false);
	showScrollBar(video, SB_MASTERVOL_SCROLL);
	showPushButton(video, bmp, PB_CONFIG_MASTVOL_DOWN);
	showPushButton(video, bmp, PB_CONFIG_MASTVOL_UP);

	/* Volume ramping - ACTIVE */
	checkBoxes[CB_CONF_VOLRAMP].checked = cfg->volumeRamp;
	showCheckBox(video, bmp, CB_CONF_VOLRAMP);
	textOutShadow(video, bmp, 529, 160, PAL_FORGRND, PAL_DSKTOP2, "Volume ramping");
}

/* ============ DRAW CONFIG LAYOUT TAB ============ */

static void showConfigLayout(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;

	/* Framework sections matching standalone layout */
	drawFramework(video, 110, 0, 142, 106, FRAMEWORK_TYPE1);   /* Pattern layout */
	drawFramework(video, 252, 0, 142, 98, FRAMEWORK_TYPE1);    /* Pattern modes */
	drawFramework(video, 394, 0, 238, 86, FRAMEWORK_TYPE1);    /* Palette/Pattern text */
	drawFramework(video, 110, 106, 142, 67, FRAMEWORK_TYPE1);  /* Mouse shape */
	drawFramework(video, 252, 98, 142, 45, FRAMEWORK_TYPE1);   /* Pattern font */
	drawFramework(video, 394, 86, 238, 87, FRAMEWORK_TYPE1);   /* Palette presets */
	drawFramework(video, 252, 143, 142, 30, FRAMEWORK_TYPE1);  /* Scopes/software mouse */

	/* Pattern layout section - ACTIVE */
	textOutShadow(video, bmp, 114, 3, PAL_FORGRND, PAL_DSKTOP2, "Pattern layout:");

	checkBoxes[CB_CONF_PATTSTRETCH].checked = cfg->ptnStretch;
	showCheckBox(video, bmp, CB_CONF_PATTSTRETCH);
	textOutShadow(video, bmp, 130, 16, PAL_FORGRND, PAL_DSKTOP2, "Pattern stretch");

	checkBoxes[CB_CONF_HEXCOUNT].checked = cfg->ptnHex;
	showCheckBox(video, bmp, CB_CONF_HEXCOUNT);
	textOutShadow(video, bmp, 130, 29, PAL_FORGRND, PAL_DSKTOP2, "Hex line numbers");

	checkBoxes[CB_CONF_ACCIDENTAL].checked = cfg->ptnAcc;
	showCheckBox(video, bmp, CB_CONF_ACCIDENTAL);
	textOutShadow(video, bmp, 130, 42, PAL_FORGRND, PAL_DSKTOP2, "Accidential");

	checkBoxes[CB_CONF_SHOWZEROS].checked = cfg->ptnInstrZero;
	showCheckBox(video, bmp, CB_CONF_SHOWZEROS);
	textOutShadow(video, bmp, 130, 55, PAL_FORGRND, PAL_DSKTOP2, "Show zeroes");

	checkBoxes[CB_CONF_FRAMEWORK].checked = cfg->ptnFrmWrk;
	showCheckBox(video, bmp, CB_CONF_FRAMEWORK);
	textOutShadow(video, bmp, 130, 68, PAL_FORGRND, PAL_DSKTOP2, "Framework");

	checkBoxes[CB_CONF_LINECOLORS].checked = cfg->ptnLineLight;
	showCheckBox(video, bmp, CB_CONF_LINECOLORS);
	textOutShadow(video, bmp, 130, 81, PAL_FORGRND, PAL_DSKTOP2, "Line number colors");

	checkBoxes[CB_CONF_CHANNUMS].checked = cfg->ptnChnNumbers;
	showCheckBox(video, bmp, CB_CONF_CHANNUMS);
	textOutShadow(video, bmp, 130, 94, PAL_FORGRND, PAL_DSKTOP2, "Channel numbering");

	/* Mouse shape - grayed out (not applicable for plugin) */
	textOutShadow(video, bmp, 114, 109, PAL_DSKTOP2, PAL_DSKTOP2, "Mouse shape:");
	textOutShadow(video, bmp, 130, 121, PAL_DSKTOP2, PAL_DSKTOP2, "Nice");
	textOutShadow(video, bmp, 194, 121, PAL_DSKTOP2, PAL_DSKTOP2, "Ugly");
	textOutShadow(video, bmp, 130, 135, PAL_DSKTOP2, PAL_DSKTOP2, "Awful");
	textOutShadow(video, bmp, 194, 135, PAL_DSKTOP2, PAL_DSKTOP2, "Usable");
	textOutShadow(video, bmp, 114, 148, PAL_DSKTOP2, PAL_DSKTOP2, "Mouse busy shape:");
	textOutShadow(video, bmp, 130, 160, PAL_DSKTOP2, PAL_DSKTOP2, "Vogue");
	textOutShadow(video, bmp, 194, 160, PAL_DSKTOP2, PAL_DSKTOP2, "Mr. H");

	/* Pattern modes section - ACTIVE */
	textOutShadow(video, bmp, 256, 3, PAL_FORGRND, PAL_DSKTOP2, "Pattern modes:");

	checkBoxes[CB_CONF_SHOWVOLCOL].checked = cfg->ptnShowVolColumn;
	showCheckBox(video, bmp, CB_CONF_SHOWVOLCOL);
	textOutShadow(video, bmp, 271, 16, PAL_FORGRND, PAL_DSKTOP2, "Show volume column");

	textOutShadow(video, bmp, 256, 30, PAL_FORGRND, PAL_DSKTOP2, "Maximum visible chn.:");

	setLayoutConfigRadioButtonStates(cfg);
	showRadioButtonGroup(video, bmp, RB_GROUP_CONFIG_PATTERN_CHANS);
	textOutShadow(video, bmp, 272, 43, PAL_FORGRND, PAL_DSKTOP2, "4 channels");
	textOutShadow(video, bmp, 272, 57, PAL_FORGRND, PAL_DSKTOP2, "6 channels");
	textOutShadow(video, bmp, 272, 71, PAL_FORGRND, PAL_DSKTOP2, "8 channels");
	textOutShadow(video, bmp, 272, 85, PAL_FORGRND, PAL_DSKTOP2, "12 channels");

	/* Pattern font section - ACTIVE */
	textOutShadow(video, bmp, 257, 101, PAL_FORGRND, PAL_DSKTOP2, "Pattern font:");
	showRadioButtonGroup(video, bmp, RB_GROUP_CONFIG_FONT);
	textOutShadow(video, bmp, 272, 115, PAL_FORGRND, PAL_DSKTOP2, "Capitals");
	textOutShadow(video, bmp, 338, 114, PAL_FORGRND, PAL_DSKTOP2, "Lower-c.");
	textOutShadow(video, bmp, 272, 130, PAL_FORGRND, PAL_DSKTOP2, "Future");
	textOutShadow(video, bmp, 338, 129, PAL_FORGRND, PAL_DSKTOP2, "Bold");

	/* Scopes section - ACTIVE */
	textOutShadow(video, bmp, 256, 146, PAL_FORGRND, PAL_DSKTOP2, "Scopes:");
	showRadioButtonGroup(video, bmp, RB_GROUP_CONFIG_SCOPE);
	textOutShadow(video, bmp, 319, 146, PAL_FORGRND, PAL_DSKTOP2, "FT2");
	textOutShadow(video, bmp, 360, 146, PAL_FORGRND, PAL_DSKTOP2, "Lined");

	/* Software mouse - grayed out */
	textOutShadow(video, bmp, 272, 160, PAL_DSKTOP2, PAL_DSKTOP2, "Software mouse");

	/* Pattern text / Palette section - ACTIVE */
	textOutShadow(video, bmp, 414, 3, PAL_FORGRND, PAL_DSKTOP2, "Pattern text");
	textOutShadow(video, bmp, 414, 17, PAL_FORGRND, PAL_DSKTOP2, "Block mark");
	textOutShadow(video, bmp, 414, 31, PAL_FORGRND, PAL_DSKTOP2, "Text on block");
	textOutShadow(video, bmp, 414, 45, PAL_FORGRND, PAL_DSKTOP2, "Mouse");
	textOutShadow(video, bmp, 414, 59, PAL_FORGRND, PAL_DSKTOP2, "Desktop");
	textOutShadow(video, bmp, 414, 73, PAL_FORGRND, PAL_DSKTOP2, "Buttons");

	/* Palette presets - ACTIVE */
	textOutShadow(video, bmp, 414, 90, PAL_FORGRND, PAL_DSKTOP2, "Arctic");
	textOutShadow(video, bmp, 528, 90, PAL_FORGRND, PAL_DSKTOP2, "LiTHe dark");
	textOutShadow(video, bmp, 414, 104, PAL_FORGRND, PAL_DSKTOP2, "Aurora Borealis");
	textOutShadow(video, bmp, 528, 104, PAL_FORGRND, PAL_DSKTOP2, "Rose");
	textOutShadow(video, bmp, 414, 118, PAL_FORGRND, PAL_DSKTOP2, "Blues");
	textOutShadow(video, bmp, 528, 118, PAL_FORGRND, PAL_DSKTOP2, "Dark mode");
	textOutShadow(video, bmp, 414, 132, PAL_FORGRND, PAL_DSKTOP2, "Gold");
	textOutShadow(video, bmp, 528, 132, PAL_FORGRND, PAL_DSKTOP2, "Violent");
	textOutShadow(video, bmp, 414, 146, PAL_FORGRND, PAL_DSKTOP2, "Heavy Metal");
	textOutShadow(video, bmp, 528, 146, PAL_FORGRND, PAL_DSKTOP2, "Why colors?");
	textOutShadow(video, bmp, 414, 160, PAL_FORGRND, PAL_DSKTOP2, "Jungle");
	textOutShadow(video, bmp, 528, 160, PAL_FORGRND, PAL_DSKTOP2, "User defined");

	/* Show palette editor widgets (scrollbars, buttons, radio buttons) */
	showPaletteEditor(inst, video, bmp);
}

/* ============ DRAW CONFIG MISCELLANEOUS TAB ============ */

static void showConfigMiscellaneous(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;

	/* Framework sections matching standalone layout */
	drawFramework(video, 110, 0, 99, 43, FRAMEWORK_TYPE1);     /* Dir sorting */
	drawFramework(video, 209, 0, 199, 55, FRAMEWORK_TYPE1);    /* Cut to buffer / kill voices */
	drawFramework(video, 408, 0, 224, 91, FRAMEWORK_TYPE1);    /* Default directories */

	drawFramework(video, 110, 43, 99, 57, FRAMEWORK_TYPE1);    /* Window size */
	drawFramework(video, 209, 55, 199, 102, FRAMEWORK_TYPE1);  /* Rec./Edit/Play */
	drawFramework(video, 408, 91, 224, 82, FRAMEWORK_TYPE1);   /* MIDI settings */

	drawFramework(video, 110, 100, 99, 73, FRAMEWORK_TYPE1);   /* Video settings */
	drawFramework(video, 209, 157, 199, 16, FRAMEWORK_TYPE1);  /* Original FT2 About */

	/* Dir sorting - grayed out (not applicable for plugin) */
	textOutShadow(video, bmp, 114, 3, PAL_DSKTOP2, PAL_DSKTOP2, "Dir. sorting pri.:");
	textOutShadow(video, bmp, 130, 16, PAL_DSKTOP2, PAL_DSKTOP2, "Ext.");
	textOutShadow(video, bmp, 130, 30, PAL_DSKTOP2, PAL_DSKTOP2, "Name");

	/* Sample/Pattern cut to buffer - ACTIVE */
	checkBoxes[CB_CONF_SAMPCUTBUF].checked = cfg->smpCutToBuffer;
	showCheckBox(video, bmp, CB_CONF_SAMPCUTBUF);
	textOutShadow(video, bmp, 228, 4, PAL_FORGRND, PAL_DSKTOP2, "Sample \"cut to buffer\"");

	checkBoxes[CB_CONF_PATTCUTBUF].checked = cfg->ptnCutToBuffer;
	showCheckBox(video, bmp, CB_CONF_PATTCUTBUF);
	textOutShadow(video, bmp, 228, 17, PAL_FORGRND, PAL_DSKTOP2, "Pattern \"cut to buffer\"");

	/* Kill voices at music stop - ACTIVE */
	checkBoxes[CB_CONF_KILLNOTES].checked = cfg->killNotesOnStopPlay;
	showCheckBox(video, bmp, CB_CONF_KILLNOTES);
	textOutShadow(video, bmp, 228, 30, PAL_FORGRND, PAL_DSKTOP2, "Kill voices at music stop");

	/* File-overwrite warning - grayed out */
	textOutShadow(video, bmp, 228, 43, PAL_DSKTOP2, PAL_DSKTOP2, "File-overwrite warning");

	/* Default directories - grayed out */
	textOutShadow(video, bmp, 464, 3, PAL_DSKTOP2, PAL_DSKTOP2, "Default directories:");
	textOutShadow(video, bmp, 413, 17, PAL_DSKTOP2, PAL_DSKTOP2, "Modules");
	textOutShadow(video, bmp, 413, 32, PAL_DSKTOP2, PAL_DSKTOP2, "Instruments");
	textOutShadow(video, bmp, 413, 47, PAL_DSKTOP2, PAL_DSKTOP2, "Samples");
	textOutShadow(video, bmp, 413, 62, PAL_DSKTOP2, PAL_DSKTOP2, "Patterns");
	textOutShadow(video, bmp, 413, 77, PAL_DSKTOP2, PAL_DSKTOP2, "Tracks");

	/* Text boxes for directories - grayed out placeholders */
	drawFramework(video, 485, 15, 145, 14, FRAMEWORK_TYPE2);
	drawFramework(video, 485, 30, 145, 14, FRAMEWORK_TYPE2);
	drawFramework(video, 485, 45, 145, 14, FRAMEWORK_TYPE2);
	drawFramework(video, 485, 60, 145, 14, FRAMEWORK_TYPE2);
	drawFramework(video, 485, 75, 145, 14, FRAMEWORK_TYPE2);

	/* Window size - grayed out */
	textOutShadow(video, bmp, 114, 46, PAL_DSKTOP2, PAL_DSKTOP2, "Window size:");
	textOutShadow(video, bmp, 130, 59, PAL_DSKTOP2, PAL_DSKTOP2, "Auto fit");
	textOutShadow(video, bmp, 130, 73, PAL_DSKTOP2, PAL_DSKTOP2, "1x");
	textOutShadow(video, bmp, 172, 73, PAL_DSKTOP2, PAL_DSKTOP2, "3x");
	textOutShadow(video, bmp, 130, 87, PAL_DSKTOP2, PAL_DSKTOP2, "2x");
	textOutShadow(video, bmp, 172, 87, PAL_DSKTOP2, PAL_DSKTOP2, "4x");

	/* Video settings - grayed out */
	textOutShadow(video, bmp, 114, 103, PAL_DSKTOP2, PAL_DSKTOP2, "Video settings:");
	textOutShadow(video, bmp, 130, 117, PAL_DSKTOP2, PAL_DSKTOP2, "VSync off");
	textOutShadow(video, bmp, 130, 130, PAL_DSKTOP2, PAL_DSKTOP2, "Fullscreen");
	textOutShadow(video, bmp, 130, 143, PAL_DSKTOP2, PAL_DSKTOP2, "Stretched");
	textOutShadow(video, bmp, 130, 156, PAL_DSKTOP2, PAL_DSKTOP2, "Pixel filter");

	/* Rec./Edit/Play section - ACTIVE */
	textOutShadow(video, bmp, 213, 57, PAL_FORGRND, PAL_DSKTOP2, "Rec./Edit/Play:");

	checkBoxes[CB_CONF_MULTICHAN_REC].checked = cfg->multiRec;
	showCheckBox(video, bmp, CB_CONF_MULTICHAN_REC);
	textOutShadow(video, bmp, 228, 70, PAL_FORGRND, PAL_DSKTOP2, "Multichannel record");

	checkBoxes[CB_CONF_MULTICHAN_KEYJAZZ].checked = cfg->multiKeyJazz;
	showCheckBox(video, bmp, CB_CONF_MULTICHAN_KEYJAZZ);
	textOutShadow(video, bmp, 228, 83, PAL_FORGRND, PAL_DSKTOP2, "Multichannel \"key jazz\"");

	checkBoxes[CB_CONF_MULTICHAN_EDIT].checked = cfg->multiEdit;
	showCheckBox(video, bmp, CB_CONF_MULTICHAN_EDIT);
	textOutShadow(video, bmp, 228, 96, PAL_FORGRND, PAL_DSKTOP2, "Multichannel edit");

	checkBoxes[CB_CONF_REC_KEYOFF].checked = cfg->recRelease;
	showCheckBox(video, bmp, CB_CONF_REC_KEYOFF);
	textOutShadow(video, bmp, 228, 109, PAL_FORGRND, PAL_DSKTOP2, "Record key-off notes");

	checkBoxes[CB_CONF_QUANTIZE].checked = cfg->recQuant;
	showCheckBox(video, bmp, CB_CONF_QUANTIZE);
	textOutShadow(video, bmp, 228, 122, PAL_FORGRND, PAL_DSKTOP2, "Quantization");

	/* Quantization value */
	textOutShadow(video, bmp, 338, 122, PAL_FORGRND, PAL_DSKTOP2, "1/");
	char quantStr[8];
	snprintf(quantStr, sizeof(quantStr), "%d", cfg->recQuantRes);
	textOutShadow(video, bmp, 350, 122, PAL_FORGRND, PAL_DSKTOP2, quantStr);

	checkBoxes[CB_CONF_CHANGE_PATTLEN].checked = cfg->recTrueInsert;
	showCheckBox(video, bmp, CB_CONF_CHANGE_PATTLEN);
	textOutShadow(video, bmp, 228, 135, PAL_FORGRND, PAL_DSKTOP2, "Change pattern length when");
	textOutShadow(video, bmp, 228, 146, PAL_FORGRND, PAL_DSKTOP2, "inserting/deleting line.");

	/* Original FT2 About screen - grayed out */
	textOutShadow(video, bmp, 228, 161, PAL_DSKTOP2, PAL_DSKTOP2, "Original FT2 About screen");

	/* MIDI settings - grayed out (plugin uses DAW MIDI) */
	textOutShadow(video, bmp, 428, 95, PAL_DSKTOP2, PAL_DSKTOP2, "Enable MIDI");
	textOutShadow(video, bmp, 412, 108, PAL_DSKTOP2, PAL_DSKTOP2, "Record MIDI chn.");
	charOutShadow(video, bmp, 523, 108, PAL_DSKTOP2, PAL_DSKTOP2, '(');
	textOutShadow(video, bmp, 546, 108, PAL_DSKTOP2, PAL_DSKTOP2, "all )");
	textOutShadow(video, bmp, 428, 121, PAL_DSKTOP2, PAL_DSKTOP2, "Record transpose");
	textOutShadow(video, bmp, 428, 134, PAL_DSKTOP2, PAL_DSKTOP2, "Record velocity");
	textOutShadow(video, bmp, 428, 147, PAL_DSKTOP2, PAL_DSKTOP2, "Record aftertouch");
	textOutShadow(video, bmp, 412, 160, PAL_DSKTOP2, PAL_DSKTOP2, "Vel./A.t. senstvty.");
	charOutShadow(video, bmp, 547, 160, PAL_DSKTOP2, PAL_DSKTOP2, '%');
}

/* ============ MAIN DRAW FUNCTION ============ */

void drawConfigScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	/* Clear the top screen area (0-173 pixels) */
	clearRect(video, 0, 0, 632, 173);

	/* Draw left sidebar framework */
	drawFramework(video, 0, 0, 110, 173, FRAMEWORK_TYPE1);
	
	/* Set and show config tab radio buttons */
	setConfigRadioButtonStates(&inst->config);
	showRadioButtonGroup(video, bmp, RB_GROUP_CONFIG_SELECT);

	/* Show push buttons */
	showPushButton(video, bmp, PB_CONFIG_RESET);
	showPushButton(video, bmp, PB_CONFIG_LOAD);
	showPushButton(video, bmp, PB_CONFIG_SAVE);
	showPushButton(video, bmp, PB_CONFIG_EXIT);

	/* Draw text labels for tabs */
	textOutShadow(video, bmp, 4, 4, PAL_FORGRND, PAL_DSKTOP2, "Configuration:");
	textOutShadow(video, bmp, 21, 19, PAL_FORGRND, PAL_DSKTOP2, "Audio");
	textOutShadow(video, bmp, 21, 35, PAL_FORGRND, PAL_DSKTOP2, "Layout");
	textOutShadow(video, bmp, 21, 51, PAL_FORGRND, PAL_DSKTOP2, "Miscellaneous");
	textOutShadow(video, bmp, 21, 67, PAL_DSKTOP2, PAL_DSKTOP2, "MIDI input");

	/* Draw current tab content */
	switch (inst->config.currConfigScreen)
	{
		case CONFIG_SCREEN_AUDIO:
			showConfigAudio(inst, video, bmp);
			break;
		case CONFIG_SCREEN_LAYOUT:
			showConfigLayout(inst, video, bmp);
			break;
		case CONFIG_SCREEN_MISCELLANEOUS:
			showConfigMiscellaneous(inst, video, bmp);
			break;
		default:
			showConfigAudio(inst, video, bmp);
			break;
	}
}

/* ============ TAB SWITCHING ============ */

void rbConfigAudio(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_AUDIO;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigLayout(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_LAYOUT;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMiscellaneous(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_MISCELLANEOUS;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

/* ============ INTERPOLATION CALLBACKS ============ */

/* Helper to safely change interpolation mode.
 * Stops all voices first to prevent race conditions with the audio thread,
 * matching the standalone's use of lockMixerCallback()/unlockMixerCallback(). */
static void setInterpolationType(ft2_instance_t *inst, uint8_t type)
{
	ft2_stop_all_voices(inst);
	inst->config.interpolation = type;
	inst->audio.interpolationType = type;
}

void rbConfigIntrpNone(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_DISABLED);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_NONE);
}

void rbConfigIntrpLinear(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_LINEAR);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_LINEAR);
}

void rbConfigIntrpQuadratic(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_QUADRATIC);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_QUADRATIC);
}

void rbConfigIntrpCubic(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_CUBIC);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_CUBIC);
}

void rbConfigIntrpSinc8(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_SINC8);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_SINC8);
}

void rbConfigIntrpSinc16(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_SINC16);
	checkRadioButtonNoRedraw(RB_CONFIG_AUDIO_INTRP_SINC16);
}

/* ============ SCOPE STYLE CALLBACKS ============ */

void rbConfigScopeFT2(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.linedScopes = false;
	checkRadioButtonNoRedraw(RB_CONFIG_SCOPE_STANDARD);
}

void rbConfigScopeLined(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.linedScopes = true;
	checkRadioButtonNoRedraw(RB_CONFIG_SCOPE_LINED);
}

/* ============ CHANNEL COUNT CALLBACKS ============ */

void rbConfigPatt4Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_4;
	inst->uiState.maxVisibleChannels = 4;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_PATT_4CHANS);
}

void rbConfigPatt6Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_6;
	inst->uiState.maxVisibleChannels = 6;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_PATT_6CHANS);
}

void rbConfigPatt8Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_8;
	inst->uiState.maxVisibleChannels = 8;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_PATT_8CHANS);
}

void rbConfigPatt12Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_12;
	inst->uiState.maxVisibleChannels = 12;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_PATT_12CHANS);
}

/* ============ FONT CALLBACKS ============ */

void rbConfigFontCapitals(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_FONT_CAPITALS);
}

void rbConfigFontLowerCase(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_FONT_LOWERCASE);
}

void rbConfigFontFuture(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_FONT_FUTURE);
}

void rbConfigFontBold(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_BOLD;
	inst->uiState.ptnFont = PATT_FONT_BOLD;
	inst->uiState.updatePatternEditor = true;
	checkRadioButtonNoRedraw(RB_CONFIG_FONT_BOLD);
}

/* ============ PATTERN EDITOR CHECKBOXES ============ */

void cbConfigPattStretch(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnStretch = !inst->config.ptnStretch;
	inst->uiState.ptnStretch = inst->config.ptnStretch;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigHexCount(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnHex = !inst->config.ptnHex;
	inst->uiState.ptnHex = inst->config.ptnHex;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigAccidential(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnAcc = !inst->config.ptnAcc;
	inst->uiState.ptnAcc = inst->config.ptnAcc;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigShowZeroes(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnInstrZero = !inst->config.ptnInstrZero;
	inst->uiState.ptnInstrZero = inst->config.ptnInstrZero;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigFramework(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->config.ptnFrmWrk = !inst->config.ptnFrmWrk;
	inst->uiState.ptnFrmWrk = inst->config.ptnFrmWrk;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigLineColors(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnLineLight = !inst->config.ptnLineLight;
	inst->uiState.ptnLineLight = inst->config.ptnLineLight;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigChanNums(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnChnNumbers = !inst->config.ptnChnNumbers;
	inst->uiState.ptnChnNumbers = inst->config.ptnChnNumbers;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigShowVolCol(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnShowVolColumn = !inst->config.ptnShowVolColumn;
	inst->uiState.ptnShowVolColumn = inst->config.ptnShowVolColumn;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
}

/* ============ VOLUME RAMPING CHECKBOX ============ */

void cbConfigVolRamp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->config.volumeRamp = !inst->config.volumeRamp;
	inst->audio.volumeRampingFlag = inst->config.volumeRamp;
}

/* ============ MISCELLANEOUS CHECKBOXES ============ */

void cbSampCutToBuff(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.smpCutToBuffer = !inst->config.smpCutToBuffer;
}

void cbPattCutToBuff(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.ptnCutToBuffer = !inst->config.ptnCutToBuffer;
}

void cbKillNotesAtStop(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.killNotesOnStopPlay = !inst->config.killNotesOnStopPlay;
}

void cbMultiChanRec(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.multiRec = !inst->config.multiRec;
}

void cbMultiChanKeyJazz(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.multiKeyJazz = !inst->config.multiKeyJazz;
}

void cbMultiChanEdit(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.multiEdit = !inst->config.multiEdit;
}

void cbRecKeyOff(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.recRelease = !inst->config.recRelease;
}

void cbQuantize(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.recQuant = !inst->config.recQuant;
}

void cbChangePattLen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.recTrueInsert = !inst->config.recTrueInsert;
}

/* ============ DAW SYNC CHECKBOXES ============ */

void cbSyncBpmFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->config.syncBpmFromDAW = !inst->config.syncBpmFromDAW;

	if (inst->config.syncBpmFromDAW)
	{
		/* Enabling sync: save current BPM (DAW will control it now) */
		inst->config.savedBpm = inst->replayer.song.BPM;
	}
	else
	{
		/* Disabling sync: restore saved BPM */
		if (inst->config.savedBpm > 0)
		{
			inst->replayer.song.BPM = inst->config.savedBpm;
			ft2_set_bpm(inst, inst->config.savedBpm);
		}

		/* Also disable position sync (it depends on BPM sync) */
		if (inst->config.syncPositionFromDAW)
			inst->config.syncPositionFromDAW = false;
	}

	/* Invalidate timemap since BPM handling changes affect timing */
	ft2_timemap_invalidate(inst);

	/* Trigger full redraw to update BPM buttons and display */
	inst->uiState.needsFullRedraw = true;
}

void cbSyncTransportFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.syncTransportFromDAW = !inst->config.syncTransportFromDAW;
}

void cbSyncPositionFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Position sync requires BPM sync to be enabled */
	if (!inst->config.syncBpmFromDAW && !inst->config.syncPositionFromDAW)
	{
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
		{
			ft2_dialog_show_message(&ui->dialog, "System message",
				"Position sync requires BPM sync to be enabled.");
		}
		return;
	}

	inst->config.syncPositionFromDAW = !inst->config.syncPositionFromDAW;
}

void cbAllowFxxSpeedChanges(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->config.allowFxxSpeedChanges = !inst->config.allowFxxSpeedChanges;

	if (inst->config.allowFxxSpeedChanges)
	{
		/* Restoring: bring back the saved speed */
		if (inst->config.savedSpeed > 0)
			inst->replayer.song.speed = inst->config.savedSpeed;
	}
	else
	{
		/* Disabling: save current speed and lock to 6 */
		inst->config.savedSpeed = inst->replayer.song.speed;
		inst->replayer.song.speed = 6;
	}

	/* Invalidate timemap since speed handling changes affect timing */
	ft2_timemap_invalidate(inst);

	/* Trigger full redraw to update speed buttons and display */
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
}

/* ============ AMPLIFICATION ARROW CALLBACKS ============ */

void configAmpDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	
	scrollBarScrollLeft(inst, video, SB_AMP_SCROLL, 1);
}

void configAmpUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	
	scrollBarScrollRight(inst, video, SB_AMP_SCROLL, 1);
}

void configMasterVolDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	
	scrollBarScrollLeft(inst, video, SB_MASTERVOL_SCROLL, 1);
}

void configMasterVolUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	
	scrollBarScrollRight(inst, video, SB_MASTERVOL_SCROLL, 1);
}

/* Scrollbar position callbacks - these are called when the scrollbar is moved */
void sbAmpPos(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;
	
	uint8_t newLevel = (uint8_t)(pos + 1);
	if (newLevel < 1) newLevel = 1;
	if (newLevel > 32) newLevel = 32;
	
	if (inst->config.boostLevel != newLevel)
	{
		inst->config.boostLevel = newLevel;
		ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);
		
		/* Redraw amp value */
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL && inst->uiState.configScreenShown && inst->config.currConfigScreen == CONFIG_SCREEN_AUDIO)
		{
			char ampStr[8];
			snprintf(ampStr, sizeof(ampStr), "%2dx", inst->config.boostLevel);
			fillRect(&ui->video, 601, 105, 20, 8, PAL_DESKTOP);
			textOutShadow(&ui->video, &ui->bmp, 601, 105, PAL_FORGRND, PAL_DSKTOP2, ampStr);
		}
	}
}

void sbMasterVolPos(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;
	
	uint16_t newVol = (uint16_t)pos;
	if (newVol > 256) newVol = 256;
	
	if (inst->config.masterVol != newVol)
	{
		inst->config.masterVol = newVol;
		ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);
		
		/* Redraw master vol value */
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL && inst->uiState.configScreenShown && inst->config.currConfigScreen == CONFIG_SCREEN_AUDIO)
		{
			char volStr[8];
			snprintf(volStr, sizeof(volStr), "%3d", inst->config.masterVol);
			fillRect(&ui->video, 601, 133, 20, 8, PAL_DESKTOP);
			textOutShadow(&ui->video, &ui->bmp, 601, 133, PAL_FORGRND, PAL_DSKTOP2, volStr);
		}
	}
}

/* ============ CONFIG BUTTON CALLBACKS ============ */

static void onResetConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result != DIALOG_RESULT_YES || inst == NULL)
		return;

	inst->uiState.requestResetConfig = true;
}

void pbConfigReset(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Reset all settings to factory defaults?",
			inst, onResetConfigResult, NULL);
	}
}

static void onLoadGlobalConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                     const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result != DIALOG_RESULT_YES || inst == NULL)
		return;

	inst->uiState.requestLoadGlobalConfig = true;
}

void pbConfigLoad(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Load your global config?",
			inst, onLoadGlobalConfigResult, NULL);
	}
}

static void onSaveGlobalConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                     const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result != DIALOG_RESULT_YES || inst == NULL)
		return;

	inst->uiState.requestSaveGlobalConfig = true;
}

void pbConfigSave(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Overwrite global config?",
			inst, onSaveGlobalConfigResult, NULL);
	}
}
