/**
 * @file ft2_plugin_replayer.c
 * @brief Instance-based replayer core for plugin architecture.
 *
 * This is a complete port of ft2_replayer.c that operates on
 * ft2_instance_t rather than global state.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_interpolation.h"
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_config.h"
#include "../ft2_instance.h"

/* Period lookup tables from ft2_tables_plugin.c */
extern const uint16_t linearPeriodLUT[1936];
extern const uint16_t amigaPeriodLUT[1936];
extern const uint8_t vibratoTab[32];
extern const int8_t autoVibSineTab[256];
extern const uint8_t arpeggioTab[32];
extern const uint32_t songTickDuration35fp[(255-32)+1];

/* -------------------------------------------------------------------------
 * Helper macros
 * ------------------------------------------------------------------------- */

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* -------------------------------------------------------------------------
 * Forward declarations for internal functions
 * ------------------------------------------------------------------------- */

static void doVibrato(ft2_instance_t *inst, ft2_channel_t *ch);
static void portamento(ft2_instance_t *inst, ft2_channel_t *ch);
static void volSlide(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param);
static uint16_t period2NotePeriod(ft2_instance_t *inst, uint16_t period, uint8_t noteOffset, ft2_channel_t *ch);

/* -------------------------------------------------------------------------
 * Voice management
 * ------------------------------------------------------------------------- */

void ft2_stop_voice(ft2_instance_t *inst, int32_t voiceNum)
{
	if (inst == NULL || voiceNum < 0 || voiceNum >= FT2_MAX_CHANNELS)
		return;

	/* Clear main voice */
	ft2_voice_t *v = &inst->voice[voiceNum];
	memset(v, 0, sizeof(ft2_voice_t));
	v->panning = 128;

	/* Clear fadeout voice too (matches standalone behavior) */
	v = &inst->voice[FT2_MAX_CHANNELS + voiceNum];
	memset(v, 0, sizeof(ft2_voice_t));
	v->panning = 128;
}

void ft2_stop_all_voices(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Stop all voices (main + fadeout voices) */
	for (int32_t i = 0; i < FT2_MAX_CHANNELS * 2; i++)
	{
		ft2_voice_t *v = &inst->voice[i];
		memset(v, 0, sizeof(ft2_voice_t));
		v->panning = 128;
	}

	/* Request scope clear (matches standalone stopVoices -> stopAllScopes) */
	inst->scopesClearRequested = true;
}

void ft2_fadeout_all_voices(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	for (int32_t i = 0; i < FT2_MAX_CHANNELS; i++)
	{
		ft2_voice_t *v = &inst->voice[i];
		if (!v->active || (v->fCurrVolumeL == 0.0f && v->fCurrVolumeR == 0.0f))
			continue;

		/* Copy to fadeout slot */
		ft2_voice_t *f = &inst->voice[FT2_MAX_CHANNELS + i];
		*f = *v;

		/* Setup volume ramp to zero */
		f->volumeRampLength = inst->audio.quickVolRampSamples;
		f->fVolumeLDelta = -f->fCurrVolumeL * inst->audio.fQuickVolRampSamplesMul;
		f->fVolumeRDelta = -f->fCurrVolumeR * inst->audio.fQuickVolRampSamplesMul;
		f->fTargetVolumeL = 0.0f;
		f->fTargetVolumeR = 0.0f;
		f->isFadeOutVoice = true;

		/* Clear main voice */
		memset(v, 0, sizeof(ft2_voice_t));
		v->panning = 128;
	}

	/* Request scope clear */
	inst->scopesClearRequested = true;
}

void ft2_stop_sample_voices(ft2_instance_t *inst, ft2_sample_t *smp)
{
	if (inst == NULL || smp == NULL)
		return;

	/* Stop only voices that are playing the specified sample */
	for (int32_t i = 0; i < FT2_MAX_CHANNELS * 2; i++)
	{
		ft2_voice_t *v = &inst->voice[i];
		if (!v->active)
			continue;

		bool isSameSample = false;
		if (v->base8 != NULL && v->base8 == (const int8_t *)smp->dataPtr)
			isSameSample = true;
		else if (v->base16 != NULL && v->base16 == (const int16_t *)smp->dataPtr)
			isSameSample = true;

		if (isSameSample)
		{
			memset(v, 0, sizeof(ft2_voice_t));
			v->panning = 128;
		}
	}
}

uint64_t ft2_period_to_delta(ft2_instance_t *inst, uint32_t period)
{
	period &= 0xFFFF;

	if (inst == NULL || period == 0)
		return 0;

	if (inst->audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF;
		const uint32_t quotient  = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);
		return inst->replayer.logTab[remainder] >> ((14 - quotient) & 31);
	}
	else
	{
		return inst->replayer.amigaPeriodDiv / period;
	}
}

void ft2_trigger_voice(ft2_instance_t *inst, int32_t voiceNum, ft2_sample_t *smp, int32_t startPos)
{
	if (inst == NULL || smp == NULL || voiceNum < 0 || voiceNum >= FT2_MAX_CHANNELS)
		return;

	ft2_voice_t *v = &inst->voice[voiceNum];

	/* Note: fadeout voice is set up in ft2_voice_update_volumes(), not here.
	** This matches the standalone behavior where voiceUpdateVolumes() handles
	** fadeout and voiceTrigger() only sets sample-related fields. */

	const int32_t length = smp->length;
	const int32_t loopStart = smp->loopStart;
	const int32_t loopLength = smp->loopLength;
	const int32_t loopEnd = loopStart + loopLength;
	const bool sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);
	uint8_t loopType = smp->flags & (FT2_LOOP_FWD | FT2_LOOP_BIDI);

	if (smp->dataPtr == NULL || length < 1)
	{
		v->active = false; /* shut down voice (illegal parameters) */
		return;
	}

	if (loopLength < 1) /* disable loop if loopLength is below 1 */
		loopType = 0;

	if (sample16Bit)
	{
		v->base16 = (const int16_t *)smp->dataPtr;
		v->base8 = NULL; /* Clear unused pointer for correct mixer type detection */
		v->revBase16 = &v->base16[loopStart + loopEnd]; /* for pingpong loops */
		v->leftEdgeTaps16 = smp->leftEdgeTapSamples16 + FT2_MAX_LEFT_TAPS;
	}
	else
	{
		v->base8 = smp->dataPtr;
		v->base16 = NULL; /* Clear unused pointer for correct mixer type detection */
		v->revBase8 = &v->base8[loopStart + loopEnd]; /* for pingpong loops */
		v->leftEdgeTaps8 = smp->leftEdgeTapSamples8 + FT2_MAX_LEFT_TAPS;
	}

	v->hasLooped = false; /* for cubic/sinc interpolation special case */
	v->samplingBackwards = false;
	v->loopType = loopType;
	v->sampleEnd = (loopType == 0) ? length : loopEnd;
	v->loopStart = loopStart;
	v->loopLength = loopLength;
	v->position = startPos;
	v->positionFrac = 0;

	/* if position overflows, shut down voice (f.ex. through 9xx command) */
	if (v->position >= v->sampleEnd)
	{
		v->active = false;
		return;
	}

	/* Set mix function offset based on sample type and loop type */
	int32_t mixFuncOffset = ((int32_t)sample16Bit * 3) + loopType;
	mixFuncOffset += inst->audio.interpolationType * 6;
	v->mixFuncOffset = (uint8_t)mixFuncOffset;

	v->active = true;
}

void ft2_voice_update_volumes(ft2_instance_t *inst, int32_t voiceNum, uint8_t status)
{
	if (inst == NULL || voiceNum < 0 || voiceNum >= FT2_MAX_CHANNELS)
		return;

	ft2_voice_t *v = &inst->voice[voiceNum];

	v->fTargetVolumeL = v->fVolume * inst->fSqrtPanningTable[256 - v->panning];
	v->fTargetVolumeR = v->fVolume * inst->fSqrtPanningTable[v->panning];

	if (!inst->audio.volumeRampingFlag)
	{
		/* Volume ramping is disabled, set volume directly */
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
		return;
	}

	/* Now we need to handle volume ramping */

	const bool voiceTriggerFlag = !!(status & FT2_CS_TRIGGER_VOICE);
	if (voiceTriggerFlag)
	{
		/* Voice is about to start, ramp out/in at the same time */

		if (v->fCurrVolumeL > 0.0f || v->fCurrVolumeR > 0.0f)
		{
			/* Setup fadeout voice */
			ft2_voice_t *f = &inst->voice[FT2_MAX_CHANNELS + voiceNum];

			*f = *v; /* Copy current voice to new fadeout-ramp voice */

			const float fVolumeLDiff = 0.0f - f->fCurrVolumeL;
			const float fVolumeRDiff = 0.0f - f->fCurrVolumeR;

			f->volumeRampLength = inst->audio.quickVolRampSamples; /* 5ms */
			f->fVolumeLDelta = fVolumeLDiff * inst->audio.fQuickVolRampSamplesMul;
			f->fVolumeRDelta = fVolumeRDiff * inst->audio.fQuickVolRampSamplesMul;

			f->isFadeOutVoice = true;
		}

		/* Make current voice fade in from zero when it starts */
		v->fCurrVolumeL = v->fCurrVolumeR = 0.0f;
	}

	if (!voiceTriggerFlag && v->fTargetVolumeL == v->fCurrVolumeL && v->fTargetVolumeR == v->fCurrVolumeR)
	{
		v->volumeRampLength = 0; /* No ramp needed for now */
	}
	else
	{
		const float fVolumeLDiff = v->fTargetVolumeL - v->fCurrVolumeL;
		const float fVolumeRDiff = v->fTargetVolumeR - v->fCurrVolumeR;

		float fRampLengthMul;
		if (status & FT2_CS_USE_QUICK_VOLRAMP) /* 5ms duration */
		{
			v->volumeRampLength = inst->audio.quickVolRampSamples;
			fRampLengthMul = inst->audio.fQuickVolRampSamplesMul;
		}
		else /* Duration of a tick */
	{
		v->volumeRampLength = inst->audio.samplesPerTickInt;
			fRampLengthMul = inst->audio.fSamplesPerTickIntMul;
		}

		v->fVolumeLDelta = fVolumeLDiff * fRampLengthMul;
		v->fVolumeRDelta = fVolumeRDiff * fRampLengthMul;
	}
}

/* -------------------------------------------------------------------------
 * Volume ramp reset (called at start of each tick)
 * ------------------------------------------------------------------------- */

void ft2_reset_ramp_volumes(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_voice_t *v = inst->voice;
	for (int32_t i = 0; i < inst->replayer.song.numChannels; i++, v++)
	{
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
	}
}

/* -------------------------------------------------------------------------
 * BPM/Tempo management
 * ------------------------------------------------------------------------- */

void ft2_set_bpm(ft2_instance_t *inst, int32_t bpm)
{
	if (inst == NULL)
		return;

	if (bpm < FT2_MIN_BPM)
		bpm = FT2_MIN_BPM;
	else if (bpm > FT2_MAX_BPM)
		bpm = FT2_MAX_BPM;

	inst->replayer.song.BPM = bpm;
	ft2_instance_init_bpm_vars(inst);
}

void ft2_set_interpolation(ft2_instance_t *inst, uint8_t type)
{
	if (inst == NULL)
		return;
	
	/* Clamp to valid range */
	if (type >= FT2_NUM_INTERP_MODES)
		type = FT2_INTERP_LINEAR;
	
	inst->audio.interpolationType = type;
}

/* Update sinc LUT pointer for a voice based on its delta */
static void updateVoiceSincLUT(ft2_instance_t *inst, ft2_voice_t *v)
{
	if (inst->audio.interpolationType != FT2_INTERP_SINC8 &&
	    inst->audio.interpolationType != FT2_INTERP_SINC16)
		return;
	
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	if (tables == NULL)
		return;
	
	bool is16Point;
	v->fSincLUT = ft2_select_sinc_kernel(v->delta, tables, &is16Point);
	
	/* Override interpolation type based on selected kernel */
	/* This is handled per-voice for sinc since kernel selection depends on delta */
}

/* -------------------------------------------------------------------------
 * Key off and trigger helpers
 * ------------------------------------------------------------------------- */

static void keyOff(ft2_instance_t *inst, ft2_channel_t *ch)
{
	ch->keyOff = true;

	ft2_instr_t *ins = ch->instrPtr;
	if (ins == NULL)
		return;

	if (ins->volEnvFlags & FT2_ENV_ENABLED)
	{
		if (ch->volEnvTick >= (uint16_t)ins->volEnvPoints[ch->volEnvPos][0])
			ch->volEnvTick = ins->volEnvPoints[ch->volEnvPos][0] - 1;
	}
	else
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
	}

	if (!(ins->panEnvFlags & FT2_ENV_ENABLED)) // FT2 logic bug!
	{
		if (ch->panEnvTick >= (uint16_t)ins->panEnvPoints[ch->panEnvPos][0])
			ch->panEnvTick = ins->panEnvPoints[ch->panEnvPos][0] - 1;
	}

	/* MIDI Output - send note off if instrument has midiOn enabled */
	if (ins->midiOn && ch->midiNoteActive)
	{
		ft2_midi_event_t offEvent;
		offEvent.type = FT2_MIDI_NOTE_OFF;
		offEvent.channel = ins->midiChannel;
		offEvent.note = ch->lastMidiNote;
		offEvent.velocity = 0;
		offEvent.program = 0;
		offEvent.samplePos = 0;
		ft2_midi_queue_push(inst, &offEvent);

		ch->midiNoteActive = false;
	}
}

static void resetVolumes(ft2_channel_t *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_UPDATE_PAN | FT2_CS_USE_QUICK_VOLRAMP;
}

static void triggerInstrument(ft2_channel_t *ch)
{
	if (!(ch->vibTremCtrl & 0x04)) ch->vibratoPos = 0;
	if (!(ch->vibTremCtrl & 0x40)) ch->tremoloPos = 0;

	ch->noteRetrigCounter = 0;
	ch->tremorPos = 0;
	ch->keyOff = false;

	ft2_instr_t *ins = ch->instrPtr;
	if (ins != NULL)
	{
		// reset volume envelope
		if (ins->volEnvFlags & FT2_ENV_ENABLED)
		{
			ch->volEnvTick = 65535;
			ch->volEnvPos = 0;
		}

		// reset panning envelope
		if (ins->panEnvFlags & FT2_ENV_ENABLED)
		{
			ch->panEnvTick = 65535;
			ch->panEnvPos = 0;
		}

		// reset fadeout
		ch->fadeoutSpeed = ins->fadeout;
		ch->fadeoutVol = 32768;

		// reset auto-vibrato
		if (ins->autoVibDepth > 0)
		{
			ch->autoVibPos = 0;
			if (ins->autoVibSweep > 0)
			{
				ch->autoVibAmp = 0;
				ch->autoVibSweep = (ins->autoVibDepth << 8) / ins->autoVibSweep;
			}
			else
			{
				ch->autoVibAmp = ins->autoVibDepth << 8;
				ch->autoVibSweep = 0;
			}
		}
	}
}

/* -------------------------------------------------------------------------
 * Period/note helpers
 * ------------------------------------------------------------------------- */

static uint16_t getNotePeriod(ft2_instance_t *inst, int8_t note, int8_t finetune)
{
	if (note < 0)
		note = 0;
	else if (note >= 10 * 12)
		note = (10 * 12) - 1;

	const int32_t index = ((note % 12) << 4) + ((finetune >> 3) + 16);
	const int32_t octave = note / 12;

	if (inst->audio.linearPeriodsFlag)
		return linearPeriodLUT[index] >> octave;
	else
		return amigaPeriodLUT[index] >> octave;
}

static uint16_t period2NotePeriod(ft2_instance_t *inst, uint16_t period, uint8_t noteOffset, ft2_channel_t *ch)
{
	const int32_t fineTune = ((int8_t)ch->finetune >> 3) + 16;
	const uint16_t *lut = inst->audio.linearPeriodsFlag ? linearPeriodLUT : amigaPeriodLUT;

	int32_t hiPeriod = 8 * 12 * 16;
	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 8; i++)
	{
		int32_t tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + fineTune;

		int32_t lookUp = tmpPeriod - 8;
		if (lookUp < 0)
			lookUp = 0;

		if (period >= lut[lookUp])
			hiPeriod = (tmpPeriod - fineTune) & ~15;
		else
			loPeriod = (tmpPeriod - fineTune) & ~15;
	}

	int32_t tmpPeriod = loPeriod + fineTune + (noteOffset << 4);
	if (tmpPeriod >= (8 * 12 * 16 + 15) - 1)
		tmpPeriod = (8 * 12 * 16 + 16) - 1;

	return lut[tmpPeriod];
}

/* -------------------------------------------------------------------------
 * Trigger note
 * ------------------------------------------------------------------------- */

static void triggerNote(ft2_instance_t *inst, uint8_t note, uint8_t efx, uint8_t efxData, ft2_channel_t *ch)
{
	if (note == FT2_NOTE_OFF)
	{
		keyOff(inst, ch);
		return;
	}

	if (note == 0)
	{
		note = ch->noteNum;
		if (note == 0)
			return;
	}

	ch->noteNum = note;

	ft2_instr_t *ins = inst->replayer.instr[ch->instrNum];
	if (ins == NULL)
		ins = inst->replayer.instr[0];

	ch->instrPtr = ins;
	ch->mute = ins ? ins->mute : false;

	if (note > 96)
		note = 96;

	ch->smpNum = ins ? (ins->note2SampleLUT[note - 1] & 0xF) : 0;
	ft2_sample_t *s = ins ? &ins->smp[ch->smpNum] : NULL;

	ch->smpPtr = s;
	
	if (s)
	{
		ch->relativeNote = s->relativeNote;
		ch->oldVol = s->volume;
		ch->oldPan = s->panning;

		if (efx == 0x0E && (efxData & 0xF0) == 0x50) // E5x (Set Finetune)
			ch->finetune = ((efxData & 0x0F) * 16) - 128;
		else
			ch->finetune = s->finetune;
	}

	int8_t finalNote = (int8_t)note + ch->relativeNote;
	if (finalNote >= 10 * 12)
		return;

	if (finalNote != 0)
	{
		const uint16_t noteIndex = ((finalNote - 1) * 16) + (((int8_t)ch->finetune >> 3) + 16);
		const uint16_t *lut = inst->audio.linearPeriodsFlag ? linearPeriodLUT : amigaPeriodLUT;
		ch->outPeriod = ch->realPeriod = lut[noteIndex];
	}

	ch->status |= FT2_CF_UPDATE_PERIOD | FT2_CS_UPDATE_VOL | FT2_CS_UPDATE_PAN | 
	              FT2_CS_TRIGGER_VOICE | FT2_CS_USE_QUICK_VOLRAMP;

	if (efx == 9) // 9xx (Set Sample Offset)
	{
		if (efxData > 0)
			ch->sampleOffset = efxData;
		ch->smpStartPos = ch->sampleOffset << 8;
	}
	else
	{
		ch->smpStartPos = 0;
	}

	/* MIDI Output - send note on if instrument has midiOn enabled */
	if (ins != NULL && ins->midiOn && !ins->mute)
	{
		/* Convert FT2 note (1-96) to MIDI note (0-127) */
		/* FT2 note 48 = C-4 = MIDI 60, so: midiNote = ft2Note + 12 */
		int midiNote = finalNote + 11;  /* Match offset from ft2_midi.c */
		if (midiNote >= 0 && midiNote <= 127)
		{
			/* Send note off for previous note on this channel's MIDI channel */
			if (ch->midiNoteActive && ch->lastMidiNote != (uint8_t)midiNote)
			{
				ft2_midi_event_t offEvent;
				offEvent.type = FT2_MIDI_NOTE_OFF;
				offEvent.channel = ins->midiChannel;
				offEvent.note = ch->lastMidiNote;
				offEvent.velocity = 0;
				offEvent.program = 0;
				offEvent.samplePos = 0;
				ft2_midi_queue_push(inst, &offEvent);
			}

			/* Send note on */
			ft2_midi_event_t onEvent;
			onEvent.type = FT2_MIDI_NOTE_ON;
			onEvent.channel = ins->midiChannel;
			onEvent.note = (uint8_t)midiNote;
			/* Convert FT2 volume (0-64) to MIDI velocity (0-127) */
			onEvent.velocity = (ch->outVol > 0) ? (uint8_t)((ch->outVol * 127) / 64) : 100;
			onEvent.program = 0;
			onEvent.samplePos = 0;
			ft2_midi_queue_push(inst, &onEvent);

			ch->lastMidiNote = (uint8_t)midiNote;
			ch->midiNoteActive = true;
		}
	}
}

/* -------------------------------------------------------------------------
 * Effects - Tick Zero
 * ------------------------------------------------------------------------- */

static void finePitchSlideUp(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPitchSlideUpSpeed;
	ch->fPitchSlideUpSpeed = param;

	ch->realPeriod -= param * 4;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void finePitchSlideDown(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPitchSlideDownSpeed;
	ch->fPitchSlideDownSpeed = param;

	ch->realPeriod += param * 4;
	if ((int16_t)ch->realPeriod >= 32000)
		ch->realPeriod = 32000 - 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void fineVolSlideUp(ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideUpSpeed;
	ch->fVolSlideUpSpeed = param;

	ch->realVol += param;
	if (ch->realVol > 64)
		ch->realVol = 64;

	ch->outVol = ch->realVol;
	ch->status |= FT2_CS_UPDATE_VOL;
}

static void fineVolSlideDown(ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideDownSpeed;
	ch->fVolSlideDownSpeed = param;

	ch->realVol -= param;
	if ((int8_t)ch->realVol < 0)
		ch->realVol = 0;

	ch->outVol = ch->realVol;
	ch->status |= FT2_CS_UPDATE_VOL;
}

static void patternLoop(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	ft2_song_t *s = &inst->replayer.song;
	ft2_replayer_state_t *rep = &inst->replayer;

	if (param == 0)
	{
		ch->patternLoopStartRow = s->row & 0xFF;
	}
	else if (rep->patternLoopStateSet && rep->patternLoopCounter > 0)
	{
		/* DAW seek set loop state - use restored counter for mid-loop seeks */
		ch->patternLoopCounter = rep->patternLoopCounter;
		ch->patternLoopStartRow = (uint8_t)rep->patternLoopStartRow;
		rep->patternLoopStateSet = false;  /* Only use once */

		if (ch->patternLoopCounter > 0)
		{
			ch->patternLoopCounter--;  /* Decrement since we're about to play this iteration */
			if (ch->patternLoopCounter > 0)
			{
				s->pBreakPos = ch->patternLoopStartRow;
				s->pBreakFlag = true;
			}
			/* else: counter exhausted, continue normally */
		}
	}
	else if (ch->patternLoopCounter == 0)
	{
		ch->patternLoopCounter = param;
		s->pBreakPos = ch->patternLoopStartRow;
		s->pBreakFlag = true;
	}
	else if (--ch->patternLoopCounter > 0)
	{
		s->pBreakPos = ch->patternLoopStartRow;
		s->pBreakFlag = true;
	}
}

static void patternDelay(ft2_instance_t *inst, uint8_t param)
{
	if (inst->replayer.song.pattDelTime2 == 0)
		inst->replayer.song.pattDelTime = param + 1;
}

static void extraFinePitchSlide(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	const uint8_t slideType = param >> 4;
	param &= 0x0F;

	if (slideType == 1) // slide up
	{
		if (param == 0)
			param = ch->efPitchSlideUpSpeed;
		ch->efPitchSlideUpSpeed = param;

		ch->realPeriod -= param;
		if ((int16_t)ch->realPeriod < 1)
			ch->realPeriod = 1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= FT2_CF_UPDATE_PERIOD;
	}
	else if (slideType == 2) // slide down
	{
		if (param == 0)
			param = ch->efPitchSlideDownSpeed;
		ch->efPitchSlideDownSpeed = param;

		ch->realPeriod += param;
		if ((int16_t)ch->realPeriod >= 32000)
			ch->realPeriod = 32000 - 1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= FT2_CF_UPDATE_PERIOD;
	}
}

static void setEnvelopePos(ft2_channel_t *ch, uint8_t param)
{
	bool envUpdate;
	int8_t point;
	int32_t tick;

	ft2_instr_t *ins = ch->instrPtr;
	if (ins == NULL)
		return;

	/* *** VOLUME ENVELOPE *** */
	if (ins->volEnvFlags & FT2_ENV_ENABLED)
	{
		ch->volEnvTick = param - 1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->volEnvLength > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->volEnvLength - 1; i++)
			{
				if (tick < ins->volEnvPoints[point][0])
				{
					point--;

					tick -= ins->volEnvPoints[point][0];
					if (tick == 0) /* FT2 doesn't test for <= 0 here */
					{
						envUpdate = false;
						break;
					}

					const int32_t xDiff = ins->volEnvPoints[point + 1][0] - ins->volEnvPoints[point + 0][0];
					if (xDiff <= 0)
					{
						envUpdate = true;
						break;
					}

					const int32_t y0 = ins->volEnvPoints[point + 0][1] & 0xFF;
					const int32_t y1 = ins->volEnvPoints[point + 1][1] & 0xFF;
					const int32_t yDiff = y1 - y0;

					ch->fVolEnvDelta = (float)yDiff / (float)xDiff;
					ch->fVolEnvValue = (float)y0 + (ch->fVolEnvDelta * (tick - 1));

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate)
		{
			ch->fVolEnvDelta = 0.0f;
			ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[point][1] & 0xFF);
		}

		if (point >= ins->volEnvLength)
		{
			point = ins->volEnvLength - 1;
			if (point < 0)
				point = 0;
		}

		ch->volEnvPos = point;
	}

	/* *** PANNING ENVELOPE *** */
	/* FT2 logic bug: should've been ins->panEnvFlags, but FT2 uses volEnvFlags & ENV_SUSTAIN */
	if (ins->volEnvFlags & FT2_ENV_SUSTAIN)
	{
		ch->panEnvTick = param - 1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->panEnvLength > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->panEnvLength - 1; i++)
			{
				if (tick < ins->panEnvPoints[point][0])
				{
					point--;

					tick -= ins->panEnvPoints[point][0];
					if (tick == 0) /* FT2 doesn't test for <= 0 here */
					{
						envUpdate = false;
						break;
					}

					const int32_t xDiff = ins->panEnvPoints[point + 1][0] - ins->panEnvPoints[point + 0][0];
					if (xDiff <= 0)
					{
						envUpdate = true;
						break;
					}

					const int32_t y0 = ins->panEnvPoints[point + 0][1] & 0xFF;
					const int32_t y1 = ins->panEnvPoints[point + 1][1] & 0xFF;
					const int32_t yDiff = y1 - y0;

					ch->fPanEnvDelta = (float)yDiff / (float)xDiff;
					ch->fPanEnvValue = (float)y0 + (ch->fPanEnvDelta * (tick - 1));

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate)
		{
			ch->fPanEnvDelta = 0.0f;
			ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[point][1] & 0xFF);
		}

		if (point >= ins->panEnvLength)
		{
			point = ins->panEnvLength - 1;
			if (point < 0)
				point = 0;
		}

		ch->panEnvPos = point;
	}
}

static void doMultiNoteRetrig(ft2_instance_t *inst, ft2_channel_t *ch)
{
	uint8_t cnt = ch->noteRetrigCounter + 1;
	if (cnt < ch->noteRetrigSpeed)
	{
		ch->noteRetrigCounter = cnt;
		return;
	}

	ch->noteRetrigCounter = 0;

	int16_t vol = ch->realVol;
	switch (ch->noteRetrigVol)
	{
		case 0x1: vol -= 1; break;
		case 0x2: vol -= 2; break;
		case 0x3: vol -= 4; break;
		case 0x4: vol -= 8; break;
		case 0x5: vol -= 16; break;
		case 0x6: vol = (vol >> 1) + (vol >> 3) + (vol >> 4); break;
		case 0x7: vol >>= 1; break;
		case 0x8: break; /* does not change the volume */
		case 0x9: vol += 1; break;
		case 0xA: vol += 2; break;
		case 0xB: vol += 4; break;
		case 0xC: vol += 8; break;
		case 0xD: vol += 16; break;
		case 0xE: vol = (vol >> 1) + vol; break;
		case 0xF: vol += vol; break;
		default: break;
	}
	vol = CLAMP(vol, 0, 64);

	ch->realVol = (uint8_t)vol;
	ch->outVol = ch->realVol;

	if (ch->volColumnVol >= 0x10 && ch->volColumnVol <= 0x50)
	{
		ch->outVol = ch->volColumnVol - 0x10;
		ch->realVol = ch->outVol;
	}
	else if (ch->volColumnVol >= 0xC0 && ch->volColumnVol <= 0xCF)
	{
		ch->outPan = (ch->volColumnVol & 0x0F) << 4;
	}

	triggerNote(inst, 0, 0, 0, ch);
}

static void multiNoteRetrig(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param, uint8_t volumeColumnData)
{
	uint8_t tmpParam;

	tmpParam = param & 0x0F;
	if (tmpParam == 0)
		tmpParam = ch->noteRetrigSpeed;

	ch->noteRetrigSpeed = tmpParam;

	tmpParam = param >> 4;
	if (tmpParam == 0)
		tmpParam = ch->noteRetrigVol;

	ch->noteRetrigVol = tmpParam;

	if (volumeColumnData == 0)
		doMultiNoteRetrig(inst, ch);
}

static void E_Effects_TickZero(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	const uint8_t efx = param >> 4;
	param &= 0x0F;

	if (ch->channelOff)
	{
		if (efx == 0x6)
			patternLoop(inst, ch, param);
		else if (efx == 0xE)
			patternDelay(inst, param);
		return;
	}

	switch (efx)
	{
		case 0x1: finePitchSlideUp(inst, ch, param); break;
		case 0x2: finePitchSlideDown(inst, ch, param); break;
		case 0x3: ch->semitonePortaMode = (param != 0); break;
		case 0x4: ch->vibTremCtrl = (ch->vibTremCtrl & 0xF0) | param; break;
		case 0x6: patternLoop(inst, ch, param); break;
		case 0x7: ch->vibTremCtrl = (param << 4) | (ch->vibTremCtrl & 0x0F); break;
		case 0xA: fineVolSlideUp(ch, param); break;
		case 0xB: fineVolSlideDown(ch, param); break;
		case 0xC: if (param == 0) { ch->realVol = 0; ch->outVol = 0; ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP; } break;
		case 0xE: patternDelay(inst, param); break;
		default: break;
	}
}

static void handleMoreEffects_TickZero(ft2_instance_t *inst, ft2_channel_t *ch)
{
	const uint8_t efx = ch->efx;
	const uint8_t param = ch->efxData;

	switch (efx)
	{
		case 0x0B: // Position jump
			if (inst->replayer.playMode != FT2_PLAYMODE_PATT && inst->replayer.playMode != FT2_PLAYMODE_RECPATT)
			{
				const int16_t pos = (int16_t)param - 1;
				if (pos < 0 || pos >= inst->replayer.song.songLength)
					inst->replayer.bxxOverflow = true;
				else
					inst->replayer.song.songPos = pos;
			}
			inst->replayer.song.pBreakPos = 0;
			inst->replayer.song.posJumpFlag = true;
			break;

		case 0x0D: // Pattern break
			{
				uint8_t row = ((param >> 4) * 10) + (param & 0x0F);
				if (row > 63) row = 0;
				inst->replayer.song.pBreakPos = row;
				inst->replayer.song.posJumpFlag = true;
			}
			break;

		case 0x0E: // E effects
			E_Effects_TickZero(inst, ch, param);
			break;

		case 0x0F: // Set speed/BPM
			if (param >= 32)
			{
				/* Only apply BPM effect if not syncing BPM from DAW */
				if (!inst->config.syncBpmFromDAW)
				{
					inst->replayer.song.BPM = param;
					ft2_set_bpm(inst, param);
					inst->uiState.updatePosSections = true;
				}
			}
			else if (inst->config.allowFxxSpeedChanges)
			{
				inst->replayer.song.tick = inst->replayer.song.speed = param;
				inst->uiState.updatePosSections = true;
			}
			break;

		case 0x10: // Set global volume
			{
				uint8_t gvol = (param > 64) ? 64 : param;
				inst->replayer.song.globalVolume = gvol;
				for (int32_t i = 0; i < inst->replayer.song.numChannels; i++)
					inst->replayer.channel[i].status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0x15: // Lxx - Set envelope position
			setEnvelopePos(ch, param);
			break;

		case 0x21: // Extra fine pitch slide
			extraFinePitchSlide(inst, ch, param);
			break;

		default:
			break;
	}
}

/* Handle volume column effects on tick zero */
static void handleVolColumnEffects_TickZero(ft2_channel_t *ch)
{
	const uint8_t vol = ch->volColumnVol;
	const uint8_t cmd = vol >> 4;
	const uint8_t param = vol & 0x0F;

	switch (cmd)
	{
		case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: // Set volume
			{
				uint8_t v = vol - 0x10;
				if (v > 64) v = 64;
				ch->outVol = ch->realVol = v;
				ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
			}
			break;

		case 0x6: // Volume slide down
			{
				uint8_t newVol = ch->realVol - param;
				if ((int8_t)newVol < 0) newVol = 0;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0x7: // Volume slide up
			{
				uint8_t newVol = ch->realVol + param;
				if (newVol > 64) newVol = 64;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0x8: // Fine volume slide down
			{
				uint8_t newVol = ch->realVol - param;
				if ((int8_t)newVol < 0) newVol = 0;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0x9: // Fine volume slide up
			{
				uint8_t newVol = ch->realVol + param;
				if (newVol > 64) newVol = 64;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0xA: // Set vibrato speed
			{
				uint8_t speed = param * 4;
				if (speed != 0)
					ch->vibratoSpeed = speed;
			}
			break;

		case 0xC: // Set panning
			ch->outPan = param << 4;
			ch->status |= FT2_CS_UPDATE_PAN;
			break;

		case 0xF: // Tone portamento
			if (param > 0)
				ch->portamentoSpeed = (param << 4) * 4;
			break;

		default:
			break;
	}
}

static void handleEffects_TickZero(ft2_instance_t *inst, ft2_channel_t *ch)
{
	/* FT2 quirk: volume column value is modified by vol col effects and passed to Rxy.
	** This modified value determines whether multiNoteRetrig triggers on tick zero. */
	uint8_t newVolCol = ch->volColumnVol;
	const uint8_t volCmd = ch->volColumnVol >> 4;
	const uint8_t volParam = ch->volColumnVol & 0x0F;

	/* Apply volume column effect and track modified value for Rxy quirk */
	switch (volCmd)
	{
		case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: /* Set volume */
			newVolCol -= 16;
			if (newVolCol > 64) newVolCol = 64;
			ch->outVol = ch->realVol = newVolCol;
			ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
			break;

		case 0x6: /* Volume slide down */
			newVolCol = (uint8_t)(0 - volParam) + ch->realVol;
			if ((int8_t)newVolCol < 0) newVolCol = 0;
			ch->outVol = ch->realVol = newVolCol;
			ch->status |= FT2_CS_UPDATE_VOL;
			break;

		case 0x7: /* Volume slide up */
			newVolCol = volParam + ch->realVol;
			if (newVolCol > 64) newVolCol = 64;
			ch->outVol = ch->realVol = newVolCol;
			ch->status |= FT2_CS_UPDATE_VOL;
			break;

		case 0x8: /* Fine volume slide down */
			newVolCol = (uint8_t)(0 - volParam) + ch->realVol;
			if ((int8_t)newVolCol < 0) newVolCol = 0;
			ch->outVol = ch->realVol = newVolCol;
			ch->status |= FT2_CS_UPDATE_VOL;
			break;

		case 0x9: /* Fine volume slide up */
			newVolCol = volParam + ch->realVol;
			if (newVolCol > 64) newVolCol = 64;
			ch->outVol = ch->realVol = newVolCol;
			ch->status |= FT2_CS_UPDATE_VOL;
			break;

		case 0xA: /* Set vibrato speed */
			newVolCol = volParam * 4;
			if (newVolCol != 0)
				ch->vibratoSpeed = newVolCol;
			break;

		case 0xC: /* Set panning */
			newVolCol = volParam << 4;
			ch->outPan = newVolCol;
			ch->status |= FT2_CS_UPDATE_PAN;
			break;

		case 0xF: /* Tone portamento */
			if (volParam > 0)
				ch->portamentoSpeed = (volParam << 4) * 4;
			break;

		default:
			break;
	}

	/* Normal effects */
	const uint8_t param = ch->efxData;
	if (ch->efx == 0 && param == 0)
		return;

	if (ch->efx == 0x08) /* Set panning */
	{
			ch->outPan = param;
			ch->status |= FT2_CS_UPDATE_PAN;
	}
	else if (ch->efx == 0x0C) /* Set volume */
	{
		uint8_t vol = (param > 64) ? 64 : param;
		ch->outVol = ch->realVol = vol;
				ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
	}
	else if (ch->efx == 0x1B) /* Rxy - Multi-note retrig */
	{
		multiNoteRetrig(inst, ch, param, newVolCol);
	}

	handleMoreEffects_TickZero(inst, ch);
}

/* -------------------------------------------------------------------------
 * Effects - Tick Non-Zero
 * ------------------------------------------------------------------------- */

static void arpeggio(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	const uint8_t tick = arpeggioTab[inst->replayer.song.tick & 31];
	
	if (tick == 0)
	{
		ch->outPeriod = ch->realPeriod;
	}
	else
	{
		uint8_t noteOffset = (tick == 1) ? (param >> 4) : (param & 0x0F);
		ch->outPeriod = period2NotePeriod(inst, ch->realPeriod, noteOffset, ch);
	}
	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void pitchSlideUp(ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->pitchSlideUpSpeed;
	ch->pitchSlideUpSpeed = param;

	ch->realPeriod -= param * 4;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void pitchSlideDown(ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->pitchSlideDownSpeed;
	ch->pitchSlideDownSpeed = param;

	ch->realPeriod += param * 4;
	if ((int16_t)ch->realPeriod >= 32000)
		ch->realPeriod = 32000 - 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void portamento(ft2_instance_t *inst, ft2_channel_t *ch)
{
	if (ch->portamentoDirection == 0)
		return;

	if (ch->portamentoDirection > 1)
	{
		ch->realPeriod -= ch->portamentoSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->portamentoTargetPeriod)
		{
			ch->portamentoDirection = 1;
			ch->realPeriod = ch->portamentoTargetPeriod;
		}
	}
	else
	{
		ch->realPeriod += ch->portamentoSpeed;
		if (ch->realPeriod >= ch->portamentoTargetPeriod)
		{
			ch->portamentoDirection = 1;
			ch->realPeriod = ch->portamentoTargetPeriod;
		}
	}

	if (ch->semitonePortaMode)
		ch->outPeriod = period2NotePeriod(inst, ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= FT2_CF_UPDATE_PERIOD;
}

static void doVibrato(ft2_instance_t *inst, ft2_channel_t *ch)
{
	uint8_t tmpVib = (ch->vibratoPos >> 2) & 0x1F;

	switch (ch->vibTremCtrl & 3)
	{
		case 0: tmpVib = vibratoTab[tmpVib]; break;
		case 1:
			tmpVib <<= 3;
			if ((int8_t)ch->vibratoPos < 0)
				tmpVib = ~tmpVib;
			break;
		default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibratoDepth) >> 5;

	if ((int8_t)ch->vibratoPos < 0)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= FT2_CF_UPDATE_PERIOD;
	ch->vibratoPos += ch->vibratoSpeed;
}

static void vibrato(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param > 0)
	{
		const uint8_t depth = param & 0x0F;
		if (depth > 0)
			ch->vibratoDepth = depth;

		const uint8_t speed = (param & 0xF0) >> 2;
		if (speed > 0)
			ch->vibratoSpeed = speed;
	}
	doVibrato(inst, ch);
}

static void volSlide(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->volSlideSpeed;
	ch->volSlideSpeed = param;

	uint8_t newVol = ch->realVol;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		newVol += param >> 4;
		if (newVol > 64)
			newVol = 64;
	}

	ch->outVol = ch->realVol = newVol;
	ch->status |= FT2_CS_UPDATE_VOL;
}

static void tremolo(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param > 0)
	{
		const uint8_t depth = param & 0x0F;
		if (depth > 0)
			ch->tremoloDepth = depth;

		const uint8_t speed = (param & 0xF0) >> 2;
		if (speed > 0)
			ch->tremoloSpeed = speed;
	}

	uint8_t tmpTrem = (ch->tremoloPos >> 2) & 0x1F;
	switch ((ch->vibTremCtrl >> 4) & 3)
	{
		case 0: tmpTrem = vibratoTab[tmpTrem]; break;
		case 1:
			tmpTrem <<= 3;
			if ((int8_t)ch->vibratoPos < 0) // FT2 bug
				tmpTrem = ~tmpTrem;
			break;
		default: tmpTrem = 255; break;
	}
	tmpTrem = (tmpTrem * ch->tremoloDepth) >> 6;

	int16_t tremVol;
	if ((int8_t)ch->tremoloPos < 0)
	{
		tremVol = ch->realVol - tmpTrem;
		if (tremVol < 0) tremVol = 0;
	}
	else
	{
		tremVol = ch->realVol + tmpTrem;
		if (tremVol > 64) tremVol = 64;
	}

	ch->outVol = (uint8_t)tremVol;
	ch->status |= FT2_CS_UPDATE_VOL;
	ch->tremoloPos += ch->tremoloSpeed;
}

static void globalVolSlide(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->globVolSlideSpeed;
	ch->globVolSlideSpeed = param;

	uint8_t newVol = (uint8_t)inst->replayer.song.globalVolume;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		newVol += param >> 4;
		if (newVol > 64)
			newVol = 64;
	}

	inst->replayer.song.globalVolume = newVol;
	for (int32_t i = 0; i < inst->replayer.song.numChannels; i++)
		inst->replayer.channel[i].status |= FT2_CS_UPDATE_VOL;
}

static void panningSlide(ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->panningSlideSpeed;
	ch->panningSlideSpeed = param;

	int16_t newPan = (int16_t)ch->outPan;
	if ((param & 0xF0) == 0)
	{
		newPan -= param;
		if (newPan < 0) newPan = 0;
	}
	else
	{
		newPan += param >> 4;
		if (newPan > 255) newPan = 255;
	}

	ch->outPan = (uint8_t)newPan;
	ch->status |= FT2_CS_UPDATE_PAN;
}

static void tremor(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->tremorParam;
	ch->tremorParam = param;

	uint8_t tremorSign = ch->tremorPos & 0x80;
	uint8_t tremorData = ch->tremorPos & 0x7F;

	tremorData--;
	if ((int8_t)tremorData < 0)
	{
		if (tremorSign == 0x80)
		{
			tremorSign = 0x00;
			tremorData = param & 0x0F;
		}
		else
		{
			tremorSign = 0x80;
			tremorData = param >> 4;
		}
	}

	ch->tremorPos = tremorSign | tremorData;
	ch->outVol = (tremorSign == 0x80) ? ch->realVol : 0;
	ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
}

static void retrigNote(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if (param == 0)
		return;

	if ((inst->replayer.song.speed - inst->replayer.song.tick) % param == 0)
	{
		triggerNote(inst, 0, 0, 0, ch);
		triggerInstrument(ch);
	}
}

static void noteCut(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if ((uint8_t)(inst->replayer.song.speed - inst->replayer.song.tick) == param)
	{
		ch->outVol = ch->realVol = 0;
		ch->status |= FT2_CS_UPDATE_VOL | FT2_CS_USE_QUICK_VOLRAMP;
	}
}

static void noteDelay(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if ((uint8_t)(inst->replayer.song.speed - inst->replayer.song.tick) == param)
	{
		const uint8_t note = ch->copyOfInstrAndNote & 0x00FF;
		triggerNote(inst, note, 0, 0, ch);

		const uint8_t instrument = ch->copyOfInstrAndNote >> 8;
		if (instrument > 0)
			resetVolumes(ch);

		triggerInstrument(ch);

		if (ch->volColumnVol >= 0x10 && ch->volColumnVol <= 0x50)
		{
			ch->outVol = ch->volColumnVol - 16;
			ch->realVol = ch->outVol;
		}
		else if (ch->volColumnVol >= 0xC0 && ch->volColumnVol <= 0xCF)
		{
			ch->outPan = (ch->volColumnVol & 0x0F) << 4;
		}
	}
}

static void keyOffCmd(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	if ((uint8_t)(inst->replayer.song.speed - inst->replayer.song.tick) == (param & 31))
		keyOff(inst, ch);
}

static void E_Effects_TickNonZero(ft2_instance_t *inst, ft2_channel_t *ch, uint8_t param)
{
	const uint8_t efx = param >> 4;
	param &= 0x0F;

	switch (efx)
	{
		case 0x9: retrigNote(inst, ch, param); break;
		case 0xC: noteCut(inst, ch, param); break;
		case 0xD: noteDelay(inst, ch, param); break;
		default: break;
	}
}

/* Handle volume column effects on non-zero ticks */
static void handleVolColumnEffects_TickNonZero(ft2_instance_t *inst, ft2_channel_t *ch)
{
	const uint8_t cmd = ch->volColumnVol >> 4;
	const uint8_t param = ch->volColumnVol & 0x0F;

	switch (cmd)
	{
		case 0x6: // Volume slide down
			{
				uint8_t newVol = ch->realVol - param;
				if ((int8_t)newVol < 0) newVol = 0;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0x7: // Volume slide up
			{
				uint8_t newVol = ch->realVol + param;
				if (newVol > 64) newVol = 64;
				ch->outVol = ch->realVol = newVol;
				ch->status |= FT2_CS_UPDATE_VOL;
			}
			break;

		case 0xB: // Vibrato
			if (param > 0)
				ch->vibratoDepth = param;
			doVibrato(inst, ch);
			break;

		case 0xD: // Panning slide left
			{
				uint16_t tmp = ch->outPan + (uint8_t)(0 - param);
				if (tmp < 256)
					tmp = 0;
				ch->outPan = (uint8_t)tmp;
				ch->status |= FT2_CS_UPDATE_PAN;
			}
			break;

		case 0xE: // Panning slide right
			{
				uint16_t tmp = ch->outPan + param;
				if (tmp > 255)
					tmp = 255;
				ch->outPan = (uint8_t)tmp;
				ch->status |= FT2_CS_UPDATE_PAN;
			}
			break;

		case 0xF: // Tone portamento
			portamento(inst, ch);
			break;

		default:
			break;
	}
}

static void handleEffects_TickNonZero(ft2_instance_t *inst, ft2_channel_t *ch)
{
	if (ch->channelOff)
		return;

	handleVolColumnEffects_TickNonZero(inst, ch);

	if ((ch->efx == 0 && ch->efxData == 0) || ch->efx > 35)
		return;

	const uint8_t param = ch->efxData;

	switch (ch->efx)
	{
		case 0x00: arpeggio(inst, ch, param); break;
		case 0x01: pitchSlideUp(ch, param); break;
		case 0x02: pitchSlideDown(ch, param); break;
		case 0x03: portamento(inst, ch); break;
		case 0x04: vibrato(inst, ch, param); break;
		case 0x05: portamento(inst, ch); volSlide(inst, ch, param); break;
		case 0x06: doVibrato(inst, ch); volSlide(inst, ch, param); break;
		case 0x07: tremolo(inst, ch, param); break;
		case 0x0A: volSlide(inst, ch, param); break;
		case 0x0E: E_Effects_TickNonZero(inst, ch, param); break;
		case 0x11: globalVolSlide(inst, ch, param); break;
		case 0x14: keyOffCmd(inst, ch, param); break;
		case 0x19: panningSlide(ch, param); break;
		case 0x1B: doMultiNoteRetrig(inst, ch); break;
		case 0x1D: tremor(inst, ch, param); break;
		default: break;
	}
}

/* -------------------------------------------------------------------------
 * Envelope and auto-vibrato processing
 * ------------------------------------------------------------------------- */

static void updateVolPanAutoVib(ft2_instance_t *inst, ft2_channel_t *ch)
{
	ft2_instr_t *ins = ch->instrPtr;
	if (ins == NULL)
		ins = inst->replayer.instr[0];

	// Fadeout on key off
	if (ch->keyOff)
	{
		if (ch->fadeoutSpeed > 0)
		{
			ch->fadeoutVol -= ch->fadeoutSpeed;
			if (ch->fadeoutVol <= 0)
			{
				ch->fadeoutVol = 0;
				ch->fadeoutSpeed = 0;
			}
		}
		ch->status |= FT2_CS_UPDATE_VOL;
	}

	float fEnvVal = 0.0f;
	float fVol;

	if (!ch->mute && ins)
	{
		// Volume envelope
		if (ins->volEnvFlags & FT2_ENV_ENABLED)
		{
			bool envDidInterpolate = false;
			uint8_t envPos = ch->volEnvPos;

			ch->volEnvTick++;

			if (ch->volEnvTick == ins->volEnvPoints[envPos][0])
			{
				ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[envPos][1] & 0xFF);

				envPos++;
				if (ins->volEnvFlags & FT2_ENV_LOOP)
				{
					envPos--;
					if (envPos == ins->volEnvLoopEnd)
					{
						if (!(ins->volEnvFlags & FT2_ENV_SUSTAIN) || envPos != ins->volEnvSustain || !ch->keyOff)
						{
							envPos = ins->volEnvLoopStart;
							ch->volEnvTick = ins->volEnvPoints[envPos][0];
							ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[envPos][1] & 0xFF);
						}
					}
					envPos++;
				}

				if (envPos < ins->volEnvLength)
				{
					bool envInterpolateFlag = true;
					if ((ins->volEnvFlags & FT2_ENV_SUSTAIN) && !ch->keyOff)
					{
						if (envPos - 1 == ins->volEnvSustain)
						{
							envPos--;
							ch->fVolEnvDelta = 0.0f;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag)
					{
						ch->volEnvPos = envPos;

						const int32_t x0 = ins->volEnvPoints[envPos - 1][0];
						const int32_t x1 = ins->volEnvPoints[envPos][0];
						const int32_t xDiff = x1 - x0;

						if (xDiff > 0)
						{
							const int32_t y0 = ins->volEnvPoints[envPos - 1][1] & 0xFF;
							const int32_t y1 = ins->volEnvPoints[envPos][1] & 0xFF;
							const int32_t yDiff = y1 - y0;

							ch->fVolEnvDelta = (float)yDiff / (float)xDiff;
							fEnvVal = ch->fVolEnvValue;
							envDidInterpolate = true;
						}
						else
						{
							ch->fVolEnvDelta = 0.0f;
						}
					}
				}
				else
				{
					ch->fVolEnvDelta = 0.0f;
				}
			}

			if (!envDidInterpolate)
			{
				ch->fVolEnvValue += ch->fVolEnvDelta;
				fEnvVal = ch->fVolEnvValue;
				if (fEnvVal < 0.0f || fEnvVal > 64.0f)
				{
					fEnvVal = CLAMP(fEnvVal, 0.0f, 64.0f);
					ch->fVolEnvDelta = 0.0f;
				}
			}

			const int32_t vol = inst->replayer.song.globalVolume * ch->outVol * ch->fadeoutVol;
			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
			fVol *= fEnvVal * (1.0f / 64.0f);
			ch->status |= FT2_CS_UPDATE_VOL;
		}
		else
		{
			const int32_t vol = inst->replayer.song.globalVolume * ch->outVol * ch->fadeoutVol;
			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
		}

		ch->fFinalVol = CLAMP(fVol, 0.0f, 1.0f);
	}
	else
	{
		ch->fFinalVol = 0.0f;
	}

	// Panning envelope
	if (ins && (ins->panEnvFlags & FT2_ENV_ENABLED))
	{
		bool envDidInterpolate = false;
		uint8_t envPos = ch->panEnvPos;

		ch->panEnvTick++;

		if (ch->panEnvTick == ins->panEnvPoints[envPos][0])
		{
			ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[envPos][1] & 0xFF);

			envPos++;
			if (ins->panEnvFlags & FT2_ENV_LOOP)
			{
				envPos--;
				if (envPos == ins->panEnvLoopEnd)
				{
					if (!(ins->panEnvFlags & FT2_ENV_SUSTAIN) || envPos != ins->panEnvSustain || !ch->keyOff)
					{
						envPos = ins->panEnvLoopStart;
						ch->panEnvTick = ins->panEnvPoints[envPos][0];
						ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[envPos][1] & 0xFF);
					}
				}
				envPos++;
			}

			if (envPos < ins->panEnvLength)
			{
				bool envInterpolateFlag = true;
				if ((ins->panEnvFlags & FT2_ENV_SUSTAIN) && !ch->keyOff)
				{
					if (envPos - 1 == ins->panEnvSustain)
					{
						envPos--;
						ch->fPanEnvDelta = 0.0f;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag)
				{
					ch->panEnvPos = envPos;

					const int32_t x0 = ins->panEnvPoints[envPos - 1][0];
					const int32_t x1 = ins->panEnvPoints[envPos][0];
					const int32_t xDiff = x1 - x0;

					if (xDiff > 0)
					{
						const int32_t y0 = ins->panEnvPoints[envPos - 1][1] & 0xFF;
						const int32_t y1 = ins->panEnvPoints[envPos][1] & 0xFF;
						const int32_t yDiff = y1 - y0;

						ch->fPanEnvDelta = (float)yDiff / (float)xDiff;
						envDidInterpolate = true;
					}
					else
					{
						ch->fPanEnvDelta = 0.0f;
					}
				}
			}
			else
			{
				ch->fPanEnvDelta = 0.0f;
			}
		}

		if (!envDidInterpolate)
		{
			ch->fPanEnvValue += ch->fPanEnvDelta;
			if (ch->fPanEnvValue < 0.0f || ch->fPanEnvValue > 64.0f)
			{
				ch->fPanEnvValue = CLAMP(ch->fPanEnvValue, 0.0f, 64.0f);
				ch->fPanEnvDelta = 0.0f;
			}
		}

		fEnvVal = ch->fPanEnvValue;
		ch->finalPan = ch->outPan + (uint8_t)((fEnvVal - 32.0f) * ((128.0f - (float)abs(ch->outPan - 128)) / 32.0f));
		ch->status |= FT2_CS_UPDATE_PAN;
	}
	else
	{
		ch->finalPan = ch->outPan;
	}

	// Auto-vibrato
	if (ins && ins->autoVibDepth > 0)
	{
		uint16_t autoVibAmp;
		if (ins->autoVibSweep > 0)
		{
			autoVibAmp = ch->autoVibAmp;
			if (autoVibAmp < (ins->autoVibDepth << 8))
			{
				autoVibAmp += ch->autoVibSweep;
				if (autoVibAmp > (ins->autoVibDepth << 8))
					autoVibAmp = ins->autoVibDepth << 8;
				ch->autoVibAmp = autoVibAmp;
			}
		}
		else
		{
			autoVibAmp = ins->autoVibDepth << 8;
		}

		ch->autoVibPos += ins->autoVibRate;

		int16_t autoVibVal;
		switch (ins->autoVibType)
		{
			case 1: autoVibVal = (ch->autoVibPos > 127) ? 64 : -64; break;
			case 2: autoVibVal = (((ch->autoVibPos >> 1) + 64) & 127) - 64; break;
			case 3: autoVibVal = ((-(ch->autoVibPos >> 1) + 64) & 127) - 64; break;
			default: autoVibVal = autoVibSineTab[ch->autoVibPos]; break;
		}

		autoVibVal = (autoVibVal * (int16_t)autoVibAmp) >> (6 + 8);

		uint16_t tmpPeriod = ch->outPeriod + autoVibVal;
		if (tmpPeriod >= 32000)
			tmpPeriod = 0;

		ch->finalPeriod = tmpPeriod;
		ch->status |= FT2_CF_UPDATE_PERIOD;
	}
	else
	{
		ch->finalPeriod = ch->outPeriod;
	}
}

/* -------------------------------------------------------------------------
 * Prepare portamento for note with 3xx/5xx or volume column Fxx
 * ------------------------------------------------------------------------- */

static void preparePortamento(ft2_instance_t *inst, ft2_channel_t *ch, ft2_note_t *p, uint8_t instNum)
{
	if (p->note > 0)
	{
		if (p->note == FT2_NOTE_OFF)
		{
			keyOff(inst, ch);
		}
		else
		{
			const uint16_t note = (((p->note - 1) + ch->relativeNote) * 16) + (((int8_t)ch->finetune >> 3) + 16);
			if (note < 1936)
			{
				const uint16_t *lut = inst->audio.linearPeriodsFlag ? linearPeriodLUT : amigaPeriodLUT;
				ch->portamentoTargetPeriod = lut[note];

				if (ch->portamentoTargetPeriod == ch->realPeriod)
					ch->portamentoDirection = 0;
				else if (ch->portamentoTargetPeriod > ch->realPeriod)
					ch->portamentoDirection = 1;
				else
					ch->portamentoDirection = 2;
			}
		}
	}

	if (instNum > 0)
	{
		resetVolumes(ch);
		if (p->note != FT2_NOTE_OFF)
			triggerInstrument(ch);
	}
}

/* -------------------------------------------------------------------------
 * Get new note (tick zero note processing)
 * ------------------------------------------------------------------------- */

static void getNewNote(ft2_instance_t *inst, ft2_channel_t *ch, ft2_note_t *p)
{
	ch->volColumnVol = p->vol;

	if (ch->efx == 0)
	{
		if (ch->efxData > 0) // arpeggio running
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= FT2_CF_UPDATE_PERIOD;
		}
	}
	else
	{
		if ((ch->efx == 4 || ch->efx == 6) && (p->efx != 4 && p->efx != 6))
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= FT2_CF_UPDATE_PERIOD;
		}
	}

	ch->efx = p->efx;
	ch->efxData = p->efxData;
	ch->copyOfInstrAndNote = (p->instr << 8) | p->note;

	if (ch->channelOff)
	{
		handleMoreEffects_TickZero(inst, ch);
		return;
	}

	uint8_t instNum = p->instr;
	if (instNum > 0)
	{
		if (instNum <= FT2_MAX_INST)
			ch->instrNum = instNum;
		else
			instNum = 0;
	}

	if (p->efx == 0x0E && p->efxData >= 0xD1 && p->efxData <= 0xDF)
		return; // note delay

	if (p->efx != 0x0E || p->efxData != 0x90)
	{
		if ((ch->volColumnVol & 0xF0) == 0xF0) // volume column portamento
		{
			const uint8_t param = ch->volColumnVol & 0x0F;
			if (param > 0)
				ch->portamentoSpeed = (param << 4) * 4;

			preparePortamento(inst, ch, p, instNum);
			handleEffects_TickZero(inst, ch);
			return;
		}

		if (p->efx == 3 || p->efx == 5) // 3xx or 5xx
		{
			if (p->efx != 5 && p->efxData != 0)
				ch->portamentoSpeed = p->efxData * 4;

			preparePortamento(inst, ch, p, instNum);
			handleEffects_TickZero(inst, ch);
			return;
		}

		if (p->efx == 0x14 && p->efxData == 0) // K00
		{
			keyOff(inst, ch);
			if (instNum)
				resetVolumes(ch);
			handleEffects_TickZero(inst, ch);
			return;
		}

		if (p->note == 0)
		{
			if (instNum > 0)
			{
				resetVolumes(ch);
				triggerInstrument(ch);
			}
			handleEffects_TickZero(inst, ch);
			return;
		}
	}

	if (p->note == FT2_NOTE_OFF)
		keyOff(inst, ch);
	else
		triggerNote(inst, p->note, p->efx, p->efxData, ch);

	if (instNum > 0)
	{
		resetVolumes(ch);
		if (p->note != FT2_NOTE_OFF)
			triggerInstrument(ch);
	}

	handleEffects_TickZero(inst, ch);
}

/* -------------------------------------------------------------------------
 * Get next position
 * ------------------------------------------------------------------------- */

static void getNextPos(ft2_instance_t *inst)
{
	ft2_song_t *s = &inst->replayer.song;
	ft2_replayer_state_t *rep = &inst->replayer;

	if (s->tick != 1)
		return;

	s->row++;

	if (s->pattDelTime > 0)
	{
		s->pattDelTime2 = s->pattDelTime;
		s->pattDelTime = 0;
	}

	if (s->pattDelTime2 > 0)
	{
		s->pattDelTime2--;
		if (s->pattDelTime2 > 0)
			s->row--;
	}

	if (s->pBreakFlag)
	{
		s->pBreakFlag = false;
		s->row = s->pBreakPos;
	}

	if (s->row >= s->currNumRows || s->posJumpFlag)
	{
		s->row = s->pBreakPos;
		s->pBreakPos = 0;
		s->posJumpFlag = false;

		if (rep->playMode != FT2_PLAYMODE_PATT && rep->playMode != FT2_PLAYMODE_RECPATT)
		{
			if (rep->bxxOverflow)
			{
				s->songPos = 0;
				rep->bxxOverflow = false;
			}
			else if (++s->songPos >= s->songLength)
			{
				s->songPos = s->songLoopStart;
			}

			s->pattNum = s->orders[s->songPos & 0xFF];
			s->currNumRows = rep->patternNumRows[s->pattNum & 0xFF];

			/* Sync editor state with song position (matches standalone behavior) */
			inst->editor.editPattern = (uint8_t)s->pattNum;
			inst->editor.songPos = s->songPos;
			inst->uiState.updatePosSections = true;
		}

		if (s->row >= s->currNumRows)
			s->row = 0;
	}
}

/* -------------------------------------------------------------------------
 * Main replayer tick
 * ------------------------------------------------------------------------- */

void ft2_replayer_tick(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_replayer_state_t *rep = &inst->replayer;
	ft2_song_t *s = &rep->song;

	if (!rep->songPlaying)
	{
		for (int32_t i = 0; i < s->numChannels; i++)
			updateVolPanAutoVib(inst, &rep->channel[i]);
		return;
	}

	/* Update playback time counter (hh:mm:ss) */
	if (s->BPM >= FT2_MIN_BPM && s->BPM <= FT2_MAX_BPM)
	{
		s->playbackSecondsFrac += songTickDuration35fp[s->BPM - FT2_MIN_BPM];
		if (s->playbackSecondsFrac >= 1ULL << 35)
		{
			s->playbackSecondsFrac &= (1ULL << 35) - 1;
			s->playbackSeconds++;
		}
	}

	bool tickZero = false;
	s->tick--;
	if (s->tick == 0)
	{
		s->tick = s->speed;
		tickZero = true;
	}

	const bool readNewNote = tickZero && (s->pattDelTime2 == 0);

	if (readNewNote)
	{
		s->curReplayerRow = (uint8_t)s->row;
		s->curReplayerPattNum = (uint8_t)s->pattNum;
		s->curReplayerSongPos = (uint8_t)s->songPos;

		ft2_note_t *patternPtr = rep->nilPatternLine;
		if (rep->pattern[s->pattNum] != NULL)
			patternPtr = &rep->pattern[s->pattNum][s->row * FT2_MAX_CHANNELS];

		for (int32_t i = 0; i < s->numChannels; i++)
		{
			ft2_channel_t *ch = &rep->channel[i];
			getNewNote(inst, ch, &patternPtr[i]);
			updateVolPanAutoVib(inst, ch);
		}
	}
	else
	{
		for (int32_t i = 0; i < s->numChannels; i++)
		{
			ft2_channel_t *ch = &rep->channel[i];
			handleEffects_TickNonZero(inst, ch);
			updateVolPanAutoVib(inst, ch);
		}
	}

	getNextPos(inst);

	/* Sync editor row with song row for pattern editor display */
	if (inst->editor.row != (uint8_t)s->row)
	{
		inst->editor.row = (uint8_t)s->row;
		inst->uiState.updatePatternEditor = true;
	}
}

/* -------------------------------------------------------------------------
 * Voice update (channel -> voice)
 * ------------------------------------------------------------------------- */

void ft2_update_voices(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_replayer_state_t *rep = &inst->replayer;

	for (int32_t i = 0; i < rep->song.numChannels; i++)
	{
		ft2_channel_t *ch = &rep->channel[i];
		ft2_voice_t *v = &inst->voice[i];

		const uint8_t status = ch->status;
		if (status == 0)
			continue;

		ch->status = 0;

		if (ch->channelOff || ch->mute)
			continue;

		if (status & FT2_CS_UPDATE_VOL)
			v->fVolume = ch->fFinalVol;

		if (status & FT2_CS_UPDATE_PAN)
			v->panning = ch->finalPan;

		if (status & (FT2_CS_UPDATE_VOL | FT2_CS_UPDATE_PAN))
			ft2_voice_update_volumes(inst, i, status);

		if (status & FT2_CF_UPDATE_PERIOD)
		{
			v->delta = ft2_period_to_delta(inst, ch->finalPeriod);
			updateVoiceSincLUT(inst, v);
		}

		if (status & FT2_CS_TRIGGER_VOICE)
			ft2_trigger_voice(inst, i, ch->smpPtr, ch->smpStartPos);

		/* Push scope sync entry for UI thread */
		if (status & (FT2_CS_UPDATE_VOL | FT2_CF_UPDATE_PERIOD | FT2_CS_TRIGGER_VOICE))
		{
			ft2_scope_sync_entry_t syncEntry = {0};
			syncEntry.channel = (uint8_t)i;
			syncEntry.status = status & (FT2_CS_UPDATE_VOL | FT2_CF_UPDATE_PERIOD | FT2_CS_TRIGGER_VOICE);
			
			if (status & FT2_CS_UPDATE_VOL)
				syncEntry.scopeVolume = (uint8_t)((ch->fFinalVol * (SCOPE_HEIGHT * 4.0f)) + 0.5f);
			
			if (status & FT2_CF_UPDATE_PERIOD)
				syncEntry.period = ch->finalPeriod;
			
			if (status & FT2_CS_TRIGGER_VOICE)
			{
				ft2_sample_t *smp = ch->smpPtr;
				if (smp != NULL && smp->dataPtr != NULL)
				{
					syncEntry.base8 = smp->dataPtr;
					syncEntry.base16 = (smp->flags & FT2_SAMPLE_16BIT) ? (const int16_t *)smp->dataPtr : NULL;
					syncEntry.length = smp->length;
					syncEntry.loopStart = smp->loopStart;
					syncEntry.loopLength = smp->loopLength;
					syncEntry.loopType = smp->flags & (FT2_LOOP_FWD | FT2_LOOP_BIDI);
					syncEntry.sample16Bit = !!(smp->flags & FT2_SAMPLE_16BIT);
					syncEntry.smpStartPos = ch->smpStartPos;
				}
			}
			
			ft2_scope_sync_queue_push(inst, &syncEntry);
		}
	}
}

/* -------------------------------------------------------------------------
 * Voice mixing
 * ------------------------------------------------------------------------- */

/* Interpolation helpers */

/* Linear interpolation (2-point) */
static inline float linearInterp8(const int8_t *s, uint64_t frac)
{
	const float f = (float)((uint32_t)(frac) >> 1) * (1.0f / 2147483648.0f);
	return ((float)s[0] + ((float)(s[1] - s[0]) * f)) * (1.0f / 128.0f);
}

static inline float linearInterp16(const int16_t *s, uint64_t frac)
{
	const float f = (float)((uint32_t)(frac) >> 1) * (1.0f / 2147483648.0f);
	return ((float)s[0] + ((float)(s[1] - s[0]) * f)) * (1.0f / 32768.0f);
}

/* Quadratic spline interpolation (3-point) */
static inline float quadraticInterp8(const int8_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> QUADRATIC_SPLINE_FRACSHIFT) * QUADRATIC_SPLINE_WIDTH);
	return ((s[0] * t[0]) + (s[1] * t[1]) + (s[2] * t[2])) * (1.0f / 128.0f);
}

static inline float quadraticInterp16(const int16_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> QUADRATIC_SPLINE_FRACSHIFT) * QUADRATIC_SPLINE_WIDTH);
	return ((s[0] * t[0]) + (s[1] * t[1]) + (s[2] * t[2])) * (1.0f / 32768.0f);
}

/* Cubic spline interpolation (4-point Catmull-Rom) */
static inline float cubicInterp8(const int8_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> CUBIC_SPLINE_FRACSHIFT) & CUBIC_SPLINE_FRACMASK);
	return ((s[-1] * t[0]) + (s[0] * t[1]) + (s[1] * t[2]) + (s[2] * t[3])) * (1.0f / 128.0f);
}

static inline float cubicInterp16(const int16_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> CUBIC_SPLINE_FRACSHIFT) & CUBIC_SPLINE_FRACMASK);
	return ((s[-1] * t[0]) + (s[0] * t[1]) + (s[1] * t[2]) + (s[2] * t[3])) * (1.0f / 32768.0f);
}

/* 8-point windowed sinc interpolation */
static inline float sinc8Interp8(const int8_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> SINC8_FRACSHIFT) & SINC8_FRACMASK);
	return ((s[-3] * t[0]) + (s[-2] * t[1]) + (s[-1] * t[2]) + (s[0] * t[3]) +
	        (s[1] * t[4]) + (s[2] * t[5]) + (s[3] * t[6]) + (s[4] * t[7])) * (1.0f / 128.0f);
}

static inline float sinc8Interp16(const int16_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> SINC8_FRACSHIFT) & SINC8_FRACMASK);
	return ((s[-3] * t[0]) + (s[-2] * t[1]) + (s[-1] * t[2]) + (s[0] * t[3]) +
	        (s[1] * t[4]) + (s[2] * t[5]) + (s[3] * t[6]) + (s[4] * t[7])) * (1.0f / 32768.0f);
}

/* 16-point windowed sinc interpolation */
static inline float sinc16Interp8(const int8_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> SINC16_FRACSHIFT) & SINC16_FRACMASK);
	return ((s[-7] * t[0]) + (s[-6] * t[1]) + (s[-5] * t[2]) + (s[-4] * t[3]) +
	        (s[-3] * t[4]) + (s[-2] * t[5]) + (s[-1] * t[6]) + (s[0] * t[7]) +
	        (s[1] * t[8]) + (s[2] * t[9]) + (s[3] * t[10]) + (s[4] * t[11]) +
	        (s[5] * t[12]) + (s[6] * t[13]) + (s[7] * t[14]) + (s[8] * t[15])) * (1.0f / 128.0f);
}

static inline float sinc16Interp16(const int16_t *s, uint64_t frac, const float *lut)
{
	const float *t = lut + (((uint32_t)(frac) >> SINC16_FRACSHIFT) & SINC16_FRACMASK);
	return ((s[-7] * t[0]) + (s[-6] * t[1]) + (s[-5] * t[2]) + (s[-4] * t[3]) +
	        (s[-3] * t[4]) + (s[-2] * t[5]) + (s[-1] * t[6]) + (s[0] * t[7]) +
	        (s[1] * t[8]) + (s[2] * t[9]) + (s[3] * t[10]) + (s[4] * t[11]) +
	        (s[5] * t[12]) + (s[6] * t[13]) + (s[7] * t[14]) + (s[8] * t[15])) * (1.0f / 32768.0f);
}

/* Sample with interpolation based on mode */
static inline float sampleInterp8(const int8_t *s, uint64_t frac, uint8_t mode, ft2_interp_tables_t *tables, const float *sincLUT)
{
	switch (mode)
	{
		case FT2_INTERP_LINEAR:
			return linearInterp8(s, frac);
		case FT2_INTERP_QUADRATIC:
			return quadraticInterp8(s, frac, tables->fQuadraticSplineLUT);
		case FT2_INTERP_CUBIC:
			return cubicInterp8(s, frac, tables->fCubicSplineLUT);
		case FT2_INTERP_SINC8:
			return sinc8Interp8(s, frac, sincLUT);
		case FT2_INTERP_SINC16:
			return sinc16Interp8(s, frac, sincLUT);
		default: /* FT2_INTERP_DISABLED */
			return s[0] * (1.0f / 128.0f);
	}
}

static inline float sampleInterp16(const int16_t *s, uint64_t frac, uint8_t mode, ft2_interp_tables_t *tables, const float *sincLUT)
{
	switch (mode)
	{
		case FT2_INTERP_LINEAR:
			return linearInterp16(s, frac);
		case FT2_INTERP_QUADRATIC:
			return quadraticInterp16(s, frac, tables->fQuadraticSplineLUT);
		case FT2_INTERP_CUBIC:
			return cubicInterp16(s, frac, tables->fCubicSplineLUT);
		case FT2_INTERP_SINC8:
			return sinc8Interp16(s, frac, sincLUT);
		case FT2_INTERP_SINC16:
			return sinc16Interp16(s, frac, sincLUT);
		default: /* FT2_INTERP_DISABLED */
			return s[0] * (1.0f / 32768.0f);
	}
}

static void mixVoice8BitNoLoop(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int8_t *base = v->base8;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	const uint64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		if (position >= v->sampleEnd)
		{
			v->active = false;
			break;
		}

		const float fSample = sampleInterp8(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
}

static void mixVoice16BitNoLoop(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int16_t *base = v->base16;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	const uint64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		if (position >= v->sampleEnd)
		{
			v->active = false;
			break;
		}

		const float fSample = sampleInterp16(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
}

static void mixVoice8BitLoop(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int8_t *base = v->base8;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	const uint64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const int32_t loopStart = v->loopStart;
	const int32_t loopEnd = loopStart + v->loopLength;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		while (position >= loopEnd)
		{
			position -= v->loopLength;
			v->hasLooped = true;
		}

		const float fSample = sampleInterp8(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
}

static void mixVoice16BitLoop(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int16_t *base = v->base16;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	const uint64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const int32_t loopStart = v->loopStart;
	const int32_t loopEnd = loopStart + v->loopLength;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		while (position >= loopEnd)
		{
			position -= v->loopLength;
			v->hasLooped = true;
		}

		const float fSample = sampleInterp16(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
}

static void mixVoice8BitBidi(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int8_t *base = v->base8;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	int64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const int32_t loopStart = v->loopStart;
	const int32_t loopEnd = loopStart + v->loopLength;
	bool backwards = v->samplingBackwards;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	if (backwards)
		delta = -delta;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		if (backwards)
		{
			while (position < loopStart)
			{
				position = loopStart + (loopStart - position);
				backwards = false;
				delta = -delta;
				v->hasLooped = true;
			}
		}
		else
		{
			while (position >= loopEnd)
			{
				position = loopEnd - 1 - (position - loopEnd);
				backwards = true;
				delta = -delta;
				v->hasLooped = true;
			}
		}

		const float fSample = sampleInterp8(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += (uint64_t)delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
	v->samplingBackwards = backwards;
}

static void mixVoice16BitBidi(ft2_instance_t *inst, ft2_voice_t *v, uint32_t numSamples)
{
	const int16_t *base = v->base16;
	int32_t position = v->position;
	uint64_t positionFrac = v->positionFrac;
	int64_t delta = v->delta;
	float *fMixBufferL = inst->audio.fMixBufferL;
	float *fMixBufferR = inst->audio.fMixBufferR;
	float fVolumeL = v->fCurrVolumeL;
	float fVolumeR = v->fCurrVolumeR;
	const float fVolLDelta = v->fVolumeLDelta;
	const float fVolRDelta = v->fVolumeRDelta;
	uint32_t rampLen = v->volumeRampLength;
	const int32_t loopStart = v->loopStart;
	const int32_t loopEnd = loopStart + v->loopLength;
	bool backwards = v->samplingBackwards;
	const uint8_t interpMode = inst->audio.interpolationType;
	ft2_interp_tables_t *tables = ft2_interp_tables_get();
	const float *sincLUT = v->fSincLUT;

	if (backwards)
		delta = -delta;

	for (uint32_t i = 0; i < numSamples; i++)
	{
		if (backwards)
		{
			while (position < loopStart)
			{
				position = loopStart + (loopStart - position);
				backwards = false;
				delta = -delta;
				v->hasLooped = true;
			}
		}
		else
		{
			while (position >= loopEnd)
			{
				position = loopEnd - 1 - (position - loopEnd);
				backwards = true;
				delta = -delta;
				v->hasLooped = true;
			}
		}

		const float fSample = sampleInterp16(&base[position], positionFrac, interpMode, tables, sincLUT);
		fMixBufferL[i] += fSample * fVolumeL;
		fMixBufferR[i] += fSample * fVolumeR;

		if (rampLen > 0)
		{
			fVolumeL += fVolLDelta;
			fVolumeR += fVolRDelta;
			rampLen--;
		}

		positionFrac += (uint64_t)delta;
		position += (int32_t)(positionFrac >> 32);
		positionFrac &= 0xFFFFFFFF;
	}

	v->position = position;
	v->positionFrac = positionFrac;
	v->fCurrVolumeL = fVolumeL;
	v->fCurrVolumeR = fVolumeR;
	v->volumeRampLength = rampLen;
	v->samplingBackwards = backwards;
}

/* Silence mix routine - advances sample position without mixing audio.
** This is called when a voice is active but has zero volume with no ramp.
** Matches standalone silenceMixRoutine() behavior. */
static void silenceMixRoutine(ft2_voice_t *v, int32_t numSamples)
{
	const uint64_t samplesToMix64 = (uint64_t)v->delta * (uint32_t)numSamples;
	const uint32_t samples = (uint32_t)(samplesToMix64 >> 32);
	const uint64_t samplesFrac = (samplesToMix64 & 0xFFFFFFFF) + v->positionFrac;

	uint32_t position = v->position + samples + (uint32_t)(samplesFrac >> 32);
	uint64_t positionFrac = samplesFrac & 0xFFFFFFFF;

	if (position < (uint32_t)v->sampleEnd) /* haven't reached end yet */
	{
		v->positionFrac = positionFrac;
		v->position = position;
		return;
	}

	/* End of sample (or loop) reached */
	if (v->loopType == FT2_LOOP_OFF)
	{
		v->active = false;
		return;
	}

	if (v->loopType == FT2_LOOP_FWD)
	{
		if (v->loopLength >= 2)
			position = v->loopStart + ((position - v->sampleEnd) % v->loopLength);
		else
			position = v->loopStart;

		v->hasLooped = true;
	}
	else /* FT2_LOOP_BIDI */
	{
		if (v->loopLength >= 2)
		{
			const uint32_t overflow = position - v->sampleEnd;
			const uint32_t cycles = overflow / v->loopLength;
			const uint32_t phase = overflow % v->loopLength;

			position = v->loopStart + phase;
			v->samplingBackwards = (cycles & 1) ? !v->samplingBackwards : v->samplingBackwards;
		}
		else
		{
			position = v->loopStart;
		}

		v->hasLooped = true;
	}

	v->positionFrac = positionFrac;
	v->position = position;
}

void ft2_mix_voices(ft2_instance_t *inst, int32_t bufferPos, int32_t samplesToMix)
{
	if (inst == NULL || samplesToMix <= 0)
		return;

	for (int32_t i = 0; i < inst->replayer.song.numChannels; i++)
	{
		ft2_voice_t *v = &inst->voice[i];
		if (!v->active)
			continue;

		const bool volRampFlag = (v->volumeRampLength > 0);
		if (!volRampFlag && v->fCurrVolumeL == 0.0f && v->fCurrVolumeR == 0.0f)
		{
			/* Voice is silent - advance position but don't mix */
			silenceMixRoutine(v, samplesToMix);
			continue;
		}

		const bool is16Bit = (v->base16 != NULL && v->base8 == NULL);

		switch (v->loopType)
		{
			case FT2_LOOP_OFF:
				if (is16Bit)
					mixVoice16BitNoLoop(inst, v, samplesToMix);
				else
					mixVoice8BitNoLoop(inst, v, samplesToMix);
				break;

			case FT2_LOOP_FWD:
				if (is16Bit)
					mixVoice16BitLoop(inst, v, samplesToMix);
				else
					mixVoice8BitLoop(inst, v, samplesToMix);
				break;

			case FT2_LOOP_BIDI:
				if (is16Bit)
					mixVoice16BitBidi(inst, v, samplesToMix);
				else
					mixVoice8BitBidi(inst, v, samplesToMix);
				break;
		}
	}

	/* Process fadeout voices */
	for (int32_t i = FT2_MAX_CHANNELS; i < FT2_MAX_CHANNELS * 2; i++)
	{
		ft2_voice_t *v = &inst->voice[i];
		if (!v->active)
			continue;

		if (v->volumeRampLength == 0)
		{
			v->active = false;
			continue;
		}

		const bool is16Bit = (v->base16 != NULL && v->base8 == NULL);

		switch (v->loopType)
		{
			case FT2_LOOP_OFF:
				if (is16Bit)
					mixVoice16BitNoLoop(inst, v, samplesToMix);
				else
					mixVoice8BitNoLoop(inst, v, samplesToMix);
				break;

			case FT2_LOOP_FWD:
				if (is16Bit)
					mixVoice16BitLoop(inst, v, samplesToMix);
				else
					mixVoice8BitLoop(inst, v, samplesToMix);
				break;

			case FT2_LOOP_BIDI:
				if (is16Bit)
					mixVoice16BitBidi(inst, v, samplesToMix);
				else
					mixVoice8BitBidi(inst, v, samplesToMix);
				break;
		}
	}
}

void ft2_mix_voices_multiout(ft2_instance_t *inst, int32_t bufferPos, int32_t samplesToMix)
{
	if (inst == NULL || samplesToMix <= 0 || !inst->audio.multiOutEnabled)
		return;

	/* Save original mix buffer pointers */
	float *origMixL = inst->audio.fMixBufferL;
	float *origMixR = inst->audio.fMixBufferR;

	/* Mix each channel's voices to routed output buffer */
	for (int32_t ch = 0; ch < inst->replayer.song.numChannels && ch < FT2_MAX_CHANNELS; ch++)
	{
		/* Route to configured output (0-15 -> Out 1-16) */
		int outIdx = inst->config.channelRouting[ch];
		if (outIdx >= FT2_NUM_OUTPUTS)
			outIdx = ch % FT2_NUM_OUTPUTS; /* Fallback for invalid config */

		/* Point mix buffers to routed output buffer at the correct offset */
		inst->audio.fMixBufferL = inst->audio.fChannelBufferL[outIdx] + bufferPos;
		inst->audio.fMixBufferR = inst->audio.fChannelBufferR[outIdx] + bufferPos;

		/* Mix main voice for this channel */
		ft2_voice_t *v = &inst->voice[ch];
		if (v->active)
		{
			const bool volRampFlag = (v->volumeRampLength > 0);
			if (volRampFlag || v->fCurrVolumeL != 0.0f || v->fCurrVolumeR != 0.0f)
			{
				const bool is16Bit = (v->base16 != NULL && v->base8 == NULL);

				switch (v->loopType)
				{
					case FT2_LOOP_OFF:
						if (is16Bit)
							mixVoice16BitNoLoop(inst, v, samplesToMix);
						else
							mixVoice8BitNoLoop(inst, v, samplesToMix);
						break;

					case FT2_LOOP_FWD:
						if (is16Bit)
							mixVoice16BitLoop(inst, v, samplesToMix);
						else
							mixVoice8BitLoop(inst, v, samplesToMix);
						break;

					case FT2_LOOP_BIDI:
						if (is16Bit)
							mixVoice16BitBidi(inst, v, samplesToMix);
						else
							mixVoice8BitBidi(inst, v, samplesToMix);
						break;
				}
			}
			else
			{
				silenceMixRoutine(v, samplesToMix);
			}
		}

		/* Mix fadeout voice for this channel */
		ft2_voice_t *fadeV = &inst->voice[FT2_MAX_CHANNELS + ch];
		if (fadeV->active)
		{
			if (fadeV->volumeRampLength == 0)
			{
				fadeV->active = false;
			}
			else
			{
				const bool is16Bit = (fadeV->base16 != NULL && fadeV->base8 == NULL);

				switch (fadeV->loopType)
				{
					case FT2_LOOP_OFF:
						if (is16Bit)
							mixVoice16BitNoLoop(inst, fadeV, samplesToMix);
						else
							mixVoice8BitNoLoop(inst, fadeV, samplesToMix);
						break;

					case FT2_LOOP_FWD:
						if (is16Bit)
							mixVoice16BitLoop(inst, fadeV, samplesToMix);
						else
							mixVoice8BitLoop(inst, fadeV, samplesToMix);
						break;

					case FT2_LOOP_BIDI:
						if (is16Bit)
							mixVoice16BitBidi(inst, fadeV, samplesToMix);
						else
							mixVoice8BitBidi(inst, fadeV, samplesToMix);
						break;
				}
			}
		}
	}

	/* Restore original mix buffer pointers */
	inst->audio.fMixBufferL = origMixL;
	inst->audio.fMixBufferR = origMixR;
}

/* -------------------------------------------------------------------------
 * Public wrappers for channel state functions (for keyjazz)
 * ------------------------------------------------------------------------- */

void ft2_channel_reset_volumes(ft2_channel_t *ch)
{
	resetVolumes(ch);
}

void ft2_channel_trigger_instrument(ft2_channel_t *ch)
{
	triggerInstrument(ch);
}

void ft2_channel_update_vol_pan_autovib(ft2_instance_t *inst, ft2_channel_t *ch)
{
	updateVolPanAutoVib(inst, ch);
}
