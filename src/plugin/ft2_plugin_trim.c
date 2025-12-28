/**
 * @file ft2_plugin_trim.c
 * @brief Trim screen functionality for the FT2 plugin.
 *
 * Ported from ft2_trim.c - allows trimming unused patterns, instruments,
 * samples, channels, and converting samples to 8-bit.
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

/* Module-local state for trim flags */
static bool removePatt = true;
static bool removeInst = true;
static bool removeSamp = true;
static bool removeChans = true;
static bool removeSmpDataAfterLoop = true;
static bool convSmpsTo8Bit = false;

/* Calculated sizes */
static int64_t xmSize64 = -1;
static int64_t xmAfterTrimSize64 = -1;
static int64_t spaceSaved64 = -1;

/* Temporary storage for trim calculations */
static char byteFormatBuffer[64];
static char tmpInstrName[129][23];    /* 1 + MAX_INST */
static char tmpInstName[128][23];     /* MAX_INST */
static uint8_t instrUsed[128], instrOrder[128], pattUsed[256], pattOrder[256];
static int16_t oldPattLens[256], tmpPattLens[256];
static ft2_note_t *oldPatts[256], *tmpPatt[256];
static ft2_instr_t *tmpInstr[129], *tmpInst[128];

/* Forward declarations */
static int64_t calculateXMSize(ft2_instance_t *inst);
static int64_t calculateTrimSize(ft2_instance_t *inst);
static const char *formatBytes(uint64_t bytes, bool roundUp);
static void freeTmpInstruments(void);
static bool setTmpInstruments(ft2_instance_t *inst);

/* ============ BYTE FORMATTING ============ */

static const char *formatBytes(uint64_t bytes, bool roundUp)
{
	double dBytes;

	if (bytes == 0)
	{
		strcpy(byteFormatBuffer, "0");
		return byteFormatBuffer;
	}

	bytes %= 1000ULL*1024*1024*999; /* wrap around gigabytes in case of overflow */
	if (bytes >= 1024ULL*1024*1024*9)
	{
		/* gigabytes */
		dBytes = bytes / (1024.0*1024.0*1024.0);
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fGB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dGB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*1024*9)
	{
		/* megabytes */
		dBytes = bytes / (1024.0*1024.0);
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fMB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dMB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*9)
	{
		/* kilobytes */
		dBytes = bytes / 1024.0;
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fkB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dkB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else
	{
		/* bytes */
		sprintf(byteFormatBuffer, "%d", (int32_t)bytes);
	}

	return byteFormatBuffer;
}

/* ============ UI FUNCTIONS ============ */

void drawTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	char sizeBuf[16];

	if (inst == NULL || video == NULL)
		return;

	/* Draw frameworks */
	drawFramework(video, 0,   92, 136, 81, FRAMEWORK_TYPE1);
	drawFramework(video, 136, 92, 155, 81, FRAMEWORK_TYPE1);

	/* Left panel labels */
	textOutShadow(video, bmp, 4,    95, PAL_FORGRND, PAL_DSKTOP2, "What to remove:");
	textOutShadow(video, bmp, 19,  109, PAL_FORGRND, PAL_DSKTOP2, "Unused patterns");
	textOutShadow(video, bmp, 19,  122, PAL_FORGRND, PAL_DSKTOP2, "Unused instruments");
	textOutShadow(video, bmp, 19,  135, PAL_FORGRND, PAL_DSKTOP2, "Unused samples");
	textOutShadow(video, bmp, 19,  148, PAL_FORGRND, PAL_DSKTOP2, "Unused channels");
	textOutShadow(video, bmp, 19,  161, PAL_FORGRND, PAL_DSKTOP2, "Smp. dat. after loop");

	/* Right panel labels */
	textOutShadow(video, bmp, 155,  96, PAL_FORGRND, PAL_DSKTOP2, "Conv. samples to 8-bit");
	textOutShadow(video, bmp, 140, 111, PAL_FORGRND, PAL_DSKTOP2, ".xm size before");
	textOutShadow(video, bmp, 140, 124, PAL_FORGRND, PAL_DSKTOP2, ".xm size after");
	textOutShadow(video, bmp, 140, 137, PAL_FORGRND, PAL_DSKTOP2, "Bytes to save");

	/* Size displays */
	if (xmSize64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(xmSize64, true));
		textOut(video, bmp, 287 - textWidth(sizeBuf), 111, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(video, bmp, 287 - textWidth("Unknown"), 111, PAL_FORGRND, "Unknown");
	}

	if (xmAfterTrimSize64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(xmAfterTrimSize64, true));
		textOut(video, bmp, 287 - textWidth(sizeBuf), 124, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(video, bmp, 287 - textWidth("Unknown"), 124, PAL_FORGRND, "Unknown");
	}

	if (spaceSaved64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(spaceSaved64, false));
		textOut(video, bmp, 287 - textWidth(sizeBuf), 137, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(video, bmp, 287 - textWidth("Unknown"), 137, PAL_FORGRND, "Unknown");
	}

	/* Show checkboxes */
	showCheckBox(video, bmp, CB_TRIM_PATT);
	showCheckBox(video, bmp, CB_TRIM_INST);
	showCheckBox(video, bmp, CB_TRIM_SAMP);
	showCheckBox(video, bmp, CB_TRIM_CHAN);
	showCheckBox(video, bmp, CB_TRIM_SMPD);
	showCheckBox(video, bmp, CB_TRIM_CONV);

	/* Show buttons */
	showPushButton(video, bmp, PB_TRIM_CALC);
	showPushButton(video, bmp, PB_TRIM_TRIM);
}

void showTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);

	/* Hide other top-left overlays */
	if (inst->uiState.sampleEditorExtShown)
	{
		inst->uiState.sampleEditorExtShown = false;
		inst->uiState.scopesShown = true;
	}
	if (inst->uiState.instEditorExtShown)
	{
		inst->uiState.instEditorExtShown = false;
		inst->uiState.scopesShown = true;
	}
	if (inst->uiState.transposeShown)
	{
		hideTranspose(inst);
	}
	if (inst->uiState.advEditShown)
	{
		hideAdvEdit(inst);
	}

	inst->uiState.trimScreenShown = true;
	inst->uiState.scopesShown = false;

	if (video != NULL)
		drawTrimScreen(inst, video, bmp);

	inst->uiState.needsFullRedraw = true;
}

void hideTrimScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Hide checkboxes */
	hideCheckBox(CB_TRIM_PATT);
	hideCheckBox(CB_TRIM_INST);
	hideCheckBox(CB_TRIM_SAMP);
	hideCheckBox(CB_TRIM_CHAN);
	hideCheckBox(CB_TRIM_SMPD);
	hideCheckBox(CB_TRIM_CONV);

	/* Hide buttons */
	hidePushButton(PB_TRIM_CALC);
	hidePushButton(PB_TRIM_TRIM);

	inst->uiState.trimScreenShown = false;
	inst->uiState.scopesShown = true;

	/* Trigger scope framework redraw */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
		ui->scopes.needsFrameworkRedraw = true;

	inst->uiState.needsFullRedraw = true;
}

void toggleTrimScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;

	if (inst->uiState.trimScreenShown)
		hideTrimScreen(inst);
	else
		showTrimScreen(inst, video, bmp);
}

/* ============ INITIALIZATION ============ */

void setInitialTrimFlags(void)
{
	removePatt = true;
	removeInst = true;
	removeSamp = true;
	removeChans = true;
	removeSmpDataAfterLoop = true;
	convSmpsTo8Bit = false;

	checkBoxes[CB_TRIM_PATT].checked = true;
	checkBoxes[CB_TRIM_INST].checked = true;
	checkBoxes[CB_TRIM_SAMP].checked = true;
	checkBoxes[CB_TRIM_CHAN].checked = true;
	checkBoxes[CB_TRIM_SMPD].checked = true;
	checkBoxes[CB_TRIM_CONV].checked = false;
}

void resetTrimSizes(ft2_instance_t *inst)
{
	xmSize64 = -1;
	xmAfterTrimSize64 = -1;
	spaceSaved64 = -1;

	/* Redraw if trim screen is shown */
	if (inst != NULL && inst->uiState.trimScreenShown)
	{
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
			drawTrimScreen(inst, &ui->video, &ui->bmp);
	}
}

/* ============ CHECKBOX CALLBACKS ============ */

void cbTrimUnusedPatt(ft2_instance_t *inst)
{
	(void)inst;
	removePatt ^= 1;
}

void cbTrimUnusedInst(ft2_instance_t *inst)
{
	(void)inst;
	removeInst ^= 1;
}

void cbTrimUnusedSamp(ft2_instance_t *inst)
{
	(void)inst;
	removeSamp ^= 1;
}

void cbTrimUnusedChans(ft2_instance_t *inst)
{
	(void)inst;
	removeChans ^= 1;
}

void cbTrimUnusedSmpData(ft2_instance_t *inst)
{
	(void)inst;
	removeSmpDataAfterLoop ^= 1;
}

void cbTrimSmpsTo8Bit(ft2_instance_t *inst)
{
	(void)inst;
	convSmpsTo8Bit ^= 1;
}

/* ============ HELPER FUNCTIONS ============ */

static void freeTmpInstruments(void)
{
	for (int32_t i = 0; i <= 128; i++)
	{
		if (tmpInstr[i] != NULL)
		{
			/* don't free samples, as the pointers are shared with main instruments */
			free(tmpInstr[i]);
			tmpInstr[i] = NULL;
		}
	}
}

static bool setTmpInstruments(ft2_instance_t *inst)
{
	freeTmpInstruments();

	for (int32_t i = 0; i <= 128; i++)
	{
		if (inst->replayer.instr[i] != NULL)
		{
			tmpInstr[i] = (ft2_instr_t *)malloc(sizeof(ft2_instr_t));
			if (tmpInstr[i] == NULL)
			{
				freeTmpInstruments();
				return false;
			}

			*tmpInstr[i] = *inst->replayer.instr[i];
		}
	}

	return true;
}

static int16_t getUsedSamples(ft2_instance_t *inst, uint16_t insNum)
{
	if (inst->replayer.instr[insNum] == NULL)
		return 0;

	ft2_instr_t *ins = inst->replayer.instr[insNum];

	int16_t i = 16 - 1;
	while (i >= 0 && ins->smp[i].dataPtr == NULL && ins->smp[i].name[0] == '\0')
		i--;

	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->note2SampleLUT[j] > i)
			i = ins->note2SampleLUT[j];
	}

	return i + 1;
}

static int16_t getUsedTempSamples(uint16_t insNum)
{
	if (tmpInstr[insNum] == NULL)
		return 0;

	ft2_instr_t *ins = tmpInstr[insNum];

	int16_t i = 16 - 1;
	while (i >= 0 && ins->smp[i].dataPtr == NULL && ins->smp[i].name[0] == '\0')
		i--;

	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->note2SampleLUT[j] > i)
			i = ins->note2SampleLUT[j];
	}

	return i + 1;
}

static bool tmpPatternEmpty(uint16_t pattNum, int32_t numChannels)
{
	if (tmpPatt[pattNum] == NULL)
		return true;

	uint8_t *scanPtr = (uint8_t *)tmpPatt[pattNum];
	int32_t scanLen = numChannels * sizeof(ft2_note_t);
	int32_t numRows = tmpPattLens[pattNum];

	for (int32_t i = 0; i < numRows; i++, scanPtr += (FT2_MAX_CHANNELS * sizeof(ft2_note_t)))
	{
		for (int32_t j = 0; j < scanLen; j++)
		{
			if (scanPtr[j] != 0)
				return false;
		}
	}

	return true;
}

/* XM file structure sizes */
#define XM_HEADER_SIZE 336
#define XM_INSTR_HEADER_SIZE 263
#define XM_SAMPLE_HEADER_SIZE 40
#define XM_PATT_HEADER_SIZE 9

static uint16_t getPackedPattSize(ft2_note_t *p, int32_t numRows, int32_t antChn)
{
	uint8_t bytes[5];

	uint16_t totalPackLen = 0;
	uint8_t *pattPtr = (uint8_t *)p;
	uint8_t *writePtr = pattPtr;

	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < antChn; chn++)
		{
			bytes[0] = *pattPtr++;
			bytes[1] = *pattPtr++;
			bytes[2] = *pattPtr++;
			bytes[3] = *pattPtr++;
			bytes[4] = *pattPtr++;

			uint8_t *firstBytePtr = writePtr++;

			uint8_t packBits = 0;
			if (bytes[0] > 0) { packBits |= 1; writePtr++; }
			if (bytes[1] > 0) { packBits |= 2; writePtr++; }
			if (bytes[2] > 0) { packBits |= 4; writePtr++; }
			if (bytes[3] > 0) { packBits |= 8; writePtr++; }

			if (packBits == 15)
			{
				totalPackLen += 5;
				writePtr += 5;
				continue;
			}

			if (bytes[4] > 0) writePtr++;

			totalPackLen += (uint16_t)(writePtr - firstBytePtr);
		}

		pattPtr += sizeof(ft2_note_t) * (FT2_MAX_CHANNELS - antChn);
	}

	return totalPackLen;
}

static int64_t getTempInsAndSmpSize(ft2_instance_t *inst)
{
	int16_t j;

	int16_t ai = 128;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && tmpInstrName[ai][0] == '\0')
		ai--;

	int64_t currSize64 = 0;

	for (int16_t i = 1; i <= ai; i++)
	{
		if (tmpInstr[i] == NULL)
			j = 0;
		else
			j = i;

		const int16_t a = getUsedTempSamples(i);
		if (a > 0)
			currSize64 += XM_INSTR_HEADER_SIZE + (a * XM_SAMPLE_HEADER_SIZE);
		else
			currSize64 += 22 + 11;

		ft2_instr_t *ins = tmpInstr[j];
		for (int16_t k = 0; k < a; k++)
		{
			ft2_sample_t *s = &ins->smp[k];
			if (s->dataPtr != NULL && s->length > 0)
			{
				if (s->flags & FT2_SAMPLE_16BIT)
					currSize64 += s->length << 1;
				else
					currSize64 += s->length;
			}
		}
	}

	return currSize64;
}

static int64_t calculateXMSize(ft2_instance_t *inst)
{
	if (inst == NULL)
		return 0;

	int64_t currSize64 = XM_HEADER_SIZE;

	/* Count number of patterns that would be saved */
	int16_t ap = 256;
	do
	{
		if (patternEmpty(inst, ap - 1))
			ap--;
		else
			break;
	}
	while (ap > 0);

	/* Count number of instruments */
	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(inst, ai) == 0 && inst->replayer.song.instrName[ai][0] == '\0')
		ai--;

	/* Count packed pattern data size */
	for (int16_t i = 0; i < ap; i++)
	{
		currSize64 += XM_PATT_HEADER_SIZE;
		if (!patternEmpty(inst, i))
			currSize64 += getPackedPattSize(inst->replayer.pattern[i], inst->replayer.patternNumRows[i], inst->replayer.song.numChannels);
	}

	/* Count instrument and sample data size */
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t j;
		if (inst->replayer.instr[i] == NULL)
			j = 0;
		else
			j = i;

		const int16_t a = getUsedSamples(inst, i);
		if (a > 0)
			currSize64 += XM_INSTR_HEADER_SIZE + (a * XM_SAMPLE_HEADER_SIZE);
		else
			currSize64 += 22 + 11;

		ft2_instr_t *ins = inst->replayer.instr[j];
		for (int16_t k = 0; k < a; k++)
		{
			ft2_sample_t *s = &ins->smp[k];
			if (s->dataPtr != NULL && s->length > 0)
			{
				if (s->flags & FT2_SAMPLE_16BIT)
					currSize64 += s->length << 1;
				else
					currSize64 += s->length;
			}
		}
	}

	return currSize64;
}

/* Forward declarations for trim operations */
static void wipePattsUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ap);
static void wipeInstrUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ai, int32_t ap, int32_t antChn);
static void wipeSamplesUnused(ft2_instance_t *inst, bool testWipeSize, int16_t ai);
static void wipeSmpDataAfterLoop(ft2_instance_t *inst, bool testWipeSize, int16_t ai);
static void convertSamplesTo8bit(ft2_instance_t *inst, bool testWipeSize, int16_t ai);

static int64_t calculateTrimSize(ft2_instance_t *inst)
{
	int16_t i;

	if (inst == NULL)
		return 0;

	int32_t numChannels = inst->replayer.song.numChannels;
	int32_t pattDataLen = 0;
	int32_t newPattDataLen = 0;
	int64_t bytes64 = 0;
	int64_t oldInstrSize64 = 0;

	/* Copy over temp data */
	memcpy(tmpPatt, inst->replayer.pattern, sizeof(tmpPatt));
	memcpy(tmpPattLens, inst->replayer.patternNumRows, sizeof(tmpPattLens));
	memcpy(tmpInstrName, inst->replayer.song.instrName, sizeof(tmpInstrName));

	if (!setTmpInstruments(inst))
	{
		/* TODO: Show dialog when dialog system supports it better */
		return 0;
	}

	/* Get current size of all instruments and their samples */
	if (removeInst || removeSamp || removeSmpDataAfterLoop || convSmpsTo8Bit)
		oldInstrSize64 = getTempInsAndSmpSize(inst);

	/* Count number of patterns that would be saved */
	int16_t ap = 256;
	do
	{
		if (tmpPatternEmpty(ap - 1, numChannels))
			ap--;
		else
			break;
	}
	while (ap > 0);

	/* Count number of instruments that would be saved */
	int16_t ai = 128;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && tmpInstrName[ai][0] == '\0')
		ai--;

	/* Calculate "remove unused samples" size */
	if (removeSamp) wipeSamplesUnused(inst, true, ai);

	/* Calculate "remove sample data after loop" size */
	if (removeSmpDataAfterLoop) wipeSmpDataAfterLoop(inst, true, ai);

	/* Calculate "convert samples to 8-bit" size */
	if (convSmpsTo8Bit) convertSamplesTo8bit(inst, true, ai);

	/* Get old pattern data length */
	if (removeChans || removePatt)
	{
		for (i = 0; i < ap; i++)
		{
			pattDataLen += XM_PATT_HEADER_SIZE;
			if (!tmpPatternEmpty(i, numChannels))
				pattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}
	}

	/* Calculate "remove unused channels" size */
	if (removeChans)
	{
		int16_t highestChan = -1;
		for (i = 0; i < ap; i++)
		{
			ft2_note_t *pattPtr = tmpPatt[i];
			if (pattPtr == NULL)
				continue;

			const int16_t numRows = tmpPattLens[i];
			for (int32_t j = 0; j < numRows; j++)
			{
				for (int32_t k = 0; k < numChannels; k++)
				{
					ft2_note_t *p = &pattPtr[(j * FT2_MAX_CHANNELS) + k];
					if (p->note > 0 || p->instr > 0 || p->vol > 0 || p->efx > 0 || p->efxData > 0)
					{
						if (k > highestChan)
							highestChan = k;
					}
				}
			}
		}

		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1)
				highestChan++;

			if (highestChan < 2)
				highestChan = 2;
			if (highestChan > numChannels)
				highestChan = numChannels;

			numChannels = highestChan;
		}
	}

	/* Calculate "remove unused patterns" size */
	if (removePatt) wipePattsUnused(inst, true, &ap);

	/* Calculate new pattern data size */
	if (removeChans || removePatt)
	{
		for (i = 0; i < ap; i++)
		{
			newPattDataLen += XM_PATT_HEADER_SIZE;
			if (!tmpPatternEmpty(i, numChannels))
				newPattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}

		if (pattDataLen > newPattDataLen)
			bytes64 += (pattDataLen - newPattDataLen);
	}

	/* Calculate "remove unused instruments" size */
	if (removeInst) wipeInstrUnused(inst, true, &ai, ap, numChannels);

	/* Calculate new instruments and samples size */
	if (removeInst || removeSamp || removeSmpDataAfterLoop || convSmpsTo8Bit)
	{
		int64_t newInstrSize64 = getTempInsAndSmpSize(inst);

		if (oldInstrSize64 > newInstrSize64)
			bytes64 += (oldInstrSize64 - newInstrSize64);
	}

	freeTmpInstruments();
	return bytes64;
}

/* ============ TRIM OPERATIONS ============ */

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

static void remapInstrInSong(ft2_instance_t *inst, uint8_t src, uint8_t dst, int32_t ap)
{
	for (int32_t i = 0; i < ap; i++)
	{
		ft2_note_t *pattPtr = inst->replayer.pattern[i];
		if (pattPtr == NULL)
			continue;

		const int32_t readLen = inst->replayer.patternNumRows[i] * FT2_MAX_CHANNELS;

		ft2_note_t *p = pattPtr;
		for (int32_t j = 0; j < readLen; j++, p++)
		{
			if (p->instr == src)
				p->instr = dst;
		}
	}
}

static void wipeInstrUnused(ft2_instance_t *inst, bool testWipeSize, int16_t *ai, int32_t ap, int32_t antChn)
{
	int16_t numRows;
	int32_t i, j, k;
	ft2_note_t *p;

	int32_t numInsts = *ai;

	/* Calculate what instruments are used */
	memset(instrUsed, 0, numInsts);
	for (i = 0; i < ap; i++)
	{
		if (testWipeSize)
		{
			p = tmpPatt[i];
			numRows = tmpPattLens[i];
		}
		else
		{
			p = inst->replayer.pattern[i];
			numRows = inst->replayer.patternNumRows[i];
		}

		if (p == NULL)
			continue;

		for (j = 0; j < numRows; j++)
		{
			for (k = 0; k < antChn; k++)
			{
				uint8_t ins = p[(j * FT2_MAX_CHANNELS) + k].instr;
				if (ins > 0 && ins <= 128)
					instrUsed[ins - 1] = true;
			}
		}
	}

	int16_t instToDel = 0;
	uint8_t newInst = 0;
	int16_t newNumInsts = 0;

	memset(instrOrder, 0, numInsts);
	for (i = 0; i < numInsts; i++)
	{
		if (instrUsed[i])
		{
			newNumInsts++;
			instrOrder[i] = newInst++;
		}
		else
		{
			instToDel++;
		}
	}

	if (instToDel == 0)
		return;

	if (testWipeSize)
	{
		for (i = 0; i < numInsts; i++)
		{
			if (!instrUsed[i] && tmpInstr[1 + i] != NULL)
			{
				free(tmpInstr[1 + i]);
				tmpInstr[1 + i] = NULL;
			}
		}

		/* Relocate instruments */
		memcpy(tmpInstName, &tmpInstrName[1], 128 * sizeof(tmpInstrName[0]));
		memcpy(tmpInst, &tmpInstr[1], 128 * sizeof(tmpInstr[0]));

		memset(&tmpInstr[1], 0, numInsts * sizeof(tmpInstr[0]));
		memset(&tmpInstrName[1], 0, numInsts * sizeof(tmpInstrName[0]));

		for (i = 0; i < numInsts; i++)
		{
			if (instrUsed[i])
			{
				newInst = instrOrder[i];

				memcpy(&tmpInstr[1 + newInst], &tmpInst[i], sizeof(tmpInst[0]));
				strcpy(tmpInstrName[1 + newInst], tmpInstName[i]);
			}
		}

		*ai = newNumInsts;
		return;
	}

	/* Clear unused instruments */
	for (i = 0; i < numInsts; i++)
	{
		if (!instrUsed[i])
			ft2_instance_free_instr(inst, 1 + i);
	}

	/* Relocate instruments */
	static char localInstName[128][23];
	static ft2_instr_t *localInst[128];
	
	memcpy(localInstName, &inst->replayer.song.instrName[1], 128 * sizeof(inst->replayer.song.instrName[0]));
	memcpy(localInst, &inst->replayer.instr[1], 128 * sizeof(inst->replayer.instr[0]));

	memset(&inst->replayer.instr[1], 0, numInsts * sizeof(inst->replayer.instr[0]));
	memset(&inst->replayer.song.instrName[1], 0, numInsts * sizeof(inst->replayer.song.instrName[0]));

	for (i = 0; i < numInsts; i++)
	{
		if (instrUsed[i])
		{
			newInst = instrOrder[i];
			remapInstrInSong(inst, 1 + (uint8_t)i, 1 + newInst, ap);

			memcpy(&inst->replayer.instr[1 + newInst], &localInst[i], sizeof(localInst[0]));
			strcpy(inst->replayer.song.instrName[1 + newInst], localInstName[i]);
		}
	}

	*ai = newNumInsts;

	setTmpInstruments(inst);
}

static void wipeSamplesUnused(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	uint8_t smpUsed[16], smpOrder[16];
	int16_t j, k, l;
	ft2_instr_t *ins;
	ft2_sample_t tempSamples[16];

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (inst->replayer.instr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = inst->replayer.instr[l];
			l = getUsedSamples(inst, i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = tmpInstr[l];
			l = getUsedTempSamples(i);
		}

		memset(smpUsed, 0, l);
		if (l > 0)
		{
			ft2_sample_t *s = ins->smp;
			for (j = 0; j < l; j++, s++)
			{
				/* Check if sample is referenced in instrument */
				for (k = 0; k < 96; k++)
				{
					if (ins->note2SampleLUT[k] == j)
					{
						smpUsed[j] = true;
						break;
					}
				}

				if (k == 96)
				{
					/* Sample is unused */
					if (s->dataPtr != NULL && !testWipeSize)
						freeSmpData(inst, i, j);

					memset(s, 0, sizeof(ft2_sample_t));
				}
			}

			/* Create re-order list */
			uint8_t newSamp = 0;
			memset(smpOrder, 0, l);
			for (j = 0; j < l; j++)
			{
				if (smpUsed[j])
					smpOrder[j] = newSamp++;
			}

			/* Re-order samples */
			memcpy(tempSamples, ins->smp, l * sizeof(ft2_sample_t));
			memset(ins->smp, 0, l * sizeof(ft2_sample_t));

			for (j = 0; j < l; j++)
			{
				if (smpUsed[j])
					ins->smp[smpOrder[j]] = tempSamples[j];
			}

			/* Re-order note->sample list */
			for (j = 0; j < 96; j++)
			{
				newSamp = ins->note2SampleLUT[j];
				if (smpUsed[newSamp])
					ins->note2SampleLUT[j] = smpOrder[newSamp];
				else
					ins->note2SampleLUT[j] = 0;
			}
		}
	}
}

static void wipeSmpDataAfterLoop(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	int16_t l;
	ft2_instr_t *ins;

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (inst->replayer.instr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = inst->replayer.instr[l];
			l = getUsedSamples(inst, i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = tmpInstr[l];
			l = getUsedTempSamples(i);
		}

		ft2_sample_t *s = ins->smp;
		for (int16_t j = 0; j < l; j++, s++)
		{
			uint8_t loopType = s->flags & 3;
			if (s->dataPtr != NULL && loopType != FT2_LOOP_OFF && s->length > 0 && s->length > s->loopStart + s->loopLength)
			{
				s->length = s->loopStart + s->loopLength;
				if (!testWipeSize && s->length <= 0)
				{
					s->length = 0;
					freeSmpData(inst, i, j);
				}
			}
		}
	}
}

static void convertSamplesTo8bit(ft2_instance_t *inst, bool testWipeSize, int16_t ai)
{
	int16_t k;
	ft2_instr_t *ins;

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (inst->replayer.instr[i] == NULL)
				k = 0;
			else
				k = i;

			ins = inst->replayer.instr[k];
			k = getUsedSamples(inst, i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				k = 0;
			else
				k = i;

			ins = tmpInstr[k];
			k = getUsedTempSamples(i);
		}

		ft2_sample_t *s = ins->smp;
		for (int16_t j = 0; j < k; j++, s++)
		{
			if (s->dataPtr != NULL && s->length > 0 && (s->flags & FT2_SAMPLE_16BIT))
			{
				if (testWipeSize)
				{
					s->flags &= ~FT2_SAMPLE_16BIT;
				}
				else
				{
					const int16_t *src16 = (const int16_t *)s->dataPtr;
					int8_t *dst8 = s->dataPtr;

					for (int32_t a = 0; a < s->length; a++)
						dst8[a] = src16[a] >> 8;

					s->flags &= ~FT2_SAMPLE_16BIT;
				}
			}
		}
	}
}

/* ============ BUTTON CALLBACKS ============ */

void pbTrimCalc(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	xmSize64 = calculateXMSize(inst);
	spaceSaved64 = calculateTrimSize(inst);

	xmAfterTrimSize64 = xmSize64 - spaceSaved64;
	if (xmAfterTrimSize64 < 0)
		xmAfterTrimSize64 = 0;

	if (inst->uiState.trimScreenShown)
	{
		ft2_ui_t *ui = ft2_ui_get_current();
		if (ui != NULL)
			drawTrimScreen(inst, &ui->video, &ui->bmp);
	}
}

/* Callback when user confirms trim operation */
static void doTrimConfirmed(ft2_instance_t *inst, ft2_dialog_result_t result, 
                            const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;

	if (inst == NULL || result != DIALOG_RESULT_YES)
		return;

	/* Count number of patterns */
	int16_t ap = 256;
	do
	{
		if (patternEmpty(inst, ap - 1))
			ap--;
		else
			break;
	}
	while (ap > 0);

	/* Count number of instruments */
	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(inst, ai) == 0 && inst->replayer.song.instrName[ai][0] == '\0')
		ai--;

	if (!setTmpInstruments(inst))
	{
		/* TODO: Show dialog when dialog system supports it better */
		return;
	}

	/* Stop voices before trimming */
	ft2_stop_all_voices(inst);

	/* Remove unused samples */
	if (removeSamp)
		wipeSamplesUnused(inst, false, ai);

	/* Remove sample data after loop */
	if (removeSmpDataAfterLoop)
		wipeSmpDataAfterLoop(inst, false, ai);

	/* Convert samples to 8-bit */
	if (convSmpsTo8Bit)
		convertSamplesTo8bit(inst, false, ai);

	/* Remove unused channels */
	if (removeChans)
	{
		/* Count used channels */
		int16_t highestChan = -1;
		for (int32_t i = 0; i < ap; i++)
		{
			ft2_note_t *pattPtr = inst->replayer.pattern[i];
			if (pattPtr == NULL)
				continue;

			const int16_t numRows = inst->replayer.patternNumRows[i];
			for (int32_t j = 0; j < numRows; j++)
			{
				for (int32_t k = 0; k < inst->replayer.song.numChannels; k++)
				{
					ft2_note_t *p = &pattPtr[(j * FT2_MAX_CHANNELS) + k];
					if (p->note > 0 || p->vol > 0 || p->instr > 0 || p->efx > 0 || p->efxData > 0)
					{
						if (k > highestChan)
							highestChan = k;
					}
				}
			}
		}

		/* Set new 'channels used' number */
		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1)
				highestChan++;

			if (highestChan < 2)
				highestChan = 2;
			if (highestChan > inst->replayer.song.numChannels)
				highestChan = inst->replayer.song.numChannels;

			inst->replayer.song.numChannels = (uint8_t)highestChan;
		}

		/* Clear potentially unused channel data */
		if (inst->replayer.song.numChannels < FT2_MAX_CHANNELS)
		{
			for (int32_t i = 0; i < 256; i++)
			{
				ft2_note_t *p = inst->replayer.pattern[i];
				if (p == NULL)
					continue;

				const int16_t numRows = inst->replayer.patternNumRows[i];
				for (int32_t j = 0; j < numRows; j++)
					memset(&p[(j * FT2_MAX_CHANNELS) + inst->replayer.song.numChannels], 0, sizeof(ft2_note_t) * (FT2_MAX_CHANNELS - inst->replayer.song.numChannels));
			}
		}
	}

	/* Clear unused patterns */
	if (removePatt)
		wipePattsUnused(inst, false, &ap);

	/* Remove unused instruments */
	if (removeInst)
		wipeInstrUnused(inst, false, &ai, ap, inst->replayer.song.numChannels);

	freeTmpInstruments();

	/* Mark song as modified */
	ft2_song_mark_modified(inst);

	/* Recalculate sizes */
	pbTrimCalc(inst);

	/* Trigger UI update */
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.needsFullRedraw = true;
}

void pbTrimDoTrim(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (!removePatt && !removeInst && !removeSamp && !removeChans && !removeSmpDataAfterLoop && !convSmpsTo8Bit)
		return; /* nothing to trim */

	/* Show confirmation dialog */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL)
		return;

	ft2_dialog_show_yesno_cb(&ui->dialog, "System request",
		"Are you sure you want to trim the song? Making a backup of the song first is recommended.",
		inst, doTrimConfirmed, NULL);
}

