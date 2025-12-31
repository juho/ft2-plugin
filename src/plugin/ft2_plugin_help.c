/**
 * @file ft2_plugin_help.c
 * @brief Exact port of help screen from ft2_help.c
 * 
 * This is a direct port of the standalone FT2 help system,
 * parsing the original helpData[] and rendering with proper
 * formatting (big fonts, colors, positioning).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "ft2_plugin_help.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_ui.h"
#include "ft2_instance.h"
#include "../../plugin/helpdata/ft2_plugin_help_data.h"

/* Help record structure - exact match to standalone */
typedef struct
{
	bool bigFont, noLine;
	uint8_t color;
	int16_t xPos;
	char text[100];
} helpRec;

/* Constants - exact match to standalone */
#define HELP_LINES 15
#define MAX_HELP_LINES 768
#define HELP_SIZE sizeof(helpRec)
#define MAX_SUBJ 10
#define HELP_COLUMN 135
#define HELP_WIDTH (596 - HELP_COLUMN)

/* Static state - exact match to standalone */
static uint8_t fHlp_Num;
static int16_t textLine, fHlp_Line, subjLen[MAX_SUBJ];
static int32_t helpBufferPos;
static helpRec *subjPtrArr[MAX_SUBJ];
static bool helpInitialized = false;

/* Forward declarations */
static void readHelp(void);
static void writeHelp(ft2_video_t *video, const ft2_bmp_t *bmp);

/* ============ HELP DATA PARSER - EXACT PORT ============ */

static void addText(helpRec *t, int16_t xPos, uint8_t color, char *text)
{
	if (*text == '\0')
		return;

	t->xPos = xPos;
	t->color = color;
	t->bigFont = false;
	t->noLine = false;
	strcpy(t->text, text);
	*text = '\0'; /* empty old string */

	textLine++;
}

static bool getLine(char *output)
{
	if (helpBufferPos >= (int32_t)sizeof(helpData))
	{
		*output = '\0';
		return false;
	}

	const uint8_t strLen = helpData[helpBufferPos++];
	memcpy(output, &helpData[helpBufferPos], strLen);
	output[strLen] = '\0';

	helpBufferPos += strLen;

	return true;
}

static int16_t controlCodeToNum(const char *controlCode)
{
	return (((controlCode[0]-'0')%10)*100) + (((controlCode[1]-'0')%10)*10) + ((controlCode[2]-'0')%10);
}

static char *ltrim(char *s)
{
	if (*s == '\0')
		return (s);

	while (*s == ' ') s++;

	return s;
}

static char *rtrim(char *s)
{
	if (*s == '\0')
		return (s);

	int32_t i = (int32_t)strlen(s) - 1;
	while (i >= 0)
	{
		if (s[i] != ' ')
		{
			s[i+1] = '\0';
			break;
		}

		i--;
	}

	return s;
}

static void readHelp(void) /* this is a bit messy... (exact port from standalone) */
{
	char text[256], text2[256], *s, *sEnd, *s3;
	int16_t a, b, i, k;

	helpRec *tempText = (helpRec *)malloc(HELP_SIZE * MAX_HELP_LINES);
	if (tempText == NULL)
		return;
	
	text[0] = '\0';
	text2[0] = '\0';

	char *s2 = text2;

	helpBufferPos = 0;
	for (int16_t subj = 0; subj < MAX_SUBJ; subj++)
	{
		textLine = 0;
		int16_t currColumn = 0;
		uint8_t currColor = PAL_FORGRND;

		getLine(text); s = text;
		while (strncmp(s, "END", 3) != 0)
		{
			if (*s == ';')
			{
				if (!getLine(text))
					break;

				s = text;
				continue;
			}

			if (*(uint16_t *)s == 0x4C40) /* @L - "big font" */
			{
				addText(&tempText[textLine], currColumn, currColor, s2);
				s += 2;

				if (*(uint16_t *)s == 0x5840) /* @X - "change X position" */
				{
					currColumn = controlCodeToNum(&s[2]);
					s += 5;
				}

				if (*(uint16_t *)s == 0x4340) /* @C - "change color" */
				{
					currColor = (uint8_t)controlCodeToNum(&s[2]);
					currColor = (currColor < 2) ? PAL_FORGRND : PAL_BUTTONS;
					s += 5;
				}

				helpRec *t = &tempText[textLine];
				t->xPos = currColumn;
				t->color = currColor;
				t->bigFont = true;
				t->noLine = false;
				strcpy(t->text, s);
				textLine++;

				t = &tempText[textLine];
				t->noLine = true;
				textLine++;
			}
			else
			{
				if (*s == '>')
				{
					addText(&tempText[textLine], currColumn, currColor, s2);
					s++;
				}

				if (*(uint16_t *)s == 0x5840) /* @X - "set X position (relative to help X start)" */
				{
					currColumn = controlCodeToNum(&s[2]);
					s += 5;
				}

				if (*(uint16_t *)s == 0x4340) /* @C - "change color" */
				{
					currColor = (uint8_t)controlCodeToNum(&s[2]);
					currColor = (currColor < 2) ? PAL_FORGRND : PAL_BUTTONS;
					s += 5;
				}

				s = ltrim(rtrim(s));
				if (*s == '\0')
				{
					addText(&tempText[textLine], currColumn, currColor, s2);
					strcpy(s2, " ");
					addText(&tempText[textLine], currColumn, currColor, s2);
				}

				int16_t sLen = (int16_t)strlen(s);

				sEnd = &s[sLen];
				while (s < sEnd)
				{
					if (sLen < 0)
						sLen = 0;

					i = 0;
					while (s[i] != ' ' && i < sLen) i++;
					i++;

					if (*(uint16_t *)s == 0x5440) /* @T - "set absolute X position (in the middle of text)" */
					{
						k = controlCodeToNum(&s[2]);
						s += 5; sLen -= 5;

						s3 = &s2[strlen(s2)];
						while (textWidth(s2) + charWidth(' ') + 1 < k-currColumn)
						{
							s3[0] = ' ';
							s3[1] = '\0';
							s3++;
						}

						b = textWidth(s2) + 1;
						if (b < k-currColumn)
						{
							s3 = &s2[strlen(s2)];
							for (a = 0; a < k-b-currColumn; a++)
								s3[a] = 127; /* one-pixel spacer glyph */
							s3[a] = '\0';
						}
					}

					if (textWidth(s2)+textNWidth(s,i)+2 > HELP_WIDTH-currColumn)
						addText(&tempText[textLine], currColumn, currColor, s2);

					strncat(s2, s, i);

					s += i; sLen -= i;
					if ((*s == '\0') || (s >= sEnd))
						strcat(s2, " ");
				}
			}

			if (textLine >= MAX_HELP_LINES || !getLine(text))
				break;

			s = text;
		}

		subjPtrArr[subj] = (helpRec *)malloc(HELP_SIZE * textLine);
		if (subjPtrArr[subj] == NULL)
			break;

		memcpy(subjPtrArr[subj], tempText, HELP_SIZE * textLine);
		subjLen[subj] = textLine;
	}

	free(tempText);
}

/* ============ TEXT RENDERING - EXACT PORT ============ */

static void bigTextOutHalf(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, 
                           uint8_t paletteIndex, bool lowerHalf, const char *textPtr)
{
	assert(textPtr != NULL);
	if (video == NULL || video->frameBuffer == NULL || bmp == NULL || bmp->font2 == NULL)
		return;

	uint16_t currX = xPos;
	while (true)
	{
		const char chr = *textPtr++ & 0x7F;
		if (chr == '\0')
			break;

		if (chr != ' ')
		{
			const uint8_t *srcPtr = &bmp->font2[chr * FONT2_CHAR_W];
			if (!lowerHalf)
				srcPtr += (FONT2_CHAR_H / 2) * FONT2_WIDTH;

			uint32_t *dstPtr = &video->frameBuffer[(yPos * SCREEN_W) + currX];
			const uint32_t pixVal = video->palette[paletteIndex];

			for (uint32_t y = 0; y < FONT2_CHAR_H/2; y++)
			{
				for (uint32_t x = 0; x < FONT2_CHAR_W; x++)
				{
					if (srcPtr[x])
						dstPtr[x] = pixVal;
				}

				srcPtr += FONT2_WIDTH;
				dstPtr += SCREEN_W;
			}
		}

		currX += charWidth16(chr);
	}
}

static void writeHelp(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	helpRec *ptr = subjPtrArr[fHlp_Num];
	if (ptr == NULL || video == NULL)
		return;

	for (int16_t i = 0; i < HELP_LINES; i++)
	{
		const int16_t k = i + fHlp_Line;
		if (k >= subjLen[fHlp_Num])
			break;

		clearRect(video, HELP_COLUMN, 5 + (i * 11), HELP_WIDTH, 11);

		if (ptr[k].noLine)
		{
			if (i == 0)
				bigTextOutHalf(video, bmp, HELP_COLUMN + ptr[k-1].xPos, 5 + (i * 11), PAL_FORGRND, false, ptr[k-1].text);
		}
		else
		{
			if (ptr[k].bigFont)
			{
				if (i == HELP_LINES-1)
				{
					bigTextOutHalf(video, bmp, HELP_COLUMN + ptr[k].xPos, 5 + (i * 11), PAL_FORGRND, true, ptr[k].text);
					return;
				}
				else
				{
					clearRect(video, HELP_COLUMN, 5 + ((i + 1) * 11), HELP_WIDTH, 11);
					bigTextOut(video, bmp, HELP_COLUMN + ptr[k].xPos, 5 + (i * 11), PAL_FORGRND, ptr[k].text);
					i++;
				}
			}
			else
			{
				textOut(video, bmp, HELP_COLUMN + ptr[k].xPos, 5 + (i * 11), ptr[k].color, ptr[k].text);
			}
		}
	}
}

/* ============ PER-FRAME DRAWING ============ */

void drawHelpScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	uint16_t tmpID;

	if (inst == NULL || video == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL)
		return;
	ft2_widgets_t *widgets = &ui->widgets;

	if (!helpInitialized)
		initFTHelp();

	/* Draw framework - exact match to standalone */
	drawFramework(video, 0,   0, 128, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 128, 0, 504, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 130, 2, 479, 169, FRAMEWORK_TYPE2);

	/* Show push buttons */
	showPushButton(widgets, video, bmp, PB_HELP_EXIT);
	showPushButton(widgets, video, bmp, PB_HELP_SCROLL_UP);
	showPushButton(widgets, video, bmp, PB_HELP_SCROLL_DOWN);

	/* Set up radio buttons for current subject */
	switch (fHlp_Num)
	{
		default:
		case 0: tmpID = RB_HELP_FEATURES;    break;
		case 1: tmpID = RB_HELP_EFFECTS;     break;
		case 2: tmpID = RB_HELP_KEYBINDINGS; break;
		case 3: tmpID = RB_HELP_HOWTO;       break;
		case 4: tmpID = RB_HELP_PLUGIN;      break;
	}
	widgets->radioButtonState[tmpID] = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_HELP);

	/* Set scrollbar range and show it */
	setScrollBarEnd(inst, widgets, video, SB_HELP_SCROLL, subjLen[fHlp_Num]);
	setScrollBarPos(inst, widgets, video, SB_HELP_SCROLL, fHlp_Line, false);
	showScrollBar(widgets, video, SB_HELP_SCROLL);

	/* Draw subject labels */
	textOutShadow(video, bmp, 4,   4, PAL_FORGRND, PAL_DSKTOP2, "Subjects:");
	textOutShadow(video, bmp, 21, 19, PAL_FORGRND, PAL_DSKTOP2, "Features");
	textOutShadow(video, bmp, 21, 35, PAL_FORGRND, PAL_DSKTOP2, "Effects");
	textOutShadow(video, bmp, 21, 51, PAL_FORGRND, PAL_DSKTOP2, "Keybindings");
	textOutShadow(video, bmp, 21, 67, PAL_FORGRND, PAL_DSKTOP2, "How to use FT2");
	textOutShadow(video, bmp, 21, 83, PAL_FORGRND, PAL_DSKTOP2, "Plugin");

	writeHelp(video, bmp);
}

/* ============ SCROLL FUNCTIONS ============ */

void helpScrollUp(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	if (fHlp_Line > 0)
	{
		scrollBarScrollUp(inst, widgets, video, SB_HELP_SCROLL, 1);
		writeHelp(video, bmp);
	}
}

void helpScrollDown(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL)
		return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	
	if (fHlp_Line < subjLen[fHlp_Num]-1)
	{
		scrollBarScrollDown(inst, widgets, video, SB_HELP_SCROLL, 1);
		writeHelp(video, bmp);
	}
}

void helpScrollSetPos(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp, uint32_t pos)
{
	(void)inst;
	if (fHlp_Line != (int16_t)pos)
	{
		fHlp_Line = (int16_t)pos;
		writeHelp(video, bmp);
	}
}

/* ============ VISIBILITY ============ */

void showHelpScreen(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	uint16_t tmpID;

	if (inst == NULL || video == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL)
		return;
	ft2_widgets_t *widgets = &ui->widgets;

	if (!helpInitialized)
		initFTHelp();

	hideTopScreen(inst);
	inst->uiState.helpScreenShown = true;
	inst->uiState.scopesShown = false;

	/* Draw framework - exact match to standalone */
	drawFramework(video, 0,   0, 128, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 128, 0, 504, 173, FRAMEWORK_TYPE1);
	drawFramework(video, 130, 2, 479, 169, FRAMEWORK_TYPE2);

	/* Show push buttons */
	showPushButton(widgets, video, bmp, PB_HELP_EXIT);
	showPushButton(widgets, video, bmp, PB_HELP_SCROLL_UP);
	showPushButton(widgets, video, bmp, PB_HELP_SCROLL_DOWN);

	/* Set up radio buttons for current subject */
	uncheckRadioButtonGroup(widgets, RB_GROUP_HELP);
	switch (fHlp_Num)
	{
		default:
		case 0: tmpID = RB_HELP_FEATURES;    break;
		case 1: tmpID = RB_HELP_EFFECTS;     break;
		case 2: tmpID = RB_HELP_KEYBINDINGS; break;
		case 3: tmpID = RB_HELP_HOWTO;       break;
		case 4: tmpID = RB_HELP_PLUGIN;      break;
	}
	widgets->radioButtonState[tmpID] = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_HELP);

	/* Set scrollbar range for current subject and show it */
	setScrollBarEnd(inst, widgets, video, SB_HELP_SCROLL, subjLen[fHlp_Num]);
	setScrollBarPos(inst, widgets, video, SB_HELP_SCROLL, fHlp_Line, false);
	showScrollBar(widgets, video, SB_HELP_SCROLL);

	/* Draw subject labels */
	textOutShadow(video, bmp, 4,   4, PAL_FORGRND, PAL_DSKTOP2, "Subjects:");
	textOutShadow(video, bmp, 21, 19, PAL_FORGRND, PAL_DSKTOP2, "Features");
	textOutShadow(video, bmp, 21, 35, PAL_FORGRND, PAL_DSKTOP2, "Effects");
	textOutShadow(video, bmp, 21, 51, PAL_FORGRND, PAL_DSKTOP2, "Keybindings");
	textOutShadow(video, bmp, 21, 67, PAL_FORGRND, PAL_DSKTOP2, "How to use FT2");
	textOutShadow(video, bmp, 21, 83, PAL_FORGRND, PAL_DSKTOP2, "Plugin");

	writeHelp(video, bmp);
}

void hideHelpScreen(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui == NULL)
		return;
	ft2_widgets_t *widgets = &ui->widgets;

	hidePushButton(widgets, PB_HELP_EXIT);
	hidePushButton(widgets, PB_HELP_SCROLL_UP);
	hidePushButton(widgets, PB_HELP_SCROLL_DOWN);

	hideRadioButtonGroup(widgets, RB_GROUP_HELP);
	hideScrollBar(widgets, SB_HELP_SCROLL);

	inst->uiState.helpScreenShown = false;
}

void exitHelpScreen(ft2_instance_t *inst)
{
		hideHelpScreen(inst);
	inst->uiState.scopesShown = true;
	inst->uiState.instrSwitcherShown = true;
	inst->uiState.needsFullRedraw = true;
	inst->uiState.updatePosSections = true;
	inst->uiState.updateInstrSwitcher = true;
}

/* ============ SUBJECT SELECTION ============ */

static void setHelpSubject(ft2_instance_t *inst, ft2_widgets_t *widgets, ft2_video_t *video, uint8_t Nr)
{
	fHlp_Num = Nr;
	fHlp_Line = 0;

	setScrollBarEnd(inst, widgets, video, SB_HELP_SCROLL, subjLen[fHlp_Num]);
	setScrollBarPos(inst, widgets, video, SB_HELP_SCROLL, 0, false);
}

void rbHelpFeatures(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	checkRadioButton(widgets, video, bmp, RB_HELP_FEATURES);
	setHelpSubject(inst, widgets, video, 0);
	writeHelp(video, bmp);
}

void rbHelpEffects(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	checkRadioButton(widgets, video, bmp, RB_HELP_EFFECTS);
	setHelpSubject(inst, widgets, video, 1);
	writeHelp(video, bmp);
}

void rbHelpKeybindings(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	checkRadioButton(widgets, video, bmp, RB_HELP_KEYBINDINGS);
	setHelpSubject(inst, widgets, video, 2);
	writeHelp(video, bmp);
}

void rbHelpHowToUseFT2(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	checkRadioButton(widgets, video, bmp, RB_HELP_HOWTO);
	setHelpSubject(inst, widgets, video, 3);
	writeHelp(video, bmp);
}

void rbHelpPlugin(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (inst == NULL) return;
	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	ft2_widgets_t *widgets = (ui != NULL) ? &ui->widgets : NULL;
	checkRadioButton(widgets, video, bmp, RB_HELP_PLUGIN);
	setHelpSubject(inst, widgets, video, 4);
	writeHelp(video, bmp);
}

/* ============ INITIALIZATION ============ */

void initFTHelp(void)
{
	if (helpInitialized)
		return;

	readHelp();
	fHlp_Num = 0;
	fHlp_Line = 0;
	helpInitialized = true;
}

void windUpFTHelp(void)
{
	for (int16_t i = 0; i < MAX_SUBJ; i++)
{
		if (subjPtrArr[i] != NULL)
		{
			free(subjPtrArr[i]);
			subjPtrArr[i] = NULL;
		}
	}
	helpInitialized = false;
}
