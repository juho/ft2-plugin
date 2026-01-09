/*
** FT2 Plugin - Main UI Controller
** Central rendering and input handling. Manages update loop, input dispatch,
** and frame rendering.
**
** Architecture:
**   ft2_plugin_ui.c     - Rendering loop, input handling, per-frame updates
**   ft2_plugin_gui.c    - Widget visibility management (hide/show groups)
**   ft2_plugin_layout.c - Drawing functions + initial widget show calls
**
** Top/bottom screen areas are independent: top overlays (config/help) don't
** affect bottom editors (pattern/sample/instrument) which always draw when visible.
*/

#include <stdlib.h>
#include <string.h>
#include "ft2_plugin_ui.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_echo_panel.h"
#include "ft2_plugin_wave_panel.h"
#include "ft2_plugin_filter_panel.h"
#include "ft2_plugin_smpfx.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_about.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_layout.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_callbacks.h"
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_instrsw.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_help.h"
#include "ft2_plugin_trim.h"
#include "ft2_plugin_nibbles.h"
#include "ft2_instance.h"

/* ------------------------------------------------------------------------- */
/*                       LIFECYCLE                                           */
/* ------------------------------------------------------------------------- */

ft2_ui_t* ft2_ui_create(void)
{
	ft2_ui_t* ui = (ft2_ui_t*)calloc(1, sizeof(ft2_ui_t));
	if (ui) ft2_ui_init(ui);
	return ui;
}

void ft2_ui_destroy(ft2_ui_t* ui)
{
	if (ui) { ft2_ui_shutdown(ui); free(ui); }
}

void ft2_ui_init(ft2_ui_t *ui)
{
	if (!ui) return;
	memset(ui, 0, sizeof(ft2_ui_t));

	ft2_video_init(&ui->video);
	ui->bmpLoaded = ft2_bmp_load(&ui->bmp);
	ft2_input_init(&ui->input);

	ft2_pattern_ed_init(&ui->patternEditor, &ui->video);
	ft2_sample_ed_init(&ui->sampleEditor, &ui->video);
	ft2_instr_ed_init(&ui->instrEditor);
	ft2_scopes_init(&ui->scopes);
	ft2_widgets_init(&ui->widgets);
	ft2_about_init();
	ft2_textbox_init();
	ft2_dialog_init(&ui->dialog);
	setInitialTrimFlags(NULL);

	ui->currentScreen = FT2_SCREEN_PATTERN;
	ui->currInstr = 1;
	ui->currSample = 0;
	ui->currOctave = 4;
	ui->needsFullRedraw = true;
}

void ft2_ui_shutdown(ft2_ui_t *ui)
{
	if (!ui) return;
	if (ui->bmpLoaded) { ft2_bmp_free(&ui->bmp); ui->bmpLoaded = false; }
	ft2_video_free(&ui->video);
	ft2_textbox_free();
}

void ft2_ui_set_screen(ft2_ui_t *ui, ft2_ui_screen screen)
{
	if (!ui || screen >= FT2_NUM_SCREENS) return;
	ui->currentScreen = screen;
	ui->needsFullRedraw = true;
}

/* ------------------------------------------------------------------------- */
/*                         DRAWING                                           */
/* ------------------------------------------------------------------------- */

void ft2_ui_draw(ft2_ui_t *ui, void *inst)
{
	if (!ui) return;

	ft2_video_t *video = &ui->video;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;

	/* Apply config palette on first draw */
	if (ft2inst && !ui->paletteInitialized)
	{
		setPal16(video, pluginPalTable[ft2inst->config.palettePreset], false);
		ui->paletteInitialized = true;
		ui->needsFullRedraw = true;
	}

	if (ft2inst && ft2inst->uiState.needsFullRedraw)
	{
		ft2inst->uiState.needsFullRedraw = false;
		ui->needsFullRedraw = true;
	}

	if (ui->needsFullRedraw)
		clearRect(video, 0, 0, SCREEN_W, SCREEN_H);

	if (ui->needsFullRedraw && ft2inst)
		drawGUILayout(ft2inst, video, bmp);

	/* Top screen: scopes or overlay panels that replace them */
	if (ft2inst)
	{
		if (ft2inst->uiState.sampleEditorExtShown)
			drawSampleEditorExt(ft2inst, video, bmp);
		else if (ft2inst->uiState.transposeShown)
			drawTranspose(ft2inst, video, bmp);
		else if (ft2inst->uiState.advEditShown)
			drawAdvEdit(ft2inst, video, bmp);
		else if (ft2inst->uiState.trimScreenShown)
			drawTrimScreen(ft2inst, video, bmp);
		else if (ft2inst->uiState.instEditorExtShown)
			drawInstEditorExt(ft2inst);
		else if (ft2inst->uiState.scopesShown)
		{
			ui->scopes.ptnChnNumbers = ft2inst->uiState.ptnChnNumbers;
			if (ui->needsFullRedraw || ui->scopes.needsFrameworkRedraw)
			{
				ui->scopes.needsFrameworkRedraw = false;
				ft2_scopes_draw_framework(&ui->scopes, video, bmp);
			}
			ft2_scopes_draw(&ui->scopes, video, bmp);
		}
	}

	/* Bottom screen: editors always draw when visible (independent of top overlays) */
	if (ft2inst)
	{
		if (ft2inst->uiState.patternEditorShown)
			ft2_pattern_ed_draw(&ui->patternEditor, bmp, ft2inst);
		else if (ft2inst->uiState.sampleEditorShown)
			ft2_sample_ed_draw(ft2inst);
		else if (ft2inst->uiState.instEditorShown)
			ft2_instr_ed_draw(ft2inst);

		ft2_widgets_draw(&ui->widgets, video, bmp);
	}

	/* Modal panels and dialogs on top */
	if (ft2_modal_panel_is_any_active())
		ft2_modal_panel_draw_active(video, bmp);
	else if (ft2_dialog_is_active(&ui->dialog))
		ft2_dialog_draw(&ui->dialog, video, bmp);

	ui->needsFullRedraw = false;
	ft2_video_swap_buffers(video);
}

/* ------------------------------------------------------------------------- */
/*                       PER-FRAME UPDATES                                   */
/* ------------------------------------------------------------------------- */

/* Handles incremental redraws between full redraws */
static void handleRedrawing(ft2_ui_t *ui, ft2_instance_t *inst)
{
	if (!ui || !inst) return;

	ft2_video_t *video = &ui->video;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;

	static uint32_t textCursorCounter = 0;
	textCursorCounter = ft2_textbox_is_editing() ? textCursorCounter + 1 : 0;

	if (!inst->uiState.configScreenShown && !inst->uiState.helpScreenShown)
	{
		if (inst->uiState.aboutScreenShown)
		{
			ft2_about_render_frame(video, bmp);
		}
		else if (inst->uiState.nibblesShown)
		{
			/* Process deferred nibbles actions */
			if (inst->uiState.nibblesPlayRequested)      { inst->uiState.nibblesPlayRequested = false; ft2_nibbles_play(inst, video, bmp); }
			if (inst->uiState.nibblesHelpRequested)      { inst->uiState.nibblesHelpRequested = false; ft2_nibbles_show_help(inst, video, bmp); }
			if (inst->uiState.nibblesHighScoreRequested) { inst->uiState.nibblesHighScoreRequested = false; ft2_nibbles_show_highscores(inst, video, bmp); }
			if (inst->uiState.nibblesExitRequested)      { inst->uiState.nibblesExitRequested = false; ft2_nibbles_exit(inst, video, bmp); }
			if (inst->uiState.nibblesRedrawRequested)    { inst->uiState.nibblesRedrawRequested = false; if (inst->nibbles.playing) ft2_nibbles_redraw(inst, video, bmp); }
			ft2_nibbles_tick(inst, video, bmp);
		}
		else
		{
			/* Position/song info updates */
			if (inst->uiState.updatePosSections)
			{
				inst->uiState.updatePosSections = false;
				if (!inst->uiState.diskOpShown)
				{
					drawSongLoopStart(inst, video, bmp);
					drawSongLength(inst, video, bmp);
					drawPosEdNums(inst, video, bmp);
					drawEditPattern(inst, video, bmp);
					drawPatternLength(inst, video, bmp);
					drawSongBPM(inst, video, bmp);
					drawSongSpeed(inst, video, bmp);
					drawIDAdd(inst, video, bmp);
					drawGlobalVol(inst, video, bmp);
					if (!inst->uiState.extendedPatternEditor) drawSongName(inst, video, bmp);
					setScrollBarPos(inst, &ui->widgets, video, SB_POS_ED, inst->replayer.song.songPos, false);
				}
			}

			if (inst->uiState.updatePosEdScrollBar)
			{
				inst->uiState.updatePosEdScrollBar = false;
				setScrollBarPos(inst, &ui->widgets, video, SB_POS_ED, inst->replayer.song.songPos, false);
				setScrollBarEnd(inst, &ui->widgets, video, SB_POS_ED, (inst->replayer.song.songLength - 1) + 5);
			}

			if (!inst->uiState.diskOpShown)
			{
				drawPlaybackTime(inst, video, bmp);
				drawGlobalVol(inst, video, bmp);
			}

			/* Instrument switcher */
			if (inst->uiState.updateInstrSwitcher)
			{
				inst->uiState.updateInstrSwitcher = false;
				ft2_textbox_update_pointers(inst);
				if (inst->uiState.instrSwitcherShown) updateInstrumentSwitcher(inst, video, bmp);
			}

			/* Textbox cursor blink */
			if (ft2_textbox_is_editing())
			{
				int16_t activeBox = ft2_textbox_get_active();
				if (activeBox >= 0)
					ft2_textbox_draw_with_cursor(video, bmp, (uint16_t)activeBox, (textCursorCounter & 0x10) == 0, inst);
			}
			else
			{
				int16_t needsRedraw = ft2_textbox_get_needs_redraw();
				if (needsRedraw >= 0)
				{
					ft2_textbox_draw_with_cursor(video, bmp, (uint16_t)needsRedraw, false, inst);
					if ((needsRedraw >= TB_INST1 && needsRedraw <= TB_INST8) || (needsRedraw >= TB_SAMP1 && needsRedraw <= TB_SAMP5))
						inst->uiState.updateInstrSwitcher = true;
					else if (needsRedraw == TB_SONG_NAME && !inst->uiState.extendedPatternEditor)
						drawSongName(inst, video, bmp);
				}
			}

			/* Bank swap button toggle */
			if (inst->uiState.instrBankSwapPending)
			{
				inst->uiState.instrBankSwapPending = false;
				if (inst->uiState.instrSwitcherShown)
					for (uint16_t i = 0; i < 8; i++)
					{
						hidePushButton(&ui->widgets, PB_RANGE1 + i + (!inst->editor.instrBankSwapped * 8));
						showPushButton(&ui->widgets, video, bmp, PB_RANGE1 + i + (inst->editor.instrBankSwapped * 8));
					}
			}
		}
	}

	/* Channel scrollbar */
	if (inst->uiState.updateChanScrollPos)
	{
		inst->uiState.updateChanScrollPos = false;
		if (inst->uiState.pattChanScrollShown)
			setScrollBarPos(inst, &ui->widgets, video, SB_CHAN_SCROLL, inst->uiState.channelOffset, false);
	}

	/* Editor redraws */
	if (inst->uiState.updatePatternEditor)
	{
		inst->uiState.updatePatternEditor = false;
		if (inst->uiState.patternEditorShown) ft2_pattern_ed_draw(&ui->patternEditor, bmp, inst);
	}
	if (inst->uiState.sampleEditorShown && inst->uiState.updateSampleEditor)
	{
		inst->uiState.updateSampleEditor = false;
		ft2_sample_ed_set_sample(inst, inst->editor.curInstr, inst->editor.curSmp);
		ft2_sample_ed_draw(inst);
	}
	if (inst->uiState.instEditorShown && inst->uiState.updateInstEditor)
	{
		inst->uiState.updateInstEditor = false;
		ft2_instr_ed_draw(inst);
	}

	/* Text cursor blink counter */
	if (inst->editor.editTextFlag)
	{
		inst->editor.textCursorBlinkCounter++;
		if (inst->editor.textCursorBlinkCounter >= 16) inst->editor.textCursorBlinkCounter = 0;
	}

	/* Play mode indicator */
	if (bmp && !inst->uiState.diskOpShown && !inst->uiState.aboutScreenShown &&
	    !inst->uiState.configScreenShown && !inst->uiState.helpScreenShown && !inst->uiState.nibblesShown)
	{
		const char *str = NULL;
		if (inst->replayer.playMode == FT2_PLAYMODE_PATT)    str = "> Play ptn. <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_EDIT)    str = "> Editing <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_RECSONG) str = "> Rec. sng. <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_RECPATT) str = "> Rec. ptn. <";

		uint16_t areaWidth = inst->uiState.extendedPatternEditor ? 443 : 102;
		uint16_t x = 101, y = inst->uiState.extendedPatternEditor ? 56 : 80;
		uint16_t clrX = x + ((areaWidth - 76) / 2);
		fillRect(video, clrX, y, 76, 11, PAL_DESKTOP);
		if (str)
		{
			uint16_t textW = textWidth(str);
			textOut(video, bmp, x + ((areaWidth - textW) / 2), y, PAL_FORGRND, str);
		}
	}
}

void ft2_ui_update(ft2_ui_t *ui, void *inst)
{
	if (!ui) return;

	ft2_input_update(&ui->input);
	ft2_scopes_update(&ui->scopes, inst);

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
	ft2_widgets_handle_held_down(&ui->widgets, ft2inst, &ui->video, bmp);

	if (ft2inst) handleRedrawing(ui, ft2inst);
}

uint32_t *ft2_ui_get_framebuffer(ft2_ui_t *ui)
{
	return ui ? ui->video.displayBuffer : NULL;
}

/* ------------------------------------------------------------------------- */
/*                        MOUSE INPUT                                        */
/* ------------------------------------------------------------------------- */

void ft2_ui_mouse_press(ft2_ui_t *ui, void *inst, int x, int y, bool leftButton, bool rightButton)
{
	if (!ui) return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	int button = leftButton ? MOUSE_BUTTON_LEFT : (rightButton ? MOUSE_BUTTON_RIGHT : 0);

	/* Modal panels */
	if (ft2_modal_panel_is_any_active())
	{
		if (ft2_modal_panel_get_active() == MODAL_PANEL_ECHO)
			ft2_echo_panel_mouse_down(x, y, button);
		ft2_widgets_mouse_down(&ui->widgets, ft2inst, &ui->video, x, y, true);
		return;
	}

	if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_mouse_down(&ui->dialog, x, y, button);
		return;
	}

	ft2_input_mouse_down(&ui->input, x, y, button);

	/* Instrument switcher first (FT2 behavior: left=select, right=edit name) */
	if (ft2inst) testInstrSwitcherMouseDown(ft2inst, x, y);

	/* Textbox click test */
	if (ft2inst) ft2_textbox_update_pointers(ft2inst);
	int16_t textBoxHit = ft2_textbox_test_mouse_down(x, y, rightButton);
	if (textBoxHit >= 0)
	{
		if (ft2inst) ft2inst->uiState.updateInstrSwitcher = true;
		return;
	}

	/* Widgets */
	if (rightButton)
		ft2_widgets_mouse_down_right(&ui->widgets, x, y, ft2inst);
	else
		ft2_widgets_mouse_down(&ui->widgets, ft2inst, &ui->video, x, y, false);

	if (getLastUsedWidget() != -1) return;

	/* Disk op file list */
	if (ft2inst && ft2inst->uiState.diskOpShown && diskOpTestMouseDown(ft2inst, x, y))
		return;

	/* Scopes (mute/rec/solo) */
	if (ft2inst && ft2inst->uiState.scopesShown)
	{
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		if (ft2_scopes_mouse_down(&ui->scopes, &ui->video, bmp, x, y, leftButton, rightButton))
		{
			for (int i = 0; i < ft2inst->replayer.song.numChannels; i++)
			{
				ft2inst->replayer.channel[i].channelOff = ui->scopes.channelMuted[i];
				if (ui->scopes.channelMuted[i])
				{
					ft2inst->replayer.channel[i].realVol = 0;
					ft2inst->replayer.channel[i].outVol = 0;
					ft2inst->replayer.channel[i].fFinalVol = 0.0f;
					ft2_scope_stop(&ui->scopes, i);
				}
			}
			return;
		}
	}

	/* Pattern editor - marking */
	if (ft2inst && ft2inst->uiState.patternEditorShown)
	{
		int32_t pattY1 = ft2inst->uiState.extendedPatternEditor ? 71 : 176;
		int32_t pattY2 = ft2inst->uiState.pattChanScrollShown ? 382 : 396;
		if (y >= pattY1 && y <= pattY2 && x >= 29 && x <= 602)
		{
			handlePatternDataMouseDown(ft2inst, x, y, false, rightButton);
			ui->input.pattMarkDragging = true;
			return;
		}
	}

	/* Sample editor */
	if (ft2inst && ft2inst->uiState.sampleEditorShown && y >= 174 && y <= 328)
	{
		ft2_sample_ed_mouse_click(ft2inst, x, y, button);
		ui->input.mouseDragging = true;
	}

	/* Instrument editor */
	if (ft2inst && ft2inst->uiState.instEditorShown && y >= 173)
	{
		ft2_instr_ed_mouse_click(ft2inst, x, y, button);
		ui->input.mouseDragging = true;
	}
}

void ft2_ui_mouse_release(ft2_ui_t *ui, void *inst, int x, int y, int button)
{
	if (!ui) return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;

	if (ft2_modal_panel_is_any_active())
	{
		ft2_widgets_mouse_up(&ui->widgets, x, y, ft2inst, &ui->video, bmp);
		return;
	}

	if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_mouse_up(&ui->dialog, x, y, button);
		return;
	}

	ft2_input_mouse_up(&ui->input, x, y, button);
	ui->input.mouseDragging = false;
	ui->input.pattMarkDragging = false;

	if (button == MOUSE_BUTTON_RIGHT)
		ft2_widgets_mouse_up_right(&ui->widgets, x, y, ft2inst, &ui->video, bmp);
	else
		ft2_widgets_mouse_up(&ui->widgets, x, y, ft2inst, &ui->video, bmp);

	if (ft2inst && ft2inst->uiState.sampleEditorShown) ft2_sample_ed_mouse_up(ft2inst);
	if (ft2inst && ft2inst->uiState.instEditorShown) ft2_instr_ed_mouse_up(ft2inst);
}

void ft2_ui_mouse_move(ft2_ui_t *ui, void *inst, int x, int y)
{
	if (!ui) return;

	ft2_input_mouse_move(&ui->input, x, y);
	ft2_widgets_mouse_move(&ui->widgets, x, y);

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	if (!ft2inst) return;

	if (ft2inst->uiState.patternEditorShown && ui->input.pattMarkDragging)
		handlePatternDataMouseDown(ft2inst, x, y, true, false);

	if (ft2inst->uiState.sampleEditorShown && ui->input.mouseDragging)
		ft2_sample_ed_mouse_drag(ft2inst, x, y, (ui->input.modifiers & FT2_MOD_SHIFT) != 0);

	if (ft2inst->uiState.instEditorShown && ui->input.mouseDragging)
		ft2_instr_ed_mouse_drag(ft2inst, x, y);
}

void ft2_ui_mouse_wheel(ft2_ui_t *ui, void *inst, int x, int y, int delta)
{
	if (!ui) return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	ft2_input_mouse_wheel(&ui->input, delta);
	if (!ft2inst) return;

	bool up = (delta > 0);

	/* Top screen (y < 173) */
	if (y < 173)
	{
		/* Help: 2x scroll */
		if (ft2inst->uiState.helpScreenShown)
		{
			ft2_video_t *video = &ui->video;
			const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
			if (up) { helpScrollUp(ft2inst, video, bmp); helpScrollUp(ft2inst, video, bmp); }
			else    { helpScrollDown(ft2inst, video, bmp); helpScrollDown(ft2inst, video, bmp); }
			return;
		}

		/* Disk op: 3x scroll in file list */
		if (ft2inst->uiState.diskOpShown && x <= 355)
		{
			for (int i = 0; i < 3; i++) up ? pbDiskOpListUp(ft2inst) : pbDiskOpListDown(ft2inst);
			return;
		}

		if (ft2inst->uiState.aboutScreenShown || ft2inst->uiState.configScreenShown ||
		    ft2inst->uiState.nibblesShown || ft2inst->uiState.diskOpShown)
			return;

		/* Position editor */
		if (x <= 111 && y <= 76)
		{
			bool changed = false;
			if (up && ft2inst->replayer.song.songPos > 0) { ft2inst->replayer.song.songPos--; changed = true; }
			else if (!up && ft2inst->replayer.song.songPos < ft2inst->replayer.song.songLength - 1) { ft2inst->replayer.song.songPos++; changed = true; }

			if (changed)
			{
				ft2inst->replayer.song.pattNum = ft2inst->replayer.song.orders[ft2inst->replayer.song.songPos];
				ft2inst->replayer.song.currNumRows = ft2inst->replayer.patternNumRows[ft2inst->replayer.song.pattNum];
				ft2inst->replayer.song.row = 0;
				if (!ft2inst->replayer.songPlaying)
				{
					ft2inst->editor.row = 0;
					ft2inst->editor.editPattern = (uint8_t)ft2inst->replayer.song.pattNum;
				}
				ft2inst->uiState.updatePosSections = true;
				ft2inst->uiState.updatePosEdScrollBar = true;
				ft2inst->uiState.updatePatternEditor = true;
			}
		}
		/* Instrument/sample area */
		else if (x >= 421)
		{
			if (y <= 93)
			{
				if (up && ft2inst->editor.curInstr > 0) ft2inst->editor.curInstr--;
				else if (!up && ft2inst->editor.curInstr < 127) ft2inst->editor.curInstr++;
			}
			else
			{
				if (up && ft2inst->editor.curSmp > 0) ft2inst->editor.curSmp--;
				else if (!up && ft2inst->editor.curSmp < 15) ft2inst->editor.curSmp++;
			}
		}
	}
	else
	{
		/* Bottom screen */
		if (ft2inst->uiState.patternEditorShown)
		{
			int32_t numRows = ft2inst->replayer.patternNumRows[ft2inst->editor.editPattern];
			if (up && ft2inst->editor.row > 0) ft2inst->editor.row--;
			else if (!up && ft2inst->editor.row < numRows - 1) ft2inst->editor.row++;
			ft2inst->uiState.updatePatternEditor = true;
		}
		else if (ft2inst->uiState.sampleEditorShown && y >= 174 && y <= 328)
		{
			up ? ft2_sample_ed_zoom_in(ft2inst, x) : ft2_sample_ed_zoom_out(ft2inst, x);
			ft2inst->uiState.updateSampleEditor = true;
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                       KEYBOARD INPUT                                      */
/* ------------------------------------------------------------------------- */

void ft2_ui_key_press(ft2_ui_t *ui, void *inst, int key, int modifiers)
{
	if (!ui) return;

	/* Modal panels with text input */
	if (ft2_wave_panel_is_active()) { ft2_wave_panel_key_down(key); return; }
	if (ft2_filter_panel_is_active()) { ft2_filter_panel_key_down(key); return; }
	if (ft2_modal_panel_is_any_active()) return;
	if (ft2_dialog_is_active(&ui->dialog)) { ft2_dialog_key_down(&ui->dialog, key); return; }

	if (ft2_textbox_is_editing())
	{
		ft2_textbox_handle_key(key, modifiers);
		return;
	}

	ft2_input_key_down((ft2_instance_t *)inst, &ui->input, key, modifiers);
	ft2_widgets_key_press(&ui->widgets, key);
}

void ft2_ui_text_input(ft2_ui_t *ui, char c)
{
	if (!ui) return;

	if (ft2_wave_panel_is_active()) { ft2_wave_panel_char_input(c); return; }
	if (ft2_filter_panel_is_active()) { ft2_filter_panel_char_input(c); return; }
	if (ft2_dialog_is_active(&ui->dialog)) { ft2_dialog_char_input(&ui->dialog, c); return; }
	if (ft2_textbox_is_editing()) ft2_textbox_input_char(c);
}

void ft2_ui_key_release(ft2_ui_t *ui, void *inst, int key, int modifiers)
{
	if (!ui) return;
	ft2_input_key_up((ft2_instance_t *)inst, &ui->input, key, modifiers);
}

void ft2_ui_key_state_changed(ft2_ui_t *ui, bool isKeyDown)
{
	(void)ui; (void)isKeyDown;
}
