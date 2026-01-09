/**
 * @file ft2_plugin_instr_ed.h
 * @brief Instrument editor: vol/pan envelopes, piano, MIDI extension.
 *
 * Port of standalone ft2_inst_ed.c with instance-aware state.
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

/* Piano key dimensions */
#define PIANOKEY_WHITE_W 10
#define PIANOKEY_WHITE_H 46
#define PIANOKEY_BLACK_W 7
#define PIANOKEY_BLACK_H 29

/* Envelope area */
#define ENV_WIDTH      333
#define ENV_HEIGHT     67
#define VOL_ENV_Y      189
#define PAN_ENV_Y      276
#define ENV_MAX_POINTS 12

/* Envelope flags (bit flags) */
#define ENV_ENABLED (1 << 0)
#define ENV_SUSTAIN (1 << 1)
#define ENV_LOOP    (1 << 2)

/* Piano position */
#define PIANO_X       8
#define PIANO_Y       351
#define PIANO_OCTAVES 8

typedef struct ft2_instrument_editor_t {
	bool draggingVolEnv;
	bool draggingPanEnv;
	int32_t saveMouseX;      /* Drag offset from point center */
	int32_t saveMouseY;
	bool pianoKeyStatus[96]; /* Key pressed state */
	bool draggingPiano;      /* Click-drag sample assignment */
	int32_t lastMouseX;
	int32_t lastMouseY;
} ft2_instrument_editor_t;

/* Init/draw */
void ft2_instr_ed_init(ft2_instrument_editor_t *editor);
void ft2_instr_ed_draw(struct ft2_instance_t *inst);
void ft2_instr_ed_draw_vol_env(struct ft2_instance_t *inst);
void ft2_instr_ed_draw_pan_env(struct ft2_instance_t *inst);
void ft2_instr_ed_draw_envelope(struct ft2_instance_t *inst, int envNum);
void ft2_instr_ed_draw_note_map(struct ft2_instance_t *inst);
void ft2_instr_ed_draw_piano(struct ft2_instance_t *inst);
void updateInstEditor(struct ft2_instance_t *inst);

/* Mouse */
void ft2_instr_ed_mouse_click(struct ft2_instance_t *inst, int x, int y, int button);
void ft2_instr_ed_mouse_drag(struct ft2_instance_t *inst, int x, int y);
void ft2_instr_ed_mouse_up(struct ft2_instance_t *inst);

/* Visibility */
void showInstEditor(struct ft2_instance_t *inst);
void hideInstEditor(struct ft2_instance_t *inst);
void toggleInstEditor(struct ft2_instance_t *inst);
void exitInstEditor(struct ft2_instance_t *inst);

/* I.E.Ext (MIDI settings) */
void showInstEditorExt(struct ft2_instance_t *inst);
void hideInstEditorExt(struct ft2_instance_t *inst);
void toggleInstEditorExt(struct ft2_instance_t *inst);
void drawInstEditorExt(struct ft2_instance_t *inst);

/* Envelope presets (left=recall, right=store) */
void setStdVolEnvelope(struct ft2_instance_t *inst, uint8_t num);
void setStdPanEnvelope(struct ft2_instance_t *inst, uint8_t num);
void setOrStoreVolEnvPreset(struct ft2_instance_t *inst, uint8_t num);
void setOrStorePanEnvPreset(struct ft2_instance_t *inst, uint8_t num);

/* Preset buttons */
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

/* Envelope point/loop controls */
void pbVolEnvAdd(struct ft2_instance_t *inst);
void pbVolEnvDel(struct ft2_instance_t *inst);
void pbVolEnvSusUp(struct ft2_instance_t *inst);
void pbVolEnvSusDown(struct ft2_instance_t *inst);
void pbVolEnvRepSUp(struct ft2_instance_t *inst);
void pbVolEnvRepSDown(struct ft2_instance_t *inst);
void pbVolEnvRepEUp(struct ft2_instance_t *inst);
void pbVolEnvRepEDown(struct ft2_instance_t *inst);
void pbPanEnvAdd(struct ft2_instance_t *inst);
void pbPanEnvDel(struct ft2_instance_t *inst);
void pbPanEnvSusUp(struct ft2_instance_t *inst);
void pbPanEnvSusDown(struct ft2_instance_t *inst);
void pbPanEnvRepSUp(struct ft2_instance_t *inst);
void pbPanEnvRepSDown(struct ft2_instance_t *inst);
void pbPanEnvRepEUp(struct ft2_instance_t *inst);
void pbPanEnvRepEDown(struct ft2_instance_t *inst);

/* Sample/instrument value buttons */
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
void pbInstExit(struct ft2_instance_t *inst);

/* I.E.Ext MIDI buttons */
void pbInstExtMidiChDown(struct ft2_instance_t *inst);
void pbInstExtMidiChUp(struct ft2_instance_t *inst);
void pbInstExtMidiPrgDown(struct ft2_instance_t *inst);
void pbInstExtMidiPrgUp(struct ft2_instance_t *inst);
void pbInstExtMidiBendDown(struct ft2_instance_t *inst);
void pbInstExtMidiBendUp(struct ft2_instance_t *inst);

/* Scrollbar callbacks */
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

/* Checkbox callbacks */
void cbInstVEnv(struct ft2_instance_t *inst);
void cbInstVEnvSus(struct ft2_instance_t *inst);
void cbInstVEnvLoop(struct ft2_instance_t *inst);
void cbInstPEnv(struct ft2_instance_t *inst);
void cbInstPEnvSus(struct ft2_instance_t *inst);
void cbInstPEnvLoop(struct ft2_instance_t *inst);
void cbInstExtMidi(struct ft2_instance_t *inst);
void cbInstExtMute(struct ft2_instance_t *inst);

/* Vibrato waveform radio buttons */
void rbInstWaveSine(struct ft2_instance_t *inst);
void rbInstWaveSquare(struct ft2_instance_t *inst);
void rbInstWaveRampDown(struct ft2_instance_t *inst);
void rbInstWaveRampUp(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
