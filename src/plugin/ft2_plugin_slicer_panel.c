/**
 * @file ft2_plugin_slicer_panel.c
 * @brief Sample slicer panel for the sample editor.
 *
 * Provides transient-based sample slicing with automatic detection,
 * manual marker adjustment, preview, and processing to create
 * chromatically-mapped instrument key zones.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ft2_plugin_slicer_panel.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_sample_utils.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_xm2_convert.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_dialog.h"
#include "../ft2_instance.h"

#define SLICER_STATE(inst) (&(inst)->slicer)

/* Note names for root note display */
static const char *noteNames[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

/* Right-aligned 3-digit decimal strings */
static const char *dec3StrTab[101] = {
	"  0","  1","  2","  3","  4","  5","  6","  7","  8","  9",
	" 10"," 11"," 12"," 13"," 14"," 15"," 16"," 17"," 18"," 19",
	" 20"," 21"," 22"," 23"," 24"," 25"," 26"," 27"," 28"," 29",
	" 30"," 31"," 32"," 33"," 34"," 35"," 36"," 37"," 38"," 39",
	" 40"," 41"," 42"," 43"," 44"," 45"," 46"," 47"," 48"," 49",
	" 50"," 51"," 52"," 53"," 54"," 55"," 56"," 57"," 58"," 59",
	" 60"," 61"," 62"," 63"," 64"," 65"," 66"," 67"," 68"," 69",
	" 70"," 71"," 72"," 73"," 74"," 75"," 76"," 77"," 78"," 79",
	" 80"," 81"," 82"," 83"," 84"," 85"," 86"," 87"," 88"," 89",
	" 90"," 91"," 92"," 93"," 94"," 95"," 96"," 97"," 98"," 99",
	"100"
};

static void buildSliceSampleName(char *dst, size_t dstSize, const char *srcName, int sliceNumber)
{
	if (!dst || dstSize == 0)
		return;

	char baseName[22 + 1];
	strncpy(baseName, (srcName != NULL) ? srcName : "", sizeof(baseName) - 1);
	baseName[sizeof(baseName) - 1] = '\0';

	int nameLen = (int)strlen(baseName);
	while (nameLen > 0 && baseName[nameLen - 1] == ' ')
		baseName[--nameLen] = '\0';

	int end = nameLen;
	while (end > 0 && baseName[end - 1] >= '0' && baseName[end - 1] <= '9')
		end--;
	if (end < nameLen && end > 0 && baseName[end - 1] == ' ')
	{
		baseName[end - 1] = '\0';
		nameLen = end - 1;
		while (nameLen > 0 && baseName[nameLen - 1] == ' ')
			baseName[--nameLen] = '\0';
	}

	int digits = 1;
	int tmp = sliceNumber;
	while (tmp >= 10)
	{
		digits++;
		tmp /= 10;
	}

	int maxBaseLen = (int)dstSize - 1 - 1 - digits;
	if (maxBaseLen < 0)
		maxBaseLen = 0;
	if (nameLen > maxBaseLen)
		nameLen = maxBaseLen;

	snprintf(dst, dstSize, "%.*s %d", nameLen, baseName, sliceNumber);
}

/* Get current sample from sample editor */
static ft2_sample_t *getCurrentSample(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed) return NULL;
	if (ed->currInstr <= 0 || ed->currInstr >= 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[ed->currInstr];
	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	if (!instr || ed->currSample < 0 || ed->currSample >= maxSamples) return NULL;
	return &instr->smp[ed->currSample];
}

/* Get current instrument */
static ft2_instr_t *getCurrentInstrument(ft2_instance_t *inst)
{
	if (!inst) return NULL;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed || ed->currInstr <= 0 || ed->currInstr >= 128) return NULL;
	return inst->replayer.instr[ed->currInstr];
}

/* Initialize slicer state with defaults */
static void initSlicerState(ft2_instance_t *inst)
{
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	state->numSlices = 8;
	state->rootNote = 48;  /* C-4 */
	state->sensitivity = 5;  /* 0-10 range, 5 is balanced */
	state->draggedMarker = -1;
	state->selectedSlice = -1;
	memset(state->slicePos, 0, sizeof(state->slicePos));
}

static int32_t getMaxSlices(ft2_instance_t *inst)
{
	return ft2_get_max_samples_per_instr(inst);
}

/* Setup slicer panel widgets */
static void setupWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	pushButton_t *p;

	/* Slices up/down */
	p = &widgets->pushButtons[PB_SLICER_SLICES_DOWN]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 47; p->y = 349; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerSlicesDown;
	widgets->pushButtonVisible[PB_SLICER_SLICES_DOWN] = true;

	p = &widgets->pushButtons[PB_SLICER_SLICES_UP]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 104; p->y = 349; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerSlicesUp;
	widgets->pushButtonVisible[PB_SLICER_SLICES_UP] = true;

	/* Sensitivity up/down */
	p = &widgets->pushButtons[PB_SLICER_SENS_DOWN]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 47; p->y = 366; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerSensDown;
	widgets->pushButtonVisible[PB_SLICER_SENS_DOWN] = true;

	p = &widgets->pushButtons[PB_SLICER_SENS_UP]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 104; p->y = 366; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerSensUp;
	widgets->pushButtonVisible[PB_SLICER_SENS_UP] = true;

	/* Root note up/down */
	p = &widgets->pushButtons[PB_SLICER_ROOT_DOWN]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 47; p->y = 383; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerRootDown;
	widgets->pushButtonVisible[PB_SLICER_ROOT_DOWN] = true;

	p = &widgets->pushButtons[PB_SLICER_ROOT_UP]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 104; p->y = 383; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = pbSlicerRootUp;
	widgets->pushButtonVisible[PB_SLICER_ROOT_UP] = true;

	/* Process button (right column, within framework x=290-353) */
	p = &widgets->pushButtons[PB_SLICER_PROCESS]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Process"; p->x = 294; p->y = 349; p->w = 55; p->h = 16;
	p->callbackFuncOnUp = pbSlicerProcess;
	widgets->pushButtonVisible[PB_SLICER_PROCESS] = true;

	/* Back button (right column) */
	p = &widgets->pushButtons[PB_SLICER_BACK]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Back..."; p->x = 294; p->y = 366; p->w = 55; p->h = 16;
	p->callbackFuncOnUp = pbSlicerBack;
	widgets->pushButtonVisible[PB_SLICER_BACK] = true;
}

/* Hide slicer widgets */
static void hideWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	widgets->pushButtonVisible[PB_SLICER_SLICES_UP] = false;
	widgets->pushButtonVisible[PB_SLICER_SLICES_DOWN] = false;
	widgets->pushButtonVisible[PB_SLICER_SENS_UP] = false;
	widgets->pushButtonVisible[PB_SLICER_SENS_DOWN] = false;
	widgets->pushButtonVisible[PB_SLICER_ROOT_UP] = false;
	widgets->pushButtonVisible[PB_SLICER_ROOT_DOWN] = false;
	widgets->pushButtonVisible[PB_SLICER_PROCESS] = false;
	widgets->pushButtonVisible[PB_SLICER_BACK] = false;
	hideRadioButtonGroup(widgets, RB_GROUP_SLICER_MODE);
}

/* ------------------------------------------------------------------------- */
/*                        SCREEN VISIBILITY                                  */
/* ------------------------------------------------------------------------- */

void showSlicerScreen(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &FT2_UI(inst)->widgets : NULL;
	if (!widgets) return;

	/* Hide effects screen if open */
	if (inst->uiState.sampleEditorEffectsShown)
		hideSampleEffectsScreen(inst);

	/* Initialize state if needed */
	initSlicerState(inst);

	inst->uiState.sampleEditorSlicerShown = true;

	/* Hide sample editor buttons */
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = false;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_STOP] = false;
	widgets->pushButtonVisible[PB_SAMP_PWAVE] = false;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_SLICER] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = false;
	widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = false;
	widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = false;
	widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;

	/* Setup slicer widgets */
	setupWidgets(inst);

	/* Show radio buttons and select "Samples" by default */
	widgets->radioButtonVisible[RB_SLICER_SAMPLES] = true;
	widgets->radioButtonVisible[RB_SLICER_INSTRUMENTS] = true;
	checkRadioButtonNoRedraw(widgets, RB_SLICER_SAMPLES);

	/* Run initial detection */
	slicerDetectTransients(inst);

	inst->uiState.needsFullRedraw = true;
}

void hideSlicerScreen(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &FT2_UI(inst)->widgets : NULL;
	if (!widgets) return;

	inst->uiState.sampleEditorSlicerShown = false;

	/* Hide slicer widgets */
	hideWidgets(inst);

	/* Only restore sample editor buttons if sample editor is still visible */
	if (inst->uiState.sampleEditorShown)
	{
		widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = true;
		widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = true;
		widgets->pushButtonVisible[PB_SAMP_STOP] = true;
		widgets->pushButtonVisible[PB_SAMP_PWAVE] = true;
		widgets->pushButtonVisible[PB_SAMP_PRANGE] = true;
		widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = true;
		widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = true;
		widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = true;
		widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = true;
		widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = true;
		widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = true;
		widgets->pushButtonVisible[PB_SAMP_SLICER] = true;
		widgets->pushButtonVisible[PB_SAMP_CUT] = true;
		widgets->pushButtonVisible[PB_SAMP_COPY] = true;
		widgets->pushButtonVisible[PB_SAMP_PASTE] = true;
		widgets->pushButtonVisible[PB_SAMP_CROP] = true;
		widgets->pushButtonVisible[PB_SAMP_VOLUME] = true;
		widgets->pushButtonVisible[PB_SAMP_EFFECTS] = true;
	}

	inst->uiState.needsFullRedraw = true;
}

void restoreSlicerWidgets(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	if (!inst->uiState.sampleEditorShown || !inst->uiState.sampleEditorSlicerShown) return;

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	/* Hide sample editor buttons */
	widgets->pushButtonVisible[PB_SAMP_PNOTE_UP] = false;
	widgets->pushButtonVisible[PB_SAMP_PNOTE_DOWN] = false;
	widgets->pushButtonVisible[PB_SAMP_STOP] = false;
	widgets->pushButtonVisible[PB_SAMP_PWAVE] = false;
	widgets->pushButtonVisible[PB_SAMP_PRANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_PDISPLAY] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_RANGE_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_CLR_RANGE] = false;
	widgets->pushButtonVisible[PB_SAMP_ZOOM_OUT] = false;
	widgets->pushButtonVisible[PB_SAMP_SHOW_ALL] = false;
	widgets->pushButtonVisible[PB_SAMP_SLICER] = false;
	widgets->pushButtonVisible[PB_SAMP_CUT] = false;
	widgets->pushButtonVisible[PB_SAMP_COPY] = false;
	widgets->pushButtonVisible[PB_SAMP_PASTE] = false;
	widgets->pushButtonVisible[PB_SAMP_CROP] = false;
	widgets->pushButtonVisible[PB_SAMP_VOLUME] = false;
	widgets->pushButtonVisible[PB_SAMP_EFFECTS] = false;

	/* Setup slicer widgets */
	setupWidgets(inst);

	/* Show radio buttons without changing selection */
	widgets->radioButtonVisible[RB_SLICER_SAMPLES] = true;
	widgets->radioButtonVisible[RB_SLICER_INSTRUMENTS] = true;
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

void drawSlicerScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_widgets_t *widgets = inst->ui ? &FT2_UI(inst)->widgets : NULL;

	/* Three grouped frameworks like the effects panel */
	drawFramework(video, 0, 346, 132, 54, FRAMEWORK_TYPE1);    /* Slices/Sens/Root */
	drawFramework(video, 132, 346, 158, 54, FRAMEWORK_TYPE1);  /* Create mode */
	drawFramework(video, 290, 346, 63, 54, FRAMEWORK_TYPE1);   /* Process/Back */

	/* Column 1: Slices/Sens/Root labels and values */
	textOutShadow(video, bmp, 4, 351, PAL_FORGRND, PAL_DSKTOP2, "Slices:");
	textOutShadow(video, bmp, 4, 368, PAL_FORGRND, PAL_DSKTOP2, "Sens.:");
	textOutShadow(video, bmp, 4, 385, PAL_FORGRND, PAL_DSKTOP2, "Root:");

	/* Values with sunken frames (fit between arrow buttons: x=70-104, h=13) */
	char numStr[4];
	snprintf(numStr, sizeof(numStr), "%3d", state->numSlices);
	drawFramework(video, 70, 349, 34, 13, FRAMEWORK_TYPE2);
	textOut(video, bmp, 76, 351, PAL_FORGRND, numStr);

	drawFramework(video, 70, 366, 34, 13, FRAMEWORK_TYPE2);
	if (state->sensitivity <= 10)
		textOut(video, bmp, 76, 368, PAL_FORGRND, dec3StrTab[state->sensitivity]);

	/* Root note (e.g., "C-4") */
	char noteStr[8];
	uint8_t octave = state->rootNote / 12;
	uint8_t note = state->rootNote % 12;
	snprintf(noteStr, sizeof(noteStr), "%s%d", noteNames[note], octave);
	drawFramework(video, 70, 383, 34, 13, FRAMEWORK_TYPE2);
	textOut(video, bmp, 76, 385, PAL_FORGRND, noteStr);

	/* Column 2: Create mode radio buttons */
	textOutShadow(video, bmp, 137, 351, PAL_FORGRND, PAL_DSKTOP2, "Create..");
	textOutShadow(video, bmp, 151, 368, PAL_FORGRND, PAL_DSKTOP2, "Samples");
	textOutShadow(video, bmp, 151, 385, PAL_FORGRND, PAL_DSKTOP2, "Instruments");

	/* Draw radio buttons */
	if (widgets)
	{
		drawRadioButton(widgets, video, bmp, RB_SLICER_SAMPLES);
		drawRadioButton(widgets, video, bmp, RB_SLICER_INSTRUMENTS);
	}
}

/* ------------------------------------------------------------------------- */
/*                        SLICE MARKERS                                      */
/* ------------------------------------------------------------------------- */

void drawSliceMarkers(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !inst->uiState.sampleEditorSlicerShown) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s || s->length <= 0) return;

	/* Draw N markers for N slices (skip the end marker at slicePos[N]) */
	for (int i = 0; i < state->numSlices; i++)
	{
		int32_t smpPos = state->slicePos[i];
		int32_t screenX = ft2_sample_smpPos2Scr(inst, smpPos);

		if (screenX >= 0 && screenX < SAMPLE_AREA_WIDTH)
		{
			/* Draw marker line in cyan/green color */
			for (int y = SAMPLE_AREA_Y_START; y < SAMPLE_AREA_Y_START + SAMPLE_AREA_HEIGHT; y++)
			{
				if (screenX < SCREEN_W)
					video->frameBuffer[y * SCREEN_W + screenX] = video->palette[PAL_BUTTONS];
			}

			/* Label ALL markers 1 through N */
			if (bmp)
			{
				char numStr[4];
				snprintf(numStr, sizeof(numStr), "%d", i + 1);
				int textX = screenX - 4;
				if (textX < 0) textX = 0;
				if (textX > SAMPLE_AREA_WIDTH - 8) textX = SAMPLE_AREA_WIDTH - 8;
				textOut(video, bmp, textX, SAMPLE_AREA_Y_START + 2, PAL_FORGRND, numStr);
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                     TRANSIENT DETECTION                                   */
/* ------------------------------------------------------------------------- */

/* Onset candidate with position, strength, and blended score */
typedef struct {
	int32_t pos;
	float strength;
	float score;
} onset_t;

/* Compare onsets by blended score (descending) for qsort */
static int compareOnsetScore(const void *a, const void *b)
{
	const onset_t *oa = (const onset_t *)a;
	const onset_t *ob = (const onset_t *)b;
	if (ob->score > oa->score) return 1;
	if (ob->score < oa->score) return -1;
	return 0;
}

/* Compare onsets by position (ascending) for qsort */
static int compareOnsetPosition(const void *a, const void *b)
{
	const onset_t *oa = (const onset_t *)a;
	const onset_t *ob = (const onset_t *)b;
	return oa->pos - ob->pos;
}

void slicerDetectTransients(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format) || s->length <= 0)
	{
		for (int i = 0; i <= state->numSlices; i++)
			state->slicePos[i] = 0;
		return;
	}

	int32_t smpLen = s->length;
	const bool isStereo = ft2_sample_is_stereo(s, format);

	/* Parameters for onset detection */
	int32_t windowSize = 512;
	int32_t hopSize = 256;
	int32_t minSpacing = smpLen / (state->numSlices * 3);
	if (minSpacing < hopSize * 2) minSpacing = hopSize * 2;

	int32_t numFrames = (smpLen - windowSize) / hopSize + 1;
	if (numFrames < 3)
	{
		/* Sample too short - use even division */
		for (int i = 0; i <= state->numSlices; i++)
			state->slicePos[i] = (i * smpLen) / state->numSlices;
		return;
	}

	/* Allocate energy buffer */
	float *energy = (float *)malloc(numFrames * sizeof(float));
	if (!energy)
	{
		for (int i = 0; i <= state->numSlices; i++)
			state->slicePos[i] = (i * smpLen) / state->numSlices;
		return;
	}

	/* Calculate RMS energy for each overlapping frame */
	for (int32_t f = 0; f < numFrames; f++)
	{
		int32_t start = f * hopSize;
		double sum = 0.0;

		int32_t end = start + windowSize;
		if (end > smpLen) end = smpLen;
		for (int32_t j = start; j < end; j++)
		{
			float v = ft2_sample_get_float(s, format, false, j);
			if (isStereo)
				v = 0.5f * (v + ft2_sample_get_float(s, format, true, j));
			sum += (double)v * (double)v;
		}

		energy[f] = (float)sqrt(sum / windowSize);
	}

	/* Calculate onset detection function (positive energy differences) */
	float *diff = (float *)malloc(numFrames * sizeof(float));
	float maxDiff = 0.0f;
	if (diff)
	{
		diff[0] = 0.0f;
		for (int32_t f = 1; f < numFrames; f++)
		{
			float d = energy[f] - energy[f - 1];
			diff[f] = (d > 0.0f) ? d : 0.0f;
			if (diff[f] > maxDiff) maxDiff = diff[f];
		}
	}

	/* Adaptive threshold based on sensitivity (0-10 range) */
	/* sensitivity=0: threshold = 50% of max (only strong transients) */
	/* sensitivity=5: threshold ~= 12.5% of max */
	/* sensitivity=10: threshold = 0 (all local maxima) */
	float sensNorm = state->sensitivity / 10.0f;
	float threshold = maxDiff * (1.0f - sensNorm) * (1.0f - sensNorm) * 0.5f;

	/* Collect all onset candidates above threshold */
	onset_t *candidates = (onset_t *)malloc(numFrames * sizeof(onset_t));
	int32_t numCandidates = 0;

	if (diff && candidates)
	{
		/* Peak picking: diff[f] must be a local maximum */
		for (int32_t f = 1; f < numFrames - 1; f++)
		{
			if (diff[f] > threshold && diff[f] >= diff[f - 1] && diff[f] >= diff[f + 1])
			{
				candidates[numCandidates].pos = f * hopSize;
				candidates[numCandidates].strength = diff[f];
				numCandidates++;
			}
		}
	}

	/* Set slice boundaries */
	state->slicePos[0] = 0;
	state->slicePos[state->numSlices] = smpLen;

	/* Calculate ideal positions for even distribution */
	int32_t *idealPos = (int32_t *)malloc((size_t)(state->numSlices + 1) * sizeof(int32_t));
	if (!idealPos)
	{
		free(energy);
		if (diff) free(diff);
		if (candidates) free(candidates);
		return;
	}
	for (int i = 1; i < state->numSlices; i++)
		idealPos[i] = (i * smpLen) / state->numSlices;

	/* Sensitivity controls blend between strength (0) and position (10) */
	float sensBlend = state->sensitivity / 10.0f;
	float idealSpacing = (float)smpLen / state->numSlices;

	/* Score each candidate based on blend of strength and position */
	for (int32_t c = 0; c < numCandidates; c++)
	{
		/* Normalize strength to 0-1 range */
		float strengthScore = (maxDiff > 0.0f) ? (candidates[c].strength / maxDiff) : 0.0f;

		/* Find proximity to nearest ideal position (0-1, higher = closer) */
		float minDist = (float)smpLen;
		for (int i = 1; i < state->numSlices; i++)
		{
			float dist = (float)abs(candidates[c].pos - idealPos[i]);
			if (dist < minDist) minDist = dist;
		}
		float positionScore = 1.0f - (minDist / idealSpacing);
		if (positionScore < 0.0f) positionScore = 0.0f;

		/* Blend scores: sens=0 uses strength only, sens=100 uses position only */
		candidates[c].score = strengthScore * (1.0f - sensBlend) + positionScore * sensBlend;
	}

	if (numCandidates >= state->numSlices - 1)
	{
		/* Sort by blended score (highest first) */
		qsort(candidates, numCandidates, sizeof(onset_t), compareOnsetScore);

		/* Pick the best (numSlices - 1) onsets with minimum spacing */
		onset_t *selected = (onset_t *)malloc((state->numSlices - 1) * sizeof(onset_t));
		int32_t numSelected = 0;

		if (selected)
		{
			for (int32_t c = 0; c < numCandidates && numSelected < state->numSlices - 1; c++)
			{
				/* Check minimum spacing from fixed boundaries (0 and smpLen) */
				if (candidates[c].pos < minSpacing || candidates[c].pos > smpLen - minSpacing)
					continue;

				/* Check minimum spacing from all already-selected onsets */
				bool tooClose = false;
				for (int32_t s = 0; s < numSelected; s++)
				{
					if (abs(candidates[c].pos - selected[s].pos) < minSpacing)
					{
						tooClose = true;
						break;
					}
				}
				if (!tooClose)
				{
					selected[numSelected++] = candidates[c];
				}
			}

			/* Sort selected by position */
			qsort(selected, numSelected, sizeof(onset_t), compareOnsetPosition);

			/* Assign to slice positions */
			for (int i = 0; i < numSelected; i++)
				state->slicePos[i + 1] = selected[i].pos;

			/* If we didn't get enough, fill the rest with even division of REMAINING space */
			for (int i = numSelected + 1; i < state->numSlices; i++)
			{
				int32_t prevPos = state->slicePos[i - 1];
				int32_t remainingSlices = state->numSlices - i;
				state->slicePos[i] = prevPos + (smpLen - prevPos) / (remainingSlices + 1);
			}

			free(selected);
		}
	}
	else
	{
		/* Not enough candidates - use even division */
		for (int i = 1; i < state->numSlices; i++)
			state->slicePos[i] = (i * smpLen) / state->numSlices;
	}

	/* Ensure positions are sorted and valid */
	for (int i = 1; i <= state->numSlices; i++)
	{
		if (state->slicePos[i] <= state->slicePos[i - 1])
			state->slicePos[i] = state->slicePos[i - 1] + 1;
		if (state->slicePos[i] > smpLen)
			state->slicePos[i] = smpLen;
	}

	free(idealPos);
	free(energy);
	if (diff) free(diff);
	if (candidates) free(candidates);

	ft2_sample_ed_mark_overlay_dirty(inst);
}

/* ------------------------------------------------------------------------- */
/*                        BUTTON CALLBACKS                                   */
/* ------------------------------------------------------------------------- */

void pbSlicerSlicesUp(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	const int32_t maxSlices = getMaxSlices(inst);
	if (state->numSlices < maxSlices)
	{
		state->numSlices++;
		slicerDetectTransients(inst);
	}
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerSlicesDown(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (state->numSlices > 2)
	{
		state->numSlices--;
		slicerDetectTransients(inst);
	}
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerSensUp(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (state->sensitivity < 10)
	{
		state->sensitivity++;
		slicerDetectTransients(inst);
	}
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerSensDown(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (state->sensitivity > 0)
	{
		state->sensitivity--;
		slicerDetectTransients(inst);
	}
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerRootUp(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	int32_t maxRoot = 96 - (int32_t)state->numSlices;
	if (maxRoot < 0) maxRoot = 0;
	if (state->rootNote < maxRoot)
		state->rootNote++;
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerRootDown(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (state->rootNote > 0)
		state->rootNote--;
	inst->uiState.needsFullRedraw = true;
}

void pbSlicerDetect(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	const int32_t maxSlices = getMaxSlices(inst);
	if (state->numSlices > maxSlices)
		state->numSlices = (uint8_t)maxSlices;
	slicerDetectTransients(inst);
	inst->uiState.needsFullRedraw = true;
}

/* Process slices into sample slots within current instrument */
static void slicerProcessSamples(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_instr_t *instr = getCurrentInstrument(inst);
	ft2_sample_t *src = getCurrentSample(inst);

	ft2_sample_format_t format = ft2_sample_get_format(src);
	if (!instr || !src || !ft2_sample_has_data(src, format) || src->length <= 0)
		return;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	const bool isStereo = ft2_sample_is_stereo(src, format);
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	int32_t srcLen = src->length;

	/* Make a copy of the source sample data since we'll overwrite slot 0 */
	uint8_t *srcCopy = (uint8_t *)malloc((size_t)srcLen * bytesPerSample);
	if (!srcCopy) return;
	memcpy(srcCopy, ft2_sample_get_data_ptr_const(src, format, false), (size_t)srcLen * bytesPerSample);

	uint8_t *srcCopyR = NULL;
	if (isStereo)
	{
		srcCopyR = (uint8_t *)malloc((size_t)srcLen * bytesPerSample);
		if (!srcCopyR) { free(srcCopy); return; }
		memcpy(srcCopyR, ft2_sample_get_data_ptr_const(src, format, true), (size_t)srcLen * bytesPerSample);
	}

	/* Save source sample properties */
	uint8_t srcVolume = src->volume;
	uint8_t srcPanning = src->panning;
	int8_t srcFinetune = src->finetune;
	int8_t srcRelNote = src->relativeNote;
	uint8_t srcFlags = src->flags;

	/* Clear note-to-sample mapping for the instrument */
	memset(instr->note2SampleLUT, 0, sizeof(instr->note2SampleLUT));

	/* Process each slice */
	int i = 0;
	while (i < state->numSlices && i < maxSamples)
	{
		int32_t start = state->slicePos[i];
		int32_t end = state->slicePos[i + 1];
		int32_t len = end - start;

		if (len <= 0) { i++; continue; }

		ft2_sample_t *dst = &instr->smp[i];

		/* Free existing sample data */
		ft2_sample_free_buffers(dst);

		/* Allocate new sample data with padding for interpolation */
		if (!ft2_sample_alloc_buffers(dst, len, format, isStereo)) { i++; continue; }

		/* Copy slice data from the backup */
		uint8_t *dstL = (uint8_t *)ft2_sample_get_data_ptr(dst, format, false);
		memcpy(dstL, srcCopy + (start * bytesPerSample), (size_t)len * bytesPerSample);
		if (isStereo)
		{
			uint8_t *dstR = (uint8_t *)ft2_sample_get_data_ptr(dst, format, true);
			memcpy(dstR, srcCopyR + (start * bytesPerSample), (size_t)len * bytesPerSample);
		}

		/* Setup sample */
		dst->length = len;
		dst->loopStart = 0;
		dst->loopLength = 0;
		dst->flags = srcFlags & ~(LOOP_FWD | LOOP_BIDI);
		if (format == FT2_SMP_FORMAT_16BIT)
			dst->flags |= FT2_SAMPLE_16BIT;
		else
			dst->flags &= ~FT2_SAMPLE_16BIT;
		if (isStereo)
			dst->flags |= FT2_SAMPLE_STEREO;
		else
			dst->flags &= ~FT2_SAMPLE_STEREO;
		dst->volume = srcVolume;
		dst->panning = srcPanning;
		dst->finetune = srcFinetune;
		int32_t relNote = (int32_t)srcRelNote - i;
		if (relNote < -48)
			relNote = -48;
		else if (relNote > 71)
			relNote = 71;
		dst->relativeNote = (int8_t)relNote;
		dst->format = format;
		dst->sampleRate = src->sampleRate;
		dst->isFixed = false;

		/* Copy name based on source sample name with slice number */
		buildSliceSampleName(dst->name, sizeof(dst->name), src->name, i + 1);

		/* Fix sample for interpolation */
		ft2_fix_sample(dst);

		/* Setup note-to-sample mapping chromatically from root note */
		int note = state->rootNote + i;
		if (note < 96)
			instr->note2SampleLUT[note] = (uint8_t)i;

		i++;
	}

	free(srcCopy);
	if (srcCopyR) free(srcCopyR);

	/* Update instrument sample count */
	if (state->numSlices > instr->numSamples)
		instr->numSamples = state->numSlices;

	/* Mark as modified and update UI */
	ft2_song_mark_modified(inst);
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
	ft2_xm2_update_usage(inst);

	/* Exit slicer mode */
	hideSlicerScreen(inst);
}

/* Process slices into separate instruments (one per slice) */
static void slicerProcessInstruments(ft2_instance_t *inst)
{
	if (!inst) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed) return;

	uint8_t startInstr = ed->currInstr;
	ft2_instr_t *srcInstr = inst->replayer.instr[startInstr];
	ft2_sample_t *src = getCurrentSample(inst);

	ft2_sample_format_t format = ft2_sample_get_format(src);
	if (!srcInstr || !src || !ft2_sample_has_data(src, format) || src->length <= 0)
		return;

	/* Check if we have enough instrument slots */
	if (startInstr + state->numSlices > 127)
		return;

	const int32_t maxSamples = ft2_get_max_samples_per_instr(inst);
	const bool isStereo = ft2_sample_is_stereo(src, format);
	const uint32_t bytesPerSample = ft2_sample_format_bytes(format);
	int32_t srcLen = src->length;

	/* Make a copy of the source sample data */
	uint8_t *srcCopy = (uint8_t *)malloc((size_t)srcLen * bytesPerSample);
	if (!srcCopy) return;
	memcpy(srcCopy, ft2_sample_get_data_ptr_const(src, format, false), (size_t)srcLen * bytesPerSample);

	uint8_t *srcCopyR = NULL;
	if (isStereo)
	{
		srcCopyR = (uint8_t *)malloc((size_t)srcLen * bytesPerSample);
		if (!srcCopyR) { free(srcCopy); return; }
		memcpy(srcCopyR, ft2_sample_get_data_ptr_const(src, format, true), (size_t)srcLen * bytesPerSample);
	}

	/* Save source sample properties */
	uint8_t srcVolume = src->volume;
	uint8_t srcPanning = src->panning;
	int8_t srcFinetune = src->finetune;
	int8_t srcRelNote = src->relativeNote;
	uint8_t srcFlags = src->flags;

	/* Process each slice into a separate instrument */
	int i = 0;
	while (i < state->numSlices && i < maxSamples)
	{
		int32_t start = state->slicePos[i];
		int32_t end = state->slicePos[i + 1];
		int32_t len = end - start;

		if (len <= 0) { i++; continue; }

		uint8_t instrNum = startInstr + i;

		/* Allocate instrument if needed */
		if (!inst->replayer.instr[instrNum])
		{
			inst->replayer.instr[instrNum] = (ft2_instr_t *)calloc(1, sizeof(ft2_instr_t));
			if (!inst->replayer.instr[instrNum]) { i++; continue; }
		}

		ft2_instr_t *dstInstr = inst->replayer.instr[instrNum];

		/* Clear existing samples in this instrument */
		int j = 0;
		while (j < maxSamples)
		{
			ft2_sample_t *s = &dstInstr->smp[j];
			ft2_sample_free_buffers(s);
			memset(s, 0, sizeof(ft2_sample_t));
			j++;
		}

		/* Copy instrument settings from source (envelopes, fadeout, vibrato, etc.) */
		dstInstr->midiOn = srcInstr->midiOn;
		dstInstr->mute = srcInstr->mute;
		dstInstr->midiChannel = srcInstr->midiChannel;
		dstInstr->volEnvLength = srcInstr->volEnvLength;
		dstInstr->panEnvLength = srcInstr->panEnvLength;
		dstInstr->volEnvSustain = srcInstr->volEnvSustain;
		dstInstr->volEnvLoopStart = srcInstr->volEnvLoopStart;
		dstInstr->volEnvLoopEnd = srcInstr->volEnvLoopEnd;
		dstInstr->panEnvSustain = srcInstr->panEnvSustain;
		dstInstr->panEnvLoopStart = srcInstr->panEnvLoopStart;
		dstInstr->panEnvLoopEnd = srcInstr->panEnvLoopEnd;
		dstInstr->volEnvFlags = srcInstr->volEnvFlags;
		dstInstr->panEnvFlags = srcInstr->panEnvFlags;
		dstInstr->autoVibType = srcInstr->autoVibType;
		dstInstr->autoVibSweep = srcInstr->autoVibSweep;
		dstInstr->autoVibDepth = srcInstr->autoVibDepth;
		dstInstr->autoVibRate = srcInstr->autoVibRate;
		dstInstr->fadeout = srcInstr->fadeout;
		memcpy(dstInstr->volEnvPoints, srcInstr->volEnvPoints, sizeof(srcInstr->volEnvPoints));
		memcpy(dstInstr->panEnvPoints, srcInstr->panEnvPoints, sizeof(srcInstr->panEnvPoints));
		memcpy(dstInstr->volEnvCurve, srcInstr->volEnvCurve, sizeof(srcInstr->volEnvCurve));
		memcpy(dstInstr->panEnvCurve, srcInstr->panEnvCurve, sizeof(srcInstr->panEnvCurve));
		dstInstr->volEnvSpeed = srcInstr->volEnvSpeed;
		dstInstr->panEnvSpeed = srcInstr->panEnvSpeed;
		dstInstr->midiProgram = srcInstr->midiProgram;
		dstInstr->midiBend = srcInstr->midiBend;

		dstInstr->numSamples = 1;

		/* All notes map to sample 0 */
		memset(dstInstr->note2SampleLUT, 0, sizeof(dstInstr->note2SampleLUT));

		/* Create the sample */
		ft2_sample_t *dst = &dstInstr->smp[0];

		/* Allocate new sample data with padding for interpolation */
		if (!ft2_sample_alloc_buffers(dst, len, format, isStereo)) { i++; continue; }

		/* Copy slice data from the backup */
		uint8_t *dstL = (uint8_t *)ft2_sample_get_data_ptr(dst, format, false);
		memcpy(dstL, srcCopy + (start * bytesPerSample), (size_t)len * bytesPerSample);
		if (isStereo)
		{
			uint8_t *dstR = (uint8_t *)ft2_sample_get_data_ptr(dst, format, true);
			memcpy(dstR, srcCopyR + (start * bytesPerSample), (size_t)len * bytesPerSample);
		}

		/* Setup sample */
		dst->length = len;
		dst->loopStart = 0;
		dst->loopLength = 0;
		dst->flags = srcFlags & ~(LOOP_FWD | LOOP_BIDI);
		if (format == FT2_SMP_FORMAT_16BIT)
			dst->flags |= FT2_SAMPLE_16BIT;
		else
			dst->flags &= ~FT2_SAMPLE_16BIT;
		if (isStereo)
			dst->flags |= FT2_SAMPLE_STEREO;
		else
			dst->flags &= ~FT2_SAMPLE_STEREO;
		dst->volume = srcVolume;
		dst->panning = srcPanning;
		dst->finetune = srcFinetune;
		dst->relativeNote = srcRelNote;
		dst->format = format;
		dst->sampleRate = src->sampleRate;
		dst->isFixed = false;

		/* Copy name based on source sample name with slice number */
		buildSliceSampleName(dst->name, sizeof(dst->name), src->name, i + 1);

		/* Fix sample for interpolation */
		ft2_fix_sample(dst);

		i++;
	}

	free(srcCopy);
	if (srcCopyR) free(srcCopyR);

	/* Mark as modified and update UI */
	ft2_song_mark_modified(inst);
	ft2_sample_ed_mark_waveform_dirty(inst);
	inst->uiState.updateInstrSwitcher = true;
	ft2_xm2_update_usage(inst);

	/* Exit slicer mode */
	hideSlicerScreen(inst);
}

/* Dialog callback for slicer confirmation */
static void onSlicerConfirmResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                  const char *inputText, void *userData)
{
	(void)inputText;

	if (!inst || result != DIALOG_RESULT_YES)
		return;

	bool instrumentMode = (userData != NULL);
	if (instrumentMode)
		slicerProcessInstruments(inst);
	else
		slicerProcessSamples(inst);
}

void pbSlicerProcess(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;

	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);

	ft2_sample_t *src = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(src);
	if (!src || !ft2_sample_has_data(src, format) || src->length <= 0)
		return;

	bool instrumentMode = (widgets->radioButtonState[RB_SLICER_INSTRUMENTS] == RADIOBUTTON_CHECKED);

	if (instrumentMode)
	{
		/* Check if we have enough instrument slots */
		uint8_t startInstr = ed->currInstr;
		uint8_t endInstr = startInstr + state->numSlices - 1;
		if (endInstr > 127)
		{
			ft2_dialog_show_message(&ui->dialog, "System message",
				"Not enough instrument slots! Reduce slices or start from a lower instrument.");
			return;
		}

		char msg[128];
		snprintf(msg, sizeof(msg),
			"This will overwrite instruments (%02X-%02X) with the slices. Proceed?",
			startInstr, endInstr);
		ft2_dialog_show_yesno_cb(&ui->dialog, "System request", msg,
			inst, onSlicerConfirmResult, (void*)1);
	}
	else
	{
		ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
			"This will overwrite samples in the selected instrument with the slices. Proceed?",
			inst, onSlicerConfirmResult, NULL);
	}
}

void pbSlicerBack(ft2_instance_t *inst)
{
	if (!inst) return;
	hideSlicerScreen(inst);
}

/* ------------------------------------------------------------------------- */
/*                        MARKER INTERACTION                                 */
/* ------------------------------------------------------------------------- */

int8_t slicerHitTestMarker(ft2_instance_t *inst, int32_t screenX)
{
	if (!inst || !inst->uiState.sampleEditorSlicerShown) return -1;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	if (!s) return -1;

	/* Check each marker (skip first and last - they're fixed) */
	for (int i = 1; i < state->numSlices; i++)
	{
		int32_t markerX = ft2_sample_smpPos2Scr(inst, state->slicePos[i]);
		if (abs(screenX - markerX) <= 4)
			return (int8_t)i;
	}

	return -1;
}

void slicerStartMarkerDrag(ft2_instance_t *inst, int8_t markerIdx)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (markerIdx > 0 && markerIdx < state->numSlices)
		state->draggedMarker = markerIdx;
}

void slicerDragMarker(ft2_instance_t *inst, int32_t screenX)
{
	if (!inst) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	if (state->draggedMarker < 1 || state->draggedMarker >= state->numSlices) return;

	ft2_sample_t *s = getCurrentSample(inst);
	if (!s) return;

	/* Clear any slice selection when dragging markers */
	if (state->selectedSlice >= 0)
	{
		state->selectedSlice = -1;
		ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
		if (ed) ed->hasRange = false;
	}

	int32_t newPos = ft2_sample_scr2SmpPos(inst, screenX);

	/* Clamp to neighboring markers */
	int32_t minPos = state->slicePos[state->draggedMarker - 1] + 1;
	int32_t maxPos = state->slicePos[state->draggedMarker + 1] - 1;

	if (newPos < minPos) newPos = minPos;
	if (newPos > maxPos) newPos = maxPos;

	state->slicePos[state->draggedMarker] = newPos;
	ft2_sample_ed_mark_overlay_dirty(inst);
}

void slicerEndMarkerDrag(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_slicer_state_t *state = SLICER_STATE(inst);
	state->draggedMarker = -1;
}

/* ------------------------------------------------------------------------- */
/*                        SLICE PREVIEW                                      */
/* ------------------------------------------------------------------------- */

void slicerPreviewSlice(ft2_instance_t *inst, int32_t screenX)
{
	if (!inst || !inst->uiState.sampleEditorSlicerShown) return;

	ft2_slicer_state_t *state = SLICER_STATE(inst);
	ft2_sample_t *s = getCurrentSample(inst);
	ft2_sample_format_t format = ft2_sample_get_format(s);
	if (!s || !ft2_sample_has_data(s, format)) return;

	int32_t smpPos = ft2_sample_scr2SmpPos(inst, screenX);

	/* Find which slice contains this position */
	int sliceIdx = -1;
	int i = 0;
	while (i < state->numSlices)
	{
		if (smpPos >= state->slicePos[i] && smpPos < state->slicePos[i + 1])
		{
			sliceIdx = i;
			break;
		}
		i++;
	}

	if (sliceIdx < 0) return;

	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed) return;

	/* Store selected slice and set visual range markers */
	state->selectedSlice = (int8_t)sliceIdx;
	ed->rangeStart = state->slicePos[sliceIdx];
	ed->rangeEnd = state->slicePos[sliceIdx + 1];
	ed->hasRange = true;
	ft2_sample_ed_mark_overlay_dirty(inst);

	/* Play the slice */
	uint8_t ch = inst->cursor.ch;
	uint8_t instr = inst->editor.curInstr;
	uint8_t smp = inst->editor.curSmp;
	uint8_t note = inst->editor.smpEd_NoteNr + 1;

	int32_t sliceStart = state->slicePos[sliceIdx];
	int32_t sliceLen = state->slicePos[sliceIdx + 1] - sliceStart;

	ft2_instance_play_sample(inst, note, instr, smp, ch, 64, sliceStart, sliceLen);
}

/* ------------------------------------------------------------------------- */
/*                        STATE ACCESSORS                                    */
/* ------------------------------------------------------------------------- */

uint8_t getSlicerNumSlices(ft2_instance_t *inst)
{
	return inst ? SLICER_STATE(inst)->numSlices : 8;
}

uint8_t getSlicerSensitivity(ft2_instance_t *inst)
{
	return inst ? SLICER_STATE(inst)->sensitivity : 50;
}

uint8_t getSlicerRootNote(ft2_instance_t *inst)
{
	return inst ? SLICER_STATE(inst)->rootNote : 48;
}
