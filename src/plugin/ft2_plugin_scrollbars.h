/*
** FT2 Plugin - Scrollbars API
** Constant definitions + per-instance mutable state in ft2_widgets_t.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct ft2_video_t;
struct ft2_instance_t;
struct ft2_widgets_t;

/* Scrollbar IDs - order matches scrollBarsTemplate[] */
enum
{
	SB_RES_1, SB_RES_2, SB_RES_3,  /* Reserved */
	SB_POS_ED,                     /* Position editor */
	SB_SAMPLE_LIST,                /* Instrument switcher */
	SB_CHAN_SCROLL,                /* Pattern viewer */
	SB_HELP_SCROLL,                /* Help screen */
	SB_SAMP_SCROLL,                /* Sample editor */
	SB_INST_VOL, SB_INST_PAN, SB_INST_FTUNE, SB_INST_FADEOUT,
	SB_INST_VIBSPEED, SB_INST_VIBDEPTH, SB_INST_VIBSWEEP,  /* Instrument editor */
	SB_INST_EXT_MIDI_CH, SB_INST_EXT_MIDI_PRG, SB_INST_EXT_MIDI_BEND,  /* Inst editor ext */
	SB_AUDIO_OUTPUT_SCROLL, SB_AUDIO_INPUT_SCROLL, SB_AMP_SCROLL, SB_MASTERVOL_SCROLL,  /* Config audio */
	SB_PAL_R, SB_PAL_G, SB_PAL_B, SB_PAL_CONTRAST,  /* Config layout */
	SB_MIDI_CHANNEL, SB_MIDI_TRANSPOSE, SB_MIDI_SENS,  /* Config MIDI */
	SB_DISKOP_LIST,                /* Disk op */
	NUM_SCROLLBARS
};

/* Scrollbar type/state constants */
enum
{
	SCROLLBAR_UNPRESSED = 0, SCROLLBAR_PRESSED = 1,
	SCROLLBAR_HORIZONTAL = 0, SCROLLBAR_VERTICAL = 1,
	SCROLLBAR_FIXED_THUMB_SIZE = 0, SCROLLBAR_DYNAMIC_THUMB_SIZE = 1
};

typedef void (*sbCallback_t)(struct ft2_instance_t *inst, uint32_t pos);

/* Constant definition (mutable state in ft2_widgets_t) */
typedef struct scrollBar_t
{
	uint16_t x, y, w, h;       /* Position and size */
	uint8_t type, thumbType;   /* HORIZONTAL/VERTICAL, FIXED/DYNAMIC */
	sbCallback_t callbackFunc; /* Position change callback */
} scrollBar_t;

extern const scrollBar_t scrollBarsTemplate[NUM_SCROLLBARS];

/* Initialization */
void initScrollBars(struct ft2_widgets_t *widgets);

/* Visibility */
void drawScrollBar(struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID);
void showScrollBar(struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID);
void hideScrollBar(struct ft2_widgets_t *widgets, uint16_t scrollBarID);

/* Position control */
void setScrollBarPos(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t pos, bool triggerCallback);
uint32_t getScrollBarPos(struct ft2_widgets_t *widgets, uint16_t scrollBarID);
void setScrollBarEnd(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t end);
void setScrollBarPageLength(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t pageLength);

/* Scroll operations */
void scrollBarScrollUp(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollDown(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollLeft(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollRight(struct ft2_instance_t *inst, struct ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount);

/* Mouse handling */
int16_t testScrollBarMouseDown(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, int32_t mouseX, int32_t mouseY, bool sysReqShown);
void testScrollBarMouseRelease(struct ft2_widgets_t *widgets, struct ft2_video_t *video, int16_t lastScrollBarID);
void handleScrollBarWhileMouseDown(struct ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, int32_t mouseX, int32_t mouseY, int16_t scrollBarID);
