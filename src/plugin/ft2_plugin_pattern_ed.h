/**
 * @file ft2_plugin_pattern_ed.h
 * @brief Pattern editor rendering and editing.
 *
 * Handles pattern display (notes/instr/vol/effects), cursor navigation, block marking,
 * copy/paste, transpose, instrument remap, and extended pattern editor mode.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

/* Pattern constants */
#define MAX_PATT_LEN 256
#define NOTE_OFF 97
#define FT2_MAX_PATTERNS 256

/* Pattern font types */
#define FONT_TYPE3 0  /* Small (4px wide) */
#define FONT_TYPE4 1  /* Medium (8px wide) */
#define FONT_TYPE5 2  /* Big (16px wide) */
#define FONT_TYPE7 3  /* Tiny (6px wide) */

/* Font dimensions - must match ft2_bmp.h */
#define FONT3_CHAR_W 4
#define FONT3_CHAR_H 7
#define FONT3_WIDTH 172
#define FONT4_CHAR_W 8
#define FONT4_CHAR_H 8
#define FONT4_WIDTH 624
#define FONT5_CHAR_W 16
#define FONT5_CHAR_H 8
#define FONT5_WIDTH 624
#define FONT7_CHAR_W 6
#define FONT7_CHAR_H 7
#define FONT7_WIDTH 140

/* Pattern coordinate lookup structures */
typedef struct pattCoord_t {
	uint16_t upperRowsY, lowerRowsY;
	uint16_t upperRowsTextY, midRowTextY, lowerRowsTextY;
	uint16_t numUpperRows, numLowerRows;
} pattCoord_t;

typedef struct pattCoord2_t {
	uint16_t upperRowsY, lowerRowsY;
	uint16_t upperRowsH, lowerRowsH;
} pattCoord2_t;

typedef struct markCoord_t {
	uint16_t upperRowsY, midRowY, lowerRowsY;
} markCoord_t;

typedef struct pattMark_t {
	int16_t markX1, markX2, markY1, markY2;
} pattMark_t;

typedef struct cursor_t {
	uint8_t ch;      /* Current channel */
	uint8_t object;  /* Cursor field (NOTE/INSTR/VOL/EFX) */
} cursor_t;

/* Forward declaration for block buffer */
struct ft2_note_t;

/* Block clipboard state */
typedef struct {
	bool blockCopied;
	int32_t markXSize, markYSize;
	struct ft2_note_t *blkCopyBuff;      /* Allocated: MAX_PATT_LEN * MAX_CHANNELS */
	int32_t lastMouseX, lastMouseY;
	int8_t lastChMark;
	int16_t lastRowMark;
	int16_t lastMarkX1, lastMarkX2, lastMarkY1, lastMarkY2;
} patt_clipboard_t;

/* Pattern editor state */
typedef struct ft2_pattern_editor_t {
	struct ft2_video_t *video;
	int16_t currRow, currPattern, channelOffset;
	uint8_t numChannelsShown, maxVisibleChannels;
	uint16_t patternChannelWidth;
	cursor_t cursor;
	int16_t ptnCursorY;

	/* Display options */
	bool ptnStretch;            /* Row height stretch (11px vs 8px) */
	bool ptnChanScrollShown;    /* Horizontal scrollbar visible */
	bool ptnShowVolColumn;      /* Volume column visible */
	bool ptnHex;                /* Hex row numbers */
	bool ptnLineLight;          /* Highlight every 4th row */
	bool ptnChnNumbers;         /* Show channel numbers */
	bool ptnInstrZero;          /* Show leading zeros for instruments */
	bool ptnAcc;                /* Flat notation (vs sharp) */
	bool ptnFrmWrk;             /* Draw framework */
	uint8_t ptnFont;            /* Font style 0-3 */
	bool extendedPatternEditor; /* Extended mode */

	pattMark_t pattMark;
	const uint8_t *font4Ptr, *font5Ptr;
	
	/* Block clipboard (per-instance) */
	patt_clipboard_t clipboard;
} ft2_pattern_editor_t;

/* Transpose modes */
enum {
	TRANSP_TRACK = 0, TRANSP_PATT = 1, TRANSP_SONG = 2, TRANSP_BLOCK = 3,
	TRANSP_CUR_INSTRUMENT = false, TRANSP_ALL_INSTRUMENTS = true
};

/* Init/drawing */
void ft2_pattern_ed_init(ft2_pattern_editor_t *editor, struct ft2_video_t *video);
void ft2_pattern_ed_update_font_ptrs(ft2_pattern_editor_t *editor, const struct ft2_bmp_t *bmp);
void ft2_pattern_ed_draw_borders(ft2_pattern_editor_t *editor, const struct ft2_bmp_t *bmp);
void ft2_pattern_ed_write_pattern(ft2_pattern_editor_t *editor, const struct ft2_bmp_t *bmp, struct ft2_instance_t *inst);
void ft2_pattern_ed_draw(ft2_pattern_editor_t *editor, const struct ft2_bmp_t *bmp, struct ft2_instance_t *instance);

/* Visibility */
void showPatternEditor(struct ft2_instance_t *inst);
void hidePatternEditor(struct ft2_instance_t *inst);

/* Extended mode */
void updatePatternEditorGUI(struct ft2_instance_t *inst);
void patternEditorExtended(struct ft2_instance_t *inst);
void exitPatternEditorExtended(struct ft2_instance_t *inst);
void togglePatternEditorExtended(struct ft2_instance_t *inst);

/* Advanced edit */
void setAdvEditCheckBoxes(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void updateAdvEdit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void drawAdvEdit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void showAdvEdit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideAdvEdit(struct ft2_instance_t *inst);
void toggleAdvEdit(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Instrument remap */
void remapTrack(struct ft2_instance_t *inst);
void remapPattern(struct ft2_instance_t *inst);
void remapSong(struct ft2_instance_t *inst);
void remapBlock(struct ft2_instance_t *inst);

/* Mask toggle callbacks (Adv.Edit checkboxes) */
void cbEnableMasking(struct ft2_instance_t *inst);
void cbCopyMask0(struct ft2_instance_t *inst);
void cbCopyMask1(struct ft2_instance_t *inst);
void cbCopyMask2(struct ft2_instance_t *inst);
void cbCopyMask3(struct ft2_instance_t *inst);
void cbCopyMask4(struct ft2_instance_t *inst);
void cbPasteMask0(struct ft2_instance_t *inst);
void cbPasteMask1(struct ft2_instance_t *inst);
void cbPasteMask2(struct ft2_instance_t *inst);
void cbPasteMask3(struct ft2_instance_t *inst);
void cbPasteMask4(struct ft2_instance_t *inst);
void cbTranspMask0(struct ft2_instance_t *inst);
void cbTranspMask1(struct ft2_instance_t *inst);
void cbTranspMask2(struct ft2_instance_t *inst);
void cbTranspMask3(struct ft2_instance_t *inst);
void cbTranspMask4(struct ft2_instance_t *inst);

/* Transpose */
void drawTranspose(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideTranspose(struct ft2_instance_t *inst);
void showTranspose(struct ft2_instance_t *inst);
void toggleTranspose(struct ft2_instance_t *inst);
void doTranspose(struct ft2_instance_t *inst, uint8_t mode, int8_t addValue, bool allInstrumentsFlag);

/* Cursor navigation */
void cursorLeft(struct ft2_instance_t *inst);
void cursorRight(struct ft2_instance_t *inst);
void cursorTabLeft(struct ft2_instance_t *inst);
void cursorTabRight(struct ft2_instance_t *inst);
void cursorChannelLeft(struct ft2_instance_t *inst);
void cursorChannelRight(struct ft2_instance_t *inst);
void chanLeft(struct ft2_instance_t *inst);
void chanRight(struct ft2_instance_t *inst);

/* Row navigation */
void rowOneUpWrap(struct ft2_instance_t *inst);
void rowOneDownWrap(struct ft2_instance_t *inst);
void rowUp(struct ft2_instance_t *inst, uint16_t amount);
void rowDown(struct ft2_instance_t *inst, uint16_t amount);

/* Block marking */
void clearPattMark(struct ft2_instance_t *inst);
void checkMarkLimits(struct ft2_instance_t *inst);
void keybPattMarkUp(struct ft2_instance_t *inst);
void keybPattMarkDown(struct ft2_instance_t *inst);
void keybPattMarkLeft(struct ft2_instance_t *inst);
void keybPattMarkRight(struct ft2_instance_t *inst);

/* Block operations */
void cutBlock(struct ft2_instance_t *inst);
void copyBlock(struct ft2_instance_t *inst);
void pasteBlock(struct ft2_instance_t *inst);
void handlePatternDataMouseDown(struct ft2_instance_t *inst, int32_t mouseX, int32_t mouseY, bool mouseButtonHeld, bool rightButton);

/* Channel scroll */
void scrollChannelLeft(struct ft2_instance_t *inst);
void scrollChannelRight(struct ft2_instance_t *inst);
void setChannelScrollPos(struct ft2_instance_t *inst, uint32_t pos);
void jumpToChannel(struct ft2_instance_t *inst, uint8_t chNr);

/* Pattern memory */
bool allocatePattern(struct ft2_instance_t *inst, uint16_t pattNum);
void killPatternIfUnused(struct ft2_instance_t *inst, uint16_t pattNum);
bool patternEmpty(struct ft2_instance_t *inst, uint16_t pattNum);
void updatePatternWidth(struct ft2_instance_t *inst);
uint8_t getMaxVisibleChannels(struct ft2_instance_t *inst);
void updateChanNums(struct ft2_instance_t *inst);

#ifdef __cplusplus
}
#endif
