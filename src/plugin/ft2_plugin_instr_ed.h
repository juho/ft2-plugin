/**
 * @file ft2_plugin_instr_ed.h
 * @brief Exact port of instrument editor from ft2_inst_ed.c
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

/* Instrument editor constants - match original FT2 */
#define PIANOKEY_WHITE_W 10
#define PIANOKEY_WHITE_H 46
#define PIANOKEY_BLACK_W 7
#define PIANOKEY_BLACK_H 29

/* Envelope constants */
#define ENV_WIDTH 333
#define ENV_HEIGHT 67
#define VOL_ENV_Y 189
#define PAN_ENV_Y 276
#define ENV_MAX_POINTS 12

/* Envelope flags */
#define ENV_ENABLED  (1 << 0)
#define ENV_SUSTAIN  (1 << 1)
#define ENV_LOOP     (1 << 2)

/* Piano constants */
#define PIANO_X 8
#define PIANO_Y 351
#define PIANO_OCTAVES 8

typedef struct ft2_instrument_editor_t
{
	struct ft2_video_t *video;
	struct ft2_instance_t *instance;
	
	int16_t currInstr;
	int16_t currSmp;    /* Current sample index (0-15) within instrument */
	
	/* Envelope editing state */
	int8_t currVolEnvPoint;
	int8_t currPanEnvPoint;
	bool draggingVolEnv;
	bool draggingPanEnv;
	int32_t saveMouseX;   /* Offset from point center for precise dragging */
	int32_t saveMouseY;
	
	/* Piano state */
	bool pianoKeyStatus[96];
	bool draggingPiano;   /* For click-drag sample assignment */
	
	/* Mouse state */
	int32_t lastMouseX;
	int32_t lastMouseY;
} ft2_instrument_editor_t;

void ft2_instr_ed_init(ft2_instrument_editor_t *editor, struct ft2_video_t *video);
void ft2_instr_ed_set_instance(ft2_instrument_editor_t *editor, struct ft2_instance_t *inst);
void ft2_instr_ed_set_instr(ft2_instrument_editor_t *editor, int instr);
void ft2_instr_ed_set_current(ft2_instrument_editor_t *editor);
ft2_instrument_editor_t *ft2_instr_ed_get_current(void);
void ft2_instr_ed_draw(ft2_instrument_editor_t *editor, const struct ft2_bmp_t *bmp);
void ft2_instr_ed_draw_vol_env(ft2_instrument_editor_t *editor);
void ft2_instr_ed_draw_pan_env(ft2_instrument_editor_t *editor);
void ft2_instr_ed_draw_envelope(ft2_instrument_editor_t *editor, int envNum);
void ft2_instr_ed_draw_note_map(ft2_instrument_editor_t *editor, const struct ft2_bmp_t *bmp);
void ft2_instr_ed_draw_piano(ft2_instrument_editor_t *editor, const struct ft2_bmp_t *bmp);
void updateInstEditor(ft2_instrument_editor_t *editor, const struct ft2_bmp_t *bmp);

/* Mouse handling */
void ft2_instr_ed_mouse_click(ft2_instrument_editor_t *editor, int x, int y, int button);
void ft2_instr_ed_mouse_drag(ft2_instrument_editor_t *editor, int x, int y);
void ft2_instr_ed_mouse_up(ft2_instrument_editor_t *editor);

/* Envelope editing */
void ft2_instr_ed_add_env_point(ft2_instrument_editor_t *editor, bool volEnv);
void ft2_instr_ed_del_env_point(ft2_instrument_editor_t *editor, bool volEnv);
void ft2_instr_ed_set_sustain(ft2_instrument_editor_t *editor, bool volEnv, int point);
void ft2_instr_ed_set_loop_start(ft2_instrument_editor_t *editor, bool volEnv, int point);
void ft2_instr_ed_set_loop_end(ft2_instrument_editor_t *editor, bool volEnv, int point);

/* Instrument operations */
void ft2_instr_ed_copy(ft2_instrument_editor_t *editor);
void ft2_instr_ed_paste(ft2_instrument_editor_t *editor);
void ft2_instr_ed_clear(ft2_instrument_editor_t *editor);

/* Visibility */
void showInstEditor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideInstEditor(struct ft2_instance_t *inst);
void toggleInstEditor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void exitInstEditor(struct ft2_instance_t *inst);

/* Extended instrument editor */
void showInstEditorExt(struct ft2_instance_t *inst);
void hideInstEditorExt(struct ft2_instance_t *inst);
void toggleInstEditorExt(struct ft2_instance_t *inst);
void drawInstEditorExt(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* MIDI controls */
void midiChDown(struct ft2_instance_t *inst);
void midiChUp(struct ft2_instance_t *inst);
void midiPrgDown(struct ft2_instance_t *inst);
void midiPrgUp(struct ft2_instance_t *inst);
void midiBendDown(struct ft2_instance_t *inst);
void midiBendUp(struct ft2_instance_t *inst);

/* Envelope preset helpers */
void setStdVolEnvelope(struct ft2_instance_t *inst, uint8_t num);
void setStdPanEnvelope(struct ft2_instance_t *inst, uint8_t num);
void setOrStoreVolEnvPreset(struct ft2_instance_t *inst, uint8_t num);
void setOrStorePanEnvPreset(struct ft2_instance_t *inst, uint8_t num);

/* Volume envelope presets */
void volPreDef1(struct ft2_instance_t *inst);
void volPreDef2(struct ft2_instance_t *inst);
void volPreDef3(struct ft2_instance_t *inst);
void volPreDef4(struct ft2_instance_t *inst);
void volPreDef5(struct ft2_instance_t *inst);
void volPreDef6(struct ft2_instance_t *inst);

/* Pan envelope presets */
void panPreDef1(struct ft2_instance_t *inst);
void panPreDef2(struct ft2_instance_t *inst);
void panPreDef3(struct ft2_instance_t *inst);
void panPreDef4(struct ft2_instance_t *inst);
void panPreDef5(struct ft2_instance_t *inst);
void panPreDef6(struct ft2_instance_t *inst);

/* Relative note controls */
void relativeNoteOctUp(struct ft2_instance_t *inst);
void relativeNoteOctDown(struct ft2_instance_t *inst);
void relativeNoteUp(struct ft2_instance_t *inst);
void relativeNoteDown(struct ft2_instance_t *inst);

/* Volume envelope controls */
void volEnvAdd(struct ft2_instance_t *inst);
void volEnvDel(struct ft2_instance_t *inst);
void volEnvSusUp(struct ft2_instance_t *inst);
void volEnvSusDown(struct ft2_instance_t *inst);
void volEnvRepSUp(struct ft2_instance_t *inst);
void volEnvRepSDown(struct ft2_instance_t *inst);
void volEnvRepEUp(struct ft2_instance_t *inst);
void volEnvRepEDown(struct ft2_instance_t *inst);

/* Pan envelope controls */
void panEnvAdd(struct ft2_instance_t *inst);
void panEnvDel(struct ft2_instance_t *inst);
void panEnvSusUp(struct ft2_instance_t *inst);
void panEnvSusDown(struct ft2_instance_t *inst);
void panEnvRepSUp(struct ft2_instance_t *inst);
void panEnvRepSDown(struct ft2_instance_t *inst);
void panEnvRepEUp(struct ft2_instance_t *inst);
void panEnvRepEDown(struct ft2_instance_t *inst);

/* Sample parameter controls */
void volDown(struct ft2_instance_t *inst);
void volUp(struct ft2_instance_t *inst);
void panDown(struct ft2_instance_t *inst);
void panUp(struct ft2_instance_t *inst);
void ftuneDown(struct ft2_instance_t *inst);
void ftuneUp(struct ft2_instance_t *inst);
void fadeoutDown(struct ft2_instance_t *inst);
void fadeoutUp(struct ft2_instance_t *inst);
void vibSpeedDown(struct ft2_instance_t *inst);
void vibSpeedUp(struct ft2_instance_t *inst);
void vibDepthDown(struct ft2_instance_t *inst);
void vibDepthUp(struct ft2_instance_t *inst);
void vibSweepDown(struct ft2_instance_t *inst);
void vibSweepUp(struct ft2_instance_t *inst);

/* Vibrato type */
void rbVibWaveSine(struct ft2_instance_t *inst);
void rbVibWaveSquare(struct ft2_instance_t *inst);
void rbVibWaveRampDown(struct ft2_instance_t *inst);
void rbVibWaveRampUp(struct ft2_instance_t *inst);

/* Envelope enable checkboxes */
void cbVEnv(struct ft2_instance_t *inst);
void cbVEnvSus(struct ft2_instance_t *inst);
void cbVEnvLoop(struct ft2_instance_t *inst);
void cbPEnv(struct ft2_instance_t *inst);
void cbPEnvSus(struct ft2_instance_t *inst);
void cbPEnvLoop(struct ft2_instance_t *inst);

/* MIDI checkboxes */
void cbInstMidiEnable(struct ft2_instance_t *inst);
void cbInstMuteComputer(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
