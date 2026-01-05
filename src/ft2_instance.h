#pragma once

/**
 * @file ft2_instance.h
 * @brief Core instance structure for plugin architecture.
 *
 * This header defines the ft2_instance_t structure which encapsulates all
 * per-instance state that was previously stored in global variables.
 * This enables multiple instances of the replayer to run simultaneously,
 * as required for audio plugin architectures (VST, AU, etc.).
 */

#include <stdint.h>
#include <stdbool.h>
#include "plugin/ft2_plugin_config.h"
#include "plugin/ft2_plugin_timemap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for UI struct (defined in ft2_plugin_ui.h) */
struct ft2_ui_t;

/* 
 * Constants matching ft2_replayer.h to avoid SDL2 dependency.
 */
#define FT2_MAX_CHANNELS 32
#define FT2_MAX_PATTERNS 256
#define FT2_MAX_INST 128
#define FT2_MAX_SMP_PER_INST 16
#define FT2_MAX_ORDERS 256
#define FT2_MAX_PATT_LEN 256
#define FT2_MIN_BPM 32
#define FT2_MAX_BPM 255
#define FT2_MAX_TAPS 16
#define FT2_MAX_LEFT_TAPS 8
#define FT2_MAX_RIGHT_TAPS 8
#define FT2_MAX_SAMPLE_LEN 0x3FFFFFFF
#define FT2_NOTE_OFF 97
#define FT2_DISKOP_ENTRY_NUM 15
#define FT2_PATH_MAX 1024

/* Disk Op. item types */
enum ft2_diskop_item
{
	FT2_DISKOP_ITEM_MODULE = 0,
	FT2_DISKOP_ITEM_INSTR = 1,
	FT2_DISKOP_ITEM_SAMPLE = 2,
	FT2_DISKOP_ITEM_PATTERN = 3,
	FT2_DISKOP_ITEM_TRACK = 4
};

/* Disk Op. save formats */
enum ft2_diskop_save_format
{
	FT2_MOD_SAVE_MOD = 0,
	FT2_MOD_SAVE_XM = 1,
	FT2_MOD_SAVE_WAV = 2,
	FT2_SMP_SAVE_RAW = 0,
	FT2_SMP_SAVE_IFF = 1,
	FT2_SMP_SAVE_WAV = 2
};

/* Playback modes */
enum ft2_playmode
{
	FT2_PLAYMODE_IDLE = 0,
	FT2_PLAYMODE_EDIT = 1,
	FT2_PLAYMODE_SONG = 2,
	FT2_PLAYMODE_PATT = 3,
	FT2_PLAYMODE_RECSONG = 4,
	FT2_PLAYMODE_RECPATT = 5
};

/* Channel status flags */
enum ft2_channel_status
{
	FT2_CS_UPDATE_VOL = 1,
	FT2_CF_UPDATE_PERIOD = 2,
	FT2_CS_TRIGGER_VOICE = 4,
	FT2_CS_UPDATE_PAN = 8,
	FT2_CS_USE_QUICK_VOLRAMP = 16
};

/* Loop types */
enum ft2_loop_type
{
	FT2_LOOP_OFF = 0,
	FT2_LOOP_FWD = 1,
	FT2_LOOP_BIDI = 2
};

/* Sample flags */
enum ft2_sample_flags
{
	FT2_SAMPLE_16BIT = 16,
	FT2_SAMPLE_STEREO = 32
};

/* Envelope flags */
enum ft2_envelope_flags
{
	FT2_ENV_ENABLED = 1,
	FT2_ENV_SUSTAIN = 2,
	FT2_ENV_LOOP = 4
};

/**
 * @brief Pattern note structure (packed).
 */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct ft2_note_t
{
	uint8_t note, instr, vol, efx, efxData;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
ft2_note_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/**
 * @brief Sample structure.
 */
typedef struct ft2_sample_t
{
	char name[22+1];
	bool isFixed;
	int8_t finetune, relativeNote;
	int8_t *dataPtr, *origDataPtr;
	uint8_t volume, flags, panning;
	int32_t length, loopStart, loopLength;

	int8_t leftEdgeTapSamples8[FT2_MAX_TAPS * 2];
	int16_t leftEdgeTapSamples16[FT2_MAX_TAPS * 2];
	int16_t fixedSmp[FT2_MAX_TAPS * 2];
	int32_t fixedPos;
} ft2_sample_t;

/**
 * @brief Instrument structure.
 */
typedef struct ft2_instr_t
{
	bool midiOn, mute;
	uint8_t midiChannel, note2SampleLUT[96];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t autoVibType, autoVibSweep, autoVibDepth, autoVibRate;
	uint16_t fadeout;
	int16_t volEnvPoints[12][2], panEnvPoints[12][2];
	int16_t midiProgram, midiBend;
	int16_t numSamples;
	ft2_sample_t smp[16];
} ft2_instr_t;

/**
 * @brief Channel state structure.
 */
typedef struct ft2_channel_t
{
	bool dontRenderThisChannel, keyOff, channelOff, mute, semitonePortaMode;
	volatile uint8_t status, tmpStatus;
	int8_t relativeNote, finetune;
	uint8_t smpNum, instrNum, efxData, efx, sampleOffset, tremorParam, tremorPos;
	uint8_t globVolSlideSpeed, panningSlideSpeed, vibTremCtrl, portamentoDirection;
	uint8_t vibratoPos, tremoloPos, vibratoSpeed, vibratoDepth, tremoloSpeed, tremoloDepth;
	uint8_t patternLoopStartRow, patternLoopCounter, volSlideSpeed;
	uint8_t fVolSlideUpSpeed, fVolSlideDownSpeed;
	uint8_t fPitchSlideUpSpeed, fPitchSlideDownSpeed;
	uint8_t efPitchSlideUpSpeed, efPitchSlideDownSpeed;
	uint8_t pitchSlideUpSpeed, pitchSlideDownSpeed;
	uint8_t noteRetrigSpeed, noteRetrigCounter, noteRetrigVol;
	uint8_t volColumnVol, noteNum, panEnvPos, autoVibPos, volEnvPos;
	uint8_t realVol, oldVol, outVol;
	uint8_t oldPan, outPan, finalPan;
	int16_t midiPitch;
	uint16_t outPeriod, realPeriod, finalPeriod;
	uint16_t copyOfInstrAndNote, portamentoTargetPeriod, portamentoSpeed;
	uint16_t volEnvTick, panEnvTick, autoVibAmp, autoVibSweep;
	uint16_t midiVibDepth;
	int32_t fadeoutVol, fadeoutSpeed;
	int32_t smpStartPos;

	float fFinalVol, fVolEnvDelta, fPanEnvDelta, fVolEnvValue, fPanEnvValue;

	ft2_sample_t *smpPtr;
	ft2_instr_t *instrPtr;

	/* MIDI output tracking */
	uint8_t lastMidiNote;    /* Last MIDI note sent on this channel */
	bool midiNoteActive;     /* Is a MIDI note currently active on this channel */
} ft2_channel_t;

/**
 * @brief Song state structure.
 */
typedef struct ft2_song_t
{
	bool pBreakFlag, posJumpFlag, isModified;
	char name[20+1], instrName[1 + FT2_MAX_INST][22+1];
	uint8_t curReplayerTick, curReplayerRow, curReplayerSongPos, curReplayerPattNum;
	uint8_t pattDelTime, pattDelTime2, pBreakPos;
	uint8_t orders[FT2_MAX_ORDERS];
	int16_t songPos, pattNum, row, currNumRows;
	uint16_t songLength, songLoopStart, BPM, speed, initialSpeed, globalVolume, tick;
	int32_t numChannels;

	uint32_t playbackSeconds;
	uint64_t playbackSecondsFrac;
} ft2_song_t;

/**
 * @brief Per-instance audio state.
 */
typedef struct ft2_audio_state_t
{
	volatile bool locked;
	bool volumeRampingFlag, linearPeriodsFlag, sincInterpolation;
	volatile uint8_t interpolationType;
	uint32_t quickVolRampSamples, freq;

	uint32_t tickSampleCounter, samplesPerTickInt;
	uint64_t tickSampleCounterFrac, samplesPerTickFrac;

	uint32_t samplesPerTickIntTab[(FT2_MAX_BPM - FT2_MIN_BPM) + 1];
	uint64_t samplesPerTickFracTab[(FT2_MAX_BPM - FT2_MIN_BPM) + 1];

	uint32_t tickTimeIntTab[(FT2_MAX_BPM - FT2_MIN_BPM) + 1];
	uint64_t tickTimeFracTab[(FT2_MAX_BPM - FT2_MIN_BPM) + 1];

	uint64_t tickTime64, tickTime64Frac;

	float *fMixBufferL, *fMixBufferR;
	float fQuickVolRampSamplesMul, fSamplesPerTickIntMul;

	/* Per-channel mix buffers for multi-output support */
	float *fChannelBufferL[FT2_MAX_CHANNELS];
	float *fChannelBufferR[FT2_MAX_CHANNELS];
	bool multiOutEnabled;
	uint32_t multiOutBufferSize;
} ft2_audio_state_t;

/**
 * @brief Per-instance voice state for the mixer.
 */
typedef struct ft2_voice_t
{
	const int8_t *base8, *revBase8;
	const int16_t *base16, *revBase16;
	bool active, samplingBackwards, isFadeOutVoice, hasLooped;
	uint8_t scopeVolume, mixFuncOffset, panning, loopType;
	int32_t position, sampleEnd, loopStart, loopLength;
	uint32_t volumeRampLength;
	uint64_t positionFrac, delta, scopeDelta;

	const int8_t *leftEdgeTaps8;
	const int16_t *leftEdgeTaps16;

	const float *fSincLUT;
	float fVolume, fCurrVolumeL, fCurrVolumeR, fVolumeLDelta, fVolumeRDelta;
	float fTargetVolumeL, fTargetVolumeR;
} ft2_voice_t;

/**
 * @brief Per-instance replayer state.
 */
typedef struct ft2_replayer_state_t
{
	int8_t playMode;
	bool songPlaying, audioPaused, musicPaused;
	volatile bool replayerBusy;

	const uint16_t *note2PeriodLUT;
	int16_t patternNumRows[FT2_MAX_PATTERNS];
	ft2_channel_t channel[FT2_MAX_CHANNELS];
	ft2_song_t song;
	ft2_instr_t *instr[128 + 4];
	ft2_note_t *pattern[FT2_MAX_PATTERNS];

	uint64_t logTab[4 * 12 * 16];
	uint64_t scopeLogTab[4 * 12 * 16];
	uint64_t scopeDrawLogTab[4 * 12 * 16];
	uint64_t amigaPeriodDiv, scopeAmigaPeriodDiv, scopeDrawAmigaPeriodDiv;
	double dLogTab[4 * 12 * 16];
	double dExp2MulTab[32];
	bool bxxOverflow;
	ft2_note_t nilPatternLine[FT2_MAX_CHANNELS];

	/* DAW sync pattern loop state (set by timemap lookup on seek) */
	uint8_t patternLoopCounter;   /* Remaining E6x loop iterations */
	uint16_t patternLoopStartRow; /* E60 loop start row */
	bool patternLoopStateSet;     /* True if loop state was set by seek */
} ft2_replayer_state_t;

/**
 * @brief Pattern marking structure.
 */
typedef struct ft2_patt_mark_t
{
	int16_t markX1, markX2, markY1, markY2;
} ft2_patt_mark_t;

/**
 * @brief Editor state structure.
 * 
 * Corresponds to editor_t from ft2_structs.h.
 */
typedef struct ft2_editor_t
{
	volatile bool busy, programRunning;
	volatile bool updateCurSmp, updateCurInstr, updateWindowTitle;
	volatile uint8_t loadMusicEvent;

	bool autoPlayOnDrop, editTextFlag;
	bool copyMaskEnable, samplingAudioFlag, editSampleFlag;
	bool instrBankSwapped, channelMuted[FT2_MAX_CHANNELS], NI_Play;

	uint8_t curPlayInstr, curPlaySmp, curSmpChannel, currPanEnvPoint, currVolEnvPoint;
	uint8_t copyMask[5], pasteMask[5], transpMask[5], smpEd_NoteNr, instrBankOffset, sampleBankOffset;
	uint8_t srcInstr, curInstr, srcSmp, curSmp, currHelpScreen, currConfigScreen, textCursorBlinkCounter;
	uint8_t keyOnTab[FT2_MAX_CHANNELS], editRowSkip, curOctave;
	uint8_t sampleSaveMode, moduleSaveMode, ptnJumpPos[4];
	int16_t globalVolume, songPos, row;
	uint16_t tmpPattern, editPattern, BPM, speed, tick, ptnCursorY;
	int32_t keyOffNr, keyOffTime[FT2_MAX_CHANNELS];
	uint32_t framesPassed;
	
	/* Pattern marking */
	ft2_patt_mark_t pattMark;
} ft2_editor_t;

/**
 * @brief UI state structure.
 * 
 * Corresponds to ui_t from ft2_structs.h.
 */
typedef struct ft2_ui_state_t
{
	volatile bool setMouseBusy, setMouseIdle;
	bool sysReqEnterPressed;

	/* All screens */
	bool extendedPatternEditor, sysReqShown;

	/* Top screens */
	bool instrSwitcherShown, aboutScreenShown, helpScreenShown, configScreenShown;
	bool scopesShown, diskOpShown, nibblesShown, transposeShown, instEditorExtShown;
	bool sampleEditorExtShown, advEditShown, wavRendererShown, trimScreenShown;
	bool drawBPMFlag, drawSpeedFlag, drawGlobVolFlag, drawPosEdFlag, drawPattNumLenFlag;
	bool updatePosSections, updatePosEdScrollBar, updateInstrSwitcher, instrBankSwapPending;
	bool needsFullRedraw; /* Flag to trigger full UI redraw */
	uint8_t oldTopLeftScreen;

	/* Bottom screens */
	bool patternEditorShown, instEditorShown, sampleEditorShown, sampleEditorEffectsShown, pattChanScrollShown;
	bool leftLoopPinMoving, rightLoopPinMoving;
	bool drawReplayerPianoFlag, drawPianoFlag, updatePatternEditor, updateSampleEditor, updateInstEditor;
	bool updateChanScrollPos; /* Flag to update channel scrollbar position */
	uint8_t channelOffset, numChannelsShown, maxVisibleChannels;
	uint16_t patternChannelWidth;
	int32_t sampleDataOrLoopDrag;

	/* Pattern editor settings */
	bool ptnShowVolColumn;   /* Show volume column */
	bool ptnHex;             /* Hex row numbers */
	bool ptnLineLight;       /* Highlight every 4th row */
	bool ptnChnNumbers;      /* Show channel numbers */
	bool ptnInstrZero;       /* Show leading zeros for instruments */
	bool ptnAcc;             /* Use flats instead of sharps */
	bool ptnStretch;         /* Row height stretching */
	bool ptnFrmWrk;          /* Draw pattern editor framework */
	uint8_t ptnFont;         /* Pattern font style (0-3) */

	/* Nibbles deferred action flags */
	bool nibblesPlayRequested, nibblesHelpRequested, nibblesHighScoreRequested;
	bool nibblesExitRequested, nibblesRedrawRequested;
	bool nibblesHelpShown, nibblesHighScoresShown; /* Track overlay screens in game area */

	/* Config action request flags (set by C, handled by JUCE) */
	bool requestResetConfig;
	bool requestLoadGlobalConfig;
	bool requestSaveGlobalConfig;

	/* About screen request flags (set by C, handled by JUCE) */
	bool requestOpenGitHub;
	bool requestShowUpdateDialog;

	/* Backup flags for extended pattern editor */
	bool _aboutScreenShown, _helpScreenShown, _configScreenShown, _diskOpShown;
	bool _nibblesShown, _transposeShown, _instEditorShown;
	bool _instEditorExtShown, _sampleEditorExtShown, _sampleEditorEffectsShown, _patternEditorShown;
	bool _sampleEditorShown, _advEditShown, _wavRendererShown, _trimScreenShown;
} ft2_ui_state_t;

/**
 * @brief Nibbles high score entry.
 */
typedef struct ft2_nibbles_highscore_t
{
	uint8_t nameLen;
	char name[22];
	int32_t score;
	uint8_t level;
} ft2_nibbles_highscore_t;

/**
 * @brief Nibbles game coordinate.
 */
typedef struct ft2_nibbles_coord_t
{
	uint8_t x, y;
} ft2_nibbles_coord_t;

/**
 * @brief Nibbles input buffer.
 */
typedef struct ft2_nibbles_buffer_t
{
	int16_t length;
	uint8_t data[8];
} ft2_nibbles_buffer_t;

/**
 * @brief Nibbles game state.
 */
typedef struct ft2_nibbles_state_t
{
	bool playing;
	bool eternalLives;
	uint8_t level;
	uint8_t screen[51][23];

	/* Player 1 */
	int16_t p1Dir, p1Len, p1NoClear;
	int32_t p1Score;
	uint16_t p1Lives;
	ft2_nibbles_coord_t p1[256];

	/* Player 2 */
	int16_t p2Dir, p2Len, p2NoClear;
	int32_t p2Score;
	uint16_t p2Lives;
	ft2_nibbles_coord_t p2[256];

	/* Game state */
	int16_t number, numberX, numberY;
	uint8_t curSpeed, curTick;
	uint8_t curSpeed60Hz, curTick60Hz;
	ft2_nibbles_buffer_t inputBuffer[2];

	/* Settings */
	uint8_t numPlayers;  /* 0 = 1 player, 1 = 2 players */
	uint8_t speed;       /* 0-3: novice, average, pro, triton */
	bool surround;
	bool grid;
	bool wrap;

	/* High scores */
	ft2_nibbles_highscore_t highScores[10];

	/* Cheat code tracking */
	uint8_t cheatIndex;
	char cheatBuffer[16];

	/* Async high score entry state */
	bool pendingP1HighScore;
	bool pendingP2HighScore;
	int16_t pendingP1Slot;
	int16_t pendingP2Slot;
} ft2_nibbles_state_t;

/**
 * @brief Cursor state structure.
 */
typedef struct ft2_cursor_t
{
	uint8_t ch;
	int8_t object;
} ft2_cursor_t;

/**
 * @brief Disk Op. file list entry.
 */
typedef struct ft2_diskop_entry_t
{
	char name[256];
	bool isDir;
	int32_t filesize;
} ft2_diskop_entry_t;

/**
 * @brief Disk Op. state structure.
 */
typedef struct ft2_diskop_state_t
{
	/* Per-item-type paths (remembered between sessions) */
	char modulePath[FT2_PATH_MAX];
	char instrPath[FT2_PATH_MAX];
	char samplePath[FT2_PATH_MAX];
	char patternPath[FT2_PATH_MAX];
	char trackPath[FT2_PATH_MAX];

	char currentPath[FT2_PATH_MAX];  /* Active path */
	char filename[FT2_PATH_MAX];     /* Filename textbox */

	ft2_diskop_entry_t *entries;     /* File list (dynamically allocated) */
	int32_t fileCount;
	int32_t dirPos;                  /* Scroll position */
	int32_t selectedEntry;           /* -1 = none */

	uint8_t itemType;                /* 0=Module, 1=Instr, 2=Sample, 3=Pattern, 4=Track */
	uint8_t saveFormat[5];           /* Per-item save format */
	bool showAllFiles;
	bool firstOpen;                  /* True until first directory read */

	/* Request flags (set by C code, processed by JUCE) */
	volatile bool requestReadDir;    /* Request directory read */
	volatile bool requestGoParent;   /* Request navigate to parent */
	volatile bool requestGoRoot;     /* Request navigate to root */
	volatile bool requestGoHome;     /* Request navigate to home */
	volatile int32_t requestOpenEntry; /* Entry index to open, -1 = none */
	volatile int32_t requestLoadEntry; /* Entry index to load, -1 = none */
	volatile bool requestSave;       /* Request save */
	volatile bool requestSaveConfirmed; /* Overwrite confirmed, proceed without check */
	volatile bool requestDelete;     /* Request delete selected file */
	volatile bool requestRename;     /* Request rename selected file */
	volatile bool requestMakeDir;    /* Request create directory */
	volatile bool requestSetPath;    /* Request set path (validate and change) */
	char newDirName[256];            /* Directory name for make dir */
	char newPath[FT2_PATH_MAX];      /* Path for set path */

	/* Pending drop for unsaved changes confirmation */
	char pendingDropPath[FT2_PATH_MAX];  /* Path of dropped file waiting for confirmation */
	volatile bool requestDropLoad;       /* Set after user confirms drop load */

	/* Error flags (set by JUCE when operations fail) */
	volatile bool pathSetFailed;         /* Set path failed - path doesn't exist */
	volatile bool makeDirFailed;         /* Make dir failed - already exists or access denied */

#ifdef _WIN32
	/* Drive button state (Windows only) */
	#define FT2_DISKOP_MAX_DRIVES 7
	char driveNames[FT2_DISKOP_MAX_DRIVES][4];  /* "C:", "D:", etc. */
	uint8_t numDrives;
	volatile int8_t requestDriveIndex;          /* -1 = none, 0-6 = drive to navigate to */
	volatile bool requestEnumerateDrives;       /* Request JUCE to enumerate drives */
#endif
} ft2_diskop_state_t;

/**
 * @brief Main instance structure for the FT2 replayer.
 */
/* Scope sync queue for audio-to-UI communication */
#define FT2_SCOPE_SYNC_QUEUE_LEN 256
#define FT2_SCOPE_UPDATE_VOL 1
#define FT2_SCOPE_UPDATE_PERIOD 2
#define FT2_SCOPE_TRIGGER_VOICE 4

typedef struct ft2_scope_sync_entry_t
{
	uint8_t channel;
	uint8_t status;
	uint8_t scopeVolume;
	uint16_t period;
	const int8_t *base8;
	const int16_t *base16;
	int32_t length;
	int32_t loopStart;
	int32_t loopLength;
	int32_t smpStartPos;
	uint8_t loopType;
	bool sample16Bit;
} ft2_scope_sync_entry_t;

typedef struct ft2_scope_sync_queue_t
{
	ft2_scope_sync_entry_t entries[FT2_SCOPE_SYNC_QUEUE_LEN];
	volatile int32_t readPos;
	volatile int32_t writePos;
} ft2_scope_sync_queue_t;

/**
 * MIDI output event types
 */
typedef enum ft2_midi_event_type_t
{
	FT2_MIDI_NOTE_ON,
	FT2_MIDI_NOTE_OFF,
	FT2_MIDI_PROGRAM_CHANGE
} ft2_midi_event_type_t;

/**
 * MIDI output event structure
 */
typedef struct ft2_midi_event_t
{
	ft2_midi_event_type_t type;
	uint8_t channel;    /* MIDI channel 0-15 */
	uint8_t note;       /* MIDI note 0-127 */
	uint8_t velocity;   /* Velocity 0-127 */
	uint8_t program;    /* Program number for program change */
	int32_t samplePos;  /* Sample position in buffer for timing */
} ft2_midi_event_t;

#define FT2_MIDI_QUEUE_LEN 256

/**
 * MIDI output event queue (lock-free SPSC)
 */
typedef struct ft2_midi_queue_t
{
	ft2_midi_event_t events[FT2_MIDI_QUEUE_LEN];
	volatile int32_t readPos;
	volatile int32_t writePos;
} ft2_midi_queue_t;

typedef struct ft2_instance_t
{
	ft2_audio_state_t audio;
	ft2_replayer_state_t replayer;
	ft2_voice_t voice[FT2_MAX_CHANNELS * 2];
	ft2_editor_t editor;
	ft2_ui_state_t uiState;
	ft2_cursor_t cursor;
	ft2_nibbles_state_t nibbles;
	ft2_diskop_state_t diskop;   /* Disk Op. state */
	ft2_plugin_config_t config;  /* Per-instance configuration */
	ft2_scope_sync_queue_t scopeSyncQueue;  /* Audio-to-UI scope sync */
	ft2_timemap_t timemap;                  /* DAW position sync time map */
	ft2_midi_queue_t midiOutQueue;          /* MIDI output event queue */

	struct ft2_ui_t *ui;  /* UI state (allocated by ft2_ui_create) */

	uint32_t sampleRate;
	float fAudioNormalizeMul;
	float fSqrtPanningTable[256 + 1];

	uint32_t tickTimeLenInt;
	uint64_t tickTimeLenFrac;

	uint32_t randSeed;
	float fPrngStateL, fPrngStateR;

	volatile bool scopesClearRequested;  /* Set by audio thread, cleared by UI after stopping scopes */
} ft2_instance_t;

/**
 * @brief Push scope sync entry from audio thread.
 * @param inst The instance.
 * @param entry The sync entry to push.
 */
void ft2_scope_sync_queue_push(ft2_instance_t *inst, const ft2_scope_sync_entry_t *entry);

/**
 * @brief Pop scope sync entry in UI thread.
 * @param inst The instance.
 * @param entry Output: the popped entry.
 * @return true if entry was available, false if queue empty.
 */
bool ft2_scope_sync_queue_pop(ft2_instance_t *inst, ft2_scope_sync_entry_t *entry);

/**
 * @brief Push MIDI output event from audio thread.
 * @param inst The instance.
 * @param event The MIDI event to queue.
 */
void ft2_midi_queue_push(ft2_instance_t *inst, const ft2_midi_event_t *event);

/**
 * @brief Pop MIDI output event in processBlock.
 * @param inst The instance.
 * @param event Output: the popped event.
 * @return true if event was available, false if queue empty.
 */
bool ft2_midi_queue_pop(ft2_instance_t *inst, ft2_midi_event_t *event);

/**
 * @brief Clear all pending MIDI output events.
 * @param inst The instance.
 */
void ft2_midi_queue_clear(ft2_instance_t *inst);

/**
 * @brief Creates and initializes a new FT2 instance.
 * @param sampleRate The audio sample rate in Hz.
 * @return Pointer to the new instance, or NULL on failure.
 */
ft2_instance_t *ft2_instance_create(uint32_t sampleRate);

/**
 * @brief Destroys an FT2 instance and frees all associated memory.
 * @param instance The instance to destroy.
 */
void ft2_instance_destroy(ft2_instance_t *instance);

/**
 * @brief Resets an instance to its initial state.
 * @param instance The instance to reset.
 */
void ft2_instance_reset(ft2_instance_t *instance);

/**
 * @brief Sets the sample rate for an instance.
 * @param instance The instance.
 * @param sampleRate The new sample rate in Hz.
 */
void ft2_instance_set_sample_rate(ft2_instance_t *instance, uint32_t sampleRate);

/**
 * @brief Loads an XM module into an instance.
 * @param instance The instance.
 * @param data Pointer to the XM file data.
 * @param dataSize Size of the data in bytes.
 * @return true on success, false on failure.
 */
bool ft2_instance_load_xm(ft2_instance_t *instance, const uint8_t *data, uint32_t dataSize);

/**
 * @brief Starts playback.
 * @param instance The instance.
 * @param mode Playback mode (FT2_PLAYMODE_SONG, FT2_PLAYMODE_PATT, etc.).
 * @param startRow Starting row.
 */
void ft2_instance_play(ft2_instance_t *instance, int8_t mode, int16_t startRow);

/**
 * @brief Stops playback.
 * @param instance The instance.
 */
void ft2_instance_stop(ft2_instance_t *instance);

/**
 * @brief Start playing a specific pattern (for MIDI pattern trigger mode).
 * Unlike ft2_instance_play which uses the song's orders array, this sets
 * pattNum directly to allow triggering any pattern by MIDI note number.
 * @param instance The instance.
 * @param patternNum Pattern number (0-255).
 * @param startRow Starting row.
 */
void ft2_instance_play_pattern(ft2_instance_t *instance, uint8_t patternNum, int16_t startRow);

/**
 * @brief Triggers a note on a channel.
 * @param instance The instance.
 * @param note Note number (1-96).
 * @param instr Instrument number (0-127).
 * @param channel Channel number.
 * @param volume Volume (0-64).
 */
void ft2_instance_trigger_note(ft2_instance_t *instance, int8_t note, uint8_t instr, uint8_t channel, uint8_t volume);

/**
 * @brief Releases a note on a channel.
 * @param instance The instance.
 * @param channel Channel number.
 */
void ft2_instance_release_note(ft2_instance_t *instance, uint8_t channel);

/**
 * @brief Plays a specific sample with optional offset and length.
 * @param instance The instance.
 * @param note Note number (1-96).
 * @param instr Instrument number.
 * @param smpNum Sample number within instrument (0-15).
 * @param channel Channel number.
 * @param volume Volume (0-64).
 * @param offset Sample start offset (0 for full sample).
 * @param length Length to play (0 for full sample from offset).
 */
void ft2_instance_play_sample(ft2_instance_t *instance, int8_t note, uint8_t instr, uint8_t smpNum, uint8_t channel, uint8_t volume, int32_t offset, int32_t length);

/**
 * @brief Renders audio to a buffer.
 * @param instance The instance.
 * @param outputL Left channel output buffer.
 * @param outputR Right channel output buffer.
 * @param numSamples Number of samples to render.
 */
void ft2_instance_render(ft2_instance_t *instance, float *outputL, float *outputR, uint32_t numSamples);

/**
 * @brief Mix voices without advancing replayer tick (for jam/preview mode).
 * @param instance The instance.
 * @param outputL Left channel output buffer.
 * @param outputR Right channel output buffer.
 * @param numSamples Number of samples to render.
 */
void ft2_mix_voices_only(ft2_instance_t *instance, float *outputL, float *outputR, uint32_t numSamples);

/**
 * @brief Render audio with multi-output support (per-channel buffers).
 * @param instance The instance.
 * @param mainOutL Main mix left channel output buffer.
 * @param mainOutR Main mix right channel output buffer.
 * @param numSamples Number of samples to render.
 * @note Per-channel outputs are stored in instance->audio.fChannelBufferL/R
 */
void ft2_instance_render_multiout(ft2_instance_t *instance, float *mainOutL, float *mainOutR, uint32_t numSamples);

/**
 * @brief Enable/disable multi-output mode and allocate buffers.
 * @param instance The instance.
 * @param enabled true to enable multi-output, false to disable.
 * @param bufferSize Size of per-channel buffers (in samples).
 * @return true on success, false on allocation failure.
 */
bool ft2_instance_set_multiout(ft2_instance_t *instance, bool enabled, uint32_t bufferSize);

/**
 * @brief Gets the current playback position.
 * @param instance The instance.
 * @param outSongPos Output for song position.
 * @param outRow Output for current row.
 */
void ft2_instance_get_position(ft2_instance_t *instance, int16_t *outSongPos, int16_t *outRow);

/**
 * @brief Sets the playback position.
 * @param instance The instance.
 * @param songPos Song position.
 * @param row Row within the pattern.
 */
void ft2_instance_set_position(ft2_instance_t *instance, int16_t songPos, int16_t row);

/**
 * @brief Allocates an instrument in the instance.
 * @param instance The instance.
 * @param insNum Instrument number (1-128).
 * @return true on success, false on failure.
 */
bool ft2_instance_alloc_instr(ft2_instance_t *instance, int16_t insNum);

/**
 * @brief Frees an instrument in the instance.
 * @param instance The instance.
 * @param insNum Instrument number (1-128).
 */
void ft2_instance_free_instr(ft2_instance_t *instance, int32_t insNum);

/**
 * @brief Frees all instruments in the instance.
 * @param instance The instance.
 */
void ft2_instance_free_all_instr(ft2_instance_t *instance);

/**
 * @brief Frees all patterns in the instance.
 * @param instance The instance.
 */
void ft2_instance_free_all_patterns(ft2_instance_t *instance);

/**
 * @brief Sets the interpolation type for audio mixing.
 * @param instance The instance.
 * @param type 0 = no interpolation, 1 = linear (default).
 */
void ft2_instance_set_interpolation(ft2_instance_t *instance, uint8_t type);

/**
 * @brief Initialize BPM-related timing variables from current song BPM.
 *
 * This updates samplesPerTickInt, samplesPerTickFrac, fSamplesPerTickIntMul,
 * tickTimeLenInt, and tickTimeLenFrac from the current song.BPM value.
 * Matches standalone setMixerBPM() behavior.
 *
 * @param instance The instance.
 */
void ft2_instance_init_bpm_vars(ft2_instance_t *instance);

/**
 * @brief Marks the song as modified and invalidates the time map.
 * Call this instead of setting song.isModified directly.
 * @param inst The instance.
 */
void ft2_song_mark_modified(ft2_instance_t *inst);

/**
 * @brief Validates and clamps instrument parameters.
 * @param ins The instrument to sanitize.
 */
void ft2_sanitize_instrument(ft2_instr_t *ins);

/**
 * @brief Validates and clamps sample parameters.
 * @param s The sample to sanitize.
 */
void ft2_sanitize_sample(ft2_sample_t *s);

/**
 * @brief Prepares sample for branchless mixer interpolation.
 *
 * Modifies samples before index 0, and after loop/end for interpolation.
 * This must be called after loading or modifying sample data.
 *
 * @param s The sample to fix.
 */
void ft2_fix_sample(ft2_sample_t *s);

/**
 * @brief Restores sample data that was modified by ft2_fix_sample().
 *
 * Call this before modifying sample data to restore original values.
 *
 * @param s The sample to unfix.
 */
void ft2_unfix_sample(ft2_sample_t *s);

/**
 * @brief Sets the audio amplification multiplier.
 *
 * Calculates fAudioNormalizeMul from boost level and master volume.
 * Matches standalone setAudioAmp() behavior.
 *
 * @param inst The instance.
 * @param boostLevel Amplification level (1-32).
 * @param masterVol Master volume (0-256).
 */
void ft2_instance_set_audio_amp(ft2_instance_t *inst, int16_t boostLevel, int16_t masterVol);

#ifdef __cplusplus
}
#endif
