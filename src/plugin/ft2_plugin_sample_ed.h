/**
 * @file ft2_plugin_sample_ed.h
 * @brief Exact port of sample editor from ft2_sample_ed.h and ft2_sample_ed.c
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

/* Sample editor constants - match original FT2 */
#define SAMPLE_AREA_HEIGHT 154
#define SAMPLE_AREA_WIDTH 632
#define SAMPLE_AREA_Y_CENTER 250
#define SAMPLE_AREA_Y_START 174

/* Sample flags */
#define SAMPLE_16BIT (1 << 4)
#define LOOP_OFF 0
#define LOOP_FWD 1
#define LOOP_BIDI 2
#define GET_LOOPTYPE(f) ((f) & 3)

typedef struct ft2_sample_editor_t
{
	struct ft2_video_t *video;
	const struct ft2_bmp_t *bmp;
	
	/* Current sample selection */
	int16_t currInstr;
	int16_t currSample;
	
	/* View state */
	int32_t scrPos;       /* Scroll position in samples */
	int32_t viewSize;     /* View size in samples */
	int32_t oldScrPos;    /* Previous scroll position (for zoom) */
	int32_t oldViewSize;  /* Previous view size (for zoom) */
	
	/* Range selection */
	int32_t rangeStart;
	int32_t rangeEnd;
	bool hasRange;
	
	/* Loop point cache for visuals */
	int32_t loopStart;
	int32_t loopLength;
	
	/* Position line */
	int32_t oldSmpPosLine;
	
	/* Scaling factors */
	double dScrPosScaled;
	double dPos2ScrMul;
	double dScr2SmpPosMul;
	
	/* Mouse state */
	int32_t lastMouseX;
	int32_t lastMouseY;
	int32_t mouseXOffs;
	
	/* Draw mode state (right-click waveform editing) */
	int32_t lastDrawX;
	int32_t lastDrawY;
} ft2_sample_editor_t;

void ft2_sample_ed_init(ft2_sample_editor_t *editor, struct ft2_video_t *video);
void ft2_sample_ed_set_sample(struct ft2_instance_t *inst, int instr, int sample);
void ft2_sample_ed_draw(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_waveform(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_range(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_loop_points(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_pos_line(struct ft2_instance_t *inst);

/* View control */
void ft2_sample_ed_zoom_in(struct ft2_instance_t *inst, int32_t mouseX);
void ft2_sample_ed_zoom_out(struct ft2_instance_t *inst, int32_t mouseX);
void ft2_sample_ed_show_all(struct ft2_instance_t *inst);
void ft2_sample_ed_show_loop(struct ft2_instance_t *inst);
void ft2_sample_ed_show_range(struct ft2_instance_t *inst);

/* Selection */
void ft2_sample_ed_set_selection(struct ft2_instance_t *inst, int32_t start, int32_t end);
void ft2_sample_ed_clear_selection(struct ft2_instance_t *inst);
void ft2_sample_ed_range_all(struct ft2_instance_t *inst);

/* Mouse handling */
void ft2_sample_ed_mouse_click(struct ft2_instance_t *inst, int x, int y, int button);
void ft2_sample_ed_mouse_drag(struct ft2_instance_t *inst, int x, int y, bool shiftPressed);
void ft2_sample_ed_mouse_up(struct ft2_instance_t *inst);

/* Sample operations */
void ft2_sample_ed_cut(struct ft2_instance_t *inst);
void ft2_sample_ed_copy(struct ft2_instance_t *inst);
void ft2_sample_ed_paste(struct ft2_instance_t *inst);
void ft2_sample_ed_delete(struct ft2_instance_t *inst);
void ft2_sample_ed_reverse(struct ft2_instance_t *inst);
void ft2_sample_ed_normalize(struct ft2_instance_t *inst);
void ft2_sample_ed_fade_in(struct ft2_instance_t *inst);
void ft2_sample_ed_fade_out(struct ft2_instance_t *inst);
void ft2_sample_ed_crossfade_loop(struct ft2_instance_t *inst);

/* Coordinate conversion */
int32_t ft2_sample_scr2SmpPos(struct ft2_instance_t *inst, int32_t x);
int32_t ft2_sample_smpPos2Scr(struct ft2_instance_t *inst, int32_t pos);

/* Visibility */
void showSampleEditor(struct ft2_instance_t *inst);
void hideSampleEditor(struct ft2_instance_t *inst);
void toggleSampleEditor(struct ft2_instance_t *inst);
void exitSampleEditor(struct ft2_instance_t *inst);

/* Extended sample editor */
void showSampleEditorExt(struct ft2_instance_t *inst);
void hideSampleEditorExt(struct ft2_instance_t *inst);
void hideSampleEditorExtButtons(struct ft2_instance_t *inst);
void toggleSampleEditorExt(struct ft2_instance_t *inst);
void drawSampleEditorExt(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Sample memory */
bool allocateSmpData(struct ft2_instance_t *inst, int instrNum, int smpNum, int32_t length, bool sample16Bit);
void freeSmpData(struct ft2_instance_t *inst, int instrNum, int smpNum);
void clearSample(struct ft2_instance_t *inst);
void clearInstr(struct ft2_instance_t *inst);
void clearCopyBuffer(struct ft2_instance_t *inst);

/* Copy/Exchange operations */
struct ft2_sample_t;
bool cloneSample(struct ft2_sample_t *src, struct ft2_sample_t *dst);
void copySmp(struct ft2_instance_t *inst);
void xchgSmp(struct ft2_instance_t *inst);
void copyInstr(struct ft2_instance_t *inst);
void xchgInstr(struct ft2_instance_t *inst);

/* Sample processing */
void sampleBackwards(struct ft2_instance_t *inst);
void sampleChangeSign(struct ft2_instance_t *inst);
void sampleByteSwap(struct ft2_instance_t *inst);
void fixDC(struct ft2_instance_t *inst);

/* Loop type */
void rbSampleNoLoop(struct ft2_instance_t *inst);
void rbSampleForwardLoop(struct ft2_instance_t *inst);
void rbSamplePingpongLoop(struct ft2_instance_t *inst);

/* Bit depth */
void rbSample8bit(struct ft2_instance_t *inst);
void rbSample16bit(struct ft2_instance_t *inst);

/* Sample editing */
void sampCrop(struct ft2_instance_t *inst);
void sampMinimize(struct ft2_instance_t *inst);
void sampApplyVolume(struct ft2_instance_t *inst, double startVol, double endVol);

/* Repeat/Length controls */
void sampRepeatUp(struct ft2_instance_t *inst);
void sampRepeatDown(struct ft2_instance_t *inst);
void sampReplenUp(struct ft2_instance_t *inst);
void sampReplenDown(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
