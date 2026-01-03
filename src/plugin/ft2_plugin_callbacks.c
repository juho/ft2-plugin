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
#include "ft2_plugin_widgets.h"
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
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
	inst->uiState.updatePatternEditor = true;
	inst->uiState.updatePosSections = true;
}

void pbPosEdLenUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	
	if (inst->replayer.song.songLength < 255)
	{
		inst->replayer.song.songLength++;
		ft2_song_mark_modified(inst);
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
		ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_song_mark_modified(inst);
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
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
		/* Also hide S.E.Ext if open */
		if (inst->uiState.sampleEditorExtShown)
		{
			hideSampleEditorExt(inst);
		}
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpHowToUseFT2(inst, video, bmp);
	}
}

void cbHelpPlugin(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		rbHelpPlugin(inst, video, bmp);
	}
}

/* Help scroll button callbacks */
void pbHelpScrollUp(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
	{
		ft2_video_t *video = &ui->video;
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		ft2_about_show(&ui->widgets, video, bmp);
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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		hidePushButton(widgets, PB_EXIT_ABOUT);
	
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
	
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL)
		return;
	
	ft2_dialog_show_zap_cb(&ui->dialog, "System request", "Total devastation of the...",
	                       inst, zapDialogCallback, NULL);
}

void pbTrim(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	togglePatternEditorExtended(inst);
}

void pbExitExtPatt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	exitPatternEditorExtended(inst);
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
	toggleInstEditorExt(inst);
}

void pbSmpEdExt(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	toggleSampleEditorExt(inst);
}

void pbAdvEdit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;

	inst->config.id_FastLogo ^= 1;
	changeLogoType(&ui->widgets, &ui->bmp, inst->config.id_FastLogo);
	drawPushButton(&ui->widgets, &ui->video, &ui->bmp, PB_LOGO);
}

void pbBadge(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL) return;

	inst->config.id_TritonProd ^= 1;
	changeBadgeType(&ui->widgets, &ui->bmp, inst->config.id_TritonProd);
	drawPushButton(&ui->widgets, &ui->video, &ui->bmp, PB_BADGE);
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

/* ========== SAMPLE EDITOR CALLBACKS ========== */

void pbSampScrollLeft(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	
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
	if (inst == NULL || inst->ui == NULL)
		return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	
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
	if (inst->ui == NULL)
		return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	if (!ed->hasRange || ed->rangeStart == ed->rangeEnd)
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
	
	if (inst->ui != NULL)
	{
		ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
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
	ft2_sample_ed_show_range(inst);
}

void pbSampRangeAll(ft2_instance_t *inst)
{
	ft2_sample_ed_range_all(inst);
}

void pbSampClrRange(ft2_instance_t *inst)
{
	ft2_sample_ed_clear_selection(inst);
}

void pbSampZoomOut(ft2_instance_t *inst)
{
	ft2_sample_ed_zoom_out(inst, SAMPLE_AREA_WIDTH / 2); /* Center zoom for button press */
}

void pbSampShowAll(ft2_instance_t *inst)
{
	ft2_sample_ed_show_all(inst);
}

void pbSampSaveRng(ft2_instance_t *inst) { (void)inst; }

void pbSampCut(ft2_instance_t *inst)
{
	ft2_sample_ed_cut(inst);
}

void pbSampCopy(ft2_instance_t *inst)
{
	ft2_sample_ed_copy(inst);
}

void pbSampPaste(ft2_instance_t *inst)
{
	ft2_sample_ed_paste(inst);
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
void pbSampFxXFade(ft2_instance_t *inst) { ft2_sample_ed_crossfade_loop(inst); }
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

	ft2_widgets_t *widgets = (inst != NULL && inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_SAMPLE_NO_LOOP);

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

	ft2_widgets_t *widgets = (inst != NULL && inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_SAMPLE_FWD_LOOP);

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

	ft2_widgets_t *widgets2 = (inst != NULL && inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets2 != NULL)
		checkRadioButtonNoRedraw(widgets2, RB_SAMPLE_BIDI_LOOP);

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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_SAMPLE_8BIT);
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
	ft2_widgets_t *widgets = (inst->ui != NULL) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (widgets != NULL)
		checkRadioButtonNoRedraw(widgets, RB_SAMPLE_16BIT);
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
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

void sbSampScroll(ft2_instance_t *inst, uint32_t pos)
{
	if (inst == NULL || inst->ui == NULL)
		return;
	ft2_sample_editor_t *ed = FT2_SAMPLE_ED(inst);
	
	ed->scrPos = (int32_t)pos;
	inst->uiState.updateSampleEditor = true;
}

/* ========== CALLBACK INITIALIZATION ========== */

void initCallbacks(ft2_widgets_t *widgets)
{
	if (widgets == NULL)
		return;

	/* Position editor buttons - arrow buttons use callbackFuncOnDown for repeat */
	widgets->pushButtons[PB_POSED_POS_UP].callbackFuncOnDown = pbPosEdPosUp;
	widgets->pushButtons[PB_POSED_POS_UP].preDelay = 1;
	widgets->pushButtons[PB_POSED_POS_UP].delayFrames = 4;
	widgets->pushButtons[PB_POSED_POS_DOWN].callbackFuncOnDown = pbPosEdPosDown;
	widgets->pushButtons[PB_POSED_POS_DOWN].preDelay = 1;
	widgets->pushButtons[PB_POSED_POS_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_POSED_INS].callbackFuncOnUp = pbPosEdIns;
	widgets->pushButtons[PB_POSED_DEL].callbackFuncOnUp = pbPosEdDel;
	widgets->pushButtons[PB_POSED_PATT_UP].callbackFuncOnDown = pbPosEdPattUp;
	widgets->pushButtons[PB_POSED_PATT_UP].preDelay = 1;
	widgets->pushButtons[PB_POSED_PATT_UP].delayFrames = 6;
	widgets->pushButtons[PB_POSED_PATT_DOWN].callbackFuncOnDown = pbPosEdPattDown;
	widgets->pushButtons[PB_POSED_PATT_DOWN].preDelay = 1;
	widgets->pushButtons[PB_POSED_PATT_DOWN].delayFrames = 6;
	widgets->pushButtons[PB_POSED_LEN_UP].callbackFuncOnDown = pbPosEdLenUp;
	widgets->pushButtons[PB_POSED_LEN_UP].preDelay = 1;
	widgets->pushButtons[PB_POSED_LEN_UP].delayFrames = 4;
	widgets->pushButtons[PB_POSED_LEN_DOWN].callbackFuncOnDown = pbPosEdLenDown;
	widgets->pushButtons[PB_POSED_LEN_DOWN].preDelay = 1;
	widgets->pushButtons[PB_POSED_LEN_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_POSED_REP_UP].callbackFuncOnDown = pbPosEdRepUp;
	widgets->pushButtons[PB_POSED_REP_UP].preDelay = 1;
	widgets->pushButtons[PB_POSED_REP_UP].delayFrames = 4;
	widgets->pushButtons[PB_POSED_REP_DOWN].callbackFuncOnDown = pbPosEdRepDown;
	widgets->pushButtons[PB_POSED_REP_DOWN].preDelay = 1;
	widgets->pushButtons[PB_POSED_REP_DOWN].delayFrames = 4;

	/* Song/pattern buttons - arrow buttons use callbackFuncOnDown for repeat */
	widgets->pushButtons[PB_BPM_UP].callbackFuncOnDown = pbBPMUp;
	widgets->pushButtons[PB_BPM_UP].preDelay = 1;
	widgets->pushButtons[PB_BPM_UP].delayFrames = 4;
	widgets->pushButtons[PB_BPM_DOWN].callbackFuncOnDown = pbBPMDown;
	widgets->pushButtons[PB_BPM_DOWN].preDelay = 1;
	widgets->pushButtons[PB_BPM_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_SPEED_UP].callbackFuncOnDown = pbSpeedUp;
	widgets->pushButtons[PB_SPEED_UP].preDelay = 1;
	widgets->pushButtons[PB_SPEED_UP].delayFrames = 4;
	widgets->pushButtons[PB_SPEED_DOWN].callbackFuncOnDown = pbSpeedDown;
	widgets->pushButtons[PB_SPEED_DOWN].preDelay = 1;
	widgets->pushButtons[PB_SPEED_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_EDITADD_UP].callbackFuncOnDown = pbEditAddUp;
	widgets->pushButtons[PB_EDITADD_UP].preDelay = 1;
	widgets->pushButtons[PB_EDITADD_UP].delayFrames = 4;
	widgets->pushButtons[PB_EDITADD_DOWN].callbackFuncOnDown = pbEditAddDown;
	widgets->pushButtons[PB_EDITADD_DOWN].preDelay = 1;
	widgets->pushButtons[PB_EDITADD_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_PATT_UP].callbackFuncOnDown = pbPattUp;
	widgets->pushButtons[PB_PATT_UP].preDelay = 1;
	widgets->pushButtons[PB_PATT_UP].delayFrames = 4;
	widgets->pushButtons[PB_PATT_DOWN].callbackFuncOnDown = pbPattDown;
	widgets->pushButtons[PB_PATT_DOWN].preDelay = 1;
	widgets->pushButtons[PB_PATT_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_PATTLEN_UP].callbackFuncOnDown = pbPattLenUp;
	widgets->pushButtons[PB_PATTLEN_UP].preDelay = 1;
	widgets->pushButtons[PB_PATTLEN_UP].delayFrames = 4;
	widgets->pushButtons[PB_PATTLEN_DOWN].callbackFuncOnDown = pbPattLenDown;
	widgets->pushButtons[PB_PATTLEN_DOWN].preDelay = 1;
	widgets->pushButtons[PB_PATTLEN_DOWN].delayFrames = 4;
	/* Expand/Shrink only fire on release */
	widgets->pushButtons[PB_PATT_EXPAND].callbackFuncOnUp = pbPattExpand;
	widgets->pushButtons[PB_PATT_SHRINK].callbackFuncOnUp = pbPattShrink;

	/* Playback buttons */
	widgets->pushButtons[PB_PLAY_SONG].callbackFuncOnUp = pbPlaySong;
	widgets->pushButtons[PB_PLAY_PATT].callbackFuncOnUp = pbPlayPatt;
	widgets->pushButtons[PB_STOP].callbackFuncOnUp = pbStop;
	widgets->pushButtons[PB_RECORD_SONG].callbackFuncOnUp = pbRecordSong;
	widgets->pushButtons[PB_RECORD_PATT].callbackFuncOnUp = pbRecordPatt;

	/* Menu buttons */
	widgets->pushButtons[PB_DISK_OP].callbackFuncOnUp = pbDiskOp;
	widgets->pushButtons[PB_INST_ED].callbackFuncOnUp = pbInstEd;
	widgets->pushButtons[PB_SMP_ED].callbackFuncOnUp = pbSmpEd;
	widgets->pushButtons[PB_CONFIG].callbackFuncOnUp = pbConfig;
	widgets->pushButtons[PB_CONFIG_EXIT].callbackFuncOnUp = pbConfigExit;
	widgets->pushButtons[PB_CONFIG_RESET].callbackFuncOnUp = pbConfigReset;
	widgets->pushButtons[PB_CONFIG_LOAD].callbackFuncOnUp = pbConfigLoad;
	widgets->pushButtons[PB_CONFIG_SAVE].callbackFuncOnUp = pbConfigSave;

	/* Channel output routing buttons */
	widgets->pushButtons[PB_CONFIG_ROUTING_CH1_UP].callbackFuncOnDown = pbRoutingCh1Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH1_DOWN].callbackFuncOnDown = pbRoutingCh1Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH2_UP].callbackFuncOnDown = pbRoutingCh2Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH2_DOWN].callbackFuncOnDown = pbRoutingCh2Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH3_UP].callbackFuncOnDown = pbRoutingCh3Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH3_DOWN].callbackFuncOnDown = pbRoutingCh3Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH4_UP].callbackFuncOnDown = pbRoutingCh4Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH4_DOWN].callbackFuncOnDown = pbRoutingCh4Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH5_UP].callbackFuncOnDown = pbRoutingCh5Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH5_DOWN].callbackFuncOnDown = pbRoutingCh5Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH6_UP].callbackFuncOnDown = pbRoutingCh6Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH6_DOWN].callbackFuncOnDown = pbRoutingCh6Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH7_UP].callbackFuncOnDown = pbRoutingCh7Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH7_DOWN].callbackFuncOnDown = pbRoutingCh7Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH8_UP].callbackFuncOnDown = pbRoutingCh8Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH8_DOWN].callbackFuncOnDown = pbRoutingCh8Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH9_UP].callbackFuncOnDown = pbRoutingCh9Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH9_DOWN].callbackFuncOnDown = pbRoutingCh9Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH10_UP].callbackFuncOnDown = pbRoutingCh10Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH10_DOWN].callbackFuncOnDown = pbRoutingCh10Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH11_UP].callbackFuncOnDown = pbRoutingCh11Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH11_DOWN].callbackFuncOnDown = pbRoutingCh11Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH12_UP].callbackFuncOnDown = pbRoutingCh12Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH12_DOWN].callbackFuncOnDown = pbRoutingCh12Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH13_UP].callbackFuncOnDown = pbRoutingCh13Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH13_DOWN].callbackFuncOnDown = pbRoutingCh13Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH14_UP].callbackFuncOnDown = pbRoutingCh14Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH14_DOWN].callbackFuncOnDown = pbRoutingCh14Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH15_UP].callbackFuncOnDown = pbRoutingCh15Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH15_DOWN].callbackFuncOnDown = pbRoutingCh15Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH16_UP].callbackFuncOnDown = pbRoutingCh16Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH16_DOWN].callbackFuncOnDown = pbRoutingCh16Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH17_UP].callbackFuncOnDown = pbRoutingCh17Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH17_DOWN].callbackFuncOnDown = pbRoutingCh17Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH18_UP].callbackFuncOnDown = pbRoutingCh18Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH18_DOWN].callbackFuncOnDown = pbRoutingCh18Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH19_UP].callbackFuncOnDown = pbRoutingCh19Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH19_DOWN].callbackFuncOnDown = pbRoutingCh19Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH20_UP].callbackFuncOnDown = pbRoutingCh20Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH20_DOWN].callbackFuncOnDown = pbRoutingCh20Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH21_UP].callbackFuncOnDown = pbRoutingCh21Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH21_DOWN].callbackFuncOnDown = pbRoutingCh21Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH22_UP].callbackFuncOnDown = pbRoutingCh22Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH22_DOWN].callbackFuncOnDown = pbRoutingCh22Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH23_UP].callbackFuncOnDown = pbRoutingCh23Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH23_DOWN].callbackFuncOnDown = pbRoutingCh23Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH24_UP].callbackFuncOnDown = pbRoutingCh24Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH24_DOWN].callbackFuncOnDown = pbRoutingCh24Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH25_UP].callbackFuncOnDown = pbRoutingCh25Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH25_DOWN].callbackFuncOnDown = pbRoutingCh25Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH26_UP].callbackFuncOnDown = pbRoutingCh26Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH26_DOWN].callbackFuncOnDown = pbRoutingCh26Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH27_UP].callbackFuncOnDown = pbRoutingCh27Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH27_DOWN].callbackFuncOnDown = pbRoutingCh27Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH28_UP].callbackFuncOnDown = pbRoutingCh28Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH28_DOWN].callbackFuncOnDown = pbRoutingCh28Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH29_UP].callbackFuncOnDown = pbRoutingCh29Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH29_DOWN].callbackFuncOnDown = pbRoutingCh29Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH30_UP].callbackFuncOnDown = pbRoutingCh30Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH30_DOWN].callbackFuncOnDown = pbRoutingCh30Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH31_UP].callbackFuncOnDown = pbRoutingCh31Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH31_DOWN].callbackFuncOnDown = pbRoutingCh31Down;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH32_UP].callbackFuncOnDown = pbRoutingCh32Up;
	widgets->pushButtons[PB_CONFIG_ROUTING_CH32_DOWN].callbackFuncOnDown = pbRoutingCh32Down;

	widgets->pushButtons[PB_HELP].callbackFuncOnUp = pbHelp;
	widgets->pushButtons[PB_HELP_EXIT].callbackFuncOnUp = pbHelpExit;
	widgets->pushButtons[PB_HELP_SCROLL_UP].callbackFuncOnDown = pbHelpScrollUp;
	widgets->pushButtons[PB_HELP_SCROLL_DOWN].callbackFuncOnDown = pbHelpScrollDown;
	widgets->pushButtons[PB_ABOUT].callbackFuncOnUp = pbAbout;
	widgets->pushButtons[PB_EXIT_ABOUT].callbackFuncOnUp = pbExitAbout;
	widgets->pushButtons[PB_NIBBLES].callbackFuncOnUp = pbNibbles;
	widgets->pushButtons[PB_KILL].callbackFuncOnUp = pbKill;
	widgets->pushButtons[PB_TRIM].callbackFuncOnUp = pbTrim;
	widgets->pushButtons[PB_TRIM_CALC].callbackFuncOnUp = pbTrimCalcWrapper;
	widgets->pushButtons[PB_TRIM_TRIM].callbackFuncOnUp = pbTrimDoTrimWrapper;
	widgets->pushButtons[PB_EXTEND_VIEW].callbackFuncOnUp = pbExtendView;
	widgets->pushButtons[PB_EXIT_EXT_PATT].callbackFuncOnUp = pbExitExtPatt;
	widgets->pushButtons[PB_TRANSPOSE].callbackFuncOnUp = pbTranspose;

	/* Transpose operation buttons */
	widgets->pushButtons[PB_TRANSP_CUR_INS_TRK_UP].callbackFuncOnUp = pbTrackTranspCurInsUp;
	widgets->pushButtons[PB_TRANSP_CUR_INS_TRK_DN].callbackFuncOnUp = pbTrackTranspCurInsDn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_TRK_12UP].callbackFuncOnUp = pbTrackTranspCurIns12Up;
	widgets->pushButtons[PB_TRANSP_CUR_INS_TRK_12DN].callbackFuncOnUp = pbTrackTranspCurIns12Dn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_TRK_UP].callbackFuncOnUp = pbTrackTranspAllInsUp;
	widgets->pushButtons[PB_TRANSP_ALL_INS_TRK_DN].callbackFuncOnUp = pbTrackTranspAllInsDn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_TRK_12UP].callbackFuncOnUp = pbTrackTranspAllIns12Up;
	widgets->pushButtons[PB_TRANSP_ALL_INS_TRK_12DN].callbackFuncOnUp = pbTrackTranspAllIns12Dn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_PAT_UP].callbackFuncOnUp = pbPattTranspCurInsUp;
	widgets->pushButtons[PB_TRANSP_CUR_INS_PAT_DN].callbackFuncOnUp = pbPattTranspCurInsDn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_PAT_12UP].callbackFuncOnUp = pbPattTranspCurIns12Up;
	widgets->pushButtons[PB_TRANSP_CUR_INS_PAT_12DN].callbackFuncOnUp = pbPattTranspCurIns12Dn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_PAT_UP].callbackFuncOnUp = pbPattTranspAllInsUp;
	widgets->pushButtons[PB_TRANSP_ALL_INS_PAT_DN].callbackFuncOnUp = pbPattTranspAllInsDn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_PAT_12UP].callbackFuncOnUp = pbPattTranspAllIns12Up;
	widgets->pushButtons[PB_TRANSP_ALL_INS_PAT_12DN].callbackFuncOnUp = pbPattTranspAllIns12Dn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_SNG_UP].callbackFuncOnUp = pbSongTranspCurInsUp;
	widgets->pushButtons[PB_TRANSP_CUR_INS_SNG_DN].callbackFuncOnUp = pbSongTranspCurInsDn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_SNG_12UP].callbackFuncOnUp = pbSongTranspCurIns12Up;
	widgets->pushButtons[PB_TRANSP_CUR_INS_SNG_12DN].callbackFuncOnUp = pbSongTranspCurIns12Dn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_SNG_UP].callbackFuncOnUp = pbSongTranspAllInsUp;
	widgets->pushButtons[PB_TRANSP_ALL_INS_SNG_DN].callbackFuncOnUp = pbSongTranspAllInsDn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_SNG_12UP].callbackFuncOnUp = pbSongTranspAllIns12Up;
	widgets->pushButtons[PB_TRANSP_ALL_INS_SNG_12DN].callbackFuncOnUp = pbSongTranspAllIns12Dn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_BLK_UP].callbackFuncOnUp = pbBlockTranspCurInsUp;
	widgets->pushButtons[PB_TRANSP_CUR_INS_BLK_DN].callbackFuncOnUp = pbBlockTranspCurInsDn;
	widgets->pushButtons[PB_TRANSP_CUR_INS_BLK_12UP].callbackFuncOnUp = pbBlockTranspCurIns12Up;
	widgets->pushButtons[PB_TRANSP_CUR_INS_BLK_12DN].callbackFuncOnUp = pbBlockTranspCurIns12Dn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_BLK_UP].callbackFuncOnUp = pbBlockTranspAllInsUp;
	widgets->pushButtons[PB_TRANSP_ALL_INS_BLK_DN].callbackFuncOnUp = pbBlockTranspAllInsDn;
	widgets->pushButtons[PB_TRANSP_ALL_INS_BLK_12UP].callbackFuncOnUp = pbBlockTranspAllIns12Up;
	widgets->pushButtons[PB_TRANSP_ALL_INS_BLK_12DN].callbackFuncOnUp = pbBlockTranspAllIns12Dn;

	widgets->pushButtons[PB_INST_ED_EXT].callbackFuncOnUp = pbInstEdExt;
	widgets->pushButtons[PB_SMP_ED_EXT].callbackFuncOnUp = pbSmpEdExt;
	widgets->pushButtons[PB_ADV_EDIT].callbackFuncOnUp = pbAdvEdit;

	/* Advanced edit remap buttons */
	widgets->pushButtons[PB_REMAP_TRACK].callbackFuncOnUp = pbRemapTrack;
	widgets->pushButtons[PB_REMAP_PATTERN].callbackFuncOnUp = pbRemapPattern;
	widgets->pushButtons[PB_REMAP_SONG].callbackFuncOnUp = pbRemapSong;
	widgets->pushButtons[PB_REMAP_BLOCK].callbackFuncOnUp = pbRemapBlock;

	widgets->pushButtons[PB_ADD_CHANNELS].callbackFuncOnUp = pbAddChannels;
	widgets->pushButtons[PB_SUB_CHANNELS].callbackFuncOnUp = pbSubChannels;

	/* Logo/Badge buttons */
	widgets->pushButtons[PB_LOGO].callbackFuncOnUp = pbLogo;
	widgets->pushButtons[PB_BADGE].callbackFuncOnUp = pbBadge;

	/* Instrument switcher */
	widgets->pushButtons[PB_SWAP_BANK].callbackFuncOnUp = pbSwapInstrBank;
	widgets->pushButtons[PB_SAMPLE_LIST_UP].callbackFuncOnUp = pbSampleListUp;
	widgets->pushButtons[PB_SAMPLE_LIST_DOWN].callbackFuncOnUp = pbSampleListDown;

	/* Channel scroll buttons - use funcOnDown for continuous scrolling */
	widgets->pushButtons[PB_CHAN_SCROLL_LEFT].callbackFuncOnDown = pbChanScrollLeft;
	widgets->pushButtons[PB_CHAN_SCROLL_RIGHT].callbackFuncOnDown = pbChanScrollRight;

	/* Instrument range buttons */
	widgets->pushButtons[PB_RANGE1].callbackFuncOnUp = pbRange1;
	widgets->pushButtons[PB_RANGE2].callbackFuncOnUp = pbRange2;
	widgets->pushButtons[PB_RANGE3].callbackFuncOnUp = pbRange3;
	widgets->pushButtons[PB_RANGE4].callbackFuncOnUp = pbRange4;
	widgets->pushButtons[PB_RANGE5].callbackFuncOnUp = pbRange5;
	widgets->pushButtons[PB_RANGE6].callbackFuncOnUp = pbRange6;
	widgets->pushButtons[PB_RANGE7].callbackFuncOnUp = pbRange7;
	widgets->pushButtons[PB_RANGE8].callbackFuncOnUp = pbRange8;
	widgets->pushButtons[PB_RANGE9].callbackFuncOnUp = pbRange9;
	widgets->pushButtons[PB_RANGE10].callbackFuncOnUp = pbRange10;
	widgets->pushButtons[PB_RANGE11].callbackFuncOnUp = pbRange11;
	widgets->pushButtons[PB_RANGE12].callbackFuncOnUp = pbRange12;
	widgets->pushButtons[PB_RANGE13].callbackFuncOnUp = pbRange13;
	widgets->pushButtons[PB_RANGE14].callbackFuncOnUp = pbRange14;
	widgets->pushButtons[PB_RANGE15].callbackFuncOnUp = pbRange15;
	widgets->pushButtons[PB_RANGE16].callbackFuncOnUp = pbRange16;

	/* Instrument editor - envelope presets */
	widgets->pushButtons[PB_INST_VDEF1].callbackFuncOnUp = pbVolPreDef1;
	widgets->pushButtons[PB_INST_VDEF2].callbackFuncOnUp = pbVolPreDef2;
	widgets->pushButtons[PB_INST_VDEF3].callbackFuncOnUp = pbVolPreDef3;
	widgets->pushButtons[PB_INST_VDEF4].callbackFuncOnUp = pbVolPreDef4;
	widgets->pushButtons[PB_INST_VDEF5].callbackFuncOnUp = pbVolPreDef5;
	widgets->pushButtons[PB_INST_VDEF6].callbackFuncOnUp = pbVolPreDef6;
	widgets->pushButtons[PB_INST_PDEF1].callbackFuncOnUp = pbPanPreDef1;
	widgets->pushButtons[PB_INST_PDEF2].callbackFuncOnUp = pbPanPreDef2;
	widgets->pushButtons[PB_INST_PDEF3].callbackFuncOnUp = pbPanPreDef3;
	widgets->pushButtons[PB_INST_PDEF4].callbackFuncOnUp = pbPanPreDef4;
	widgets->pushButtons[PB_INST_PDEF5].callbackFuncOnUp = pbPanPreDef5;
	widgets->pushButtons[PB_INST_PDEF6].callbackFuncOnUp = pbPanPreDef6;

	/* Instrument editor - volume envelope */
	widgets->pushButtons[PB_INST_VP_ADD].callbackFuncOnDown = pbVolEnvAdd;
	widgets->pushButtons[PB_INST_VP_DEL].callbackFuncOnDown = pbVolEnvDel;
	widgets->pushButtons[PB_INST_VS_UP].callbackFuncOnDown = pbVolEnvSusUp;
	widgets->pushButtons[PB_INST_VS_DOWN].callbackFuncOnDown = pbVolEnvSusDown;
	widgets->pushButtons[PB_INST_VREPS_UP].callbackFuncOnDown = pbVolEnvRepSUp;
	widgets->pushButtons[PB_INST_VREPS_DOWN].callbackFuncOnDown = pbVolEnvRepSDown;
	widgets->pushButtons[PB_INST_VREPE_UP].callbackFuncOnDown = pbVolEnvRepEUp;
	widgets->pushButtons[PB_INST_VREPE_DOWN].callbackFuncOnDown = pbVolEnvRepEDown;

	/* Instrument editor - pan envelope */
	widgets->pushButtons[PB_INST_PP_ADD].callbackFuncOnDown = pbPanEnvAdd;
	widgets->pushButtons[PB_INST_PP_DEL].callbackFuncOnDown = pbPanEnvDel;
	widgets->pushButtons[PB_INST_PS_UP].callbackFuncOnDown = pbPanEnvSusUp;
	widgets->pushButtons[PB_INST_PS_DOWN].callbackFuncOnDown = pbPanEnvSusDown;
	widgets->pushButtons[PB_INST_PREPS_UP].callbackFuncOnDown = pbPanEnvRepSUp;
	widgets->pushButtons[PB_INST_PREPS_DOWN].callbackFuncOnDown = pbPanEnvRepSDown;
	widgets->pushButtons[PB_INST_PREPE_UP].callbackFuncOnDown = pbPanEnvRepEUp;
	widgets->pushButtons[PB_INST_PREPE_DOWN].callbackFuncOnDown = pbPanEnvRepEDown;

	/* Instrument editor - sample parameters */
	widgets->pushButtons[PB_INST_VOL_DOWN].callbackFuncOnDown = pbInstVolDown;
	widgets->pushButtons[PB_INST_VOL_UP].callbackFuncOnDown = pbInstVolUp;
	widgets->pushButtons[PB_INST_PAN_DOWN].callbackFuncOnDown = pbInstPanDown;
	widgets->pushButtons[PB_INST_PAN_UP].callbackFuncOnDown = pbInstPanUp;
	widgets->pushButtons[PB_INST_FTUNE_DOWN].callbackFuncOnDown = pbInstFtuneDown;
	widgets->pushButtons[PB_INST_FTUNE_UP].callbackFuncOnDown = pbInstFtuneUp;
	widgets->pushButtons[PB_INST_FADEOUT_DOWN].callbackFuncOnDown = pbInstFadeoutDown;
	widgets->pushButtons[PB_INST_FADEOUT_UP].callbackFuncOnDown = pbInstFadeoutUp;
	widgets->pushButtons[PB_INST_VIBSPEED_DOWN].callbackFuncOnDown = pbInstVibSpeedDown;
	widgets->pushButtons[PB_INST_VIBSPEED_UP].callbackFuncOnDown = pbInstVibSpeedUp;
	widgets->pushButtons[PB_INST_VIBDEPTH_DOWN].callbackFuncOnDown = pbInstVibDepthDown;
	widgets->pushButtons[PB_INST_VIBDEPTH_UP].callbackFuncOnDown = pbInstVibDepthUp;
	widgets->pushButtons[PB_INST_VIBSWEEP_DOWN].callbackFuncOnDown = pbInstVibSweepDown;
	widgets->pushButtons[PB_INST_VIBSWEEP_UP].callbackFuncOnDown = pbInstVibSweepUp;

	/* Instrument editor - relative note */
	widgets->pushButtons[PB_INST_OCT_UP].callbackFuncOnDown = pbInstOctUp;
	widgets->pushButtons[PB_INST_OCT_DOWN].callbackFuncOnDown = pbInstOctDown;
	widgets->pushButtons[PB_INST_HALFTONE_UP].callbackFuncOnDown = pbInstHalftoneUp;
	widgets->pushButtons[PB_INST_HALFTONE_DOWN].callbackFuncOnDown = pbInstHalftoneDown;

	/* Instrument editor - exit */
	widgets->pushButtons[PB_INST_EXIT].callbackFuncOnUp = pbInstExit;

	/* Instrument editor extension */
	widgets->pushButtons[PB_INST_EXT_MIDI_CH_DOWN].callbackFuncOnDown = pbInstExtMidiChDown;
	widgets->pushButtons[PB_INST_EXT_MIDI_CH_UP].callbackFuncOnDown = pbInstExtMidiChUp;
	widgets->pushButtons[PB_INST_EXT_MIDI_PRG_DOWN].callbackFuncOnDown = pbInstExtMidiPrgDown;
	widgets->pushButtons[PB_INST_EXT_MIDI_PRG_UP].callbackFuncOnDown = pbInstExtMidiPrgUp;
	widgets->pushButtons[PB_INST_EXT_MIDI_BEND_DOWN].callbackFuncOnDown = pbInstExtMidiBendDown;
	widgets->pushButtons[PB_INST_EXT_MIDI_BEND_UP].callbackFuncOnDown = pbInstExtMidiBendUp;

	/* Sample editor */
	widgets->pushButtons[PB_SAMP_SCROLL_LEFT].callbackFuncOnDown = pbSampScrollLeft;
	widgets->pushButtons[PB_SAMP_SCROLL_RIGHT].callbackFuncOnDown = pbSampScrollRight;
	widgets->pushButtons[PB_SAMP_PNOTE_UP].callbackFuncOnDown = pbSampPNoteUp;
	widgets->pushButtons[PB_SAMP_PNOTE_DOWN].callbackFuncOnDown = pbSampPNoteDown;
	widgets->pushButtons[PB_SAMP_STOP].callbackFuncOnUp = pbSampStop;
	widgets->pushButtons[PB_SAMP_PWAVE].callbackFuncOnUp = pbSampPlayWave;
	widgets->pushButtons[PB_SAMP_PRANGE].callbackFuncOnUp = pbSampPlayRange;
	widgets->pushButtons[PB_SAMP_PDISPLAY].callbackFuncOnUp = pbSampPlayDisplay;
	widgets->pushButtons[PB_SAMP_SHOW_RANGE].callbackFuncOnUp = pbSampShowRange;
	widgets->pushButtons[PB_SAMP_RANGE_ALL].callbackFuncOnUp = pbSampRangeAll;
	widgets->pushButtons[PB_SAMP_CLR_RANGE].callbackFuncOnUp = pbSampClrRange;
	widgets->pushButtons[PB_SAMP_ZOOM_OUT].callbackFuncOnUp = pbSampZoomOut;
	widgets->pushButtons[PB_SAMP_SHOW_ALL].callbackFuncOnUp = pbSampShowAll;
	widgets->pushButtons[PB_SAMP_SAVE_RNG].callbackFuncOnUp = pbSampSaveRng;
	widgets->pushButtons[PB_SAMP_CUT].callbackFuncOnUp = pbSampCut;
	widgets->pushButtons[PB_SAMP_COPY].callbackFuncOnUp = pbSampCopy;
	widgets->pushButtons[PB_SAMP_PASTE].callbackFuncOnUp = pbSampPaste;
	widgets->pushButtons[PB_SAMP_CROP].callbackFuncOnUp = pbSampCrop;
	widgets->pushButtons[PB_SAMP_VOLUME].callbackFuncOnUp = pbSampVolume;
	widgets->pushButtons[PB_SAMP_EFFECTS].callbackFuncOnUp = pbSampEffects;
	widgets->pushButtons[PB_SAMP_EXIT].callbackFuncOnUp = pbSampExit;
	widgets->pushButtons[PB_SAMP_CLEAR].callbackFuncOnUp = pbSampClear;
	widgets->pushButtons[PB_SAMP_MIN].callbackFuncOnUp = pbSampMin;
	widgets->pushButtons[PB_SAMP_REPEAT_UP].callbackFuncOnDown = sampRepeatUp;
	widgets->pushButtons[PB_SAMP_REPEAT_DOWN].callbackFuncOnDown = sampRepeatDown;
	widgets->pushButtons[PB_SAMP_REPLEN_UP].callbackFuncOnDown = sampReplenUp;
	widgets->pushButtons[PB_SAMP_REPLEN_DOWN].callbackFuncOnDown = sampReplenDown;

	/* Sample editor effects */
	widgets->pushButtons[PB_SAMPFX_CYCLES_UP].callbackFuncOnDown = pbSampFxCyclesUp;
	widgets->pushButtons[PB_SAMPFX_CYCLES_DOWN].callbackFuncOnDown = pbSampFxCyclesDown;
	widgets->pushButtons[PB_SAMPFX_TRIANGLE].callbackFuncOnUp = pbSampFxTriangle;
	widgets->pushButtons[PB_SAMPFX_SAW].callbackFuncOnUp = pbSampFxSaw;
	widgets->pushButtons[PB_SAMPFX_SINE].callbackFuncOnUp = pbSampFxSine;
	widgets->pushButtons[PB_SAMPFX_SQUARE].callbackFuncOnUp = pbSampFxSquare;
	widgets->pushButtons[PB_SAMPFX_RESO_UP].callbackFuncOnDown = pbSampFxResoUp;
	widgets->pushButtons[PB_SAMPFX_RESO_DOWN].callbackFuncOnDown = pbSampFxResoDown;
	widgets->pushButtons[PB_SAMPFX_LOWPASS].callbackFuncOnUp = pbSampFxLowPass;
	widgets->pushButtons[PB_SAMPFX_HIGHPASS].callbackFuncOnUp = pbSampFxHighPass;
	widgets->pushButtons[PB_SAMPFX_SUB_BASS].callbackFuncOnUp = pbSampFxSubBass;
	widgets->pushButtons[PB_SAMPFX_SUB_TREBLE].callbackFuncOnUp = pbSampFxSubTreble;
	widgets->pushButtons[PB_SAMPFX_ADD_BASS].callbackFuncOnUp = pbSampFxAddBass;
	widgets->pushButtons[PB_SAMPFX_ADD_TREBLE].callbackFuncOnUp = pbSampFxAddTreble;
	widgets->pushButtons[PB_SAMPFX_SET_AMP].callbackFuncOnUp = pbSampFxSetAmp;
	widgets->pushButtons[PB_SAMPFX_UNDO].callbackFuncOnUp = pbSampFxUndo;
	widgets->pushButtons[PB_SAMPFX_XFADE].callbackFuncOnUp = pbSampFxXFade;
	widgets->pushButtons[PB_SAMPFX_BACK].callbackFuncOnUp = pbSampFxBack;

	/* Sample editor extension */
	widgets->pushButtons[PB_SAMP_EXT_CLEAR_COPYBUF].callbackFuncOnUp = pbSampExtClearCopyBuf;
	widgets->pushButtons[PB_SAMP_EXT_CONV].callbackFuncOnUp = pbSampExtSign;
	widgets->pushButtons[PB_SAMP_EXT_ECHO].callbackFuncOnUp = pbSampExtEcho;
	widgets->pushButtons[PB_SAMP_EXT_BACKWARDS].callbackFuncOnUp = pbSampExtBackwards;
	widgets->pushButtons[PB_SAMP_EXT_CONV_W].callbackFuncOnUp = pbSampExtByteSwap;
	widgets->pushButtons[PB_SAMP_EXT_MORPH].callbackFuncOnUp = pbSampExtFixDC;
	widgets->pushButtons[PB_SAMP_EXT_COPY_INS].callbackFuncOnUp = pbSampExtCopyIns;
	widgets->pushButtons[PB_SAMP_EXT_COPY_SMP].callbackFuncOnUp = pbSampExtCopySmp;
	widgets->pushButtons[PB_SAMP_EXT_XCHG_INS].callbackFuncOnUp = pbSampExtXchgIns;
	widgets->pushButtons[PB_SAMP_EXT_XCHG_SMP].callbackFuncOnUp = pbSampExtXchgSmp;
	widgets->pushButtons[PB_SAMP_EXT_RESAMPLE].callbackFuncOnUp = pbSampExtResample;
	widgets->pushButtons[PB_SAMP_EXT_MIX_SAMPLE].callbackFuncOnUp = pbSampExtMixSample;

	/* Disk op */
	widgets->pushButtons[PB_DISKOP_SAVE].callbackFuncOnUp = pbDiskOpSave;
	widgets->pushButtons[PB_DISKOP_MAKEDIR].callbackFuncOnUp = pbDiskOpMakeDir;
	widgets->pushButtons[PB_DISKOP_REFRESH].callbackFuncOnUp = pbDiskOpRefresh;
	widgets->pushButtons[PB_DISKOP_SET_PATH].callbackFuncOnUp = pbDiskOpSetPath;
	widgets->pushButtons[PB_DISKOP_SHOW_ALL].callbackFuncOnUp = pbDiskOpShowAll;
	widgets->pushButtons[PB_DISKOP_EXIT].callbackFuncOnUp = pbDiskOpExit;
	widgets->pushButtons[PB_DISKOP_ROOT].callbackFuncOnUp = pbDiskOpRoot;
	widgets->pushButtons[PB_DISKOP_PARENT].callbackFuncOnUp = pbDiskOpParent;
	widgets->pushButtons[PB_DISKOP_HOME].callbackFuncOnUp = pbDiskOpHome;
	widgets->pushButtons[PB_DISKOP_LIST_UP].callbackFuncOnDown = pbDiskOpListUp;
	widgets->pushButtons[PB_DISKOP_LIST_DOWN].callbackFuncOnDown = pbDiskOpListDown;

	/* Scrollbar callbacks */
	widgets->scrollBars[SB_POS_ED].callbackFunc = sbPosEd;
	widgets->scrollBars[SB_SAMPLE_LIST].callbackFunc = sbSampleList;
	widgets->scrollBars[SB_CHAN_SCROLL].callbackFunc = sbChanScroll;
	widgets->scrollBars[SB_INST_VOL].callbackFunc = sbInstVol;
	widgets->scrollBars[SB_INST_PAN].callbackFunc = sbInstPan;
	widgets->scrollBars[SB_INST_FTUNE].callbackFunc = sbInstFtune;
	widgets->scrollBars[SB_INST_FADEOUT].callbackFunc = sbInstFadeout;
	widgets->scrollBars[SB_INST_VIBSPEED].callbackFunc = sbInstVibSpeed;
	widgets->scrollBars[SB_INST_VIBDEPTH].callbackFunc = sbInstVibDepth;
	widgets->scrollBars[SB_INST_VIBSWEEP].callbackFunc = sbInstVibSweep;
	widgets->scrollBars[SB_INST_EXT_MIDI_CH].callbackFunc = sbInstExtMidiCh;
	widgets->scrollBars[SB_INST_EXT_MIDI_PRG].callbackFunc = sbInstExtMidiPrg;
	widgets->scrollBars[SB_INST_EXT_MIDI_BEND].callbackFunc = sbInstExtMidiBend;
	widgets->scrollBars[SB_SAMP_SCROLL].callbackFunc = sbSampScroll;
	widgets->scrollBars[SB_HELP_SCROLL].callbackFunc = sbHelpScroll;
	widgets->scrollBars[SB_DISKOP_LIST].callbackFunc = sbDiskOpSetPos;

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
	widgets->pushButtons[PB_CONFIG_AMP_DOWN].callbackFuncOnDown = configAmpDown;
	widgets->pushButtons[PB_CONFIG_AMP_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_AMP_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_AMP_UP].callbackFuncOnDown = configAmpUp;
	widgets->pushButtons[PB_CONFIG_AMP_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_AMP_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MASTVOL_DOWN].callbackFuncOnDown = configMasterVolDown;
	widgets->pushButtons[PB_CONFIG_MASTVOL_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MASTVOL_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MASTVOL_UP].callbackFuncOnDown = configMasterVolUp;
	widgets->pushButtons[PB_CONFIG_MASTVOL_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MASTVOL_UP].delayFrames = 4;

	/* Config layout palette arrow buttons */
	widgets->pushButtons[PB_CONFIG_PAL_R_DOWN].callbackFuncOnDown = configPalRDown;
	widgets->pushButtons[PB_CONFIG_PAL_R_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_R_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_R_UP].callbackFuncOnDown = configPalRUp;
	widgets->pushButtons[PB_CONFIG_PAL_R_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_R_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_G_DOWN].callbackFuncOnDown = configPalGDown;
	widgets->pushButtons[PB_CONFIG_PAL_G_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_G_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_G_UP].callbackFuncOnDown = configPalGUp;
	widgets->pushButtons[PB_CONFIG_PAL_G_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_G_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_B_DOWN].callbackFuncOnDown = configPalBDown;
	widgets->pushButtons[PB_CONFIG_PAL_B_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_B_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_B_UP].callbackFuncOnDown = configPalBUp;
	widgets->pushButtons[PB_CONFIG_PAL_B_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_B_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_DOWN].callbackFuncOnDown = configPalContDown;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_UP].callbackFuncOnDown = configPalContUp;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_PAL_CONT_UP].delayFrames = 4;

	/* Config miscellaneous quantize arrow buttons */
	widgets->pushButtons[PB_CONFIG_QUANTIZE_UP].callbackFuncOnDown = configQuantizeUp;
	widgets->pushButtons[PB_CONFIG_QUANTIZE_DOWN].callbackFuncOnDown = configQuantizeDown;

	/* Config MIDI input arrow buttons */
	widgets->pushButtons[PB_CONFIG_MIDICHN_DOWN].callbackFuncOnDown = configMidiChnDown;
	widgets->pushButtons[PB_CONFIG_MIDICHN_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDICHN_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MIDICHN_UP].callbackFuncOnDown = configMidiChnUp;
	widgets->pushButtons[PB_CONFIG_MIDICHN_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDICHN_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_DOWN].callbackFuncOnDown = configMidiTransDown;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_UP].callbackFuncOnDown = configMidiTransUp;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDITRANS_UP].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MIDISENS_DOWN].callbackFuncOnDown = configMidiSensDown;
	widgets->pushButtons[PB_CONFIG_MIDISENS_DOWN].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDISENS_DOWN].delayFrames = 4;
	widgets->pushButtons[PB_CONFIG_MIDISENS_UP].callbackFuncOnDown = configMidiSensUp;
	widgets->pushButtons[PB_CONFIG_MIDISENS_UP].preDelay = 1;
	widgets->pushButtons[PB_CONFIG_MIDISENS_UP].delayFrames = 4;

	/* Nibbles buttons */
	widgets->pushButtons[PB_NIBBLES_PLAY].callbackFuncOnUp = pbNibblesPlay;
	widgets->pushButtons[PB_NIBBLES_HELP].callbackFuncOnUp = pbNibblesHelp;
	widgets->pushButtons[PB_NIBBLES_HIGHS].callbackFuncOnUp = pbNibblesHighScores;
	widgets->pushButtons[PB_NIBBLES_EXIT].callbackFuncOnUp = pbNibblesExit;

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
