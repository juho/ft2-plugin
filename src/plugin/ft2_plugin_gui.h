/**
 * @file ft2_plugin_gui.h
 * @brief Screen visibility management for the FT2 plugin.
 *
 * Ported from ft2_gui.c - handles showing/hiding top screen elements
 * to ensure only one overlay screen (config/help/about/nibbles) is visible at a time.
 */

#pragma once

#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/**
 * Hide the top left main screen widgets.
 * Hides position editor, logo/badge buttons, and left menu buttons.
 */
void hideTopLeftMainScreen(struct ft2_instance_t *inst);

/**
 * Hide the top right main screen widgets.
 * Hides right menu buttons, instrument switcher, and song name textbox.
 */
void hideTopRightMainScreen(struct ft2_instance_t *inst);

/**
 * Hide all top screen elements.
 * Hides both main screen sides plus all overlay screens (config/help/about/nibbles).
 * Call this before showing any overlay screen.
 */
void hideTopScreen(struct ft2_instance_t *inst);
