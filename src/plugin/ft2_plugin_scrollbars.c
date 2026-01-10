/*
** FT2 Plugin - Scrollbars
** Port of ft2_scrollbars.c with exact FT2 coordinates.
**
** Constant definitions (position, type, callback) in scrollBarsTemplate.
** Per-instance mutable state (visibility, position) in ft2_widgets_t.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_config.h"

#define FIXED_THUMB_SIZE 15
#define MIN_THUMB_SIZE 5

/* ------------------------------------------------------------------------- */
/*                      SCROLLBAR DEFINITIONS                                */
/* ------------------------------------------------------------------------- */

/* Template: {x, y, w, h, type, thumbType} - callbacks set in initScrollBars() */
const scrollBar_t scrollBarsTemplate[NUM_SCROLLBARS] =
{
	{ 0 }, { 0 }, { 0 },  /* Reserved */
	{ 55,  15,  18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },  /* Position editor */
	{ 566, 112, 18, 28, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },  /* Instrument switcher */
	{ 28,  385, 576,13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },  /* Pattern viewer */
	{ 611, 15,  18, 143,SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },  /* Help screen */
	{ 26,  331, 580,13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },  /* Sample editor */
	/* Instrument editor (vol, pan, ftune, fadeout, vibspeed, vibdepth, vibsweep) */
	{ 544, 175, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 189, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 203, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 220, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 234, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 248, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 262, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	/* Instrument editor extension (MIDI ch, prg, bend) */
	{ 195, 130, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 195, 144, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 195, 158, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	/* Config audio (output list, input list, amp, mastervol) */
	{ 365, 29,  18, 43, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },
	{ 365, 116, 18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },
	{ 272, 103, 105,13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 272, 131, 105,13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	/* Config layout (palette R, G, B, contrast) */
	{ 536, 15,  70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 29,  70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 43,  70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 71,  70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	/* Config MIDI (channel, transpose unused; sens) */
	{ 0,   0,   0,  0,  SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 0,   0,   0,  0,  SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 226, 98,  60, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	/* Disk op */
	{ 335, 15,  18, 143,SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE }
};

/* ------------------------------------------------------------------------- */
/*                         THUMB CALCULATION                                 */
/* ------------------------------------------------------------------------- */

/* Computes thumb position/size from scrollbar state (pos, end, page) */
static void setScrollBarThumbCoords(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0) state->page = 1;

	/* Uninitialized: fill entire track */
	if (state->end == 0)
	{
		state->thumbX = sb->x + 1;
		state->thumbY = sb->y + 1;
		state->thumbW = sb->w - 2;
		state->thumbH = sb->h - 2;
		return;
	}

	int16_t thumbX, thumbY, thumbW, thumbH, scrollEnd, originalThumbSize;
	int32_t length, end;
	double dTmp;

	if (sb->type == SCROLLBAR_HORIZONTAL)
	{
		thumbY = sb->y + 1;
		thumbH = sb->h - 2;
		scrollEnd = sb->x + sb->w;

		if (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
		{
			thumbW = originalThumbSize = FIXED_THUMB_SIZE;
			thumbX = (state->end > 0)
				? (int16_t)(sb->x + (int32_t)(((sb->w - originalThumbSize) / (double)state->end) * state->pos + 0.5))
				: sb->x;
		}
		else
		{
			originalThumbSize = (state->end > 0)
				? (int16_t)CLAMP((int32_t)((sb->w / (double)state->end) * state->page + 0.5), 1, sb->w)
				: 1;
			thumbW = (originalThumbSize < MIN_THUMB_SIZE) ? MIN_THUMB_SIZE : originalThumbSize;

			if (state->end > state->page)
			{
				length = sb->w - thumbW;
				end = state->end - state->page;
				thumbX = (int16_t)(sb->x + (int32_t)((length / (double)end) * state->pos + 0.5));
			}
			else
				thumbX = sb->x;
		}

		thumbX = CLAMP(thumbX, sb->x, scrollEnd - 1);
		if (thumbX + thumbW > scrollEnd) thumbW = scrollEnd - thumbX;
	}
	else
	{
		thumbX = sb->x + 1;
		thumbW = sb->w - 2;
		scrollEnd = sb->y + sb->h;

		originalThumbSize = (state->end > 0)
			? (int16_t)CLAMP((int32_t)((sb->h / (double)state->end) * state->page + 0.5), 1, sb->h)
			: 1;
		thumbH = (originalThumbSize < MIN_THUMB_SIZE) ? MIN_THUMB_SIZE : originalThumbSize;

		if (state->end > state->page)
		{
			length = sb->h - thumbH;
			end = state->end - state->page;
			thumbY = (int16_t)(sb->y + (int32_t)((length / (double)end) * state->pos + 0.5));
		}
		else
			thumbY = sb->y;

		thumbY = CLAMP(thumbY, sb->y, scrollEnd - 1);
		if (thumbY + thumbH > scrollEnd) thumbH = scrollEnd - thumbY;
	}

	state->thumbX = thumbX;
	state->thumbY = thumbY;
	state->thumbW = thumbW;
	state->thumbH = thumbH;
}

/* ------------------------------------------------------------------------- */
/*                          INITIALIZATION                                   */
/* ------------------------------------------------------------------------- */

void initScrollBars(ft2_widgets_t *widgets)
{
	if (!widgets) return;

	/* Wire up callbacks */
	widgets->scrollBars[SB_AMP_SCROLL].callbackFunc = sbAmpPos;
	widgets->scrollBars[SB_MASTERVOL_SCROLL].callbackFunc = sbMasterVolPos;
	widgets->scrollBars[SB_PAL_R].callbackFunc = sbPalRPos;
	widgets->scrollBars[SB_PAL_G].callbackFunc = sbPalGPos;
	widgets->scrollBars[SB_PAL_B].callbackFunc = sbPalBPos;
	widgets->scrollBars[SB_PAL_CONTRAST].callbackFunc = sbPalContrastPos;
	widgets->scrollBars[SB_MIDI_CHANNEL].callbackFunc = sbMidiChannel;
	widgets->scrollBars[SB_MIDI_TRANSPOSE].callbackFunc = sbMidiTranspose;
	widgets->scrollBars[SB_MIDI_SENS].callbackFunc = sbMidiSens;

	/* Initialize state */
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		widgets->scrollBarState[i].visible = false;
		widgets->scrollBarState[i].state = SCROLLBAR_UNPRESSED;
		widgets->scrollBarState[i].pos = 0;
		widgets->scrollBarState[i].page = 1;
		widgets->scrollBarState[i].end = 1;
	}

	/* Screen-specific defaults (page, end) */
	widgets->scrollBarState[SB_CHAN_SCROLL].page = 8;
	widgets->scrollBarState[SB_CHAN_SCROLL].end = 8;
	widgets->scrollBarState[SB_POS_ED].page = 5;
	widgets->scrollBarState[SB_POS_ED].end = 5;
	widgets->scrollBarState[SB_SAMPLE_LIST].page = 5;
	widgets->scrollBarState[SB_SAMPLE_LIST].end = 16;
	widgets->scrollBarState[SB_HELP_SCROLL].page = 15;
	widgets->scrollBarState[SB_HELP_SCROLL].end = 1;
	widgets->scrollBarState[SB_DISKOP_LIST].page = 15;
	widgets->scrollBarState[SB_DISKOP_LIST].end = 1;

	/* Config audio */
	widgets->scrollBarState[SB_AUDIO_OUTPUT_SCROLL].page = 6;
	widgets->scrollBarState[SB_AUDIO_OUTPUT_SCROLL].end = 1;
	widgets->scrollBarState[SB_AUDIO_INPUT_SCROLL].page = 4;
	widgets->scrollBarState[SB_AUDIO_INPUT_SCROLL].end = 1;
	widgets->scrollBarState[SB_AMP_SCROLL].page = 1;
	widgets->scrollBarState[SB_AMP_SCROLL].end = 31;
	widgets->scrollBarState[SB_MASTERVOL_SCROLL].page = 1;
	widgets->scrollBarState[SB_MASTERVOL_SCROLL].end = 256;

	/* Config palette */
	widgets->scrollBarState[SB_PAL_R].page = 1;
	widgets->scrollBarState[SB_PAL_R].end = 63;
	widgets->scrollBarState[SB_PAL_G].page = 1;
	widgets->scrollBarState[SB_PAL_G].end = 63;
	widgets->scrollBarState[SB_PAL_B].page = 1;
	widgets->scrollBarState[SB_PAL_B].end = 63;
	widgets->scrollBarState[SB_PAL_CONTRAST].page = 1;
	widgets->scrollBarState[SB_PAL_CONTRAST].end = 100;

	/* Config MIDI (channel 0-15, transpose 0-96 = -48..+48, sens 0-200) */
	widgets->scrollBarState[SB_MIDI_CHANNEL].page = 1;
	widgets->scrollBarState[SB_MIDI_CHANNEL].end = 15;
	widgets->scrollBarState[SB_MIDI_TRANSPOSE].page = 1;
	widgets->scrollBarState[SB_MIDI_TRANSPOSE].end = 96;
	widgets->scrollBarState[SB_MIDI_SENS].page = 1;
	widgets->scrollBarState[SB_MIDI_SENS].end = 200;

	/* Instrument editor */
	widgets->scrollBarState[SB_INST_VOL].page = 1;
	widgets->scrollBarState[SB_INST_VOL].end = 64;
	widgets->scrollBarState[SB_INST_PAN].page = 1;
	widgets->scrollBarState[SB_INST_PAN].end = 255;
	widgets->scrollBarState[SB_INST_FTUNE].page = 1;
	widgets->scrollBarState[SB_INST_FTUNE].end = 255;
	widgets->scrollBarState[SB_INST_FADEOUT].page = 1;
	widgets->scrollBarState[SB_INST_FADEOUT].end = 0xFFF;
	widgets->scrollBarState[SB_INST_VIBSPEED].page = 1;
	widgets->scrollBarState[SB_INST_VIBSPEED].end = 0x3F;
	widgets->scrollBarState[SB_INST_VIBDEPTH].page = 1;
	widgets->scrollBarState[SB_INST_VIBDEPTH].end = 0xF;
	widgets->scrollBarState[SB_INST_VIBSWEEP].page = 1;
	widgets->scrollBarState[SB_INST_VIBSWEEP].end = 0xFF;

	/* Instrument editor extension (MIDI ch 0-15, prg 0-127, bend 0-36) */
	widgets->scrollBarState[SB_INST_EXT_MIDI_CH].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_CH].end = 15;
	widgets->scrollBarState[SB_INST_EXT_MIDI_PRG].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_PRG].end = 127;
	widgets->scrollBarState[SB_INST_EXT_MIDI_BEND].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_BEND].end = 36;
}

/* ------------------------------------------------------------------------- */
/*                           DRAWING                                         */
/* ------------------------------------------------------------------------- */

void drawScrollBar(ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];
	if (!state->visible) return;

	setScrollBarThumbCoords(widgets, scrollBarID);

	int16_t thumbX = state->thumbX, thumbY = state->thumbY;
	int16_t thumbW = state->thumbW, thumbH = state->thumbH;

	clearRect(video, sb->x, sb->y, sb->w, sb->h);
	if (thumbW <= 0 || thumbH <= 0) return;

	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		fillRect(video, thumbX, thumbY, thumbW, thumbH, PAL_PATTEXT);
	}
	else
	{
		fillRect(video, thumbX, thumbY, thumbW, thumbH, PAL_BUTTONS);
		if (thumbW >= 2 && thumbH >= 3)
		{
			if (state->state == SCROLLBAR_UNPRESSED)
			{
				hLine(video, thumbX, thumbY, thumbW - 1, PAL_BUTTON1);
				vLine(video, thumbX, thumbY + 1, thumbH - 2, PAL_BUTTON1);
				hLine(video, thumbX, thumbY + thumbH - 1, thumbW - 1, PAL_BUTTON2);
				vLine(video, thumbX + thumbW - 1, thumbY, thumbH, PAL_BUTTON2);
			}
			else
			{
				hLine(video, thumbX, thumbY, thumbW, PAL_BUTTON2);
				vLine(video, thumbX, thumbY + 1, thumbH - 1, PAL_BUTTON2);
			}
		}
	}
}

void showScrollBar(ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;
	widgets->scrollBarState[scrollBarID].visible = true;
	drawScrollBar(widgets, video, scrollBarID);
}

void hideScrollBar(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;
	widgets->scrollBarState[scrollBarID].state = SCROLLBAR_UNPRESSED;
	widgets->scrollBarState[scrollBarID].visible = false;
}

/* ------------------------------------------------------------------------- */
/*                        POSITION CONTROL                                   */
/* ------------------------------------------------------------------------- */

void setScrollBarPos(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pos, bool triggerCallback)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0) { state->pos = 0; return; }

	if (state->end < state->page || state->pos == pos)
	{
		setScrollBarThumbCoords(widgets, scrollBarID);
		if (video) drawScrollBar(widgets, video, scrollBarID);
		return;
	}

	uint32_t endPos = state->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
		endPos = (endPos >= state->page) ? endPos - state->page : 0;

	state->pos = (pos > endPos) ? endPos : pos;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video) drawScrollBar(widgets, video, scrollBarID);
	if (triggerCallback && sb->callbackFunc) sb->callbackFunc(inst, state->pos);
}

uint32_t getScrollBarPos(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return 0;
	return widgets->scrollBarState[scrollBarID].pos;
}

void setScrollBarEnd(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t end)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];
	if (end < 1) end = 1;
	state->end = end;

	bool setPos = (state->pos >= end);
	if (setPos) state->pos = end - 1;

	if (state->page > 0)
	{
		if (setPos) setScrollBarPos(inst, widgets, video, scrollBarID, state->pos, false);
		else { setScrollBarThumbCoords(widgets, scrollBarID); if (video) drawScrollBar(widgets, video, scrollBarID); }
	}
}

void setScrollBarPageLength(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pageLength)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];
	if (pageLength < 1) pageLength = 1;
	state->page = pageLength;

	if (state->end > 0)
	{
		setScrollBarPos(inst, widgets, video, scrollBarID, state->pos, false);
		setScrollBarThumbCoords(widgets, scrollBarID);
		if (video) drawScrollBar(widgets, video, scrollBarID);
	}
}

/* ------------------------------------------------------------------------- */
/*                        SCROLL OPERATIONS                                  */
/* ------------------------------------------------------------------------- */

void scrollBarScrollUp(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0 || state->end == 0 || state->end < state->page || state->pos == 0) return;

	state->pos = (state->pos >= amount) ? state->pos - amount : 0;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video) drawScrollBar(widgets, video, scrollBarID);
	if (sb->callbackFunc) sb->callbackFunc(inst, state->pos);
}

void scrollBarScrollDown(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	if (!widgets || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0 || state->end == 0 || state->end < state->page) return;

	uint32_t endPos = state->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
		endPos = (endPos >= state->page) ? endPos - state->page : 0;

	if (state->pos == endPos) return;

	state->pos += amount;
	if (state->pos > endPos) state->pos = endPos;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video) drawScrollBar(widgets, video, scrollBarID);
	if (sb->callbackFunc) sb->callbackFunc(inst, state->pos);
}

void scrollBarScrollLeft(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{ scrollBarScrollUp(inst, widgets, video, scrollBarID, amount); }

void scrollBarScrollRight(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{ scrollBarScrollDown(inst, widgets, video, scrollBarID, amount); }

/* ------------------------------------------------------------------------- */
/*                        MOUSE HANDLING                                     */
/* ------------------------------------------------------------------------- */

/* Returns scrollbar ID if hit, -1 otherwise. Sets up drag tracking. */
int16_t testScrollBarMouseDown(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	if (!widgets) return -1;

	uint16_t start = sysReqShown ? 0 : 3;
	uint16_t end = sysReqShown ? 3 : NUM_SCROLLBARS;

	for (uint16_t i = start; i < end; i++)
	{
		scrollBar_t *sb = &widgets->scrollBars[i];
		ft2_scrollbar_state_t *state = &widgets->scrollBarState[i];

		if (!state->visible || widgets->scrollBarDisabled[i]) continue;
		if (mouseX < sb->x || mouseX >= sb->x + sb->w || mouseY < sb->y || mouseY >= sb->y + sb->h) continue;

		state->state = SCROLLBAR_PRESSED;
		widgets->mouse.scrollLastX = mouseX;
		widgets->mouse.scrollLastY = mouseY;

		if (sb->type == SCROLLBAR_HORIZONTAL)
		{
			if (mouseX >= state->thumbX && mouseX < state->thumbX + state->thumbW)
				widgets->mouse.scrollBias = mouseX - state->thumbX;
			else
			{
				widgets->mouse.scrollBias = state->thumbW >> 1;
				int32_t scrollPos = CLAMP(mouseX - widgets->mouse.scrollBias - sb->x, 0, (int32_t)sb->w);
				int32_t length = (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE) ? sb->w - state->thumbW
					: sb->w + ((state->end > 0 ? (int16_t)CLAMP((int32_t)((sb->w / (double)state->end) * state->page + 0.5), 1, sb->w) : 1) - state->thumbW);
				if (length < 1) length = 1;
				setScrollBarPos(inst, widgets, video, i, (uint32_t)(((double)scrollPos * state->end) / length + 0.5), true);
			}
		}
		else
		{
			if (mouseY >= state->thumbY && mouseY < state->thumbY + state->thumbH)
				widgets->mouse.scrollBias = mouseY - state->thumbY;
			else
			{
				widgets->mouse.scrollBias = state->thumbH >> 1;
				int32_t scrollPos = CLAMP(mouseY - widgets->mouse.scrollBias - sb->y, 0, (int32_t)sb->h);
				int16_t origThumb = (state->end > 0) ? (int16_t)CLAMP((int32_t)((sb->h / (double)state->end) * state->page + 0.5), 1, sb->h) : 1;
				int32_t length = sb->h + (origThumb - state->thumbH);
				if (length < 1) length = 1;
				setScrollBarPos(inst, widgets, video, i, (uint32_t)(((double)scrollPos * state->end) / length + 0.5), true);
			}
		}
		return (int16_t)i;
	}
	return -1;
}

void testScrollBarMouseRelease(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video, int16_t lastScrollBarID)
{
	if (!widgets || lastScrollBarID < 0 || lastScrollBarID >= NUM_SCROLLBARS) return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[lastScrollBarID];
	if (state->visible)
	{
		state->state = SCROLLBAR_UNPRESSED;
		if (video) drawScrollBar(widgets, video, lastScrollBarID);
	}
	resetPaletteErrorFlag(inst);
}

/* Tracks thumb drag - only updates when mouse moves along relevant axis */
void handleScrollBarWhileMouseDown(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, int16_t scrollBarID)
{
	if (!widgets || scrollBarID < 0 || scrollBarID >= NUM_SCROLLBARS) return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];
	if (!state->visible) return;

	if (sb->type == SCROLLBAR_HORIZONTAL)
	{
		if (mouseX == widgets->mouse.scrollLastX) return;
		widgets->mouse.scrollLastX = mouseX;

		int32_t scrollPos = CLAMP(mouseX - widgets->mouse.scrollBias - sb->x, 0, (int32_t)sb->w);
		int32_t length = (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE) ? sb->w - state->thumbW
			: sb->w + ((state->end > 0 ? (int16_t)CLAMP((int32_t)((sb->w / (double)state->end) * state->page + 0.5), 1, sb->w) : 1) - state->thumbW);
		if (length < 1) length = 1;

		setScrollBarPos(inst, widgets, video, scrollBarID, (uint32_t)(((double)scrollPos * state->end) / length + 0.5), true);
		if (video) drawScrollBar(widgets, video, scrollBarID);
	}
	else
	{
		if (mouseY == widgets->mouse.scrollLastY) return;
		widgets->mouse.scrollLastY = mouseY;

		int32_t scrollPos = CLAMP(mouseY - widgets->mouse.scrollBias - sb->y, 0, (int32_t)sb->h);
		int16_t origThumb = (state->end > 0) ? (int16_t)CLAMP((int32_t)((sb->h / (double)state->end) * state->page + 0.5), 1, sb->h) : 1;
		int32_t length = sb->h + (origThumb - state->thumbH);
		if (length < 1) length = 1;

		setScrollBarPos(inst, widgets, video, scrollBarID, (uint32_t)(((double)scrollPos * state->end) / length + 0.5), true);
		if (video) drawScrollBar(widgets, video, scrollBarID);
	}
}
