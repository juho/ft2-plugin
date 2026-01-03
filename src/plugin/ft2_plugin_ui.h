/**
 * @file ft2_plugin_ui.h
 * @brief Main UI controller for the FT2 plugin.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_input.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_scopes.h"
#include "ft2_plugin_dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Screen dimensions */
#define FT2_SCREEN_W SCREEN_W
#define FT2_SCREEN_H SCREEN_H

/* UI layout constants */
#define FT2_HEADER_HEIGHT 16
#define FT2_UI_SCOPE_HEIGHT 77
#define FT2_INFO_HEIGHT 24
#define FT2_TAB_HEIGHT 16
#define FT2_MAX_CHANNELS 32

/* UI screens */
typedef enum ft2_ui_screen
{
	FT2_SCREEN_PATTERN = 0,
	FT2_SCREEN_SAMPLE,
	FT2_SCREEN_INSTR,
	FT2_SCREEN_CONFIG,
	FT2_SCREEN_DISK_OP,
	FT2_SCREEN_ABOUT,
	FT2_NUM_SCREENS
} ft2_ui_screen;

/**
 * Main UI state
 */
typedef struct ft2_ui_t
{
	/* Video state */
	ft2_video_t video;

	/* Bitmap assets */
	ft2_bmp_t bmp;
	bool bmpLoaded;

	/* Input state */
	ft2_input_state_t input;

	/* Widget state */
	ft2_widgets_t widgets;

	/* Current screen */
	ft2_ui_screen currentScreen;

	/* Component instances */
	ft2_pattern_editor_t patternEditor;
	ft2_sample_editor_t sampleEditor;
	ft2_instrument_editor_t instrEditor;
	ft2_scopes_t scopes;

	/* Song info */
	int16_t currInstr;
	int16_t currSample;
	int16_t currOctave;

	/* Dialog state */
	ft2_dialog_t dialog;

	/* Redraw flags */
	bool needsFullRedraw;

	/* Palette initialization flag - ensures config palette is applied once */
	bool paletteInitialized;
} ft2_ui_t;

/* Helper macros to derive editor pointers from instance */
#define FT2_UI(inst)         ((ft2_ui_t*)(inst)->ui)
#define FT2_SAMPLE_ED(inst)  (&FT2_UI(inst)->sampleEditor)
#define FT2_INSTR_ED(inst)   (&FT2_UI(inst)->instrEditor)
#define FT2_PATTERN_ED(inst) (&FT2_UI(inst)->patternEditor)
#define FT2_VIDEO(inst)      (&FT2_UI(inst)->video)
#define FT2_BMP(inst)        (&FT2_UI(inst)->bmp)

void ft2_ui_init(ft2_ui_t *ui);
void ft2_ui_shutdown(ft2_ui_t *ui);
void ft2_ui_set_screen(ft2_ui_t *ui, ft2_ui_screen screen);
void ft2_ui_draw(ft2_ui_t *ui, void *inst);
void ft2_ui_update(ft2_ui_t *ui, void *inst);
uint32_t *ft2_ui_get_framebuffer(ft2_ui_t *ui);
void ft2_ui_mouse_press(ft2_ui_t *ui, void *inst, int x, int y, bool leftButton, bool rightButton);
void ft2_ui_mouse_release(ft2_ui_t *ui, void *inst, int x, int y, int button);
void ft2_ui_mouse_move(ft2_ui_t *ui, void *inst, int x, int y);
void ft2_ui_mouse_wheel(ft2_ui_t *ui, void *inst, int x, int y, int delta);
void ft2_ui_key_press(ft2_ui_t *ui, void *inst, int key, int modifiers);
void ft2_ui_key_release(ft2_ui_t *ui, void *inst, int key, int modifiers);
void ft2_ui_key_state_changed(ft2_ui_t *ui, bool isKeyDown);
void ft2_ui_text_input(ft2_ui_t *ui, char c);

#ifdef __cplusplus
}
#endif
