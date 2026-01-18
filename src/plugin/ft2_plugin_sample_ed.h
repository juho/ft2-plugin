/*
** FT2 Plugin - Sample Editor API
** Waveform display, range selection, loop editing, clipboard, and sample processing.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_plugin_smpfx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

#include "../ft2_instance.h"

/* Sample clipboard state (per-instance) */
typedef struct smp_clipboard_t {
	int8_t *data;              /* Clipboard sample data (left channel for stereo) */
	int8_t *dataR;             /* Right channel clipboard data (NULL for mono) */
	int32_t length;            /* Clipboard sample length */
	ft2_sample_format_t format;/* Clipboard sample format */
	uint32_t bytesPerSample;   /* Cached bytes per sample */
	bool is16Bit;              /* Clipboard sample bit depth (legacy) */
	bool isStereo;             /* Clipboard sample is stereo */
	bool didCopyWholeSample;   /* True if entire sample was copied */
	bool hasInfo;              /* True if sampleInfo is valid */
	ft2_sample_t sampleInfo;   /* Embedded sample metadata for whole-sample copies */
} smp_clipboard_t;

/* Sample undo buffer (per-instance, single-level) */
typedef struct smp_undo_t {
	bool filled, keepSampleMark;
	uint8_t flags, undoInstr, undoSmp;
	uint32_t length, loopStart, loopLength;
	ft2_sample_format_t format;
	uint32_t sampleRate;
	uint32_t bytesPerSample;
	bool isStereo;
	uint8_t *smpDataL;
	uint8_t *smpDataR;
} smp_undo_t;

/* Display area constants (match original FT2) */
#define SAMPLE_AREA_HEIGHT 154
#define SAMPLE_AREA_WIDTH 632
#define SAMPLE_AREA_Y_CENTER 250
#define SAMPLE_AREA_Y_START 174

/* Stereo display: split area into top (L) and bottom (R) halves */
#define SAMPLE_AREA_L_START   174
#define SAMPLE_AREA_L_HEIGHT  75
#define SAMPLE_AREA_L_CENTER  211
#define SAMPLE_AREA_SEPARATOR 249
#define SAMPLE_AREA_R_START   251
#define SAMPLE_AREA_R_HEIGHT  77
#define SAMPLE_AREA_R_CENTER  289

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

	int16_t currInstr;        /* Current instrument index (1-127) */
	int16_t currSample;       /* Current sample slot (0-15) */

	int32_t scrPos;           /* View scroll position (samples) */
	int32_t viewSize;         /* View width (samples) */
	int32_t oldScrPos;        /* Previous scrPos for zoom calc */
	int32_t oldViewSize;      /* Previous viewSize for zoom calc */

	int32_t rangeStart;       /* Selection start (samples) */
	int32_t rangeEnd;         /* Selection end (samples) */
	bool hasRange;            /* True if selection is valid */

	int32_t loopStart;        /* Cached loop start for display */
	int32_t loopLength;       /* Cached loop length for display */
	int32_t oldSmpPosLine;    /* Last playback position line X */
	int32_t oldRangeStart;    /* Previous selection start (samples) */
	int32_t oldRangeEnd;      /* Previous selection end (samples) */
	bool oldHasRange;         /* Previous selection valid flag */
	int32_t oldLoopStart;     /* Previous loop start (samples) */
	int32_t oldLoopLength;    /* Previous loop length (samples) */
	bool oldLoopActive;       /* Previous loop active flag */

	double dScrPosScaled;     /* scrPos * dPos2ScrMul (floored) */
	double dPos2ScrMul;       /* Sample->screen multiplier */
	double dScr2SmpPosMul;    /* Screen->sample multiplier */

	int32_t lastMouseX;       /* Last mouse X for drag tracking */
	int32_t lastMouseY;       /* Last mouse Y for drag tracking */
	int32_t mouseXOffs;       /* Offset for loop pin dragging */

	int32_t lastDrawX;        /* Last draw position (draw mode) */
	int32_t lastDrawY;        /* Last draw value (draw mode) */

	/* Waveform cache */
	uint32_t *waveformCache;
	bool waveformCacheValid;
	bool waveformDirty;
	bool overlaysDirty;
	int16_t cacheInstr;
	int16_t cacheSample;
	int32_t cacheScrPos;
	int32_t cacheViewSize;
	int32_t cacheLength;
	ft2_sample_format_t cacheFormat;
	bool cacheStereo;
	const void *cacheDataPtr;
	const void *cacheDataPtrR;

	/* Zoomed-out min/max peak cache (24-bit scale) */
	int32_t *peakMinL;
	int32_t *peakMaxL;
	int32_t *peakMinR;
	int32_t *peakMaxR;
	int32_t peakBlockSize;
	int32_t peakBlockCount;
	bool peakCacheValid;
	int16_t peakInstr;
	int16_t peakSample;
	int32_t peakLength;
	ft2_sample_format_t peakFormat;
	bool peakStereo;
	const void *peakDataPtrL;
	const void *peakDataPtrR;

	/* Clipboard (per-instance) */
	smp_clipboard_t clipboard;
	
	/* Undo buffer (per-instance) */
	smp_undo_t undo;

	/* Sample effects state (per-instance) */
	smpfx_state_t smpfx;
} ft2_sample_editor_t;

/* Initialization */
void ft2_sample_ed_init(ft2_sample_editor_t *editor, struct ft2_video_t *video);
void ft2_sample_ed_free(ft2_sample_editor_t *editor);
void ft2_sample_ed_set_sample(struct ft2_instance_t *inst, int instr, int sample);

/* Drawing */
void ft2_sample_ed_draw(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_waveform(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_range(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_loop_points(struct ft2_instance_t *inst);
void ft2_sample_ed_draw_pos_line(struct ft2_instance_t *inst);
void ft2_sample_ed_mark_waveform_dirty(struct ft2_instance_t *inst);
void ft2_sample_ed_mark_overlay_dirty(struct ft2_instance_t *inst);

/* View control */
void ft2_sample_ed_zoom_in(struct ft2_instance_t *inst, int32_t mouseX);
void ft2_sample_ed_zoom_out(struct ft2_instance_t *inst, int32_t mouseX);
void ft2_sample_ed_scroll_left(struct ft2_instance_t *inst);
void ft2_sample_ed_scroll_right(struct ft2_instance_t *inst);
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

/* Clipboard operations */
void ft2_sample_ed_cut(struct ft2_instance_t *inst);
void ft2_sample_ed_copy(struct ft2_instance_t *inst);
void ft2_sample_ed_paste(struct ft2_instance_t *inst);
void ft2_sample_ed_delete(struct ft2_instance_t *inst);

/* Sample processing */
void ft2_sample_ed_reverse(struct ft2_instance_t *inst);
void ft2_sample_ed_normalize(struct ft2_instance_t *inst);
void ft2_sample_ed_fade_in(struct ft2_instance_t *inst);
void ft2_sample_ed_fade_out(struct ft2_instance_t *inst);
void ft2_sample_ed_crossfade_loop(struct ft2_instance_t *inst);
bool ft2_sample_ed_resample_format(struct ft2_instance_t *inst, ft2_sample_format_t targetFormat, uint32_t targetRate);

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

/* Sample memory management */
bool allocateSmpData(struct ft2_instance_t *inst, int instrNum, int smpNum, int32_t length, bool sample16Bit);
void freeSmpData(struct ft2_instance_t *inst, int instrNum, int smpNum);
void clearSample(struct ft2_instance_t *inst);
void clearInstr(struct ft2_instance_t *inst);
void clearCopyBuffer(struct ft2_instance_t *inst);

/* Copy/exchange operations */
struct ft2_sample_t;
bool cloneSample(struct ft2_sample_t *src, struct ft2_sample_t *dst);
void copySmp(struct ft2_instance_t *inst);
void xchgSmp(struct ft2_instance_t *inst);
void copyInstr(struct ft2_instance_t *inst);
void xchgInstr(struct ft2_instance_t *inst);

/* Sample data transformations */
void sampleBackwards(struct ft2_instance_t *inst);
void sampleChangeSign(struct ft2_instance_t *inst);
void sampleByteSwap(struct ft2_instance_t *inst);
void fixDC(struct ft2_instance_t *inst);

/* Loop type radio buttons */
void rbSampleNoLoop(struct ft2_instance_t *inst);
void rbSampleForwardLoop(struct ft2_instance_t *inst);
void rbSamplePingpongLoop(struct ft2_instance_t *inst);

/* Bit depth radio buttons */
void rbSample8bit(struct ft2_instance_t *inst);
void rbSample16bit(struct ft2_instance_t *inst);

/* Sample editing */
void sampCrop(struct ft2_instance_t *inst);
void sampMinimize(struct ft2_instance_t *inst);
void sampApplyVolume(struct ft2_instance_t *inst, double startVol, double endVol);

/* Loop point controls */
void sampRepeatUp(struct ft2_instance_t *inst);
void sampRepeatDown(struct ft2_instance_t *inst);
void sampReplenUp(struct ft2_instance_t *inst);
void sampReplenDown(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
