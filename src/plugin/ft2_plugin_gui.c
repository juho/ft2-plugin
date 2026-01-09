/**
 * @file ft2_plugin_gui.c
 * @brief Screen visibility management for the FT2 plugin.
 *
 * This file handles widget visibility state management - showing/hiding widget
 * groups for different screen configurations. It ensures only one top-screen
 * overlay (config/help/about/nibbles) is visible at a time.
 *
 * SEPARATION OF CONCERNS:
 * - ft2_plugin_gui.c: Widget visibility state management (hide/show widget groups)
 * - ft2_plugin_ui.c:  Main UI controller (rendering loop, input handling, updates)
 * - ft2_plugin_layout.c: Drawing functions that also call show* on initial display
 * - ft2_plugin_config.c/help.c/etc: Screen-specific show/hide and callbacks
 *
 * This separation mirrors the standalone architecture (ft2_gui.c vs ft2_video.c).
 *
 * KEY PRINCIPLE: Top and bottom screen areas are independent.
 * - Top overlays (config/help) only hide/show top-screen widgets
 * - Bottom editors (pattern/sample/instrument) continue to draw regardless
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_gui.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_textbox.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_help.h"
#include "ft2_plugin_layout.h"
#include "ft2_plugin_instrsw.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_trim.h"
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_ui.h"
#include "ft2_instance.h"

void hideAllTopLeftPanelOverlays(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Always hide ALL overlays unconditionally - don't check flags.
	 * This ensures widget visibility matches state even if flags were
	 * already cleared (e.g., by ft2_instance_reset during module loading).
	 * Calling hide on an already-hidden overlay is safe and idempotent. */
	hideSampleEditorExt(inst);
		hideInstEditorExt(inst);
		hideTranspose(inst);
		hideAdvEdit(inst);
		hideTrimScreen(inst);
}

void hideTopLeftMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->uiState.scopesShown = false;

	/* Position editor */
	hideScrollBar(widgets, SB_POS_ED);
	hidePushButton(widgets, PB_POSED_POS_UP);
	hidePushButton(widgets, PB_POSED_POS_DOWN);
	hidePushButton(widgets, PB_POSED_INS);
	hidePushButton(widgets, PB_POSED_PATT_UP);
	hidePushButton(widgets, PB_POSED_PATT_DOWN);
	hidePushButton(widgets, PB_POSED_DEL);
	hidePushButton(widgets, PB_POSED_LEN_UP);
	hidePushButton(widgets, PB_POSED_LEN_DOWN);
	hidePushButton(widgets, PB_POSED_REP_UP);
	hidePushButton(widgets, PB_POSED_REP_DOWN);

	/* Logo buttons */
	hidePushButton(widgets, PB_LOGO);
	hidePushButton(widgets, PB_BADGE);

	/* Left menu buttons */
	hidePushButton(widgets, PB_ABOUT);
	hidePushButton(widgets, PB_NIBBLES);
	hidePushButton(widgets, PB_KILL);
	hidePushButton(widgets, PB_TRIM);
	hidePushButton(widgets, PB_EXTEND_VIEW);
	hidePushButton(widgets, PB_TRANSPOSE);
	hidePushButton(widgets, PB_INST_ED_EXT);
	hidePushButton(widgets, PB_SMP_ED_EXT);
	hidePushButton(widgets, PB_ADV_EDIT);
	hidePushButton(widgets, PB_ADD_CHANNELS);
	hidePushButton(widgets, PB_SUB_CHANNELS);

	/* Song/pattern control buttons */
	hidePushButton(widgets, PB_BPM_UP);
	hidePushButton(widgets, PB_BPM_DOWN);
	hidePushButton(widgets, PB_SPEED_UP);
	hidePushButton(widgets, PB_SPEED_DOWN);
	hidePushButton(widgets, PB_EDITADD_UP);
	hidePushButton(widgets, PB_EDITADD_DOWN);
	hidePushButton(widgets, PB_PATT_UP);
	hidePushButton(widgets, PB_PATT_DOWN);
	hidePushButton(widgets, PB_PATTLEN_UP);
	hidePushButton(widgets, PB_PATTLEN_DOWN);
	hidePushButton(widgets, PB_PATT_EXPAND);
	hidePushButton(widgets, PB_PATT_SHRINK);

	/* Hide all panel overlays (S.E.Ext, I.E.Ext, Transpose, Adv.Edit, Trim) */
	hideAllTopLeftPanelOverlays(inst);
	inst->uiState.diskOpShown = false;
}

void hideTopRightMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

	/* Right menu buttons */
	hidePushButton(widgets, PB_PLAY_SONG);
	hidePushButton(widgets, PB_PLAY_PATT);
	hidePushButton(widgets, PB_STOP);
	hidePushButton(widgets, PB_RECORD_SONG);
	hidePushButton(widgets, PB_RECORD_PATT);
	hidePushButton(widgets, PB_DISK_OP);
	hidePushButton(widgets, PB_INST_ED);
	hidePushButton(widgets, PB_SMP_ED);
	hidePushButton(widgets, PB_CONFIG);
	hidePushButton(widgets, PB_HELP);

	/* Instrument switcher */
	hideInstrumentSwitcher(inst);
	inst->uiState.instrSwitcherShown = false;

	/* Song name textbox */
	ft2_textbox_hide(TB_SONG_NAME);
}

void hideTopScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Hide both main screen sides */
	hideTopLeftMainScreen(inst);
	hideTopRightMainScreen(inst);

	/* Hide all overlay screens */
	hideConfigScreen(inst);
	hideHelpScreen(inst);
	hideDiskOpScreen(inst);
	/* hideAboutScreen(inst); - about screen doesn't have widgets to hide */
	/* hideNibblesScreen(inst); - nibbles not implemented yet */

	/* Reset visibility flags */
	inst->uiState.instrSwitcherShown = false;
	inst->uiState.scopesShown = false;
}

