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

	/* MIDI Input */
	config->midiEnabled = true;
	config->midiAllChannels = true;
	config->midiChannel = 1;
	config->midiTranspose = 0;
	config->midiVelocitySens = 100;
	config->midiRecordVelocity = true;

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

	/* Default channel routing: wrap around (Ch 1->Out1, ..., Ch15->Out15, Ch16->Out1, ...) */
	for (int i = 0; i < 32; i++)
	{
		config->channelRouting[i] = i % FT2_NUM_OUTPUTS;
		config->channelToMain[i] = true;  /* All channels go to main by default */
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

static void setConfigRadioButtonStates(ft2_widgets_t *widgets, ft2_plugin_config_t *config)
{
	if (widgets == NULL)
		return;

	uint16_t tmpID;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_SELECT);
	switch (config->currConfigScreen)
	{
		default:
		case CONFIG_SCREEN_AUDIO:         tmpID = RB_CONFIG_AUDIO; break;
		case CONFIG_SCREEN_LAYOUT:        tmpID = RB_CONFIG_LAYOUT; break;
		case CONFIG_SCREEN_MISCELLANEOUS: tmpID = RB_CONFIG_MISC; break;
		case CONFIG_SCREEN_IO_ROUTING:    tmpID = RB_CONFIG_IO_ROUTING; break;
		case CONFIG_SCREEN_MIDI_INPUT:    tmpID = RB_CONFIG_MIDI; break;
	}
	widgets->radioButtonState[tmpID] = RADIOBUTTON_CHECKED;
}

static void setAudioConfigRadioButtonStates(ft2_widgets_t *widgets, ft2_plugin_config_t *config)
{
	if (widgets == NULL)
		return;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	switch (config->interpolation)
	{
		case INTERPOLATION_DISABLED:  widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_NONE] = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_LINEAR:    widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_LINEAR] = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_QUADRATIC: widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_QUADRATIC] = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_CUBIC:     widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_CUBIC] = RADIOBUTTON_CHECKED; break;
		case INTERPOLATION_SINC8:     widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_SINC8] = RADIOBUTTON_CHECKED; break;
		default:
		case INTERPOLATION_SINC16:    widgets->radioButtonState[RB_CONFIG_AUDIO_INTRP_SINC16] = RADIOBUTTON_CHECKED; break;
	}
}

static void setLayoutConfigRadioButtonStates(ft2_widgets_t *widgets, ft2_plugin_config_t *config)
{
	if (widgets == NULL)
		return;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_PATTERN_CHANS);
	switch (config->ptnMaxChannels)
	{
		case MAX_CHANS_SHOWN_4:  widgets->radioButtonState[RB_CONFIG_PATT_4CHANS] = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_6:  widgets->radioButtonState[RB_CONFIG_PATT_6CHANS] = RADIOBUTTON_CHECKED; break;
		default:
		case MAX_CHANS_SHOWN_8:  widgets->radioButtonState[RB_CONFIG_PATT_8CHANS] = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_12: widgets->radioButtonState[RB_CONFIG_PATT_12CHANS] = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_FONT);
	switch (config->ptnFont)
	{
		default:
		case PATT_FONT_CAPITALS:  widgets->radioButtonState[RB_CONFIG_FONT_CAPITALS] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_LOWERCASE: widgets->radioButtonState[RB_CONFIG_FONT_LOWERCASE] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_FUTURE:    widgets->radioButtonState[RB_CONFIG_FONT_FUTURE] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_BOLD:      widgets->radioButtonState[RB_CONFIG_FONT_BOLD] = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_SCOPE);
	if (config->linedScopes)
		widgets->radioButtonState[RB_CONFIG_SCOPE_LINED] = RADIOBUTTON_CHECKED;
	else
		widgets->radioButtonState[RB_CONFIG_SCOPE_STANDARD] = RADIOBUTTON_CHECKED;
}

/* ============ HIDE CONFIG SCREEN ============ */

void hideConfigScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
	{
		inst->uiState.configScreenShown = false;
		return;
	}

	/* CONFIG LEFT SIDE */
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_SELECT);
	hideCheckBox(widgets, CB_CONF_AUTOSAVE);
	hidePushButton(widgets, PB_CONFIG_RESET);
	hidePushButton(widgets, PB_CONFIG_LOAD);
	hidePushButton(widgets, PB_CONFIG_SAVE);
	hidePushButton(widgets, PB_CONFIG_EXIT);

	/* CONFIG AUDIO */
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_SOUND_BUFF_SIZE);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_BIT_DEPTH);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_FREQ);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_FREQ_SLIDES);
	hideCheckBox(widgets, CB_CONF_VOLRAMP);
	hideCheckBox(widgets, CB_CONF_SYNC_BPM);
	hideCheckBox(widgets, CB_CONF_SYNC_TRANSPORT);
	hideCheckBox(widgets, CB_CONF_SYNC_POSITION);
	hideCheckBox(widgets, CB_CONF_ALLOW_FXX_SPEED);
	hidePushButton(widgets, PB_CONFIG_AMP_DOWN);
	hidePushButton(widgets, PB_CONFIG_AMP_UP);
	hidePushButton(widgets, PB_CONFIG_MASTVOL_DOWN);
	hidePushButton(widgets, PB_CONFIG_MASTVOL_UP);
	hideScrollBar(widgets, SB_AMP_SCROLL);
	hideScrollBar(widgets, SB_MASTERVOL_SCROLL);

	/* CONFIG LAYOUT */
	hideCheckBox(widgets, CB_CONF_PATTSTRETCH);
	hideCheckBox(widgets, CB_CONF_HEXCOUNT);
	hideCheckBox(widgets, CB_CONF_ACCIDENTAL);
	hideCheckBox(widgets, CB_CONF_SHOWZEROS);
	hideCheckBox(widgets, CB_CONF_FRAMEWORK);
	hideCheckBox(widgets, CB_CONF_LINECOLORS);
	hideCheckBox(widgets, CB_CONF_CHANNUMS);
	hideCheckBox(widgets, CB_CONF_SHOWVOLCOL);
	hideCheckBox(widgets, CB_CONF_SOFTMOUSE);
	hideCheckBox(widgets, CB_CONF_USENICEPTR);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_MOUSE);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_MOUSE_BUSY);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_SCOPE);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_PATTERN_CHANS);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_FONT);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_PAL_ENTRIES);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_PAL_PRESET);
	hideScrollBar(widgets, SB_PAL_R);
	hideScrollBar(widgets, SB_PAL_G);
	hideScrollBar(widgets, SB_PAL_B);
	hideScrollBar(widgets, SB_PAL_CONTRAST);
	hidePushButton(widgets, PB_CONFIG_PAL_R_DOWN);
	hidePushButton(widgets, PB_CONFIG_PAL_R_UP);
	hidePushButton(widgets, PB_CONFIG_PAL_G_DOWN);
	hidePushButton(widgets, PB_CONFIG_PAL_G_UP);
	hidePushButton(widgets, PB_CONFIG_PAL_B_DOWN);
	hidePushButton(widgets, PB_CONFIG_PAL_B_UP);
	hidePushButton(widgets, PB_CONFIG_PAL_CONT_DOWN);
	hidePushButton(widgets, PB_CONFIG_PAL_CONT_UP);

	/* CONFIG MISCELLANEOUS */
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_FILESORT);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_WIN_SIZE);
	hideCheckBox(widgets, CB_CONF_SAMPCUTBUF);
	hideCheckBox(widgets, CB_CONF_PATTCUTBUF);
	hideCheckBox(widgets, CB_CONF_KILLNOTES);
	hideCheckBox(widgets, CB_CONF_OVERWRITE_WARN);
	hideCheckBox(widgets, CB_CONF_MULTICHAN_REC);
	hideCheckBox(widgets, CB_CONF_MULTICHAN_KEYJAZZ);
	hideCheckBox(widgets, CB_CONF_MULTICHAN_EDIT);
	hideCheckBox(widgets, CB_CONF_REC_KEYOFF);
	hideCheckBox(widgets, CB_CONF_QUANTIZE);
	hideCheckBox(widgets, CB_CONF_CHANGE_PATTLEN);
	hideCheckBox(widgets, CB_CONF_OLDABOUTLOGO);
	hideCheckBox(widgets, CB_CONF_MIDI_ENABLE);
	hideCheckBox(widgets, CB_CONF_MIDI_ALLCHN);
	hideCheckBox(widgets, CB_CONF_MIDI_TRANSP);
	hideCheckBox(widgets, CB_CONF_MIDI_VELOCITY);
	hideCheckBox(widgets, CB_CONF_MIDI_AFTERTOUCH);
	hideCheckBox(widgets, CB_CONF_VSYNC_OFF);
	hideCheckBox(widgets, CB_CONF_FULLSCREEN);
	hideCheckBox(widgets, CB_CONF_STRETCH);
	hideCheckBox(widgets, CB_CONF_PIXELFILTER);
	hidePushButton(widgets, PB_CONFIG_QUANTIZE_UP);
	hidePushButton(widgets, PB_CONFIG_QUANTIZE_DOWN);

	/* CONFIG MIDI INPUT */
	hideScrollBar(widgets, SB_MIDI_CHANNEL);
	hideScrollBar(widgets, SB_MIDI_TRANSPOSE);
	hideScrollBar(widgets, SB_MIDI_SENS);
	hidePushButton(widgets, PB_CONFIG_MIDICHN_DOWN);
	hidePushButton(widgets, PB_CONFIG_MIDICHN_UP);
	hidePushButton(widgets, PB_CONFIG_MIDITRANS_DOWN);
	hidePushButton(widgets, PB_CONFIG_MIDITRANS_UP);
	hidePushButton(widgets, PB_CONFIG_MIDISENS_DOWN);
	hidePushButton(widgets, PB_CONFIG_MIDISENS_UP);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_TRIGGER);

	/* CONFIG I/O ROUTING */
	for (int i = 0; i < 32; i++)
	{
		hidePushButton(widgets, PB_CONFIG_ROUTING_CH1_UP + (i * 2));
		hidePushButton(widgets, PB_CONFIG_ROUTING_CH1_DOWN + (i * 2));
		hideCheckBox(widgets, CB_CONF_ROUTING_CH1_TOMAIN + i);
	}

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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

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
	widgets->checkBoxChecked[CB_CONF_SYNC_BPM] = cfg->syncBpmFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_BPM);
	textOutShadow(video, bmp, 131, 21, PAL_FORGRND, PAL_DSKTOP2, "Sync BPM");

	/* Sync transport checkbox */
	widgets->checkBoxChecked[CB_CONF_SYNC_TRANSPORT] = cfg->syncTransportFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_TRANSPORT);
	textOutShadow(video, bmp, 131, 37, PAL_FORGRND, PAL_DSKTOP2, "Sync transport (start/stop)");

	/* Sync position checkbox */
	widgets->checkBoxChecked[CB_CONF_SYNC_POSITION] = cfg->syncPositionFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_POSITION);
	textOutShadow(video, bmp, 131, 53, PAL_FORGRND, PAL_DSKTOP2, "Sync position (seek)");

	/* Allow Fxx speed changes checkbox */
	widgets->checkBoxChecked[CB_CONF_ALLOW_FXX_SPEED] = cfg->allowFxxSpeedChanges;
	showCheckBox(widgets, video, bmp, CB_CONF_ALLOW_FXX_SPEED);
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
	setAudioConfigRadioButtonStates(widgets, cfg);
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
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
	setScrollBarPos(inst, widgets, video, SB_AMP_SCROLL, cfg->boostLevel - 1, false);
	showScrollBar(widgets, video, SB_AMP_SCROLL);
	showPushButton(widgets, video, bmp, PB_CONFIG_AMP_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_AMP_UP);

	/* Master volume - ACTIVE */
	textOutShadow(video, bmp, 513, 133, PAL_FORGRND, PAL_DSKTOP2, "Master volume:");
	char volStr[8];
	snprintf(volStr, sizeof(volStr), "%3d", cfg->masterVol);
	textOutShadow(video, bmp, 601, 133, PAL_FORGRND, PAL_DSKTOP2, volStr);
	setScrollBarPos(inst, widgets, video, SB_MASTERVOL_SCROLL, cfg->masterVol, false);
	showScrollBar(widgets, video, SB_MASTERVOL_SCROLL);
	showPushButton(widgets, video, bmp, PB_CONFIG_MASTVOL_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_MASTVOL_UP);

	/* Volume ramping - ACTIVE */
	widgets->checkBoxChecked[CB_CONF_VOLRAMP] = cfg->volumeRamp;
	showCheckBox(widgets, video, bmp, CB_CONF_VOLRAMP);
	textOutShadow(video, bmp, 529, 160, PAL_FORGRND, PAL_DSKTOP2, "Volume ramping");
}

/* ============ DRAW CONFIG LAYOUT TAB ============ */

static void showConfigLayout(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

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

	widgets->checkBoxChecked[CB_CONF_PATTSTRETCH] = cfg->ptnStretch;
	showCheckBox(widgets, video, bmp, CB_CONF_PATTSTRETCH);
	textOutShadow(video, bmp, 130, 16, PAL_FORGRND, PAL_DSKTOP2, "Pattern stretch");

	widgets->checkBoxChecked[CB_CONF_HEXCOUNT] = cfg->ptnHex;
	showCheckBox(widgets, video, bmp, CB_CONF_HEXCOUNT);
	textOutShadow(video, bmp, 130, 29, PAL_FORGRND, PAL_DSKTOP2, "Hex line numbers");

	widgets->checkBoxChecked[CB_CONF_ACCIDENTAL] = cfg->ptnAcc;
	showCheckBox(widgets, video, bmp, CB_CONF_ACCIDENTAL);
	textOutShadow(video, bmp, 130, 42, PAL_FORGRND, PAL_DSKTOP2, "Accidential");

	widgets->checkBoxChecked[CB_CONF_SHOWZEROS] = cfg->ptnInstrZero;
	showCheckBox(widgets, video, bmp, CB_CONF_SHOWZEROS);
	textOutShadow(video, bmp, 130, 55, PAL_FORGRND, PAL_DSKTOP2, "Show zeroes");

	widgets->checkBoxChecked[CB_CONF_FRAMEWORK] = cfg->ptnFrmWrk;
	showCheckBox(widgets, video, bmp, CB_CONF_FRAMEWORK);
	textOutShadow(video, bmp, 130, 68, PAL_FORGRND, PAL_DSKTOP2, "Framework");

	widgets->checkBoxChecked[CB_CONF_LINECOLORS] = cfg->ptnLineLight;
	showCheckBox(widgets, video, bmp, CB_CONF_LINECOLORS);
	textOutShadow(video, bmp, 130, 81, PAL_FORGRND, PAL_DSKTOP2, "Line number colors");

	widgets->checkBoxChecked[CB_CONF_CHANNUMS] = cfg->ptnChnNumbers;
	showCheckBox(widgets, video, bmp, CB_CONF_CHANNUMS);
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

	widgets->checkBoxChecked[CB_CONF_SHOWVOLCOL] = cfg->ptnShowVolColumn;
	showCheckBox(widgets, video, bmp, CB_CONF_SHOWVOLCOL);
	textOutShadow(video, bmp, 271, 16, PAL_FORGRND, PAL_DSKTOP2, "Show volume column");

	textOutShadow(video, bmp, 256, 30, PAL_FORGRND, PAL_DSKTOP2, "Maximum visible chn.:");

	setLayoutConfigRadioButtonStates(widgets, cfg);
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_PATTERN_CHANS);
	textOutShadow(video, bmp, 272, 43, PAL_FORGRND, PAL_DSKTOP2, "4 channels");
	textOutShadow(video, bmp, 272, 57, PAL_FORGRND, PAL_DSKTOP2, "6 channels");
	textOutShadow(video, bmp, 272, 71, PAL_FORGRND, PAL_DSKTOP2, "8 channels");
	textOutShadow(video, bmp, 272, 85, PAL_FORGRND, PAL_DSKTOP2, "12 channels");

	/* Pattern font section - ACTIVE */
	textOutShadow(video, bmp, 257, 101, PAL_FORGRND, PAL_DSKTOP2, "Pattern font:");
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_FONT);
	textOutShadow(video, bmp, 272, 115, PAL_FORGRND, PAL_DSKTOP2, "Capitals");
	textOutShadow(video, bmp, 338, 114, PAL_FORGRND, PAL_DSKTOP2, "Lower-c.");
	textOutShadow(video, bmp, 272, 130, PAL_FORGRND, PAL_DSKTOP2, "Future");
	textOutShadow(video, bmp, 338, 129, PAL_FORGRND, PAL_DSKTOP2, "Bold");

	/* Scopes section - ACTIVE */
	textOutShadow(video, bmp, 256, 146, PAL_FORGRND, PAL_DSKTOP2, "Scopes:");
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_SCOPE);
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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

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
	widgets->checkBoxChecked[CB_CONF_SAMPCUTBUF] = cfg->smpCutToBuffer;
	showCheckBox(widgets, video, bmp, CB_CONF_SAMPCUTBUF);
	textOutShadow(video, bmp, 228, 4, PAL_FORGRND, PAL_DSKTOP2, "Sample \"cut to buffer\"");

	widgets->checkBoxChecked[CB_CONF_PATTCUTBUF] = cfg->ptnCutToBuffer;
	showCheckBox(widgets, video, bmp, CB_CONF_PATTCUTBUF);
	textOutShadow(video, bmp, 228, 17, PAL_FORGRND, PAL_DSKTOP2, "Pattern \"cut to buffer\"");

	/* Kill voices at music stop - ACTIVE */
	widgets->checkBoxChecked[CB_CONF_KILLNOTES] = cfg->killNotesOnStopPlay;
	showCheckBox(widgets, video, bmp, CB_CONF_KILLNOTES);
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

	widgets->checkBoxChecked[CB_CONF_MULTICHAN_REC] = cfg->multiRec;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_REC);
	textOutShadow(video, bmp, 228, 70, PAL_FORGRND, PAL_DSKTOP2, "Multichannel record");

	widgets->checkBoxChecked[CB_CONF_MULTICHAN_KEYJAZZ] = cfg->multiKeyJazz;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_KEYJAZZ);
	textOutShadow(video, bmp, 228, 83, PAL_FORGRND, PAL_DSKTOP2, "Multichannel \"key jazz\"");

	widgets->checkBoxChecked[CB_CONF_MULTICHAN_EDIT] = cfg->multiEdit;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_EDIT);
	textOutShadow(video, bmp, 228, 96, PAL_FORGRND, PAL_DSKTOP2, "Multichannel edit");

	widgets->checkBoxChecked[CB_CONF_REC_KEYOFF] = cfg->recRelease;
	showCheckBox(widgets, video, bmp, CB_CONF_REC_KEYOFF);
	textOutShadow(video, bmp, 228, 109, PAL_FORGRND, PAL_DSKTOP2, "Record key-off notes");

	widgets->checkBoxChecked[CB_CONF_QUANTIZE] = cfg->recQuant;
	showCheckBox(widgets, video, bmp, CB_CONF_QUANTIZE);
	textOutShadow(video, bmp, 228, 122, PAL_FORGRND, PAL_DSKTOP2, "Quantization");

	/* Quantization value */
	textOutShadow(video, bmp, 338, 122, PAL_FORGRND, PAL_DSKTOP2, "1/");
	char quantStr[8];
	snprintf(quantStr, sizeof(quantStr), "%d", cfg->recQuantRes);
	textOutShadow(video, bmp, 350, 122, PAL_FORGRND, PAL_DSKTOP2, quantStr);

	widgets->checkBoxChecked[CB_CONF_CHANGE_PATTLEN] = cfg->recTrueInsert;
	showCheckBox(widgets, video, bmp, CB_CONF_CHANGE_PATTLEN);
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

static void showConfigIORouting(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Draw main content framework */
	drawFramework(video, 110, 0, 522, 173, FRAMEWORK_TYPE1);

	/* Title */
	textOutShadow(video, bmp, 116, 4, PAL_FORGRND, PAL_DSKTOP2, "Channel Output Routing:");
	textOutShadow(video, bmp, 116, 16, PAL_FORGRND, PAL_DSKTOP2, "Map each tracker channel (1-32) to an output bus (1-15) and/or to the main mix.");

	/* Column headers - "Ch" aligned with channel numbers, "Out" and "M" (Main) headers */
	textOutShadow(video, bmp, 120, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 152, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 210, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");
	textOutShadow(video, bmp, 280, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 312, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 370, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");
	textOutShadow(video, bmp, 440, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 472, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 530, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");

	/* Draw 32 channel routing assignments in 3 columns */
	char buf[16];
	for (int i = 0; i < 32; i++)
	{
		int col = i / 11;  /* 0, 1, 2 */
		int row = i % 11;
		int baseX = 120 + col * 160;
		int baseY = 43 + row * 11;  /* 1px higher than before */

		/* Channel number */
		sprintf(buf, "%2d:", i + 1);
		textOutShadow(video, bmp, baseX, baseY, PAL_FORGRND, PAL_DSKTOP2, buf);

		/* Output assignment (1-15) */
		sprintf(buf, "%2d", cfg->channelRouting[i] + 1);
		textOutShadow(video, bmp, baseX + 32, baseY, PAL_FORGRND, PAL_DSKTOP2, buf);

		/* Show up/down buttons */
		showPushButton(widgets, video, bmp, PB_CONFIG_ROUTING_CH1_UP + (i * 2));
		showPushButton(widgets, video, bmp, PB_CONFIG_ROUTING_CH1_DOWN + (i * 2));

		/* Show "to main" checkbox */
		widgets->checkBoxChecked[CB_CONF_ROUTING_CH1_TOMAIN + i] = cfg->channelToMain[i];
		showCheckBox(widgets, video, bmp, CB_CONF_ROUTING_CH1_TOMAIN + i);
	}
}

static void showConfigMidiInput(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Determine what's enabled */
	const bool midiEnabled = cfg->midiEnabled;
	const bool notesMode = !cfg->midiTriggerPatterns;
	const bool noteSettingsEnabled = midiEnabled && notesMode;

	/* Colors for enabled/disabled states */
	const uint8_t triggerColor = midiEnabled ? PAL_FORGRND : PAL_DSKTOP2;
	const uint8_t noteSettingsColor = noteSettingsEnabled ? PAL_FORGRND : PAL_DSKTOP2;

	/* Draw main content framework */
	drawFramework(video, 110, 0, 522, 173, FRAMEWORK_TYPE1);

	/* Title */
	textOutShadow(video, bmp, 116, 4, PAL_FORGRND, PAL_DSKTOP2, "MIDI Input Settings:");

	/* MIDI Enable checkbox - always active */
	textOutShadow(video, bmp, 131, 20, PAL_FORGRND, PAL_DSKTOP2, "Enable MIDI input");
	widgets->checkBoxChecked[CB_CONF_MIDI_ENABLE] = cfg->midiEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_ENABLE);

	/* Notes trigger mode (moved up, below Enable) - active if MIDI enabled */
	textOutShadow(video, bmp, 116, 36, triggerColor, PAL_DSKTOP2, "Notes trigger:");
	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_TRIGGER);
	if (cfg->midiTriggerPatterns)
		widgets->radioButtonState[RB_CONFIG_MIDI_PATTERNS] = RADIOBUTTON_CHECKED;
	else
		widgets->radioButtonState[RB_CONFIG_MIDI_NOTES] = RADIOBUTTON_CHECKED;
	if (midiEnabled)
	{
		showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_MIDI_TRIGGER);
		textOutShadow(video, bmp, 195, 36, PAL_FORGRND, PAL_DSKTOP2, "Notes");
		textOutShadow(video, bmp, 258, 36, PAL_FORGRND, PAL_DSKTOP2, "Patterns");
	}
	else
	{
		textOutShadow(video, bmp, 195, 36, PAL_DSKTOP2, PAL_DSKTOP2, "Notes");
		textOutShadow(video, bmp, 258, 36, PAL_DSKTOP2, PAL_DSKTOP2, "Patterns");
	}

	/* All channels checkbox - active if MIDI enabled AND notes mode */
	textOutShadow(video, bmp, 131, 50, noteSettingsColor, PAL_DSKTOP2, "Receive all channels");
	widgets->checkBoxChecked[CB_CONF_MIDI_ALLCHN] = cfg->midiAllChannels;
	if (noteSettingsEnabled)
		showCheckBox(widgets, video, bmp, CB_CONF_MIDI_ALLCHN);

	/* Channel number with scrollbar - active if MIDI enabled AND notes mode */
	textOutShadow(video, bmp, 116, 68, noteSettingsColor, PAL_DSKTOP2, "Channel:");
	if (noteSettingsEnabled)
	{
		setScrollBarPos(inst, widgets, video, SB_MIDI_CHANNEL, cfg->midiChannel - 1, false);
		showScrollBar(widgets, video, SB_MIDI_CHANNEL);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDICHN_DOWN);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDICHN_UP);
	}
	char chnStr[8];
	snprintf(chnStr, sizeof(chnStr), "%2d", cfg->midiChannel);
	textOutShadow(video, bmp, 304, 68, noteSettingsColor, PAL_DSKTOP2, chnStr);

	/* Transpose with scrollbar - active if MIDI enabled AND notes mode */
	textOutShadow(video, bmp, 116, 84, noteSettingsColor, PAL_DSKTOP2, "Transpose:");
	if (noteSettingsEnabled)
	{
		setScrollBarPos(inst, widgets, video, SB_MIDI_TRANSPOSE, cfg->midiTranspose + 48, false);
		showScrollBar(widgets, video, SB_MIDI_TRANSPOSE);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDITRANS_DOWN);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDITRANS_UP);
	}
	char transStr[8];
	if (cfg->midiTranspose >= 0)
		snprintf(transStr, sizeof(transStr), "+%d", cfg->midiTranspose);
	else
		snprintf(transStr, sizeof(transStr), "%d", cfg->midiTranspose);
	textOutShadow(video, bmp, 304, 84, noteSettingsColor, PAL_DSKTOP2, transStr);

	/* Velocity sensitivity with scrollbar - active if MIDI enabled AND notes mode */
	textOutShadow(video, bmp, 116, 100, noteSettingsColor, PAL_DSKTOP2, "Velocity sens.:");
	if (noteSettingsEnabled)
	{
		setScrollBarPos(inst, widgets, video, SB_MIDI_SENS, cfg->midiVelocitySens, false);
		showScrollBar(widgets, video, SB_MIDI_SENS);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDISENS_DOWN);
		showPushButton(widgets, video, bmp, PB_CONFIG_MIDISENS_UP);
	}
	char sensStr[8];
	snprintf(sensStr, sizeof(sensStr), "%3d", cfg->midiVelocitySens);
	textOutShadow(video, bmp, 304, 100, noteSettingsColor, PAL_DSKTOP2, sensStr);
	charOutShadow(video, bmp, 328, 100, noteSettingsColor, PAL_DSKTOP2, '%');

	/* Record velocity checkbox - active if MIDI enabled AND notes mode */
	textOutShadow(video, bmp, 131, 114, noteSettingsColor, PAL_DSKTOP2, "Record velocity as volume");
	widgets->checkBoxChecked[CB_CONF_MIDI_VELOCITY] = cfg->midiRecordVelocity;
	if (noteSettingsEnabled)
		showCheckBox(widgets, video, bmp, CB_CONF_MIDI_VELOCITY);
}

/* ============ MAIN DRAW FUNCTION ============ */

void drawConfigScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL)
		return;

	/* Clear the top screen area (0-173 pixels) */
	clearRect(video, 0, 0, 632, 173);

	/* Draw left sidebar framework */
	drawFramework(video, 0, 0, 110, 173, FRAMEWORK_TYPE1);
	
	/* Set and show config tab radio buttons */
	setConfigRadioButtonStates(widgets, &inst->config);
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_SELECT);

	/* Show push buttons */
	showPushButton(widgets, video, bmp, PB_CONFIG_RESET);
	showPushButton(widgets, video, bmp, PB_CONFIG_LOAD);
	showPushButton(widgets, video, bmp, PB_CONFIG_SAVE);
	showPushButton(widgets, video, bmp, PB_CONFIG_EXIT);

	/* Draw text labels for tabs */
	textOutShadow(video, bmp, 4, 4, PAL_FORGRND, PAL_DSKTOP2, "Configuration:");
	textOutShadow(video, bmp, 21, 19, PAL_FORGRND, PAL_DSKTOP2, "Audio");
	textOutShadow(video, bmp, 21, 35, PAL_FORGRND, PAL_DSKTOP2, "Layout");
	textOutShadow(video, bmp, 21, 51, PAL_FORGRND, PAL_DSKTOP2, "Miscellaneous");
	textOutShadow(video, bmp, 21, 67, PAL_FORGRND, PAL_DSKTOP2, "MIDI input");
	textOutShadow(video, bmp, 21, 83, PAL_FORGRND, PAL_DSKTOP2, "I/O Routing");

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
		case CONFIG_SCREEN_IO_ROUTING:
			showConfigIORouting(inst, video, bmp);
			break;
		case CONFIG_SCREEN_MIDI_INPUT:
			showConfigMidiInput(inst, video, bmp);
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

void rbConfigIORouting(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_IO_ROUTING;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMidiInput(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_MIDI_INPUT;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

/* ============ MIDI TRIGGER MODE CALLBACKS ============ */

void rbConfigMidiTriggerNotes(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.midiTriggerPatterns = false;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_NOTES);
}

/* Callback for sync settings warning dialog */
static void onMidiPatternSyncWarningResult(ft2_instance_t *inst,
	ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (result == DIALOG_RESULT_YES)
	{
		/* User wants to disable sync settings */
		inst->config.syncTransportFromDAW = false;
		inst->config.syncPositionFromDAW = false;
	}

	/* Enable pattern trigger mode either way */
	inst->config.midiTriggerPatterns = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_PATTERNS);
}

void rbConfigMidiTriggerPatterns(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Check if sync settings are enabled */
	if (inst->config.syncTransportFromDAW || inst->config.syncPositionFromDAW)
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL)
		{
			ft2_dialog_show_yesno_cb(&ui->dialog,
				"System request",
				"For consistent playback, turn off \"Sync transport\" and \"Sync position\" in audio settings?",
				inst, onMidiPatternSyncWarningResult, NULL);
			return;
		}
	}

	/* No sync settings enabled, just enable pattern mode */
	inst->config.midiTriggerPatterns = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_PATTERNS);
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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_NONE);
}

void rbConfigIntrpLinear(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_LINEAR);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_LINEAR);
}

void rbConfigIntrpQuadratic(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_QUADRATIC);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_QUADRATIC);
}

void rbConfigIntrpCubic(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_CUBIC);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_CUBIC);
}

void rbConfigIntrpSinc8(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_SINC8);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_SINC8);
}

void rbConfigIntrpSinc16(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	setInterpolationType(inst, INTERPOLATION_SINC16);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_SINC16);
}

/* ============ SCOPE STYLE CALLBACKS ============ */

void rbConfigScopeFT2(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.linedScopes = false;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_SCOPE_STANDARD);
}

void rbConfigScopeLined(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->config.linedScopes = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_SCOPE_LINED);
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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_4CHANS);
}

void rbConfigPatt6Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_6;
	inst->uiState.maxVisibleChannels = 6;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_6CHANS);
}

void rbConfigPatt8Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_8;
	inst->uiState.maxVisibleChannels = 8;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_8CHANS);
}

void rbConfigPatt12Chans(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_12;
	inst->uiState.maxVisibleChannels = 12;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_12CHANS);
}

/* ============ FONT CALLBACKS ============ */

void rbConfigFontCapitals(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_CAPITALS);
}

void rbConfigFontLowerCase(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_LOWERCASE);
}

void rbConfigFontFuture(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_FUTURE);
}

void rbConfigFontBold(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->config.ptnFont = PATT_FONT_BOLD;
	inst->uiState.ptnFont = PATT_FONT_BOLD;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_BOLD);
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
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	scrollBarScrollLeft(inst, widgets, video, SB_AMP_SCROLL, 1);
}

void configAmpUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	scrollBarScrollRight(inst, widgets, video, SB_AMP_SCROLL, 1);
}

void configMasterVolDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	scrollBarScrollLeft(inst, widgets, video, SB_MASTERVOL_SCROLL, 1);
}

void configMasterVolUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	scrollBarScrollRight(inst, widgets, video, SB_MASTERVOL_SCROLL, 1);
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
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Overwrite global config?",
			inst, onSaveGlobalConfigResult, NULL);
	}
}

/* ============ CHANNEL OUTPUT ROUTING CALLBACKS ============ */

static void routingUp(ft2_instance_t *inst, int channel)
{
	if (inst == NULL || channel < 0 || channel >= 32)
		return;

	if (inst->config.channelRouting[channel] < FT2_NUM_OUTPUTS - 1)
		inst->config.channelRouting[channel]++;
	else
		inst->config.channelRouting[channel] = 0;

	inst->uiState.needsFullRedraw = true;
}

static void routingDown(ft2_instance_t *inst, int channel)
{
	if (inst == NULL || channel < 0 || channel >= 32)
		return;

	if (inst->config.channelRouting[channel] > 0)
		inst->config.channelRouting[channel]--;
	else
		inst->config.channelRouting[channel] = FT2_NUM_OUTPUTS - 1;

	inst->uiState.needsFullRedraw = true;
}

/* Generate callbacks for all 32 channels */
#define ROUTING_CALLBACK(n) \
	void pbRoutingCh##n##Up(ft2_instance_t *inst) { routingUp(inst, n - 1); } \
	void pbRoutingCh##n##Down(ft2_instance_t *inst) { routingDown(inst, n - 1); }

ROUTING_CALLBACK(1)
ROUTING_CALLBACK(2)
ROUTING_CALLBACK(3)
ROUTING_CALLBACK(4)
ROUTING_CALLBACK(5)
ROUTING_CALLBACK(6)
ROUTING_CALLBACK(7)
ROUTING_CALLBACK(8)
ROUTING_CALLBACK(9)
ROUTING_CALLBACK(10)
ROUTING_CALLBACK(11)
ROUTING_CALLBACK(12)
ROUTING_CALLBACK(13)
ROUTING_CALLBACK(14)
ROUTING_CALLBACK(15)
ROUTING_CALLBACK(16)
ROUTING_CALLBACK(17)
ROUTING_CALLBACK(18)
ROUTING_CALLBACK(19)
ROUTING_CALLBACK(20)
ROUTING_CALLBACK(21)
ROUTING_CALLBACK(22)
ROUTING_CALLBACK(23)
ROUTING_CALLBACK(24)
ROUTING_CALLBACK(25)
ROUTING_CALLBACK(26)
ROUTING_CALLBACK(27)
ROUTING_CALLBACK(28)
ROUTING_CALLBACK(29)
ROUTING_CALLBACK(30)
ROUTING_CALLBACK(31)
ROUTING_CALLBACK(32)

/* Callback for "to main" checkboxes - syncs checkbox state to config */
void cbRoutingToMain(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL)
		return;
	ft2_widgets_t *widgets = &ui->widgets;

	/* Sync all 32 checkbox states to config */
	for (int i = 0; i < 32; i++)
	{
		uint16_t cbId = CB_CONF_ROUTING_CH1_TOMAIN + i;
		if (widgets->checkBoxVisible[cbId])
			inst->config.channelToMain[i] = widgets->checkBoxChecked[cbId];
	}
}

/* ============ MIDI INPUT CALLBACKS ============ */

static void drawMidiChannelValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || 
	    inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return;

	char str[8];
	snprintf(str, sizeof(str), "%2d", inst->config.midiChannel);
	fillRect(&ui->video, 304, 52, 16, 8, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 304, 52, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawMidiTransposeValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || 
	    inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return;

	char str[8];
	int8_t val = inst->config.midiTranspose;
	if (val >= 0)
		snprintf(str, sizeof(str), "+%d", val);
	else
		snprintf(str, sizeof(str), "%d", val);
	fillRect(&ui->video, 304, 68, 24, 8, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 304, 68, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawMidiSensValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || 
	    inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return;

	char str[8];
	snprintf(str, sizeof(str), "%3d", inst->config.midiVelocitySens);
	fillRect(&ui->video, 304, 84, 24, 8, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 304, 84, PAL_FORGRND, PAL_DSKTOP2, str);
}

void configMidiChnDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollLeft(inst, widgets, video, SB_MIDI_CHANNEL, 1);
}

void configMidiChnUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollRight(inst, widgets, video, SB_MIDI_CHANNEL, 1);
}

void sbMidiChannel(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;

	uint8_t newCh = (uint8_t)(pos + 1);  /* 0-15 -> 1-16 */
	if (newCh < 1) newCh = 1;
	if (newCh > 16) newCh = 16;

	if (inst->config.midiChannel != newCh)
	{
		inst->config.midiChannel = newCh;
		drawMidiChannelValue(inst);
	}
}

void configMidiTransDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollLeft(inst, widgets, video, SB_MIDI_TRANSPOSE, 1);
}

void configMidiTransUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollRight(inst, widgets, video, SB_MIDI_TRANSPOSE, 1);
}

void sbMidiTranspose(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;

	int8_t newTrans = (int8_t)((int32_t)pos - 48);  /* 0-96 -> -48 to +48 */
	if (newTrans < -48) newTrans = -48;
	if (newTrans > 48) newTrans = 48;

	if (inst->config.midiTranspose != newTrans)
	{
		inst->config.midiTranspose = newTrans;
		drawMidiTransposeValue(inst);
	}
}

void configMidiSensDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollLeft(inst, widgets, video, SB_MIDI_SENS, 1);
}

void configMidiSensUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = (ui != NULL) ? &ui->video : NULL;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;

	scrollBarScrollRight(inst, widgets, video, SB_MIDI_SENS, 1);
}

void sbMidiSens(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL)
		return;

	uint16_t newSens = (uint16_t)pos;
	if (newSens > 200) newSens = 200;

	if (inst->config.midiVelocitySens != newSens)
	{
		inst->config.midiVelocitySens = newSens;
		drawMidiSensValue(inst);
	}
}
