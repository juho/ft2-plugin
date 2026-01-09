/**
 * @file ft2_plugin_config.c
 * @brief Configuration screen: tabs for Audio, Layout, Misc, I/O Routing, MIDI.
 *
 * Plugin-specific additions vs standalone:
 *   - DAW sync options (BPM, transport, position)
 *   - I/O routing (map 32 tracker channels to 15 output buses + main mix)
 *   - MIDI input with pattern trigger mode
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

/* Initialize config with FT2 defaults. Called once per instance at creation. */
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

	/* Disk operation defaults */
	config->dirSortPriority = 0;    /* 0 = extension first (default) */
	config->overwriteWarning = true;

	/* DAW sync defaults (all enabled by default) */
	config->syncBpmFromDAW = true;
	config->syncTransportFromDAW = true;
	config->syncPositionFromDAW = true;
	config->allowFxxSpeedChanges = true;
	config->savedSpeed = 6;  /* Default speed */
	config->savedBpm = 125;  /* Default BPM */
	config->lockedSpeed = 6; /* Default locked speed */

	/* MIDI Input */
	config->midiEnabled = true;
	config->midiAllChannels = true;
	config->midiChannel = 1;
	config->midiRecordTranspose = true;
	config->midiTranspose = 0;
	config->midiVelocitySens = 100;
	config->midiRecordVelocity = true;
	config->midiRecordAftertouch = false;
	config->midiRecordModWheel = false;
	config->midiRecordPitchBend = false;
	config->midiRecordPriority = 0;  /* Pitch bend priority by default */
	config->midiModRange = 8;        /* Mid-range vibrato depth by default */
	config->midiBendRange = 2;       /* 2 semitones by default */

	/* Miscellaneous */
	config->autoUpdateCheck = true;  /* Enabled by default */

	/* Palette */
	config->palettePreset = PAL_ARCTIC;

	/* Start on audio screen (matches standalone) */
	config->currConfigScreen = CONFIG_SCREEN_AUDIO;

	/*
	 * Envelope presets 1-6: match standalone defConfigData.
	 * Volume envelope: attack-decay shape. Panning: subtle sweep to center.
	 */
	for (int i = 0; i < 6; i++)
	{
		/* Volume envelope: 6 points, attack-decay shape */
		config->stdEnvPoints[i][0][0][0] = 0;  config->stdEnvPoints[i][0][0][1] = 48;
		config->stdEnvPoints[i][0][1][0] = 4;  config->stdEnvPoints[i][0][1][1] = 64;
		config->stdEnvPoints[i][0][2][0] = 8;  config->stdEnvPoints[i][0][2][1] = 44;
		config->stdEnvPoints[i][0][3][0] = 14; config->stdEnvPoints[i][0][3][1] = 8;
		config->stdEnvPoints[i][0][4][0] = 24; config->stdEnvPoints[i][0][4][1] = 22;
		config->stdEnvPoints[i][0][5][0] = 32; config->stdEnvPoints[i][0][5][1] = 8;
		for (int p = 6; p < 12; p++) {
			config->stdEnvPoints[i][0][p][0] = 0;
			config->stdEnvPoints[i][0][p][1] = 0;
		}
		config->stdVolEnvLength[i] = 6;
		config->stdVolEnvSustain[i] = 2;
		config->stdVolEnvLoopStart[i] = 3;
		config->stdVolEnvLoopEnd[i] = 5;
		config->stdVolEnvFlags[i] = 0;

		/* Panning envelope: 6 points, subtle sweep settling to center (Y=32) */
		config->stdEnvPoints[i][1][0][0] = 0;  config->stdEnvPoints[i][1][0][1] = 32;
		config->stdEnvPoints[i][1][1][0] = 10; config->stdEnvPoints[i][1][1][1] = 40;
		config->stdEnvPoints[i][1][2][0] = 30; config->stdEnvPoints[i][1][2][1] = 24;
		config->stdEnvPoints[i][1][3][0] = 50; config->stdEnvPoints[i][1][3][1] = 32;
		config->stdEnvPoints[i][1][4][0] = 60; config->stdEnvPoints[i][1][4][1] = 32;
		config->stdEnvPoints[i][1][5][0] = 70; config->stdEnvPoints[i][1][5][1] = 32;
		for (int p = 6; p < 12; p++) {
			config->stdEnvPoints[i][1][p][0] = 0;
			config->stdEnvPoints[i][1][p][1] = 0;
		}
		config->stdPanEnvLength[i] = 6;
		config->stdPanEnvSustain[i] = 2;
		config->stdPanEnvLoopStart[i] = 3;
		config->stdPanEnvLoopEnd[i] = 5;
		config->stdPanEnvFlags[i] = 0;

		config->stdFadeout[i] = 128;
		config->stdVibRate[i] = 0;
		config->stdVibDepth[i] = 0;
		config->stdVibSweep[i] = 0;
		config->stdVibType[i] = 0;
	}

	/* Channel routing: wrap 32 channels across 15 outputs, all to main mix */
	for (int i = 0; i < 32; i++) {
		config->channelRouting[i] = i % FT2_NUM_OUTPUTS;
		config->channelToMain[i] = true;
	}
}

/* Copy config values to instance state. Called after loading config or changing tabs. */
void ft2_config_apply(ft2_instance_t *inst, ft2_plugin_config_t *config)
{
	if (inst == NULL || config == NULL)
		return;

	/* Pattern editor */
	inst->uiState.ptnStretch = config->ptnStretch;
	inst->uiState.ptnHex = config->ptnHex;
	inst->uiState.ptnInstrZero = config->ptnInstrZero;
	inst->uiState.ptnFrmWrk = config->ptnFrmWrk;
	inst->uiState.ptnLineLight = config->ptnLineLight;
	inst->uiState.ptnShowVolColumn = config->ptnShowVolColumn;
	inst->uiState.ptnChnNumbers = config->ptnChnNumbers;
	inst->uiState.ptnAcc = config->ptnAcc;
	inst->uiState.ptnFont = config->ptnFont;

	/* Audio/mixer */
	inst->audio.interpolationType = config->interpolation;
	inst->audio.volumeRampingFlag = config->volumeRamp;

	switch (config->ptnMaxChannels)
	{
		case MAX_CHANS_SHOWN_4:  inst->uiState.maxVisibleChannels = 4;  break;
		case MAX_CHANS_SHOWN_6:  inst->uiState.maxVisibleChannels = 6;  break;
		case MAX_CHANS_SHOWN_8:  inst->uiState.maxVisibleChannels = 8;  break;
		case MAX_CHANS_SHOWN_12: inst->uiState.maxVisibleChannels = 12; break;
		default:                 inst->uiState.maxVisibleChannels = 8;  break;
	}
}

/* ---------- Radio button state sync ---------- */

/* Sync tab selector radio buttons to current config screen */
static void setConfigRadioButtonStates(ft2_widgets_t *widgets, ft2_plugin_config_t *config)
{
	if (widgets == NULL) return;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_SELECT);
	uint16_t tmpID;
	switch (config->currConfigScreen) {
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
	if (widgets == NULL) return;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	switch (config->interpolation) {
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
	if (widgets == NULL) return;

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_PATTERN_CHANS);
	switch (config->ptnMaxChannels) {
		case MAX_CHANS_SHOWN_4:  widgets->radioButtonState[RB_CONFIG_PATT_4CHANS] = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_6:  widgets->radioButtonState[RB_CONFIG_PATT_6CHANS] = RADIOBUTTON_CHECKED; break;
		default:
		case MAX_CHANS_SHOWN_8:  widgets->radioButtonState[RB_CONFIG_PATT_8CHANS] = RADIOBUTTON_CHECKED; break;
		case MAX_CHANS_SHOWN_12: widgets->radioButtonState[RB_CONFIG_PATT_12CHANS] = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_FONT);
	switch (config->ptnFont) {
		default:
		case PATT_FONT_CAPITALS:  widgets->radioButtonState[RB_CONFIG_FONT_CAPITALS] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_LOWERCASE: widgets->radioButtonState[RB_CONFIG_FONT_LOWERCASE] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_FUTURE:    widgets->radioButtonState[RB_CONFIG_FONT_FUTURE] = RADIOBUTTON_CHECKED; break;
		case PATT_FONT_BOLD:      widgets->radioButtonState[RB_CONFIG_FONT_BOLD] = RADIOBUTTON_CHECKED; break;
	}

	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_SCOPE);
	widgets->radioButtonState[config->linedScopes ? RB_CONFIG_SCOPE_LINED : RB_CONFIG_SCOPE_STANDARD] = RADIOBUTTON_CHECKED;
}

/* ---------- Screen visibility ---------- */

/* Hide all config widgets. Called when switching tabs or exiting config. */
void hideConfigScreen(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) {
		inst->uiState.configScreenShown = false;
		return;
	}

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
	hideCheckBox(widgets, CB_CONF_AUTO_UPDATE_CHECK);
	hideCheckBox(widgets, CB_CONF_MIDI_ENABLE);
	hideCheckBox(widgets, CB_CONF_MIDI_ALLCHN);
	hideCheckBox(widgets, CB_CONF_MIDI_TRANSP);
	hideCheckBox(widgets, CB_CONF_MIDI_VELOCITY);
	hideCheckBox(widgets, CB_CONF_MIDI_AFTERTOUCH);
	hideCheckBox(widgets, CB_CONF_VSYNC_OFF);
	hideCheckBox(widgets, CB_CONF_FULLSCREEN);
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
	hidePushButton(widgets, PB_CONFIG_MODRANGE_UP);
	hidePushButton(widgets, PB_CONFIG_MODRANGE_DOWN);
	hidePushButton(widgets, PB_CONFIG_BENDRANGE_UP);
	hidePushButton(widgets, PB_CONFIG_BENDRANGE_DOWN);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_TRIGGER);
	hideRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_PRIORITY);
	hideCheckBox(widgets, CB_CONF_MIDI_MODWHEEL);
	hideCheckBox(widgets, CB_CONF_MIDI_PITCHBEND);

	/* CONFIG I/O ROUTING */
	for (int i = 0; i < 32; i++)
	{
		hidePushButton(widgets, PB_CONFIG_ROUTING_CH1_UP + (i * 2));
		hidePushButton(widgets, PB_CONFIG_ROUTING_CH1_DOWN + (i * 2));
		hideCheckBox(widgets, CB_CONF_ROUTING_CH1_TOMAIN + i);
	}

	inst->uiState.configScreenShown = false;
}

void showConfigScreen(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->uiState.configScreenShown = true;
	inst->uiState.scopesShown = false;
}

void exitConfigScreen(ft2_instance_t *inst)
{
	hideConfigScreen(inst);
	inst->uiState.scopesShown = true;
}

/* ---------- Audio tab ---------- */

/*
 * Audio config: DAW sync, interpolation, amplification, frequency slides.
 * Plugin diverges from standalone: no device selection, adds DAW sync options.
 */
static void showConfigAudio(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	/* Framework boxes */
	drawFramework(video, 110, 0, 522, 87, FRAMEWORK_TYPE1);
	drawFramework(video, 110, 87, 138, 86, FRAMEWORK_TYPE1);
	drawFramework(video, 248, 87, 153, 86, FRAMEWORK_TYPE1);
	drawFramework(video, 401, 87, 231, 86, FRAMEWORK_TYPE1);

	/* DAW Sync (plugin-specific) */
	textOutShadow(video, bmp, 114, 4, PAL_FORGRND, PAL_DSKTOP2, "DAW Sync:");

	widgets->checkBoxChecked[CB_CONF_SYNC_BPM] = cfg->syncBpmFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_BPM);
	textOutShadow(video, bmp, 131, 21, PAL_FORGRND, PAL_DSKTOP2, "Sync BPM");

	widgets->checkBoxChecked[CB_CONF_SYNC_TRANSPORT] = cfg->syncTransportFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_TRANSPORT);
	textOutShadow(video, bmp, 131, 37, PAL_FORGRND, PAL_DSKTOP2, "Sync transport (start/stop)");

	widgets->checkBoxChecked[CB_CONF_SYNC_POSITION] = cfg->syncPositionFromDAW;
	showCheckBox(widgets, video, bmp, CB_CONF_SYNC_POSITION);
	textOutShadow(video, bmp, 131, 53, PAL_FORGRND, PAL_DSKTOP2, "Sync position (seek)");

	widgets->checkBoxChecked[CB_CONF_ALLOW_FXX_SPEED] = cfg->allowFxxSpeedChanges;
	showCheckBox(widgets, video, bmp, CB_CONF_ALLOW_FXX_SPEED);
	textOutShadow(video, bmp, 131, 69, PAL_FORGRND, PAL_DSKTOP2, "Allow Fxx speed changes");

	/* Interpolation */
	setAudioConfigRadioButtonStates(widgets, cfg);
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	textOutShadow(video, bmp, 131, 91, PAL_FORGRND, PAL_DSKTOP2, "No interpolation");
	textOutShadow(video, bmp, 131, 105, PAL_FORGRND, PAL_DSKTOP2, "Linear (FT2)");
	textOutShadow(video, bmp, 131, 119, PAL_FORGRND, PAL_DSKTOP2, "Quadratic spline");
	textOutShadow(video, bmp, 131, 133, PAL_FORGRND, PAL_DSKTOP2, "Cubic spline");
	textOutShadow(video, bmp, 131, 147, PAL_FORGRND, PAL_DSKTOP2, "Sinc (8 point)");
	textOutShadow(video, bmp, 131, 161, PAL_FORGRND, PAL_DSKTOP2, "Sinc (16 point)");

	/* Frequency slides: Amiga (hardware period table) vs Linear (FT2 default) */
	textOutShadow(video, bmp, 405, 91, PAL_FORGRND, PAL_DSKTOP2, "Frequency slides:");
	textOutShadow(video, bmp, 420, 105, PAL_FORGRND, PAL_DSKTOP2, "Amiga");
	textOutShadow(video, bmp, 420, 119, PAL_FORGRND, PAL_DSKTOP2, "Linear (default)");
	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_FREQ_SLIDES);
	if (inst->audio.linearPeriodsFlag)
		widgets->radioButtonState[RB_CONFIG_FREQ_LINEAR] = RADIOBUTTON_CHECKED;
	else
		widgets->radioButtonState[RB_CONFIG_FREQ_AMIGA] = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_FREQ_SLIDES);

	/* Amplification: 1x-32x boost applied before master volume */
	textOutShadow(video, bmp, 252, 91, PAL_FORGRND, PAL_DSKTOP2, "Amplification:");
	char ampStr[8];
	snprintf(ampStr, sizeof(ampStr), "%2dx", cfg->boostLevel);
	textOutShadow(video, bmp, 374, 91, PAL_FORGRND, PAL_DSKTOP2, ampStr);
	setScrollBarPos(inst, widgets, video, SB_AMP_SCROLL, cfg->boostLevel - 1, false);
	showScrollBar(widgets, video, SB_AMP_SCROLL);
	showPushButton(widgets, video, bmp, PB_CONFIG_AMP_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_AMP_UP);

	/* Master volume: 0-256 final output scaling */
	textOutShadow(video, bmp, 252, 119, PAL_FORGRND, PAL_DSKTOP2, "Master volume:");
	char volStr[8];
	snprintf(volStr, sizeof(volStr), "%3d", cfg->masterVol);
	textOutShadow(video, bmp, 374, 119, PAL_FORGRND, PAL_DSKTOP2, volStr);
	setScrollBarPos(inst, widgets, video, SB_MASTERVOL_SCROLL, cfg->masterVol, false);
	showScrollBar(widgets, video, SB_MASTERVOL_SCROLL);
	showPushButton(widgets, video, bmp, PB_CONFIG_MASTVOL_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_MASTVOL_UP);

	/* Volume ramping: prevents clicks on note start/stop */
	widgets->checkBoxChecked[CB_CONF_VOLRAMP] = cfg->volumeRamp;
	showCheckBox(widgets, video, bmp, CB_CONF_VOLRAMP);
	textOutShadow(video, bmp, 268, 147, PAL_FORGRND, PAL_DSKTOP2, "Volume ramping");
}

/* ---------- Layout tab ---------- */

/* Layout config: pattern display, fonts, scopes, palette. Matches standalone. */
static void showConfigLayout(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	/* Framework boxes (same layout as standalone) */
	drawFramework(video, 110, 0, 142, 106, FRAMEWORK_TYPE1);
	drawFramework(video, 252, 0, 142, 98, FRAMEWORK_TYPE1);
	drawFramework(video, 394, 0, 238, 86, FRAMEWORK_TYPE1);
	drawFramework(video, 110, 106, 142, 67, FRAMEWORK_TYPE1);
	drawFramework(video, 252, 98, 142, 45, FRAMEWORK_TYPE1);
	drawFramework(video, 394, 86, 238, 87, FRAMEWORK_TYPE1);
	drawFramework(video, 252, 143, 142, 30, FRAMEWORK_TYPE1);

	/* Pattern layout checkboxes */
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

	/* Pattern modes: volume column, max channels */
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

	/* Pattern font: 4 styles from original FT2 */
	textOutShadow(video, bmp, 257, 101, PAL_FORGRND, PAL_DSKTOP2, "Pattern font:");
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_FONT);
	textOutShadow(video, bmp, 272, 115, PAL_FORGRND, PAL_DSKTOP2, "Capitals");
	textOutShadow(video, bmp, 338, 114, PAL_FORGRND, PAL_DSKTOP2, "Lower-c.");
	textOutShadow(video, bmp, 272, 130, PAL_FORGRND, PAL_DSKTOP2, "Future");
	textOutShadow(video, bmp, 338, 129, PAL_FORGRND, PAL_DSKTOP2, "Bold");

	/* Scopes: FT2 (filled) vs Lined (vector) */
	textOutShadow(video, bmp, 256, 146, PAL_FORGRND, PAL_DSKTOP2, "Scopes:");
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_SCOPE);
	textOutShadow(video, bmp, 319, 146, PAL_FORGRND, PAL_DSKTOP2, "FT2");
	textOutShadow(video, bmp, 360, 146, PAL_FORGRND, PAL_DSKTOP2, "Lined");

	/* Palette entry labels (for color editor) */
	textOutShadow(video, bmp, 414, 3, PAL_FORGRND, PAL_DSKTOP2, "Pattern text");
	textOutShadow(video, bmp, 414, 17, PAL_FORGRND, PAL_DSKTOP2, "Block mark");
	textOutShadow(video, bmp, 414, 31, PAL_FORGRND, PAL_DSKTOP2, "Text on block");
	textOutShadow(video, bmp, 414, 45, PAL_FORGRND, PAL_DSKTOP2, "Mouse");
	textOutShadow(video, bmp, 414, 59, PAL_FORGRND, PAL_DSKTOP2, "Desktop");
	textOutShadow(video, bmp, 414, 73, PAL_FORGRND, PAL_DSKTOP2, "Buttons");

	/* Palette presets (12 total, including user-defined) */
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

/* ---------- Miscellaneous tab ---------- */

/* Misc config: cut behavior, file sorting, record/edit options. */
static void showConfigMiscellaneous(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	drawFramework(video, 110, 0, 198, 57, FRAMEWORK_TYPE1);
	drawFramework(video, 308, 0, 324, 57, FRAMEWORK_TYPE1);
	drawFramework(video, 110, 57, 198, 116, FRAMEWORK_TYPE1);
	drawFramework(video, 308, 57, 324, 116, FRAMEWORK_TYPE1);

	/* Cut to buffer: if enabled, cut also copies to clipboard */
	widgets->checkBoxChecked[CB_CONF_SAMPCUTBUF] = cfg->smpCutToBuffer;
	showCheckBox(widgets, video, bmp, CB_CONF_SAMPCUTBUF);
	textOutShadow(video, bmp, 128, 4, PAL_FORGRND, PAL_DSKTOP2, "Sample \"cut to buffer\"");

	widgets->checkBoxChecked[CB_CONF_PATTCUTBUF] = cfg->ptnCutToBuffer;
	showCheckBox(widgets, video, bmp, CB_CONF_PATTCUTBUF);
	textOutShadow(video, bmp, 128, 17, PAL_FORGRND, PAL_DSKTOP2, "Pattern \"cut to buffer\"");

	/* Kill voices: stop all sound immediately when playback stops */
	widgets->checkBoxChecked[CB_CONF_KILLNOTES] = cfg->killNotesOnStopPlay;
	showCheckBox(widgets, video, bmp, CB_CONF_KILLNOTES);
	textOutShadow(video, bmp, 128, 30, PAL_FORGRND, PAL_DSKTOP2, "Kill voices at music stop");

	widgets->checkBoxChecked[CB_CONF_OVERWRITE_WARN] = cfg->overwriteWarning;
	showCheckBox(widgets, video, bmp, CB_CONF_OVERWRITE_WARN);
	textOutShadow(video, bmp, 128, 43, PAL_FORGRND, PAL_DSKTOP2, "File-overwrite warning");

	/* Dir sorting: extension-first groups files by type, name sorts alphabetically */
	textOutShadow(video, bmp, 312, 3, PAL_FORGRND, PAL_DSKTOP2, "Dir. sorting pri.:");
	textOutShadow(video, bmp, 328, 16, PAL_FORGRND, PAL_DSKTOP2, "Ext.");
	textOutShadow(video, bmp, 328, 30, PAL_FORGRND, PAL_DSKTOP2, "Name");
	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_FILESORT);
	if (cfg->dirSortPriority == 0)
		widgets->radioButtonState[RB_CONFIG_FILESORT_EXT] = RADIOBUTTON_CHECKED;
	else
		widgets->radioButtonState[RB_CONFIG_FILESORT_NAME] = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_FILESORT);

	/* Record/Edit options */
	textOutShadow(video, bmp, 114, 59, PAL_FORGRND, PAL_DSKTOP2, "Rec./Edit/Play:");

	widgets->checkBoxChecked[CB_CONF_MULTICHAN_REC] = cfg->multiRec;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_REC);
	textOutShadow(video, bmp, 128, 72, PAL_FORGRND, PAL_DSKTOP2, "Multichannel record");

	/* Multichannel keyjazz: spread polyphonic notes across channels */
	widgets->checkBoxChecked[CB_CONF_MULTICHAN_KEYJAZZ] = cfg->multiKeyJazz;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_KEYJAZZ);
	textOutShadow(video, bmp, 128, 85, PAL_FORGRND, PAL_DSKTOP2, "Multichannel \"key jazz\"");

	widgets->checkBoxChecked[CB_CONF_MULTICHAN_EDIT] = cfg->multiEdit;
	showCheckBox(widgets, video, bmp, CB_CONF_MULTICHAN_EDIT);
	textOutShadow(video, bmp, 128, 98, PAL_FORGRND, PAL_DSKTOP2, "Multichannel edit");

	widgets->checkBoxChecked[CB_CONF_REC_KEYOFF] = cfg->recRelease;
	showCheckBox(widgets, video, bmp, CB_CONF_REC_KEYOFF);
	textOutShadow(video, bmp, 128, 111, PAL_FORGRND, PAL_DSKTOP2, "Record key-off notes");

	/* Quantization: snap recorded notes to grid */
	widgets->checkBoxChecked[CB_CONF_QUANTIZE] = cfg->recQuant;
	showCheckBox(widgets, video, bmp, CB_CONF_QUANTIZE);
	textOutShadow(video, bmp, 128, 124, PAL_FORGRND, PAL_DSKTOP2, "Quantization");

	textOutShadow(video, bmp, 238, 124, PAL_FORGRND, PAL_DSKTOP2, "1/");
	char quantStr[8];
	snprintf(quantStr, sizeof(quantStr), "%d", cfg->recQuantRes);
	textOutShadow(video, bmp, 250, 124, PAL_FORGRND, PAL_DSKTOP2, quantStr);
	showPushButton(widgets, video, bmp, PB_CONFIG_QUANTIZE_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_QUANTIZE_DOWN);

	/* True insert: insert/delete actually changes pattern length */
	widgets->checkBoxChecked[CB_CONF_CHANGE_PATTLEN] = cfg->recTrueInsert;
	showCheckBox(widgets, video, bmp, CB_CONF_CHANGE_PATTLEN);
	textOutShadow(video, bmp, 128, 137, PAL_FORGRND, PAL_DSKTOP2, "Change pattern length when");
	textOutShadow(video, bmp, 128, 148, PAL_FORGRND, PAL_DSKTOP2, "inserting/deleting line.");

	widgets->checkBoxChecked[CB_CONF_AUTO_UPDATE_CHECK] = cfg->autoUpdateCheck;
	showCheckBox(widgets, video, bmp, CB_CONF_AUTO_UPDATE_CHECK);
	textOutShadow(video, bmp, 128, 161, PAL_FORGRND, PAL_DSKTOP2, "Automatic update check");
}

/* ---------- I/O Routing tab (plugin-specific) ---------- */

/*
 * Route 32 tracker channels to 15 output buses plus optional main mix.
 * Each channel can go to one bus (multi-out) and/or to main stereo out.
 * Useful for DAW mixing, stem export, or per-channel processing.
 */
static void showConfigIORouting(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;

	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	drawFramework(video, 110, 0, 522, 173, FRAMEWORK_TYPE1);

	textOutShadow(video, bmp, 116, 4, PAL_FORGRND, PAL_DSKTOP2, "Channel Output Routing:");
	textOutShadow(video, bmp, 116, 16, PAL_FORGRND, PAL_DSKTOP2, "Map each tracker channel (1-32) to an output bus (1-15) and/or to the main mix.");
	textOutShadow(video, bmp, 120, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 152, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 210, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");
	textOutShadow(video, bmp, 280, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 312, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 370, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");
	textOutShadow(video, bmp, 440, 32, PAL_FORGRND, PAL_DSKTOP2, "Ch");
	textOutShadow(video, bmp, 472, 32, PAL_FORGRND, PAL_DSKTOP2, "Out");
	textOutShadow(video, bmp, 530, 32, PAL_FORGRND, PAL_DSKTOP2, "Main");

	/* 32 channels in 3 columns */
	char buf[16];
	for (int i = 0; i < 32; i++) {
		int col = i / 11, row = i % 11;
		int baseX = 120 + col * 160, baseY = 43 + row * 11;

		sprintf(buf, "%2d:", i + 1);
		textOutShadow(video, bmp, baseX, baseY, PAL_FORGRND, PAL_DSKTOP2, buf);

		sprintf(buf, "%2d", cfg->channelRouting[i] + 1);
		textOutShadow(video, bmp, baseX + 32, baseY, PAL_FORGRND, PAL_DSKTOP2, buf);

		showPushButton(widgets, video, bmp, PB_CONFIG_ROUTING_CH1_UP + (i * 2));
		showPushButton(widgets, video, bmp, PB_CONFIG_ROUTING_CH1_DOWN + (i * 2));

		widgets->checkBoxChecked[CB_CONF_ROUTING_CH1_TOMAIN + i] = cfg->channelToMain[i];
		showCheckBox(widgets, video, bmp, CB_CONF_ROUTING_CH1_TOMAIN + i);
	}
}

/* ---------- MIDI Input tab (plugin-specific) ---------- */

/*
 * MIDI input config: channel filter, transpose, velocity/CC recording.
 * Two trigger modes:
 *   Notes: play notes on current instrument (default)
 *   Patterns: use MIDI notes to trigger patterns (for live performance)
 */
static void showConfigMidiInput(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;

	ft2_plugin_config_t *cfg = &inst->config;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	/* Disable note settings when MIDI off or in pattern-trigger mode */
	const bool midiEnabled = cfg->midiEnabled;
	const bool notesMode = !cfg->midiTriggerPatterns;
	const bool noteSettingsEnabled = midiEnabled && notesMode;
	const uint8_t triggerColor = midiEnabled ? PAL_FORGRND : PAL_DSKTOP2;
	const uint8_t noteSettingsColor = noteSettingsEnabled ? PAL_FORGRND : PAL_DSKTOP2;

	drawFramework(video, 110, 0, 240, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 350, 0, 282, 173, FRAMEWORK_TYPE1);
	textOutShadow(video, bmp, 129, 4, PAL_FORGRND, PAL_DSKTOP2, "Enable MIDI input");
	widgets->checkBoxChecked[CB_CONF_MIDI_ENABLE] = cfg->midiEnabled;
	widgets->checkBoxDisabled[CB_CONF_MIDI_ENABLE] = false;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_ENABLE);

	/* Trigger mode: Notes (play instrument) or Patterns (trigger by note) */
	textOutShadow(video, bmp, 114, 20, triggerColor, PAL_DSKTOP2, "Notes trigger:");
	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_TRIGGER);
	widgets->radioButtonState[cfg->midiTriggerPatterns ? RB_CONFIG_MIDI_PATTERNS : RB_CONFIG_MIDI_NOTES] = RADIOBUTTON_CHECKED;
	widgets->radioButtonDisabled[RB_CONFIG_MIDI_NOTES] = !midiEnabled;
	widgets->radioButtonDisabled[RB_CONFIG_MIDI_PATTERNS] = !midiEnabled;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_MIDI_TRIGGER);
	textOutShadow(video, bmp, 231, 20, triggerColor, PAL_DSKTOP2, "Notes");
	textOutShadow(video, bmp, 294, 20, triggerColor, PAL_DSKTOP2, "Patterns");

	/* Channel filter: all or specific (1-16) */
	textOutShadow(video, bmp, 114, 36, noteSettingsColor, PAL_DSKTOP2, "Record MIDI chn.");
	charOutShadow(video, bmp, 225, 36, noteSettingsColor, PAL_DSKTOP2, '(');
	textOutShadow(video, bmp, 248, 36, noteSettingsColor, PAL_DSKTOP2, "all )");
	widgets->checkBoxChecked[CB_CONF_MIDI_ALLCHN] = cfg->midiAllChannels;
	widgets->checkBoxDisabled[CB_CONF_MIDI_ALLCHN] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_ALLCHN);
	/* Channel value and up/down buttons */
	char chnStr[8];
	snprintf(chnStr, sizeof(chnStr), "%02d", cfg->midiChannel);
	textOutShadow(video, bmp, 288, 36, noteSettingsColor, PAL_DSKTOP2, chnStr);
	widgets->pushButtonDisabled[PB_CONFIG_MIDICHN_UP] = !noteSettingsEnabled;
	widgets->pushButtonDisabled[PB_CONFIG_MIDICHN_DOWN] = !noteSettingsEnabled;
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDICHN_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDICHN_DOWN);

	/* Transpose: shift incoming notes by semitones */
	textOutShadow(video, bmp, 129, 52, noteSettingsColor, PAL_DSKTOP2, "Record transpose");
	widgets->checkBoxChecked[CB_CONF_MIDI_TRANSP] = cfg->midiRecordTranspose;
	widgets->checkBoxDisabled[CB_CONF_MIDI_TRANSP] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_TRANSP);
	/* Transpose value and up/down buttons - always show but grey out if disabled */
	const bool transposeEnabled = noteSettingsEnabled && cfg->midiRecordTranspose;
	const uint8_t transposeColor = transposeEnabled ? PAL_FORGRND : PAL_DSKTOP2;
	char transStr[8];
	if (cfg->midiTranspose >= 0)
		snprintf(transStr, sizeof(transStr), "+%d", cfg->midiTranspose);
	else
		snprintf(transStr, sizeof(transStr), "%d", cfg->midiTranspose);
	textOutShadow(video, bmp, 288, 52, transposeColor, PAL_DSKTOP2, transStr);
	widgets->pushButtonDisabled[PB_CONFIG_MIDITRANS_UP] = !transposeEnabled;
	widgets->pushButtonDisabled[PB_CONFIG_MIDITRANS_DOWN] = !transposeEnabled;
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDITRANS_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDITRANS_DOWN);

	/* Velocity: record as volume column value */
	textOutShadow(video, bmp, 129, 68, noteSettingsColor, PAL_DSKTOP2, "Record velocity");
	widgets->checkBoxChecked[CB_CONF_MIDI_VELOCITY] = cfg->midiRecordVelocity;
	widgets->checkBoxDisabled[CB_CONF_MIDI_VELOCITY] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_VELOCITY);

	/* Aftertouch: record as volume slides */
	textOutShadow(video, bmp, 129, 84, noteSettingsColor, PAL_DSKTOP2, "Record aftertouch");
	widgets->checkBoxChecked[CB_CONF_MIDI_AFTERTOUCH] = cfg->midiRecordAftertouch;
	widgets->checkBoxDisabled[CB_CONF_MIDI_AFTERTOUCH] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_AFTERTOUCH);

	/* Sensitivity: scale velocity/aftertouch values */
	textOutShadow(video, bmp, 114, 100, noteSettingsColor, PAL_DSKTOP2, "Vel./A.T. sens.");
	setScrollBarPos(inst, widgets, video, SB_MIDI_SENS, cfg->midiVelocitySens, false);
	widgets->scrollBarDisabled[SB_MIDI_SENS] = !noteSettingsEnabled;
	showScrollBar(widgets, video, SB_MIDI_SENS);
	widgets->pushButtonDisabled[PB_CONFIG_MIDISENS_DOWN] = !noteSettingsEnabled;
	widgets->pushButtonDisabled[PB_CONFIG_MIDISENS_UP] = !noteSettingsEnabled;
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDISENS_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_MIDISENS_UP);
	char sensStr[8];
	snprintf(sensStr, sizeof(sensStr), "%3d", cfg->midiVelocitySens);
	textOutShadow(video, bmp, 310, 100, noteSettingsColor, PAL_DSKTOP2, sensStr);
	charOutShadow(video, bmp, 334, 100, noteSettingsColor, PAL_DSKTOP2, '%');

	/* Mod wheel: record as vibrato effect (4xy) */
	textOutShadow(video, bmp, 129, 116, noteSettingsColor, PAL_DSKTOP2, "Record mod wheel");
	widgets->checkBoxChecked[CB_CONF_MIDI_MODWHEEL] = cfg->midiRecordModWheel;
	widgets->checkBoxDisabled[CB_CONF_MIDI_MODWHEEL] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_MODWHEEL);
	const bool modEnabled = noteSettingsEnabled && cfg->midiRecordModWheel;
	const uint8_t modColor = modEnabled ? PAL_FORGRND : PAL_DSKTOP2;
	textOutShadow(video, bmp, 246, 116, modColor, PAL_DSKTOP2, "range:");
	char modStr[8];
	snprintf(modStr, sizeof(modStr), "%2d", cfg->midiModRange);
	textOutShadow(video, bmp, 288, 116, modColor, PAL_DSKTOP2, modStr);
	widgets->pushButtonDisabled[PB_CONFIG_MODRANGE_UP] = !modEnabled;
	widgets->pushButtonDisabled[PB_CONFIG_MODRANGE_DOWN] = !modEnabled;
	showPushButton(widgets, video, bmp, PB_CONFIG_MODRANGE_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_MODRANGE_DOWN);

	/* Pitch bend: record as portamento effect (1xx/2xx) */
	textOutShadow(video, bmp, 129, 132, noteSettingsColor, PAL_DSKTOP2, "Record pitch bend");
	widgets->checkBoxChecked[CB_CONF_MIDI_PITCHBEND] = cfg->midiRecordPitchBend;
	widgets->checkBoxDisabled[CB_CONF_MIDI_PITCHBEND] = !noteSettingsEnabled;
	showCheckBox(widgets, video, bmp, CB_CONF_MIDI_PITCHBEND);
	const bool bendEnabled = noteSettingsEnabled && cfg->midiRecordPitchBend;
	const uint8_t bendColor = bendEnabled ? PAL_FORGRND : PAL_DSKTOP2;
	textOutShadow(video, bmp, 246, 132, bendColor, PAL_DSKTOP2, "range:");
	char bendStr[8];
	snprintf(bendStr, sizeof(bendStr), "%2d", cfg->midiBendRange);
	textOutShadow(video, bmp, 288, 132, bendColor, PAL_DSKTOP2, bendStr);
	widgets->pushButtonDisabled[PB_CONFIG_BENDRANGE_UP] = !bendEnabled;
	widgets->pushButtonDisabled[PB_CONFIG_BENDRANGE_DOWN] = !bendEnabled;
	showPushButton(widgets, video, bmp, PB_CONFIG_BENDRANGE_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_BENDRANGE_DOWN);

	/* Priority: which CC gets effect column (other goes to volume) */
	textOutShadow(video, bmp, 114, 148, noteSettingsColor, PAL_DSKTOP2, "Recording priority:");
	uncheckRadioButtonGroup(widgets, RB_GROUP_CONFIG_MIDI_PRIORITY);
	widgets->radioButtonState[(cfg->midiRecordPriority == 0) ? RB_CONFIG_MIDI_PITCH_PRIO : RB_CONFIG_MIDI_MOD_PRIO] = RADIOBUTTON_CHECKED;
	widgets->radioButtonDisabled[RB_CONFIG_MIDI_PITCH_PRIO] = !noteSettingsEnabled;
	widgets->radioButtonDisabled[RB_CONFIG_MIDI_MOD_PRIO] = !noteSettingsEnabled;
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_MIDI_PRIORITY);
	textOutShadow(video, bmp, 239, 148, noteSettingsColor, PAL_DSKTOP2, "Bend");
	textOutShadow(video, bmp, 287, 148, noteSettingsColor, PAL_DSKTOP2, "Mod whl.");

	const char *priorityExplain = (cfg->midiRecordPriority == 0)
		? "Bend to effects, mod to volume"
		: "Mod to effects, bend to volume";
	textOutShadow(video, bmp, 114, 160, noteSettingsColor, PAL_DSKTOP2, priorityExplain);
}

/* ---------- Main draw ---------- */

void drawConfigScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL) return;

	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets == NULL) return;

	clearRect(video, 0, 0, 632, 173);

	/* Left sidebar: tab selector and action buttons */
	drawFramework(video, 0, 0, 110, 173, FRAMEWORK_TYPE1);
	setConfigRadioButtonStates(widgets, &inst->config);
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_SELECT);
	showPushButton(widgets, video, bmp, PB_CONFIG_RESET);
	showPushButton(widgets, video, bmp, PB_CONFIG_LOAD);
	showPushButton(widgets, video, bmp, PB_CONFIG_SAVE);
	showPushButton(widgets, video, bmp, PB_CONFIG_EXIT);
	textOutShadow(video, bmp, 4, 4, PAL_FORGRND, PAL_DSKTOP2, "Configuration:");
	textOutShadow(video, bmp, 21, 19, PAL_FORGRND, PAL_DSKTOP2, "Audio");
	textOutShadow(video, bmp, 21, 35, PAL_FORGRND, PAL_DSKTOP2, "Layout");
	textOutShadow(video, bmp, 21, 51, PAL_FORGRND, PAL_DSKTOP2, "Miscellaneous");
	textOutShadow(video, bmp, 21, 67, PAL_FORGRND, PAL_DSKTOP2, "MIDI input");
	textOutShadow(video, bmp, 21, 83, PAL_FORGRND, PAL_DSKTOP2, "I/O Routing");

	switch (inst->config.currConfigScreen) {
		case CONFIG_SCREEN_AUDIO:         showConfigAudio(inst, video, bmp); break;
		case CONFIG_SCREEN_LAYOUT:        showConfigLayout(inst, video, bmp); break;
		case CONFIG_SCREEN_MISCELLANEOUS: showConfigMiscellaneous(inst, video, bmp); break;
		case CONFIG_SCREEN_IO_ROUTING:    showConfigIORouting(inst, video, bmp); break;
		case CONFIG_SCREEN_MIDI_INPUT:    showConfigMidiInput(inst, video, bmp); break;
		default:                          showConfigAudio(inst, video, bmp); break;
	}
}

/* ---------- Tab switching callbacks ---------- */

void rbConfigAudio(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_AUDIO;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigLayout(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_LAYOUT;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMiscellaneous(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_MISCELLANEOUS;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigIORouting(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_IO_ROUTING;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMidiInput(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideConfigScreen(inst);
	inst->config.currConfigScreen = CONFIG_SCREEN_MIDI_INPUT;
	showConfigScreen(inst);
	inst->uiState.needsFullRedraw = true;
}

/* ---------- File sorting ---------- */

void rbFileSortExt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.dirSortPriority = 0;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FILESORT_EXT);
	inst->diskop.requestReadDir = true;
}

void rbFileSortName(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.dirSortPriority = 1;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FILESORT_NAME);
	inst->diskop.requestReadDir = true;
}

/* ---------- Frequency slides ---------- */

void rbConfigFreqSlidesAmiga(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->audio.linearPeriodsFlag = false;
}

void rbConfigFreqSlidesLinear(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->audio.linearPeriodsFlag = true;
}

/* ---------- MIDI trigger mode ---------- */

void rbConfigMidiTriggerNotes(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.midiTriggerPatterns = false;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_NOTES);
	inst->uiState.needsFullRedraw = true;
}

/* Dialog callback for pattern-trigger sync warning */
static void onMidiPatternSyncWarningResult(ft2_instance_t *inst,
	ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;

	if (result == DIALOG_RESULT_YES) {
		inst->config.syncTransportFromDAW = false;
		inst->config.syncPositionFromDAW = false;
	}

	inst->config.midiTriggerPatterns = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_PATTERNS);
	inst->uiState.needsFullRedraw = true;
}

/* Pattern trigger mode conflicts with DAW sync; warn user if sync is on */
void rbConfigMidiTriggerPatterns(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	if (inst->config.syncTransportFromDAW || inst->config.syncPositionFromDAW) {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL) {
			ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
				"For consistent playback, turn off \"Sync transport\" and \"Sync position\" in audio settings?",
				inst, onMidiPatternSyncWarningResult, NULL);
			return;
		}
	}

	inst->config.midiTriggerPatterns = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_PATTERNS);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMidiPitchPrio(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.midiRecordPriority = 0;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_PITCH_PRIO);
	inst->uiState.needsFullRedraw = true;
}

void rbConfigMidiModPrio(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.midiRecordPriority = 1;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_MIDI_MOD_PRIO);
	inst->uiState.needsFullRedraw = true;
}

/* ---------- Interpolation ---------- */

/* Stop voices before changing interpolation to avoid audio glitches */
static void setInterpolationType(ft2_instance_t *inst, uint8_t type)
{
	ft2_stop_all_voices(inst);
	inst->config.interpolation = type;
	inst->audio.interpolationType = type;
}

void rbConfigIntrpNone(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_DISABLED);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_NONE);
}

void rbConfigIntrpLinear(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_LINEAR);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_LINEAR);
}

void rbConfigIntrpQuadratic(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_QUADRATIC);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_QUADRATIC);
}

void rbConfigIntrpCubic(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_CUBIC);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_CUBIC);
}

void rbConfigIntrpSinc8(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_SINC8);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_SINC8);
}

void rbConfigIntrpSinc16(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	setInterpolationType(inst, INTERPOLATION_SINC16);
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_AUDIO_INTRP_SINC16);
}

/* ---------- Scope style ---------- */

void rbConfigScopeFT2(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.linedScopes = false;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_SCOPE_STANDARD);
}

void rbConfigScopeLined(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.linedScopes = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_SCOPE_LINED);
}

/* ---------- Channel count ---------- */

void rbConfigPatt4Chans(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_4;
	inst->uiState.maxVisibleChannels = 4;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_4CHANS);
}

void rbConfigPatt6Chans(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_6;
	inst->uiState.maxVisibleChannels = 6;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_6CHANS);
}

void rbConfigPatt8Chans(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_8;
	inst->uiState.maxVisibleChannels = 8;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_8CHANS);
}

void rbConfigPatt12Chans(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnMaxChannels = MAX_CHANS_SHOWN_12;
	inst->uiState.maxVisibleChannels = 12;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_PATT_12CHANS);
}

/* ---------- Pattern font ---------- */

void rbConfigFontCapitals(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.ptnFont = PATT_FONT_CAPITALS;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_CAPITALS);
}

void rbConfigFontLowerCase(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.ptnFont = PATT_FONT_LOWERCASE;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_LOWERCASE);
}

void rbConfigFontFuture(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.ptnFont = PATT_FONT_FUTURE;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_FUTURE);
}

void rbConfigFontBold(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnFont = PATT_FONT_BOLD;
	inst->uiState.ptnFont = PATT_FONT_BOLD;
	inst->uiState.updatePatternEditor = true;
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL) checkRadioButtonNoRedraw(widgets, RB_CONFIG_FONT_BOLD);
}

/* ---------- Pattern editor checkboxes ---------- */

void cbConfigPattStretch(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnStretch = !inst->config.ptnStretch;
	inst->uiState.ptnStretch = inst->config.ptnStretch;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigHexCount(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnHex = !inst->config.ptnHex;
	inst->uiState.ptnHex = inst->config.ptnHex;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigAccidential(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnAcc = !inst->config.ptnAcc;
	inst->uiState.ptnAcc = inst->config.ptnAcc;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigShowZeroes(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnInstrZero = !inst->config.ptnInstrZero;
	inst->uiState.ptnInstrZero = inst->config.ptnInstrZero;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigFramework(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnFrmWrk = !inst->config.ptnFrmWrk;
	inst->uiState.ptnFrmWrk = inst->config.ptnFrmWrk;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigLineColors(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnLineLight = !inst->config.ptnLineLight;
	inst->uiState.ptnLineLight = inst->config.ptnLineLight;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigChanNums(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnChnNumbers = !inst->config.ptnChnNumbers;
	inst->uiState.ptnChnNumbers = inst->config.ptnChnNumbers;
	inst->uiState.updatePatternEditor = true;
}

void cbConfigShowVolCol(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.ptnShowVolColumn = !inst->config.ptnShowVolColumn;
	inst->uiState.ptnShowVolColumn = inst->config.ptnShowVolColumn;
	updateChanNums(inst);
	inst->uiState.updatePatternEditor = true;
}

/* ---------- Volume ramping ---------- */

void cbConfigVolRamp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.volumeRamp = !inst->config.volumeRamp;
	inst->audio.volumeRampingFlag = inst->config.volumeRamp;
}

/* ---------- Miscellaneous checkboxes ---------- */

void cbSampCutToBuff(ft2_instance_t *inst) { if (inst) inst->config.smpCutToBuffer = !inst->config.smpCutToBuffer; }
void cbPattCutToBuff(ft2_instance_t *inst) { if (inst) inst->config.ptnCutToBuffer = !inst->config.ptnCutToBuffer; }
void cbKillNotesAtStop(ft2_instance_t *inst) { if (inst) inst->config.killNotesOnStopPlay = !inst->config.killNotesOnStopPlay; }
void cbFileOverwriteWarn(ft2_instance_t *inst) { if (inst) inst->config.overwriteWarning = !inst->config.overwriteWarning; }
void cbMultiChanRec(ft2_instance_t *inst) { if (inst) inst->config.multiRec = !inst->config.multiRec; }
void cbMultiChanKeyJazz(ft2_instance_t *inst) { if (inst) inst->config.multiKeyJazz = !inst->config.multiKeyJazz; }
void cbMultiChanEdit(ft2_instance_t *inst) { if (inst) inst->config.multiEdit = !inst->config.multiEdit; }
void cbRecKeyOff(ft2_instance_t *inst) { if (inst) inst->config.recRelease = !inst->config.recRelease; }
void cbQuantize(ft2_instance_t *inst) { if (inst) inst->config.recQuant = !inst->config.recQuant; }
void cbChangePattLen(ft2_instance_t *inst) { if (inst) inst->config.recTrueInsert = !inst->config.recTrueInsert; }
void cbAutoUpdateCheck(ft2_instance_t *inst) { if (inst) inst->config.autoUpdateCheck = !inst->config.autoUpdateCheck; }

/* Quantization: 1/1, 1/2, 1/4, 1/8, 1/16 */
void configQuantizeUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.recQuantRes <= 8) {
		inst->config.recQuantRes *= 2;
		inst->uiState.needsFullRedraw = true;
	}
}

void configQuantizeDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.recQuantRes > 1) {
		inst->config.recQuantRes /= 2;
		inst->uiState.needsFullRedraw = true;
	}
}

/* ---------- DAW sync checkboxes ---------- */

/*
 * BPM sync: when enabled, DAW controls tempo; when disabled, internal BPM restored.
 * Also auto-disables position sync (requires BPM sync).
 */
void cbSyncBpmFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.syncBpmFromDAW = !inst->config.syncBpmFromDAW;

	if (inst->config.syncBpmFromDAW) {
		inst->config.savedBpm = inst->replayer.song.BPM;
	} else {
		if (inst->config.savedBpm > 0) {
			inst->replayer.song.BPM = inst->config.savedBpm;
			ft2_set_bpm(inst, inst->config.savedBpm);
		}
		if (inst->config.syncPositionFromDAW)
			inst->config.syncPositionFromDAW = false;
	}

	ft2_timemap_invalidate(inst);
	inst->uiState.needsFullRedraw = true;
}

void cbSyncTransportFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.syncTransportFromDAW = !inst->config.syncTransportFromDAW;
}

/* Position sync requires BPM sync (for accurate timing calculation) */
void cbSyncPositionFromDAW(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	if (!inst->config.syncBpmFromDAW && !inst->config.syncPositionFromDAW) {
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL)
			ft2_dialog_show_message(&ui->dialog, "System message", "Position sync requires BPM sync to be enabled.");
		return;
	}
	inst->config.syncPositionFromDAW = !inst->config.syncPositionFromDAW;
}

/*
 * Fxx speed changes: when disabled, speed locked to 3 or 6 for consistent timing.
 * Used with DAW sync for predictable bar/beat alignment.
 */
void cbAllowFxxSpeedChanges(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->config.allowFxxSpeedChanges = !inst->config.allowFxxSpeedChanges;

	if (inst->config.allowFxxSpeedChanges) {
		if (inst->config.savedSpeed > 0)
			inst->replayer.song.speed = inst->config.savedSpeed;
	} else {
		inst->config.savedSpeed = inst->replayer.song.speed;
		inst->replayer.song.speed = inst->config.lockedSpeed;
	}

	ft2_timemap_invalidate(inst);
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
}

/* ---------- Amplification and master volume ---------- */

void configAmpDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollLeft(inst, &ui->widgets, &ui->video, SB_AMP_SCROLL, 1);
}

void configAmpUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollRight(inst, &ui->widgets, &ui->video, SB_AMP_SCROLL, 1);
}

void configMasterVolDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollLeft(inst, &ui->widgets, &ui->video, SB_MASTERVOL_SCROLL, 1);
}

void configMasterVolUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollRight(inst, &ui->widgets, &ui->video, SB_MASTERVOL_SCROLL, 1);
}

void sbAmpPos(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;

	uint8_t newLevel = (uint8_t)(pos + 1);
	if (newLevel < 1) newLevel = 1;
	if (newLevel > 32) newLevel = 32;

	if (inst->config.boostLevel != newLevel) {
		inst->config.boostLevel = newLevel;
		ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);

		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL && inst->uiState.configScreenShown && inst->config.currConfigScreen == CONFIG_SCREEN_AUDIO) {
			char ampStr[8];
			snprintf(ampStr, sizeof(ampStr), "%2dx", inst->config.boostLevel);
			fillRect(&ui->video, 374, 91, 24, 9, PAL_DESKTOP);
			textOutShadow(&ui->video, &ui->bmp, 374, 91, PAL_FORGRND, PAL_DSKTOP2, ampStr);
		}
	}
}

void sbMasterVolPos(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;

	uint16_t newVol = (uint16_t)pos;
	if (newVol > 256) newVol = 256;

	if (inst->config.masterVol != newVol) {
		inst->config.masterVol = newVol;
		ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);

		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui != NULL && inst->uiState.configScreenShown && inst->config.currConfigScreen == CONFIG_SCREEN_AUDIO) {
			char volStr[8];
			snprintf(volStr, sizeof(volStr), "%3d", inst->config.masterVol);
			fillRect(&ui->video, 374, 119, 24, 9, PAL_DESKTOP);
			textOutShadow(&ui->video, &ui->bmp, 374, 119, PAL_FORGRND, PAL_DSKTOP2, volStr);
		}
	}
}

/* ---------- Config action buttons ---------- */

static void onResetConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (result == DIALOG_RESULT_YES && inst != NULL)
		inst->uiState.requestResetConfig = true;
}

void pbConfigReset(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
		ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Reset all settings to factory defaults?", inst, onResetConfigResult, NULL);
}

static void onLoadGlobalConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (result == DIALOG_RESULT_YES && inst != NULL)
		inst->uiState.requestLoadGlobalConfig = true;
}

void pbConfigLoad(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
		ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Load your global config?", inst, onLoadGlobalConfigResult, NULL);
}

static void onSaveGlobalConfigResult(ft2_instance_t *inst, ft2_dialog_result_t result, const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (result == DIALOG_RESULT_YES && inst != NULL)
		inst->uiState.requestSaveGlobalConfig = true;
}

void pbConfigSave(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
		ft2_dialog_show_yesno_cb(&ui->dialog, "System request", "Overwrite global config?", inst, onSaveGlobalConfigResult, NULL);
}

/* ---------- Channel output routing ---------- */

/* Cycle output bus assignment (wraps 0-14) */
static void routingUp(ft2_instance_t *inst, int channel)
{
	if (inst == NULL || channel < 0 || channel >= 32) return;
	inst->config.channelRouting[channel] = (inst->config.channelRouting[channel] + 1) % FT2_NUM_OUTPUTS;
	inst->uiState.needsFullRedraw = true;
}

static void routingDown(ft2_instance_t *inst, int channel)
{
	if (inst == NULL || channel < 0 || channel >= 32) return;
	inst->config.channelRouting[channel] = (inst->config.channelRouting[channel] + FT2_NUM_OUTPUTS - 1) % FT2_NUM_OUTPUTS;
	inst->uiState.needsFullRedraw = true;
}

/* Macro generates Up/Down callbacks for each channel */
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

/* Sync "to main" checkbox states to config (called on any checkbox toggle) */
void cbRoutingToMain(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;

	for (int i = 0; i < 32; i++) {
		uint16_t cbId = CB_CONF_ROUTING_CH1_TOMAIN + i;
		if (ui->widgets.checkBoxVisible[cbId])
			inst->config.channelToMain[i] = ui->widgets.checkBoxChecked[cbId];
	}
}

/* ---------- MIDI input value display helpers ---------- */

static void drawMidiChannelValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return;
	char str[8];
	snprintf(str, sizeof(str), "%02d", inst->config.midiChannel);
	fillRect(&ui->video, 288, 36, 20, 9, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 288, 36, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawMidiTransposeValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return;
	char str[8];
	snprintf(str, sizeof(str), inst->config.midiTranspose >= 0 ? "+%d" : "%d", inst->config.midiTranspose);
	fillRect(&ui->video, 288, 52, 20, 9, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 288, 52, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawMidiSensValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return;
	char str[8];
	snprintf(str, sizeof(str), "%3d", inst->config.midiVelocitySens);
	fillRect(&ui->video, 310, 100, 24, 10, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 310, 100, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawBendRangeValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return;
	char str[8];
	snprintf(str, sizeof(str), "%2d", inst->config.midiBendRange);
	fillRect(&ui->video, 288, 132, 16, 9, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 288, 132, PAL_FORGRND, PAL_DSKTOP2, str);
}

static void drawModRangeValue(ft2_instance_t *inst)
{
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL || !inst->uiState.configScreenShown || inst->config.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return;
	char str[8];
	snprintf(str, sizeof(str), "%2d", inst->config.midiModRange);
	fillRect(&ui->video, 288, 116, 16, 9, PAL_DESKTOP);
	textOutShadow(&ui->video, &ui->bmp, 288, 116, PAL_FORGRND, PAL_DSKTOP2, str);
}

/* ---------- MIDI input callbacks ---------- */

void configMidiChnDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiChannel > 1) { inst->config.midiChannel--; drawMidiChannelValue(inst); }
}

void configMidiChnUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiChannel < 16) { inst->config.midiChannel++; drawMidiChannelValue(inst); }
}

void sbMidiChannel(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	uint8_t newCh = (uint8_t)(pos + 1);
	if (newCh < 1) newCh = 1;
	if (newCh > 16) newCh = 16;
	if (inst->config.midiChannel != newCh) { inst->config.midiChannel = newCh; drawMidiChannelValue(inst); }
}

void configMidiTransDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiTranspose > -72) { inst->config.midiTranspose--; drawMidiTransposeValue(inst); }
}

void configMidiTransUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiTranspose < 72) { inst->config.midiTranspose++; drawMidiTransposeValue(inst); }
}

void sbMidiTranspose(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	int8_t newTrans = (int8_t)((int32_t)pos - 48);
	if (newTrans < -48) newTrans = -48;
	if (newTrans > 48) newTrans = 48;
	if (inst->config.midiTranspose != newTrans) { inst->config.midiTranspose = newTrans; drawMidiTransposeValue(inst); }
}

void configMidiSensDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollLeft(inst, &ui->widgets, &ui->video, SB_MIDI_SENS, 1);
}

void configMidiSensUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL) scrollBarScrollRight(inst, &ui->widgets, &ui->video, SB_MIDI_SENS, 1);
}

void sbMidiSens(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	uint16_t newSens = (uint16_t)pos;
	if (newSens > 200) newSens = 200;
	if (inst->config.midiVelocitySens != newSens) { inst->config.midiVelocitySens = newSens; drawMidiSensValue(inst); }
}

void configBendRangeUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiBendRange < 12) { inst->config.midiBendRange++; drawBendRangeValue(inst); }
}

void configBendRangeDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiBendRange > 1) { inst->config.midiBendRange--; drawBendRangeValue(inst); }
}

void configModRangeUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiModRange < 15) { inst->config.midiModRange++; drawModRangeValue(inst); }
}

void configModRangeDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->config.midiModRange > 1) { inst->config.midiModRange--; drawModRangeValue(inst); }
}
