/**
 * @file ft2_plugin_palette.c
 * @brief Palette editor and color scheme management.
 *
 * 12 preset palettes (Arctic, Blues, Dark Mode, etc.) plus user-defined.
 * Each palette has 16 colors in 18-bit VGA format (6 bits per channel).
 * Desktop and Buttons colors have contrast scaling for derived shades.
 */

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

#include "ft2_plugin_video.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_dialog.h"
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

/* Helper macro to access per-instance palette editor state */
#define PAL_ED(inst) (&FT2_UI(inst)->paletteEditor)

/* Maps editor entry index to palette index */
static const uint8_t FTC_EditOrder[6] = { PAL_PATTEXT, PAL_BLCKMRK, PAL_BLCKTXT, PAL_MOUSEPT, PAL_DESKTOP, PAL_BUTTONS };

/* Derived color indices for contrast scaling (Desktop/Buttons derive 3 shades each) */
static const uint8_t scaleOrder[3] = { 8, 4, 9 };

/* Contrast values per preset [preset][0=Desktop, 1=Buttons] */
uint8_t palContrast[12][2] = {
	{59, 55}, {59, 53}, {56, 59}, {68, 55}, {57, 59}, {48, 55},
	{66, 62}, {68, 57}, {58, 42}, {57, 55}, {62, 57}, {52, 57}
};

/* 18-bit VGA palettes (0-63 per channel). Indices match PAL_* enum order. */
pal16 pluginPalTable[12][16] = {
	{ /* Arctic */
		{0, 0, 0},{30, 38, 63},{0, 0, 17},{63, 63, 63},
		{27, 36, 40},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{10, 13, 14},{49, 63, 63},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Aurora Borealis */
		{0, 0, 0},{21, 40, 63},{0, 0, 17},{63, 63, 63},
		{6, 39, 35},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{2, 14, 13},{11, 63, 63},{16, 16, 16},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Blues */
		{0, 0, 0},{39, 52, 63},{8, 8, 13},{57, 57, 63},
		{10, 21, 33},{63, 63, 63},{37, 37, 45},{0, 0, 0},
		{4, 8, 13},{18, 37, 58},{13, 13, 16},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Gold */
		{0, 0, 0},{47, 47, 47},{9, 9, 9},{63, 63, 63},
		{37, 29, 7},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{11, 9, 2},{63, 58, 14},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Heavy Metal */
		{0, 0, 0},{46, 45, 46},{13, 9, 9},{63, 63, 63},
		{22, 19, 22},{63, 63, 63},{36, 32, 34},{0, 0, 0},
		{8, 7, 8},{39, 34, 39},{13, 12, 12},{63, 58, 62},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Jungle */
		{0, 0, 0},{19, 49, 54},{0, 11, 7},{52, 63, 61},
		{9, 31, 21},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{4, 13, 9},{15, 50, 34},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Lithe Dark */
		{0, 0, 0},{27, 37, 53},{0, 0, 20},{63, 63, 63},
		{7, 12, 21},{63, 63, 63},{38, 39, 39},{0, 0, 0},
		{2, 4, 7},{14, 23, 41},{13, 13, 13},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Rose */
		{0, 0, 0},{63, 54, 62},{18, 3, 3},{63, 63, 63},
		{36, 19, 25},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{11, 6, 8},{63, 38, 50},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Dark Mode */
		{0, 0, 0},{31, 36, 42},{6, 6, 9},{47, 50, 54},
		{11, 12, 13},{55, 55, 56},{32, 32, 33},{0, 0, 0},
		{3, 4, 4},{22, 24, 26},{15, 15, 15},{50, 50, 52},
		{55, 55, 56},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Violent */
		{0, 0, 0},{50, 46, 63},{15, 0, 16},{59, 58, 63},
		{34, 21, 41},{63, 63, 63},{40, 40, 40},{0, 0, 0},
		{13, 8, 15},{61, 37, 63},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* Why Colors */
		{0, 0, 0},{63, 63, 32},{10, 10, 10},{63, 63, 63},
		{18, 29, 32},{63, 63, 63},{39, 39, 39},{0, 0, 0},
		{6, 10, 11},{34, 54, 60},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	},
	{ /* User Defined (default: Blues variant) */
		{0, 0, 0},{36, 47, 63},{9, 9, 16},{63, 63, 63},
		{19, 24, 38},{63, 63, 63},{39, 39, 39},{0, 0, 0},
		{8, 10, 15},{32, 41, 63},{15, 15, 15},{63, 63, 63},
		{63, 63, 63},{63, 63, 63},{63, 63, 63},{63, 63, 63}
	}
};

/* ---------- Helpers ---------- */

/* Clamp to 6-bit VGA range */
uint8_t palMax(int32_t c) { return (c < 0) ? 0 : (c > 63) ? 63 : (uint8_t)c; }

/* Clamped power function for contrast scaling */
static double palPow(double dX, double dY)
{
	if (dY == 1.0) return dX;
	dY *= log(fabs(dX));
	if (dY < -86.0) dY = -86.0;
	if (dY > 86.0) dY = 86.0;
	return exp(dY);
}

/* Apply 16-color palette to video. High byte stores palette index for recoloring. */
void setPal16(ft2_video_t *video, pal16 *p, bool redrawScreen)
{
#define LOOP_PIN_COL_SUB 96
#define TEXT_MARK_COLOR 0x0078D7
#define BOX_SELECT_COLOR 0x7F7F7F

	if (video == NULL) return;

	/* Convert 6-bit VGA to 8-bit and store index in high byte */
	for (int32_t i = 0; i < 16; i++) {
		int16_t r = P6_TO_P8(p[i].r), g = P6_TO_P8(p[i].g), b = P6_TO_P8(p[i].b);
		video->palette[i] = ((uint32_t)i << 24) | RGB32(r, g, b);
	}

	/* Extended palette entries (FT2 clone additions) */
	video->palette[PAL_TEXTMRK] = ((uint32_t)PAL_TEXTMRK << 24) | TEXT_MARK_COLOR;
	video->palette[PAL_BOXSLCT] = ((uint32_t)PAL_BOXSLCT << 24) | BOX_SELECT_COLOR;

	/* Loop pin: darkened version of pattern text */
	int16_t r = RGB32_R(video->palette[PAL_PATTEXT]) - LOOP_PIN_COL_SUB;
	int16_t g = RGB32_G(video->palette[PAL_PATTEXT]) - LOOP_PIN_COL_SUB;
	int16_t b = RGB32_B(video->palette[PAL_PATTEXT]) - LOOP_PIN_COL_SUB;
	if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
	video->palette[PAL_LOOPPIN] = ((uint32_t)PAL_LOOPPIN << 24) | RGB32(r, g, b);

	/* Remap existing framebuffer pixels using stored palette index */
	if (redrawScreen && video->frameBuffer)
		for (int32_t i = 0; i < SCREEN_W * SCREEN_H; i++)
			video->frameBuffer[i] = video->palette[(video->frameBuffer[i] >> 24) & 15];
}

/* ---------- Editor state ---------- */

static void showColorErrorMsg(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	if (pal->colorErrorShown) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_dialog_show_message(&ui->dialog, "System message", "Default colors cannot be modified.");
	pal->colorErrorShown = true;
}

void resetPaletteErrorFlag(ft2_instance_t *inst)
{
	if (inst && inst->ui) PAL_ED(inst)->colorErrorShown = false;
}

/* Draw hex value and color swatch in config header */
void drawCurrentPaletteColor(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	const uint8_t palIndex = FTC_EditOrder[pal->colorNum];
	uint32_t color = RGB32(P6_TO_P8(pal->red), P6_TO_P8(pal->green), P6_TO_P8(pal->blue)) & 0xFFFFFF;

	textOutShadow(video, bmp, 516, 3, PAL_FORGRND, PAL_DSKTOP2, "Palette:");
	hexOutBg(video, bmp, 573, 3, PAL_FORGRND, PAL_DESKTOP, color, 6);
	clearRect(video, 616, 2, 12, 10);
	fillRect(video, 617, 3, 10, 8, palIndex);
}

/* Sync sliders with current palette entry */
void updatePaletteEditor(ft2_instance_t *inst, ft2_video_t *video)
{
	if (!inst || !inst->ui) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = &ui->widgets;
	palette_editor_state_t *pal = PAL_ED(inst);

	const uint8_t colorNum = FTC_EditOrder[pal->colorNum];
	const uint8_t palNum = inst->config.palettePreset;

	pal->red = pluginPalTable[palNum][colorNum].r;
	pal->green = pluginPalTable[palNum][colorNum].g;
	pal->blue = pluginPalTable[palNum][colorNum].b;
	pal->contrast = (pal->colorNum >= 4) ? palContrast[palNum][pal->colorNum - 4] : 0;

	setScrollBarPos(inst, widgets, video, SB_PAL_R, pal->red, false);
	setScrollBarPos(inst, widgets, video, SB_PAL_G, pal->green, false);
	setScrollBarPos(inst, widgets, video, SB_PAL_B, pal->blue, false);
	setScrollBarPos(inst, widgets, video, SB_PAL_CONTRAST, pal->contrast, false);
}

/* Called when RGB/contrast sliders are dragged. Only user-defined palette is editable. */
static void paletteDragMoved(ft2_instance_t *inst)
{
	if (!inst || !inst->ui) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_video_t *video = &ui->video;
	ft2_widgets_t *widgets = &ui->widgets;
	palette_editor_state_t *pal = PAL_ED(inst);

	if (inst->config.palettePreset != PAL_USER_DEFINED) {
		updatePaletteEditor(inst, video);
		showColorErrorMsg(inst);
		return;
	}

	const uint8_t colorNum = FTC_EditOrder[pal->colorNum];
	const uint8_t p = inst->config.palettePreset;

	pluginPalTable[p][colorNum].r = pal->red;
	pluginPalTable[p][colorNum].g = pal->green;
	pluginPalTable[p][colorNum].b = pal->blue;

	/* Desktop/Buttons derive 3 shades via contrast-scaled power curve */
	if (pal->colorNum >= 4) {
		int32_t contrast = (pal->contrast < 1) ? 1 : pal->contrast;
		double dContrast = contrast * (1.0 / 40.0);
		for (int32_t i = 0; i < 3; i++) {
			int32_t k = scaleOrder[i] + (pal->colorNum - 4) * 2;
			double dMul = palPow((i + 1) * 0.5, dContrast);
			pluginPalTable[p][k].r = palMax((int32_t)(pal->red * dMul + 0.5));
			pluginPalTable[p][k].g = palMax((int32_t)(pal->green * dMul + 0.5));
			pluginPalTable[p][k].b = palMax((int32_t)(pal->blue * dMul + 0.5));
		}
		palContrast[p][pal->colorNum - 4] = pal->contrast;
	} else {
		pal->contrast = 0;
		setScrollBarPos(inst, widgets, video, SB_PAL_R, pal->red, false);
		setScrollBarPos(inst, widgets, video, SB_PAL_G, pal->green, false);
		setScrollBarPos(inst, widgets, video, SB_PAL_B, pal->blue, false);
	}

	setScrollBarPos(inst, widgets, video, SB_PAL_CONTRAST, pal->contrast, false);
	setPal16(video, pluginPalTable[inst->config.palettePreset], true);
	drawCurrentPaletteColor(inst, video, &ui->bmp);

	/* Sync changes to config struct for persistence */
	for (int i = 0; i < 16; i++) {
		inst->config.userPalette[i][0] = pluginPalTable[PAL_USER_DEFINED][i].r;
		inst->config.userPalette[i][1] = pluginPalTable[PAL_USER_DEFINED][i].g;
		inst->config.userPalette[i][2] = pluginPalTable[PAL_USER_DEFINED][i].b;
	}
	inst->config.userPaletteContrast[0] = palContrast[PAL_USER_DEFINED][0];
	inst->config.userPaletteContrast[1] = palContrast[PAL_USER_DEFINED][1];
}

/* ---------- Scrollbar callbacks ---------- */

void sbPalRPos(ft2_instance_t *inst, uint32_t pos) {
	if (!inst || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	if (pal->red != (uint8_t)pos) { pal->red = (uint8_t)pos; paletteDragMoved(inst); }
}
void sbPalGPos(ft2_instance_t *inst, uint32_t pos) {
	if (!inst || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	if (pal->green != (uint8_t)pos) { pal->green = (uint8_t)pos; paletteDragMoved(inst); }
}
void sbPalBPos(ft2_instance_t *inst, uint32_t pos) {
	if (!inst || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	if (pal->blue != (uint8_t)pos) { pal->blue = (uint8_t)pos; paletteDragMoved(inst); }
}
void sbPalContrastPos(ft2_instance_t *inst, uint32_t pos) {
	if (!inst || !inst->ui) return;
	palette_editor_state_t *pal = PAL_ED(inst);
	if (pal->contrast != (uint8_t)pos) { pal->contrast = (uint8_t)pos; paletteDragMoved(inst); }
}

/* ---------- Pushbutton callbacks (RGB/Contrast +/-) ---------- */

#define PAL_BTN_HELPER(sb_id, fn) \
	if (!inst) return; \
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui; \
	if (!ui) return; \
	if (inst->config.palettePreset != PAL_USER_DEFINED) showColorErrorMsg(inst); \
	else fn(inst, &ui->widgets, &ui->video, sb_id, 1)

void configPalRDown(ft2_instance_t *inst)    { PAL_BTN_HELPER(SB_PAL_R, scrollBarScrollLeft); }
void configPalRUp(ft2_instance_t *inst)      { PAL_BTN_HELPER(SB_PAL_R, scrollBarScrollRight); }
void configPalGDown(ft2_instance_t *inst)    { PAL_BTN_HELPER(SB_PAL_G, scrollBarScrollLeft); }
void configPalGUp(ft2_instance_t *inst)      { PAL_BTN_HELPER(SB_PAL_G, scrollBarScrollRight); }
void configPalBDown(ft2_instance_t *inst)    { PAL_BTN_HELPER(SB_PAL_B, scrollBarScrollLeft); }
void configPalBUp(ft2_instance_t *inst)      { PAL_BTN_HELPER(SB_PAL_B, scrollBarScrollRight); }
void configPalContDown(ft2_instance_t *inst) { PAL_BTN_HELPER(SB_PAL_CONTRAST, scrollBarScrollLeft); }
void configPalContUp(ft2_instance_t *inst)   { PAL_BTN_HELPER(SB_PAL_CONTRAST, scrollBarScrollRight); }

#undef PAL_BTN_HELPER

/* ---------- Radio button callbacks - entry selection ---------- */

#define PAL_ENTRY_CB(num, rb_id) \
	if (!inst || !inst->ui) return; \
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui; \
	PAL_ED(inst)->colorNum = num; \
	checkRadioButtonNoRedraw(&ui->widgets, rb_id); \
	updatePaletteEditor(inst, &ui->video)

void rbConfigPalPatternText(ft2_instance_t *inst) { PAL_ENTRY_CB(0, RB_CONFIG_PAL_PATTEXT); }
void rbConfigPalBlockMark(ft2_instance_t *inst)   { PAL_ENTRY_CB(1, RB_CONFIG_PAL_BLOCKMARK); }
void rbConfigPalTextOnBlock(ft2_instance_t *inst) { PAL_ENTRY_CB(2, RB_CONFIG_PAL_TEXTONBLOCK); }
void rbConfigPalMouse(ft2_instance_t *inst)       { PAL_ENTRY_CB(3, RB_CONFIG_PAL_MOUSE); }
void rbConfigPalDesktop(ft2_instance_t *inst)     { PAL_ENTRY_CB(4, RB_CONFIG_PAL_DESKTOP); }
void rbConfigPalButtons(ft2_instance_t *inst)     { PAL_ENTRY_CB(5, RB_CONFIG_PAL_BUTTONS); }

#undef PAL_ENTRY_CB

/* ---------- Radio button callbacks - preset selection ---------- */

static void applyPalettePreset(ft2_instance_t *inst, uint8_t preset, uint16_t rbId)
{
	if (!inst) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (!ui) return;
	inst->config.palettePreset = preset;
	updatePaletteEditor(inst, &ui->video);
	setPal16(&ui->video, pluginPalTable[preset], true);
	checkRadioButtonNoRedraw(&ui->widgets, rbId);
	drawCurrentPaletteColor(inst, &ui->video, &ui->bmp);
}

void rbConfigPalArctic(ft2_instance_t *inst)        { applyPalettePreset(inst, PAL_ARCTIC, RB_CONFIG_PAL_ARCTIC); }
void rbConfigPalLitheDark(ft2_instance_t *inst)     { applyPalettePreset(inst, PAL_LITHE_DARK, RB_CONFIG_PAL_LITHE_DARK); }
void rbConfigPalAuroraBorealis(ft2_instance_t *inst){ applyPalettePreset(inst, PAL_AURORA_BOREALIS, RB_CONFIG_PAL_AURORA_BOREALIS); }
void rbConfigPalRose(ft2_instance_t *inst)          { applyPalettePreset(inst, PAL_ROSE, RB_CONFIG_PAL_ROSE); }
void rbConfigPalBlues(ft2_instance_t *inst)         { applyPalettePreset(inst, PAL_BLUES, RB_CONFIG_PAL_BLUES); }
void rbConfigPalDarkMode(ft2_instance_t *inst)      { applyPalettePreset(inst, PAL_DARK_MODE, RB_CONFIG_PAL_DARK_MODE); }
void rbConfigPalGold(ft2_instance_t *inst)          { applyPalettePreset(inst, PAL_GOLD, RB_CONFIG_PAL_GOLD); }
void rbConfigPalViolent(ft2_instance_t *inst)       { applyPalettePreset(inst, PAL_VIOLENT, RB_CONFIG_PAL_VIOLENT); }
void rbConfigPalHeavyMetal(ft2_instance_t *inst)    { applyPalettePreset(inst, PAL_HEAVY_METAL, RB_CONFIG_PAL_HEAVY_METAL); }
void rbConfigPalWhyColors(ft2_instance_t *inst)     { applyPalettePreset(inst, PAL_WHY_COLORS, RB_CONFIG_PAL_WHY_COLORS); }
void rbConfigPalJungle(ft2_instance_t *inst)        { applyPalettePreset(inst, PAL_JUNGLE, RB_CONFIG_PAL_JUNGLE); }
void rbConfigPalUserDefined(ft2_instance_t *inst)   { applyPalettePreset(inst, PAL_USER_DEFINED, RB_CONFIG_PAL_USER); }

/* ---------- Public API ---------- */

void showPaletteEditor(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!inst || !video || !bmp) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (!ui) return;
	ft2_widgets_t *widgets = &ui->widgets;

	/* RGB labels and sliders */
	charOutShadow(video, bmp, 503, 17, PAL_FORGRND, PAL_DSKTOP2, 'R');
	charOutShadow(video, bmp, 503, 31, PAL_FORGRND, PAL_DSKTOP2, 'G');
	charOutShadow(video, bmp, 503, 45, PAL_FORGRND, PAL_DSKTOP2, 'B');
	showScrollBar(widgets, video, SB_PAL_R);
	showScrollBar(widgets, video, SB_PAL_G);
	showScrollBar(widgets, video, SB_PAL_B);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_R_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_R_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_G_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_G_UP);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_B_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_B_UP);

	/* Entry selection */
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_PAL_ENTRIES);

	/* Contrast (only active for Desktop/Buttons entries) */
	textOutShadow(video, bmp, 516, 59, PAL_FORGRND, PAL_DSKTOP2, "Contrast:");
	showScrollBar(widgets, video, SB_PAL_CONTRAST);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_CONT_DOWN);
	showPushButton(widgets, video, bmp, PB_CONFIG_PAL_CONT_UP);

	/* Preset selection */
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_CONFIG_PAL_PRESET);

	/* Set radio button states */
	checkRadioButtonNoRedraw(widgets, RB_CONFIG_PAL_PATTEXT + PAL_ED(inst)->colorNum);
	
	uint16_t presetRbId;
	switch (inst->config.palettePreset) {
		case PAL_ARCTIC:          presetRbId = RB_CONFIG_PAL_ARCTIC; break;
		case PAL_AURORA_BOREALIS: presetRbId = RB_CONFIG_PAL_AURORA_BOREALIS; break;
		case PAL_BLUES:           presetRbId = RB_CONFIG_PAL_BLUES; break;
		case PAL_GOLD:            presetRbId = RB_CONFIG_PAL_GOLD; break;
		case PAL_HEAVY_METAL:     presetRbId = RB_CONFIG_PAL_HEAVY_METAL; break;
		case PAL_JUNGLE:          presetRbId = RB_CONFIG_PAL_JUNGLE; break;
		case PAL_LITHE_DARK:      presetRbId = RB_CONFIG_PAL_LITHE_DARK; break;
		case PAL_ROSE:            presetRbId = RB_CONFIG_PAL_ROSE; break;
		case PAL_DARK_MODE:       presetRbId = RB_CONFIG_PAL_DARK_MODE; break;
		case PAL_VIOLENT:         presetRbId = RB_CONFIG_PAL_VIOLENT; break;
		case PAL_WHY_COLORS:      presetRbId = RB_CONFIG_PAL_WHY_COLORS; break;
		case PAL_USER_DEFINED:    presetRbId = RB_CONFIG_PAL_USER; break;
		default:                  presetRbId = RB_CONFIG_PAL_DARK_MODE; break;
	}
	checkRadioButtonNoRedraw(widgets, presetRbId);

	updatePaletteEditor(inst, video);
	drawCurrentPaletteColor(inst, video, bmp);
}

