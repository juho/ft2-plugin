/**
 * @file ft2_plugin_scrollbars.c
 * @brief Scrollbar implementation for the FT2 plugin UI.
 * 
 * Ported from ft2_scrollbars.c - exact coordinates preserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_palette.h"
#include "ft2_plugin_config.h"

#define FIXED_THUMB_SIZE 15
#define MIN_THUMB_SIZE 5

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* Track where user clicked on the thumb for smooth dragging */
static int32_t lastMouseX, lastMouseY, scrollBias;

scrollBar_t scrollBars[NUM_SCROLLBARS] =
{
	/* Reserved scrollbars */
	{ 0 }, { 0 }, { 0 },

	/* Position editor */
	/*x,  y,  w,  h,  type,               style,                        callback */
	{ 55, 15, 18, 21, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },

	/* Instrument switcher */
	/*x,   y,   w,  h,  type,               style,                        callback */
	{ 566, 112, 18, 28, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },

	/* Pattern viewer */
	/*x,  y,   w,   h,  type,                 style,                        callback */
	{ 28, 385, 576, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },

	/* Help screen */
	/*x,   y,  w,  h,   type,               style,                        callback */
	{ 611, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },

	/* Sample editor */
	/*x,  y,   w,   h,  type,                 style,                        callback */
	{ 26, 331, 580, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },

	/* Instrument editor */
	/*x,   y,   w,  h,  type,                 style,                      callback */
	{ 544, 175, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 189, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 203, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 220, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 234, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 248, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 544, 262, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },

	/* Instrument editor extension */
	/*x,   y,   w,  h,  type,                 style,                      callback */
	{ 195, 130, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 195, 144, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 195, 158, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },

	/* Config audio */
	/*x,   y,   w,  h,  type,                 style,                        callback */
	{ 365,  29, 18, 43, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },
	{ 365, 116, 18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL },
	{ 533, 117, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE,   NULL },
	{ 533, 143, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE,   NULL },

	/* Config layout */
	/*x,   y,  w,  h,  type,                 style,                      callback */
	{ 536, 15, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 536, 29, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 536, 43, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },
	{ 536, 71, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },

	/* Config miscellaneous */
	/*x,   y,   w,  h,  type,                 style,                      callback */
	{ 578, 158, 29, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, NULL },

	/* Disk op */
	/*x,   y,  w,  h,   type,               style,                        callback */
	{ 335, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, NULL }
};

static void setScrollBarThumbCoords(uint16_t scrollBarID)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (sb->page == 0)
		sb->page = 1;

	/* Uninitialized scrollbar */
	if (sb->end == 0)
	{
		sb->thumbX = sb->x + 1;
		sb->thumbY = sb->y + 1;
		sb->thumbW = sb->w - 2;
		sb->thumbH = sb->h - 2;
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

			if (sb->end > 0)
			{
				length = sb->w - originalThumbSize;
				dTmp = (length / (double)sb->end) * sb->pos;
				thumbX = (int16_t)(sb->x + (int32_t)(dTmp + 0.5));
			}
			else
			{
				thumbX = sb->x;
			}
		}
		else
		{
			if (sb->end > 0)
			{
				dTmp = (sb->w / (double)sb->end) * sb->page;
				originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->w);
			}
			else
			{
				originalThumbSize = 1;
			}

			thumbW = originalThumbSize;
			if (thumbW < MIN_THUMB_SIZE)
				thumbW = MIN_THUMB_SIZE;

			if (sb->end > sb->page)
			{
				length = sb->w - thumbW;
				end = sb->end - sb->page;
				dTmp = (length / (double)end) * sb->pos;
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

		if (sb->end > 0)
		{
			dTmp = (sb->h / (double)sb->end) * sb->page;
			originalThumbSize = (int16_t)CLAMP((int32_t)(dTmp + 0.5), 1, sb->h);
		}
		else
		{
			originalThumbSize = 1;
		}

		thumbH = originalThumbSize;
		if (thumbH < MIN_THUMB_SIZE)
			thumbH = MIN_THUMB_SIZE;

		if (sb->end > sb->page)
		{
			length = sb->h - thumbH;
			end = sb->end - sb->page;
			dTmp = (length / (double)end) * sb->pos;
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

	sb->originalThumbSize = originalThumbSize;
	sb->thumbX = thumbX;
	sb->thumbY = thumbY;
	sb->thumbW = thumbW;
	sb->thumbH = thumbH;
}

void initScrollBars(void)
{
	for (int i = 0; i < NUM_SCROLLBARS; i++)
	{
		scrollBars[i].visible = false;
		scrollBars[i].state = SCROLLBAR_UNPRESSED;
		scrollBars[i].pos = 0;
		scrollBars[i].page = 1;
		scrollBars[i].end = 1;
	}

	/* Pattern editor */
	scrollBars[SB_CHAN_SCROLL].page = 8;
	scrollBars[SB_CHAN_SCROLL].end = 8;

	/* Position editor */
	scrollBars[SB_POS_ED].page = 5;
	scrollBars[SB_POS_ED].end = 5;

	/* Instrument switcher */
	scrollBars[SB_SAMPLE_LIST].page = 5;
	scrollBars[SB_SAMPLE_LIST].end = 16;

	/* Help screen */
	scrollBars[SB_HELP_SCROLL].page = 15;
	scrollBars[SB_HELP_SCROLL].end = 1;

	/* Config */
	scrollBars[SB_AMP_SCROLL].page = 1;
	scrollBars[SB_AMP_SCROLL].end = 31;
	scrollBars[SB_AMP_SCROLL].callbackFunc = sbAmpPos;
	scrollBars[SB_MASTERVOL_SCROLL].page = 1;
	scrollBars[SB_MASTERVOL_SCROLL].end = 256;
	scrollBars[SB_MASTERVOL_SCROLL].callbackFunc = sbMasterVolPos;
	scrollBars[SB_PAL_R].page = 1;
	scrollBars[SB_PAL_R].end = 63;
	scrollBars[SB_PAL_R].callbackFunc = sbPalRPos;
	scrollBars[SB_PAL_G].page = 1;
	scrollBars[SB_PAL_G].end = 63;
	scrollBars[SB_PAL_G].callbackFunc = sbPalGPos;
	scrollBars[SB_PAL_B].page = 1;
	scrollBars[SB_PAL_B].end = 63;
	scrollBars[SB_PAL_B].callbackFunc = sbPalBPos;
	scrollBars[SB_PAL_CONTRAST].page = 1;
	scrollBars[SB_PAL_CONTRAST].end = 100;
	scrollBars[SB_PAL_CONTRAST].callbackFunc = sbPalContrastPos;
	scrollBars[SB_MIDI_SENS].page = 1;
	scrollBars[SB_MIDI_SENS].end = 200;
	scrollBars[SB_AUDIO_OUTPUT_SCROLL].page = 6;
	scrollBars[SB_AUDIO_OUTPUT_SCROLL].end = 1;
	scrollBars[SB_AUDIO_INPUT_SCROLL].page = 4;
	scrollBars[SB_AUDIO_INPUT_SCROLL].end = 1;

	/* Disk op */
	scrollBars[SB_DISKOP_LIST].page = 15;
	scrollBars[SB_DISKOP_LIST].end = 1;

	/* Instrument editor */
	scrollBars[SB_INST_VOL].page = 1;
	scrollBars[SB_INST_VOL].end = 64;
	scrollBars[SB_INST_PAN].page = 1;
	scrollBars[SB_INST_PAN].end = 255;
	scrollBars[SB_INST_FTUNE].page = 1;
	scrollBars[SB_INST_FTUNE].end = 255;
	scrollBars[SB_INST_FADEOUT].page = 1;
	scrollBars[SB_INST_FADEOUT].end = 0xFFF;
	scrollBars[SB_INST_VIBSPEED].page = 1;
	scrollBars[SB_INST_VIBSPEED].end = 0x3F;
	scrollBars[SB_INST_VIBDEPTH].page = 1;
	scrollBars[SB_INST_VIBDEPTH].end = 0xF;
	scrollBars[SB_INST_VIBSWEEP].page = 1;
	scrollBars[SB_INST_VIBSWEEP].end = 0xFF;

	/* Instrument editor extension */
	scrollBars[SB_INST_EXT_MIDI_CH].page = 1;
	scrollBars[SB_INST_EXT_MIDI_CH].end = 15;
	scrollBars[SB_INST_EXT_MIDI_PRG].page = 1;
	scrollBars[SB_INST_EXT_MIDI_PRG].end = 127;
	scrollBars[SB_INST_EXT_MIDI_BEND].page = 1;
	scrollBars[SB_INST_EXT_MIDI_BEND].end = 36;
}

void drawScrollBar(struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];
	if (!sb->visible)
		return;

	setScrollBarThumbCoords(scrollBarID);

	int16_t thumbX = sb->thumbX;
	int16_t thumbY = sb->thumbY;
	int16_t thumbW = sb->thumbW;
	int16_t thumbH = sb->thumbH;

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

		if (sb->state == SCROLLBAR_UNPRESSED)
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

void showScrollBar(struct ft2_video_t *video, uint16_t scrollBarID)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBars[scrollBarID].visible = true;
	drawScrollBar(video, scrollBarID);
}

void hideScrollBar(uint16_t scrollBarID)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBars[scrollBarID].state = SCROLLBAR_UNPRESSED;
	scrollBars[scrollBarID].visible = false;
}

void setScrollBarPos(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t pos, bool triggerCallback)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (sb->page == 0)
	{
		sb->pos = 0;
		return;
	}

	if (sb->end < sb->page || sb->pos == pos)
	{
		setScrollBarThumbCoords(scrollBarID);
		if (video != NULL)
			drawScrollBar(video, scrollBarID);
		return;
	}

	uint32_t endPos = sb->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= sb->page)
			endPos -= sb->page;
		else
			endPos = 0;
	}

	sb->pos = pos;
	if (sb->pos > endPos)
		sb->pos = endPos;

	setScrollBarThumbCoords(scrollBarID);
	if (video != NULL)
		drawScrollBar(video, scrollBarID);

	if (triggerCallback && sb->callbackFunc != NULL)
		sb->callbackFunc(inst, sb->pos);
}

uint32_t getScrollBarPos(uint16_t scrollBarID)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return 0;

	return scrollBars[scrollBarID].pos;
}

void setScrollBarEnd(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t end)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (end < 1)
		end = 1;

	sb->end = end;

	bool setPos = false;
	if (sb->pos >= end)
	{
		sb->pos = end - 1;
		setPos = true;
	}

	if (sb->page > 0)
	{
		if (setPos)
			setScrollBarPos(inst, video, scrollBarID, sb->pos, false);
		else
		{
			setScrollBarThumbCoords(scrollBarID);
			if (video != NULL)
				drawScrollBar(video, scrollBarID);
		}
	}
}

void setScrollBarPageLength(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t pageLength)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (pageLength < 1)
		pageLength = 1;

	sb->page = pageLength;
	if (sb->end > 0)
	{
		setScrollBarPos(inst, video, scrollBarID, sb->pos, false);
		setScrollBarThumbCoords(scrollBarID);
		if (video != NULL)
			drawScrollBar(video, scrollBarID);
	}
}

void scrollBarScrollUp(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (sb->page == 0 || sb->end == 0)
		return;

	if (sb->end < sb->page || sb->pos == 0)
		return;

	if (sb->pos >= amount)
		sb->pos -= amount;
	else
		sb->pos = 0;

	setScrollBarThumbCoords(scrollBarID);
	if (video != NULL)
		drawScrollBar(video, scrollBarID);

	if (sb->callbackFunc != NULL)
		sb->callbackFunc(inst, sb->pos);
}

void scrollBarScrollDown(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount)
{
	if (scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];

	if (sb->page == 0 || sb->end == 0)
		return;

	if (sb->end < sb->page)
		return;

	uint32_t endPos = sb->end;
	if (sb->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= sb->page)
			endPos -= sb->page;
		else
			endPos = 0;
	}

	if (sb->pos == endPos)
		return;

	sb->pos += amount;
	if (sb->pos > endPos)
		sb->pos = endPos;

	setScrollBarThumbCoords(scrollBarID);
	if (video != NULL)
		drawScrollBar(video, scrollBarID);

	if (sb->callbackFunc != NULL)
		sb->callbackFunc(inst, sb->pos);
}

void scrollBarScrollLeft(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollUp(inst, video, scrollBarID, amount);
}

void scrollBarScrollRight(struct ft2_instance_t *inst, struct ft2_video_t *video, uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollDown(inst, video, scrollBarID, amount);
}

int16_t testScrollBarMouseDown(struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, bool sysReqShown)
{
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
		scrollBar_t *sb = &scrollBars[i];
		if (!sb->visible)
			continue;

		if (mouseX >= sb->x && mouseX < sb->x + sb->w &&
		    mouseY >= sb->y && mouseY < sb->y + sb->h)
		{
			sb->state = SCROLLBAR_PRESSED;
			lastMouseX = mouseX;
			lastMouseY = mouseY;

			if (sb->type == SCROLLBAR_HORIZONTAL)
			{
				/* Check if clicked on thumb or outside */
				if (mouseX >= sb->thumbX && mouseX < sb->thumbX + sb->thumbW)
				{
					/* Clicked on thumb - track offset */
					scrollBias = mouseX - sb->thumbX;
				}
				else
				{
					/* Clicked outside thumb - center thumb on click position */
					scrollBias = sb->thumbW >> 1;

					int32_t scrollPos = mouseX - scrollBias - sb->x;
					scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->w);

					int32_t length;
					if (sb->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
						length = sb->w - sb->thumbW;
					else
						length = sb->w + (sb->originalThumbSize - sb->thumbW);

					if (length < 1)
						length = 1;

					int32_t newPos = (int32_t)(((double)scrollPos * sb->end) / length + 0.5);
					setScrollBarPos(inst, video, i, (uint32_t)newPos, true);
				}
			}
			else
			{
				/* Vertical scrollbar */
				if (mouseY >= sb->thumbY && mouseY < sb->thumbY + sb->thumbH)
				{
					/* Clicked on thumb - track offset */
					scrollBias = mouseY - sb->thumbY;
				}
				else
				{
					/* Clicked outside thumb - center thumb on click position */
					scrollBias = sb->thumbH >> 1;

					int32_t scrollPos = mouseY - scrollBias - sb->y;
					scrollPos = CLAMP(scrollPos, 0, (int32_t)sb->h);

					int32_t length = sb->h + (sb->originalThumbSize - sb->thumbH);
					if (length < 1)
						length = 1;

					int32_t newPos = (int32_t)(((double)scrollPos * sb->end) / length + 0.5);
					setScrollBarPos(inst, video, i, (uint32_t)newPos, true);
				}
			}

			return (int16_t)i;
		}
	}

	return -1;
}

void testScrollBarMouseRelease(struct ft2_video_t *video, int16_t lastScrollBarID)
{
	if (lastScrollBarID < 0 || lastScrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[lastScrollBarID];
	if (sb->visible)
	{
		sb->state = SCROLLBAR_UNPRESSED;
		if (video != NULL)
			drawScrollBar(video, lastScrollBarID);
	}

	/* Reset palette error flag so it can show again on next click */
	resetPaletteErrorFlag();
}

void handleScrollBarWhileMouseDown(struct ft2_instance_t *inst, struct ft2_video_t *video,
	int32_t mouseX, int32_t mouseY, int16_t scrollBarID)
{
	if (scrollBarID < 0 || scrollBarID >= NUM_SCROLLBARS)
		return;

	scrollBar_t *sb = &scrollBars[scrollBarID];
	if (!sb->visible)
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
				length = sb->w - sb->thumbW;
			else
				length = sb->w + (sb->originalThumbSize - sb->thumbW);

			if (length < 1)
				length = 1;

			int32_t newPos = (int32_t)(((double)scrollPos * sb->end) / length + 0.5);
			setScrollBarPos(inst, video, scrollBarID, (uint32_t)newPos, true);

			if (video != NULL)
				drawScrollBar(video, scrollBarID);
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

			int32_t length = sb->h + (sb->originalThumbSize - sb->thumbH);
			if (length < 1)
				length = 1;

			int32_t newPos = (int32_t)(((double)scrollPos * sb->end) / length + 0.5);
			setScrollBarPos(inst, video, scrollBarID, (uint32_t)newPos, true);

			if (video != NULL)
				drawScrollBar(video, scrollBarID);
		}
	}
}

