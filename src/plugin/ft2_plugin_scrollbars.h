/**
 * @file ft2_plugin_scrollbars.h
 * @brief Scrollbar definitions for the FT2 plugin UI.
 * 
 * Ported from ft2_scrollbars.h - all coordinates preserved exactly.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_video_t;
struct ft2_instance_t;

enum
{
	/* Reserved scrollbars */
	SB_RES_1,
	SB_RES_2,
	SB_RES_3,

	/* Position editor */
	SB_POS_ED,

	/* Instrument switcher */
	SB_SAMPLE_LIST,

	/* Pattern viewer */
	SB_CHAN_SCROLL,

	/* Help screen */
	SB_HELP_SCROLL,

	/* Sample editor */
	SB_SAMP_SCROLL,

	/* Instrument editor */
	SB_INST_VOL,
	SB_INST_PAN,
	SB_INST_FTUNE,
	SB_INST_FADEOUT,
	SB_INST_VIBSPEED,
	SB_INST_VIBDEPTH,
	SB_INST_VIBSWEEP,

	/* Instrument editor extension */
	SB_INST_EXT_MIDI_CH,
	SB_INST_EXT_MIDI_PRG,
	SB_INST_EXT_MIDI_BEND,

	/* Config audio */
	SB_AUDIO_OUTPUT_SCROLL,
	SB_AUDIO_INPUT_SCROLL,
	SB_AMP_SCROLL,
	SB_MASTERVOL_SCROLL,

	/* Config layout */
	SB_PAL_R,
	SB_PAL_G,
	SB_PAL_B,
	SB_PAL_CONTRAST,

	/* Config miscellaneous */
	SB_MIDI_SENS,

	/* Disk op */
	SB_DISKOP_LIST,

	NUM_SCROLLBARS
};

enum
{
	SCROLLBAR_UNPRESSED = 0,
	SCROLLBAR_PRESSED = 1,
	SCROLLBAR_HORIZONTAL = 0,
	SCROLLBAR_VERTICAL = 1,
	SCROLLBAR_FIXED_THUMB_SIZE = 0,
	SCROLLBAR_DYNAMIC_THUMB_SIZE = 1
};

typedef void (*sbCallback_t)(struct ft2_instance_t *inst, uint32_t pos);

typedef struct scrollBar_t
{
	uint16_t x, y, w, h;
	uint8_t type, thumbType;
	sbCallback_t callbackFunc;

	bool visible;
	uint8_t state;
	uint32_t pos, page, end;
	uint16_t thumbX, thumbY, thumbW, thumbH, originalThumbSize;
} scrollBar_t;

extern scrollBar_t scrollBars[NUM_SCROLLBARS];

/**
 * Initialize scrollbars array.
 */
void initScrollBars(void);

/**
 * Draw a scrollbar.
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 */
void drawScrollBar(struct ft2_video_t *video, uint16_t scrollBarID);

/**
 * Show a scrollbar.
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 */
void showScrollBar(struct ft2_video_t *video, uint16_t scrollBarID);

/**
 * Hide a scrollbar.
 * @param scrollBarID Scrollbar ID
 */
void hideScrollBar(uint16_t scrollBarID);

/**
 * Set scrollbar position.
 * @param inst FT2 instance
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 * @param pos Position
 * @param triggerCallback Whether to trigger callback
 */
void setScrollBarPos(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pos, bool triggerCallback);

/**
 * Get scrollbar position.
 * @param scrollBarID Scrollbar ID
 * @return Position
 */
uint32_t getScrollBarPos(uint16_t scrollBarID);

/**
 * Set scrollbar end value.
 * @param inst FT2 instance
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 * @param end End value
 */
void setScrollBarEnd(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t end);

/**
 * Set scrollbar page length.
 * @param inst FT2 instance
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 * @param pageLength Page length
 */
void setScrollBarPageLength(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pageLength);

/**
 * Scroll the scrollbar up/left by the specified amount.
 * @param inst FT2 instance
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 * @param amount Amount to scroll
 */
void scrollBarScrollUp(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount);

/**
 * Scroll the scrollbar down/right by the specified amount.
 * @param inst FT2 instance
 * @param video Video context
 * @param scrollBarID Scrollbar ID
 * @param amount Amount to scroll
 */
void scrollBarScrollDown(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount);

/**
 * Alias for scrollBarScrollUp (for horizontal scrollbars).
 */
void scrollBarScrollLeft(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount);

/**
 * Alias for scrollBarScrollDown (for horizontal scrollbars).
 */
void scrollBarScrollRight(struct ft2_instance_t *inst, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount);

/**
 * Test if mouse click is on a scrollbar.
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param sysReqShown Whether system request is shown
 * @return Scrollbar ID if clicked, -1 otherwise
 */
int16_t testScrollBarMouseDown(struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, bool sysReqShown);

/**
 * Handle scrollbar mouse release.
 * @param video Video context
 * @param lastScrollBarID Last scrollbar that was pressed
 */
void testScrollBarMouseRelease(struct ft2_video_t *video, int16_t lastScrollBarID);

/**
 * Handle continuous scrollbar interaction while mouse is held down.
 * @param inst FT2 instance
 * @param video Video context
 * @param mouseX Mouse X coordinate
 * @param mouseY Mouse Y coordinate
 * @param scrollBarID Scrollbar ID being held
 */
void handleScrollBarWhileMouseDown(struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, int16_t scrollBarID);

