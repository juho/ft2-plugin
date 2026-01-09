/**
 * @file ft2_plugin_gui.h
 * @brief Screen visibility management: hide/show widget groups.
 *
 * Ensures only one top-screen overlay (config/help/about/nibbles) is visible.
 */

#pragma once

#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Hide S.E.Ext, I.E.Ext, Transpose, Adv.Edit, Trim (mutually exclusive) */
void hideAllTopLeftPanelOverlays(struct ft2_instance_t *inst);

/* Hide position editor, logo, left menu, song/pattern controls */
void hideTopLeftMainScreen(struct ft2_instance_t *inst);

/* Hide right menu, instrument switcher, song name textbox */
void hideTopRightMainScreen(struct ft2_instance_t *inst);

/* Hide all top-screen elements: main sides + overlays */
void hideTopScreen(struct ft2_instance_t *inst);
