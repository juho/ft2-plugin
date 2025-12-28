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
#include "ft2_instance.h"

void hideTopLeftMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	inst->uiState.scopesShown = false;

	/* Position editor */
	hideScrollBar(SB_POS_ED);
	hidePushButton(PB_POSED_POS_UP);
	hidePushButton(PB_POSED_POS_DOWN);
	hidePushButton(PB_POSED_INS);
	hidePushButton(PB_POSED_PATT_UP);
	hidePushButton(PB_POSED_PATT_DOWN);
	hidePushButton(PB_POSED_DEL);
	hidePushButton(PB_POSED_LEN_UP);
	hidePushButton(PB_POSED_LEN_DOWN);
	hidePushButton(PB_POSED_REP_UP);
	hidePushButton(PB_POSED_REP_DOWN);

	/* Logo buttons */
	hidePushButton(PB_LOGO);
	hidePushButton(PB_BADGE);

	/* Left menu buttons */
	hidePushButton(PB_ABOUT);
	hidePushButton(PB_NIBBLES);
	hidePushButton(PB_KILL);
	hidePushButton(PB_TRIM);
	hidePushButton(PB_EXTEND_VIEW);
	hidePushButton(PB_TRANSPOSE);
	hidePushButton(PB_INST_ED_EXT);
	hidePushButton(PB_SMP_ED_EXT);
	hidePushButton(PB_ADV_EDIT);
	hidePushButton(PB_ADD_CHANNELS);
	hidePushButton(PB_SUB_CHANNELS);

	/* Song/pattern control buttons */
	hidePushButton(PB_BPM_UP);
	hidePushButton(PB_BPM_DOWN);
	hidePushButton(PB_SPEED_UP);
	hidePushButton(PB_SPEED_DOWN);
	hidePushButton(PB_EDITADD_UP);
	hidePushButton(PB_EDITADD_DOWN);
	hidePushButton(PB_PATT_UP);
	hidePushButton(PB_PATT_DOWN);
	hidePushButton(PB_PATTLEN_UP);
	hidePushButton(PB_PATTLEN_DOWN);
	hidePushButton(PB_PATT_EXPAND);
	hidePushButton(PB_PATT_SHRINK);

	/* Hide and reset sub-screens */
	hideTranspose(inst);
	hideAdvEdit(inst);
	hideTrimScreen(inst);
	inst->uiState.diskOpShown = false;
	inst->uiState.sampleEditorExtShown = false;
	inst->uiState.instEditorExtShown = false;
}

void hideTopRightMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Right menu buttons */
	hidePushButton(PB_PLAY_SONG);
	hidePushButton(PB_PLAY_PATT);
	hidePushButton(PB_STOP);
	hidePushButton(PB_RECORD_SONG);
	hidePushButton(PB_RECORD_PATT);
	hidePushButton(PB_DISK_OP);
	hidePushButton(PB_INST_ED);
	hidePushButton(PB_SMP_ED);
	hidePushButton(PB_CONFIG);
	hidePushButton(PB_HELP);

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

