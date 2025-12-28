/**
 * @file ft2_plugin_callbacks.h
 * @brief Widget callback functions for the FT2 plugin.
 * 
 * These functions are called by the widget system when buttons are clicked,
 * scrollbars are moved, etc.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;

/**
 * Initialize all callback pointers in the pushButtons, scrollBars, etc. arrays.
 * Must be called after the widget arrays are initialized.
 */
void initCallbacks(void);

/* ========== POSITION EDITOR CALLBACKS ========== */
void pbPosEdPosUp(struct ft2_instance_t *inst);
void pbPosEdPosDown(struct ft2_instance_t *inst);
void pbPosEdIns(struct ft2_instance_t *inst);
void pbPosEdDel(struct ft2_instance_t *inst);
void pbPosEdPattUp(struct ft2_instance_t *inst);
void pbPosEdPattDown(struct ft2_instance_t *inst);
void pbPosEdLenUp(struct ft2_instance_t *inst);
void pbPosEdLenDown(struct ft2_instance_t *inst);
void pbPosEdRepUp(struct ft2_instance_t *inst);
void pbPosEdRepDown(struct ft2_instance_t *inst);

/* ========== SONG/PATTERN CALLBACKS ========== */
void pbBPMUp(struct ft2_instance_t *inst);
void pbBPMDown(struct ft2_instance_t *inst);
void pbSpeedUp(struct ft2_instance_t *inst);
void pbSpeedDown(struct ft2_instance_t *inst);
void pbEditAddUp(struct ft2_instance_t *inst);
void pbEditAddDown(struct ft2_instance_t *inst);
void pbPattUp(struct ft2_instance_t *inst);
void pbPattDown(struct ft2_instance_t *inst);
void pbPattLenUp(struct ft2_instance_t *inst);
void pbPattLenDown(struct ft2_instance_t *inst);
void pbPattExpand(struct ft2_instance_t *inst);
void pbPattShrink(struct ft2_instance_t *inst);

/* ========== PLAYBACK CALLBACKS ========== */
void pbPlaySong(struct ft2_instance_t *inst);
void pbPlayPatt(struct ft2_instance_t *inst);
void pbStop(struct ft2_instance_t *inst);
void pbRecordSong(struct ft2_instance_t *inst);
void pbRecordPatt(struct ft2_instance_t *inst);

/* ========== MENU CALLBACKS ========== */
void pbDiskOp(struct ft2_instance_t *inst);
void pbInstEd(struct ft2_instance_t *inst);
void pbSmpEd(struct ft2_instance_t *inst);
void pbConfig(struct ft2_instance_t *inst);
void pbConfigExit(struct ft2_instance_t *inst);
void pbHelp(struct ft2_instance_t *inst);
void pbHelpExit(struct ft2_instance_t *inst);
void pbAbout(struct ft2_instance_t *inst);
void pbExitAbout(struct ft2_instance_t *inst);

/* Help screen radio button callbacks (wrappers that get video/bmp context) */
void cbHelpFeatures(struct ft2_instance_t *inst);
void cbHelpEffects(struct ft2_instance_t *inst);
void cbHelpKeybindings(struct ft2_instance_t *inst);
void cbHelpHowToUseFT2(struct ft2_instance_t *inst);
void cbHelpFAQ(struct ft2_instance_t *inst);
void cbHelpKnownBugs(struct ft2_instance_t *inst);

/* Help screen scroll callbacks */
void pbHelpScrollUp(struct ft2_instance_t *inst);
void pbHelpScrollDown(struct ft2_instance_t *inst);
void sbHelpScroll(struct ft2_instance_t *inst, uint32_t pos);
void pbNibbles(struct ft2_instance_t *inst);
void pbKill(struct ft2_instance_t *inst);
void pbTrim(struct ft2_instance_t *inst);
void pbExtendView(struct ft2_instance_t *inst);
void pbTranspose(struct ft2_instance_t *inst);

/* Transpose operation callbacks */
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

void pbInstEdExt(struct ft2_instance_t *inst);
void pbSmpEdExt(struct ft2_instance_t *inst);
void pbAdvEdit(struct ft2_instance_t *inst);
void pbAddChannels(struct ft2_instance_t *inst);
void pbSubChannels(struct ft2_instance_t *inst);

/* ========== LOGO/BADGE CALLBACKS ========== */
void pbLogo(struct ft2_instance_t *inst);
void pbBadge(struct ft2_instance_t *inst);

/* ========== INSTRUMENT SWITCHER CALLBACKS ========== */
void pbSwapInstrBank(struct ft2_instance_t *inst);
void pbSampleListUp(struct ft2_instance_t *inst);
void pbSampleListDown(struct ft2_instance_t *inst);

/* ========== CHANNEL SCROLL CALLBACKS ========== */
void pbChanScrollLeft(struct ft2_instance_t *inst);
void pbChanScrollRight(struct ft2_instance_t *inst);

/* Instrument range buttons */
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

/* ========== INSTRUMENT EDITOR CALLBACKS ========== */

/* Envelope presets */
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

/* Volume envelope controls */
void pbVolEnvAdd(struct ft2_instance_t *inst);
void pbVolEnvDel(struct ft2_instance_t *inst);
void pbVolEnvSusUp(struct ft2_instance_t *inst);
void pbVolEnvSusDown(struct ft2_instance_t *inst);
void pbVolEnvRepSUp(struct ft2_instance_t *inst);
void pbVolEnvRepSDown(struct ft2_instance_t *inst);
void pbVolEnvRepEUp(struct ft2_instance_t *inst);
void pbVolEnvRepEDown(struct ft2_instance_t *inst);

/* Pan envelope controls */
void pbPanEnvAdd(struct ft2_instance_t *inst);
void pbPanEnvDel(struct ft2_instance_t *inst);
void pbPanEnvSusUp(struct ft2_instance_t *inst);
void pbPanEnvSusDown(struct ft2_instance_t *inst);
void pbPanEnvRepSUp(struct ft2_instance_t *inst);
void pbPanEnvRepSDown(struct ft2_instance_t *inst);
void pbPanEnvRepEUp(struct ft2_instance_t *inst);
void pbPanEnvRepEDown(struct ft2_instance_t *inst);

/* Sample parameter buttons */
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

/* Relative note */
void pbInstOctUp(struct ft2_instance_t *inst);
void pbInstOctDown(struct ft2_instance_t *inst);
void pbInstHalftoneUp(struct ft2_instance_t *inst);
void pbInstHalftoneDown(struct ft2_instance_t *inst);

/* Exit */
void pbInstExit(struct ft2_instance_t *inst);

/* ========== INSTRUMENT EDITOR EXTENSION CALLBACKS ========== */
void pbInstExtMidiChDown(struct ft2_instance_t *inst);
void pbInstExtMidiChUp(struct ft2_instance_t *inst);
void pbInstExtMidiPrgDown(struct ft2_instance_t *inst);
void pbInstExtMidiPrgUp(struct ft2_instance_t *inst);
void pbInstExtMidiBendDown(struct ft2_instance_t *inst);
void pbInstExtMidiBendUp(struct ft2_instance_t *inst);

/* ========== SAMPLE EDITOR CALLBACKS ========== */
void pbSampScrollLeft(struct ft2_instance_t *inst);
void pbSampScrollRight(struct ft2_instance_t *inst);
void pbSampPNoteUp(struct ft2_instance_t *inst);
void pbSampPNoteDown(struct ft2_instance_t *inst);
void pbSampStop(struct ft2_instance_t *inst);
void pbSampPlayWave(struct ft2_instance_t *inst);
void pbSampPlayRange(struct ft2_instance_t *inst);
void pbSampPlayDisplay(struct ft2_instance_t *inst);
void pbSampShowRange(struct ft2_instance_t *inst);
void pbSampRangeAll(struct ft2_instance_t *inst);
void pbSampClrRange(struct ft2_instance_t *inst);
void pbSampZoomOut(struct ft2_instance_t *inst);
void pbSampShowAll(struct ft2_instance_t *inst);
void pbSampSaveRng(struct ft2_instance_t *inst);
void pbSampCut(struct ft2_instance_t *inst);
void pbSampCopy(struct ft2_instance_t *inst);
void pbSampPaste(struct ft2_instance_t *inst);
void pbSampCrop(struct ft2_instance_t *inst);
void pbSampVolume(struct ft2_instance_t *inst);
void pbSampEffects(struct ft2_instance_t *inst);
void pbSampExit(struct ft2_instance_t *inst);
void pbSampClear(struct ft2_instance_t *inst);
void pbSampMin(struct ft2_instance_t *inst);

/* ========== SAMPLE EDITOR EFFECTS CALLBACKS ========== */
void pbSampFxCyclesUp(struct ft2_instance_t *inst);
void pbSampFxCyclesDown(struct ft2_instance_t *inst);
void pbSampFxTriangle(struct ft2_instance_t *inst);
void pbSampFxSaw(struct ft2_instance_t *inst);
void pbSampFxSine(struct ft2_instance_t *inst);
void pbSampFxSquare(struct ft2_instance_t *inst);
void pbSampFxResoUp(struct ft2_instance_t *inst);
void pbSampFxResoDown(struct ft2_instance_t *inst);
void pbSampFxLowPass(struct ft2_instance_t *inst);
void pbSampFxHighPass(struct ft2_instance_t *inst);
void pbSampFxSubBass(struct ft2_instance_t *inst);
void pbSampFxSubTreble(struct ft2_instance_t *inst);
void pbSampFxAddBass(struct ft2_instance_t *inst);
void pbSampFxAddTreble(struct ft2_instance_t *inst);
void pbSampFxSetAmp(struct ft2_instance_t *inst);
void pbSampFxUndo(struct ft2_instance_t *inst);
void pbSampFxXFade(struct ft2_instance_t *inst);
void pbSampFxBack(struct ft2_instance_t *inst);
void cbSampFxNorm(struct ft2_instance_t *inst);

/* ========== SAMPLE EDITOR EXTENSION CALLBACKS ========== */
void pbSampExtClearCopyBuf(struct ft2_instance_t *inst);
void pbSampExtSign(struct ft2_instance_t *inst);
void pbSampExtEcho(struct ft2_instance_t *inst);
void pbSampExtBackwards(struct ft2_instance_t *inst);
void pbSampExtByteSwap(struct ft2_instance_t *inst);
void pbSampExtFixDC(struct ft2_instance_t *inst);
void pbSampExtCopyIns(struct ft2_instance_t *inst);
void pbSampExtCopySmp(struct ft2_instance_t *inst);
void pbSampExtXchgIns(struct ft2_instance_t *inst);
void pbSampExtXchgSmp(struct ft2_instance_t *inst);
void pbSampExtResample(struct ft2_instance_t *inst);
void pbSampExtMixSample(struct ft2_instance_t *inst);

/* ========== DISK OP CALLBACKS ========== */
void pbDiskOpSave(struct ft2_instance_t *inst);
void pbDiskOpDelete(struct ft2_instance_t *inst);
void pbDiskOpRename(struct ft2_instance_t *inst);
void pbDiskOpMakeDir(struct ft2_instance_t *inst);
void pbDiskOpRefresh(struct ft2_instance_t *inst);
void pbDiskOpSetPath(struct ft2_instance_t *inst);
void pbDiskOpShowAll(struct ft2_instance_t *inst);
void pbDiskOpExit(struct ft2_instance_t *inst);
void pbDiskOpRoot(struct ft2_instance_t *inst);
void pbDiskOpParent(struct ft2_instance_t *inst);
void pbDiskOpHome(struct ft2_instance_t *inst);
void pbDiskOpListUp(struct ft2_instance_t *inst);
void pbDiskOpListDown(struct ft2_instance_t *inst);

/* ========== SCROLLBAR CALLBACKS ========== */
void sbPosEd(struct ft2_instance_t *inst, uint32_t pos);
void sbSampleList(struct ft2_instance_t *inst, uint32_t pos);
void sbInstVol(struct ft2_instance_t *inst, uint32_t pos);
void sbInstPan(struct ft2_instance_t *inst, uint32_t pos);
void sbInstFtune(struct ft2_instance_t *inst, uint32_t pos);
void sbInstFadeout(struct ft2_instance_t *inst, uint32_t pos);
void sbInstVibSpeed(struct ft2_instance_t *inst, uint32_t pos);
void sbInstVibDepth(struct ft2_instance_t *inst, uint32_t pos);
void sbInstVibSweep(struct ft2_instance_t *inst, uint32_t pos);
void sbInstExtMidiCh(struct ft2_instance_t *inst, uint32_t pos);
void sbInstExtMidiPrg(struct ft2_instance_t *inst, uint32_t pos);
void sbInstExtMidiBend(struct ft2_instance_t *inst, uint32_t pos);
void sbSampScroll(struct ft2_instance_t *inst, uint32_t pos);

/* ========== CHECKBOX CALLBACKS ========== */
void cbInstVEnv(struct ft2_instance_t *inst);
void cbInstVEnvSus(struct ft2_instance_t *inst);
void cbInstVEnvLoop(struct ft2_instance_t *inst);
void cbInstPEnv(struct ft2_instance_t *inst);
void cbInstPEnvSus(struct ft2_instance_t *inst);
void cbInstPEnvLoop(struct ft2_instance_t *inst);
void cbInstExtMidi(struct ft2_instance_t *inst);
void cbInstExtMute(struct ft2_instance_t *inst);

/* ========== RADIO BUTTON CALLBACKS ========== */
void rbInstWaveSine(struct ft2_instance_t *inst);
void rbInstWaveSquare(struct ft2_instance_t *inst);
void rbInstWaveRampDown(struct ft2_instance_t *inst);
void rbInstWaveRampUp(struct ft2_instance_t *inst);
void rbSampNoLoop(struct ft2_instance_t *inst);
void rbSampForwardLoop(struct ft2_instance_t *inst);
void rbSampPingPongLoop(struct ft2_instance_t *inst);
void rbSamp8Bit(struct ft2_instance_t *inst);
void rbSamp16Bit(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
