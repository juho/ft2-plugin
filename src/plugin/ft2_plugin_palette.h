/**
 * @file ft2_plugin_palette.h
 * @brief Palette editor and color scheme management.
 *
 * 12 preset palettes + user-defined. 16 colors per palette in 18-bit VGA format.
 * Editable entries: PatternText, BlockMark, TextOnBlock, Mouse, Desktop, Buttons.
 * Desktop/Buttons have contrast scaling for derived UI shades.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

#define PAL_NUM_PRESETS 12

/* RGB conversion: 6-bit VGA <-> 8-bit, pack/unpack 32-bit */
#define RGB32_R(x) (((x) >> 16) & 0xFF)
#define RGB32_G(x) (((x) >> 8) & 0xFF)
#define RGB32_B(x) ((x) & 0xFF)
#define RGB32(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define P6_TO_P8(x) (((x) << 2) | ((x) >> 4)) /* 6-bit to 8-bit expansion */

/* 18-bit VGA color (0-63 per channel) */
#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct pal16_t { uint8_t r, g, b; }
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pal16;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

extern pal16 pluginPalTable[12][16]; /* [preset][color] */
extern uint8_t cfg_ColorNum;          /* Current editor entry (0-5) */

/* Core functions */
uint8_t palMax(int32_t c); /* Clamp to 0-63 */
void setPal16(struct ft2_video_t *video, pal16 *p, bool redrawScreen);

/* Editor UI */
void showPaletteEditor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void updatePaletteEditor(struct ft2_instance_t *inst, struct ft2_video_t *video);
void drawCurrentPaletteColor(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void resetPaletteErrorFlag(void); /* Call on mouse up to re-enable error dialog */

/* Scrollbar callbacks */
void sbPalRPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalGPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalBPos(struct ft2_instance_t *inst, uint32_t pos);
void sbPalContrastPos(struct ft2_instance_t *inst, uint32_t pos);

/* RGB/Contrast +/- buttons */
void configPalRDown(struct ft2_instance_t *inst);
void configPalRUp(struct ft2_instance_t *inst);
void configPalGDown(struct ft2_instance_t *inst);
void configPalGUp(struct ft2_instance_t *inst);
void configPalBDown(struct ft2_instance_t *inst);
void configPalBUp(struct ft2_instance_t *inst);
void configPalContDown(struct ft2_instance_t *inst);
void configPalContUp(struct ft2_instance_t *inst);

/* Entry selection radio buttons */
void rbConfigPalPatternText(struct ft2_instance_t *inst);
void rbConfigPalBlockMark(struct ft2_instance_t *inst);
void rbConfigPalTextOnBlock(struct ft2_instance_t *inst);
void rbConfigPalMouse(struct ft2_instance_t *inst);
void rbConfigPalDesktop(struct ft2_instance_t *inst);
void rbConfigPalButtons(struct ft2_instance_t *inst);

/* Preset selection radio buttons */
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

