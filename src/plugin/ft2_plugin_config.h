/**
 * @file ft2_plugin_config.h
 * @brief Configuration screen for the FT2 plugin.
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

/* Config screen tabs (matches standalone order) */
enum
{
	CONFIG_SCREEN_AUDIO = 0,
	CONFIG_SCREEN_LAYOUT = 1,
	CONFIG_SCREEN_MISCELLANEOUS = 2,
	CONFIG_SCREEN_COUNT
};

/* Palette presets */
enum
{
	PAL_ARCTIC = 0,
	PAL_AURORA_BOREALIS = 1,
	PAL_BLUES = 2,
	PAL_GOLD = 3,
	PAL_HEAVY_METAL = 4,
	PAL_JUNGLE = 5,
	PAL_LITHE_DARK = 6,
	PAL_ROSE = 7,
	PAL_DARK_MODE = 8,
	PAL_VIOLENT = 9,
	PAL_WHY_COLORS = 10,
	PAL_USER_DEFINED = 11,
	PAL_PRESET_COUNT
};

/* Pattern font types */
enum
{
	PATT_FONT_CAPITALS = 0,
	PATT_FONT_LOWERCASE = 1,
	PATT_FONT_FUTURE = 2,
	PATT_FONT_BOLD = 3,
	PATT_FONT_COUNT
};

/* Max channels shown */
enum
{
	MAX_CHANS_SHOWN_4 = 0,
	MAX_CHANS_SHOWN_6 = 1,
	MAX_CHANS_SHOWN_8 = 2,
	MAX_CHANS_SHOWN_12 = 3
};

/* Interpolation types (matches standalone) */
enum
{
	INTERPOLATION_DISABLED = 0,
	INTERPOLATION_LINEAR = 1,
	INTERPOLATION_QUADRATIC = 2,
	INTERPOLATION_CUBIC = 3,
	INTERPOLATION_SINC8 = 4,
	INTERPOLATION_SINC16 = 5,
	INTERPOLATION_COUNT
};

/* Configuration structure for plugin */
typedef struct ft2_plugin_config_t
{
	/* Pattern editor settings */
	bool ptnStretch;
	bool ptnHex;
	bool ptnInstrZero;
	bool ptnFrmWrk;
	bool ptnLineLight;
	bool ptnShowVolColumn;
	bool ptnChnNumbers;
	bool ptnAcc;   /* Use flats instead of sharps */
	uint8_t ptnFont;
	uint8_t ptnMaxChannels;
	uint8_t ptnLineLightStep;

	/* Recording/Editing settings */
	bool multiRec;         /* Multi-channel record */
	bool multiKeyJazz;     /* Multi-channel keyjazz (polyphonic jamming) */
	bool multiEdit;        /* Multi-channel edit */
	bool recRelease;       /* Record key-off notes */
	bool recQuant;         /* Enable quantization */
	uint8_t recQuantRes;   /* Quantization resolution (1,2,4,8,16) */
	bool recTrueInsert;    /* Change pattern length on insert/delete */

	/* Audio/Mixer settings */
	uint8_t interpolation; /* 0=none, 1=linear, 2=quadratic, 3=cubic, 4=sinc8, 5=sinc16 */
	uint8_t boostLevel;    /* 1-32 amplification */
	uint16_t masterVol;    /* 0-256 master volume */
	bool volumeRamp;       /* Enable volume ramping */

	/* Visual settings */
	bool linedScopes;      /* Lined scopes vs filled */

	/* Sample editor settings */
	uint8_t smpEdNote;     /* Preview note for sample playback (C-4 = 48) */

	/* Miscellaneous settings */
	bool smpCutToBuffer;
	bool ptnCutToBuffer;
	bool killNotesOnStopPlay;

	/* DAW sync settings */
	bool syncBpmFromDAW;        /* Sync BPM from DAW host */
	bool syncTransportFromDAW;  /* Sync start/stop from DAW host */
	bool syncPositionFromDAW;   /* Sync position from DAW (seek follows DAW playhead) */
	bool allowFxxSpeedChanges;  /* Allow Fxx speed effects (param < 0x20). Default: false */
	uint16_t savedSpeed;        /* Saved speed when Fxx changes disabled, restored when enabled */

	/* Palette */
	uint8_t palettePreset;

	/* Logo/Badge settings */
	bool id_FastLogo;    /* Toggle between FT2 logo styles */
	bool id_TritonProd;  /* Toggle between badge styles */

	/* Current config screen */
	uint8_t currConfigScreen;

	/* Envelope presets (6 slots) - matches standalone config_t */
	int16_t stdEnvPoints[6][2][12][2];  /* [preset][vol/pan][point][x/y] */
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

/* Initialize config with defaults */
void ft2_config_init(ft2_plugin_config_t *config);

/* Apply config to instance */
void ft2_config_apply(struct ft2_instance_t *inst, ft2_plugin_config_t *config);

/* Show/Hide config screen */
void showConfigScreen(struct ft2_instance_t *inst);
void hideConfigScreen(struct ft2_instance_t *inst);
void exitConfigScreen(struct ft2_instance_t *inst);

/* Draw config screen */
void drawConfigScreen(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Config screen tab switching */
void rbConfigAudio(struct ft2_instance_t *inst);
void rbConfigLayout(struct ft2_instance_t *inst);
void rbConfigMiscellaneous(struct ft2_instance_t *inst);

/* Audio config callbacks */
void rbConfigIntrpNone(struct ft2_instance_t *inst);
void rbConfigIntrpLinear(struct ft2_instance_t *inst);
void rbConfigIntrpQuadratic(struct ft2_instance_t *inst);
void rbConfigIntrpCubic(struct ft2_instance_t *inst);
void rbConfigIntrpSinc8(struct ft2_instance_t *inst);
void rbConfigIntrpSinc16(struct ft2_instance_t *inst);
void cbConfigVolRamp(struct ft2_instance_t *inst);

/* Layout config callbacks */
void rbConfigPatt4Chans(struct ft2_instance_t *inst);
void rbConfigPatt6Chans(struct ft2_instance_t *inst);
void rbConfigPatt8Chans(struct ft2_instance_t *inst);
void rbConfigPatt12Chans(struct ft2_instance_t *inst);
void rbConfigFontCapitals(struct ft2_instance_t *inst);
void rbConfigFontLowerCase(struct ft2_instance_t *inst);
void rbConfigFontFuture(struct ft2_instance_t *inst);
void rbConfigFontBold(struct ft2_instance_t *inst);
void rbConfigScopeFT2(struct ft2_instance_t *inst);
void rbConfigScopeLined(struct ft2_instance_t *inst);

/* Pattern editor checkboxes */
void cbConfigPattStretch(struct ft2_instance_t *inst);
void cbConfigHexCount(struct ft2_instance_t *inst);
void cbConfigAccidential(struct ft2_instance_t *inst);
void cbConfigShowZeroes(struct ft2_instance_t *inst);
void cbConfigFramework(struct ft2_instance_t *inst);
void cbConfigLineColors(struct ft2_instance_t *inst);
void cbConfigChanNums(struct ft2_instance_t *inst);
void cbConfigShowVolCol(struct ft2_instance_t *inst);

/* Miscellaneous checkboxes */
void cbSampCutToBuff(struct ft2_instance_t *inst);
void cbPattCutToBuff(struct ft2_instance_t *inst);
void cbKillNotesAtStop(struct ft2_instance_t *inst);
void cbMultiChanRec(struct ft2_instance_t *inst);
void cbMultiChanKeyJazz(struct ft2_instance_t *inst);
void cbMultiChanEdit(struct ft2_instance_t *inst);
void cbRecKeyOff(struct ft2_instance_t *inst);
void cbQuantize(struct ft2_instance_t *inst);
void cbChangePattLen(struct ft2_instance_t *inst);

/* Amplification arrow button callbacks */
void configAmpDown(struct ft2_instance_t *inst);
void configAmpUp(struct ft2_instance_t *inst);
void configMasterVolDown(struct ft2_instance_t *inst);
void configMasterVolUp(struct ft2_instance_t *inst);

/* Scrollbar position callbacks */
void sbAmpPos(struct ft2_instance_t *inst, uint32_t pos);
void sbMasterVolPos(struct ft2_instance_t *inst, uint32_t pos);

/* DAW sync checkboxes */
void cbSyncBpmFromDAW(struct ft2_instance_t *inst);
void cbSyncTransportFromDAW(struct ft2_instance_t *inst);
void cbSyncPositionFromDAW(struct ft2_instance_t *inst);
void cbAllowFxxSpeedChanges(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif

