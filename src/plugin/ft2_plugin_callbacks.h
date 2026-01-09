/**
 * @file ft2_plugin_callbacks.h
 * @brief Widget callback declarations.
 *
 * Naming conventions:
 *   pb* = push button callback
 *   sb* = scrollbar callback
 *   rb* = radio button callback
 *   cb* = checkbox callback
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_widgets_t;

/* Wire all widget callbacks. Call after widget arrays are initialized. */
void initCallbacks(struct ft2_widgets_t *widgets);

/* Position editor: song position, pattern number, song length, loop point */
void pbPosEdPosUp(struct ft2_instance_t *inst);
void pbPosEdPosDown(struct ft2_instance_t *inst);
void pbPosEdIns(struct ft2_instance_t *inst);    /* Insert order entry */
void pbPosEdDel(struct ft2_instance_t *inst);    /* Delete order entry */
void pbPosEdPattUp(struct ft2_instance_t *inst);
void pbPosEdPattDown(struct ft2_instance_t *inst);
void pbPosEdLenUp(struct ft2_instance_t *inst);
void pbPosEdLenDown(struct ft2_instance_t *inst);
void pbPosEdRepUp(struct ft2_instance_t *inst);  /* Loop start position */
void pbPosEdRepDown(struct ft2_instance_t *inst);

/* Song/pattern controls: tempo, speed, pattern selection */
void pbBPMUp(struct ft2_instance_t *inst);
void pbBPMDown(struct ft2_instance_t *inst);
void pbSpeedUp(struct ft2_instance_t *inst);     /* Ticks per row (or toggle 3/6 if locked) */
void pbSpeedDown(struct ft2_instance_t *inst);
void pbEditAddUp(struct ft2_instance_t *inst);   /* Row skip after note entry */
void pbEditAddDown(struct ft2_instance_t *inst);
void pbPattUp(struct ft2_instance_t *inst);
void pbPattDown(struct ft2_instance_t *inst);
void pbPattLenUp(struct ft2_instance_t *inst);
void pbPattLenDown(struct ft2_instance_t *inst);
void pbPattExpand(struct ft2_instance_t *inst);  /* Double pattern length, space out notes */
void pbPattShrink(struct ft2_instance_t *inst);  /* Halve pattern length, merge rows */

/* Playback controls */
void pbPlaySong(struct ft2_instance_t *inst);    /* Play from current position */
void pbPlayPatt(struct ft2_instance_t *inst);    /* Play current pattern only */
void pbStop(struct ft2_instance_t *inst);
void pbRecordSong(struct ft2_instance_t *inst);  /* Play + record mode */
void pbRecordPatt(struct ft2_instance_t *inst);

/* Screen toggles */
void pbDiskOp(struct ft2_instance_t *inst);
void pbInstEd(struct ft2_instance_t *inst);
void pbSmpEd(struct ft2_instance_t *inst);
void pbConfig(struct ft2_instance_t *inst);
void pbConfigExit(struct ft2_instance_t *inst);
void pbHelp(struct ft2_instance_t *inst);
void pbHelpExit(struct ft2_instance_t *inst);
void pbAbout(struct ft2_instance_t *inst);
void pbExitAbout(struct ft2_instance_t *inst);

/* Help screen: tab selection radio buttons */
void cbHelpFeatures(struct ft2_instance_t *inst);
void cbHelpEffects(struct ft2_instance_t *inst);
void cbHelpKeybindings(struct ft2_instance_t *inst);
void cbHelpHowToUseFT2(struct ft2_instance_t *inst);
void cbHelpPlugin(struct ft2_instance_t *inst);

/* Help screen scroll */
void pbHelpScrollUp(struct ft2_instance_t *inst);
void pbHelpScrollDown(struct ft2_instance_t *inst);
void sbHelpScroll(struct ft2_instance_t *inst, uint32_t pos);

/* Misc screens and tools */
void pbNibbles(struct ft2_instance_t *inst);     /* Snake game */
void pbKill(struct ft2_instance_t *inst);        /* Zap song/instruments dialog */
void pbTrim(struct ft2_instance_t *inst);        /* Remove unused data */
void pbExtendView(struct ft2_instance_t *inst);  /* Extended pattern editor */
void pbExitExtPatt(struct ft2_instance_t *inst);
void pbTranspose(struct ft2_instance_t *inst);   /* Transpose dialog */

/*
 * Transpose operations: [scope][instrument filter][semitones]
 *   Scope: Track, Patt, Song, Block
 *   Filter: CurIns (current instrument only), AllIns (all instruments)
 *   Semitones: Up/Dn (+/-1), 12Up/12Dn (+/-12 octave)
 */
void pbTrackTranspCurInsUp(struct ft2_instance_t *inst);
void pbTrackTranspCurInsDn(struct ft2_instance_t *inst);
void pbTrackTranspCurIns12Up(struct ft2_instance_t *inst);
void pbTrackTranspCurIns12Dn(struct ft2_instance_t *inst);
void pbTrackTranspAllInsUp(struct ft2_instance_t *inst);
void pbTrackTranspAllInsDn(struct ft2_instance_t *inst);
void pbTrackTranspAllIns12Up(struct ft2_instance_t *inst);
void pbTrackTranspAllIns12Dn(struct ft2_instance_t *inst);
void pbPattTranspCurInsUp(struct ft2_instance_t *inst);
void pbPattTranspCurInsDn(struct ft2_instance_t *inst);
void pbPattTranspCurIns12Up(struct ft2_instance_t *inst);
void pbPattTranspCurIns12Dn(struct ft2_instance_t *inst);
void pbPattTranspAllInsUp(struct ft2_instance_t *inst);
void pbPattTranspAllInsDn(struct ft2_instance_t *inst);
void pbPattTranspAllIns12Up(struct ft2_instance_t *inst);
void pbPattTranspAllIns12Dn(struct ft2_instance_t *inst);
void pbSongTranspCurInsUp(struct ft2_instance_t *inst);
void pbSongTranspCurInsDn(struct ft2_instance_t *inst);
void pbSongTranspCurIns12Up(struct ft2_instance_t *inst);
void pbSongTranspCurIns12Dn(struct ft2_instance_t *inst);
void pbSongTranspAllInsUp(struct ft2_instance_t *inst);
void pbSongTranspAllInsDn(struct ft2_instance_t *inst);
void pbSongTranspAllIns12Up(struct ft2_instance_t *inst);
void pbSongTranspAllIns12Dn(struct ft2_instance_t *inst);
void pbBlockTranspCurInsUp(struct ft2_instance_t *inst);
void pbBlockTranspCurInsDn(struct ft2_instance_t *inst);
void pbBlockTranspCurIns12Up(struct ft2_instance_t *inst);
void pbBlockTranspCurIns12Dn(struct ft2_instance_t *inst);
void pbBlockTranspAllInsUp(struct ft2_instance_t *inst);
void pbBlockTranspAllInsDn(struct ft2_instance_t *inst);
void pbBlockTranspAllIns12Up(struct ft2_instance_t *inst);
void pbBlockTranspAllIns12Dn(struct ft2_instance_t *inst);

/* Editor extension toggles */
void pbInstEdExt(struct ft2_instance_t *inst);   /* Instrument editor MIDI settings */
void pbSmpEdExt(struct ft2_instance_t *inst);    /* Sample editor extra tools */
void pbAdvEdit(struct ft2_instance_t *inst);     /* Advanced edit (remap instruments) */
void pbAddChannels(struct ft2_instance_t *inst); /* +2 channels (max 32) */
void pbSubChannels(struct ft2_instance_t *inst); /* -2 channels (min 2) */

/* Logo/badge: click to toggle style (easter egg) */
void pbLogo(struct ft2_instance_t *inst);
void pbBadge(struct ft2_instance_t *inst);

/* Instrument switcher: bank navigation */
void pbSwapInstrBank(struct ft2_instance_t *inst); /* Toggle instruments 1-64 / 65-128 */
void pbSampleListUp(struct ft2_instance_t *inst);
void pbSampleListDown(struct ft2_instance_t *inst);

/* Pattern editor channel scroll */
void pbChanScrollLeft(struct ft2_instance_t *inst);
void pbChanScrollRight(struct ft2_instance_t *inst);

/* Instrument bank buttons 1-16: select which 8-instrument group to display */
void pbRange1(struct ft2_instance_t *inst);
void pbRange2(struct ft2_instance_t *inst);
void pbRange3(struct ft2_instance_t *inst);
void pbRange4(struct ft2_instance_t *inst);
void pbRange5(struct ft2_instance_t *inst);
void pbRange6(struct ft2_instance_t *inst);
void pbRange7(struct ft2_instance_t *inst);
void pbRange8(struct ft2_instance_t *inst);
void pbRange9(struct ft2_instance_t *inst);
void pbRange10(struct ft2_instance_t *inst);
void pbRange11(struct ft2_instance_t *inst);
void pbRange12(struct ft2_instance_t *inst);
void pbRange13(struct ft2_instance_t *inst);
void pbRange14(struct ft2_instance_t *inst);
void pbRange15(struct ft2_instance_t *inst);
void pbRange16(struct ft2_instance_t *inst);

/* Instrument editor: envelope presets (1-6 for vol, 1-6 for pan) */
void pbVolPreDef1(struct ft2_instance_t *inst);
void pbVolPreDef2(struct ft2_instance_t *inst);
void pbVolPreDef3(struct ft2_instance_t *inst);
void pbVolPreDef4(struct ft2_instance_t *inst);
void pbVolPreDef5(struct ft2_instance_t *inst);
void pbVolPreDef6(struct ft2_instance_t *inst);
void pbPanPreDef1(struct ft2_instance_t *inst);
void pbPanPreDef2(struct ft2_instance_t *inst);
void pbPanPreDef3(struct ft2_instance_t *inst);
void pbPanPreDef4(struct ft2_instance_t *inst);
void pbPanPreDef5(struct ft2_instance_t *inst);
void pbPanPreDef6(struct ft2_instance_t *inst);

/* Volume envelope: point management, sustain, loop start/end */
void pbVolEnvAdd(struct ft2_instance_t *inst);
void pbVolEnvDel(struct ft2_instance_t *inst);
void pbVolEnvSusUp(struct ft2_instance_t *inst);
void pbVolEnvSusDown(struct ft2_instance_t *inst);
void pbVolEnvRepSUp(struct ft2_instance_t *inst);
void pbVolEnvRepSDown(struct ft2_instance_t *inst);
void pbVolEnvRepEUp(struct ft2_instance_t *inst);
void pbVolEnvRepEDown(struct ft2_instance_t *inst);

/* Pan envelope: point management, sustain, loop start/end */
void pbPanEnvAdd(struct ft2_instance_t *inst);
void pbPanEnvDel(struct ft2_instance_t *inst);
void pbPanEnvSusUp(struct ft2_instance_t *inst);
void pbPanEnvSusDown(struct ft2_instance_t *inst);
void pbPanEnvRepSUp(struct ft2_instance_t *inst);
void pbPanEnvRepSDown(struct ft2_instance_t *inst);
void pbPanEnvRepEUp(struct ft2_instance_t *inst);
void pbPanEnvRepEDown(struct ft2_instance_t *inst);

/* Sample parameters: vol, pan, finetune, fadeout, vibrato settings */
void pbInstVolDown(struct ft2_instance_t *inst);
void pbInstVolUp(struct ft2_instance_t *inst);
void pbInstPanDown(struct ft2_instance_t *inst);
void pbInstPanUp(struct ft2_instance_t *inst);
void pbInstFtuneDown(struct ft2_instance_t *inst);
void pbInstFtuneUp(struct ft2_instance_t *inst);
void pbInstFadeoutDown(struct ft2_instance_t *inst);
void pbInstFadeoutUp(struct ft2_instance_t *inst);
void pbInstVibSpeedDown(struct ft2_instance_t *inst);
void pbInstVibSpeedUp(struct ft2_instance_t *inst);
void pbInstVibDepthDown(struct ft2_instance_t *inst);
void pbInstVibDepthUp(struct ft2_instance_t *inst);
void pbInstVibSweepDown(struct ft2_instance_t *inst);
void pbInstVibSweepUp(struct ft2_instance_t *inst);

/* Relative note: octave and semitone offset for sample playback */
void pbInstOctUp(struct ft2_instance_t *inst);
void pbInstOctDown(struct ft2_instance_t *inst);
void pbInstHalftoneUp(struct ft2_instance_t *inst);
void pbInstHalftoneDown(struct ft2_instance_t *inst);

void pbInstExit(struct ft2_instance_t *inst);

/* Instrument editor extension: MIDI output settings */
void pbInstExtMidiChDown(struct ft2_instance_t *inst);
void pbInstExtMidiChUp(struct ft2_instance_t *inst);
void pbInstExtMidiPrgDown(struct ft2_instance_t *inst);
void pbInstExtMidiPrgUp(struct ft2_instance_t *inst);
void pbInstExtMidiBendDown(struct ft2_instance_t *inst);
void pbInstExtMidiBendUp(struct ft2_instance_t *inst);

/* Sample editor: navigation, playback, selection, editing */
void pbSampScrollLeft(struct ft2_instance_t *inst);
void pbSampScrollRight(struct ft2_instance_t *inst);
void pbSampPNoteUp(struct ft2_instance_t *inst);   /* Preview note selection */
void pbSampPNoteDown(struct ft2_instance_t *inst);
void pbSampStop(struct ft2_instance_t *inst);
void pbSampPlayWave(struct ft2_instance_t *inst);    /* Play entire sample */
void pbSampPlayRange(struct ft2_instance_t *inst);   /* Play selected range */
void pbSampPlayDisplay(struct ft2_instance_t *inst); /* Play visible portion */
void pbSampShowRange(struct ft2_instance_t *inst);   /* Zoom to selection */
void pbSampRangeAll(struct ft2_instance_t *inst);    /* Select all */
void pbSampClrRange(struct ft2_instance_t *inst);    /* Clear selection */
void pbSampZoomOut(struct ft2_instance_t *inst);
void pbSampShowAll(struct ft2_instance_t *inst);     /* Zoom to fit */
void pbSampSaveRng(struct ft2_instance_t *inst);     /* Save range to file */
void pbSampCut(struct ft2_instance_t *inst);
void pbSampCopy(struct ft2_instance_t *inst);
void pbSampPaste(struct ft2_instance_t *inst);
void pbSampCrop(struct ft2_instance_t *inst);        /* Delete outside selection */
void pbSampVolume(struct ft2_instance_t *inst);      /* Volume panel */
void pbSampEffects(struct ft2_instance_t *inst);     /* Effects panel */
void pbSampExit(struct ft2_instance_t *inst);
void pbSampClear(struct ft2_instance_t *inst);       /* Delete sample data */
void pbSampMin(struct ft2_instance_t *inst);         /* Minimize sample to loop */

/* Sample effects panel: waveform generation, filters, EQ */
void pbSampFxCyclesUp(struct ft2_instance_t *inst);   /* Waveform generator cycles */
void pbSampFxCyclesDown(struct ft2_instance_t *inst);
void pbSampFxTriangle(struct ft2_instance_t *inst);   /* Generate triangle wave */
void pbSampFxSaw(struct ft2_instance_t *inst);        /* Generate sawtooth wave */
void pbSampFxSine(struct ft2_instance_t *inst);       /* Generate sine wave */
void pbSampFxSquare(struct ft2_instance_t *inst);     /* Generate square wave */
void pbSampFxResoUp(struct ft2_instance_t *inst);     /* Filter resonance */
void pbSampFxResoDown(struct ft2_instance_t *inst);
void pbSampFxLowPass(struct ft2_instance_t *inst);    /* Apply low-pass filter */
void pbSampFxHighPass(struct ft2_instance_t *inst);   /* Apply high-pass filter */
void pbSampFxSubBass(struct ft2_instance_t *inst);    /* Reduce bass */
void pbSampFxSubTreble(struct ft2_instance_t *inst);  /* Reduce treble */
void pbSampFxAddBass(struct ft2_instance_t *inst);    /* Boost bass */
void pbSampFxAddTreble(struct ft2_instance_t *inst);  /* Boost treble */
void pbSampFxSetAmp(struct ft2_instance_t *inst);     /* Set amplification */
void pbSampFxUndo(struct ft2_instance_t *inst);       /* Revert last effect */
void pbSampFxXFade(struct ft2_instance_t *inst);      /* Crossfade loop point */
void pbSampFxBack(struct ft2_instance_t *inst);       /* Return to sample editor */
void cbSampFxNorm(struct ft2_instance_t *inst);       /* Normalize checkbox */

/* Sample editor extension: utilities, copy/exchange, processing */
void pbSampExtClearCopyBuf(struct ft2_instance_t *inst); /* Clear clipboard */
void pbSampExtSign(struct ft2_instance_t *inst);         /* Convert signed/unsigned */
void pbSampExtEcho(struct ft2_instance_t *inst);         /* Echo effect dialog */
void pbSampExtBackwards(struct ft2_instance_t *inst);    /* Reverse sample */
void pbSampExtByteSwap(struct ft2_instance_t *inst);     /* Swap byte order */
void pbSampExtFixDC(struct ft2_instance_t *inst);        /* Remove DC offset */
void pbSampExtCopyIns(struct ft2_instance_t *inst);      /* Copy instrument */
void pbSampExtCopySmp(struct ft2_instance_t *inst);      /* Copy sample */
void pbSampExtXchgIns(struct ft2_instance_t *inst);      /* Exchange instruments */
void pbSampExtXchgSmp(struct ft2_instance_t *inst);      /* Exchange samples */
void pbSampExtResample(struct ft2_instance_t *inst);     /* Resample dialog */
void pbSampExtMixSample(struct ft2_instance_t *inst);    /* Mix sample dialog */

/* Disk operations: file browser, save/load, navigation */
void pbDiskOpSave(struct ft2_instance_t *inst);
void pbDiskOpDelete(struct ft2_instance_t *inst);
void pbDiskOpRename(struct ft2_instance_t *inst);
void pbDiskOpMakeDir(struct ft2_instance_t *inst);
void pbDiskOpRefresh(struct ft2_instance_t *inst);
void pbDiskOpSetPath(struct ft2_instance_t *inst);
void pbDiskOpShowAll(struct ft2_instance_t *inst);   /* Toggle file filter */
void pbDiskOpExit(struct ft2_instance_t *inst);
void pbDiskOpRoot(struct ft2_instance_t *inst);      /* Go to filesystem root */
void pbDiskOpParent(struct ft2_instance_t *inst);    /* Go up one directory */
void pbDiskOpHome(struct ft2_instance_t *inst);      /* Go to user home */
void pbDiskOpListUp(struct ft2_instance_t *inst);
void pbDiskOpListDown(struct ft2_instance_t *inst);

/* Scrollbar callbacks: position editor, sample list, sample waveform */
void sbPosEd(struct ft2_instance_t *inst, uint32_t pos);
void sbSampleList(struct ft2_instance_t *inst, uint32_t pos);
void sbSampScroll(struct ft2_instance_t *inst, uint32_t pos);

#ifdef __cplusplus
}
#endif
