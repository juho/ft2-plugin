/**
 * @file ft2_plugin_config.h
 * @brief Configuration structure and screen callbacks.
 *
 * Tabs: Audio, Layout, Miscellaneous, I/O Routing, MIDI Input.
 * Plugin adds DAW sync and I/O routing vs standalone.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

#define FT2_NUM_OUTPUTS 15  /* Output buses for multi-out (15 + Main) */

enum { /* Config screen tabs */
	CONFIG_SCREEN_AUDIO = 0,
	CONFIG_SCREEN_LAYOUT,
	CONFIG_SCREEN_MISCELLANEOUS,
	CONFIG_SCREEN_IO_ROUTING,
	CONFIG_SCREEN_MIDI_INPUT,
	CONFIG_SCREEN_COUNT
};

enum { /* Palette presets */
	PAL_ARCTIC = 0, PAL_AURORA_BOREALIS, PAL_BLUES, PAL_GOLD, PAL_HEAVY_METAL,
	PAL_JUNGLE, PAL_LITHE_DARK, PAL_ROSE, PAL_DARK_MODE, PAL_VIOLENT,
	PAL_WHY_COLORS, PAL_USER_DEFINED, PAL_PRESET_COUNT
};

enum { /* Pattern font styles (from original FT2) */
	PATT_FONT_CAPITALS = 0, PATT_FONT_LOWERCASE, PATT_FONT_FUTURE, PATT_FONT_BOLD, PATT_FONT_COUNT
};

enum { /* Max visible channels in pattern editor */
	MAX_CHANS_SHOWN_4 = 0, MAX_CHANS_SHOWN_6, MAX_CHANS_SHOWN_8, MAX_CHANS_SHOWN_12
};

enum { /* Sample interpolation quality */
	INTERPOLATION_DISABLED = 0, INTERPOLATION_LINEAR, INTERPOLATION_QUADRATIC,
	INTERPOLATION_CUBIC, INTERPOLATION_SINC8, INTERPOLATION_SINC16, INTERPOLATION_COUNT
};

/*
 * All persistent configuration. Saved/loaded with plugin state.
 * Matches standalone config_t where applicable.
 */
typedef struct ft2_plugin_config_t
{
	/* Pattern editor */
	bool ptnStretch;        /* Stretch pattern rows vertically */
	bool ptnHex;            /* Hex row numbers */
	bool ptnInstrZero;      /* Show "00" for empty instrument */
	bool ptnFrmWrk;         /* Draw framework lines */
	bool ptnLineLight;      /* Highlight every Nth row */
	bool ptnShowVolColumn;  /* Show volume column */
	bool ptnChnNumbers;     /* Show channel numbers */
	bool ptnAcc;            /* Flats instead of sharps */
	uint8_t ptnFont;        /* PATT_FONT_* */
	uint8_t ptnMaxChannels; /* MAX_CHANS_SHOWN_* */
	uint8_t ptnLineLightStep;

	/* Recording/Editing */
	bool multiRec;          /* Multi-channel record */
	bool multiKeyJazz;      /* Spread polyphonic notes across channels */
	bool multiEdit;         /* Multi-channel edit */
	bool recRelease;        /* Record key-off notes */
	bool recQuant;          /* Quantize recorded notes */
	uint8_t recQuantRes;    /* Quantization: 1,2,4,8,16 */
	bool recTrueInsert;     /* Insert/delete changes pattern length */

	/* Audio/Mixer */
	uint8_t interpolation;  /* INTERPOLATION_* */
	uint8_t boostLevel;     /* 1-32x amplification */
	uint16_t masterVol;     /* 0-256 */
	bool volumeRamp;        /* Prevent clicks on note start/stop */

	/* Visual */
	bool linedScopes;       /* Lined (vector) vs filled (FT2) scopes */

	/* Sample editor */
	uint8_t smpEdNote;      /* Preview note (48 = C-4) */

	/* Miscellaneous */
	bool smpCutToBuffer;
	bool ptnCutToBuffer;
	bool killNotesOnStopPlay;

	/* Disk operations */
	uint8_t dirSortPriority; /* 0=extension, 1=name */
	bool overwriteWarning;

	/* DAW sync (plugin-specific) */
	bool syncBpmFromDAW;        /* DAW controls tempo */
	bool syncTransportFromDAW;  /* DAW controls play/stop */
	bool syncPositionFromDAW;   /* DAW controls position (requires BPM sync) */
	bool allowFxxSpeedChanges;  /* Allow Fxx < 0x20 to change speed */
	uint16_t savedSpeed;        /* Restore when Fxx allowed again */
	uint16_t savedBpm;          /* Restore when BPM sync disabled */
	uint8_t lockedSpeed;        /* Speed when Fxx locked (3 or 6) */

	/* MIDI input (plugin-specific) */
	bool midiEnabled;
	bool midiAllChannels;       /* Listen to all vs specific channel */
	uint8_t midiChannel;        /* 1-16 */
	bool midiRecordTranspose;
	int8_t midiTranspose;       /* -48 to +48 semitones */
	uint8_t midiVelocitySens;   /* 0-200% */
	bool midiRecordVelocity;    /* Record as volume column */
	bool midiRecordAftertouch;  /* Record as volume slides */
	bool midiRecordModWheel;    /* Record as vibrato (4xy) */
	bool midiRecordPitchBend;   /* Record as portamento (1xx/2xx) */
	uint8_t midiRecordPriority; /* 0=pitch bend, 1=mod wheel to effects */
	uint8_t midiModRange;       /* Vibrato depth 1-15 */
	uint8_t midiBendRange;      /* Semitones 1-12 */
	bool midiTriggerPatterns;   /* true=notes trigger patterns */

	bool autoUpdateCheck;
	uint8_t palettePreset;      /* PAL_* */
	uint8_t userPalette[16][3]; /* User-defined palette RGB (0-63 per channel) */
	uint8_t userPaletteContrast[2]; /* Contrast for Desktop/Buttons */

	/* I/O routing (plugin-specific) */
	uint8_t channelRouting[32]; /* Output bus 0-14 */
	bool channelToMain[32];     /* Also send to main mix */

	/* About screen */
	bool id_FastLogo;
	bool id_TritonProd;

	uint8_t currConfigScreen;   /* CONFIG_SCREEN_* */

	/* Envelope presets 1-6 (matches standalone config_t) */
	int16_t stdEnvPoints[6][2][12][2]; /* [preset][vol/pan][point][x/y] */
	uint16_t stdVolEnvLength[6];
	uint16_t stdVolEnvSustain[6];
	uint16_t stdVolEnvLoopStart[6];
	uint16_t stdVolEnvLoopEnd[6];
	uint16_t stdVolEnvFlags[6];
	uint16_t stdPanEnvLength[6];
	uint16_t stdPanEnvSustain[6];
	uint16_t stdPanEnvLoopStart[6];
	uint16_t stdPanEnvLoopEnd[6];
	uint16_t stdPanEnvFlags[6];
	uint16_t stdFadeout[6];
	uint16_t stdVibRate[6];
	uint16_t stdVibDepth[6];
	uint16_t stdVibSweep[6];
	uint16_t stdVibType[6];
} ft2_plugin_config_t;

void ft2_config_init(ft2_plugin_config_t *config);
void ft2_config_apply(struct ft2_instance_t *inst, ft2_plugin_config_t *config);

/* Screen management */
void showConfigScreen(struct ft2_instance_t *inst);
void hideConfigScreen(struct ft2_instance_t *inst);
void exitConfigScreen(struct ft2_instance_t *inst);
void drawConfigScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Tab switching */
void rbConfigAudio(struct ft2_instance_t *inst);
void rbConfigLayout(struct ft2_instance_t *inst);
void rbConfigMiscellaneous(struct ft2_instance_t *inst);
void rbConfigIORouting(struct ft2_instance_t *inst);
void rbConfigMidiInput(struct ft2_instance_t *inst);

/* Audio: interpolation */
void rbConfigIntrpNone(struct ft2_instance_t *inst);
void rbConfigIntrpLinear(struct ft2_instance_t *inst);
void rbConfigIntrpQuadratic(struct ft2_instance_t *inst);
void rbConfigIntrpCubic(struct ft2_instance_t *inst);
void rbConfigIntrpSinc8(struct ft2_instance_t *inst);
void rbConfigIntrpSinc16(struct ft2_instance_t *inst);
void cbConfigVolRamp(struct ft2_instance_t *inst);

/* Audio: frequency slides */
void rbConfigFreqSlidesAmiga(struct ft2_instance_t *inst);
void rbConfigFreqSlidesLinear(struct ft2_instance_t *inst);

/* Audio: amplification/volume */
void configAmpDown(struct ft2_instance_t *inst);
void configAmpUp(struct ft2_instance_t *inst);
void configMasterVolDown(struct ft2_instance_t *inst);
void configMasterVolUp(struct ft2_instance_t *inst);
void sbAmpPos(struct ft2_instance_t *inst, uint32_t pos);
void sbMasterVolPos(struct ft2_instance_t *inst, uint32_t pos);

/* Audio: DAW sync (plugin-specific) */
void cbSyncBpmFromDAW(struct ft2_instance_t *inst);
void cbSyncTransportFromDAW(struct ft2_instance_t *inst);
void cbSyncPositionFromDAW(struct ft2_instance_t *inst);
void cbAllowFxxSpeedChanges(struct ft2_instance_t *inst);

/* Layout: channel count */
void rbConfigPatt4Chans(struct ft2_instance_t *inst);
void rbConfigPatt6Chans(struct ft2_instance_t *inst);
void rbConfigPatt8Chans(struct ft2_instance_t *inst);
void rbConfigPatt12Chans(struct ft2_instance_t *inst);

/* Layout: pattern font */
void rbConfigFontCapitals(struct ft2_instance_t *inst);
void rbConfigFontLowerCase(struct ft2_instance_t *inst);
void rbConfigFontFuture(struct ft2_instance_t *inst);
void rbConfigFontBold(struct ft2_instance_t *inst);

/* Layout: scopes */
void rbConfigScopeFT2(struct ft2_instance_t *inst);
void rbConfigScopeLined(struct ft2_instance_t *inst);

/* Layout: pattern editor checkboxes */
void cbConfigPattStretch(struct ft2_instance_t *inst);
void cbConfigHexCount(struct ft2_instance_t *inst);
void cbConfigAccidential(struct ft2_instance_t *inst);
void cbConfigShowZeroes(struct ft2_instance_t *inst);
void cbConfigFramework(struct ft2_instance_t *inst);
void cbConfigLineColors(struct ft2_instance_t *inst);
void cbConfigChanNums(struct ft2_instance_t *inst);
void cbConfigShowVolCol(struct ft2_instance_t *inst);

/* Misc: checkboxes */
void cbSampCutToBuff(struct ft2_instance_t *inst);
void cbPattCutToBuff(struct ft2_instance_t *inst);
void cbKillNotesAtStop(struct ft2_instance_t *inst);
void cbFileOverwriteWarn(struct ft2_instance_t *inst);
void cbMultiChanRec(struct ft2_instance_t *inst);
void cbMultiChanKeyJazz(struct ft2_instance_t *inst);
void cbMultiChanEdit(struct ft2_instance_t *inst);
void cbRecKeyOff(struct ft2_instance_t *inst);
void cbQuantize(struct ft2_instance_t *inst);
void cbChangePattLen(struct ft2_instance_t *inst);
void cbAutoUpdateCheck(struct ft2_instance_t *inst);
void configQuantizeUp(struct ft2_instance_t *inst);
void configQuantizeDown(struct ft2_instance_t *inst);

/* Misc: file sorting */
void rbFileSortExt(struct ft2_instance_t *inst);
void rbFileSortName(struct ft2_instance_t *inst);

/* I/O Routing (plugin-specific) */
void cbRoutingToMain(struct ft2_instance_t *inst);

/* MIDI Input (plugin-specific) */
void rbConfigMidiTriggerNotes(struct ft2_instance_t *inst);
void rbConfigMidiTriggerPatterns(struct ft2_instance_t *inst);
void rbConfigMidiPitchPrio(struct ft2_instance_t *inst);
void rbConfigMidiModPrio(struct ft2_instance_t *inst);
void configMidiChnDown(struct ft2_instance_t *inst);
void configMidiChnUp(struct ft2_instance_t *inst);
void sbMidiChannel(struct ft2_instance_t *inst, uint32_t pos);
void configMidiTransDown(struct ft2_instance_t *inst);
void configMidiTransUp(struct ft2_instance_t *inst);
void sbMidiTranspose(struct ft2_instance_t *inst, uint32_t pos);
void configMidiSensDown(struct ft2_instance_t *inst);
void configMidiSensUp(struct ft2_instance_t *inst);
void sbMidiSens(struct ft2_instance_t *inst, uint32_t pos);
void configBendRangeUp(struct ft2_instance_t *inst);
void configBendRangeDown(struct ft2_instance_t *inst);
void configModRangeUp(struct ft2_instance_t *inst);
void configModRangeDown(struct ft2_instance_t *inst);

/* Action buttons */
void pbConfigReset(struct ft2_instance_t *inst);
void pbConfigLoad(struct ft2_instance_t *inst);
void pbConfigSave(struct ft2_instance_t *inst);

/* Channel routing callbacks (32 channels x 2 directions) */
void pbRoutingCh1Up(struct ft2_instance_t *inst);
void pbRoutingCh1Down(struct ft2_instance_t *inst);
void pbRoutingCh2Up(struct ft2_instance_t *inst);
void pbRoutingCh2Down(struct ft2_instance_t *inst);
void pbRoutingCh3Up(struct ft2_instance_t *inst);
void pbRoutingCh3Down(struct ft2_instance_t *inst);
void pbRoutingCh4Up(struct ft2_instance_t *inst);
void pbRoutingCh4Down(struct ft2_instance_t *inst);
void pbRoutingCh5Up(struct ft2_instance_t *inst);
void pbRoutingCh5Down(struct ft2_instance_t *inst);
void pbRoutingCh6Up(struct ft2_instance_t *inst);
void pbRoutingCh6Down(struct ft2_instance_t *inst);
void pbRoutingCh7Up(struct ft2_instance_t *inst);
void pbRoutingCh7Down(struct ft2_instance_t *inst);
void pbRoutingCh8Up(struct ft2_instance_t *inst);
void pbRoutingCh8Down(struct ft2_instance_t *inst);
void pbRoutingCh9Up(struct ft2_instance_t *inst);
void pbRoutingCh9Down(struct ft2_instance_t *inst);
void pbRoutingCh10Up(struct ft2_instance_t *inst);
void pbRoutingCh10Down(struct ft2_instance_t *inst);
void pbRoutingCh11Up(struct ft2_instance_t *inst);
void pbRoutingCh11Down(struct ft2_instance_t *inst);
void pbRoutingCh12Up(struct ft2_instance_t *inst);
void pbRoutingCh12Down(struct ft2_instance_t *inst);
void pbRoutingCh13Up(struct ft2_instance_t *inst);
void pbRoutingCh13Down(struct ft2_instance_t *inst);
void pbRoutingCh14Up(struct ft2_instance_t *inst);
void pbRoutingCh14Down(struct ft2_instance_t *inst);
void pbRoutingCh15Up(struct ft2_instance_t *inst);
void pbRoutingCh15Down(struct ft2_instance_t *inst);
void pbRoutingCh16Up(struct ft2_instance_t *inst);
void pbRoutingCh16Down(struct ft2_instance_t *inst);
void pbRoutingCh17Up(struct ft2_instance_t *inst);
void pbRoutingCh17Down(struct ft2_instance_t *inst);
void pbRoutingCh18Up(struct ft2_instance_t *inst);
void pbRoutingCh18Down(struct ft2_instance_t *inst);
void pbRoutingCh19Up(struct ft2_instance_t *inst);
void pbRoutingCh19Down(struct ft2_instance_t *inst);
void pbRoutingCh20Up(struct ft2_instance_t *inst);
void pbRoutingCh20Down(struct ft2_instance_t *inst);
void pbRoutingCh21Up(struct ft2_instance_t *inst);
void pbRoutingCh21Down(struct ft2_instance_t *inst);
void pbRoutingCh22Up(struct ft2_instance_t *inst);
void pbRoutingCh22Down(struct ft2_instance_t *inst);
void pbRoutingCh23Up(struct ft2_instance_t *inst);
void pbRoutingCh23Down(struct ft2_instance_t *inst);
void pbRoutingCh24Up(struct ft2_instance_t *inst);
void pbRoutingCh24Down(struct ft2_instance_t *inst);
void pbRoutingCh25Up(struct ft2_instance_t *inst);
void pbRoutingCh25Down(struct ft2_instance_t *inst);
void pbRoutingCh26Up(struct ft2_instance_t *inst);
void pbRoutingCh26Down(struct ft2_instance_t *inst);
void pbRoutingCh27Up(struct ft2_instance_t *inst);
void pbRoutingCh27Down(struct ft2_instance_t *inst);
void pbRoutingCh28Up(struct ft2_instance_t *inst);
void pbRoutingCh28Down(struct ft2_instance_t *inst);
void pbRoutingCh29Up(struct ft2_instance_t *inst);
void pbRoutingCh29Down(struct ft2_instance_t *inst);
void pbRoutingCh30Up(struct ft2_instance_t *inst);
void pbRoutingCh30Down(struct ft2_instance_t *inst);
void pbRoutingCh31Up(struct ft2_instance_t *inst);
void pbRoutingCh31Down(struct ft2_instance_t *inst);
void pbRoutingCh32Up(struct ft2_instance_t *inst);
void pbRoutingCh32Down(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif

