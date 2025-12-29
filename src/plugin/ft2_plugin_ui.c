/**
 * @file ft2_plugin_ui.c
 * @brief Main UI controller for the FT2 plugin.
 *
 * This file is the central rendering and input handling controller.
 * It manages the main update loop, input dispatch, and frame rendering.
 *
 * SEPARATION OF CONCERNS:
 * - ft2_plugin_ui.c:  Main UI controller (this file) - rendering loop, input, updates
 * - ft2_plugin_gui.c: Widget visibility state management (hide/show widget groups)
 * - ft2_plugin_layout.c: Drawing functions that also call show* on initial display
 *
 * KEY PRINCIPLE: Top and bottom screen areas are independent.
 * - Top overlays (config/help) only affect top-screen drawing (scopes, pos editor)
 * - Bottom editors (pattern/sample/instrument) ALWAYS draw when visible
 */

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

/* Screen names - currently unused but retained for future use */
#if 0
static const char *screenNames[FT2_NUM_SCREENS] = {
	"Pattern", "Sample", "Instr", "Config", "Disk Op", "About"
};
#endif

void ft2_ui_init(ft2_ui_t *ui)
{
	if (ui == NULL)
		return;

	memset(ui, 0, sizeof(ft2_ui_t));

	/* Initialize video */
	ft2_video_init(&ui->video);

	/* Load bitmap assets */
	ui->bmpLoaded = ft2_bmp_load(&ui->bmp);

	/* Initialize input */
	ft2_input_init(&ui->input);

	/* Initialize components */
	ft2_pattern_ed_init(&ui->patternEditor, &ui->video);
	ft2_sample_ed_init(&ui->sampleEditor, &ui->video);
	ft2_instr_ed_init(&ui->instrEditor, &ui->video);
	ft2_scopes_init(&ui->scopes);
	ft2_widgets_init(&ui->widgets);
	ft2_about_init();
	ft2_textbox_init();
	ft2_dialog_init(&ui->dialog);
	setInitialTrimFlags(NULL); /* inst not available yet; only sets boolean flags */

	/* Default state */
	ui->currentScreen = FT2_SCREEN_PATTERN;
	ui->currInstr = 1;
	ui->currSample = 0;
	ui->currOctave = 4;
	ui->needsFullRedraw = true;
}

void ft2_ui_shutdown(ft2_ui_t *ui)
{
	if (ui == NULL)
		return;

	if (ui->bmpLoaded)
	{
		ft2_bmp_free(&ui->bmp);
		ui->bmpLoaded = false;
	}

	ft2_video_free(&ui->video);
	ft2_textbox_free();
}

void ft2_ui_set_screen(ft2_ui_t *ui, ft2_ui_screen screen)
{
	if (ui == NULL || screen >= FT2_NUM_SCREENS)
		return;

	ui->currentScreen = screen;
	ui->needsFullRedraw = true;
}

/* Old UI drawing functions removed - now using ft2_plugin_layout.c */

void ft2_ui_draw(ft2_ui_t *ui, void *inst)
{
	if (ui == NULL)
		return;

	ft2_video_t *video = &ui->video;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;

	/* Apply config palette on first draw with valid instance */
	if (ft2inst != NULL && !ui->paletteInitialized)
	{
		setPal16(video, pluginPalTable[ft2inst->config.palettePreset], false);
		ui->paletteInitialized = true;
		ui->needsFullRedraw = true;
	}

	/* Check if instance requests a full redraw */
	if (ft2inst != NULL && ft2inst->uiState.needsFullRedraw)
	{
		ft2inst->uiState.needsFullRedraw = false;
		ui->needsFullRedraw = true;
	}

	/* Clear framebuffer if full redraw needed */
	if (ui->needsFullRedraw)
	{
		clearRect(video, 0, 0, SCREEN_W, SCREEN_H);
	}

	/* Set instance pointers on sub-components */
	if (ft2inst != NULL)
	{
		ft2_pattern_ed_set_instance(&ui->patternEditor, ft2inst);
		ft2_sample_ed_set_instance(&ui->sampleEditor, ft2inst);
		ft2_sample_ed_set_current(&ui->sampleEditor);
		ft2_instr_ed_set_instance(&ui->instrEditor, ft2inst);
		ft2_instr_ed_set_current(&ui->instrEditor);
	}

	/* Draw the exact FT2 layout */
	if (ui->needsFullRedraw && ft2inst != NULL)
	{
		drawGUILayout(ft2inst, video, bmp);
	}

	/* Draw scopes or overlay panels that replace scopes (sample editor ext, transpose, etc.) */
	if (ft2inst != NULL)
	{
		if (ft2inst->uiState.sampleEditorExtShown)
		{
			/* Draw sample editor extended panel (replaces scopes) */
			drawSampleEditorExt(ft2inst, video, bmp);
		}
		else if (ft2inst->uiState.transposeShown)
		{
			/* Draw transpose dialog (replaces scopes) */
			drawTranspose(ft2inst, video, bmp);
		}
		else if (ft2inst->uiState.advEditShown)
		{
			/* Draw advanced edit dialog (replaces scopes) */
			drawAdvEdit(ft2inst, video, bmp);
		}
		else if (ft2inst->uiState.trimScreenShown)
		{
			/* Draw trim screen (replaces scopes) */
			drawTrimScreen(ft2inst, video, bmp);
		}
		else if (ft2inst->uiState.instEditorExtShown)
		{
			/* Draw instrument editor extended panel (replaces scopes) */
			drawInstEditorExt(ft2inst, video, bmp);
		}
		else if (ft2inst->uiState.scopesShown)
	{
		/* Sync scope settings from uiState */
		ui->scopes.ptnChnNumbers = ft2inst->uiState.ptnChnNumbers;

		/* Draw scope framework on full redraw or when channel count changed */
		if (ui->needsFullRedraw || ui->scopes.needsFrameworkRedraw)
		{
			ui->scopes.needsFrameworkRedraw = false;
			ft2_scopes_draw_framework(&ui->scopes, video, bmp);
		}
		ft2_scopes_draw(&ui->scopes, video, bmp);
		}
	}

	/* Draw bottom screen based on current state.
	 * Bottom screen editors ALWAYS draw when visible - independent of top screen overlays.
	 * About/config/help only affect the TOP portion (scopes area, position editor).
	 * This matches the standalone behavior where pattern editor is visible behind about screen. */
	if (ft2inst != NULL)
	{
			if (ft2inst->uiState.patternEditorShown)
			{
				ft2_pattern_ed_draw(&ui->patternEditor, bmp, ft2inst);
			}
			else if (ft2inst->uiState.sampleEditorShown)
			{
				ft2_sample_ed_draw(&ui->sampleEditor, bmp);
			}
			else if (ft2inst->uiState.instEditorShown)
			{
				ft2_instr_ed_draw(&ui->instrEditor, bmp);
			}

			/* Always draw visible widgets - widget visibility handles what gets drawn */
			ft2_widgets_draw(&ui->widgets, video, bmp);
	}

	/* Draw modal panels and dialogs on top of everything if active */
	if (ft2_modal_panel_is_any_active())
	{
		ft2_modal_panel_draw_active(video, bmp);
	}
	else if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_draw(&ui->dialog, video, bmp);
	}

	ui->needsFullRedraw = false;
}

static void handleRedrawing(ft2_ui_t *ui, ft2_instance_t *inst)
{
	if (ui == NULL || inst == NULL)
		return;

	ft2_video_t *video = &ui->video;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;

	/* Text cursor blink counter - always increment */
	static uint32_t textCursorCounter = 0;
	if (ft2_textbox_is_editing())
		textCursorCounter++;
	else
		textCursorCounter = 0;

	if (!inst->uiState.configScreenShown && !inst->uiState.helpScreenShown)
	{
		if (inst->uiState.aboutScreenShown)
		{
			/* Render about screen frame (starfield + waving logo) */
			ft2_about_render_frame(video, bmp);
		}
		else if (inst->uiState.nibblesShown)
		{
			/* Handle deferred nibbles actions */
			if (inst->uiState.nibblesPlayRequested)
			{
				inst->uiState.nibblesPlayRequested = false;
				ft2_nibbles_play(inst, video, bmp);
			}
			if (inst->uiState.nibblesHelpRequested)
			{
				inst->uiState.nibblesHelpRequested = false;
				ft2_nibbles_show_help(inst, video, bmp);
			}
			if (inst->uiState.nibblesHighScoreRequested)
			{
				inst->uiState.nibblesHighScoreRequested = false;
				ft2_nibbles_show_highscores(inst, video, bmp);
			}
			if (inst->uiState.nibblesExitRequested)
			{
				inst->uiState.nibblesExitRequested = false;
				/* Let ft2_nibbles_exit handle confirmation dialog when playing */
				ft2_nibbles_exit(inst, video, bmp);
			}
			if (inst->uiState.nibblesRedrawRequested)
			{
				inst->uiState.nibblesRedrawRequested = false;
				/* Redraw game screen when grid setting changes */
				if (inst->nibbles.playing)
					ft2_nibbles_redraw(inst, video, bmp);
			}

			/* Run game tick if playing */
			ft2_nibbles_tick(inst, video, bmp);
		}
		else
		{
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
					
					/* Song name only exists in normal mode (not extended) */
					if (!inst->uiState.extendedPatternEditor)
				drawSongName(inst, video, bmp);

					/* Always update scrollbar position (matches standalone behavior) */
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

			/* Update instrument switcher if needed */
			if (inst->uiState.updateInstrSwitcher)
			{
				inst->uiState.updateInstrSwitcher = false;
				ft2_textbox_update_pointers(inst);  /* Update textbox pointers for new bank */
				if (inst->uiState.instrSwitcherShown)
					updateInstrumentSwitcher(inst, video, bmp);
			}

			/* Handle text editing cursor blink - AFTER instrument switcher draws highlights */
			if (ft2_textbox_is_editing())
			{
				int16_t activeBox = ft2_textbox_get_active();
				if (activeBox >= 0)
				{
					bool showCursor = (textCursorCounter & 0x10) == 0;
					ft2_textbox_draw_with_cursor(video, bmp, (uint16_t)activeBox, showCursor, inst);
				}
			}
			else
			{
				/* Check if a textbox needs redraw after editing exited (to clear cursor) */
				int16_t needsRedraw = ft2_textbox_get_needs_redraw();
				if (needsRedraw >= 0)
				{
					/* Redraw without cursor to clear it */
					ft2_textbox_draw_with_cursor(video, bmp, (uint16_t)needsRedraw, false, inst);
					
					/* Also trigger appropriate area redraw */
					if (needsRedraw >= TB_INST1 && needsRedraw <= TB_INST8)
						inst->uiState.updateInstrSwitcher = true;
					else if (needsRedraw >= TB_SAMP1 && needsRedraw <= TB_SAMP5)
						inst->uiState.updateInstrSwitcher = true;
					else if (needsRedraw == TB_SONG_NAME && !inst->uiState.extendedPatternEditor)
						drawSongName(inst, video, bmp);
				}
			}

			/* Handle bank swap button visibility */
			if (inst->uiState.instrBankSwapPending)
			{
				inst->uiState.instrBankSwapPending = false;
				if (inst->uiState.instrSwitcherShown)
				{
			/* Hide the old bank buttons, show the new bank buttons */
				for (uint16_t i = 0; i < 8; i++)
				{
					hidePushButton(&ui->widgets, PB_RANGE1 + i + (!inst->editor.instrBankSwapped * 8));
					showPushButton(&ui->widgets, video, bmp, PB_RANGE1 + i + (inst->editor.instrBankSwapped * 8));
				}
				}
			}
		}
	}

	/* Update channel scrollbar position if needed */
	if (inst->uiState.updateChanScrollPos)
	{
		inst->uiState.updateChanScrollPos = false;
		if (inst->uiState.pattChanScrollShown)
			setScrollBarPos(inst, &ui->widgets, video, SB_CHAN_SCROLL, inst->uiState.channelOffset, false);
	}

	/* Update pattern editor if needed */
	if (inst->uiState.updatePatternEditor)
	{
		inst->uiState.updatePatternEditor = false;
		if (inst->uiState.patternEditorShown)
			ft2_pattern_ed_draw(&ui->patternEditor, bmp, inst);
	}
	
	/* Update sample editor if needed */
	if (inst->uiState.sampleEditorShown)
	{
		if (inst->uiState.updateSampleEditor)
		{
			inst->uiState.updateSampleEditor = false;
			/* Sync sample editor with current instrument/sample */
			ft2_sample_ed_set_instance(&ui->sampleEditor, inst);
			ft2_sample_ed_set_sample(&ui->sampleEditor, inst->editor.curInstr, inst->editor.curSmp);
			ft2_sample_ed_draw(&ui->sampleEditor, bmp);
		}
	}
	
	/* Update instrument editor if needed */
	if (inst->uiState.instEditorShown)
	{
		if (inst->uiState.updateInstEditor)
		{
			inst->uiState.updateInstEditor = false;
			ft2_instr_ed_draw(&ui->instrEditor, bmp);
		}
	}
	
	/* Text cursor blinking */
	if (inst->editor.editTextFlag)
	{
		inst->editor.textCursorBlinkCounter++;
		if (inst->editor.textCursorBlinkCounter >= 16)
			inst->editor.textCursorBlinkCounter = 0;
	}
	
	/* Draw mode text (Edit/Play) - matches original FT2 */
	if (bmp != NULL && !inst->uiState.diskOpShown && !inst->uiState.aboutScreenShown &&
	    !inst->uiState.configScreenShown && !inst->uiState.helpScreenShown)
	{
		const char *str = NULL;
		
		if (inst->replayer.playMode == FT2_PLAYMODE_PATT)
			str = "> Play ptn. <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_EDIT)
			str = "> Editing <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_RECSONG)
			str = "> Rec. sng. <";
		else if (inst->replayer.playMode == FT2_PLAYMODE_RECPATT)
			str = "> Rec. ptn. <";

		uint16_t areaWidth = 102;
		uint16_t maxStrWidth = 76;
		uint16_t x = 101;
		uint16_t y = inst->uiState.extendedPatternEditor ? 56 : 80;

		if (inst->uiState.extendedPatternEditor)
			areaWidth = 443;

		/* Clear area */
		uint16_t clrX = x + ((areaWidth - maxStrWidth) / 2);
		fillRect(video, clrX, y, maxStrWidth, 11, PAL_DESKTOP);

		/* Draw text (centered) */
		if (str != NULL)
		{
			uint16_t textW = textWidth(str);
			uint16_t textX = x + ((areaWidth - textW) / 2);
			textOut(video, bmp, textX, y, PAL_FORGRND, str);
		}
	}
}

void ft2_ui_update(ft2_ui_t *ui, void *inst)
{
	if (ui == NULL)
		return;

	ft2_input_update(&ui->input);
	ft2_about_update();
	ft2_scopes_update(&ui->scopes, inst);

	/* Handle held-down widgets (scrollbar drag, button repeat, etc.) */
	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
	ft2_widgets_handle_held_down(&ui->widgets, ft2inst, &ui->video, bmp);

	/* Handle per-frame redrawing */
	if (ft2inst != NULL)
		handleRedrawing(ui, ft2inst);
}

uint32_t *ft2_ui_get_framebuffer(ft2_ui_t *ui)
{
	if (ui == NULL)
		return NULL;
	return ui->video.frameBuffer;
}

void ft2_ui_mouse_press(ft2_ui_t *ui, void *inst, int x, int y, bool leftButton, bool rightButton)
{
	if (ui == NULL)
		return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	
	/* Derive single button value for legacy code that needs it */
	int button = leftButton ? MOUSE_BUTTON_LEFT : (rightButton ? MOUSE_BUTTON_RIGHT : 0);

	/* Handle modal panels - use widget system with sysReqShown=true */
	if (ft2_modal_panel_is_any_active())
	{
		/* Handle special panel-specific clicks (like echo panel checkbox) */
		if (ft2_modal_panel_get_active() == MODAL_PANEL_ECHO)
			ft2_echo_panel_mouse_down(x, y, button);
		
		/* Use widget system for buttons and scrollbars (only reserved slots) */
		ft2_widgets_mouse_down(&ui->widgets, ft2inst, &ui->video, x, y, true);
		return;
	}
	
	if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_mouse_down(&ui->dialog, x, y, button);
		return;
	}

	ft2_input_mouse_down(&ui->input, x, y, button);

	/* Test instrument switcher FIRST (kludge: allow right click to both change ins. and edit text)
	 * This matches the original FT2 behavior where testInstrSwitcherMouseDown is called before
	 * testTextBoxMouseDown, allowing left-click to select instruments and right-click to edit names */
	if (ft2inst != NULL)
	{
		testInstrSwitcherMouseDown(ft2inst, x, y);
	}

	/* Test textboxes for mouse clicks
	 * - Right-click: can edit instrument/sample names (rightMouseButton=true) and song name
	 * - Left-click: can only edit textboxes that allow it (rightMouseButton=false, like song name)
	 */
	if (ft2inst != NULL)
		ft2_textbox_update_pointers(ft2inst);
	
	int16_t textBoxHit = ft2_textbox_test_mouse_down(x, y, rightButton);
	if (textBoxHit >= 0)
	{
		/* When a textbox is activated for editing, trigger instrument switcher redraw
		 * so the highlight is drawn before the textbox tries to read background color */
		if (ft2inst != NULL)
			ft2inst->uiState.updateInstrSwitcher = true;
		return;
	}

	/* Try widgets (normal mode) */
	ft2_widgets_mouse_down(&ui->widgets, ft2inst, &ui->video, x, y, false);
	
	/* If a widget was hit, don't process other areas */
	if (getLastUsedWidget() != -1)
		return;

	/* Test disk op file list clicks */
	if (ft2inst != NULL && ft2inst->uiState.diskOpShown)
	{
		if (diskOpTestMouseDown(ft2inst, x, y))
			return;
	}

	/* Test scopes mouse handling (mute/rec/solo) - pass both button states for left+right combo */
	if (ft2inst != NULL && ft2inst->uiState.scopesShown)
	{
		const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
		if (ft2_scopes_mouse_down(&ui->scopes, &ui->video, bmp, x, y, leftButton, rightButton))
		{
			/* Also sync mute state to replayer channels */
			for (int i = 0; i < ft2inst->replayer.song.numChannels; i++)
			{
				ft2inst->replayer.channel[i].channelOff = ui->scopes.channelMuted[i];
				if (ui->scopes.channelMuted[i])
				{
					ft2inst->replayer.channel[i].realVol = 0;
					ft2inst->replayer.channel[i].outVol = 0;
					ft2inst->replayer.channel[i].fFinalVol = 0.0f;
					/* Stop scope for muted channel (matches standalone behavior) */
					ft2_scope_stop(&ui->scopes, i);
				}
			}
			return;
		}
	}

	/* Pattern editor mouse handling - pattern marking */
	if (ft2inst != NULL && ft2inst->uiState.patternEditorShown)
	{
		int32_t pattY1 = ft2inst->uiState.extendedPatternEditor ? 71 : 176;
		int32_t pattY2 = ft2inst->uiState.pattChanScrollShown ? 382 : 396;
		
		if (y >= pattY1 && y <= pattY2 && x >= 29 && x <= 602)
		{
			/* Click inside pattern data area - start pattern marking */
			handlePatternDataMouseDown(ft2inst, x, y, false, rightButton);
			ui->input.pattMarkDragging = true;
			return;
		}
	}
	
	/* Sample editor mouse handling */
	if (ft2inst != NULL && ft2inst->uiState.sampleEditorShown)
	{
		if (y >= 174 && y <= 328)
		{
			/* Click inside sample data area */
			ft2_sample_ed_mouse_click(&ui->sampleEditor, x, y, button);
			ui->input.mouseDragging = true;
		}
	}
	
	/* Instrument editor mouse handling */
	if (ft2inst != NULL && ft2inst->uiState.instEditorShown)
	{
		/* Instrument editor covers y >= 173 to y <= 399 */
		if (y >= 173)
		{
			ft2_instr_ed_mouse_click(&ui->instrEditor, x, y, button);
			ui->input.mouseDragging = true;
		}
	}
}

void ft2_ui_mouse_release(ft2_ui_t *ui, void *inst, int x, int y, int button)
{
	if (ui == NULL)
		return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;

	/* Handle modal panels - widget system handles button release */
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
	
	ft2_widgets_mouse_up(&ui->widgets, x, y, ft2inst, &ui->video, bmp);

	/* Handle sample editor mouse up */
	if (ft2inst != NULL && ft2inst->uiState.sampleEditorShown)
	{
		ft2_sample_ed_mouse_up(&ui->sampleEditor);
	}
	
	/* Handle instrument editor mouse up */
	if (ft2inst != NULL && ft2inst->uiState.instEditorShown)
	{
		ft2_instr_ed_mouse_up(&ui->instrEditor);
	}
}

void ft2_ui_mouse_move(ft2_ui_t *ui, void *inst, int x, int y)
{
	if (ui == NULL)
		return;

	ft2_input_mouse_move(&ui->input, x, y);
	ft2_widgets_mouse_move(&ui->widgets, x, y);

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;

	/* Handle pattern editor mouse drag for pattern marking */
	if (ft2inst != NULL && ft2inst->uiState.patternEditorShown && ui->input.pattMarkDragging)
	{
		handlePatternDataMouseDown(ft2inst, x, y, true, false);
	}

	/* Handle sample editor mouse drag */
	if (ft2inst != NULL && ft2inst->uiState.sampleEditorShown && ui->input.mouseDragging)
	{
		bool shiftPressed = (ui->input.modifiers & FT2_MOD_SHIFT) != 0;
		ft2_sample_ed_mouse_drag(&ui->sampleEditor, x, y, shiftPressed);
	}
	
	/* Handle instrument editor mouse drag */
	if (ft2inst != NULL && ft2inst->uiState.instEditorShown && ui->input.mouseDragging)
	{
		ft2_instr_ed_mouse_drag(&ui->instrEditor, x, y);
	}
}

void ft2_ui_mouse_wheel(ft2_ui_t *ui, void *inst, int x, int y, int delta)
{
	if (ui == NULL)
		return;

	ft2_instance_t *ft2inst = (ft2_instance_t *)inst;
	ft2_input_mouse_wheel(&ui->input, delta);
	
	if (ft2inst == NULL)
		return;
	
	bool directionUp = (delta > 0);
	
	/* Top screen (y < 173) */
	if (y < 173)
	{
		/* Help screen - 2x scroll speed */
		if (ft2inst->uiState.helpScreenShown)
		{
			ft2_video_t *video = &ui->video;
			const ft2_bmp_t *bmp = ui->bmpLoaded ? &ui->bmp : NULL;
			if (directionUp)
			{
				helpScrollUp(ft2inst, video, bmp);
				helpScrollUp(ft2inst, video, bmp);
			}
			else
			{
				helpScrollDown(ft2inst, video, bmp);
				helpScrollDown(ft2inst, video, bmp);
			}
			return;
		}

		/* Disk op - 3x scroll speed when in file list area */
		if (ft2inst->uiState.diskOpShown && x <= 355)
		{
			if (directionUp)
			{
				pbDiskOpListUp(ft2inst);
				pbDiskOpListUp(ft2inst);
				pbDiskOpListUp(ft2inst);
			}
			else
			{
				pbDiskOpListDown(ft2inst);
				pbDiskOpListDown(ft2inst);
				pbDiskOpListDown(ft2inst);
			}
			return;
		}

		/* Skip other controls when overlay screens shown */
		if (ft2inst->uiState.aboutScreenShown || ft2inst->uiState.configScreenShown ||
		    ft2inst->uiState.nibblesShown || ft2inst->uiState.diskOpShown)
			return;

		/* Position editor area */
		if (x <= 111 && y <= 76)
		{
			bool posChanged = false;
			if (directionUp)
			{
				if (ft2inst->replayer.song.songPos > 0)
				{
					ft2inst->replayer.song.songPos--;
					posChanged = true;
				}
			}
			else
			{
				if (ft2inst->replayer.song.songPos < ft2inst->replayer.song.songLength - 1)
				{
					ft2inst->replayer.song.songPos++;
					posChanged = true;
				}
			}
			
			if (posChanged)
			{
				/* Update pattern and reset row (matches standalone setNewSongPos) */
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
		/* Instrument area */
		else if (x >= 421)
		{
			if (y <= 93)
			{
				/* Instrument number */
				if (directionUp)
				{
					if (ft2inst->editor.curInstr > 0)
						ft2inst->editor.curInstr--;
				}
				else
				{
					if (ft2inst->editor.curInstr < 127)
						ft2inst->editor.curInstr++;
				}
			}
			else
			{
				/* Sample number */
				if (directionUp)
				{
					if (ft2inst->editor.curSmp > 0)
						ft2inst->editor.curSmp--;
				}
				else
				{
					if (ft2inst->editor.curSmp < 15)
						ft2inst->editor.curSmp++;
				}
			}
		}
	}
	else
	{
		/* Bottom screen */
		if (ft2inst->uiState.patternEditorShown)
		{
			/* Pattern editor - scroll rows */
			int32_t numRows = ft2inst->replayer.patternNumRows[ft2inst->editor.editPattern];
			if (directionUp)
			{
				if (ft2inst->editor.row > 0)
					ft2inst->editor.row--;
			}
			else
			{
				if (ft2inst->editor.row < numRows - 1)
					ft2inst->editor.row++;
			}
			ft2inst->uiState.updatePatternEditor = true;
		}
		else if (ft2inst->uiState.sampleEditorShown)
		{
			/* Sample editor - zoom in/out when mouse is in sample area (y >= 174 && y <= 328) */
			if (y >= 174 && y <= 328)
			{
				if (directionUp)
					ft2_sample_ed_zoom_in(&ui->sampleEditor, x);
				else
					ft2_sample_ed_zoom_out(&ui->sampleEditor, x);
				
				ft2inst->uiState.updateSampleEditor = true;
			}
		}
	}
}

void ft2_ui_key_press(ft2_ui_t *ui, void *inst, int key, int modifiers)
{
	if (ui == NULL)
		return;

	/* Handle modal panels with text input (wave/filter panels) */
	if (ft2_wave_panel_is_active())
	{
		ft2_wave_panel_key_down(key);
		return;
	}
	if (ft2_filter_panel_is_active())
	{
		ft2_filter_panel_key_down(key);
		return;
	}
	
	/* Handle other modal panels - they use widgets only */
	if (ft2_modal_panel_is_any_active())
	{
		return;
	}
	if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_key_down(&ui->dialog, key);
		return;
	}

	/* If editing a textbox, forward keys there instead */
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
	if (ui == NULL)
		return;

	/* Handle modal panels with text input (wave/filter panels) */
	if (ft2_wave_panel_is_active())
	{
		ft2_wave_panel_char_input(c);
		return;
	}
	if (ft2_filter_panel_is_active())
	{
		ft2_filter_panel_char_input(c);
		return;
	}

	/* Handle dialog first if active */
	if (ft2_dialog_is_active(&ui->dialog))
	{
		ft2_dialog_char_input(&ui->dialog, c);
		return;
	}

	/* Forward to textbox if editing */
	if (ft2_textbox_is_editing())
	{
		ft2_textbox_input_char(c);
	}
}

void ft2_ui_key_release(ft2_ui_t *ui, void *inst, int key, int modifiers)
{
	if (ui == NULL)
		return;

	ft2_input_key_up((ft2_instance_t *)inst, &ui->input, key, modifiers);
}

void ft2_ui_key_state_changed(ft2_ui_t *ui, bool isKeyDown)
{
	(void)ui;
	(void)isKeyDown;
}
