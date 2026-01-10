/*
** FT2 Plugin - Trim Screen
** Port of ft2_trim.c: removes unused patterns/instruments/samples/channels,
** truncates sample data after loop, converts samples to 8-bit.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "ft2_plugin_trim.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_replayer.h"
#include "../ft2_instance.h"

#define TRIM_STATE(inst) (&FT2_UI(inst)->trimState)

/* Static temp storage - trim operations are modal/atomic */
static char tmpInstrName[129][23], tmpInstName[128][23];
static uint8_t instrUsed[128], instrOrder[128], pattUsed[256], pattOrder[256];
static int16_t oldPattLens[256], tmpPattLens[256];
static ft2_note_t *oldPatts[256], *tmpPatt[256];
static ft2_instr_t *tmpInstr[129], *tmpInst[128];

static int64_t calculateXMSize(ft2_instance_t *inst);
static int64_t calculateTrimSize(ft2_instance_t *inst);
static const char *formatBytes(ft2_instance_t *inst, uint64_t bytes, bool roundUp);
static void freeTmpInstruments(void);
static bool setTmpInstruments(ft2_instance_t *inst);

/* ------------------------------------------------------------------------- */
/*                        BYTE FORMATTING                                    */
/* ------------------------------------------------------------------------- */

/* Formats byte count as human-readable string (B/kB/MB/GB). Per-instance buffer. */
static const char *formatBytes(ft2_instance_t *inst, uint64_t bytes, bool roundUp)
{
	ft2_trim_state_t *trim = TRIM_STATE(inst);
	char *buf = trim->byteFormatBuffer;

	if (bytes == 0) { strcpy(buf, "0"); return buf; }

	bytes %= 1000ULL*1024*1024*999;
	double dBytes;

	if (bytes >= 1024ULL*1024*1024*9)
	{
		dBytes = bytes / (1024.0*1024.0*1024.0);
		(dBytes < 100) ? sprintf(buf, "%.1fGB", dBytes)
		               : sprintf(buf, "%dGB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*1024*9)
	{
		dBytes = bytes / (1024.0*1024.0);
		(dBytes < 100) ? sprintf(buf, "%.1fMB", dBytes)
		               : sprintf(buf, "%dMB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*9)
	{
		dBytes = bytes / 1024.0;
		(dBytes < 100) ? sprintf(buf, "%.1fkB", dBytes)
		               : sprintf(buf, "%dkB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else
		sprintf(buf, "%d", (int32_t)bytes);

	return buf;
}

/* ------------------------------------------------------------------------- */
/*                            DRAWING                                        */
/* ------------------------------------------------------------------------- */

void drawTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (!ui) return;
	ft2_widgets_t *widgets = &ui->widgets;
	ft2_trim_state_t *trim = TRIM_STATE(inst);

	drawFramework(video, 0,   92, 136, 81, FRAMEWORK_TYPE1);
	drawFramework(video, 136, 92, 155, 81, FRAMEWORK_TYPE1);

	/* Labels */
	textOutShadow(video, bmp, 4,    95, PAL_FORGRND, PAL_DSKTOP2, "What to remove:");
	textOutShadow(video, bmp, 19,  109, PAL_FORGRND, PAL_DSKTOP2, "Unused patterns");
	textOutShadow(video, bmp, 19,  122, PAL_FORGRND, PAL_DSKTOP2, "Unused instruments");
	textOutShadow(video, bmp, 19,  135, PAL_FORGRND, PAL_DSKTOP2, "Unused samples");
	textOutShadow(video, bmp, 19,  148, PAL_FORGRND, PAL_DSKTOP2, "Unused channels");
	textOutShadow(video, bmp, 19,  161, PAL_FORGRND, PAL_DSKTOP2, "Smp. dat. after loop");
	textOutShadow(video, bmp, 155,  96, PAL_FORGRND, PAL_DSKTOP2, "Conv. samples to 8-bit");
	textOutShadow(video, bmp, 140, 111, PAL_FORGRND, PAL_DSKTOP2, ".xm size before");
	textOutShadow(video, bmp, 140, 124, PAL_FORGRND, PAL_DSKTOP2, ".xm size after");
	textOutShadow(video, bmp, 140, 137, PAL_FORGRND, PAL_DSKTOP2, "Bytes to save");

	/* Size displays (right-aligned) */
	char sizeBuf[16];
	const char *s;

	s = (trim->xmSize64 > -1) ? (sprintf(sizeBuf, "%s", formatBytes(inst, trim->xmSize64, true)), sizeBuf) : "Unknown";
	textOut(video, bmp, 287 - textWidth(s), 111, PAL_FORGRND, s);

	s = (trim->xmAfterTrimSize64 > -1) ? (sprintf(sizeBuf, "%s", formatBytes(inst, trim->xmAfterTrimSize64, true)), sizeBuf) : "Unknown";
	textOut(video, bmp, 287 - textWidth(s), 124, PAL_FORGRND, s);

	s = (trim->spaceSaved64 > -1) ? (sprintf(sizeBuf, "%s", formatBytes(inst, trim->spaceSaved64, false)), sizeBuf) : "Unknown";
	textOut(video, bmp, 287 - textWidth(s), 137, PAL_FORGRND, s);

	/* Widgets */
	showCheckBox(widgets, video, bmp, CB_TRIM_PATT);
	showCheckBox(widgets, video, bmp, CB_TRIM_INST);
	showCheckBox(widgets, video, bmp, CB_TRIM_SAMP);
	showCheckBox(widgets, video, bmp, CB_TRIM_CHAN);
	showCheckBox(widgets, video, bmp, CB_TRIM_SMPD);
	showCheckBox(widgets, video, bmp, CB_TRIM_CONV);
	showPushButton(widgets, video, bmp, PB_TRIM_CALC);
	showPushButton(widgets, video, bmp, PB_TRIM_TRIM);
}

/* ------------------------------------------------------------------------- */
/*                       SCREEN VISIBILITY                                   */
/* ------------------------------------------------------------------------- */

void showTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst) return;
	if (inst->uiState.extendedPatternEditor) exitPatternEditorExtended(inst);
	hideAllTopLeftPanelOverlays(inst);

	inst->uiState.trimScreenShown = true;
	inst->uiState.scopesShown = false;

	if (video) drawTrimScreen(inst, video, bmp);
	inst->uiState.needsFullRedraw = true;
}

void hideTrimScreen(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (!ui) return;
	ft2_widgets_t *widgets = &ui->widgets;

	hideCheckBox(widgets, CB_TRIM_PATT);
	hideCheckBox(widgets, CB_TRIM_INST);
	hideCheckBox(widgets, CB_TRIM_SAMP);
	hideCheckBox(widgets, CB_TRIM_CHAN);
	hideCheckBox(widgets, CB_TRIM_SMPD);
	hideCheckBox(widgets, CB_TRIM_CONV);
	hidePushButton(widgets, PB_TRIM_CALC);
	hidePushButton(widgets, PB_TRIM_TRIM);

	inst->uiState.trimScreenShown = false;
	inst->uiState.scopesShown = true;
	ui->scopes.needsFrameworkRedraw = true;
	inst->uiState.needsFullRedraw = true;
}

void toggleTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst) return;
	inst->uiState.trimScreenShown ? hideTrimScreen(inst) : showTrimScreen(inst, video, bmp);
}

/* ------------------------------------------------------------------------- */
/*                        INITIALIZATION                                     */
/* ------------------------------------------------------------------------- */

void setInitialTrimFlags(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_trim_state_t *trim = TRIM_STATE(inst);
	ft2_widgets_t *widgets = &ui->widgets;

	trim->removePatt = trim->removeInst = trim->removeSamp = true;
	trim->removeChans = trim->removeSmpDataAfterLoop = true;
	trim->convSmpsTo8Bit = false;
	trim->xmSize64 = trim->xmAfterTrimSize64 = trim->spaceSaved64 = -1;

	widgets->checkBoxChecked[CB_TRIM_PATT] = widgets->checkBoxChecked[CB_TRIM_INST] = true;
	widgets->checkBoxChecked[CB_TRIM_SAMP] = widgets->checkBoxChecked[CB_TRIM_CHAN] = true;
	widgets->checkBoxChecked[CB_TRIM_SMPD] = true;
	widgets->checkBoxChecked[CB_TRIM_CONV] = false;
}

void resetTrimSizes(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_trim_state_t *trim = TRIM_STATE(inst);
	trim->xmSize64 = trim->xmAfterTrimSize64 = trim->spaceSaved64 = -1;
	if (inst->uiState.trimScreenShown)
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		drawTrimScreen(inst, &ui->video, &ui->bmp);
	}
}

/* ------------------------------------------------------------------------- */
/*                       CHECKBOX CALLBACKS                                  */
/* ------------------------------------------------------------------------- */

void cbTrimUnusedPatt(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->removePatt ^= 1; }
void cbTrimUnusedInst(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->removeInst ^= 1; }
void cbTrimUnusedSamp(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->removeSamp ^= 1; }
void cbTrimUnusedChans(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->removeChans ^= 1; }
void cbTrimUnusedSmpData(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->removeSmpDataAfterLoop ^= 1; }
void cbTrimSmpsTo8Bit(ft2_instance_t *inst) { if (inst && inst->ui) TRIM_STATE(inst)->convSmpsTo8Bit ^= 1; }

/* ------------------------------------------------------------------------- */
/*                            HELPERS                                        */
/* ------------------------------------------------------------------------- */

/* Frees temp instrument copies (sample pointers shared with originals) */
static void freeTmpInstruments(void)
{
	for (int32_t i = 0; i <= 128; i++)
		if (tmpInstr[i]) { free(tmpInstr[i]); tmpInstr[i] = NULL; }
}

/* Creates shallow copies of all instruments for size calculations */
static bool setTmpInstruments(ft2_instance_t *inst)
{
	freeTmpInstruments();
	for (int32_t i = 0; i <= 128; i++)
	{
		if (inst->replayer.instr[i])
		{
			tmpInstr[i] = (ft2_instr_t *)malloc(sizeof(ft2_instr_t));
			if (!tmpInstr[i]) { freeTmpInstruments(); return false; }
			*tmpInstr[i] = *inst->replayer.instr[i];
		}
	}
	return true;
}

/* Returns highest sample slot used by instrument (considering note2SampleLUT) */
static int16_t getUsedSamples(ft2_instance_t *inst, uint16_t insNum)
{
	if (!inst->replayer.instr[insNum]) return 0;
	ft2_instr_t *ins = inst->replayer.instr[insNum];

	int16_t i = 15;
	while (i >= 0 && !ins->smp[i].dataPtr && !ins->smp[i].name[0]) i--;
	for (int16_t j = 0; j < 96; j++) if (ins->note2SampleLUT[j] > i) i = ins->note2SampleLUT[j];
	return i + 1;
}

static int16_t getUsedTempSamples(uint16_t insNum)
{
	if (!tmpInstr[insNum]) return 0;
	ft2_instr_t *ins = tmpInstr[insNum];

	int16_t i = 15;
	while (i >= 0 && !ins->smp[i].dataPtr && !ins->smp[i].name[0]) i--;
	for (int16_t j = 0; j < 96; j++) if (ins->note2SampleLUT[j] > i) i = ins->note2SampleLUT[j];
	return i + 1;
}

static bool tmpPatternEmpty(uint16_t pattNum, int32_t numChannels)
{
	if (!tmpPatt[pattNum]) return true;

	uint8_t *scanPtr = (uint8_t *)tmpPatt[pattNum];
	int32_t scanLen = numChannels * sizeof(ft2_note_t);
	int32_t numRows = tmpPattLens[pattNum];

	for (int32_t i = 0; i < numRows; i++, scanPtr += (FT2_MAX_CHANNELS * sizeof(ft2_note_t)))
		for (int32_t j = 0; j < scanLen; j++)
			if (scanPtr[j]) return false;
	return true;
}

/* ------------------------------------------------------------------------- */
/*                      XM SIZE CALCULATION                                  */
/* ------------------------------------------------------------------------- */

#define XM_HEADER_SIZE        336
#define XM_INSTR_HEADER_SIZE  263
#define XM_SAMPLE_HEADER_SIZE 40
#define XM_PATT_HEADER_SIZE   9

/* Calculates packed pattern data size (XM RLE-like compression) */
static uint16_t getPackedPattSize(ft2_note_t *p, int32_t numRows, int32_t antChn)
{
	uint8_t bytes[5];
	uint16_t totalPackLen = 0;
	uint8_t *pattPtr = (uint8_t *)p, *writePtr = pattPtr;

	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < antChn; chn++)
		{
			bytes[0] = *pattPtr++; bytes[1] = *pattPtr++; bytes[2] = *pattPtr++;
			bytes[3] = *pattPtr++; bytes[4] = *pattPtr++;

			uint8_t *firstBytePtr = writePtr++;
			uint8_t packBits = 0;
			if (bytes[0]) { packBits |= 1; writePtr++; }
			if (bytes[1]) { packBits |= 2; writePtr++; }
			if (bytes[2]) { packBits |= 4; writePtr++; }
			if (bytes[3]) { packBits |= 8; writePtr++; }

			if (packBits == 15) { totalPackLen += 5; writePtr += 5; continue; }
			if (bytes[4]) writePtr++;
			totalPackLen += (uint16_t)(writePtr - firstBytePtr);
		}
		pattPtr += sizeof(ft2_note_t) * (FT2_MAX_CHANNELS - antChn);
	}
	return totalPackLen;
}

static int64_t getTempInsAndSmpSize(ft2_instance_t *inst)
{
	(void)inst;
	int16_t ai = 128;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && !tmpInstrName[ai][0]) ai--;

	int64_t currSize64 = 0;
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t j = tmpInstr[i] ? i : 0;
		const int16_t a = getUsedTempSamples(i);
		currSize64 += (a > 0) ? XM_INSTR_HEADER_SIZE + (a * XM_SAMPLE_HEADER_SIZE) : 22 + 11;

		ft2_instr_t *ins = tmpInstr[j];
		for (int16_t k = 0; k < a; k++)
		{
			ft2_sample_t *s = &ins->smp[k];
			if (s->dataPtr && s->length > 0)
				currSize64 += (s->flags & FT2_SAMPLE_16BIT) ? s->length << 1 : s->length;
		}
	}
	return currSize64;
}

static int64_t calculateXMSize(ft2_instance_t *inst)
{
	if (!inst) return 0;

	int64_t currSize64 = XM_HEADER_SIZE;

	/* Count non-empty patterns */
	int16_t ap = 256;
	while (ap > 0 && patternEmpty(inst, ap - 1)) ap--;

	/* Count instruments with data or names */
	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(inst, ai) == 0 && !inst->replayer.song.instrName[ai][0]) ai--;

	/* Pattern data */
	for (int16_t i = 0; i < ap; i++)
	{
		currSize64 += XM_PATT_HEADER_SIZE;
		if (!patternEmpty(inst, i))
			currSize64 += getPackedPattSize(inst->replayer.pattern[i], inst->replayer.patternNumRows[i], inst->replayer.song.numChannels);
	}

	/* Instrument + sample data */
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t j = inst->replayer.instr[i] ? i : 0;
		const int16_t a = getUsedSamples(inst, i);
		currSize64 += (a > 0) ? XM_INSTR_HEADER_SIZE + (a * XM_SAMPLE_HEADER_SIZE) : 22 + 11;

		ft2_instr_t *ins = inst->replayer.instr[j];
		for (int16_t k = 0; k < a; k++)
		{
			ft2_sample_t *s = &ins->smp[k];
			if (s->dataPtr && s->length > 0)
				currSize64 += (s->flags & FT2_SAMPLE_16BIT) ? s->length << 1 : s->length;
		}
	}
	return currSize64;
}

static void wipePattsUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ap);
static void wipeInstrUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ai, int32_t ap, int32_t antChn);
static void wipeSamplesUnused(ft2_instance_t *inst, bool testWipeSize, int16_t ai);
static void wipeSmpDataAfterLoop(ft2_instance_t *inst, bool testWipeSize, int16_t ai);
static void convertSamplesTo8bit(ft2_instance_t *inst, bool testWipeSize, int16_t ai);

/* Calculates bytes saved by applying all enabled trim options */
static int64_t calculateTrimSize(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return 0;
	ft2_trim_state_t *trim = TRIM_STATE(inst);

	int32_t numChannels = inst->replayer.song.numChannels;
	int32_t pattDataLen = 0, newPattDataLen = 0;
	int64_t bytes64 = 0, oldInstrSize64 = 0;

	memcpy(tmpPatt, inst->replayer.pattern, sizeof(tmpPatt));
	memcpy(tmpPattLens, inst->replayer.patternNumRows, sizeof(tmpPattLens));
	memcpy(tmpInstrName, inst->replayer.song.instrName, sizeof(tmpInstrName));

	if (!setTmpInstruments(inst))
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_message(&ui->dialog, "System request", "Not enough memory!");
		return 0;
	}

	if (trim->removeInst || trim->removeSamp || trim->removeSmpDataAfterLoop || trim->convSmpsTo8Bit)
		oldInstrSize64 = getTempInsAndSmpSize(inst);

	int16_t ap = 256;
	while (ap > 0 && tmpPatternEmpty(ap - 1, numChannels)) ap--;

	int16_t ai = 128;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && !tmpInstrName[ai][0]) ai--;

	if (trim->removeSamp) wipeSamplesUnused(inst, true, ai);
	if (trim->removeSmpDataAfterLoop) wipeSmpDataAfterLoop(inst, true, ai);
	if (trim->convSmpsTo8Bit) convertSamplesTo8bit(inst, true, ai);

	if (trim->removeChans || trim->removePatt)
	{
		for (int16_t i = 0; i < ap; i++)
		{
			pattDataLen += XM_PATT_HEADER_SIZE;
			if (!tmpPatternEmpty(i, numChannels))
				pattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}
	}

	if (trim->removeChans)
	{
		int16_t highestChan = -1;
		for (int16_t i = 0; i < ap; i++)
		{
			ft2_note_t *pattPtr = tmpPatt[i];
			if (!pattPtr) continue;
			const int16_t numRows = tmpPattLens[i];
			for (int32_t j = 0; j < numRows; j++)
				for (int32_t k = 0; k < numChannels; k++)
				{
					ft2_note_t *p = &pattPtr[(j * FT2_MAX_CHANNELS) + k];
					if (p->note || p->instr || p->vol || p->efx || p->efxData)
						if (k > highestChan) highestChan = k;
				}
		}
		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1) highestChan++;
			if (highestChan < 2) highestChan = 2;
			if (highestChan > numChannels) highestChan = numChannels;
			numChannels = highestChan;
		}
	}

	if (trim->removePatt) wipePattsUnused(inst, true, &ap);

	if (trim->removeChans || trim->removePatt)
	{
		for (int16_t i = 0; i < ap; i++)
		{
			newPattDataLen += XM_PATT_HEADER_SIZE;
			if (!tmpPatternEmpty(i, numChannels))
				newPattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}
		if (pattDataLen > newPattDataLen) bytes64 += (pattDataLen - newPattDataLen);
	}

	if (trim->removeInst) wipeInstrUnused(inst, true, &ai, ap, numChannels);

	if (trim->removeInst || trim->removeSamp || trim->removeSmpDataAfterLoop || trim->convSmpsTo8Bit)
	{
		int64_t newInstrSize64 = getTempInsAndSmpSize(inst);
		if (oldInstrSize64 > newInstrSize64) bytes64 += (oldInstrSize64 - newInstrSize64);
	}

	freeTmpInstruments();
	return bytes64;
}

/* ------------------------------------------------------------------------- */
/*                       TRIM OPERATIONS                                     */
/* ------------------------------------------------------------------------- */

/* Removes patterns not referenced in order list, remaps remaining */
static void wipePattsUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ap)
{
	uint8_t newPatt;
	int16_t i, *pLens;
	ft2_note_t **p;

	int16_t usedPatts = *ap;
	memset(pattUsed, 0, usedPatts);

	int16_t newUsedPatts = 0;
	for (i = 0; i < inst->replayer.song.songLength; i++)
	{
		newPatt = inst->replayer.song.orders[i];
		if (newPatt < usedPatts && !pattUsed[newPatt])
		{
			pattUsed[newPatt] = true;
			newUsedPatts++;
		}
	}

	if (newUsedPatts == 0 || newUsedPatts == usedPatts)
		return;

	newPatt = 0;
	memset(pattOrder, 0, usedPatts);
	for (i = 0; i < usedPatts; i++)
	{
		if (pattUsed[i])
			pattOrder[i] = newPatt++;
	}

	if (testWipeSize)
	{
		p = tmpPatt;
		pLens = tmpPattLens;
	}
	else
	{
		p = inst->replayer.pattern;
		pLens = inst->replayer.patternNumRows;
	}

	memcpy(oldPatts, p, usedPatts * sizeof(ft2_note_t *));
	memcpy(oldPattLens, pLens, usedPatts * sizeof(int16_t));
	memset(p, 0, usedPatts * sizeof(ft2_note_t *));
	memset(pLens, 0, usedPatts * sizeof(int16_t));

	/* Relocate patterns */
	for (i = 0; i < usedPatts; i++)
	{
		p[i] = NULL;

		if (!pattUsed[i])
		{
			if (!testWipeSize && oldPatts[i] != NULL)
			{
				free(oldPatts[i]);
				oldPatts[i] = NULL;
			}
		}
		else
		{
			newPatt = pattOrder[i];
			p[newPatt] = oldPatts[i];
			pLens[newPatt] = oldPattLens[i];
		}
	}

	if (!testWipeSize)
	{
		for (i = 0; i < 256; i++)
		{
			if (inst->replayer.pattern[i] == NULL)
				inst->replayer.patternNumRows[i] = 64;
		}

		/* Reorder order list */
		for (i = 0; i < 256; i++)
		{
			if (i < inst->replayer.song.songLength)
				inst->replayer.song.orders[i] = pattOrder[inst->replayer.song.orders[i]];
			else
				inst->replayer.song.orders[i] = 0;
		}
	}

	*ap = newUsedPatts;
}

/* Remaps instrument number in all pattern data */
static void remapInstrInSong(ft2_instance_t *inst, uint8_t src, uint8_t dst, int32_t ap)
{
	for (int32_t i = 0; i < ap; i++)
	{
		ft2_note_t *pattPtr = inst->replayer.pattern[i];
		if (!pattPtr) continue;
		const int32_t readLen = inst->replayer.patternNumRows[i] * FT2_MAX_CHANNELS;
		for (int32_t j = 0; j < readLen; j++)
			if (pattPtr[j].instr == src) pattPtr[j].instr = dst;
	}
}

/* Removes instruments not used in any pattern, remaps remaining */
static void wipeInstrUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ai, int32_t ap, int32_t antChn)
{
	int32_t numInsts = *ai;

	memset(instrUsed, 0, numInsts);
	for (int32_t i = 0; i < ap; i++)
	{
		ft2_note_t *p = testWipeSize ? tmpPatt[i] : inst->replayer.pattern[i];
		int16_t numRows = testWipeSize ? tmpPattLens[i] : inst->replayer.patternNumRows[i];
		if (!p) continue;

		for (int32_t j = 0; j < numRows; j++)
			for (int32_t k = 0; k < antChn; k++)
			{
				uint8_t ins = p[(j * FT2_MAX_CHANNELS) + k].instr;
				if (ins > 0 && ins <= 128) instrUsed[ins - 1] = true;
			}
	}

	int16_t instToDel = 0;
	uint8_t newInst = 0;
	int16_t newNumInsts = 0;
	int32_t i;

	memset(instrOrder, 0, numInsts);
	for (i = 0; i < numInsts; i++)
	{
		if (instrUsed[i]) { newNumInsts++; instrOrder[i] = newInst++; }
		else instToDel++;
	}

	if (instToDel == 0) return;

	if (testWipeSize)
	{
		for (i = 0; i < numInsts; i++)
			if (!instrUsed[i] && tmpInstr[1 + i]) { free(tmpInstr[1 + i]); tmpInstr[1 + i] = NULL; }

		memcpy(tmpInstName, &tmpInstrName[1], 128 * sizeof(tmpInstrName[0]));
		memcpy(tmpInst, &tmpInstr[1], 128 * sizeof(tmpInstr[0]));
		memset(&tmpInstr[1], 0, numInsts * sizeof(tmpInstr[0]));
		memset(&tmpInstrName[1], 0, numInsts * sizeof(tmpInstrName[0]));

		for (i = 0; i < numInsts; i++)
			if (instrUsed[i])
			{
				newInst = instrOrder[i];
				memcpy(&tmpInstr[1 + newInst], &tmpInst[i], sizeof(tmpInst[0]));
				strcpy(tmpInstrName[1 + newInst], tmpInstName[i]);
			}

		*ai = newNumInsts;
		return;
	}

	for (i = 0; i < numInsts; i++)
		if (!instrUsed[i]) ft2_instance_free_instr(inst, 1 + i);

	static char localInstName[128][23];
	static ft2_instr_t *localInst[128];

	memcpy(localInstName, &inst->replayer.song.instrName[1], 128 * sizeof(inst->replayer.song.instrName[0]));
	memcpy(localInst, &inst->replayer.instr[1], 128 * sizeof(inst->replayer.instr[0]));
	memset(&inst->replayer.instr[1], 0, numInsts * sizeof(inst->replayer.instr[0]));
	memset(&inst->replayer.song.instrName[1], 0, numInsts * sizeof(inst->replayer.song.instrName[0]));

	for (i = 0; i < numInsts; i++)
		if (instrUsed[i])
		{
			newInst = instrOrder[i];
			remapInstrInSong(inst, 1 + (uint8_t)i, 1 + newInst, ap);
			memcpy(&inst->replayer.instr[1 + newInst], &localInst[i], sizeof(localInst[0]));
			strcpy(inst->replayer.song.instrName[1 + newInst], localInstName[i]);
		}

	*ai = newNumInsts;

	setTmpInstruments(inst);
}

/* Removes samples not referenced by note2SampleLUT, remaps remaining */
static void wipeSamplesUnused(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	uint8_t smpUsed[16], smpOrder[16];
	ft2_sample_t tempSamples[16];

	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t l;
		ft2_instr_t *ins;
		if (!testWipeSize)
			{ l = inst->replayer.instr[i] ? i : 0; ins = inst->replayer.instr[l]; l = getUsedSamples(inst, i); }
		else
			{ l = tmpInstr[i] ? i : 0; ins = tmpInstr[l]; l = getUsedTempSamples(i); }

		memset(smpUsed, 0, l);
		if (l > 0)
		{
			ft2_sample_t *s = ins->smp;
			for (int16_t j = 0; j < l; j++, s++)
			{
				int16_t k;
				for (k = 0; k < 96; k++) if (ins->note2SampleLUT[k] == j) { smpUsed[j] = true; break; }
				if (k == 96)
				{
					if (s->dataPtr && !testWipeSize) freeSmpData(inst, i, j);
					memset(s, 0, sizeof(ft2_sample_t));
				}
			}

			uint8_t newSamp = 0;
			memset(smpOrder, 0, l);
			for (int16_t j = 0; j < l; j++) if (smpUsed[j]) smpOrder[j] = newSamp++;

			memcpy(tempSamples, ins->smp, l * sizeof(ft2_sample_t));
			memset(ins->smp, 0, l * sizeof(ft2_sample_t));
			for (int16_t j = 0; j < l; j++) if (smpUsed[j]) ins->smp[smpOrder[j]] = tempSamples[j];

			for (int16_t j = 0; j < 96; j++)
			{
				newSamp = ins->note2SampleLUT[j];
				ins->note2SampleLUT[j] = smpUsed[newSamp] ? smpOrder[newSamp] : 0;
			}
		}
	}
}

/* Truncates sample data past loop end */
static void wipeSmpDataAfterLoop(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t l;
		ft2_instr_t *ins;
		if (!testWipeSize)
			{ l = inst->replayer.instr[i] ? i : 0; ins = inst->replayer.instr[l]; l = getUsedSamples(inst, i); }
		else
			{ l = tmpInstr[i] ? i : 0; ins = tmpInstr[l]; l = getUsedTempSamples(i); }

		ft2_sample_t *s = ins->smp;
		for (int16_t j = 0; j < l; j++, s++)
		{
			uint8_t loopType = s->flags & 3;
			if (s->dataPtr && loopType != FT2_LOOP_OFF && s->length > 0 && s->length > s->loopStart + s->loopLength)
			{
				s->length = s->loopStart + s->loopLength;
				if (!testWipeSize && s->length <= 0) { s->length = 0; freeSmpData(inst, i, j); }
			}
		}
	}
}

/* Converts 16-bit samples to 8-bit in-place */
static void convertSamplesTo8bit(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t k;
		ft2_instr_t *ins;
		if (!testWipeSize)
			{ k = inst->replayer.instr[i] ? i : 0; ins = inst->replayer.instr[k]; k = getUsedSamples(inst, i); }
		else
			{ k = tmpInstr[i] ? i : 0; ins = tmpInstr[k]; k = getUsedTempSamples(i); }

		ft2_sample_t *s = ins->smp;
		for (int16_t j = 0; j < k; j++, s++)
		{
			if (s->dataPtr && s->length > 0 && (s->flags & FT2_SAMPLE_16BIT))
			{
				if (!testWipeSize)
				{
					const int16_t *src16 = (const int16_t *)s->dataPtr;
					int8_t *dst8 = s->dataPtr;
					for (int32_t a = 0; a < s->length; a++) dst8[a] = src16[a] >> 8;
				}
				s->flags &= ~FT2_SAMPLE_16BIT;
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                        BUTTON CALLBACKS                                   */
/* ------------------------------------------------------------------------- */

void pbTrimCalc(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_trim_state_t *trim = TRIM_STATE(inst);

	trim->xmSize64 = calculateXMSize(inst);
	trim->spaceSaved64 = calculateTrimSize(inst);
	trim->xmAfterTrimSize64 = trim->xmSize64 - trim->spaceSaved64;
	if (trim->xmAfterTrimSize64 < 0) trim->xmAfterTrimSize64 = 0;

	if (inst->uiState.trimScreenShown)
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		drawTrimScreen(inst, &ui->video, &ui->bmp);
	}
}

static void doTrimConfirmed(ft2_instance_t *inst, ft2_dialog_result_t result, 
                            const char *inputText, void *userData)
{
	(void)inputText; (void)userData;
	if (!inst || !inst->ui || result != DIALOG_RESULT_YES) return;
	ft2_trim_state_t *trim = TRIM_STATE(inst);

	int16_t ap = 256;
	while (ap > 0 && patternEmpty(inst, ap - 1)) ap--;

	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(inst, ai) == 0 && !inst->replayer.song.instrName[ai][0]) ai--;

	if (!setTmpInstruments(inst))
	{
		ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
		if (ui) ft2_dialog_show_message(&ui->dialog, "System request", "Not enough memory!");
		return;
	}

	ft2_stop_all_voices(inst);

	if (trim->removeSamp) wipeSamplesUnused(inst, false, ai);
	if (trim->removeSmpDataAfterLoop) wipeSmpDataAfterLoop(inst, false, ai);
	if (trim->convSmpsTo8Bit) convertSamplesTo8bit(inst, false, ai);

	if (trim->removeChans)
	{
		int16_t highestChan = -1;
		for (int32_t i = 0; i < ap; i++)
		{
			ft2_note_t *pattPtr = inst->replayer.pattern[i];
			if (!pattPtr) continue;
			const int16_t numRows = inst->replayer.patternNumRows[i];
			for (int32_t j = 0; j < numRows; j++)
				for (int32_t k = 0; k < inst->replayer.song.numChannels; k++)
				{
					ft2_note_t *p = &pattPtr[(j * FT2_MAX_CHANNELS) + k];
					if (p->note || p->vol || p->instr || p->efx || p->efxData)
						if (k > highestChan) highestChan = k;
				}
		}

		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1) highestChan++;
			if (highestChan < 2) highestChan = 2;
			if (highestChan > inst->replayer.song.numChannels) highestChan = inst->replayer.song.numChannels;
			inst->replayer.song.numChannels = (uint8_t)highestChan;
		}

		if (inst->replayer.song.numChannels < FT2_MAX_CHANNELS)
		{
			for (int32_t i = 0; i < 256; i++)
			{
				ft2_note_t *p = inst->replayer.pattern[i];
				if (!p) continue;
				const int16_t numRows = inst->replayer.patternNumRows[i];
				for (int32_t j = 0; j < numRows; j++)
					memset(&p[(j * FT2_MAX_CHANNELS) + inst->replayer.song.numChannels], 0,
					       sizeof(ft2_note_t) * (FT2_MAX_CHANNELS - inst->replayer.song.numChannels));
			}
		}
	}

	if (trim->removePatt) wipePattsUnused(inst, false, &ap);
	if (trim->removeInst) wipeInstrUnused(inst, false, &ai, ap, inst->replayer.song.numChannels);

	freeTmpInstruments();
	ft2_song_mark_modified(inst);
	pbTrimCalc(inst);

	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.needsFullRedraw = true;
}

void pbTrimDoTrim(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_trim_state_t *trim = TRIM_STATE(inst);
	if (!trim->removePatt && !trim->removeInst && !trim->removeSamp && 
	    !trim->removeChans && !trim->removeSmpDataAfterLoop && !trim->convSmpsTo8Bit)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
		"Are you sure you want to trim the song? Making a backup of the song first is recommended.",
		inst, doTrimConfirmed, NULL);
}

