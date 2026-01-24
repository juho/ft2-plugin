/**
 * @file ft2_instance.c
 * @brief Implementation of the instance-based FT2 replayer API.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include "ft2_instance.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_loader.h"
#include "ft2_plugin_interpolation.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_sample_utils.h"

#define INITIAL_DITHER_SEED 0x12345000
#define DEFAULT_SAMPLE_RATE 48000
#define TICK_TIME_FRAC_SCALE (1ULL << 52)

/* Period lookup tables (from ft2_tables_plugin.c) */
extern const uint16_t linearPeriodLUT[1936];
extern const uint16_t amigaPeriodLUT[1936];

static void calcPanningTableInstance(ft2_instance_t *inst)
{
	for (int32_t i = 0; i <= 256; i++)
		inst->fSqrtPanningTable[i] = sqrtf((float)i / 256.0f);
}

static void calcReplayerVarsInstance(ft2_instance_t *inst, uint32_t sampleRate)
{
	if (sampleRate == 0)
		return;

	inst->sampleRate = sampleRate;
	inst->audio.freq = sampleRate;

	const double dSampleRate = (double)sampleRate;

	for (int32_t i = FT2_MIN_BPM; i <= FT2_MAX_BPM; i++)
	{
		const int32_t index = i - FT2_MIN_BPM;
		const double dBPM = (double)i;

		const double dSamplesPerTick = dSampleRate / (dBPM / 2.5);
		const double dTickTimeLen = (double)TICK_TIME_FRAC_SCALE / (dBPM / 2.5);

		inst->audio.samplesPerTickIntTab[index] = (uint32_t)dSamplesPerTick;
		inst->audio.samplesPerTickFracTab[index] = 
			(uint64_t)((dSamplesPerTick - (double)inst->audio.samplesPerTickIntTab[index]) * (double)(1ULL << 32));

		inst->audio.tickTimeIntTab[index] = (uint32_t)dTickTimeLen;
		inst->audio.tickTimeFracTab[index] = 
			(uint64_t)((dTickTimeLen - (double)inst->audio.tickTimeIntTab[index]) * (double)(1ULL << 32));
	}

	inst->audio.quickVolRampSamples = (uint32_t)round(dSampleRate / 200.0);
	if (inst->audio.quickVolRampSamples < 1)
		inst->audio.quickVolRampSamples = 1;

	inst->audio.fQuickVolRampSamplesMul = 1.0f / (float)inst->audio.quickVolRampSamples;

	/* Calculate logTab - matches standalone calcMiscReplayerVars() + calcReplayerVars() */
	const double logTabMul = (UINT32_MAX + 1.0) / dSampleRate;
	for (int32_t i = 0; i < 4 * 12 * 16; i++)
	{
		const double dLogTabVal = (8363.0 * 256.0) * exp2(i / (4.0 * 12.0 * 16.0));
		inst->replayer.dLogTab[i] = dLogTabVal;
		inst->replayer.logTab[i] = (uint64_t)round(dLogTabVal * logTabMul);
	}

	/* Calculate dExp2MulTab - matches standalone calcMiscReplayerVars() */
	for (int32_t i = 0; i < 32; i++)
		inst->replayer.dExp2MulTab[i] = 1.0 / exp2(i);

	inst->replayer.amigaPeriodDiv = (uint64_t)round((UINT32_MAX + 1.0) * (1712.0 * 8363.0) / dSampleRate);
}

static void initReplayerState(ft2_instance_t *inst)
{
	memset(&inst->replayer, 0, sizeof(ft2_replayer_state_t));

	inst->replayer.note2PeriodLUT = linearPeriodLUT;

	for (int32_t i = 0; i < FT2_MAX_PATTERNS; i++)
		inst->replayer.patternNumRows[i] = 64;

	for (int32_t i = 0; i < FT2_MAX_CHANNELS; i++)
	{
		inst->replayer.channel[i].instrPtr = inst->replayer.instr[0];
		inst->replayer.channel[i].status = FT2_CS_UPDATE_VOL;
		inst->replayer.channel[i].oldPan = 128;
		inst->replayer.channel[i].outPan = 128;
		inst->replayer.channel[i].finalPan = 128;
	}

	inst->replayer.song.speed = 6;
	inst->replayer.song.BPM = 125;
	inst->replayer.song.globalVolume = 64;
	inst->replayer.song.numChannels = 8;
	inst->replayer.song.songLength = 1;

	/* Initialize scope delta lookup tables (matching standalone calcMiscReplayerVars) */
	#define SCOPE_FRAC_SCALE ((int64_t)1 << 32)
	#define SCOPE_HZ 64
	#define C4_FREQ 8363.0

	for (int32_t i = 0; i < 4*12*16; i++)
	{
		double dLogTab = (8363.0 * 256.0) * exp2(i / (4.0 * 12.0 * 16.0));
		inst->replayer.scopeLogTab[i] = (uint64_t)round(dLogTab * (SCOPE_FRAC_SCALE / SCOPE_HZ));
		inst->replayer.scopeDrawLogTab[i] = (uint64_t)round(dLogTab * (SCOPE_FRAC_SCALE / (C4_FREQ / 2.0)));
	}

	inst->replayer.scopeAmigaPeriodDiv = (uint64_t)round((SCOPE_FRAC_SCALE * (1712.0 * 8363.0)) / SCOPE_HZ);
	inst->replayer.scopeDrawAmigaPeriodDiv = (uint64_t)round((SCOPE_FRAC_SCALE * (1712.0 * 8363.0)) / (C4_FREQ / 2.0));

	#undef SCOPE_FRAC_SCALE
	#undef SCOPE_HZ
	#undef C4_FREQ
}

static void initAudioState(ft2_instance_t *inst)
{
	memset(&inst->audio, 0, sizeof(ft2_audio_state_t));
	inst->audio.linearPeriodsFlag = true;
	inst->audio.volumeRampingFlag = true;
	inst->audio.interpolationType = 1; /* Linear interpolation by default */
}

void ft2_instance_init_bpm_vars(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	const int32_t bpmIdx = inst->replayer.song.BPM - FT2_MIN_BPM;
	inst->audio.samplesPerTickInt = inst->audio.samplesPerTickIntTab[bpmIdx];
	inst->audio.samplesPerTickFrac = inst->audio.samplesPerTickFracTab[bpmIdx];
	inst->audio.fSamplesPerTickIntMul = 1.0f / (float)inst->audio.samplesPerTickInt;
	inst->tickTimeLenInt = inst->audio.tickTimeIntTab[bpmIdx];
	inst->tickTimeLenFrac = inst->audio.tickTimeFracTab[bpmIdx];
}

void ft2_instance_set_audio_amp(ft2_instance_t *inst, int16_t boostLevel, int16_t masterVol)
{
	if (inst == NULL)
		return;

	if (boostLevel < 1) boostLevel = 1;
	if (boostLevel > 32) boostLevel = 32;
	if (masterVol < 0) masterVol = 0;
	if (masterVol > 256) masterVol = 256;

	inst->fAudioNormalizeMul = (boostLevel * masterVol) / (32.0f * 256.0f);
}

static void initVoices(ft2_instance_t *inst)
{
	memset(inst->voice, 0, sizeof(inst->voice));
	for (int32_t i = 0; i < FT2_MAX_CHANNELS * 2; i++)
		inst->voice[i].panning = 128;
}

static void initEditorState(ft2_instance_t *inst)
{
	memset(&inst->editor, 0, sizeof(ft2_editor_t));

	inst->editor.curInstr = 1;
	inst->editor.curSmp = 0;
	inst->editor.curOctave = 4;
	inst->editor.editRowSkip = 1;  /* Default to 1 so cursor advances when entering notes */
	inst->editor.BPM = 125;
	inst->editor.speed = 6;
	inst->editor.globalVolume = 64;
	inst->editor.smpEd_NoteNr = 48;  /* C-4 (note 48 = 4*12) - same as standalone */
	inst->editor.volScaleStart = 1.0;
	inst->editor.volScaleEnd = 1.0;
	inst->editor.ptnJumpPos[0] = 0x00;
	inst->editor.ptnJumpPos[1] = 0x10;
	inst->editor.ptnJumpPos[2] = 0x20;
	inst->editor.ptnJumpPos[3] = 0x30;

	/* Advanced edit defaults - matching standalone ft2_main.c */
	inst->editor.srcInstr = 1;
	inst->editor.copyMaskEnable = true;
	for (int32_t i = 0; i < 5; i++)
	{
		inst->editor.copyMask[i] = 1;
		inst->editor.pasteMask[i] = 1;
		inst->editor.transpMask[i] = 0;  /* Transpose masks are OFF by default */
	}
}

static void initUIState(ft2_instance_t *inst)
{
	memset(&inst->uiState, 0, sizeof(ft2_ui_state_t));

	inst->uiState.patternEditorShown = true;
	inst->uiState.scopesShown = true;
	inst->uiState.instrSwitcherShown = true;
	inst->uiState.numChannelsShown = 8;
	inst->uiState.maxVisibleChannels = 8;
	inst->uiState.channelOffset = 0;
	inst->uiState.patternChannelWidth = 75; /* Default width for 8 channels */
	inst->uiState.ptnShowVolColumn = true;  /* Show volume column by default */
	inst->uiState.ptnHex = true;            /* Hex notation by default */
	inst->uiState.ptnLineLight = true;      /* Line highlighting by default */
	inst->uiState.ptnChnNumbers = true;     /* Channel numbers by default */
	inst->uiState.ptnFrmWrk = true;         /* Draw framework by default */
}

static void initCursor(ft2_instance_t *inst)
{
	memset(&inst->cursor, 0, sizeof(ft2_cursor_t));
	inst->cursor.ch = 0;
	inst->cursor.object = 0;
}

static void initDiskopState(ft2_instance_t *inst)
{
	memset(&inst->diskop, 0, sizeof(ft2_diskop_state_t));
	inst->diskop.selectedEntry = -1;
	inst->diskop.requestOpenEntry = -1;
	inst->diskop.requestLoadEntry = -1;
	inst->diskop.itemType = FT2_DISKOP_ITEM_MODULE;
	inst->diskop.saveFormat[FT2_DISKOP_ITEM_MODULE] = FT2_MOD_SAVE_XM;
	inst->diskop.saveFormat[FT2_DISKOP_ITEM_INSTR] = FT2_INS_SAVE_XI;
	inst->diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE] = FT2_SMP_SAVE_WAV;
	inst->diskop.instrSaveFormatExplicit = false;
	inst->diskop.firstOpen = true;
	inst->diskop.lastClickedEntry = -1;
}

/* --------------------------------------------------------------------- */
/*                     SCOPE SYNC QUEUE                                  */
/* --------------------------------------------------------------------- */

void ft2_scope_sync_queue_push(ft2_instance_t *inst, const ft2_scope_sync_entry_t *entry)
{
	if (inst == NULL || entry == NULL)
		return;

	ft2_scope_sync_queue_t *q = &inst->scopeSyncQueue;
	int32_t nextWritePos = (q->writePos + 1) % FT2_SCOPE_SYNC_QUEUE_LEN;
	
	if (nextWritePos == q->readPos)
		return; /* Queue full, drop entry */
	
	q->entries[q->writePos] = *entry;
	q->writePos = nextWritePos;
}

bool ft2_scope_sync_queue_pop(ft2_instance_t *inst, ft2_scope_sync_entry_t *entry)
{
	if (inst == NULL || entry == NULL)
		return false;

	ft2_scope_sync_queue_t *q = &inst->scopeSyncQueue;
	
	if (q->readPos == q->writePos)
		return false; /* Queue empty */
	
	*entry = q->entries[q->readPos];
	q->readPos = (q->readPos + 1) % FT2_SCOPE_SYNC_QUEUE_LEN;
	return true;
}

void ft2_midi_queue_push(ft2_instance_t *inst, const ft2_midi_event_t *event)
{
	if (inst == NULL || event == NULL)
		return;

	ft2_midi_queue_t *q = &inst->midiOutQueue;
	int32_t nextWritePos = (q->writePos + 1) % FT2_MIDI_QUEUE_LEN;
	
	if (nextWritePos == q->readPos)
		return; /* Queue full, drop event */
	
	q->events[q->writePos] = *event;
	q->writePos = nextWritePos;
}

bool ft2_midi_queue_pop(ft2_instance_t *inst, ft2_midi_event_t *event)
{
	if (inst == NULL || event == NULL)
		return false;

	ft2_midi_queue_t *q = &inst->midiOutQueue;
	
	if (q->readPos == q->writePos)
		return false; /* Queue empty */
	
	*event = q->events[q->readPos];
	q->readPos = (q->readPos + 1) % FT2_MIDI_QUEUE_LEN;
	return true;
}

void ft2_midi_queue_clear(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	inst->midiOutQueue.readPos = 0;
	inst->midiOutQueue.writePos = 0;
}

ft2_instance_t *ft2_instance_create(uint32_t sampleRate)
{
	ft2_instance_t *inst = (ft2_instance_t *)calloc(1, sizeof(ft2_instance_t));
	if (inst == NULL)
		return NULL;

	if (sampleRate == 0)
		sampleRate = DEFAULT_SAMPLE_RATE;

	/* Initialize global interpolation tables (ref counted) */
	if (!ft2_interp_tables_init())
	{
		free(inst);
		return NULL;
	}

	inst->randSeed = INITIAL_DITHER_SEED;

	initAudioState(inst);
	initReplayerState(inst);
	initVoices(inst);
	initEditorState(inst);
	initUIState(inst);
	initCursor(inst);
	initDiskopState(inst);
	ft2_nibbles_init(inst);  /* Initialize nibbles game state */
	ft2_config_init(&inst->config);  /* Initialize per-instance config */
	ft2_timemap_init(&inst->timemap);  /* Initialize DAW position sync time map */
	calcPanningTableInstance(inst);
	calcReplayerVarsInstance(inst, sampleRate);
	ft2_instance_init_bpm_vars(inst);
	ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);
	
	/* Allocate placeholder instrument 0 (used when no instrument is specified) */
	ft2_instance_alloc_instr(inst, 0);
	if (inst->replayer.instr[0] != NULL)
		inst->replayer.instr[0]->smp[0].volume = 0;
	
	/* Allocate default instrument (curInstr=1) so sample names can be edited */
	ft2_instance_alloc_instr(inst, 1);

	const uint32_t maxSamplesPerTick = inst->audio.samplesPerTickIntTab[FT2_MAX_BPM - FT2_MIN_BPM] + 1;

	inst->audio.fMixBufferL = (float *)calloc(maxSamplesPerTick * 2, sizeof(float));
	inst->audio.fMixBufferR = (float *)calloc(maxSamplesPerTick * 2, sizeof(float));

	if (inst->audio.fMixBufferL == NULL || inst->audio.fMixBufferR == NULL)
	{
		ft2_instance_destroy(inst);
		return NULL;
	}

	return inst;
}

void ft2_instance_destroy(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_instance_free_all_instr(inst);
	ft2_instance_free_all_patterns(inst);

	if (inst->audio.fMixBufferL != NULL)
		free(inst->audio.fMixBufferL);

	if (inst->audio.fMixBufferR != NULL)
		free(inst->audio.fMixBufferR);

	/* Free per-channel multi-out buffers */
	for (int i = 0; i < FT2_MAX_CHANNELS; i++)
	{
		if (inst->audio.fChannelBufferL[i] != NULL)
			free(inst->audio.fChannelBufferL[i]);
		if (inst->audio.fChannelBufferR[i] != NULL)
			free(inst->audio.fChannelBufferR[i]);
	}

	/* Free diskop file list */
	if (inst->diskop.entries != NULL)
		free(inst->diskop.entries);

	/* Free time map */
	ft2_timemap_free(&inst->timemap);

	/* Release reference to global interpolation tables */
	ft2_interp_tables_free();

	free(inst);
}

void ft2_instance_reset(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	const uint32_t sampleRate = inst->sampleRate;
	float *mixL = inst->audio.fMixBufferL;
	float *mixR = inst->audio.fMixBufferR;

	ft2_instance_free_all_instr(inst);
	ft2_instance_free_all_patterns(inst);

	initAudioState(inst);
	initReplayerState(inst);
	initVoices(inst);
	initEditorState(inst);
	initUIState(inst);
	initCursor(inst);

	inst->audio.fMixBufferL = mixL;
	inst->audio.fMixBufferR = mixR;
	inst->randSeed = INITIAL_DITHER_SEED;

	calcReplayerVarsInstance(inst, sampleRate);
	ft2_instance_init_bpm_vars(inst);
	ft2_instance_set_audio_amp(inst, inst->config.boostLevel, inst->config.masterVol);
	
	/* Allocate placeholder instrument 0 (used when no instrument is specified) */
	ft2_instance_alloc_instr(inst, 0);
	if (inst->replayer.instr[0] != NULL)
		inst->replayer.instr[0]->smp[0].volume = 0;
	
	/* Allocate default instrument (curInstr=1) so sample names can be edited */
	ft2_instance_alloc_instr(inst, 1);
}

void ft2_instance_set_sample_rate(ft2_instance_t *inst, uint32_t sampleRate)
{
	if (inst == NULL || sampleRate == 0)
		return;

	calcReplayerVarsInstance(inst, sampleRate);

	const uint32_t maxSamplesPerTick = inst->audio.samplesPerTickIntTab[FT2_MAX_BPM - FT2_MIN_BPM] + 1;

	float *newL = (float *)realloc(inst->audio.fMixBufferL, maxSamplesPerTick * 2 * sizeof(float));
	float *newR = (float *)realloc(inst->audio.fMixBufferR, maxSamplesPerTick * 2 * sizeof(float));

	if (newL != NULL)
		inst->audio.fMixBufferL = newL;
	if (newR != NULL)
		inst->audio.fMixBufferR = newR;
}

bool ft2_instance_alloc_instr(ft2_instance_t *inst, int16_t insNum)
{
	if (inst == NULL || insNum < 0 || insNum > FT2_MAX_INST + 4)
		return false;

	if (inst->replayer.instr[insNum] != NULL)
		return true;

	ft2_instr_t *p = (ft2_instr_t *)calloc(1, sizeof(ft2_instr_t));
	if (p == NULL)
		return false;

	for (int32_t i = 0; i < FT2_MAX_SMP_PER_INST; i++)
	{
		p->smp[i].panning = 128;
		p->smp[i].volume = 64;
	}

	/* Apply default envelope preset 0 (matches standalone setStdEnvelope(p, 0, 3)) */
	ft2_plugin_config_t *cfg = &inst->config;
	
	/* Volume envelope from preset 0 */
	memcpy(p->volEnvPoints, cfg->stdEnvPoints[0][0], sizeof(int16_t) * 12 * 2);
	memcpy(p->volEnvCurve, cfg->stdEnvCurve[0][0], sizeof(p->volEnvCurve));
	p->volEnvLength = (uint8_t)cfg->stdVolEnvLength[0];
	p->volEnvSustain = (uint8_t)cfg->stdVolEnvSustain[0];
	p->volEnvLoopStart = (uint8_t)cfg->stdVolEnvLoopStart[0];
	p->volEnvLoopEnd = (uint8_t)cfg->stdVolEnvLoopEnd[0];
	p->volEnvFlags = (uint8_t)cfg->stdVolEnvFlags[0];
	p->fadeout = cfg->stdFadeout[0];
	p->autoVibRate = (uint8_t)cfg->stdVibRate[0];
	p->autoVibDepth = (uint8_t)cfg->stdVibDepth[0];
	p->autoVibSweep = (uint8_t)cfg->stdVibSweep[0];
	p->autoVibType = (uint8_t)cfg->stdVibType[0];
	
	/* Panning envelope from preset 0 */
	memcpy(p->panEnvPoints, cfg->stdEnvPoints[0][1], sizeof(int16_t) * 12 * 2);
	memcpy(p->panEnvCurve, cfg->stdEnvCurve[0][1], sizeof(p->panEnvCurve));
	p->panEnvLength = (uint8_t)cfg->stdPanEnvLength[0];
	p->panEnvSustain = (uint8_t)cfg->stdPanEnvSustain[0];
	p->panEnvLoopStart = (uint8_t)cfg->stdPanEnvLoopStart[0];
	p->panEnvLoopEnd = (uint8_t)cfg->stdPanEnvLoopEnd[0];
	p->panEnvFlags = (uint8_t)cfg->stdPanEnvFlags[0];
	p->volEnvSpeed = 1;
	p->panEnvSpeed = 1;

	inst->replayer.instr[insNum] = p;
	return true;
}

void ft2_instance_free_instr(ft2_instance_t *inst, int32_t insNum)
{
	if (inst == NULL || insNum < 0 || insNum > FT2_MAX_INST + 4)
		return;

	ft2_instr_t *ins = inst->replayer.instr[insNum];
	if (ins == NULL)
		return;

	ft2_sample_t *s = ins->smp;
	for (int32_t i = 0; i < FT2_MAX_SMP_PER_INST; i++, s++)
	{
		if (s->origDataPtr != NULL)
		{
			free(s->origDataPtr);
			s->origDataPtr = NULL;
		}
		s->dataPtr = NULL;

		if (s->origDataPtr24 != NULL)
		{
			free(s->origDataPtr24);
			s->origDataPtr24 = NULL;
		}
		s->dataPtr24 = NULL;

		if (s->origDataPtrF != NULL)
		{
			free(s->origDataPtrF);
			s->origDataPtrF = NULL;
		}
		s->dataPtrF = NULL;

		/* Free right channel for stereo samples */
		if (s->origDataPtrR != NULL)
		{
			free(s->origDataPtrR);
			s->origDataPtrR = NULL;
		}
		s->dataPtrR = NULL;

		if (s->origDataPtr24R != NULL)
		{
			free(s->origDataPtr24R);
			s->origDataPtr24R = NULL;
		}
		s->dataPtr24R = NULL;

		if (s->origDataPtrFR != NULL)
		{
			free(s->origDataPtrFR);
			s->origDataPtrFR = NULL;
		}
		s->dataPtrFR = NULL;
	}

	free(ins);
	inst->replayer.instr[insNum] = NULL;
}

void ft2_instance_free_all_instr(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	for (int32_t i = 0; i < 128 + 4; i++)
		ft2_instance_free_instr(inst, i);
}

void ft2_instance_free_all_patterns(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	for (int32_t i = 0; i < FT2_MAX_PATTERNS; i++)
	{
		if (inst->replayer.pattern[i] != NULL)
		{
			free(inst->replayer.pattern[i]);
			inst->replayer.pattern[i] = NULL;
		}
		inst->replayer.patternNumRows[i] = 64;
	}
}

void ft2_instance_stop(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->replayer.songPlaying = false;
	inst->replayer.playMode = FT2_PLAYMODE_IDLE;

	if (inst->config.killNotesOnStopPlay)
	{
		/* Fadeout all voices smoothly instead of immediate cutoff to prevent clicks */
		ft2_fadeout_all_voices(inst);

		for (int32_t i = 0; i < FT2_MAX_CHANNELS; i++)
		{
			inst->replayer.channel[i].status = 0;
			inst->replayer.channel[i].keyOff = false;
		}
	}

	/* Update UI (matches standalone stopPlaying) */
	inst->uiState.updatePosSections = true;
	inst->uiState.updatePatternEditor = true;
}

void ft2_instance_trigger_note(ft2_instance_t *inst, int8_t note, uint8_t instr, uint8_t channel, 
                               int8_t volume, uint16_t midiVibDepth, int16_t midiPitch)
{
	if (inst == NULL || channel >= FT2_MAX_CHANNELS)
		return;
	
	if (note < 1 || note > 96)
		return;
	
	if (instr > 127)
		return;
	
	ft2_channel_t *ch = &inst->replayer.channel[channel];
	
	/* Get instrument and sample */
	ft2_instr_t *instrPtr = inst->replayer.instr[instr];
	if (instrPtr == NULL)
		return;
	
	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	uint8_t smpNum = instrPtr->note2SampleLUT[note - 1];
	if (!inst->config.enableXm2Features)
		smpNum &= 0x0F;
	else if (smpNum >= maxSamples)
		smpNum = (uint8_t)(maxSamples - 1);
	if (smpNum >= maxSamples)
		return;
	
	ft2_sample_t *smp = &instrPtr->smp[smpNum];
	const ft2_sample_format_t format = ft2_sample_get_format(smp);
	if (!ft2_sample_has_data(smp, format) || smp->length == 0)
		return;
	
	/* Set up channel state (matches standalone playTone) */
	ch->noteNum = note;
	ch->instrNum = instr;
	ch->instrPtr = instrPtr;
	ch->smpPtr = smp;
	ch->smpNum = smpNum;
	ch->relativeNote = smp->relativeNote;
	ch->finetune = smp->finetune;
	ch->oldVol = smp->volume;
	ch->oldPan = smp->panning;
	ch->efx = 0;
	ch->efxData = 0;
	ch->keyOff = false;
	ch->smpStartPos = 0;
	ch->smpEndPos = -1;  /* No range override for keyjazz */
	
	/* Calculate period using lookup tables (matches standalone triggerNote) */
	int32_t relNote = note + smp->relativeNote;
	if (relNote < 1) relNote = 1;
	if (relNote >= 10 * 12) relNote = (10 * 12) - 1;
	
	const uint16_t noteIndex = ((relNote - 1) * 16) + (((int8_t)smp->finetune >> 3) + 16);
	const uint16_t *lut = inst->audio.linearPeriodsFlag ? linearPeriodLUT : amigaPeriodLUT;
	ch->outPeriod = ch->realPeriod = lut[noteIndex];
	
	/* Initialize volumes from sample (matches resetVolumes) */
	ft2_channel_reset_volumes(ch);
	
	/* Initialize envelopes, fadeout, auto-vibrato (matches triggerInstrument) */
	ft2_channel_trigger_instrument(ch);
	
	/* Override volume if specified (volume == -1 means use sample volume) */
	if (volume >= 0)
	{
		ch->realVol = (uint8_t)volume;
		ch->outVol = (uint8_t)volume;
		ch->oldVol = (uint8_t)volume;
	}
	
	/* Apply MIDI modulation (matches standalone playTone) */
	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;
	
	/* Set status flags for voice trigger (voice triggered via ft2_update_voices) */
	ch->status = FT2_CS_TRIGGER_VOICE | FT2_CS_UPDATE_VOL | FT2_CS_UPDATE_PAN | 
	             FT2_CF_UPDATE_PERIOD | FT2_CS_USE_QUICK_VOLRAMP;

	/* Process envelopes immediately (matches updateVolPanAutoVib) */
	ft2_channel_update_vol_pan_autovib(inst, ch);

	/* Track which channel is playing the current sample for sample editor playhead */
	if (instr == inst->editor.curInstr)
		inst->editor.curSmpChannel = channel;

	/* MIDI Output - send note on if instrument has midiOn enabled */
	if (instrPtr->midiOn && !instrPtr->mute)
	{
		/* Convert FT2 note (1-96) to MIDI note (0-127) */
		int midiNote = relNote + 11;
		if (midiNote >= 0 && midiNote <= 127)
		{
			/* Send note off for previous note on this channel */
			if (ch->midiNoteActive && ch->lastMidiNote != (uint8_t)midiNote)
			{
				ft2_midi_event_t offEvent = {0};
				offEvent.type = FT2_MIDI_NOTE_OFF;
				offEvent.channel = instrPtr->midiChannel;
				offEvent.note = ch->lastMidiNote;
				ft2_midi_queue_push(inst, &offEvent);
			}

			/* Send note on */
			ft2_midi_event_t onEvent = {0};
			onEvent.type = FT2_MIDI_NOTE_ON;
			onEvent.channel = instrPtr->midiChannel;
			onEvent.note = (uint8_t)midiNote;
			onEvent.velocity = (ch->outVol > 0) ? (uint8_t)((ch->outVol * 127) / 64) : 100;
			ft2_midi_queue_push(inst, &onEvent);

			ch->lastMidiNote = (uint8_t)midiNote;
			ch->midiNoteActive = true;
		}
	}
}

void ft2_instance_release_note(ft2_instance_t *inst, uint8_t channel)
{
	if (inst == NULL || channel >= FT2_MAX_CHANNELS)
		return;
	
	ft2_channel_t *ch = &inst->replayer.channel[channel];
	ft2_key_off(inst, ch);
}

void ft2_instance_play_sample(ft2_instance_t *inst, int8_t note, uint8_t instr, uint8_t smpNum, uint8_t channel, uint8_t volume, int32_t offset, int32_t length)
{
	if (inst == NULL || channel >= FT2_MAX_CHANNELS)
		return;

	if (note < 1 || note > 96)
		return;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (instr > 127 || smpNum >= maxSamples)
		return;

	ft2_instr_t *instrPtr = inst->replayer.instr[instr];
	if (instrPtr == NULL)
		return;

	ft2_sample_t *smp = &instrPtr->smp[smpNum];
	const ft2_sample_format_t format = ft2_sample_get_format(smp);
	if (!ft2_sample_has_data(smp, format) || smp->length == 0)
		return;

	/* Clamp offset/length to sample bounds */
	int32_t startPos = offset;
	int32_t endPos = (length > 0) ? (offset + length) : -1;

	if (startPos < 0) startPos = 0;
	if (startPos > smp->length) startPos = smp->length;
	if (endPos > smp->length) endPos = smp->length;
	if (endPos > 0 && endPos < startPos) endPos = startPos;

	/* Set up channel state (matches standalone playSample/playRange) */
	ft2_channel_t *ch = &inst->replayer.channel[channel];
	ch->instrNum = instr;
	ch->instrPtr = instrPtr;
	ch->smpNum = smpNum;
	ch->smpPtr = smp;
	ch->noteNum = note;
	ch->relativeNote = smp->relativeNote;
	ch->finetune = smp->finetune;

	/* Set sample start/end positions */
	ch->smpStartPos = startPos;
	ch->smpEndPos = endPos;

	/* Reset key-off state and fadeout (critical for continuous playback) */
	ch->keyOff = false;
	ch->fadeoutVol = 32768;

	/* Set volume from sample (or override if specified) */
	uint8_t vol = (volume > 0) ? volume : smp->volume;
	ch->realVol = vol;
	ch->outVol = vol;
	ch->oldVol = vol;
	ch->oldPan = smp->panning;
	ch->outPan = smp->panning;

	/* Calculate period using lookup tables (matches standalone triggerNote) */
	int32_t relNote = note + smp->relativeNote;
	if (relNote < 1) relNote = 1;
	if (relNote >= 10 * 12) relNote = (10 * 12) - 1;

	const uint16_t noteIndex = ((relNote - 1) * 16) + (((int8_t)smp->finetune >> 3) + 16);
	const uint16_t *lut = inst->audio.linearPeriodsFlag ? linearPeriodLUT : amigaPeriodLUT;
	ch->outPeriod = ch->realPeriod = lut[noteIndex];

	/* Set status flags for voice trigger (voice triggered via ft2_update_voices) */
	ch->status = FT2_CS_TRIGGER_VOICE | FT2_CS_UPDATE_VOL | FT2_CS_UPDATE_PAN |
	             FT2_CF_UPDATE_PERIOD | FT2_CS_USE_QUICK_VOLRAMP;

	/* Calculate final volumes (matches standalone updateVolPanAutoVib call) */
	ft2_channel_update_vol_pan_autovib(inst, ch);
}

void ft2_instance_play(ft2_instance_t *inst, int8_t mode, int16_t startRow)
{
	if (inst == NULL)
		return;

	ft2_instance_stop(inst);

	ft2_song_t *s = &inst->replayer.song;

	/* Initialize position based on play mode (matches standalone setPos) */
	if (mode == FT2_PLAYMODE_PATT || mode == FT2_PLAYMODE_RECPATT)
	{
		/* Pattern play mode - keep current songPos, pattern already set */
		s->pattNum = s->orders[s->songPos & 0xFF];
	}
	else
	{
		/* Song play mode - use current editor position */
		if (s->songLength > 0 && s->songPos >= s->songLength)
			s->songPos = s->songLength - 1;
		s->pattNum = s->orders[s->songPos & 0xFF];
	}

	s->currNumRows = inst->replayer.patternNumRows[s->pattNum & 0xFF];
	s->tick = 1;
	s->row = startRow;
	if (s->row >= s->currNumRows)
		s->row = s->currNumRows - 1;
	s->pattDelTime = 0;
	s->pattDelTime2 = 0;

	/* Reset playback timer (matches standalone resetPlaybackTime) */
	s->playbackSeconds = 0;
	s->playbackSecondsFrac = 0;

	inst->replayer.playMode = mode;
	inst->replayer.songPlaying = true;

	ft2_instance_init_bpm_vars(inst);

	/* Reset tick counters to zero so first tick happens immediately */
	inst->audio.tickSampleCounter = 0;
	inst->audio.tickSampleCounterFrac = 0;

	/* Update UI (matches standalone startPlaying) */
	inst->uiState.updatePosSections = true;
	inst->uiState.updatePatternEditor = true;
}

void ft2_instance_play_pattern(ft2_instance_t *inst, uint8_t patternNum, int16_t startRow)
{
	if (inst == NULL)
		return;

	ft2_instance_stop(inst);

	ft2_song_t *s = &inst->replayer.song;

	/* Set pattern directly (not from orders array) */
	s->pattNum = patternNum;
	s->currNumRows = inst->replayer.patternNumRows[patternNum];
	s->tick = 1;
	s->row = startRow;
	if (s->row >= s->currNumRows)
		s->row = s->currNumRows - 1;
	s->pattDelTime = 0;
	s->pattDelTime2 = 0;

	/* Update editor to show the triggered pattern in UI */
	inst->editor.editPattern = patternNum;

	/* Reset playback timer */
	s->playbackSeconds = 0;
	s->playbackSecondsFrac = 0;

	inst->replayer.playMode = FT2_PLAYMODE_PATT;
	inst->replayer.songPlaying = true;

	ft2_instance_init_bpm_vars(inst);

	/* Reset tick counters so first tick happens immediately */
	inst->audio.tickSampleCounter = 0;
	inst->audio.tickSampleCounterFrac = 0;

	/* Update UI */
	inst->uiState.updatePosSections = true;
	inst->uiState.updatePatternEditor = true;
}

void ft2_instance_get_position(ft2_instance_t *inst, int16_t *outSongPos, int16_t *outRow)
{
	if (inst == NULL)
		return;

	if (outSongPos != NULL)
		*outSongPos = inst->replayer.song.songPos;
	if (outRow != NULL)
		*outRow = inst->replayer.song.row;
}

void ft2_instance_set_position(ft2_instance_t *inst, int16_t songPos, int16_t row)
{
	if (inst == NULL)
		return;

	ft2_song_t *s = &inst->replayer.song;

	if (songPos >= 0 && songPos < s->songLength)
	{
		s->songPos = songPos;
		/* Update pattern number from order list (matches standalone setPos) */
		s->pattNum = s->orders[s->songPos & 0xFF];
		s->currNumRows = inst->replayer.patternNumRows[s->pattNum & 0xFF];
	}

	if (row >= 0 && row < s->currNumRows)
		s->row = row;
}

void ft2_instance_render(ft2_instance_t *inst, float *outputL, float *outputR, uint32_t numSamples)
{
	if (inst == NULL || numSamples == 0)
		return;

	if (outputL == NULL && outputR == NULL)
		return;

	uint32_t samplesLeft = numSamples;
	uint32_t outPos = 0;

	while (samplesLeft > 0)
	{
		if (inst->audio.tickSampleCounter == 0)
		{
			inst->audio.tickSampleCounter = inst->audio.samplesPerTickInt;
			inst->audio.tickSampleCounterFrac += inst->audio.samplesPerTickFrac;
			if (inst->audio.tickSampleCounterFrac >= (1ULL << 32))
			{
				inst->audio.tickSampleCounterFrac &= 0xFFFFFFFF;
				inst->audio.tickSampleCounter++;
			}

			/* Reset volume ramps at start of tick (matches standalone behavior) */
			if (inst->audio.volumeRampingFlag)
				ft2_reset_ramp_volumes(inst);

			/* Run replayer tick and update voices */
			ft2_replayer_tick(inst);
			ft2_update_voices(inst);
		}

		uint32_t samplesToMix = samplesLeft;
		if (samplesToMix > inst->audio.tickSampleCounter)
			samplesToMix = inst->audio.tickSampleCounter;

		/* Clear mix buffers */
		memset(inst->audio.fMixBufferL, 0, samplesToMix * sizeof(float));
		memset(inst->audio.fMixBufferR, 0, samplesToMix * sizeof(float));

		/* Mix voices */
		ft2_mix_voices(inst, 0, samplesToMix);

		/* Copy to output with amplitude scaling (matches standalone outputAudio32) */
		const float mul = inst->fAudioNormalizeMul;
		for (uint32_t i = 0; i < samplesToMix; i++)
		{
		if (outputL != NULL)
			{
				float out = inst->audio.fMixBufferL[i] * mul;
				if (out < -1.0f) out = -1.0f;
				else if (out > 1.0f) out = 1.0f;
				outputL[outPos + i] = out;
			}
		if (outputR != NULL)
			{
				float out = inst->audio.fMixBufferR[i] * mul;
				if (out < -1.0f) out = -1.0f;
				else if (out > 1.0f) out = 1.0f;
				outputR[outPos + i] = out;
			}
		}

		outPos += samplesToMix;
		samplesLeft -= samplesToMix;
		inst->audio.tickSampleCounter -= samplesToMix;
	}
}

void ft2_mix_voices_only(ft2_instance_t *inst, float *outputL, float *outputR, uint32_t numSamples)
{
	if (inst == NULL || numSamples == 0)
		return;

	uint32_t samplesLeft = numSamples;
	uint32_t outPos = 0;

	while (samplesLeft > 0)
	{
		/* Process tick for envelope updates (same timing as pattern playback) */
		if (inst->audio.tickSampleCounter == 0)
		{
			inst->audio.tickSampleCounter = inst->audio.samplesPerTickInt;
			inst->audio.tickSampleCounterFrac += inst->audio.samplesPerTickFrac;
			if (inst->audio.tickSampleCounterFrac >= (1ULL << 32))
			{
				inst->audio.tickSampleCounterFrac &= 0xFFFFFFFF;
				inst->audio.tickSampleCounter++;
			}

			/* Reset volume ramps at start of tick */
			if (inst->audio.volumeRampingFlag)
				ft2_reset_ramp_volumes(inst);

			/* Process envelopes for all channels (replayer tick with songPlaying=false) */
			ft2_replayer_tick(inst);
			
			/* Sync channel state to voices */
			ft2_update_voices(inst);
		}

		uint32_t samplesToMix = samplesLeft;
		if (samplesToMix > inst->audio.tickSampleCounter)
			samplesToMix = inst->audio.tickSampleCounter;
		
		/* Limit chunk size to internal buffer size */
		uint32_t maxChunk = inst->audio.samplesPerTickIntTab[FT2_MAX_BPM - FT2_MIN_BPM];
		if (samplesToMix > maxChunk)
			samplesToMix = maxChunk;

		/* Clear mix buffers */
		memset(inst->audio.fMixBufferL, 0, samplesToMix * sizeof(float));
		memset(inst->audio.fMixBufferR, 0, samplesToMix * sizeof(float));

		/* Mix voices */
		ft2_mix_voices(inst, 0, samplesToMix);

		/* Copy to output with amplitude scaling (matches standalone outputAudio32) */
		const float mul = inst->fAudioNormalizeMul;
		for (uint32_t i = 0; i < samplesToMix; i++)
		{
		if (outputL != NULL)
			{
				float out = inst->audio.fMixBufferL[i] * mul;
				if (out < -1.0f) out = -1.0f;
				else if (out > 1.0f) out = 1.0f;
				outputL[outPos + i] = out;
			}
		if (outputR != NULL)
			{
				float out = inst->audio.fMixBufferR[i] * mul;
				if (out < -1.0f) out = -1.0f;
				else if (out > 1.0f) out = 1.0f;
				outputR[outPos + i] = out;
			}
		}

		outPos += samplesToMix;
		samplesLeft -= samplesToMix;
		inst->audio.tickSampleCounter -= samplesToMix;
	}
}

bool ft2_instance_set_multiout(ft2_instance_t *inst, bool enabled, uint32_t bufferSize)
{
	if (inst == NULL)
		return false;

	if (!enabled)
	{
		/* Free existing buffers */
		for (int i = 0; i < FT2_MAX_CHANNELS; i++)
		{
			if (inst->audio.fChannelBufferL[i] != NULL)
			{
				free(inst->audio.fChannelBufferL[i]);
				inst->audio.fChannelBufferL[i] = NULL;
			}
			if (inst->audio.fChannelBufferR[i] != NULL)
			{
				free(inst->audio.fChannelBufferR[i]);
				inst->audio.fChannelBufferR[i] = NULL;
			}
		}
		inst->audio.multiOutEnabled = false;
		inst->audio.multiOutBufferSize = 0;
		return true;
	}

	/* Check if we need to reallocate */
	if (inst->audio.multiOutEnabled && inst->audio.multiOutBufferSize >= bufferSize)
		return true; /* Already have sufficient buffers */

	/* Free old buffers if size changed */
	if (inst->audio.multiOutEnabled)
		ft2_instance_set_multiout(inst, false, 0);

	/* Allocate new buffers */
	for (int i = 0; i < FT2_MAX_CHANNELS; i++)
	{
		inst->audio.fChannelBufferL[i] = (float *)calloc(bufferSize, sizeof(float));
		inst->audio.fChannelBufferR[i] = (float *)calloc(bufferSize, sizeof(float));

		if (inst->audio.fChannelBufferL[i] == NULL || inst->audio.fChannelBufferR[i] == NULL)
		{
			/* Allocation failed, free everything */
			ft2_instance_set_multiout(inst, false, 0);
			return false;
		}
	}

	inst->audio.multiOutEnabled = true;
	inst->audio.multiOutBufferSize = bufferSize;
	return true;
}

void ft2_instance_render_multiout(ft2_instance_t *inst, float *mainOutL, float *mainOutR, uint32_t numSamples)
{
	if (inst == NULL || numSamples == 0)
		return;

	if (!inst->audio.multiOutEnabled || numSamples > inst->audio.multiOutBufferSize)
	{
		/* Fall back to regular render */
		ft2_instance_render(inst, mainOutL, mainOutR, numSamples);
		return;
	}

	uint32_t samplesLeft = numSamples;
	uint32_t outPos = 0;

	/* Clear only the 16 output buffers (routing maps 32 channels -> 16 outputs) */
	for (int out = 0; out < FT2_NUM_OUTPUTS; out++)
	{
		memset(inst->audio.fChannelBufferL[out], 0, numSamples * sizeof(float));
		memset(inst->audio.fChannelBufferR[out], 0, numSamples * sizeof(float));
	}

	while (samplesLeft > 0)
	{
		if (inst->audio.tickSampleCounter == 0)
		{
			inst->audio.tickSampleCounter = inst->audio.samplesPerTickInt;
			inst->audio.tickSampleCounterFrac += inst->audio.samplesPerTickFrac;
			if (inst->audio.tickSampleCounterFrac >= (1ULL << 32))
			{
				inst->audio.tickSampleCounterFrac &= 0xFFFFFFFF;
				inst->audio.tickSampleCounter++;
			}

			if (inst->audio.volumeRampingFlag)
				ft2_reset_ramp_volumes(inst);

			ft2_replayer_tick(inst);
			ft2_update_voices(inst);
		}

		uint32_t samplesToMix = samplesLeft;
		if (samplesToMix > inst->audio.tickSampleCounter)
			samplesToMix = inst->audio.tickSampleCounter;

		uint32_t maxChunk = inst->audio.samplesPerTickIntTab[FT2_MAX_BPM - FT2_MIN_BPM];
		if (samplesToMix > maxChunk)
			samplesToMix = maxChunk;

		/* Mix each voice to its channel's buffer */
		ft2_mix_voices_multiout(inst, outPos, samplesToMix);

		outPos += samplesToMix;
		samplesLeft -= samplesToMix;
		inst->audio.tickSampleCounter -= samplesToMix;
	}

	/* Sum output buffers into main output, respecting channelToMain routing */
	const float mul = inst->fAudioNormalizeMul;

	/* Track which output buses should be included in main mix */
	bool outputToMain[FT2_NUM_OUTPUTS] = {false};
	for (int32_t ch = 0; ch < inst->replayer.song.numChannels && ch < FT2_MAX_CHANNELS; ch++)
	{
		if (inst->config.channelToMain[ch])
		{
			int outIdx = inst->config.channelRouting[ch];
			if (outIdx >= FT2_NUM_OUTPUTS)
				outIdx = ch % FT2_NUM_OUTPUTS;
			outputToMain[outIdx] = true;
		}
	}

	/* Sum selected output buffers to main with amplitude scaling */
	for (uint32_t i = 0; i < numSamples; i++)
	{
		float sumL = 0.0f, sumR = 0.0f;
		for (int out = 0; out < FT2_NUM_OUTPUTS; out++)
		{
			if (outputToMain[out])
			{
				sumL += inst->audio.fChannelBufferL[out][i];
				sumR += inst->audio.fChannelBufferR[out][i];
			}
		}

		if (mainOutL != NULL)
		{
			float out = sumL * mul;
			if (out < -1.0f) out = -1.0f;
			else if (out > 1.0f) out = 1.0f;
			mainOutL[i] = out;
		}
		if (mainOutR != NULL)
		{
			float out = sumR * mul;
			if (out < -1.0f) out = -1.0f;
			else if (out > 1.0f) out = 1.0f;
			mainOutR[i] = out;
		}
	}

	/* Apply amplitude scaling to the 16 output buffers */
	for (int out = 0; out < FT2_NUM_OUTPUTS; out++)
	{
		for (uint32_t i = 0; i < numSamples; i++)
		{
			float outL = inst->audio.fChannelBufferL[out][i] * mul;
			float outR = inst->audio.fChannelBufferR[out][i] * mul;
			if (outL < -1.0f) outL = -1.0f; else if (outL > 1.0f) outL = 1.0f;
			if (outR < -1.0f) outR = -1.0f; else if (outR > 1.0f) outR = 1.0f;
			inst->audio.fChannelBufferL[out][i] = outL;
			inst->audio.fChannelBufferR[out][i] = outR;
		}
	}
}

bool ft2_instance_load_xm(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize)
{
	/* Use the new generic loader that supports XM, MOD, and S3M */
	return ft2_load_module(inst, data, dataSize);
}

void ft2_instance_set_interpolation(ft2_instance_t *inst, uint8_t type)
{
	if (inst == NULL)
		return;
	
	/* 0 = no interpolation (point), 1 = linear */
	inst->audio.interpolationType = (type > 1) ? 1 : type;
}

/* Helper for modulo that works correctly on negative numbers */
static int32_t myMod(int32_t a, int32_t b)
{
	int32_t c = a % b;
	return (c < 0) ? (c + b) : c;
}

void ft2_song_mark_modified(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	inst->replayer.song.isModified = true;
	ft2_timemap_invalidate(inst);
}

/**
 * @brief Validates and clamps instrument parameters.
 * @param ins The instrument to sanitize.
 */
void ft2_sanitize_instrument(ft2_instr_t *ins)
{
	if (ins == NULL)
		return;

	if (ins->midiProgram < 0)
		ins->midiProgram = 0;
	else if (ins->midiProgram > 127)
		ins->midiProgram = 127;

	if (ins->midiBend < 0)
		ins->midiBend = 0;
	else if (ins->midiBend > 36)
		ins->midiBend = 36;

	if (ins->midiChannel > 15)
		ins->midiChannel = 15;
	if (ins->autoVibDepth > 0x0F)
		ins->autoVibDepth = 0x0F;
	if (ins->autoVibRate > 0x3F)
		ins->autoVibRate = 0x3F;
	if (ins->autoVibType > 3)
		ins->autoVibType = 0;

	for (int32_t i = 0; i < 96; i++)
	{
		if (ins->note2SampleLUT[i] >= FT2_MAX_SMP_PER_INST)
			ins->note2SampleLUT[i] = FT2_MAX_SMP_PER_INST - 1;
	}

	if (ins->volEnvLength > 12) ins->volEnvLength = 12;
	if (ins->volEnvLoopStart > 11) ins->volEnvLoopStart = 11;
	if (ins->volEnvLoopEnd > 11) ins->volEnvLoopEnd = 11;
	if (ins->volEnvSustain > 11) ins->volEnvSustain = 11;
	if (ins->panEnvLength > 12) ins->panEnvLength = 12;
	if (ins->panEnvLoopStart > 11) ins->panEnvLoopStart = 11;
	if (ins->panEnvLoopEnd > 11) ins->panEnvLoopEnd = 11;
	if (ins->panEnvSustain > 11) ins->panEnvSustain = 11;
	if (ins->volEnvSpeed < 1) ins->volEnvSpeed = 1;
	else if (ins->volEnvSpeed > 16) ins->volEnvSpeed = 16;
	if (ins->panEnvSpeed < 1) ins->panEnvSpeed = 1;
	else if (ins->panEnvSpeed > 16) ins->panEnvSpeed = 16;

	for (int32_t i = 0; i < 12; i++)
	{
		if ((uint16_t)ins->volEnvPoints[i][0] > 32767) ins->volEnvPoints[i][0] = 32767;
		if ((uint16_t)ins->panEnvPoints[i][0] > 32767) ins->panEnvPoints[i][0] = 32767;
		if ((uint16_t)ins->volEnvPoints[i][1] > 64) ins->volEnvPoints[i][1] = 64;
		if ((uint16_t)ins->panEnvPoints[i][1] > 63) ins->panEnvPoints[i][1] = 63;

		if (ins->volEnvCurve[i] > FT2_ENV_CURVE_MAX) ins->volEnvCurve[i] = FT2_ENV_CURVE_MAX;
		else if (ins->volEnvCurve[i] < -FT2_ENV_CURVE_MAX) ins->volEnvCurve[i] = -FT2_ENV_CURVE_MAX;
		if (ins->panEnvCurve[i] > FT2_ENV_CURVE_MAX) ins->panEnvCurve[i] = FT2_ENV_CURVE_MAX;
		else if (ins->panEnvCurve[i] < -FT2_ENV_CURVE_MAX) ins->panEnvCurve[i] = -FT2_ENV_CURVE_MAX;
	}
	ins->volEnvCurve[0] = 0;
	ins->panEnvCurve[0] = 0;
}

/**
 * @brief Validates and clamps sample parameters.
 * @param s The sample to sanitize.
 */
void ft2_sanitize_sample(ft2_sample_t *s)
{
	if (s == NULL)
		return;

	/* If sample has both forward and pingpong loop set, it means pingpong (FT2 mixer behavior) */
	uint8_t loopType = s->flags & (FT2_LOOP_FWD | FT2_LOOP_BIDI);
	if (loopType == (FT2_LOOP_FWD | FT2_LOOP_BIDI))
		s->flags &= ~FT2_LOOP_FWD;

	if (s->volume > 64)
		s->volume = 64;

	if (s->relativeNote < -48)
		s->relativeNote = -48;
	else if (s->relativeNote > 71)
		s->relativeNote = 71;

	if (s->length < 0)
		s->length = 0;

	if (s->loopStart < 0 || s->loopLength <= 0 || s->loopStart + s->loopLength > s->length)
	{
		s->loopStart = 0;
		s->loopLength = 0;
		s->flags &= ~(FT2_LOOP_FWD | FT2_LOOP_BIDI);
	}
}

/**
 * @brief Internal helper to fix a single channel of sample data for mixer interpolation.
 */
static void fixSampleChannel(int8_t *dataPtr, int8_t *leftEdgeTaps8, int16_t *leftEdgeTaps16,
                              int16_t *fixedSmp, bool sample16Bit, uint8_t loopType,
                              int32_t length, int32_t loopStart, int32_t loopLength, int32_t loopEnd)
{
	int32_t pos;
	bool backwards;
	int16_t *ptr16 = (int16_t *)dataPtr;

	/* All negative taps should be equal to the first sample point when at sampling
	** position #0 (on sample trigger), until an eventual loop cycle, where we do
	** a special left edge case with replaced tap data.
	** The sample pointer is offset and has allocated data before it, so this is safe.
	*/
	if (sample16Bit)
	{
		for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
			ptr16[i - FT2_MAX_LEFT_TAPS] = ptr16[0];
	}
	else
	{
		for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
			dataPtr[i - FT2_MAX_LEFT_TAPS] = dataPtr[0];
	}

	if (loopType == FT2_LOOP_OFF) /* No loop */
	{
		if (sample16Bit)
		{
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				ptr16[length + i] = ptr16[length - 1];
		}
		else
		{
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				dataPtr[length + i] = dataPtr[length - 1];
		}
		return;
	}

	if (loopType == FT2_LOOP_FWD) /* Forward loop */
	{
		if (sample16Bit)
		{
			/* Left edge (we need MAX_TAPS amount of taps starting from the center tap) */
			for (int32_t i = -FT2_MAX_LEFT_TAPS; i < FT2_MAX_TAPS; i++)
			{
				pos = loopStart + myMod(i, loopLength);
				leftEdgeTaps16[FT2_MAX_LEFT_TAPS + i] = ptr16[pos];
			}

			/* Right edge (change actual sample data since data after loop is never used) */
			pos = loopStart;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			{
				fixedSmp[i] = ptr16[loopEnd + i];
				ptr16[loopEnd + i] = ptr16[pos];

				if (++pos >= loopEnd)
					pos -= loopLength;
			}
		}
		else /* 8-bit */
		{
			/* Left edge */
			for (int32_t i = -FT2_MAX_LEFT_TAPS; i < FT2_MAX_TAPS; i++)
			{
				pos = loopStart + myMod(i, loopLength);
				leftEdgeTaps8[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];
			}

			/* Right edge */
			pos = loopStart;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			{
				fixedSmp[i] = dataPtr[loopEnd + i];
				dataPtr[loopEnd + i] = dataPtr[pos];

				if (++pos >= loopEnd)
					pos -= loopLength;
			}
		}
	}
	else /* Pingpong loop */
	{
		if (sample16Bit)
		{
			/* Left edge (positive taps) */
			pos = loopStart;
			backwards = false;
			for (int32_t i = 0; i < FT2_MAX_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd)
				{
					pos = loopEnd - 1;
					backwards = true;
				}

				leftEdgeTaps16[FT2_MAX_LEFT_TAPS + i] = ptr16[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}

			/* Left edge (negative taps) */
			for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
				leftEdgeTaps16[(FT2_MAX_LEFT_TAPS - 1) - i] = leftEdgeTaps16[FT2_MAX_LEFT_TAPS + 1 + i];

			/* Right edge */
			pos = loopEnd - 1;
			backwards = true;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd)
				{
					pos = loopEnd - 1;
					backwards = true;
				}

				fixedSmp[i] = ptr16[loopEnd + i];
				ptr16[loopEnd + i] = ptr16[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}
		}
		else /* 8-bit */
		{
			/* Left edge (positive taps) */
			pos = loopStart;
			backwards = false;
			for (int32_t i = 0; i < FT2_MAX_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd)
				{
					pos = loopEnd - 1;
					backwards = true;
				}

				leftEdgeTaps8[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}

			/* Left edge (negative taps) */
			for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
				leftEdgeTaps8[(FT2_MAX_LEFT_TAPS - 1) - i] = leftEdgeTaps8[FT2_MAX_LEFT_TAPS + 1 + i];

			/* Right edge */
			pos = loopEnd - 1;
			backwards = true;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd)
				{
					pos = loopEnd - 1;
					backwards = true;
				}

				fixedSmp[i] = dataPtr[loopEnd + i];
				dataPtr[loopEnd + i] = dataPtr[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}
		}
	}
}

static void fixSampleChannel24(int32_t *dataPtr, int32_t *leftEdgeTaps24, int32_t *fixedSmp24,
                               uint8_t loopType, int32_t length, int32_t loopStart, int32_t loopLength, int32_t loopEnd)
{
	int32_t pos;
	bool backwards;

	for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
		dataPtr[i - FT2_MAX_LEFT_TAPS] = dataPtr[0];

	if (loopType == FT2_LOOP_OFF) /* No loop */
	{
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			dataPtr[length + i] = dataPtr[length - 1];
		return;
	}

	if (loopType == FT2_LOOP_FWD) /* Forward loop */
	{
		/* Left edge */
		for (int32_t i = -FT2_MAX_LEFT_TAPS; i < FT2_MAX_TAPS; i++)
		{
			pos = loopStart + myMod(i, loopLength);
			leftEdgeTaps24[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];
		}

		/* Right edge */
		pos = loopStart;
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
		{
			fixedSmp24[i] = dataPtr[loopEnd + i];
			dataPtr[loopEnd + i] = dataPtr[pos];

			if (++pos >= loopEnd)
				pos -= loopLength;
		}
	}
	else /* Pingpong loop */
	{
		/* Left edge (positive taps) */
		pos = loopStart;
		backwards = false;
		for (int32_t i = 0; i < FT2_MAX_TAPS; i++)
		{
			if (backwards)
			{
				if (pos < loopStart)
				{
					pos = loopStart;
					backwards = false;
				}
			}
			else if (pos >= loopEnd)
			{
				pos = loopEnd - 1;
				backwards = true;
			}

			leftEdgeTaps24[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];

			if (backwards)
				pos--;
			else
				pos++;
		}

		/* Left edge (negative taps) */
		for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
			leftEdgeTaps24[(FT2_MAX_LEFT_TAPS - 1) - i] = leftEdgeTaps24[FT2_MAX_LEFT_TAPS + 1 + i];

		/* Right edge */
		pos = loopEnd - 1;
		backwards = true;
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
		{
			if (backwards)
			{
				if (pos < loopStart)
				{
					pos = loopStart;
					backwards = false;
				}
			}
			else if (pos >= loopEnd)
			{
				pos = loopEnd - 1;
				backwards = true;
			}

			fixedSmp24[i] = dataPtr[loopEnd + i];
			dataPtr[loopEnd + i] = dataPtr[pos];

			if (backwards)
				pos--;
			else
				pos++;
		}
	}
}

static void fixSampleChannelFloat(float *dataPtr, float *leftEdgeTapsF, float *fixedSmpF,
                                  uint8_t loopType, int32_t length, int32_t loopStart, int32_t loopLength, int32_t loopEnd)
{
	int32_t pos;
	bool backwards;

	for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
		dataPtr[i - FT2_MAX_LEFT_TAPS] = dataPtr[0];

	if (loopType == FT2_LOOP_OFF) /* No loop */
	{
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
			dataPtr[length + i] = dataPtr[length - 1];
		return;
	}

	if (loopType == FT2_LOOP_FWD) /* Forward loop */
	{
		/* Left edge */
		for (int32_t i = -FT2_MAX_LEFT_TAPS; i < FT2_MAX_TAPS; i++)
		{
			pos = loopStart + myMod(i, loopLength);
			leftEdgeTapsF[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];
		}

		/* Right edge */
		pos = loopStart;
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
		{
			fixedSmpF[i] = dataPtr[loopEnd + i];
			dataPtr[loopEnd + i] = dataPtr[pos];

			if (++pos >= loopEnd)
				pos -= loopLength;
		}
	}
	else /* Pingpong loop */
	{
		/* Left edge (positive taps) */
		pos = loopStart;
		backwards = false;
		for (int32_t i = 0; i < FT2_MAX_TAPS; i++)
		{
			if (backwards)
			{
				if (pos < loopStart)
				{
					pos = loopStart;
					backwards = false;
				}
			}
			else if (pos >= loopEnd)
			{
				pos = loopEnd - 1;
				backwards = true;
			}

			leftEdgeTapsF[FT2_MAX_LEFT_TAPS + i] = dataPtr[pos];

			if (backwards)
				pos--;
			else
				pos++;
		}

		/* Left edge (negative taps) */
		for (int32_t i = 0; i < FT2_MAX_LEFT_TAPS; i++)
			leftEdgeTapsF[(FT2_MAX_LEFT_TAPS - 1) - i] = leftEdgeTapsF[FT2_MAX_LEFT_TAPS + 1 + i];

		/* Right edge */
		pos = loopEnd - 1;
		backwards = true;
		for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
		{
			if (backwards)
			{
				if (pos < loopStart)
				{
					pos = loopStart;
					backwards = false;
				}
			}
			else if (pos >= loopEnd)
			{
				pos = loopEnd - 1;
				backwards = true;
			}

			fixedSmpF[i] = dataPtr[loopEnd + i];
			dataPtr[loopEnd + i] = dataPtr[pos];

			if (backwards)
				pos--;
			else
				pos++;
		}
	}
}

/**
 * @brief Prepares sample for branchless mixer interpolation.
 *
 * Modifies samples before index 0, and after loop/end for interpolation.
 * This must be called after loading or modifying sample data.
 * Handles both mono and stereo samples.
 *
 * @param s The sample to fix.
 */
void ft2_fix_sample(ft2_sample_t *s)
{
	if (s == NULL || s->length <= 0)
	{
		if (s != NULL)
		{
			s->isFixed = false;
			s->fixedPos = 0;
		}
		return;
	}

	ft2_sample_format_t format = s->format;
	if (format != FT2_SMP_FORMAT_24BIT && format != FT2_SMP_FORMAT_FLOAT32)
		format = (s->flags & FT2_SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;
	const bool isStereo = !!(s->flags & FT2_SAMPLE_STEREO);

	bool hasData = false;
	if (format == FT2_SMP_FORMAT_24BIT)
		hasData = (s->dataPtr24 != NULL);
	else if (format == FT2_SMP_FORMAT_FLOAT32)
		hasData = (s->dataPtrF != NULL);
	else
		hasData = (s->dataPtr != NULL);

	if (!hasData)
	{
		s->isFixed = false;
		s->fixedPos = 0;
		return;
	}
	uint8_t loopType = s->flags & (FT2_LOOP_FWD | FT2_LOOP_BIDI);
	int32_t length = s->length;
	int32_t loopStart = s->loopStart;
	int32_t loopLength = s->loopLength;
	int32_t loopEnd = s->loopStart + s->loopLength;

	/* Treat loop as disabled if loopLen == 0 (FT2 does this) */
	if (loopType != 0 && loopLength <= 0)
	{
		loopType = 0;
		loopStart = loopLength = loopEnd = 0;
	}

	/* Fix left channel (or mono) */
	switch (format)
	{
		default:
		case FT2_SMP_FORMAT_8BIT:
		case FT2_SMP_FORMAT_16BIT:
			fixSampleChannel(s->dataPtr, s->leftEdgeTapSamples8, s->leftEdgeTapSamples16,
			                 s->fixedSmp, (format == FT2_SMP_FORMAT_16BIT),
			                 loopType, length, loopStart, loopLength, loopEnd);
			break;
		case FT2_SMP_FORMAT_24BIT:
			fixSampleChannel24(s->dataPtr24, s->leftEdgeTapSamples24, s->fixedSmp24,
			                   loopType, length, loopStart, loopLength, loopEnd);
			break;
		case FT2_SMP_FORMAT_FLOAT32:
			fixSampleChannelFloat(s->dataPtrF, s->leftEdgeTapSamplesF, s->fixedSmpF,
			                      loopType, length, loopStart, loopLength, loopEnd);
			break;
	}

	/* Fix right channel for stereo */
	bool hasStereoData = false;
	if (isStereo)
	{
		if (format == FT2_SMP_FORMAT_24BIT)
			hasStereoData = (s->dataPtr24R != NULL);
		else if (format == FT2_SMP_FORMAT_FLOAT32)
			hasStereoData = (s->dataPtrFR != NULL);
		else
			hasStereoData = (s->dataPtrR != NULL);
	}

	if (hasStereoData)
	{
		switch (format)
		{
			default:
			case FT2_SMP_FORMAT_8BIT:
			case FT2_SMP_FORMAT_16BIT:
				fixSampleChannel(s->dataPtrR, s->leftEdgeTapSamples8R, s->leftEdgeTapSamples16R,
				                 s->fixedSmpR, (format == FT2_SMP_FORMAT_16BIT),
				                 loopType, length, loopStart, loopLength, loopEnd);
				break;
			case FT2_SMP_FORMAT_24BIT:
				fixSampleChannel24(s->dataPtr24R, s->leftEdgeTapSamples24R, s->fixedSmp24R,
				                   loopType, length, loopStart, loopLength, loopEnd);
				break;
			case FT2_SMP_FORMAT_FLOAT32:
				fixSampleChannelFloat(s->dataPtrFR, s->leftEdgeTapSamplesFR, s->fixedSmpFR,
				                      loopType, length, loopStart, loopLength, loopEnd);
				break;
		}
	}

	if (loopType == FT2_LOOP_OFF)
	{
		s->fixedPos = 0;
		s->isFixed = false;
	}
	else
	{
		s->fixedPos = loopEnd;
		s->isFixed = true;
	}
}

/**
 * @brief Restores sample data that was modified by ft2_fix_sample().
 *
 * Call this before modifying sample data to restore original values.
 * Handles both mono and stereo samples.
 *
 * @param s The sample to unfix.
 */
void ft2_unfix_sample(ft2_sample_t *s)
{
	if (s == NULL || !s->isFixed)
		return;

	const bool isStereo = !!(s->flags & FT2_SAMPLE_STEREO);
	ft2_sample_format_t format = s->format;
	if (format != FT2_SMP_FORMAT_24BIT && format != FT2_SMP_FORMAT_FLOAT32)
		format = (s->flags & FT2_SAMPLE_16BIT) ? FT2_SMP_FORMAT_16BIT : FT2_SMP_FORMAT_8BIT;

	bool hasData = false;
	if (format == FT2_SMP_FORMAT_24BIT)
		hasData = (s->dataPtr24 != NULL);
	else if (format == FT2_SMP_FORMAT_FLOAT32)
		hasData = (s->dataPtrF != NULL);
	else
		hasData = (s->dataPtr != NULL);
	if (!hasData)
		return;

	switch (format)
	{
		default:
		case FT2_SMP_FORMAT_8BIT:
		{
			int8_t *ptr8 = s->dataPtr + s->fixedPos;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				ptr8[i] = (int8_t)s->fixedSmp[i];

			if (isStereo && s->dataPtrR != NULL)
			{
				int8_t *ptr8R = s->dataPtrR + s->fixedPos;
				for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
					ptr8R[i] = (int8_t)s->fixedSmpR[i];
			}
		}
		break;

		case FT2_SMP_FORMAT_16BIT:
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + s->fixedPos;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				ptr16[i] = s->fixedSmp[i];

			if (isStereo && s->dataPtrR != NULL)
			{
				int16_t *ptr16R = (int16_t *)s->dataPtrR + s->fixedPos;
				for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
					ptr16R[i] = s->fixedSmpR[i];
			}
		}
		break;

		case FT2_SMP_FORMAT_24BIT:
		{
			int32_t *ptr24 = s->dataPtr24 + s->fixedPos;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				ptr24[i] = s->fixedSmp24[i];

			if (isStereo && s->dataPtr24R != NULL)
			{
				int32_t *ptr24R = s->dataPtr24R + s->fixedPos;
				for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
					ptr24R[i] = s->fixedSmp24R[i];
			}
		}
		break;

		case FT2_SMP_FORMAT_FLOAT32:
		{
			float *ptrF = s->dataPtrF + s->fixedPos;
			for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
				ptrF[i] = s->fixedSmpF[i];

			if (isStereo && s->dataPtrFR != NULL)
			{
				float *ptrFR = s->dataPtrFR + s->fixedPos;
				for (int32_t i = 0; i < FT2_MAX_RIGHT_TAPS; i++)
					ptrFR[i] = s->fixedSmpFR[i];
			}
		}
		break;
	}

	s->isFixed = false;
}
