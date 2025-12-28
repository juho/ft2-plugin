/**
 * @file ft2_plugin_callbacks.c
 * @brief Widget callback implementations for the FT2 plugin.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "ft2_plugin_callbacks.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_volume_panel.h"
#include "ft2_plugin_resample_panel.h"
#include "ft2_plugin_echo_panel.h"
#include "ft2_plugin_mix_panel.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_ui.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_help.h"
#include "ft2_plugin_about.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_trim.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_plugin_diskop.h"
#include "ft2_instance.h"

/* ========== POSITION EDITOR CALLBACKS ========== */

void pbPosEdPosUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songPos > 0)
	{
		inst->replayer.song.songPos--;
		
		/* Update pattern number from order list (like original setPos) */
		inst->replayer.song.pattNum = inst->replayer.song.orders[inst->replayer.song.songPos];
		inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
		
		/* Reset row to 0 (matches standalone setNewSongPos -> setPos(pos, 0, true)) */
		inst->replayer.song.row = 0;
			if (!inst->replayer.songPlaying)
			inst->editor.row = 0;
		
		/* Update editor pattern if not playing */
		if (!inst->replayer.songPlaying)
			inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
		
		inst->uiState.updatePosSections = true;
		inst->uiState.updatePosEdScrollBar = true;
		inst->uiState.updatePatternEditor = true;
	}
}

void pbPosEdPosDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songPos < inst->replayer.song.songLength - 1)
	{
		inst->replayer.song.songPos++;
		
		/* Update pattern number from order list (like original setPos) */
		inst->replayer.song.pattNum = inst->replayer.song.orders[inst->replayer.song.songPos];
		inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
		
		/* Reset row to 0 (matches standalone setNewSongPos -> setPos(pos, 0, true)) */
		inst->replayer.song.row = 0;
			if (!inst->replayer.songPlaying)
			inst->editor.row = 0;
		
		/* Update editor pattern if not playing */
		if (!inst->replayer.songPlaying)
			inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
		
		inst->uiState.updatePosSections = true;
		inst->uiState.updatePosEdScrollBar = true;
		inst->uiState.updatePatternEditor = true;
	}
}

void pbPosEdIns(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLength >= 255)
		return;
	
	int16_t pos = inst->replayer.song.songPos;
	uint8_t patt = inst->replayer.song.orders[pos];
	
	for (int16_t i = inst->replayer.song.songLength; i > pos; i--)
		inst->replayer.song.orders[i] = inst->replayer.song.orders[i - 1];
	
	inst->replayer.song.orders[pos] = patt;
	inst->replayer.song.songLength++;
	inst->uiState.updatePosSections = true;
	inst->uiState.updatePosEdScrollBar = true;
}

void pbPosEdDel(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLength <= 1)
		return;
	
	int16_t pos = inst->replayer.song.songPos;
	for (int16_t i = pos; i < inst->replayer.song.songLength - 1; i++)
		inst->replayer.song.orders[i] = inst->replayer.song.orders[i + 1];
	
	inst->replayer.song.songLength--;
	
	if (inst->replayer.song.songPos >= inst->replayer.song.songLength)
		inst->replayer.song.songPos = inst->replayer.song.songLength - 1;
	
	inst->uiState.updatePosSections = true;
	inst->uiState.updatePosEdScrollBar = true;
}

void pbPosEdPattUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	int16_t pos = inst->replayer.song.songPos;
	if (inst->replayer.song.orders[pos] >= 255)
		return;
	
	inst->replayer.song.orders[pos]++;
	inst->replayer.song.pattNum = inst->replayer.song.orders[pos];
	
	inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
	if (inst->replayer.song.row >= inst->replayer.song.currNumRows)
	{
		inst->replayer.song.row = inst->replayer.song.currNumRows - 1;
		if (!inst->replayer.songPlaying)
			inst->editor.row = (uint8_t)inst->replayer.song.row;
	}
	
	if (!inst->replayer.songPlaying)
		inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPosEdPattDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	int16_t pos = inst->replayer.song.songPos;
	if (inst->replayer.song.orders[pos] == 0)
		return;
	
	inst->replayer.song.orders[pos]--;
	inst->replayer.song.pattNum = inst->replayer.song.orders[pos];
	
	inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
	if (inst->replayer.song.row >= inst->replayer.song.currNumRows)
	{
		inst->replayer.song.row = inst->replayer.song.currNumRows - 1;
		if (!inst->replayer.songPlaying)
			inst->editor.row = (uint8_t)inst->replayer.song.row;
	}
	
	if (!inst->replayer.songPlaying)
		inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPosEdLenUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLength < 255)
	{
		inst->replayer.song.songLength++;
		inst->uiState.updatePosSections = true;
		inst->uiState.updatePosEdScrollBar = true;
	}
}

void pbPosEdLenDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLength > 1)
	{
		inst->replayer.song.songLength--;
		if (inst->replayer.song.songPos >= inst->replayer.song.songLength)
			inst->replayer.song.songPos = inst->replayer.song.songLength - 1;
		inst->uiState.updatePosSections = true;
		inst->uiState.updatePosEdScrollBar = true;
	}
}

void pbPosEdRepUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLoopStart < inst->replayer.song.songLength - 1)
	{
		inst->replayer.song.songLoopStart++;
		inst->uiState.updatePosSections = true;
	}
}

void pbPosEdRepDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLoopStart > 0)
	{
		inst->replayer.song.songLoopStart--;
		inst->uiState.updatePosSections = true;
	}
}

/* ========== SONG/PATTERN CALLBACKS ========== */

void pbBPMUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	/* Ignore if BPM is synced from DAW */
	if (inst->config.syncBpmFromDAW) return;

	if (inst->replayer.song.BPM < 255)
	{
		inst->replayer.song.BPM++;
		inst->uiState.updatePosSections = true;
	}
}

void pbBPMDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	/* Ignore if BPM is synced from DAW */
	if (inst->config.syncBpmFromDAW) return;

	if (inst->replayer.song.BPM > 32)
	{
		inst->replayer.song.BPM--;
		inst->uiState.updatePosSections = true;
	}
}

void pbSpeedUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	/* Ignore if Fxx speed changes are disabled */
	if (!inst->config.allowFxxSpeedChanges) return;

	if (inst->replayer.song.speed < 31)
	{
		inst->replayer.song.speed++;
		inst->uiState.updatePosSections = true;
	}
}

void pbSpeedDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	/* Ignore if Fxx speed changes are disabled */
	if (!inst->config.allowFxxSpeedChanges) return;

	if (inst->replayer.song.speed > 1)
	{
		inst->replayer.song.speed--;
		inst->uiState.updatePosSections = true;
	}
}

void pbEditAddUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->editor.editRowSkip == 16)
		inst->editor.editRowSkip = 0;
	else
		inst->editor.editRowSkip++;
	
	inst->uiState.updatePosSections = true;
}

void pbEditAddDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->editor.editRowSkip == 0)
		inst->editor.editRowSkip = 16;
	else
		inst->editor.editRowSkip--;
	
	inst->uiState.updatePosSections = true;
}

void pbPattUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.pattNum < 255)
	{
		inst->replayer.song.pattNum++;
		
		inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
		if (inst->replayer.song.row >= inst->replayer.song.currNumRows)
		{
			inst->replayer.song.row = inst->replayer.song.currNumRows - 1;
			if (!inst->replayer.songPlaying)
				inst->editor.row = (uint8_t)inst->replayer.song.row;
		}
		
		if (!inst->replayer.songPlaying)
			inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
		
		inst->uiState.updatePatternEditor = true;
		inst->uiState.updatePosSections = true;
	}
}

void pbPattDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.pattNum > 0)
	{
		inst->replayer.song.pattNum--;
		
		inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
		if (inst->replayer.song.row >= inst->replayer.song.currNumRows)
		{
			inst->replayer.song.row = inst->replayer.song.currNumRows - 1;
			if (!inst->replayer.songPlaying)
				inst->editor.row = (uint8_t)inst->replayer.song.row;
		}
		
		if (!inst->replayer.songPlaying)
			inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
		
		inst->uiState.updatePatternEditor = true;
		inst->uiState.updatePosSections = true;
	}
}

void pbPattLenUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	uint16_t len = inst->replayer.patternNumRows[inst->editor.editPattern];
	if (len >= 256)
		return;
	
	inst->replayer.patternNumRows[inst->editor.editPattern] = len + 1;
	
	if (inst->replayer.song.pattNum == inst->editor.editPattern)
		inst->replayer.song.currNumRows = len + 1;
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPattLenDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	uint16_t len = inst->replayer.patternNumRows[inst->editor.editPattern];
	if (len <= 1)
		return;
	
	inst->replayer.patternNumRows[inst->editor.editPattern] = len - 1;
	
	if (inst->replayer.song.pattNum == inst->editor.editPattern)
		inst->replayer.song.currNumRows = len - 1;
	
	/* Clamp row position if needed */
	if (inst->replayer.song.row >= len - 1)
	{
		inst->replayer.song.row = len - 2;
		if (!inst->replayer.songPlaying)
			inst->editor.row = (uint8_t)inst->replayer.song.row;
	}
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPattExpand(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	uint8_t curPattern = inst->editor.editPattern;
	uint16_t numRows = inst->replayer.patternNumRows[curPattern];
	
	if (numRows > 128)
		return;
	
	ft2_note_t *p = inst->replayer.pattern[curPattern];
	if (p != NULL)
	{
		/* Copy pattern to temp and expand */
		ft2_note_t tmpPattern[256 * FT2_MAX_CHANNELS];
		memcpy(tmpPattern, p, numRows * FT2_MAX_CHANNELS * sizeof(ft2_note_t));
		
		for (int32_t i = numRows - 1; i >= 0; i--)
		{
			for (int32_t j = 0; j < FT2_MAX_CHANNELS; j++)
				p[((i * 2) * FT2_MAX_CHANNELS) + j] = tmpPattern[(i * FT2_MAX_CHANNELS) + j];
			
			memset(&p[((i * 2) + 1) * FT2_MAX_CHANNELS], 0, FT2_MAX_CHANNELS * sizeof(ft2_note_t));
		}
	}
	
	inst->replayer.patternNumRows[curPattern] = numRows * 2;
	
	if (inst->replayer.song.pattNum == curPattern)
		inst->replayer.song.currNumRows = numRows * 2;
	
	inst->replayer.song.row *= 2;
	if (inst->replayer.song.row >= numRows * 2)
		inst->replayer.song.row = (numRows * 2) - 1;
	
	inst->editor.row = (uint8_t)inst->replayer.song.row;
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPattShrink(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	uint8_t curPattern = inst->editor.editPattern;
	uint16_t numRows = inst->replayer.patternNumRows[curPattern];
	
	if (numRows <= 1)
		return;
	
	ft2_note_t *p = inst->replayer.pattern[curPattern];
	if (p != NULL)
	{
		for (int32_t i = 0; i < numRows / 2; i++)
		{
			for (int32_t j = 0; j < FT2_MAX_CHANNELS; j++)
				p[(i * FT2_MAX_CHANNELS) + j] = p[((i * 2) * FT2_MAX_CHANNELS) + j];
		}
	}
	
	inst->replayer.patternNumRows[curPattern] = numRows / 2;
	numRows = numRows / 2;
	
	if (inst->replayer.song.pattNum == curPattern)
		inst->replayer.song.currNumRows = numRows;
	
	inst->replayer.song.row /= 2;
	if (inst->replayer.song.row >= numRows)
		inst->replayer.song.row = numRows - 1;
	
	inst->editor.row = (uint8_t)inst->replayer.song.row;
	
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

/* ========== PLAYBACK CALLBACKS ========== */

void pbPlaySong(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_instance_play(inst, FT2_PLAYMODE_SONG, 0);
}

void pbPlayPatt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_instance_play(inst, FT2_PLAYMODE_PATT, 0);
}

void pbStop(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_instance_stop(inst);
}

void pbRecordSong(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_instance_play(inst, FT2_PLAYMODE_RECSONG, 0);
}

void pbRecordPatt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_instance_play(inst, FT2_PLAYMODE_RECPATT, 0);
}

/* ========== MENU CALLBACKS ========== */

void pbDiskOp(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	if (inst->uiState.diskOpShown)
	{
		/* Closing disk op */
		hideDiskOpScreen(inst);
		inst->uiState.scopesShown = true;
	}
	else
	{
		/* Opening disk op - hide other screens first */
		hideTopScreen(inst);
		inst->diskop.requestReadDir = true;
		inst->uiState.diskOpShown = true;
		inst->uiState.scopesShown = false;
	}

	inst->uiState.needsFullRedraw = true;
}

void pbInstEd(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL) return;
	
	toggleInstEditor(inst, &ui->video, &ui->bmp);
}

void pbSmpEd(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Hide instrument editor first if shown */
	if (inst->uiState.instEditorShown)
	{
		hideInstEditor(inst);
	}
	
	/* Toggle sample editor - use proper show/hide to set widget visibility */
	if (inst->uiState.sampleEditorShown)
	{
		hideSampleEditor(inst);
		inst->uiState.patternEditorShown = true;
	}
	else
	{
		showSampleEditor(inst);
	}
}

void pbConfig(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Toggle config screen visibility */
	if (inst->uiState.configScreenShown)
	{
		/* Exit config - hide config and restore main screen */
		hideConfigScreen(inst);
		inst->uiState.configScreenShown = false;
		inst->uiState.needsFullRedraw = true;
		/* Main screen widgets will be shown on next redraw via showTopScreen */
	}
	else
	{
		/* Show config - hide ALL main screen widgets first */
		hideTopScreen(inst);
		
		/* Set config screen flag */
		inst->uiState.configScreenShown = true;
		showConfigScreen(inst);
		inst->uiState.needsFullRedraw = true;
	}
}

void pbConfigExit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Hide all config screen widgets */
	hideConfigScreen(inst);
	inst->uiState.configScreenShown = false;
	
	/* Force full redraw - main screen widgets will be shown on next frame */
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.updateInstrSwitcher = true;
}

void pbHelp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Don't show again if already showing */
	if (inst->uiState.helpScreenShown)
		return;
	
	/* Get video/bmp from current UI */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		showHelpScreen(inst, video, bmp);
	}
	
		inst->uiState.needsFullRedraw = true;
	}

void pbHelpExit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	exitHelpScreen(inst);
}

/* Help radio button callbacks - wrappers that get video/bmp context */
void cbHelpFeatures(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpFeatures(inst, video, bmp);
	}
}

void cbHelpEffects(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpEffects(inst, video, bmp);
	}
}

void cbHelpKeybindings(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpKeybindings(inst, video, bmp);
	}
}

void cbHelpHowToUseFT2(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpHowToUseFT2(inst, video, bmp);
	}
}

void cbHelpFAQ(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpFAQ(inst, video, bmp);
	}
}

void cbHelpKnownBugs(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpKnownBugs(inst, video, bmp);
	}
}

/* Help scroll button callbacks */
void pbHelpScrollUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		helpScrollUp(inst, video, bmp);
	}
}

void pbHelpScrollDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		helpScrollDown(inst, video, bmp);
	}
}

/* Help scrollbar callback */
void sbHelpScroll(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		helpScrollSetPos(inst, video, bmp, pos);
	}
}

void pbAbout(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Don't show again if already showing */
	if (inst->uiState.aboutScreenShown)
		return;
	
	/* Hide ALL main screen widgets first */
	hideTopScreen(inst);
	
	/* Get video/bmp from current UI to draw framework and initialize starfield */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		ft2_about_show(video, bmp);
	}
	
	/* Show about screen */
	inst->uiState.aboutScreenShown = true;
	inst->uiState.needsFullRedraw = true;
	inst->uiState.scopesShown = false;
}

void pbExitAbout(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	/* Hide the exit button */
	hidePushButton(PB_EXIT_ABOUT);
	
	inst->uiState.aboutScreenShown = false;
	inst->uiState.scopesShown = true;
	inst->uiState.instrSwitcherShown = true;
	
	/* Force full redraw to restore top screen (like original showTopScreen(true)) */
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.updateInstrSwitcher = true;
}

void pbNibbles(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	/* Nibbles show is deferred to UI loop where video/bmp are available */
	inst->uiState.nibblesShown = !inst->uiState.nibblesShown;
	inst->uiState.needsFullRedraw = true;
}

/* Zap song data - clear patterns, orders, song settings */
static void zapSong(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Stop all voices before clearing */
	ft2_stop_all_voices(inst);
	
	/* Reset song parameters */
	inst->replayer.song.songLength = 1;
	inst->replayer.song.songLoopStart = 0;
	inst->replayer.song.BPM = 125;
	inst->replayer.song.speed = 6;
	inst->replayer.song.songPos = 0;
	inst->replayer.song.globalVolume = 64;
	
	/* Clear song name and orders */
	memset(inst->replayer.song.name, 0, sizeof(inst->replayer.song.name));
	memset(inst->replayer.song.orders, 0, sizeof(inst->replayer.song.orders));
	
	/* Free all patterns and reset pattern lengths */
	ft2_instance_free_all_patterns(inst);
	
	/* Reset playback state */
	inst->replayer.song.row = 0;
	inst->replayer.song.pattNum = 0;
	inst->replayer.song.currNumRows = inst->replayer.patternNumRows[0];
	
	/* Reset cursor position */
	inst->cursor.ch = 0;
	
	/* Clear pattern mark */
	clearPattMark(inst);
	
	/* Update BPM-related variables */
	ft2_instance_init_bpm_vars(inst);
	
	/* Trigger UI updates */
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

/* Zap instruments - clear all instruments and samples */
static void zapInstrs(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Stop all voices before clearing */
	ft2_stop_all_voices(inst);
	
	/* Free all instruments (1-128, not 0 which is placeholder) */
	for (int16_t i = 1; i <= FT2_MAX_INST; i++)
	{
		ft2_instance_free_instr(inst, i);
		memset(inst->replayer.song.instrName[i], 0, 22+1);
	}
	
	/* Reset editor instrument pointers */
	inst->editor.currVolEnvPoint = 0;
	inst->editor.currPanEnvPoint = 0;
	
	/* Trigger UI updates */
	inst->uiState.updateInstrSwitcher = true;
	inst->uiState.updateSampleEditor = true;
}

/* Callback for Zap dialog completion */
static void zapDialogCallback(ft2_instance_t *inst, ft2_dialog_result_t result,
                              const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;
	
	if (inst == NULL)
		return;
	
	bool didZap = false;
	
	if (result == DIALOG_RESULT_ZAP_ALL)
	{
		zapSong(inst);
		zapInstrs(inst);
		didZap = true;
	}
	else if (result == DIALOG_RESULT_ZAP_SONG)
	{
		zapSong(inst);
		didZap = true;
	}
	else if (result == DIALOG_RESULT_ZAP_INSTR)
	{
		zapInstrs(inst);
		didZap = true;
	}
	
	if (didZap)
	{
		/* Mark song as modified */
		ft2_song_mark_modified(inst);
		
		/* Trigger full UI redraw */
		inst->uiState.needsFullRedraw = true;
	}
}

void pbKill(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL)
		return;
	
	ft2_dialog_show_zap_cb(&ui->dialog, "System request", "Total devastation of the...",
	                       inst, zapDialogCallback, NULL);
}

void pbTrim(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL) return;
	toggleTrimScreen(inst, &ui->video, &ui->bmp);
}

void pbTrimCalcWrapper(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	pbTrimCalc(inst);
}

void pbTrimDoTrimWrapper(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	pbTrimDoTrim(inst);
}

void pbExtendView(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->uiState.extendedPatternEditor = !inst->uiState.extendedPatternEditor;
}

void pbTranspose(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	toggleTranspose(inst);
}

/* Track transpose - current instrument */
void pbTrackTranspCurInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, 1, TRANSP_CUR_INSTRUMENT);
}

void pbTrackTranspCurInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, -1, TRANSP_CUR_INSTRUMENT);
}

void pbTrackTranspCurIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, 12, TRANSP_CUR_INSTRUMENT);
}

void pbTrackTranspCurIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, -12, TRANSP_CUR_INSTRUMENT);
}

/* Track transpose - all instruments */
void pbTrackTranspAllInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, 1, TRANSP_ALL_INSTRUMENTS);
}

void pbTrackTranspAllInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, -1, TRANSP_ALL_INSTRUMENTS);
}

void pbTrackTranspAllIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, 12, TRANSP_ALL_INSTRUMENTS);
}

void pbTrackTranspAllIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_TRACK, -12, TRANSP_ALL_INSTRUMENTS);
}

/* Pattern transpose - current instrument */
void pbPattTranspCurInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, 1, TRANSP_CUR_INSTRUMENT);
}

void pbPattTranspCurInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, -1, TRANSP_CUR_INSTRUMENT);
}

void pbPattTranspCurIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, 12, TRANSP_CUR_INSTRUMENT);
}

void pbPattTranspCurIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, -12, TRANSP_CUR_INSTRUMENT);
}

/* Pattern transpose - all instruments */
void pbPattTranspAllInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, 1, TRANSP_ALL_INSTRUMENTS);
}

void pbPattTranspAllInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, -1, TRANSP_ALL_INSTRUMENTS);
}

void pbPattTranspAllIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, 12, TRANSP_ALL_INSTRUMENTS);
}

void pbPattTranspAllIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_PATT, -12, TRANSP_ALL_INSTRUMENTS);
}

/* Song transpose - current instrument */
void pbSongTranspCurInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, 1, TRANSP_CUR_INSTRUMENT);
}

void pbSongTranspCurInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, -1, TRANSP_CUR_INSTRUMENT);
}

void pbSongTranspCurIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, 12, TRANSP_CUR_INSTRUMENT);
}

void pbSongTranspCurIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, -12, TRANSP_CUR_INSTRUMENT);
}

/* Song transpose - all instruments */
void pbSongTranspAllInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, 1, TRANSP_ALL_INSTRUMENTS);
}

void pbSongTranspAllInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, -1, TRANSP_ALL_INSTRUMENTS);
}

void pbSongTranspAllIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, 12, TRANSP_ALL_INSTRUMENTS);
}

void pbSongTranspAllIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_SONG, -12, TRANSP_ALL_INSTRUMENTS);
}

/* Block transpose - current instrument */
void pbBlockTranspCurInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, 1, TRANSP_CUR_INSTRUMENT);
}

void pbBlockTranspCurInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, -1, TRANSP_CUR_INSTRUMENT);
}

void pbBlockTranspCurIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, 12, TRANSP_CUR_INSTRUMENT);
}

void pbBlockTranspCurIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, -12, TRANSP_CUR_INSTRUMENT);
}

/* Block transpose - all instruments */
void pbBlockTranspAllInsUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, 1, TRANSP_ALL_INSTRUMENTS);
}

void pbBlockTranspAllInsDn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, -1, TRANSP_ALL_INSTRUMENTS);
}

void pbBlockTranspAllIns12Up(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, 12, TRANSP_ALL_INSTRUMENTS);
}

void pbBlockTranspAllIns12Dn(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	doTranspose(inst, TRANSP_BLOCK, -12, TRANSP_ALL_INSTRUMENTS);
}

void pbInstEdExt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->uiState.instEditorExtShown = !inst->uiState.instEditorExtShown;
}

void pbSmpEdExt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	toggleSampleEditorExt(inst);
}

void pbAdvEdit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL) return;
	toggleAdvEdit(inst, &ui->video, &ui->bmp);
}

void pbRemapTrack(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	remapTrack(inst);
}

void pbRemapPattern(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	remapPattern(inst);
}

void pbRemapSong(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	remapSong(inst);
}

void pbRemapBlock(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	remapBlock(inst);
}

void pbAddChannels(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->replayer.song.numChannels > 30)
		return;
	
	inst->replayer.song.numChannels += 2;
	
	/* Update channel numbers and scrollbar state */
	updateChanNums(inst);
	
	/* Trigger full UI redraw */
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbSubChannels(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->replayer.song.numChannels < 4)
		return;
	
	inst->replayer.song.numChannels -= 2;
	
	/* Clamp cursor channel if needed */
	if (inst->cursor.ch >= inst->replayer.song.numChannels)
		inst->cursor.ch = inst->replayer.song.numChannels - 1;
	
	/* Update channel numbers and scrollbar state */
	updateChanNums(inst);
	
	/* Trigger full UI redraw */
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

/* ========== LOGO/BADGE CALLBACKS ========== */

void pbLogo(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL) return;

	inst->config.id_FastLogo ^= 1;
	changeLogoType(&ui->bmp, inst->config.id_FastLogo);
	drawPushButton(&ui->video, &ui->bmp, PB_LOGO);
}

void pbBadge(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui == NULL) return;

	inst->config.id_TritonProd ^= 1;
	changeBadgeType(&ui->bmp, inst->config.id_TritonProd);
	drawPushButton(&ui->video, &ui->bmp, PB_BADGE);
}

/* ========== INSTRUMENT SWITCHER CALLBACKS ========== */

void pbSwapInstrBank(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	inst->editor.instrBankSwapped ^= 1;
	
	/* Adjust instrument bank offset by +/- 64 (8 banks of 8 instruments) */
	if (inst->editor.instrBankSwapped)
		inst->editor.instrBankOffset += 8 * 8;
	else
		inst->editor.instrBankOffset -= 8 * 8;
	
	/* Trigger instrument switcher update and button visibility update */
	inst->uiState.updateInstrSwitcher = true;
	inst->uiState.instrBankSwapPending = true;
}

void pbSampleListUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->editor.sampleBankOffset > 0)
		inst->editor.sampleBankOffset--;
}

void pbSampleListDown(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	if (inst->editor.sampleBankOffset < 11)
		inst->editor.sampleBankOffset++;
}

void pbChanScrollLeft(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	scrollChannelLeft(inst);
	inst->uiState.updateChanScrollPos = true;
}

void pbChanScrollRight(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	scrollChannelRight(inst);
	inst->uiState.updateChanScrollPos = true;
}

/* Instrument bank buttons - set instrument bank offset (which bank of 8 instruments to display) */
void pbRange1(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 0 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange2(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 1 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange3(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 2 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange4(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 3 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange5(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 4 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange6(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 5 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange7(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 6 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange8(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 7 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange9(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 8 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange10(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 9 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange11(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 10 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange12(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 11 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange13(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 12 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange14(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 13 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange15(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 14 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

void pbRange16(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	inst->editor.instrBankOffset = 15 * 8;
	inst->uiState.updateInstrSwitcher = true;
}

/* ========== INSTRUMENT EDITOR CALLBACKS ========== */

/* Helper to get current instrument */
static ft2_instr_t *getCurInstr(ft2_instance_t *inst)
{
	if (inst == NULL || inst->editor.curInstr == 0 || inst->editor.curInstr >= 128)
		return NULL;
	return inst->replayer.instr[inst->editor.curInstr];
}

/* Helper to get current sample */
static ft2_sample_t *getCurSample(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL || inst->editor.curSmp >= 16)
		return NULL;
	return &instr->smp[inst->editor.curSmp];
}

/* Envelope presets - left-click recalls, right-click stores */
void pbVolPreDef1(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 0); }
void pbVolPreDef2(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 1); }
void pbVolPreDef3(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 2); }
void pbVolPreDef4(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 3); }
void pbVolPreDef5(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 4); }
void pbVolPreDef6(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 5); }
void pbPanPreDef1(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 0); }
void pbPanPreDef2(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 1); }
void pbPanPreDef3(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 2); }
void pbPanPreDef4(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 3); }
void pbPanPreDef5(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 4); }
void pbPanPreDef6(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 5); }

/* Volume envelope controls */
void pbVolEnvAdd(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getCurInstr(inst);
	if (inst->editor.curInstr == 0 || ins == NULL)
		return;
	
	const int16_t ant = ins->volEnvLength;
	if (ant >= 12)
		return;
	
	int16_t i = (int16_t)inst->editor.currVolEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant - 1;
		if (i < 0)
			i = 0;
	}
	
	/* Check if there's enough space between adjacent points */
	if (i < ant - 1 && ins->volEnvPoints[i+1][0] - ins->volEnvPoints[i][0] < 2)
		return;
	
	if (ins->volEnvPoints[i][0] >= 323)
		return;
	
	/* Shift all points after i down by one */
	for (int16_t j = ant; j > i; j--)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j-1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j-1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->volEnvSustain > i)
		ins->volEnvSustain++;
	if (ins->volEnvLoopStart > i)
		ins->volEnvLoopStart++;
	if (ins->volEnvLoopEnd > i)
		ins->volEnvLoopEnd++;
	
	/* Calculate new point position */
	if (i < ant - 1)
	{
		ins->volEnvPoints[i+1][0] = (ins->volEnvPoints[i][0] + ins->volEnvPoints[i+2][0]) / 2;
		ins->volEnvPoints[i+1][1] = (ins->volEnvPoints[i][1] + ins->volEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->volEnvPoints[i+1][0] = ins->volEnvPoints[i][0] + 10;
		ins->volEnvPoints[i+1][1] = ins->volEnvPoints[i][1];
	}
	
	if (ins->volEnvPoints[i+1][0] > 324)
		ins->volEnvPoints[i+1][0] = 324;
	
	ins->volEnvLength++;
	inst->uiState.updateInstEditor = true;
}

void pbVolEnvDel(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getCurInstr(inst);
	if (ins == NULL || inst->editor.curInstr == 0 || ins->volEnvLength <= 2)
		return;
	
	int16_t i = (int16_t)inst->editor.currVolEnvPoint;
	if (i < 0 || i >= ins->volEnvLength)
		return;
	
	/* Shift all points after i up by one */
	for (int16_t j = i; j < ins->volEnvLength; j++)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j+1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j+1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->volEnvSustain > i)
		ins->volEnvSustain--;
	if (ins->volEnvLoopStart > i)
		ins->volEnvLoopStart--;
	if (ins->volEnvLoopEnd > i)
		ins->volEnvLoopEnd--;
	
	/* Ensure first point always at X=0 */
	ins->volEnvPoints[0][0] = 0;
	ins->volEnvLength--;
	
	/* Clamp indices to valid range */
	if (ins->volEnvSustain >= ins->volEnvLength)
		ins->volEnvSustain = ins->volEnvLength - 1;
	if (ins->volEnvLoopStart >= ins->volEnvLength)
		ins->volEnvLoopStart = ins->volEnvLength - 1;
	if (ins->volEnvLoopEnd >= ins->volEnvLength)
		ins->volEnvLoopEnd = ins->volEnvLength - 1;
	
	/* Update current point selection */
	if (ins->volEnvLength == 0)
		inst->editor.currVolEnvPoint = 0;
	else if (inst->editor.currVolEnvPoint >= ins->volEnvLength)
		inst->editor.currVolEnvPoint = ins->volEnvLength - 1;
	
	inst->uiState.updateInstEditor = true;
}

void pbVolEnvSusUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvSustain < instr->volEnvLength - 1)
	{
		instr->volEnvSustain++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvSusDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvSustain > 0)
	{
		instr->volEnvSustain--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepSUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopStart < instr->volEnvLoopEnd)
	{
		instr->volEnvLoopStart++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepSDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopStart > 0)
	{
		instr->volEnvLoopStart--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepEUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopEnd < instr->volEnvLength - 1)
	{
		instr->volEnvLoopEnd++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepEDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopEnd > instr->volEnvLoopStart)
	{
		instr->volEnvLoopEnd--;
		inst->uiState.updateInstEditor = true;
	}
}

/* Pan envelope controls */
void pbPanEnvAdd(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getCurInstr(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	const int16_t ant = ins->panEnvLength;
	if (ant >= 12)
		return;
	
	int16_t i = (int16_t)inst->editor.currPanEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant - 1;
		if (i < 0)
			i = 0;
	}
	
	/* Check if there's enough space between adjacent points */
	if (i < ant - 1 && ins->panEnvPoints[i+1][0] - ins->panEnvPoints[i][0] < 2)
		return;
	
	if (ins->panEnvPoints[i][0] >= 323)
		return;
	
	/* Shift all points after i down by one */
	for (int16_t j = ant; j > i; j--)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j-1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j-1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->panEnvSustain > i)
		ins->panEnvSustain++;
	if (ins->panEnvLoopStart > i)
		ins->panEnvLoopStart++;
	if (ins->panEnvLoopEnd > i)
		ins->panEnvLoopEnd++;
	
	/* Calculate new point position */
	if (i < ant - 1)
	{
		ins->panEnvPoints[i+1][0] = (ins->panEnvPoints[i][0] + ins->panEnvPoints[i+2][0]) / 2;
		ins->panEnvPoints[i+1][1] = (ins->panEnvPoints[i][1] + ins->panEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->panEnvPoints[i+1][0] = ins->panEnvPoints[i][0] + 10;
		ins->panEnvPoints[i+1][1] = ins->panEnvPoints[i][1];
	}
	
	if (ins->panEnvPoints[i+1][0] > 324)
		ins->panEnvPoints[i+1][0] = 324;
	
	ins->panEnvLength++;
	inst->uiState.updateInstEditor = true;
}

void pbPanEnvDel(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getCurInstr(inst);
	if (ins == NULL || inst->editor.curInstr == 0 || ins->panEnvLength <= 2)
		return;
	
	int16_t i = (int16_t)inst->editor.currPanEnvPoint;
	if (i < 0 || i >= ins->panEnvLength)
		return;
	
	/* Shift all points after i up by one */
	for (int16_t j = i; j < ins->panEnvLength; j++)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j+1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j+1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->panEnvSustain > i)
		ins->panEnvSustain--;
	if (ins->panEnvLoopStart > i)
		ins->panEnvLoopStart--;
	if (ins->panEnvLoopEnd > i)
		ins->panEnvLoopEnd--;
	
	/* Ensure first point always at X=0 */
	ins->panEnvPoints[0][0] = 0;
	ins->panEnvLength--;
	
	/* Clamp indices to valid range */
	if (ins->panEnvSustain >= ins->panEnvLength)
		ins->panEnvSustain = ins->panEnvLength - 1;
	if (ins->panEnvLoopStart >= ins->panEnvLength)
		ins->panEnvLoopStart = ins->panEnvLength - 1;
	if (ins->panEnvLoopEnd >= ins->panEnvLength)
		ins->panEnvLoopEnd = ins->panEnvLength - 1;
	
	/* Update current point selection */
	if (ins->panEnvLength == 0)
		inst->editor.currPanEnvPoint = 0;
	else if (inst->editor.currPanEnvPoint >= ins->panEnvLength)
		inst->editor.currPanEnvPoint = ins->panEnvLength - 1;
	
	inst->uiState.updateInstEditor = true;
}

void pbPanEnvSusUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvSustain < instr->panEnvLength - 1)
	{
		instr->panEnvSustain++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvSusDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvSustain > 0)
	{
		instr->panEnvSustain--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepSUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopStart < instr->panEnvLoopEnd)
	{
		instr->panEnvLoopStart++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepSDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopStart > 0)
	{
		instr->panEnvLoopStart--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepEUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopEnd < instr->panEnvLength - 1)
	{
		instr->panEnvLoopEnd++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepEDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopEnd > instr->panEnvLoopStart)
	{
		instr->panEnvLoopEnd--;
		inst->uiState.updateInstEditor = true;
	}
}

/* Sample parameter buttons - these adjust scrollbar positions */
void pbInstVolDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->volume > 0)
	{
		smp->volume--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVolUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->volume < 64)
	{
		smp->volume++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstPanDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->panning > 0)
	{
		smp->panning--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstPanUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->panning < 255)
	{
		smp->panning++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFtuneDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->finetune > -128)
	{
		smp->finetune--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFtuneUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->finetune < 127)
	{
		smp->finetune++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFadeoutDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->fadeout > 0)
	{
		instr->fadeout--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFadeoutUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->fadeout < 0xFFF)
	{
		instr->fadeout++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSpeedDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibRate > 0)
	{
		instr->autoVibRate--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSpeedUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibRate < 0x3F)
	{
		instr->autoVibRate++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibDepthDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibDepth > 0)
	{
		instr->autoVibDepth--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibDepthUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibDepth < 0x0F)
	{
		instr->autoVibDepth++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSweepDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibSweep > 0)
	{
		instr->autoVibSweep--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSweepUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->autoVibSweep < 0xFF)
	{
		instr->autoVibSweep++;
		ft2_song_mark_modified(inst);
	}
}

/* Relative note */
void pbInstOctUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->relativeNote <= 71 - 12)
		smp->relativeNote += 12;
	else
		smp->relativeNote = 71;
	ft2_song_mark_modified(inst);
}

void pbInstOctDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->relativeNote >= -48 + 12)
		smp->relativeNote -= 12;
	else
		smp->relativeNote = -48;
	ft2_song_mark_modified(inst);
}

void pbInstHalftoneUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->relativeNote < 71)
	{
		smp->relativeNote++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstHalftoneDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	if (smp->relativeNote > -48)
	{
		smp->relativeNote--;
		ft2_song_mark_modified(inst);
	}
}

/* Exit */
void pbInstExit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	exitInstEditor(inst);
}

/* ========== INSTRUMENT EDITOR EXTENSION CALLBACKS ========== */

void pbInstExtMidiChDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiChannel > 0)
	{
		instr->midiChannel--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiChUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiChannel < 15)
	{
		instr->midiChannel++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiPrgDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiProgram > 0)
	{
		instr->midiProgram--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiPrgUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiProgram < 127)
	{
		instr->midiProgram++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiBendDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiBend > 0)
	{
		instr->midiBend--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiBendUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	if (instr->midiBend < 36)
	{
		instr->midiBend++;
		ft2_song_mark_modified(inst);
	}
}

/* ========== SAMPLE EDITOR CALLBACKS ========== */

void pbSampScrollLeft(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed == NULL || inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL)
		return;

	/* Early exit if view covers entire sample (matches standalone) */
	if (ed->viewSize == 0 || ed->viewSize == s->length)
		return;
	
	/* Scroll left by 1/32 of view size (matches standalone) */
	int32_t scrollAmount = ed->viewSize / 32;
	if (scrollAmount < 1)
		scrollAmount = 1;
	
	ed->scrPos -= scrollAmount;
	if (ed->scrPos < 0)
		ed->scrPos = 0;
	
	inst->uiState.updateSampleEditor = true;
}

void pbSampScrollRight(ft2_instance_t *inst)
{
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed == NULL || inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL)
		return;
	
	/* Early exit if view covers entire sample (matches standalone) */
	if (ed->viewSize == 0 || ed->viewSize == s->length)
		return;
	
	/* Scroll right by 1/32 of view size (matches standalone) */
	int32_t scrollAmount = ed->viewSize / 32;
	if (scrollAmount < 1)
		scrollAmount = 1;
	
	ed->scrPos += scrollAmount;
	int32_t maxPos = s->length - ed->viewSize;
	if (maxPos < 0)
		maxPos = 0;
	if (ed->scrPos > maxPos)
		ed->scrPos = maxPos;
	
	inst->uiState.updateSampleEditor = true;
}

void pbSampPNoteUp(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	if (inst->editor.smpEd_NoteNr < 96)
	{
		inst->editor.smpEd_NoteNr++;
		inst->uiState.updateSampleEditor = true;
	}
}

void pbSampPNoteDown(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	if (inst->editor.smpEd_NoteNr > 1)
	{
		inst->editor.smpEd_NoteNr--;
		inst->uiState.updateSampleEditor = true;
	}
}

void pbSampStop(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Stop all voices - matches standalone smpEdStop() which calls lockMixerCallback()/stopVoices() */
	ft2_stop_all_voices(inst);
}

void pbSampPlayWave(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Play the entire sample */
	uint8_t ch = inst->cursor.ch;
	uint8_t instr = inst->editor.curInstr;
	uint8_t smp = inst->editor.curSmp;
	uint8_t note = inst->editor.smpEd_NoteNr + 1; /* Convert 0-95 to 1-96 */
	
	ft2_instance_play_sample(inst, note, instr, smp, ch, 64, 0, 0);
}

void pbSampPlayRange(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Only play if there's a valid range selected - matches standalone behavior */
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed == NULL || !ed->hasRange || ed->rangeStart == ed->rangeEnd)
		return;
	
	uint8_t ch = inst->cursor.ch;
	uint8_t instr = inst->editor.curInstr;
	uint8_t smp = inst->editor.curSmp;
	uint8_t note = inst->editor.smpEd_NoteNr + 1;
	
		int32_t start = ed->rangeStart;
		int32_t end = ed->rangeEnd;
		if (start > end)
		{
			int32_t tmp = start;
			start = end;
			end = tmp;
		}
		ft2_instance_play_sample(inst, note, instr, smp, ch, 64, start, end - start);
}

void pbSampPlayDisplay(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	/* Play the displayed portion */
	uint8_t ch = inst->cursor.ch;
	uint8_t instr = inst->editor.curInstr;
	uint8_t smp = inst->editor.curSmp;
	uint8_t note = inst->editor.smpEd_NoteNr + 1;
	
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
	{
		ft2_instance_play_sample(inst, note, instr, smp, ch, 64, ed->scrPos, ed->viewSize);
	}
	else
	{
		/* No editor - play whole sample */
		ft2_instance_play_sample(inst, note, instr, smp, ch, 64, 0, 0);
	}
}

void pbSampShowRange(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_show_range(ed);
}

void pbSampRangeAll(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_range_all(ed);
}

void pbSampClrRange(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_clear_selection(ed);
}

void pbSampZoomOut(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_zoom_out(ed, SAMPLE_AREA_WIDTH / 2); /* Center zoom for button press */
}

void pbSampShowAll(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_show_all(ed);
}

void pbSampSaveRng(ft2_instance_t *inst) { (void)inst; }

void pbSampCut(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_cut(ed);
}

void pbSampCopy(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_copy(ed);
}

void pbSampPaste(ft2_instance_t *inst)
{
	(void)inst;
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed != NULL)
		ft2_sample_ed_paste(ed);
}

void pbSampCrop(ft2_instance_t *inst)
{
	sampCrop(inst);
}

void pbSampVolume(ft2_instance_t *inst)
{
	ft2_volume_panel_show(inst);
}

void pbSampEffects(ft2_instance_t *inst)
{
	showSampleEffectsScreen(inst);
}

void pbSampExit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	hideSampleEditor(inst);
	inst->uiState.patternEditorShown = true;
}

void pbSampClear(ft2_instance_t *inst)
{
	clearSample(inst);
}

void pbSampMin(ft2_instance_t *inst)
{
	sampMinimize(inst);
}

/* ========== SAMPLE EDITOR EFFECTS CALLBACKS ========== */

void pbSampFxCyclesUp(ft2_instance_t *inst) { pbSfxCyclesUp(inst); }
void pbSampFxCyclesDown(ft2_instance_t *inst) { pbSfxCyclesDown(inst); }
void pbSampFxTriangle(ft2_instance_t *inst) { pbSfxTriangle(inst); }
void pbSampFxSaw(ft2_instance_t *inst) { pbSfxSaw(inst); }
void pbSampFxSine(ft2_instance_t *inst) { pbSfxSine(inst); }
void pbSampFxSquare(ft2_instance_t *inst) { pbSfxSquare(inst); }
void pbSampFxResoUp(ft2_instance_t *inst) { pbSfxResoUp(inst); }
void pbSampFxResoDown(ft2_instance_t *inst) { pbSfxResoDown(inst); }
void pbSampFxLowPass(ft2_instance_t *inst) { pbSfxLowPass(inst); }
void pbSampFxHighPass(ft2_instance_t *inst) { pbSfxHighPass(inst); }
void pbSampFxSubBass(ft2_instance_t *inst) { pbSfxSubBass(inst); }
void pbSampFxSubTreble(ft2_instance_t *inst) { pbSfxSubTreble(inst); }
void pbSampFxAddBass(ft2_instance_t *inst) { pbSfxAddBass(inst); }
void pbSampFxAddTreble(ft2_instance_t *inst) { pbSfxAddTreble(inst); }
void pbSampFxSetAmp(ft2_instance_t *inst) { pbSfxSetAmp(inst); }
void pbSampFxUndo(ft2_instance_t *inst) { pbSfxUndo(inst); }
void pbSampFxXFade(ft2_instance_t *inst) { (void)inst; ft2_sample_ed_crossfade_loop(ft2_sample_ed_get_current()); }
void pbSampFxBack(ft2_instance_t *inst) { hideSampleEffectsScreen(inst); }
void cbSampFxNorm(ft2_instance_t *inst) { cbSfxNormalization(inst); }

/* ========== SAMPLE EDITOR EXTENSION CALLBACKS ========== */

void pbSampExtClearCopyBuf(ft2_instance_t *inst)
{
	clearCopyBuffer(inst);
}

void pbSampExtSign(ft2_instance_t *inst)
{
	sampleChangeSign(inst);
}

void pbSampExtEcho(ft2_instance_t *inst)
{
	ft2_echo_panel_show(inst);
}

void pbSampExtBackwards(ft2_instance_t *inst)
{
	sampleBackwards(inst);
}

void pbSampExtByteSwap(ft2_instance_t *inst)
{
	sampleByteSwap(inst);
}

void pbSampExtFixDC(ft2_instance_t *inst)
{
	fixDC(inst);
}

void pbSampExtCopyIns(ft2_instance_t *inst)
{
	copyInstr(inst);
}

void pbSampExtCopySmp(ft2_instance_t *inst)
{
	copySmp(inst);
}

void pbSampExtXchgIns(ft2_instance_t *inst)
{
	xchgInstr(inst);
}

void pbSampExtXchgSmp(ft2_instance_t *inst)
{
	xchgSmp(inst);
}

void pbSampExtResample(ft2_instance_t *inst)
{
	ft2_resample_panel_show(inst);
}

void pbSampExtMixSample(ft2_instance_t *inst)
{
	ft2_mix_panel_show(inst);
}

/* ========== SAMPLE EDITOR RADIO BUTTON CALLBACKS ========== */

void rbSampleNoLoop(ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Stop voices playing this sample before changing loop type */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	/* Clear loop flags */
	s->flags &= ~(LOOP_FWD | LOOP_BIDI);

	ft2_fix_sample(s);

	checkRadioButtonNoRedraw(RB_SAMPLE_NO_LOOP);

	if (inst != NULL)
	{
		inst->uiState.updateSampleEditor = true;
		ft2_song_mark_modified(inst);
	}
}

void rbSampleForwardLoop(ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Stop voices playing this sample before changing loop type */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	/* Set forward loop */
	s->flags &= ~LOOP_BIDI;
	s->flags |= LOOP_FWD;

	/* Initialize loop if not set */
	if (s->loopStart + s->loopLength == 0)
	{
		s->loopStart = 0;
		s->loopLength = s->length;
	}

	ft2_fix_sample(s);

	checkRadioButtonNoRedraw(RB_SAMPLE_FWD_LOOP);

	if (inst != NULL)
	{
		inst->uiState.updateSampleEditor = true;
		ft2_song_mark_modified(inst);
	}
}

void rbSamplePingpongLoop(ft2_instance_t *inst)
{
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Stop voices playing this sample before changing loop type */
	ft2_stop_sample_voices(inst, s);

	ft2_unfix_sample(s);

	/* Set bidi loop */
	s->flags &= ~LOOP_FWD;
	s->flags |= LOOP_BIDI;

	/* Initialize loop if not set */
	if (s->loopStart + s->loopLength == 0)
	{
		s->loopStart = 0;
		s->loopLength = s->length;
	}

	ft2_fix_sample(s);

	checkRadioButtonNoRedraw(RB_SAMPLE_BIDI_LOOP);

	if (inst != NULL)
	{
		inst->uiState.updateSampleEditor = true;
		ft2_song_mark_modified(inst);
	}
}

/* Callback for 8-bit conversion dialog result */
static void onConvert8BitResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;
	
	if (inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Must still be 16-bit */
	if (!(s->flags & SAMPLE_16BIT))
		return;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

		ft2_unfix_sample(s);

	if (result == DIALOG_RESULT_OK)
	{
		/* Yes - convert sample data (scale values from 16-bit to 8-bit) */
		int16_t *src = (int16_t *)s->dataPtr;
		int8_t *dst = s->dataPtr;
		
		for (int32_t i = 0; i < s->length; i++)
			dst[i] = (int8_t)(src[i] >> 8);

		s->flags &= ~SAMPLE_16BIT;
	}
	else
	{
		/* No - just reinterpret bytes (change flag, double length) */
		s->flags &= ~SAMPLE_16BIT;
		s->length <<= 1;  /* Double the length since each 16-bit sample becomes 2 8-bit samples */
		
		/* Adjust loop points */
		s->loopStart <<= 1;
		s->loopLength <<= 1;
	}
	
	ft2_fix_sample(s);
	checkRadioButtonNoRedraw(RB_SAMPLE_8BIT);
		inst->uiState.updateSampleEditor = true;
}

/* Callback for 16-bit conversion dialog result */
static void onConvert16BitResult(ft2_instance_t *inst, ft2_dialog_result_t result,
                                 const char *inputText, void *userData)
{
	(void)inputText;
	(void)userData;
	
	if (inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Must still be 8-bit */
	if (s->flags & SAMPLE_16BIT)
		return;

	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);

		ft2_unfix_sample(s);

	if (result == DIALOG_RESULT_OK)
	{
		/* Yes - convert sample data (scale values from 8-bit to 16-bit) */
		/* Need to reallocate - double the byte size, with proper padding */
		int32_t leftPadding = FT2_MAX_TAPS * 2;  /* 16-bit = 2 bytes per sample */
		int32_t rightPadding = FT2_MAX_TAPS * 2;
		int32_t dataLen = s->length * 2;
		size_t allocSize = (size_t)(leftPadding + dataLen + rightPadding);

		int8_t *newOrigPtr = (int8_t *)calloc(allocSize, 1);
		if (newOrigPtr == NULL)
		{
			ft2_fix_sample(s);
			return;
		}
		
		int8_t *newDataPtr = newOrigPtr + leftPadding;
		
		/* Convert 8-bit to 16-bit (process in reverse to handle in-place properly) */
		int16_t *dst = (int16_t *)newDataPtr;
		int8_t *src = s->dataPtr;
			for (int32_t i = s->length - 1; i >= 0; i--)
			dst[i] = (int16_t)(src[i] << 8);

			free(s->origDataPtr);
		s->origDataPtr = newOrigPtr;
		s->dataPtr = newDataPtr;
			s->flags |= SAMPLE_16BIT;
	}
	else
	{
		/* No - just reinterpret bytes (change flag, halve length) */
		s->flags |= SAMPLE_16BIT;
		s->length >>= 1;  /* Halve the length since each pair of bytes becomes one 16-bit sample */
		
		/* Adjust loop points */
		s->loopStart >>= 1;
		s->loopLength >>= 1;
		
		/* Clamp loop to valid range */
		if (s->loopStart < 0)
			s->loopStart = 0;
		if (s->loopStart + s->loopLength > s->length)
		{
			s->loopLength = s->length - s->loopStart;
			if (s->loopLength < 0)
			{
				s->loopLength = 0;
				s->loopStart = 0;
			}
		}
	}
	
	ft2_fix_sample(s);
	checkRadioButtonNoRedraw(RB_SAMPLE_16BIT);
		inst->uiState.updateSampleEditor = true;
}

void rbSample8bit(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Already 8-bit? */
	if (!(s->flags & SAMPLE_16BIT))
		return;

	/* Show confirmation dialog */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Pre-convert sample data?",
			inst, onConvert8BitResult, NULL);
	}
}

void rbSample16bit(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_sample_t *s = getCurSample(inst);
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	/* Already 16-bit? */
	if (s->flags & SAMPLE_16BIT)
		return;

	/* Show confirmation dialog */
	ft2_ui_t *ui = ft2_ui_get_current();
	if (ui != NULL)
	{
		ft2_dialog_show_yesno_cb(&ui->dialog,
			"System request", "Pre-convert sample data?",
			inst, onConvert16BitResult, NULL);
	}
}

/* Disk op callbacks are implemented in ft2_plugin_diskop.c */

/* ========== SCROLLBAR CALLBACKS ========== */

void sbPosEd(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	
	if (pos < inst->replayer.song.songLength)
	{
		inst->replayer.song.songPos = (int16_t)pos;
		
		/* Update pattern number from order list */
		inst->replayer.song.pattNum = inst->replayer.song.orders[inst->replayer.song.songPos];
		inst->replayer.song.currNumRows = inst->replayer.patternNumRows[inst->replayer.song.pattNum];
		
		/* Reset row to 0 (matches standalone setNewSongPos -> setPos(pos, 0, true)) */
		inst->replayer.song.row = 0;
		if (!inst->replayer.songPlaying)
		{
			inst->editor.row = 0;
			inst->editor.editPattern = (uint8_t)inst->replayer.song.pattNum;
		}
		
		inst->uiState.updatePosSections = true;
		inst->uiState.updatePatternEditor = true;
	}
}

void sbSampleList(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	inst->editor.sampleBankOffset = (uint8_t)pos;
}

void sbChanScroll(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL) return;
	setChannelScrollPos(inst, pos);
}

void sbInstVol(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->volume = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstPan(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->panning = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstFtune(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->finetune = (int8_t)(pos - 128);
	ft2_song_mark_modified(inst);
}

void sbInstFadeout(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->fadeout = (uint16_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibSpeed(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibRate = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibDepth(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibDepth = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibSweep(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibSweep = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiCh(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->midiChannel = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiPrg(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->midiProgram = (int16_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiBend(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->midiBend = (int16_t)pos;
	ft2_song_mark_modified(inst);
}

void sbSampScroll(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	if (ed == NULL || inst == NULL)
		return;
	
	ed->scrPos = (int32_t)pos;
	inst->uiState.updateSampleEditor = true;
}

/* ========== CHECKBOX CALLBACKS ========== */

void cbInstVEnv(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_ENABLED;
	ft2_song_mark_modified(inst);
}

void cbInstVEnvSus(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_SUSTAIN;
	ft2_song_mark_modified(inst);
}

void cbInstVEnvLoop(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_LOOP;
	ft2_song_mark_modified(inst);
}

void cbInstPEnv(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_ENABLED;
	ft2_song_mark_modified(inst);
}

void cbInstPEnvSus(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_SUSTAIN;
	ft2_song_mark_modified(inst);
}

void cbInstPEnvLoop(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_LOOP;
	ft2_song_mark_modified(inst);
}

void cbInstExtMidi(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->midiOn ^= 1;
	ft2_song_mark_modified(inst);
}

void cbInstExtMute(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->mute ^= 1;
	ft2_song_mark_modified(inst);
}

/* ========== RADIO BUTTON CALLBACKS ========== */

void rbInstWaveSine(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibType = 0;
	ft2_song_mark_modified(inst);
}

void rbInstWaveSquare(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibType = 1;
	ft2_song_mark_modified(inst);
}

void rbInstWaveRampDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibType = 2;
	ft2_song_mark_modified(inst);
}

void rbInstWaveRampUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getCurInstr(inst);
	if (instr == NULL) return;
	instr->autoVibType = 3;
	ft2_song_mark_modified(inst);
}

void rbSampNoLoop(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->flags = (smp->flags & ~(FT2_LOOP_FWD | FT2_LOOP_BIDI)) | FT2_LOOP_OFF;
}

void rbSampForwardLoop(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->flags = (smp->flags & ~FT2_LOOP_BIDI) | FT2_LOOP_FWD;
}

void rbSampPingPongLoop(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->flags = (smp->flags & ~FT2_LOOP_FWD) | FT2_LOOP_BIDI;
}

void rbSamp8Bit(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->flags &= ~FT2_SAMPLE_16BIT;
}

void rbSamp16Bit(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSample(inst);
	if (smp == NULL) return;
	smp->flags |= FT2_SAMPLE_16BIT;
}

/* ========== CALLBACK INITIALIZATION ========== */

void initCallbacks(void)
{
	/* Position editor buttons - arrow buttons use callbackFuncOnDown for repeat */
	pushButtons[PB_POSED_POS_UP].callbackFuncOnDown = pbPosEdPosUp;
	pushButtons[PB_POSED_POS_UP].preDelay = 1;
	pushButtons[PB_POSED_POS_UP].delayFrames = 4;
	pushButtons[PB_POSED_POS_DOWN].callbackFuncOnDown = pbPosEdPosDown;
	pushButtons[PB_POSED_POS_DOWN].preDelay = 1;
	pushButtons[PB_POSED_POS_DOWN].delayFrames = 4;
	pushButtons[PB_POSED_INS].callbackFuncOnUp = pbPosEdIns;
	pushButtons[PB_POSED_DEL].callbackFuncOnUp = pbPosEdDel;
	pushButtons[PB_POSED_PATT_UP].callbackFuncOnDown = pbPosEdPattUp;
	pushButtons[PB_POSED_PATT_UP].preDelay = 1;
	pushButtons[PB_POSED_PATT_UP].delayFrames = 6;
	pushButtons[PB_POSED_PATT_DOWN].callbackFuncOnDown = pbPosEdPattDown;
	pushButtons[PB_POSED_PATT_DOWN].preDelay = 1;
	pushButtons[PB_POSED_PATT_DOWN].delayFrames = 6;
	pushButtons[PB_POSED_LEN_UP].callbackFuncOnDown = pbPosEdLenUp;
	pushButtons[PB_POSED_LEN_UP].preDelay = 1;
	pushButtons[PB_POSED_LEN_UP].delayFrames = 4;
	pushButtons[PB_POSED_LEN_DOWN].callbackFuncOnDown = pbPosEdLenDown;
	pushButtons[PB_POSED_LEN_DOWN].preDelay = 1;
	pushButtons[PB_POSED_LEN_DOWN].delayFrames = 4;
	pushButtons[PB_POSED_REP_UP].callbackFuncOnDown = pbPosEdRepUp;
	pushButtons[PB_POSED_REP_UP].preDelay = 1;
	pushButtons[PB_POSED_REP_UP].delayFrames = 4;
	pushButtons[PB_POSED_REP_DOWN].callbackFuncOnDown = pbPosEdRepDown;
	pushButtons[PB_POSED_REP_DOWN].preDelay = 1;
	pushButtons[PB_POSED_REP_DOWN].delayFrames = 4;

	/* Song/pattern buttons - arrow buttons use callbackFuncOnDown for repeat */
	pushButtons[PB_BPM_UP].callbackFuncOnDown = pbBPMUp;
	pushButtons[PB_BPM_UP].preDelay = 1;
	pushButtons[PB_BPM_UP].delayFrames = 4;
	pushButtons[PB_BPM_DOWN].callbackFuncOnDown = pbBPMDown;
	pushButtons[PB_BPM_DOWN].preDelay = 1;
	pushButtons[PB_BPM_DOWN].delayFrames = 4;
	pushButtons[PB_SPEED_UP].callbackFuncOnDown = pbSpeedUp;
	pushButtons[PB_SPEED_UP].preDelay = 1;
	pushButtons[PB_SPEED_UP].delayFrames = 4;
	pushButtons[PB_SPEED_DOWN].callbackFuncOnDown = pbSpeedDown;
	pushButtons[PB_SPEED_DOWN].preDelay = 1;
	pushButtons[PB_SPEED_DOWN].delayFrames = 4;
	pushButtons[PB_EDITADD_UP].callbackFuncOnDown = pbEditAddUp;
	pushButtons[PB_EDITADD_UP].preDelay = 1;
	pushButtons[PB_EDITADD_UP].delayFrames = 4;
	pushButtons[PB_EDITADD_DOWN].callbackFuncOnDown = pbEditAddDown;
	pushButtons[PB_EDITADD_DOWN].preDelay = 1;
	pushButtons[PB_EDITADD_DOWN].delayFrames = 4;
	pushButtons[PB_PATT_UP].callbackFuncOnDown = pbPattUp;
	pushButtons[PB_PATT_UP].preDelay = 1;
	pushButtons[PB_PATT_UP].delayFrames = 4;
	pushButtons[PB_PATT_DOWN].callbackFuncOnDown = pbPattDown;
	pushButtons[PB_PATT_DOWN].preDelay = 1;
	pushButtons[PB_PATT_DOWN].delayFrames = 4;
	pushButtons[PB_PATTLEN_UP].callbackFuncOnDown = pbPattLenUp;
	pushButtons[PB_PATTLEN_UP].preDelay = 1;
	pushButtons[PB_PATTLEN_UP].delayFrames = 4;
	pushButtons[PB_PATTLEN_DOWN].callbackFuncOnDown = pbPattLenDown;
	pushButtons[PB_PATTLEN_DOWN].preDelay = 1;
	pushButtons[PB_PATTLEN_DOWN].delayFrames = 4;
	/* Expand/Shrink only fire on release */
	pushButtons[PB_PATT_EXPAND].callbackFuncOnUp = pbPattExpand;
	pushButtons[PB_PATT_SHRINK].callbackFuncOnUp = pbPattShrink;

	/* Playback buttons */
	pushButtons[PB_PLAY_SONG].callbackFuncOnUp = pbPlaySong;
	pushButtons[PB_PLAY_PATT].callbackFuncOnUp = pbPlayPatt;
	pushButtons[PB_STOP].callbackFuncOnUp = pbStop;
	pushButtons[PB_RECORD_SONG].callbackFuncOnUp = pbRecordSong;
	pushButtons[PB_RECORD_PATT].callbackFuncOnUp = pbRecordPatt;

	/* Menu buttons */
	pushButtons[PB_DISK_OP].callbackFuncOnUp = pbDiskOp;
	pushButtons[PB_INST_ED].callbackFuncOnUp = pbInstEd;
	pushButtons[PB_SMP_ED].callbackFuncOnUp = pbSmpEd;
	pushButtons[PB_CONFIG].callbackFuncOnUp = pbConfig;
	pushButtons[PB_CONFIG_EXIT].callbackFuncOnUp = pbConfigExit;
	pushButtons[PB_HELP].callbackFuncOnUp = pbHelp;
	pushButtons[PB_HELP_EXIT].callbackFuncOnUp = pbHelpExit;
	pushButtons[PB_HELP_SCROLL_UP].callbackFuncOnDown = pbHelpScrollUp;
	pushButtons[PB_HELP_SCROLL_DOWN].callbackFuncOnDown = pbHelpScrollDown;
	pushButtons[PB_ABOUT].callbackFuncOnUp = pbAbout;
	pushButtons[PB_EXIT_ABOUT].callbackFuncOnUp = pbExitAbout;
	pushButtons[PB_NIBBLES].callbackFuncOnUp = pbNibbles;
	pushButtons[PB_KILL].callbackFuncOnUp = pbKill;
	pushButtons[PB_TRIM].callbackFuncOnUp = pbTrim;
	pushButtons[PB_TRIM_CALC].callbackFuncOnUp = pbTrimCalcWrapper;
	pushButtons[PB_TRIM_TRIM].callbackFuncOnUp = pbTrimDoTrimWrapper;
	pushButtons[PB_EXTEND_VIEW].callbackFuncOnUp = pbExtendView;
	pushButtons[PB_TRANSPOSE].callbackFuncOnUp = pbTranspose;

	/* Transpose operation buttons */
	pushButtons[PB_TRANSP_CUR_INS_TRK_UP].callbackFuncOnUp = pbTrackTranspCurInsUp;
	pushButtons[PB_TRANSP_CUR_INS_TRK_DN].callbackFuncOnUp = pbTrackTranspCurInsDn;
	pushButtons[PB_TRANSP_CUR_INS_TRK_12UP].callbackFuncOnUp = pbTrackTranspCurIns12Up;
	pushButtons[PB_TRANSP_CUR_INS_TRK_12DN].callbackFuncOnUp = pbTrackTranspCurIns12Dn;
	pushButtons[PB_TRANSP_ALL_INS_TRK_UP].callbackFuncOnUp = pbTrackTranspAllInsUp;
	pushButtons[PB_TRANSP_ALL_INS_TRK_DN].callbackFuncOnUp = pbTrackTranspAllInsDn;
	pushButtons[PB_TRANSP_ALL_INS_TRK_12UP].callbackFuncOnUp = pbTrackTranspAllIns12Up;
	pushButtons[PB_TRANSP_ALL_INS_TRK_12DN].callbackFuncOnUp = pbTrackTranspAllIns12Dn;
	pushButtons[PB_TRANSP_CUR_INS_PAT_UP].callbackFuncOnUp = pbPattTranspCurInsUp;
	pushButtons[PB_TRANSP_CUR_INS_PAT_DN].callbackFuncOnUp = pbPattTranspCurInsDn;
	pushButtons[PB_TRANSP_CUR_INS_PAT_12UP].callbackFuncOnUp = pbPattTranspCurIns12Up;
	pushButtons[PB_TRANSP_CUR_INS_PAT_12DN].callbackFuncOnUp = pbPattTranspCurIns12Dn;
	pushButtons[PB_TRANSP_ALL_INS_PAT_UP].callbackFuncOnUp = pbPattTranspAllInsUp;
	pushButtons[PB_TRANSP_ALL_INS_PAT_DN].callbackFuncOnUp = pbPattTranspAllInsDn;
	pushButtons[PB_TRANSP_ALL_INS_PAT_12UP].callbackFuncOnUp = pbPattTranspAllIns12Up;
	pushButtons[PB_TRANSP_ALL_INS_PAT_12DN].callbackFuncOnUp = pbPattTranspAllIns12Dn;
	pushButtons[PB_TRANSP_CUR_INS_SNG_UP].callbackFuncOnUp = pbSongTranspCurInsUp;
	pushButtons[PB_TRANSP_CUR_INS_SNG_DN].callbackFuncOnUp = pbSongTranspCurInsDn;
	pushButtons[PB_TRANSP_CUR_INS_SNG_12UP].callbackFuncOnUp = pbSongTranspCurIns12Up;
	pushButtons[PB_TRANSP_CUR_INS_SNG_12DN].callbackFuncOnUp = pbSongTranspCurIns12Dn;
	pushButtons[PB_TRANSP_ALL_INS_SNG_UP].callbackFuncOnUp = pbSongTranspAllInsUp;
	pushButtons[PB_TRANSP_ALL_INS_SNG_DN].callbackFuncOnUp = pbSongTranspAllInsDn;
	pushButtons[PB_TRANSP_ALL_INS_SNG_12UP].callbackFuncOnUp = pbSongTranspAllIns12Up;
	pushButtons[PB_TRANSP_ALL_INS_SNG_12DN].callbackFuncOnUp = pbSongTranspAllIns12Dn;
	pushButtons[PB_TRANSP_CUR_INS_BLK_UP].callbackFuncOnUp = pbBlockTranspCurInsUp;
	pushButtons[PB_TRANSP_CUR_INS_BLK_DN].callbackFuncOnUp = pbBlockTranspCurInsDn;
	pushButtons[PB_TRANSP_CUR_INS_BLK_12UP].callbackFuncOnUp = pbBlockTranspCurIns12Up;
	pushButtons[PB_TRANSP_CUR_INS_BLK_12DN].callbackFuncOnUp = pbBlockTranspCurIns12Dn;
	pushButtons[PB_TRANSP_ALL_INS_BLK_UP].callbackFuncOnUp = pbBlockTranspAllInsUp;
	pushButtons[PB_TRANSP_ALL_INS_BLK_DN].callbackFuncOnUp = pbBlockTranspAllInsDn;
	pushButtons[PB_TRANSP_ALL_INS_BLK_12UP].callbackFuncOnUp = pbBlockTranspAllIns12Up;
	pushButtons[PB_TRANSP_ALL_INS_BLK_12DN].callbackFuncOnUp = pbBlockTranspAllIns12Dn;

	pushButtons[PB_INST_ED_EXT].callbackFuncOnUp = pbInstEdExt;
	pushButtons[PB_SMP_ED_EXT].callbackFuncOnUp = pbSmpEdExt;
	pushButtons[PB_ADV_EDIT].callbackFuncOnUp = pbAdvEdit;

	/* Advanced edit remap buttons */
	pushButtons[PB_REMAP_TRACK].callbackFuncOnUp = pbRemapTrack;
	pushButtons[PB_REMAP_PATTERN].callbackFuncOnUp = pbRemapPattern;
	pushButtons[PB_REMAP_SONG].callbackFuncOnUp = pbRemapSong;
	pushButtons[PB_REMAP_BLOCK].callbackFuncOnUp = pbRemapBlock;

	pushButtons[PB_ADD_CHANNELS].callbackFuncOnUp = pbAddChannels;
	pushButtons[PB_SUB_CHANNELS].callbackFuncOnUp = pbSubChannels;

	/* Logo/Badge buttons */
	pushButtons[PB_LOGO].callbackFuncOnUp = pbLogo;
	pushButtons[PB_BADGE].callbackFuncOnUp = pbBadge;

	/* Instrument switcher */
	pushButtons[PB_SWAP_BANK].callbackFuncOnUp = pbSwapInstrBank;
	pushButtons[PB_SAMPLE_LIST_UP].callbackFuncOnUp = pbSampleListUp;
	pushButtons[PB_SAMPLE_LIST_DOWN].callbackFuncOnUp = pbSampleListDown;

	/* Channel scroll buttons - use funcOnDown for continuous scrolling */
	pushButtons[PB_CHAN_SCROLL_LEFT].callbackFuncOnDown = pbChanScrollLeft;
	pushButtons[PB_CHAN_SCROLL_RIGHT].callbackFuncOnDown = pbChanScrollRight;

	/* Instrument range buttons */
	pushButtons[PB_RANGE1].callbackFuncOnUp = pbRange1;
	pushButtons[PB_RANGE2].callbackFuncOnUp = pbRange2;
	pushButtons[PB_RANGE3].callbackFuncOnUp = pbRange3;
	pushButtons[PB_RANGE4].callbackFuncOnUp = pbRange4;
	pushButtons[PB_RANGE5].callbackFuncOnUp = pbRange5;
	pushButtons[PB_RANGE6].callbackFuncOnUp = pbRange6;
	pushButtons[PB_RANGE7].callbackFuncOnUp = pbRange7;
	pushButtons[PB_RANGE8].callbackFuncOnUp = pbRange8;
	pushButtons[PB_RANGE9].callbackFuncOnUp = pbRange9;
	pushButtons[PB_RANGE10].callbackFuncOnUp = pbRange10;
	pushButtons[PB_RANGE11].callbackFuncOnUp = pbRange11;
	pushButtons[PB_RANGE12].callbackFuncOnUp = pbRange12;
	pushButtons[PB_RANGE13].callbackFuncOnUp = pbRange13;
	pushButtons[PB_RANGE14].callbackFuncOnUp = pbRange14;
	pushButtons[PB_RANGE15].callbackFuncOnUp = pbRange15;
	pushButtons[PB_RANGE16].callbackFuncOnUp = pbRange16;

	/* Instrument editor - envelope presets */
	pushButtons[PB_INST_VDEF1].callbackFuncOnUp = pbVolPreDef1;
	pushButtons[PB_INST_VDEF2].callbackFuncOnUp = pbVolPreDef2;
	pushButtons[PB_INST_VDEF3].callbackFuncOnUp = pbVolPreDef3;
	pushButtons[PB_INST_VDEF4].callbackFuncOnUp = pbVolPreDef4;
	pushButtons[PB_INST_VDEF5].callbackFuncOnUp = pbVolPreDef5;
	pushButtons[PB_INST_VDEF6].callbackFuncOnUp = pbVolPreDef6;
	pushButtons[PB_INST_PDEF1].callbackFuncOnUp = pbPanPreDef1;
	pushButtons[PB_INST_PDEF2].callbackFuncOnUp = pbPanPreDef2;
	pushButtons[PB_INST_PDEF3].callbackFuncOnUp = pbPanPreDef3;
	pushButtons[PB_INST_PDEF4].callbackFuncOnUp = pbPanPreDef4;
	pushButtons[PB_INST_PDEF5].callbackFuncOnUp = pbPanPreDef5;
	pushButtons[PB_INST_PDEF6].callbackFuncOnUp = pbPanPreDef6;

	/* Instrument editor - volume envelope */
	pushButtons[PB_INST_VP_ADD].callbackFuncOnDown = pbVolEnvAdd;
	pushButtons[PB_INST_VP_DEL].callbackFuncOnDown = pbVolEnvDel;
	pushButtons[PB_INST_VS_UP].callbackFuncOnDown = pbVolEnvSusUp;
	pushButtons[PB_INST_VS_DOWN].callbackFuncOnDown = pbVolEnvSusDown;
	pushButtons[PB_INST_VREPS_UP].callbackFuncOnDown = pbVolEnvRepSUp;
	pushButtons[PB_INST_VREPS_DOWN].callbackFuncOnDown = pbVolEnvRepSDown;
	pushButtons[PB_INST_VREPE_UP].callbackFuncOnDown = pbVolEnvRepEUp;
	pushButtons[PB_INST_VREPE_DOWN].callbackFuncOnDown = pbVolEnvRepEDown;

	/* Instrument editor - pan envelope */
	pushButtons[PB_INST_PP_ADD].callbackFuncOnDown = pbPanEnvAdd;
	pushButtons[PB_INST_PP_DEL].callbackFuncOnDown = pbPanEnvDel;
	pushButtons[PB_INST_PS_UP].callbackFuncOnDown = pbPanEnvSusUp;
	pushButtons[PB_INST_PS_DOWN].callbackFuncOnDown = pbPanEnvSusDown;
	pushButtons[PB_INST_PREPS_UP].callbackFuncOnDown = pbPanEnvRepSUp;
	pushButtons[PB_INST_PREPS_DOWN].callbackFuncOnDown = pbPanEnvRepSDown;
	pushButtons[PB_INST_PREPE_UP].callbackFuncOnDown = pbPanEnvRepEUp;
	pushButtons[PB_INST_PREPE_DOWN].callbackFuncOnDown = pbPanEnvRepEDown;

	/* Instrument editor - sample parameters */
	pushButtons[PB_INST_VOL_DOWN].callbackFuncOnDown = pbInstVolDown;
	pushButtons[PB_INST_VOL_UP].callbackFuncOnDown = pbInstVolUp;
	pushButtons[PB_INST_PAN_DOWN].callbackFuncOnDown = pbInstPanDown;
	pushButtons[PB_INST_PAN_UP].callbackFuncOnDown = pbInstPanUp;
	pushButtons[PB_INST_FTUNE_DOWN].callbackFuncOnDown = pbInstFtuneDown;
	pushButtons[PB_INST_FTUNE_UP].callbackFuncOnDown = pbInstFtuneUp;
	pushButtons[PB_INST_FADEOUT_DOWN].callbackFuncOnDown = pbInstFadeoutDown;
	pushButtons[PB_INST_FADEOUT_UP].callbackFuncOnDown = pbInstFadeoutUp;
	pushButtons[PB_INST_VIBSPEED_DOWN].callbackFuncOnDown = pbInstVibSpeedDown;
	pushButtons[PB_INST_VIBSPEED_UP].callbackFuncOnDown = pbInstVibSpeedUp;
	pushButtons[PB_INST_VIBDEPTH_DOWN].callbackFuncOnDown = pbInstVibDepthDown;
	pushButtons[PB_INST_VIBDEPTH_UP].callbackFuncOnDown = pbInstVibDepthUp;
	pushButtons[PB_INST_VIBSWEEP_DOWN].callbackFuncOnDown = pbInstVibSweepDown;
	pushButtons[PB_INST_VIBSWEEP_UP].callbackFuncOnDown = pbInstVibSweepUp;

	/* Instrument editor - relative note */
	pushButtons[PB_INST_OCT_UP].callbackFuncOnDown = pbInstOctUp;
	pushButtons[PB_INST_OCT_DOWN].callbackFuncOnDown = pbInstOctDown;
	pushButtons[PB_INST_HALFTONE_UP].callbackFuncOnDown = pbInstHalftoneUp;
	pushButtons[PB_INST_HALFTONE_DOWN].callbackFuncOnDown = pbInstHalftoneDown;

	/* Instrument editor - exit */
	pushButtons[PB_INST_EXIT].callbackFuncOnUp = pbInstExit;

	/* Instrument editor extension */
	pushButtons[PB_INST_EXT_MIDI_CH_DOWN].callbackFuncOnDown = pbInstExtMidiChDown;
	pushButtons[PB_INST_EXT_MIDI_CH_UP].callbackFuncOnDown = pbInstExtMidiChUp;
	pushButtons[PB_INST_EXT_MIDI_PRG_DOWN].callbackFuncOnDown = pbInstExtMidiPrgDown;
	pushButtons[PB_INST_EXT_MIDI_PRG_UP].callbackFuncOnDown = pbInstExtMidiPrgUp;
	pushButtons[PB_INST_EXT_MIDI_BEND_DOWN].callbackFuncOnDown = pbInstExtMidiBendDown;
	pushButtons[PB_INST_EXT_MIDI_BEND_UP].callbackFuncOnDown = pbInstExtMidiBendUp;

	/* Sample editor */
	pushButtons[PB_SAMP_SCROLL_LEFT].callbackFuncOnDown = pbSampScrollLeft;
	pushButtons[PB_SAMP_SCROLL_RIGHT].callbackFuncOnDown = pbSampScrollRight;
	pushButtons[PB_SAMP_PNOTE_UP].callbackFuncOnDown = pbSampPNoteUp;
	pushButtons[PB_SAMP_PNOTE_DOWN].callbackFuncOnDown = pbSampPNoteDown;
	pushButtons[PB_SAMP_STOP].callbackFuncOnUp = pbSampStop;
	pushButtons[PB_SAMP_PWAVE].callbackFuncOnUp = pbSampPlayWave;
	pushButtons[PB_SAMP_PRANGE].callbackFuncOnUp = pbSampPlayRange;
	pushButtons[PB_SAMP_PDISPLAY].callbackFuncOnUp = pbSampPlayDisplay;
	pushButtons[PB_SAMP_SHOW_RANGE].callbackFuncOnUp = pbSampShowRange;
	pushButtons[PB_SAMP_RANGE_ALL].callbackFuncOnUp = pbSampRangeAll;
	pushButtons[PB_SAMP_CLR_RANGE].callbackFuncOnUp = pbSampClrRange;
	pushButtons[PB_SAMP_ZOOM_OUT].callbackFuncOnUp = pbSampZoomOut;
	pushButtons[PB_SAMP_SHOW_ALL].callbackFuncOnUp = pbSampShowAll;
	pushButtons[PB_SAMP_SAVE_RNG].callbackFuncOnUp = pbSampSaveRng;
	pushButtons[PB_SAMP_CUT].callbackFuncOnUp = pbSampCut;
	pushButtons[PB_SAMP_COPY].callbackFuncOnUp = pbSampCopy;
	pushButtons[PB_SAMP_PASTE].callbackFuncOnUp = pbSampPaste;
	pushButtons[PB_SAMP_CROP].callbackFuncOnUp = pbSampCrop;
	pushButtons[PB_SAMP_VOLUME].callbackFuncOnUp = pbSampVolume;
	pushButtons[PB_SAMP_EFFECTS].callbackFuncOnUp = pbSampEffects;
	pushButtons[PB_SAMP_EXIT].callbackFuncOnUp = pbSampExit;
	pushButtons[PB_SAMP_CLEAR].callbackFuncOnUp = pbSampClear;
	pushButtons[PB_SAMP_MIN].callbackFuncOnUp = pbSampMin;
	pushButtons[PB_SAMP_REPEAT_UP].callbackFuncOnDown = sampRepeatUp;
	pushButtons[PB_SAMP_REPEAT_DOWN].callbackFuncOnDown = sampRepeatDown;
	pushButtons[PB_SAMP_REPLEN_UP].callbackFuncOnDown = sampReplenUp;
	pushButtons[PB_SAMP_REPLEN_DOWN].callbackFuncOnDown = sampReplenDown;

	/* Sample editor effects */
	pushButtons[PB_SAMPFX_CYCLES_UP].callbackFuncOnDown = pbSampFxCyclesUp;
	pushButtons[PB_SAMPFX_CYCLES_DOWN].callbackFuncOnDown = pbSampFxCyclesDown;
	pushButtons[PB_SAMPFX_TRIANGLE].callbackFuncOnUp = pbSampFxTriangle;
	pushButtons[PB_SAMPFX_SAW].callbackFuncOnUp = pbSampFxSaw;
	pushButtons[PB_SAMPFX_SINE].callbackFuncOnUp = pbSampFxSine;
	pushButtons[PB_SAMPFX_SQUARE].callbackFuncOnUp = pbSampFxSquare;
	pushButtons[PB_SAMPFX_RESO_UP].callbackFuncOnDown = pbSampFxResoUp;
	pushButtons[PB_SAMPFX_RESO_DOWN].callbackFuncOnDown = pbSampFxResoDown;
	pushButtons[PB_SAMPFX_LOWPASS].callbackFuncOnUp = pbSampFxLowPass;
	pushButtons[PB_SAMPFX_HIGHPASS].callbackFuncOnUp = pbSampFxHighPass;
	pushButtons[PB_SAMPFX_SUB_BASS].callbackFuncOnUp = pbSampFxSubBass;
	pushButtons[PB_SAMPFX_SUB_TREBLE].callbackFuncOnUp = pbSampFxSubTreble;
	pushButtons[PB_SAMPFX_ADD_BASS].callbackFuncOnUp = pbSampFxAddBass;
	pushButtons[PB_SAMPFX_ADD_TREBLE].callbackFuncOnUp = pbSampFxAddTreble;
	pushButtons[PB_SAMPFX_SET_AMP].callbackFuncOnUp = pbSampFxSetAmp;
	pushButtons[PB_SAMPFX_UNDO].callbackFuncOnUp = pbSampFxUndo;
	pushButtons[PB_SAMPFX_XFADE].callbackFuncOnUp = pbSampFxXFade;
	pushButtons[PB_SAMPFX_BACK].callbackFuncOnUp = pbSampFxBack;

	/* Sample editor extension */
	pushButtons[PB_SAMP_EXT_CLEAR_COPYBUF].callbackFuncOnUp = pbSampExtClearCopyBuf;
	pushButtons[PB_SAMP_EXT_CONV].callbackFuncOnUp = pbSampExtSign;
	pushButtons[PB_SAMP_EXT_ECHO].callbackFuncOnUp = pbSampExtEcho;
	pushButtons[PB_SAMP_EXT_BACKWARDS].callbackFuncOnUp = pbSampExtBackwards;
	pushButtons[PB_SAMP_EXT_CONV_W].callbackFuncOnUp = pbSampExtByteSwap;
	pushButtons[PB_SAMP_EXT_MORPH].callbackFuncOnUp = pbSampExtFixDC;
	pushButtons[PB_SAMP_EXT_COPY_INS].callbackFuncOnUp = pbSampExtCopyIns;
	pushButtons[PB_SAMP_EXT_COPY_SMP].callbackFuncOnUp = pbSampExtCopySmp;
	pushButtons[PB_SAMP_EXT_XCHG_INS].callbackFuncOnUp = pbSampExtXchgIns;
	pushButtons[PB_SAMP_EXT_XCHG_SMP].callbackFuncOnUp = pbSampExtXchgSmp;
	pushButtons[PB_SAMP_EXT_RESAMPLE].callbackFuncOnUp = pbSampExtResample;
	pushButtons[PB_SAMP_EXT_MIX_SAMPLE].callbackFuncOnUp = pbSampExtMixSample;

	/* Disk op */
	pushButtons[PB_DISKOP_SAVE].callbackFuncOnUp = pbDiskOpSave;
	pushButtons[PB_DISKOP_MAKEDIR].callbackFuncOnUp = pbDiskOpMakeDir;
	pushButtons[PB_DISKOP_REFRESH].callbackFuncOnUp = pbDiskOpRefresh;
	pushButtons[PB_DISKOP_SET_PATH].callbackFuncOnUp = pbDiskOpSetPath;
	pushButtons[PB_DISKOP_SHOW_ALL].callbackFuncOnUp = pbDiskOpShowAll;
	pushButtons[PB_DISKOP_EXIT].callbackFuncOnUp = pbDiskOpExit;
	pushButtons[PB_DISKOP_ROOT].callbackFuncOnUp = pbDiskOpRoot;
	pushButtons[PB_DISKOP_PARENT].callbackFuncOnUp = pbDiskOpParent;
	pushButtons[PB_DISKOP_HOME].callbackFuncOnUp = pbDiskOpHome;
	pushButtons[PB_DISKOP_LIST_UP].callbackFuncOnDown = pbDiskOpListUp;
	pushButtons[PB_DISKOP_LIST_DOWN].callbackFuncOnDown = pbDiskOpListDown;

	/* Scrollbar callbacks */
	scrollBars[SB_POS_ED].callbackFunc = sbPosEd;
	scrollBars[SB_SAMPLE_LIST].callbackFunc = sbSampleList;
	scrollBars[SB_CHAN_SCROLL].callbackFunc = sbChanScroll;
	scrollBars[SB_INST_VOL].callbackFunc = sbInstVol;
	scrollBars[SB_INST_PAN].callbackFunc = sbInstPan;
	scrollBars[SB_INST_FTUNE].callbackFunc = sbInstFtune;
	scrollBars[SB_INST_FADEOUT].callbackFunc = sbInstFadeout;
	scrollBars[SB_INST_VIBSPEED].callbackFunc = sbInstVibSpeed;
	scrollBars[SB_INST_VIBDEPTH].callbackFunc = sbInstVibDepth;
	scrollBars[SB_INST_VIBSWEEP].callbackFunc = sbInstVibSweep;
	scrollBars[SB_INST_EXT_MIDI_CH].callbackFunc = sbInstExtMidiCh;
	scrollBars[SB_INST_EXT_MIDI_PRG].callbackFunc = sbInstExtMidiPrg;
	scrollBars[SB_INST_EXT_MIDI_BEND].callbackFunc = sbInstExtMidiBend;
	scrollBars[SB_SAMP_SCROLL].callbackFunc = sbSampScroll;
	scrollBars[SB_HELP_SCROLL].callbackFunc = sbHelpScroll;
	scrollBars[SB_DISKOP_LIST].callbackFunc = sbDiskOpSetPos;

	/* Sample editor radio buttons */
	radioButtons[RB_SAMPLE_NO_LOOP].callbackFunc = rbSampleNoLoop;
	radioButtons[RB_SAMPLE_FWD_LOOP].callbackFunc = rbSampleForwardLoop;
	radioButtons[RB_SAMPLE_BIDI_LOOP].callbackFunc = rbSamplePingpongLoop;
	radioButtons[RB_SAMPLE_8BIT].callbackFunc = rbSample8bit;
	radioButtons[RB_SAMPLE_16BIT].callbackFunc = rbSample16bit;

	/* Sample effects checkbox */
	checkBoxes[CB_SAMPFX_NORM].callbackFunc = cbSampFxNorm;

	/* Instrument editor checkboxes */
	checkBoxes[CB_INST_VENV].callbackFunc = cbInstVEnv;
	checkBoxes[CB_INST_VENV_SUS].callbackFunc = cbInstVEnvSus;
	checkBoxes[CB_INST_VENV_LOOP].callbackFunc = cbInstVEnvLoop;
	checkBoxes[CB_INST_PENV].callbackFunc = cbInstPEnv;
	checkBoxes[CB_INST_PENV_SUS].callbackFunc = cbInstPEnvSus;
	checkBoxes[CB_INST_PENV_LOOP].callbackFunc = cbInstPEnvLoop;

	/* Instrument editor vibrato waveform radio buttons */
	radioButtons[RB_INST_WAVE_SINE].callbackFunc = rbInstWaveSine;
	radioButtons[RB_INST_WAVE_SQUARE].callbackFunc = rbInstWaveSquare;
	radioButtons[RB_INST_WAVE_RAMPDN].callbackFunc = rbInstWaveRampDown;
	radioButtons[RB_INST_WAVE_RAMPUP].callbackFunc = rbInstWaveRampUp;

	/* Config audio arrow buttons */
	pushButtons[PB_CONFIG_AMP_DOWN].callbackFuncOnDown = configAmpDown;
	pushButtons[PB_CONFIG_AMP_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_AMP_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_AMP_UP].callbackFuncOnDown = configAmpUp;
	pushButtons[PB_CONFIG_AMP_UP].preDelay = 1;
	pushButtons[PB_CONFIG_AMP_UP].delayFrames = 4;
	pushButtons[PB_CONFIG_MASTVOL_DOWN].callbackFuncOnDown = configMasterVolDown;
	pushButtons[PB_CONFIG_MASTVOL_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_MASTVOL_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_MASTVOL_UP].callbackFuncOnDown = configMasterVolUp;
	pushButtons[PB_CONFIG_MASTVOL_UP].preDelay = 1;
	pushButtons[PB_CONFIG_MASTVOL_UP].delayFrames = 4;

	/* Config layout palette arrow buttons */
	pushButtons[PB_CONFIG_PAL_R_DOWN].callbackFuncOnDown = configPalRDown;
	pushButtons[PB_CONFIG_PAL_R_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_R_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_R_UP].callbackFuncOnDown = configPalRUp;
	pushButtons[PB_CONFIG_PAL_R_UP].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_R_UP].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_G_DOWN].callbackFuncOnDown = configPalGDown;
	pushButtons[PB_CONFIG_PAL_G_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_G_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_G_UP].callbackFuncOnDown = configPalGUp;
	pushButtons[PB_CONFIG_PAL_G_UP].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_G_UP].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_B_DOWN].callbackFuncOnDown = configPalBDown;
	pushButtons[PB_CONFIG_PAL_B_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_B_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_B_UP].callbackFuncOnDown = configPalBUp;
	pushButtons[PB_CONFIG_PAL_B_UP].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_B_UP].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_CONT_DOWN].callbackFuncOnDown = configPalContDown;
	pushButtons[PB_CONFIG_PAL_CONT_DOWN].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_CONT_DOWN].delayFrames = 4;
	pushButtons[PB_CONFIG_PAL_CONT_UP].callbackFuncOnDown = configPalContUp;
	pushButtons[PB_CONFIG_PAL_CONT_UP].preDelay = 1;
	pushButtons[PB_CONFIG_PAL_CONT_UP].delayFrames = 4;

	/* Nibbles buttons */
	pushButtons[PB_NIBBLES_PLAY].callbackFuncOnUp = pbNibblesPlay;
	pushButtons[PB_NIBBLES_HELP].callbackFuncOnUp = pbNibblesHelp;
	pushButtons[PB_NIBBLES_HIGHS].callbackFuncOnUp = pbNibblesHighScores;
	pushButtons[PB_NIBBLES_EXIT].callbackFuncOnUp = pbNibblesExit;

	/* Nibbles radio buttons */
	radioButtons[RB_NIBBLES_1PLAYER].callbackFunc = rbNibbles1Player;
	radioButtons[RB_NIBBLES_2PLAYER].callbackFunc = rbNibbles2Players;
	radioButtons[RB_NIBBLES_NOVICE].callbackFunc = rbNibblesNovice;
	radioButtons[RB_NIBBLES_AVERAGE].callbackFunc = rbNibblesAverage;
	radioButtons[RB_NIBBLES_PRO].callbackFunc = rbNibblesPro;
	radioButtons[RB_NIBBLES_TRITON].callbackFunc = rbNibblesTriton;

	/* Nibbles checkboxes */
	checkBoxes[CB_NIBBLES_SURROUND].callbackFunc = cbNibblesSurround;
	checkBoxes[CB_NIBBLES_GRID].callbackFunc = cbNibblesGrid;
	checkBoxes[CB_NIBBLES_WRAP].callbackFunc = cbNibblesWrap;

	/* Disk op radio buttons */
	radioButtons[RB_DISKOP_MODULE].callbackFunc = rbDiskOpModule;
	radioButtons[RB_DISKOP_INSTR].callbackFunc = rbDiskOpInstr;
	radioButtons[RB_DISKOP_SAMPLE].callbackFunc = rbDiskOpSample;
	radioButtons[RB_DISKOP_PATTERN].callbackFunc = rbDiskOpPattern;
	radioButtons[RB_DISKOP_TRACK].callbackFunc = rbDiskOpTrack;

	radioButtons[RB_DISKOP_MOD_MOD].callbackFunc = rbDiskOpModSaveMod;
	radioButtons[RB_DISKOP_MOD_XM].callbackFunc = rbDiskOpModSaveXm;
	/* RB_DISKOP_MOD_WAV disabled in plugin - no WAV export */
	radioButtons[RB_DISKOP_SMP_RAW].callbackFunc = rbDiskOpSmpSaveRaw;
	radioButtons[RB_DISKOP_SMP_IFF].callbackFunc = rbDiskOpSmpSaveIff;
	radioButtons[RB_DISKOP_SMP_WAV].callbackFunc = rbDiskOpSmpSaveWav;
}
