/**
 * @file ft2_plugin_scrollbars.c
 * @brief Scrollbar implementation for the FT2 plugin UI.
 * 
 * Ported from ft2_scrollbars.c - exact coordinates preserved.
 * Static global array contains constant definitions (position, callbacks).
 * Per-instance state (visibility, position) is stored in ft2_widgets_t.
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

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* Track where user clicked on the thumb for smooth dragging */
static int32_t lastMouseX, lastMouseY, scrollBias;

/* Constant scrollbar definitions (position, size, type, callback) - template for per-instance copy */
const scrollBar_t scrollBarsTemplate[NUM_SCROLLBARS] =
{
	/* Reserved scrollbars */
	{ 0 }, { 0 }, { 0 },

	/* Position editor */
	/*x,  y,  w,  h,  type,               style */
	{ 55, 15, 18, 21, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },

	/* Instrument switcher */
	/*x,   y,   w,  h,  type,               style */
	{ 566, 112, 18, 28, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },

	/* Pattern viewer */
	/*x,  y,   w,   h,  type,                 style */
	{ 28, 385, 576, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },

	/* Help screen */
	/*x,   y,  w,  h,   type,               style */
	{ 611, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },

	/* Sample editor */
	/*x,  y,   w,   h,  type,                 style */
	{ 26, 331, 580, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE },

	/* Instrument editor */
	/*x,   y,   w,  h,  type,                 style */
	{ 544, 175, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 189, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 203, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 220, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 234, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 248, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 544, 262, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },

	/* Instrument editor extension */
	/*x,   y,   w,  h,  type,                 style */
	{ 195, 130, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 195, 144, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 195, 158, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },

	/* Config audio */
	/*x,   y,   w,  h,  type,                 style */
	{ 365,  29, 18, 43, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },
	{ 365, 116, 18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE },
	{ 533, 117, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 533, 143, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },

	/* Config layout */
	/*x,   y,  w,  h,  type,                 style */
	{ 536, 15, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 29, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 43, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },
	{ 536, 71, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },

	/* Config MIDI input */
	/*x,   y,  w,  h,  type,                 style */
	{ 225, 50, 55, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },  /* SB_MIDI_CHANNEL */
	{ 225, 66, 55, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },  /* SB_MIDI_TRANSPOSE */
	{ 225, 82, 55, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE },  /* SB_MIDI_SENS */

	/* Disk op */
	/*x,   y,  w,  h,   type,               style */
	{ 335, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE }
};

static void setScrollBarThumbCoords(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0)
		state->page = 1;

	/* Uninitialized scrollbar */
	if (state->end == 0)
	{
		state->thumbX = sb->x + 1;
		state->thumbY = sb->y + 1;
		state->thumbW = sb->w - 2;
		state->thumbH = sb->h - 2;
		return;
	}

	int16_t thumbX, thumbY, thumbW, thumbH;
	int16_t scrollEnd, originalThumbSize;
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

			if (state->end > 0)
			{
				length = sb->w - originalThumbSize;
				dTmp = (length / (double)state->end) * state->pos;
				thumbX = (int16_t)(sb->x + (int32_t)(dTmp + 0.5));
			}
			else
			{
				thumbX = sb->x;
			}
		}
		else
		{
			if (state->end > 0)
			{
				dTmp = (sb->w / (double)state->end) * state->page;
				originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->w);
			}
			else
			{
				originalThumbSize = 1;
			}

			thumbW = originalThumbSize;
			if (thumbW < MIN_THUMB_SIZE)
				thumbW = MIN_THUMB_SIZE;

			if (state->end > state->page)
			{
				length = sb->w - thumbW;
				end = state->end - state->page;
				dTmp = (length / (double)end) * state->pos;
				thumbX = (int16_t)(sb->x + (int32_t)(dTmp + 0.5));
			}
			else
			{
				thumbX = sb->x;
			}
		}

		thumbX = CLAMP(thumbX, sb->x, scrollEnd - 1);
		if (thumbX + thumbW > scrollEnd)
			thumbW = scrollEnd - thumbX;
	}
	else
	{
		/* Vertical scrollbar */
		thumbX = sb->x + 1;
		thumbW = sb->w - 2;
		scrollEnd = sb->y + sb->h;

		if (state->end > 0)
		{
			dTmp = (sb->h / (double)state->end) * state->page;
			originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->h);
		}
		else
		{
			originalThumbSize = 1;
		}

		thumbH = originalThumbSize;
		if (thumbH < MIN_THUMB_SIZE)
			thumbH = MIN_THUMB_SIZE;

		if (state->end > state->page)
		{
			length = sb->h - thumbH;
			end = state->end - state->page;
			dTmp = (length / (double)end) * state->pos;
			thumbY = (int16_t)(sb->y + (int32_t)(dTmp + 0.5));
		}
		else
		{
			thumbY = sb->y;
		}

		thumbY = CLAMP(thumbY, sb->y, scrollEnd - 1);
		if (thumbY + thumbH > scrollEnd)
			thumbH = scrollEnd - thumbY;
	}

	state->thumbX = thumbX;
	state->thumbY = thumbY;
	state->thumbW = thumbW;
	state->thumbH = thumbH;
}

void initScrollBars(ft2_widgets_t *widgets)
{
	/* Set up callbacks in the constant definition array */
	widgets->scrollBars[SB_AMP_SCROLL].callbackFunc = sbAmpPos;
	widgets->scrollBars[SB_MASTERVOL_SCROLL].callbackFunc = sbMasterVolPos;
	widgets->scrollBars[SB_PAL_R].callbackFunc = sbPalRPos;
	widgets->scrollBars[SB_PAL_G].callbackFunc = sbPalGPos;
	widgets->scrollBars[SB_PAL_B].callbackFunc = sbPalBPos;
	widgets->scrollBars[SB_PAL_CONTRAST].callbackFunc = sbPalContrastPos;
	widgets->scrollBars[SB_MIDI_CHANNEL].callbackFunc = sbMidiChannel;
	widgets->scrollBars[SB_MIDI_TRANSPOSE].callbackFunc = sbMidiTranspose;
	widgets->scrollBars[SB_MIDI_SENS].callbackFunc = sbMidiSens;

	/* Initialize per-instance state if widgets provided */
	if (widgets == NULL)
		return;

	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		widgets->scrollBarState[i].visible = false;
		widgets->scrollBarState[i].state = SCROLLBAR_UNPRESSED;
		widgets->scrollBarState[i].pos = 0;
		widgets->scrollBarState[i].page = 1;
		widgets->scrollBarState[i].end = 1;
	}

	/* Pattern editor */
	widgets->scrollBarState[SB_CHAN_SCROLL].page = 8;
	widgets->scrollBarState[SB_CHAN_SCROLL].end = 8;

	/* Position editor */
	widgets->scrollBarState[SB_POS_ED].page = 5;
	widgets->scrollBarState[SB_POS_ED].end = 5;

	/* Instrument switcher */
	widgets->scrollBarState[SB_SAMPLE_LIST].page = 5;
	widgets->scrollBarState[SB_SAMPLE_LIST].end = 16;

	/* Help screen */
	widgets->scrollBarState[SB_HELP_SCROLL].page = 15;
	widgets->scrollBarState[SB_HELP_SCROLL].end = 1;

	/* Config */
	widgets->scrollBarState[SB_AMP_SCROLL].page = 1;
	widgets->scrollBarState[SB_AMP_SCROLL].end = 31;
	widgets->scrollBarState[SB_MASTERVOL_SCROLL].page = 1;
	widgets->scrollBarState[SB_MASTERVOL_SCROLL].end = 256;
	widgets->scrollBarState[SB_PAL_R].page = 1;
	widgets->scrollBarState[SB_PAL_R].end = 63;
	widgets->scrollBarState[SB_PAL_G].page = 1;
	widgets->scrollBarState[SB_PAL_G].end = 63;
	widgets->scrollBarState[SB_PAL_B].page = 1;
	widgets->scrollBarState[SB_PAL_B].end = 63;
	widgets->scrollBarState[SB_PAL_CONTRAST].page = 1;
	widgets->scrollBarState[SB_PAL_CONTRAST].end = 100;

	/* Config MIDI input */
	widgets->scrollBarState[SB_MIDI_CHANNEL].page = 1;
	widgets->scrollBarState[SB_MIDI_CHANNEL].end = 15;  /* 0-15 = channels 1-16 */
	widgets->scrollBarState[SB_MIDI_TRANSPOSE].page = 1;
	widgets->scrollBarState[SB_MIDI_TRANSPOSE].end = 96;  /* 0-96 = -48 to +48 semitones */
	widgets->scrollBarState[SB_MIDI_SENS].page = 1;
	widgets->scrollBarState[SB_MIDI_SENS].end = 200;

	widgets->scrollBarState[SB_AUDIO_OUTPUT_SCROLL].page = 6;
	widgets->scrollBarState[SB_AUDIO_OUTPUT_SCROLL].end = 1;
	widgets->scrollBarState[SB_AUDIO_INPUT_SCROLL].page = 4;
	widgets->scrollBarState[SB_AUDIO_INPUT_SCROLL].end = 1;

	/* Disk op */
	widgets->scrollBarState[SB_DISKOP_LIST].page = 15;
	widgets->scrollBarState[SB_DISKOP_LIST].end = 1;

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

	/* Instrument editor extension */
	widgets->scrollBarState[SB_INST_EXT_MIDI_CH].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_CH].end = 15;
	widgets->scrollBarState[SB_INST_EXT_MIDI_PRG].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_PRG].end = 127;
	widgets->scrollBarState[SB_INST_EXT_MIDI_BEND].page = 1;
	widgets->scrollBarState[SB_INST_EXT_MIDI_BEND].end = 36;
}

void drawScrollBar(ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (!state->visible)
		return;

	setScrollBarThumbCoords(widgets, scrollBarID);

	int16_t thumbX = state->thumbX;
	int16_t thumbY = state->thumbY;
	int16_t thumbW = state->thumbW;
	int16_t thumbH = state->thumbH;

	/* Clear scrollbar background */
	clearRect(video, sb->x, sb->y, sb->w, sb->h);

	/* Draw thumb */
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		fillRect(video, thumbX, thumbY, thumbW, thumbH, PAL_PATTEXT);
	}
	else
	{
		/* Fixed thumb size */
		fillRect(video, thumbX, thumbY, thumbW, thumbH, PAL_BUTTONS);

		if (state->state == SCROLLBAR_UNPRESSED)
		{
			/* Top left corner inner border */
			hLine(video, thumbX, thumbY, thumbW - 1, PAL_BUTTON1);
			vLine(video, thumbX, thumbY + 1, thumbH - 2, PAL_BUTTON1);

			/* Bottom right corner inner border */
			hLine(video, thumbX, thumbY + thumbH - 1, thumbW - 1, PAL_BUTTON2);
			vLine(video, thumbX + thumbW - 1, thumbY, thumbH, PAL_BUTTON2);
		}
		else
		{
			/* Top left corner inner border */
			hLine(video, thumbX, thumbY, thumbW, PAL_BUTTON2);
			vLine(video, thumbX, thumbY + 1, thumbH - 1, PAL_BUTTON2);
		}
	}
}

void showScrollBar(ft2_widgets_t *widgets, struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	widgets->scrollBarState[scrollBarID].visible = true;
	drawScrollBar(widgets, video, scrollBarID);
}

void hideScrollBar(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	widgets->scrollBarState[scrollBarID].state = SCROLLBAR_UNPRESSED;
	widgets->scrollBarState[scrollBarID].visible = false;
}

void setScrollBarPos(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pos, bool triggerCallback)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0)
	{
		state->pos = 0;
		return;
	}

	if (state->end < state->page || state->pos == pos)
	{
		setScrollBarThumbCoords(widgets, scrollBarID);
		if (video != NULL)
			drawScrollBar(widgets, video, scrollBarID);
		return;
	}

	uint32_t endPos = state->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= state->page)
			endPos -= state->page;
		else
			endPos = 0;
	}

	state->pos = pos;
	if (state->pos > endPos)
		state->pos = endPos;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video != NULL)
		drawScrollBar(widgets, video, scrollBarID);

	if (triggerCallback && sb->callbackFunc != NULL)
		sb->callbackFunc(inst, state->pos);
}

uint32_t getScrollBarPos(ft2_widgets_t *widgets, uint16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return 0;

	return widgets->scrollBarState[scrollBarID].pos;
}

void setScrollBarEnd(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t end)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (end < 1)
		end = 1;

	state->end = end;

	bool setPos = false;
	if (state->pos >= end)
	{
		state->pos = end - 1;
		setPos = true;
	}

	if (state->page > 0)
	{
		if (setPos)
			setScrollBarPos(inst, widgets, video, scrollBarID, state->pos, false);
		else
		{
			setScrollBarThumbCoords(widgets, scrollBarID);
			if (video != NULL)
				drawScrollBar(widgets, video, scrollBarID);
		}
	}
}

void setScrollBarPageLength(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t pageLength)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (pageLength < 1)
		pageLength = 1;

	state->page = pageLength;
	if (state->end > 0)
	{
		setScrollBarPos(inst, widgets, video, scrollBarID, state->pos, false);
		setScrollBarThumbCoords(widgets, scrollBarID);
		if (video != NULL)
			drawScrollBar(widgets, video, scrollBarID);
	}
}

void scrollBarScrollUp(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0 || state->end == 0)
		return;

	if (state->end < state->page || state->pos == 0)
		return;

	if (state->pos >= amount)
		state->pos -= amount;
	else
		state->pos = 0;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video != NULL)
		drawScrollBar(widgets, video, scrollBarID);

	if (sb->callbackFunc != NULL)
		sb->callbackFunc(inst, state->pos);
}

void scrollBarScrollDown(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	if (widgets == NULL || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (state->page == 0 || state->end == 0)
		return;

	if (state->end < state->page)
		return;

	uint32_t endPos = state->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= state->page)
			endPos -= state->page;
		else
			endPos = 0;
	}

	if (state->pos == endPos)
		return;

	state->pos += amount;
	if (state->pos > endPos)
		state->pos = endPos;

	setScrollBarThumbCoords(widgets, scrollBarID);
	if (video != NULL)
		drawScrollBar(widgets, video, scrollBarID);

	if (sb->callbackFunc != NULL)
		sb->callbackFunc(inst, state->pos);
}

void scrollBarScrollLeft(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollUp(inst, widgets, video, scrollBarID, amount);
}

void scrollBarScrollRight(struct ft2_instance_t *inst, ft2_widgets_t *widgets, struct ft2_video_t *video,
	uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollDown(inst, widgets, video, scrollBarID, amount);
}

int16_t testScrollBarMouseDown(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
	if (widgets == NULL)
		return -1;

	uint16_t start, end;

	if (sysReqShown)
	{
		start = 0;
		end = 3;
	}
	else
	{
		start = 3;
		end = NUM_SCROLLBARS;
	}

	for (uint16_t i = start; i < end; i++)
	{
		scrollBar_t *sb = &widgets->scrollBars[i];
		ft2_scrollbar_state_t *state = &widgets->scrollBarState[i];

		if (!state->visible)
			continue;

		if (mouseX >= sb->x && mouseX < sb->x + sb->w &&
		    mouseY >= sb->y && mouseY < sb->y + sb->h)
		{
			state->state = SCROLLBAR_PRESSED;
			lastMouseX = mouseX;
			lastMouseY = mouseY;

			if (sb->type == SCROLLBAR_HORIZONTAL)
			{
				/* Check if clicked on thumb or outside */
				if (mouseX >= state->thumbX && mouseX < state->thumbX + state->thumbW)
				{
					/* Clicked on thumb - track offset */
					scrollBias = mouseX - state->thumbX;
				}
				else
				{
					/* Clicked outside thumb - center thumb on click position */
					scrollBias = state->thumbW >> 1;

					int32_t scrollPos = mouseX - scrollBias - sb->x;
					scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->w);

					int32_t length;
					if (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
						length = sb->w - state->thumbW;
					else
					{
						/* Calculate original thumb size for dynamic scrollbars */
						int16_t originalThumbSize;
						if (state->end > 0)
						{
							double dTmp = (sb->w / (double)state->end) * state->page;
							originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->w);
						}
						else
						{
							originalThumbSize = 1;
						}
						length = sb->w + (originalThumbSize - state->thumbW);
					}

					if (length < 1)
						length = 1;

					int32_t newPos = (int32_t)(((double)scrollPos * state->end) / length + 0.5);
					setScrollBarPos(inst, widgets, video, i, (uint32_t)newPos, true);
				}
			}
			else
			{
				/* Vertical scrollbar */
				if (mouseY >= state->thumbY && mouseY < state->thumbY + state->thumbH)
				{
					/* Clicked on thumb - track offset */
					scrollBias = mouseY - state->thumbY;
				}
				else
				{
					/* Clicked outside thumb - center thumb on click position */
					scrollBias = state->thumbH >> 1;

					int32_t scrollPos = mouseY - scrollBias - sb->y;
					scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->h);

					/* Calculate original thumb size for dynamic scrollbars */
					int16_t originalThumbSize;
					if (state->end > 0)
					{
						double dTmp = (sb->h / (double)state->end) * state->page;
						originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->h);
					}
					else
					{
						originalThumbSize = 1;
					}
					int32_t length = sb->h + (originalThumbSize - state->thumbH);
					if (length < 1)
						length = 1;

					int32_t newPos = (int32_t)(((double)scrollPos * state->end) / length + 0.5);
					setScrollBarPos(inst, widgets, video, i, (uint32_t)newPos, true);
				}
			}

			return (int16_t)i;
		}
	}

	return -1;
}

void testScrollBarMouseRelease(ft2_widgets_t *widgets, struct ft2_video_t *video, int16_t lastScrollBarID)
{
	if (widgets == NULL || lastScrollBarID < 0 || lastScrollBarID >= NUM_SCROLLBARS)
		return;

	ft2_scrollbar_state_t *state = &widgets->scrollBarState[lastScrollBarID];
	if (state->visible)
	{
		state->state = SCROLLBAR_UNPRESSED;
		if (video != NULL)
			drawScrollBar(widgets, video, lastScrollBarID);
	}

	/* Reset palette error flag so it can show again on next click */
	resetPaletteErrorFlag();
}

void handleScrollBarWhileMouseDown(ft2_widgets_t *widgets, struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, int16_t scrollBarID)
{
	if (widgets == NULL || scrollBarID < 0 || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &widgets->scrollBars[scrollBarID];
	ft2_scrollbar_state_t *state = &widgets->scrollBarState[scrollBarID];

	if (!state->visible)
		return;

	/* Match original FT2 behavior: track movement along relevant axis only,
	 * don't require mouse to stay within scrollbar bounds */

	if (sb->type == SCROLLBAR_HORIZONTAL)
	{
		/* Only update if mouse X changed */
		if (mouseX != lastMouseX)
		{
			lastMouseX = mouseX;

			int32_t scrollPos = mouseX - scrollBias - sb->x;
			scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->w);

			int32_t length;
			if (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
				length = sb->w - state->thumbW;
			else
			{
				/* Calculate original thumb size for dynamic scrollbars */
				int16_t originalThumbSize;
				if (state->end > 0)
				{
					double dTmp = (sb->w / (double)state->end) * state->page;
					originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->w);
				}
				else
				{
					originalThumbSize = 1;
				}
				length = sb->w + (originalThumbSize - state->thumbW);
			}

			if (length < 1)
				length = 1;

			int32_t newPos = (int32_t)(((double)scrollPos * state->end) / length + 0.5);
			setScrollBarPos(inst, widgets, video, scrollBarID, (uint32_t)newPos, true);

			if (video != NULL)
				drawScrollBar(widgets, video, scrollBarID);
		}
	}
	else
	{
		/* Vertical scrollbar - only update if mouse Y changed */
		if (mouseY != lastMouseY)
		{
			lastMouseY = mouseY;

			int32_t scrollPos = mouseY - scrollBias - sb->y;
			scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->h);

			/* Calculate original thumb size for dynamic scrollbars */
			int16_t originalThumbSize;
			if (state->end > 0)
			{
				double dTmp = (sb->h / (double)state->end) * state->page;
				originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->h);
			}
			else
			{
				originalThumbSize = 1;
			}
			int32_t length = sb->h + (originalThumbSize - state->thumbH);
			if (length < 1)
				length = 1;

			int32_t newPos = (int32_t)(((double)scrollPos * state->end) / length + 0.5);
			setScrollBarPos(inst, widgets, video, scrollBarID, (uint32_t)newPos, true);

			if (video != NULL)
				drawScrollBar(widgets, video, scrollBarID);
		}
	}
}

