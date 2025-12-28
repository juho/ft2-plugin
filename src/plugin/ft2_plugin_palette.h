/**
 * @file ft2_plugin_palette.h
 * @brief Palette/color scheme handling for the FT2 plugin.
 * 
 * Ported from ft2_palette.c in the standalone version.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

/* Palette color indices and presets are defined in ft2_plugin_video.h and ft2_plugin_config.h */
#define PAL_NUM_PRESETS 12

/* RGB conversion macros */
#define RGB32_R(x) (((x) >> 16) & 0xFF)
#define RGB32_G(x) (((x) >> 8) & 0xFF)
#define RGB32_B(x) ((x) & 0xFF)
#define RGB32(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define P6_TO_P8(x) (((x) << 2) | ((x) >> 4))

/* Packed palette color structure (18-bit VGA RGB) */
#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct pal16_t
{
	uint8_t r, g, b;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pal16;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/* Palette table - 12 presets, 16 colors each */
extern pal16 pluginPalTable[12][16];

/* Current selected color entry (0-5) */
extern uint8_t cfg_ColorNum;

/**
 * @brief Clamp a color value to valid 6-bit range (0-63).
 */
uint8_t palMax(int32_t c);

/**
 * @brief Apply a 16-color palette to the video system.
 * @param video The video context.
 * @param p The palette to apply.
 * @param redrawScreen If true, update the framebuffer with new colors.
 */
void setPal16(struct ft2_video_t *video, pal16 *p, bool redrawScreen);

/**
 * @brief Show the palette editor widgets and initialize values.
 */
void showPaletteEditor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * @brief Update the palette editor sliders to match current palette color.
 */
void updatePaletteEditor(struct ft2_instance_t *inst, struct ft2_video_t *video);

/**
 * @brief Draw the current palette color hex value and preview box.
 */
void drawCurrentPaletteColor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Scrollbar position callbacks */
void sbPalRPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalGPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalBPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalContrastPos(struct ft2_instance_t *inst, uint32_t pos);

/**
 * @brief Reset the palette error flag when mouse is released.
 * Call this on mouse up to allow showing the error message again on the next click.
 */
void resetPaletteErrorFlag(void);

/* Pushbutton callbacks for R/G/B/Contrast up/down */
void configPalRDown(struct ft2_instance_t *inst);
void configPalRUp(struct ft2_instance_t *inst);
void configPalGDown(struct ft2_instance_t *inst);
void configPalGUp(struct ft2_instance_t *inst);
void configPalBDown(struct ft2_instance_t *inst);
void configPalBUp(struct ft2_instance_t *inst);
void configPalContDown(struct ft2_instance_t *inst);
void configPalContUp(struct ft2_instance_t *inst);

/* Radio button callbacks for palette entry selection */
void rbConfigPalPatternText(struct ft2_instance_t *inst);
void rbConfigPalBlockMark(struct ft2_instance_t *inst);
void rbConfigPalTextOnBlock(struct ft2_instance_t *inst);
void rbConfigPalMouse(struct ft2_instance_t *inst);
void rbConfigPalDesktop(struct ft2_instance_t *inst);
void rbConfigPalButtons(struct ft2_instance_t *inst);

/* Radio button callbacks for palette preset selection */
void rbConfigPalArctic(struct ft2_instance_t *inst);
void rbConfigPalLitheDark(struct ft2_instance_t *inst);
void rbConfigPalAuroraBorealis(struct ft2_instance_t *inst);
void rbConfigPalRose(struct ft2_instance_t *inst);
void rbConfigPalBlues(struct ft2_instance_t *inst);
void rbConfigPalDarkMode(struct ft2_instance_t *inst);
void rbConfigPalGold(struct ft2_instance_t *inst);
void rbConfigPalViolent(struct ft2_instance_t *inst);
void rbConfigPalHeavyMetal(struct ft2_instance_t *inst);
void rbConfigPalWhyColors(struct ft2_instance_t *inst);
void rbConfigPalJungle(struct ft2_instance_t *inst);
void rbConfigPalUserDefined(struct ft2_instance_t *inst);

