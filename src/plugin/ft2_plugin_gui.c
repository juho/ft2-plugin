/**
 * @file ft2_plugin_gui.c
 * @brief Screen visibility management: hide/show widget groups.
 *
 * Ensures only one top-screen overlay (config/help/about/nibbles) is visible.
 * Top and bottom screen areas are independent - overlays don't affect editors.
 *
 * Related files:
 *   ft2_plugin_ui.c     - render loop, input
 *   ft2_plugin_layout.c - drawing + initial show*
 *   ft2_plugin_*.c      - screen-specific callbacks
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

/* Hide S.E.Ext, I.E.Ext, Transpose, Adv.Edit, Trim - mutually exclusive overlays */
void hideAllTopLeftPanelOverlays(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	/* Always hide unconditionally - idempotent and ensures sync after reset */
	hideSampleEditorExt(inst);
	hideInstEditorExt(inst);
	hideTranspose(inst);
	hideAdvEdit(inst);
	hideTrimScreen(inst);
}

/* Hide position editor, logo, left menu, song/pattern controls, and panel overlays */
void hideTopLeftMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
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

	/* Logo + left menu */
	hidePushButton(widgets, PB_LOGO);
	hidePushButton(widgets, PB_BADGE);
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

	/* Song/pattern controls */
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

	hideAllTopLeftPanelOverlays(inst);
	inst->uiState.diskOpShown = false;
}

/* Hide right menu, instrument switcher, and song name textbox */
void hideTopRightMainScreen(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL) return;
	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;

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

	hideInstrumentSwitcher(inst);
	inst->uiState.instrSwitcherShown = false;
	ft2_textbox_hide(TB_SONG_NAME);
}

/* Hide all top-screen elements: main sides + overlays */
void hideTopScreen(ft2_instance_t *inst)
{
	if (inst == NULL) return;

	hideTopLeftMainScreen(inst);
	hideTopRightMainScreen(inst);
	hideConfigScreen(inst);
	hideHelpScreen(inst);
	hideDiskOpScreen(inst);

	inst->uiState.instrSwitcherShown = false;
	inst->uiState.scopesShown = false;
}

