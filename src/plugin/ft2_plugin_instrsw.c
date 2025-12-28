/**
 * @file ft2_plugin_instrsw.c
 * @brief Instrument switcher for the FT2 plugin.
 * 
 * Ported from ft2_pattern_ed.c - exact pixel-accurate implementation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "ft2_plugin_instrsw.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_instance.h"

void updateInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	if (inst->uiState.aboutScreenShown || inst->uiState.configScreenShown ||
		inst->uiState.helpScreenShown || inst->uiState.nibblesShown)
		return; /* Don't redraw when not visible */

	uint8_t instrBankOffset = inst->editor.instrBankOffset;
	uint8_t srcInstr = inst->editor.srcInstr;
	uint8_t curInstr = inst->editor.curInstr;
	bool extended = inst->uiState.extendedPatternEditor;

	if (extended)
	{
		/* Extended pattern editor: 8 instruments in two columns */
		clearRect(video, 388, 5, 116, 43);  /* Left box */
		clearRect(video, 511, 5, 116, 43);  /* Right box */

		/* Draw source instrument selection highlight */
		if (srcInstr >= instrBankOffset && srcInstr <= instrBankOffset + 8)
		{
			int16_t y = 5 + ((srcInstr - instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
			{
				if (y <= 47)
					fillRect(video, 388, y, 15, 10, PAL_BUTTONS);  /* Left box */
				else
					fillRect(video, 511, y - 44, 15, 10, PAL_BUTTONS);  /* Right box */
			}
		}

		/* Draw destination instrument selection highlight */
		if (curInstr >= instrBankOffset && curInstr <= instrBankOffset + 8)
		{
			int16_t y = 5 + ((curInstr - instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
			{
				if (y <= 47)
					fillRect(video, 406, y, 98, 10, PAL_BUTTONS);  /* Left box */
				else
					fillRect(video, 529, y - 44, 98, 10, PAL_BUTTONS);  /* Right box */
			}
		}

		/* Draw numbers and instrument names */
		for (int16_t i = 0; i < 4; i++)
		{
			uint8_t instrNum = 1 + instrBankOffset + i;
			hexOut(video, bmp, 388, 5 + (i * 11), PAL_FORGRND, instrNum, 2);
			hexOut(video, bmp, 511, 5 + (i * 11), PAL_FORGRND, instrNum + 4, 2);

			/* Draw instrument names */
			if (inst->replayer.instr[instrNum] != NULL)
			{
				textOut(video, bmp, 406, 5 + (i * 11), PAL_FORGRND,
					inst->replayer.song.instrName[instrNum]);
			}
			if (inst->replayer.instr[instrNum + 4] != NULL)
			{
				textOut(video, bmp, 529, 5 + (i * 11), PAL_FORGRND,
					inst->replayer.song.instrName[instrNum + 4]);
			}
		}
	}
	else
	{
		/* Normal pattern editor: 8 instruments in one column, 5 samples below */
		/* INSTRUMENTS */
		clearRect(video, 424, 5, 15, 87);  /* src instrument */
		clearRect(video, 446, 5, 140, 87); /* main instrument - full textbox width */

		/* Draw source instrument selection highlight */
		if (srcInstr >= instrBankOffset && srcInstr <= instrBankOffset + 8)
		{
			int16_t y = 5 + ((srcInstr - instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
				fillRect(video, 424, y, 15, 10, PAL_BUTTONS);
		}

		/* Draw destination instrument selection highlight */
		if (curInstr >= instrBankOffset && curInstr <= instrBankOffset + 8)
		{
			int16_t y = 5 + ((curInstr - instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
				fillRect(video, 446, y, 139, 10, PAL_BUTTONS);
		}

		/* Draw numbers and instrument names */
		for (int16_t i = 0; i < 8; i++)
		{
			uint8_t instrNum = 1 + instrBankOffset + i;
			hexOut(video, bmp, 424, 5 + (i * 11), PAL_FORGRND, instrNum, 2);

			/* Draw instrument name at textbox position (446 + 1 for tx padding) */
			textOut(video, bmp, 447, 5 + (i * 11), PAL_FORGRND,
				inst->replayer.song.instrName[instrNum]);
		}

		/* SAMPLES */
		clearRect(video, 424, 99, 15, 54);  /* src sample */
		clearRect(video, 446, 99, 116, 54); /* main sample - full textbox width */

		/* Draw source sample selection highlight */
		uint8_t srcSmp = inst->editor.srcSmp;
		uint8_t smpOffset = inst->editor.sampleBankOffset;
		if (srcSmp >= smpOffset && srcSmp <= smpOffset + 4)
		{
			int16_t y = 99 + ((srcSmp - smpOffset) * 11);
			if (y >= 36 && y <= 143)
				fillRect(video, 424, y, 15, 10, PAL_BUTTONS);
		}

		/* Draw destination sample selection highlight */
		uint8_t curSmp = inst->editor.curSmp;
		if (curSmp >= smpOffset && curSmp <= smpOffset + 4)
		{
			int16_t y = 99 + ((curSmp - smpOffset) * 11);
			if (y >= 36 && y <= 143)
				fillRect(video, 446, y, 115, 10, PAL_BUTTONS);
		}

		/* Draw sample numbers and names */
		ft2_instr_t *instr = inst->replayer.instr[curInstr];
		for (int16_t i = 0; i < 5; i++)
		{
			uint8_t smpNum = smpOffset + i;
			hexOut(video, bmp, 424, 99 + (i * 11), PAL_FORGRND, smpNum, 2);

			if (instr != NULL && smpNum < 16)
			{
				/* Draw sample name at textbox position (446 + 1 for tx padding) */
				textOut(video, bmp, 447, 99 + (i * 11), PAL_FORGRND,
					instr->smp[smpNum].name);
			}
		}
	}
}

void showInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	if (!inst->uiState.instrSwitcherShown)
		return;

	bool extended = inst->uiState.extendedPatternEditor;

	if (extended)
	{
		/* Extended pattern editor: two-column layout */
		hidePushButton(PB_SAMPLE_LIST_UP);
		hidePushButton(PB_SAMPLE_LIST_DOWN);
		hideScrollBar(SB_SAMPLE_LIST);

		drawFramework(video, 386, 0, 246, 3, FRAMEWORK_TYPE1);
		drawFramework(video, 506, 3, 3, 47, FRAMEWORK_TYPE1);
		drawFramework(video, 386, 50, 246, 3, FRAMEWORK_TYPE1);
		drawFramework(video, 629, 3, 3, 47, FRAMEWORK_TYPE1);

		clearRect(video, 386, 3, 120, 47);
		clearRect(video, 509, 3, 120, 47);
	}
	else
	{
		/* Normal pattern editor layout */
		drawFramework(video, 421, 0, 166, 3, FRAMEWORK_TYPE1);
		drawFramework(video, 442, 3, 3, 91, FRAMEWORK_TYPE1);
		drawFramework(video, 421, 94, 166, 3, FRAMEWORK_TYPE1);
		drawFramework(video, 442, 97, 3, 58, FRAMEWORK_TYPE1);
		drawFramework(video, 563, 97, 24, 58, FRAMEWORK_TYPE1);
		drawFramework(video, 587, 0, 45, 71, FRAMEWORK_TYPE1);
		drawFramework(video, 587, 71, 45, 71, FRAMEWORK_TYPE1);
		drawFramework(video, 587, 142, 45, 31, FRAMEWORK_TYPE1);

		fillRect(video, 421, 3, 21, 91, PAL_BCKGRND);
		fillRect(video, 445, 3, 142, 91, PAL_BCKGRND);
		fillRect(video, 421, 97, 21, 58, PAL_BCKGRND);
		fillRect(video, 445, 97, 118, 58, PAL_BCKGRND);

		showPushButton(video, bmp, PB_SAMPLE_LIST_UP);
		showPushButton(video, bmp, PB_SAMPLE_LIST_DOWN);
		showScrollBar(video, SB_SAMPLE_LIST);
	}

	updateInstrumentSwitcher(inst, video, bmp);

	/* Show bank buttons */
	for (uint16_t i = 0; i < 8; i++)
		showPushButton(video, bmp, PB_RANGE1 + i + (inst->editor.instrBankSwapped * 8));

	showPushButton(video, bmp, PB_SWAP_BANK);
}

void hideInstrumentSwitcher(struct ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Hide all bank buttons */
	for (uint16_t i = 0; i < 16; i++)
		hidePushButton(PB_RANGE1 + i);

	hidePushButton(PB_SWAP_BANK);
	hidePushButton(PB_SAMPLE_LIST_UP);
	hidePushButton(PB_SAMPLE_LIST_DOWN);
	hideScrollBar(SB_SAMPLE_LIST);
}

void drawInstrumentSwitcher(struct ft2_instance_t *inst, struct ft2_video_t *video,
	const struct ft2_bmp_t *bmp)
{
	if (inst == NULL || video == NULL)
		return;

	showInstrumentSwitcher(inst, video, bmp);
}

/**
 * Test if mouse click hit the instrument switcher (exact match to original testInstrSwitcherNormal).
 * @return true if the click was handled
 */
bool testInstrSwitcherMouseDown(struct ft2_instance_t *inst, int32_t mouseX, int32_t mouseY)
{
	if (inst == NULL)
		return false;

	if (!inst->uiState.instrSwitcherShown)
		return false;

	if (inst->uiState.extendedPatternEditor)
	{
		/* Extended pattern editor mode */
		if (mouseY < 5 || mouseY > 47)
			return false;

		if (mouseX >= 511)
		{
			/* Right columns */
			if (mouseX <= 525)
			{
				/* Source instrument (right column) */
				if ((mouseY - 5) % 11 == 10)
					return true; /* Clicked on one-pixel spacer */

				uint8_t newEntry = (inst->editor.instrBankOffset + 5) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.srcInstr != newEntry)
				{
					inst->editor.srcInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
				}
				return true;
			}
			else if (mouseX >= 529 && mouseX <= 626)
			{
				/* Destination instrument (right column) */
				if ((mouseY - 5) % 11 == 10)
					return true;

				uint8_t newEntry = (inst->editor.instrBankOffset + 5) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.curInstr != newEntry)
				{
					inst->editor.curInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
					inst->uiState.updateSampleEditor = true;
					inst->uiState.updateInstEditor = true;
				}
				return true;
			}
		}
		else if (mouseX >= 388)
		{
			/* Left columns */
			if (mouseX <= 402)
			{
				/* Source instrument (left column) */
				if ((mouseY - 5) % 11 == 10)
					return true;

				uint8_t newEntry = (inst->editor.instrBankOffset + 1) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.srcInstr != newEntry)
				{
					inst->editor.srcInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
				}
				return true;
			}
			else if (mouseX >= 406 && mouseX <= 503)
			{
				/* Destination instrument (left column) */
				if ((mouseY - 5) % 11 == 10)
					return true;

				uint8_t newEntry = (inst->editor.instrBankOffset + 1) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.curInstr != newEntry)
				{
					inst->editor.curInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
					inst->uiState.updateSampleEditor = true;
					inst->uiState.updateInstEditor = true;
				}
				return true;
			}
		}
		return false;
	}
	else
	{
		/* Normal pattern editor mode */
		if (mouseX < 424 || mouseX > 585)
			return false;

		if (mouseY >= 5 && mouseY <= 91)
		{
			/* Instruments */
			if (mouseX >= 446 && mouseX <= 584)
			{
				/* Destination instrument */
				if ((mouseY - 5) % 11 == 10)
					return true; /* Clicked on one-pixel spacer */

				uint8_t newEntry = (inst->editor.instrBankOffset + 1) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.curInstr != newEntry)
				{
					inst->editor.curInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
					inst->uiState.updateSampleEditor = true;
					inst->uiState.updateInstEditor = true;
				}
				return true;
			}
			else if (mouseX >= 424 && mouseX <= 438)
			{
				/* Source instrument */
				if ((mouseY - 5) % 11 == 10)
					return true;

				uint8_t newEntry = (inst->editor.instrBankOffset + 1) + (uint8_t)((mouseY - 5) / 11);
				if (inst->editor.srcInstr != newEntry)
				{
					inst->editor.srcInstr = newEntry;
					inst->uiState.updateInstrSwitcher = true;
				}
				return true;
			}
		}
		else if (mouseY >= 99 && mouseY <= 152)
		{
			/* Samples */
			if (mouseX >= 446 && mouseX <= 560)
			{
				/* Destination sample */
				if ((mouseY - 99) % 11 == 10)
					return true;

				uint8_t newEntry = inst->editor.sampleBankOffset + (uint8_t)((mouseY - 99) / 11);
				if (newEntry > 15)
					newEntry = 15;
				if (inst->editor.curSmp != newEntry)
				{
					inst->editor.curSmp = newEntry;
					inst->uiState.updateInstrSwitcher = true;
					inst->uiState.updateSampleEditor = true;
					inst->uiState.updateInstEditor = true;
				}
				return true;
			}
		else if (mouseX >= 423 && mouseX <= 438)
		{
			/* Source sample */
			if ((mouseY - 99) % 11 == 10)
				return true;

			uint8_t newEntry = inst->editor.sampleBankOffset + (uint8_t)((mouseY - 99) / 11);
			if (newEntry > 15)
				newEntry = 15;
			if (inst->editor.srcSmp != newEntry)
			{
				inst->editor.srcSmp = newEntry;
				inst->uiState.updateInstrSwitcher = true;
			}
			return true;
		}
		}
		return false;
	}
}

