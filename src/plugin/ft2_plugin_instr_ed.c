/**
 * @file ft2_plugin_instr_ed.c
 * @brief Exact port of instrument editor from ft2_inst_ed.c
 *
 * This is a full port of the FT2 instrument editor drawing code, adapted for
 * the plugin architecture with instance-aware state.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_checkboxes.h"
#include "ft2_plugin_radiobuttons.h"
#include "ft2_plugin_config.h"
#include "ft2_plugin_widgets.h"
#include "ft2_plugin_replayer.h"
#include "ft2_plugin_pattern_ed.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_ui.h"
#include "ft2_instance.h"

/* Forward declarations */
static ft2_instr_t *getInstrForInst(ft2_instance_t *inst);

/* ============ ENVELOPE PRESET FUNCTIONS ============ */

/* Apply volume envelope preset to instrument - matches standalone setStdVolEnvelope() */
void setStdVolEnvelope(ft2_instance_t *inst, uint8_t num)
{
	if (inst == NULL || num >= 6)
		return;
	
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	ft2_plugin_config_t *cfg = &inst->config;
	
	/* Stop voices to prevent race conditions */
	ft2_stop_all_voices(inst);
	
	ins->fadeout = cfg->stdFadeout[num];
	ins->volEnvSustain = (uint8_t)cfg->stdVolEnvSustain[num];
	ins->volEnvLoopStart = (uint8_t)cfg->stdVolEnvLoopStart[num];
	ins->volEnvLoopEnd = (uint8_t)cfg->stdVolEnvLoopEnd[num];
	ins->volEnvLength = (uint8_t)cfg->stdVolEnvLength[num];
	ins->volEnvFlags = (uint8_t)cfg->stdVolEnvFlags[num];
	ins->autoVibRate = (uint8_t)cfg->stdVibRate[num];
	ins->autoVibDepth = (uint8_t)cfg->stdVibDepth[num];
	ins->autoVibSweep = (uint8_t)cfg->stdVibSweep[num];
	ins->autoVibType = (uint8_t)cfg->stdVibType[num];
	
	memcpy(ins->volEnvPoints, cfg->stdEnvPoints[num][0], sizeof(int16_t) * 12 * 2);
}

/* Apply panning envelope preset to instrument - matches standalone setStdPanEnvelope() */
void setStdPanEnvelope(ft2_instance_t *inst, uint8_t num)
{
	if (inst == NULL || num >= 6)
		return;
	
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	ft2_plugin_config_t *cfg = &inst->config;
	
	/* Stop voices to prevent race conditions */
	ft2_stop_all_voices(inst);
	
	ins->panEnvLength = (uint8_t)cfg->stdPanEnvLength[num];
	ins->panEnvSustain = (uint8_t)cfg->stdPanEnvSustain[num];
	ins->panEnvLoopStart = (uint8_t)cfg->stdPanEnvLoopStart[num];
	ins->panEnvLoopEnd = (uint8_t)cfg->stdPanEnvLoopEnd[num];
	ins->panEnvFlags = (uint8_t)cfg->stdPanEnvFlags[num];
	
	memcpy(ins->panEnvPoints, cfg->stdEnvPoints[num][1], sizeof(int16_t) * 12 * 2);
}

/* Store or recall volume envelope preset - matches standalone setOrStoreVolEnvPreset() */
void setOrStoreVolEnvPreset(ft2_instance_t *inst, uint8_t num)
{
	if (inst == NULL || num >= 6)
		return;
	
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	ft2_plugin_config_t *cfg = &inst->config;
	
	if (isMouseRightButtonReleased())
	{
		/* Store preset */
		cfg->stdFadeout[num] = ins->fadeout;
		cfg->stdVolEnvSustain[num] = ins->volEnvSustain;
		cfg->stdVolEnvLoopStart[num] = ins->volEnvLoopStart;
		cfg->stdVolEnvLoopEnd[num] = ins->volEnvLoopEnd;
		cfg->stdVolEnvLength[num] = ins->volEnvLength;
		cfg->stdVolEnvFlags[num] = ins->volEnvFlags;
		cfg->stdVibRate[num] = ins->autoVibRate;
		cfg->stdVibDepth[num] = ins->autoVibDepth;
		cfg->stdVibSweep[num] = ins->autoVibSweep;
		cfg->stdVibType[num] = ins->autoVibType;
		
		memcpy(cfg->stdEnvPoints[num][0], ins->volEnvPoints, sizeof(int16_t) * 12 * 2);
	}
	else if (isMouseLeftButtonReleased())
	{
		/* Recall preset */
		setStdVolEnvelope(inst, num);
		inst->uiState.updateInstEditor = true;
	}
}

/* Store or recall panning envelope preset - matches standalone setOrStorePanEnvPreset() */
void setOrStorePanEnvPreset(ft2_instance_t *inst, uint8_t num)
{
	if (inst == NULL || num >= 6)
		return;
	
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	ft2_plugin_config_t *cfg = &inst->config;
	
	if (isMouseRightButtonReleased())
	{
		/* Store preset */
		cfg->stdFadeout[num] = ins->fadeout;
		cfg->stdPanEnvSustain[num] = ins->panEnvSustain;
		cfg->stdPanEnvLoopStart[num] = ins->panEnvLoopStart;
		cfg->stdPanEnvLoopEnd[num] = ins->panEnvLoopEnd;
		cfg->stdPanEnvLength[num] = ins->panEnvLength;
		cfg->stdPanEnvFlags[num] = ins->panEnvFlags;
		cfg->stdVibRate[num] = ins->autoVibRate;
		cfg->stdVibDepth[num] = ins->autoVibDepth;
		cfg->stdVibSweep[num] = ins->autoVibSweep;
		cfg->stdVibType[num] = ins->autoVibType;
		
		memcpy(cfg->stdEnvPoints[num][1], ins->panEnvPoints, sizeof(int16_t) * 12 * 2);
	}
	else if (isMouseLeftButtonReleased())
	{
		/* Recall preset */
		setStdPanEnvelope(inst, num);
		inst->uiState.updateInstEditor = true;
	}
}

/* Piano key lookup tables - exact match to original */
static const bool keyIsBlackTab[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
static const uint8_t whiteKeyIndex[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const uint16_t whiteKeysBmpOrder[12] = { 0, 0, 506, 0, 1012, 0, 0, 506, 0, 506, 0, 1012 };
static const uint8_t keyXPos[12] = { 8, 15, 19, 26, 30, 41, 48, 52, 59, 63, 70, 74 };

/* Mouse X to piano key lookup (for top part of piano with black keys) - exact match to original */
static const uint8_t mx2PianoKey[77] =
{
	0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4,
	4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,
	9,9,9,9,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11
};

/* Note tables - same as pattern editor */
static const uint8_t noteTab1[96] = 
{
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
};

static const uint8_t noteTab2[96] =
{
	0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,
	4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,
};

/* Current instrument editor for callbacks */
/* Global pointer removed - use FT2_INSTR_ED(inst) macro instead */

/* ============ STATIC DRAWING FUNCTIONS ============ */

/* Draw a pixel in the envelope area - exact match to standalone */
static void envelopePixel(ft2_video_t *video, int32_t envNum, int32_t x, int32_t y, uint8_t paletteIndex)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	int32_t screenY = y + ((envNum == 0) ? VOL_ENV_Y : PAN_ENV_Y);

	if (x >= 0 && x < SCREEN_W && screenY >= 0 && screenY < SCREEN_H)
		video->frameBuffer[(screenY * SCREEN_W) + x] = video->palette[paletteIndex];
}

/* Draw a line in the envelope area using array indexing (safer than pointer arithmetic) */
static void envelopeLine(ft2_video_t *video, int32_t envNum, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t pal)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	/* Clamp coordinates */
	if (y1 < 0) y1 = 0; if (y1 > 66) y1 = 66;
	if (y2 < 0) y2 = 0; if (y2 > 66) y2 = 66;
	if (x1 < 0) x1 = 0; if (x1 > 335) x1 = 335;
	if (x2 < 0) x2 = 0; if (x2 > 335) x2 = 335;

	/* Add envelope Y offset */
	const int32_t baseY = (envNum == 0) ? 189 : 276;
	int32_t iy1 = y1 + baseY;
	int32_t iy2 = y2 + baseY;
	int32_t ix1 = x1;
	int32_t ix2 = x2;

	/* Use int32_t for all Bresenham variables to avoid overflow issues */
	const int32_t dx = ix2 - ix1;
	const int32_t ax = ABS(dx) * 2;
	const int32_t sx = SGN(dx);
	const int32_t dy = iy2 - iy1;
	const int32_t ay = ABS(dy) * 2;
	const int32_t sy = SGN(dy);
	int32_t x = ix1;
	int32_t y = iy1;

	const uint32_t pal1 = video->palette[PAL_BLCKMRK];
	const uint32_t pal2 = video->palette[PAL_BLCKTXT];
	const uint32_t pixVal = video->palette[pal];

	/* Draw line using array indexing (like sampleLine in sample editor) */
	if (ax > ay)
	{
		int32_t d = ay - (ax / 2);
		while (true)
		{
			/* Bounds check using coordinates, then array index */
			if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
			{
				uint32_t *pixel = &video->frameBuffer[(y * SCREEN_W) + x];
				if (*pixel != pal2)
			{
					if (*pixel == pal1)
						*pixel = pal2;
				else
						*pixel = pixVal;
				}
			}

			if (x == ix2)
				break;

			if (d >= 0)
			{
				y += sy;
				d -= ax;
			}

			x += sx;
			d += ay;
		}
	}
	else
	{
		int32_t d = ax - (ay / 2);
		while (true)
		{
			/* Bounds check using coordinates, then array index */
			if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
			{
				uint32_t *pixel = &video->frameBuffer[(y * SCREEN_W) + x];
				if (*pixel != pal2)
			{
					if (*pixel == pal1)
						*pixel = pal2;
				else
						*pixel = pixVal;
				}
			}

			if (y == iy2)
				break;

			if (d >= 0)
			{
				x += sx;
				d -= ay;
			}

			y += sy;
			d += ax;
		}
	}
}

/* Draw an envelope point marker */
static void drawEnvPoint(ft2_video_t *video, int32_t envNum, int32_t x, int32_t y, bool selected)
{
	if (video == NULL)
		return;

	int32_t baseY = (envNum == 0) ? VOL_ENV_Y : PAN_ENV_Y;
	int32_t screenX = 5 + x;
	int32_t screenY = baseY + y;

	uint8_t color = selected ? PAL_PATTEXT : PAL_PATTEXT;
	uint32_t pixVal = video->palette[color];

	/* Draw 3x3 marker */
	for (int32_t dy = -1; dy <= 1; dy++)
	{
		for (int32_t dx = -1; dx <= 1; dx++)
		{
			int32_t px = screenX + dx;
			int32_t py = screenY + dy;
			if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
			{
				if (selected)
					video->frameBuffer[(py * SCREEN_W) + px] = video->palette[PAL_FORGRND];
				else if (dx == 0 || dy == 0)
					video->frameBuffer[(py * SCREEN_W) + px] = pixVal;
			}
		}
	}
}

/* Key digit X positions for sample numbers - exact match to standalone */
static const uint8_t keyDigitXPos[12] = { 11, 16, 22, 27, 33, 44, 49, 55, 60, 66, 71, 77 };

/* Draw piano number (hex digit 0-F) using array indexing (safer than pointer arithmetic) */
static void pianoNumberOut(ft2_video_t *video, const ft2_bmp_t *bmp, uint16_t xPos, uint16_t yPos, 
                           uint8_t fgPalette, uint8_t bgPalette, uint8_t val)
{
	if (video == NULL || video->frameBuffer == NULL || bmp == NULL || bmp->font8 == NULL)
		return;

	if (val > 0xF)
		val = 0xF;

	const uint32_t fg = video->palette[fgPalette];
	const uint32_t bg = video->palette[bgPalette];
	const uint8_t *srcPtr = &bmp->font8[val * FONT8_CHAR_W];

	/* Draw using array indexing with per-pixel bounds check */
	for (int32_t dy = 0; dy < FONT8_CHAR_H; dy++)
	{
		int32_t py = yPos + dy;
		if (py < 0 || py >= SCREEN_H)
		{
			srcPtr += FONT8_WIDTH;
			continue;
		}

		for (int32_t dx = 0; dx < FONT8_CHAR_W; dx++)
		{
			int32_t px = xPos + dx;
			if (px >= 0 && px < SCREEN_W)
				video->frameBuffer[(py * SCREEN_W) + px] = srcPtr[dx] ? fg : bg;
		}

		srcPtr += FONT8_WIDTH;
	}
}

/* Write sample number on piano key - exact match to standalone writePianoNumber() */
static void writePianoNumber(ft2_instance_t *inst, uint8_t note, uint8_t key, uint8_t octave)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	if (video == NULL || video->frameBuffer == NULL)
		return;

	int16_t curInstr = inst->editor.curInstr;

	uint8_t number = 0;
	if (curInstr > 0 && curInstr < 128)
	{
		ft2_instr_t *ins = inst->replayer.instr[curInstr];
		if (ins != NULL)
			number = ins->note2SampleLUT[note];
	}

	const uint16_t x = keyDigitXPos[key] + (octave * 77);

	if (keyIsBlackTab[key])
		pianoNumberOut(video, bmp, x, 361, PAL_FORGRND, PAL_BCKGRND, number);
	else
		pianoNumberOut(video, bmp, x, 385, PAL_BCKGRND, PAL_FORGRND, number);
}

/* Draw a white piano key - exact match to standalone using bitmap */
static void drawWhitePianoKey(ft2_video_t *video, int key, int octave, bool keyDown, const ft2_bmp_t *bmp)
{
	if (video == NULL || video->frameBuffer == NULL || bmp == NULL || bmp->whitePianoKeys == NULL)
		return;
	const uint16_t x = keyXPos[key] + (octave * 77);
		const uint8_t *src = &bmp->whitePianoKeys[(keyDown * (11*46*3)) + whiteKeysBmpOrder[key]];
		blit(video, x, 351, src, 11, 46);
}

/* Draw a black piano key - exact match to standalone using bitmap */
static void drawBlackPianoKey(ft2_video_t *video, int key, int octave, bool keyDown, const ft2_bmp_t *bmp)
{
	if (video == NULL || video->frameBuffer == NULL || bmp == NULL || bmp->blackPianoKeys == NULL)
		return;
	const uint16_t x = keyXPos[key] + (octave * 77);
		const uint8_t *src = &bmp->blackPianoKeys[keyDown * (7*27)];
		blit(video, x, 351, src, 7, 27);
}

/* ============ ENVELOPE COORDINATE DISPLAY ============ */

/* Draw volume envelope coordinates (tick/value) - exact match to standalone drawVolEnvCoords() */
static void drawVolEnvCoords(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t tick, int16_t val)
{
	if (video == NULL || bmp == NULL)
		return;

	char str[8];

	if (tick < 0) tick = 0;
	if (tick > 324) tick = 324;
	snprintf(str, sizeof(str), "%03d", tick);
	textOutTinyOutline(video, bmp, 326, 190, str);

	if (val < 0) val = 0;
	if (val > 64) val = 64;
	snprintf(str, sizeof(str), "%02d", val);
	textOutTinyOutline(video, bmp, 330, 198, str);
}

/* Draw panning envelope coordinates (tick/value) - exact match to standalone drawPanEnvCoords() */
static void drawPanEnvCoords(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t tick, int16_t val)
{
	if (video == NULL || bmp == NULL)
		return;

	char str[8];

	if (tick < 0) tick = 0;
	if (tick > 324) tick = 324;
	snprintf(str, sizeof(str), "%03d", tick);
	textOutTinyOutline(video, bmp, 326, 277, str);

	bool negative = false;
	val -= 32;
	if (val < -32) val = -32;
	if (val > 31) val = 31;
	if (val < 0)
	{
		negative = true;
		val = -val;
	}

	if (negative)
	{
		/* Draw minus sign with outline */
		hLine(video, 326, 287, 3, PAL_BCKGRND);
		hLine(video, 326, 289, 3, PAL_BCKGRND);
		video->frameBuffer[(288 * SCREEN_W) + 325] = video->palette[PAL_BCKGRND];
		video->frameBuffer[(288 * SCREEN_W) + 329] = video->palette[PAL_BCKGRND];
		hLine(video, 326, 288, 3, PAL_FORGRND);
	}

	snprintf(str, sizeof(str), "%02d", val);
	textOutTinyOutline(video, bmp, 330, 285, str);
}

/* ============ PUBLIC FUNCTIONS ============ */

void ft2_instr_ed_init(ft2_instrument_editor_t *editor)
{
	if (editor == NULL)
		return;

	memset(editor, 0, sizeof(ft2_instrument_editor_t));
	editor->draggingVolEnv = false;
	editor->draggingPanEnv = false;
	memset(editor->pianoKeyStatus, 0, sizeof(editor->pianoKeyStatus));
}


/* Draw 3x3 envelope point dot using array indexing (safer than pointer arithmetic) */
static void envelopeDot(ft2_video_t *video, int32_t envNum, int32_t x, int32_t y)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	y += (envNum == 0) ? VOL_ENV_Y : PAN_ENV_Y;

	const uint32_t pixVal = video->palette[PAL_BLCKTXT];

	/* Draw 3x3 dot using array indexing with per-pixel bounds check */
	for (int32_t dy = 0; dy < 3; dy++)
	{
		int32_t py = y + dy;
		if (py < 0 || py >= SCREEN_H)
			continue;

		for (int32_t dx = 0; dx < 3; dx++)
	{
			int32_t px = x + dx;
			if (px >= 0 && px < SCREEN_W)
				video->frameBuffer[(py * SCREEN_W) + px] = pixVal;
		}
	}
}

/* Draw dotted vertical line in envelope area using array indexing (safer than pointer arithmetic) */
static void envelopeVertLine(ft2_video_t *video, int32_t envNum, int32_t x, int32_t y, uint8_t pal)
{
	if (video == NULL || video->frameBuffer == NULL)
		return;

	y += (envNum == 0) ? VOL_ENV_Y : PAN_ENV_Y;

	/* X bounds check */
	if (x < 0 || x >= SCREEN_W)
		return;

	const uint32_t pixVal1 = video->palette[pal];
	const uint32_t pixVal2 = video->palette[PAL_BLCKTXT];

	/* Draw 33 dots with stride of 2 using array indexing */
	int32_t py = y;
	for (int32_t i = 0; i < 33; i++)
	{
		if (py >= 0 && py < SCREEN_H)
		{
			uint32_t *pixel = &video->frameBuffer[(py * SCREEN_W) + x];
			if (*pixel != pixVal2)
				*pixel = pixVal1;
		}
		py += 2;
	}
}

void ft2_instr_ed_draw_envelope(ft2_instance_t *inst, int envNum)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	if (video == NULL || video->frameBuffer == NULL)
		return;

	/* Clear envelope area - exact match to standalone */
	int32_t baseY = (envNum == 0) ? 189 : 276;
	clearRect(video, 5, baseY, 333, 67);

	/* Draw dotted x/y lines - exact match to standalone
	 * NOTE: Using while(true)+break pattern to avoid MSVC optimizer bug
	 * that miscompiles for(i=0; i<=N; i++) loops into infinite loops */
	{
		int32_t i = 0;
		while (true)
		{
		envelopePixel(video, envNum, 5, 1 + i * 2, PAL_PATTEXT);
			if (i == 32) break;
			i++;
		}
	}
	{
		int32_t i = 0;
		while (true)
		{
		envelopePixel(video, envNum, 4, 1 + i * 8, PAL_PATTEXT);
			if (i == 8) break;
			i++;
		}
	}
	{
		int32_t i = 0;
		while (true)
		{
		envelopePixel(video, envNum, 8 + i * 2, 65, PAL_PATTEXT);
			if (i == 162) break;
			i++;
		}
	}
	{
		int32_t i = 0;
		while (true)
		{
		envelopePixel(video, envNum, 8 + i * 50, 66, PAL_PATTEXT);
			if (i == 6) break;
			i++;
		}
	}

	/* Draw center line on pan envelope */
	if (envNum == 1)
		envelopeLine(video, envNum, 8, 33, 332, 33, PAL_BLCKMRK);

	/* Get instrument from instance editor state */
	int16_t curInstr = inst->editor.curInstr;
	if (curInstr <= 0 || curInstr >= 128)
		return;

	ft2_instr_t *ins = inst->replayer.instr[curInstr];
	if (ins == NULL)
		return;

	/* Collect variables - exact match to standalone */
	int16_t nd, sp, ls, le;
	int16_t (*curEnvP)[2];
	int8_t selected;

	if (envNum == 0) /* Volume envelope */
	{
		nd = ins->volEnvLength;
		sp = (ins->volEnvFlags & ENV_SUSTAIN) ? ins->volEnvSustain : -1;
		if (ins->volEnvFlags & ENV_LOOP)
		{
			ls = ins->volEnvLoopStart;
			le = ins->volEnvLoopEnd;
		}
		else
		{
			ls = -1;
			le = -1;
		}
		curEnvP = ins->volEnvPoints;
		selected = inst->editor.currVolEnvPoint;
		if (selected < 0) selected = 0;
		if (selected >= 12) selected = 11;
	}
	else /* Panning envelope */
	{
		nd = ins->panEnvLength;
		sp = (ins->panEnvFlags & ENV_SUSTAIN) ? ins->panEnvSustain : -1;
		if (ins->panEnvFlags & ENV_LOOP)
		{
			ls = ins->panEnvLoopStart;
			le = ins->panEnvLoopEnd;
		}
		else
		{
			ls = -1;
			le = -1;
		}
		curEnvP = ins->panEnvPoints;
		selected = inst->editor.currPanEnvPoint;
		if (selected < 0) selected = 0;
		if (selected >= 12) selected = 11;
	}

	if (nd > 12)
		nd = 12;

	int16_t lx = 0;
	int16_t ly = 0;

	/* Draw envelope - exact match to standalone */
	for (int16_t i = 0; i < nd; i++)
	{
		int16_t x = curEnvP[i][0];
		int16_t y = curEnvP[i][1];

		if (x < 0) x = 0;
		if (x > 324) x = 324;

		if (envNum == 0)
		{
		if (y < 0) y = 0;
		if (y > 64) y = 64;
		}
		else
		{
			if (y < 0) y = 0;
			if (y > 63) y = 63;
		}

		if ((uint16_t)curEnvP[i][0] <= 324)
		{
			envelopeDot(video, envNum, 7 + x, 64 - y);
			
			/* Draw "envelope selected" data */
			if (i == selected)
			{
				envelopeLine(video, envNum, 5 + x, 64 - y, 5 + x, 66 - y, PAL_BLCKTXT);
				envelopeLine(video, envNum, 11 + x, 64 - y, 11 + x, 66 - y, PAL_BLCKTXT);
				envelopePixel(video, envNum, 5, 65 - y, PAL_BLCKTXT);
				envelopePixel(video, envNum, 8 + x, 65, PAL_BLCKTXT);
		}

			/* Draw loop start marker (triangle pointing down) */
			if (i == ls)
			{
				envelopeLine(video, envNum, x + 6, 1, x + 10, 1, PAL_PATTEXT);
				envelopeLine(video, envNum, x + 7, 2, x + 9, 2, PAL_PATTEXT);
				envelopeVertLine(video, envNum, x + 8, 1, PAL_PATTEXT);
	}

			/* Draw sustain marker (vertical line) */
			if (i == sp)
				envelopeVertLine(video, envNum, x + 8, 1, PAL_BLCKTXT);

			/* Draw loop end marker (triangle pointing up) */
			if (i == le)
			{
				envelopeLine(video, envNum, x + 6, 65, x + 10, 65, PAL_PATTEXT);
				envelopeLine(video, envNum, x + 7, 64, x + 9, 64, PAL_PATTEXT);
				envelopeVertLine(video, envNum, x + 8, 1, PAL_PATTEXT);
			}
		}

		/* Draw envelope line */
		if (i > 0 && lx < x)
			envelopeLine(video, envNum, lx + 8, 65 - ly, x + 8, 65 - y, PAL_PATTEXT);

		lx = x;
		ly = y;
	}
}

void ft2_instr_ed_draw_vol_env(ft2_instance_t *inst)
{
	ft2_instr_ed_draw_envelope(inst, 0);
}

void ft2_instr_ed_draw_pan_env(ft2_instance_t *inst)
{
	ft2_instr_ed_draw_envelope(inst, 1);
}

void ft2_instr_ed_draw_note_map(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	if (video == NULL || video->frameBuffer == NULL)
		return;
	
	drawFramework(video, 400, 189, 232, 67, FRAMEWORK_TYPE2);
		textOutShadow(video, bmp, 404, 193, PAL_FORGRND, PAL_DSKTOP2, "Note-Sample Map");

	/* Get instrument */
	int16_t curInstr = inst->editor.curInstr;
	if (curInstr <= 0 || curInstr >= 128)
		return;

	ft2_instr_t *ins = inst->replayer.instr[curInstr];
	if (ins == NULL)
		return;

	/* Draw a simplified note-sample indicator */
	/* Real FT2 shows a bar graph of sample assignments */
}

void ft2_instr_ed_draw_piano(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_instrument_editor_t *ed = FT2_INSTR_ED(inst);
	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);

	if (video == NULL || video->frameBuffer == NULL)
		return;

	/* Clear piano key status */
	memset(ed->pianoKeyStatus, 0, sizeof(ed->pianoKeyStatus));

	/* Draw all 96 keys - exact match to standalone redrawPiano() */
	for (uint8_t i = 0; i < 96; i++)
	{
		const uint8_t key = noteTab1[i];
		const uint8_t octave = noteTab2[i];

		if (keyIsBlackTab[key])
			drawBlackPianoKey(video, key, octave, false, bmp);
		else
			drawWhitePianoKey(video, key, octave, false, bmp);

		writePianoNumber(inst, i, key, octave);
	}
}

/* Update instrument editor - draw values and set widget states */
void updateInstEditor(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	if (video == NULL || video->frameBuffer == NULL)
		return;

	/* Get current instrument and sample from instance editor state */
	ft2_instr_t *ins = NULL;
	ft2_sample_t *s = NULL;
	int16_t curInstr = inst->editor.curInstr;
	int16_t curSmp = inst->editor.curSmp;

	if (curInstr > 0 && curInstr < 128)
	{
		ins = inst->replayer.instr[curInstr];
	}

	if (ins != NULL && curSmp >= 0 && curSmp < FT2_MAX_SMP_PER_INST)
	{
		s = &ins->smp[curSmp];
	}

	/* Draw Volume (hex at 505, 177) */
	if (s != NULL)
	{
		hexOutBg(video, bmp, 505, 177, PAL_FORGRND, PAL_DESKTOP, s->volume, 2);
	}
	else
	{
		hexOutBg(video, bmp, 505, 177, PAL_FORGRND, PAL_DESKTOP, 0, 2);
	}

	/* Draw Panning (hex at 505, 191) */
	if (s != NULL)
	{
		hexOutBg(video, bmp, 505, 191, PAL_FORGRND, PAL_DESKTOP, s->panning, 2);
	}
	else
	{
		hexOutBg(video, bmp, 505, 191, PAL_FORGRND, PAL_DESKTOP, 128, 2);
	}

	/* Draw Fine-tune (at 491, 205) */
	fillRect(video, 491, 205, 27, 8, PAL_DESKTOP);
	if (s != NULL)
			{
		int16_t ftune = s->finetune;
		if (ftune == 0)
		{
			charOut(video, bmp, 512, 205, PAL_FORGRND, '0');
		}
		else if (ftune > 0)
		{
			charOut(video, bmp, 491, 205, PAL_FORGRND, '+');
			hexOutBg(video, bmp, 498, 205, PAL_FORGRND, PAL_DESKTOP, ftune, 2);
		}
		else
		{
			charOut(video, bmp, 491, 205, PAL_FORGRND, '-');
			hexOutBg(video, bmp, 498, 205, PAL_FORGRND, PAL_DESKTOP, -ftune, 2);
		}
	}
	else
	{
		charOut(video, bmp, 512, 205, PAL_FORGRND, '0');
	}

	/* Draw Fadeout (hex at 498, 222) */
	if (ins != NULL)
	{
		hexOutBg(video, bmp, 498, 222, PAL_FORGRND, PAL_DESKTOP, ins->fadeout, 3);
	}
	else
	{
		hexOutBg(video, bmp, 498, 222, PAL_FORGRND, PAL_DESKTOP, 0, 3);
	}

	/* Draw Vib.speed (hex at 505, 236) */
	if (ins != NULL)
	{
		hexOutBg(video, bmp, 505, 236, PAL_FORGRND, PAL_DESKTOP, ins->autoVibRate, 2);
	}
	else
	{
		hexOutBg(video, bmp, 505, 236, PAL_FORGRND, PAL_DESKTOP, 0, 2);
	}

	/* Draw Vib.depth (hex at 512, 250) */
	if (ins != NULL)
	{
		hexOutBg(video, bmp, 512, 250, PAL_FORGRND, PAL_DESKTOP, ins->autoVibDepth, 1);
	}
	else
	{
		hexOutBg(video, bmp, 512, 250, PAL_FORGRND, PAL_DESKTOP, 0, 1);
	}

	/* Draw Vib.sweep (hex at 505, 264) */
	if (ins != NULL)
	{
		hexOutBg(video, bmp, 505, 264, PAL_FORGRND, PAL_DESKTOP, ins->autoVibSweep, 2);
	}
	else
	{
		hexOutBg(video, bmp, 505, 264, PAL_FORGRND, PAL_DESKTOP, 0, 2);
	}

	/* Draw C-4 rate (at 472, 299) */
	fillRect(video, 472, 299, 64, 8, PAL_DESKTOP);
	if (s != NULL)
	{
		/* Calculate C-4 frequency based on relative note and finetune */
		double dC4Hz = 8363.0;
		if (s->relativeNote != 0 || s->finetune != 0)
		{
			double dNote = s->relativeNote + (s->finetune / 128.0);
			dC4Hz = dC4Hz * pow(2.0, dNote / 12.0);
		}
		char freqStr[16];
		snprintf(freqStr, sizeof(freqStr), "%.0fHz", dC4Hz);
		textOut(video, bmp, 472, 299, PAL_FORGRND, freqStr);
			}
	else
	{
		textOut(video, bmp, 472, 299, PAL_FORGRND, "8363Hz");
		}

	/* Draw Relative note (at 600, 299) - exact match to standalone drawRelativeNote() */
	fillRect(video, 600, 299, 8 * 3, 8, PAL_BCKGRND);
	if (s != NULL)
	{
		int note2 = 48 + s->relativeNote;
		if (note2 < 0) note2 = 0;
		if (note2 > 119) note2 = 119;
		uint8_t note = (uint8_t)(note2 % 12);
		char octaChar = '0' + (note2 / 12);
		
		/* Use sharp note names - matches standalone when config.ptnAcc == 0 */
		static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
		static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };
		
		charOutBg(video, bmp, 600, 299, PAL_FORGRND, PAL_BCKGRND, sharpNote1Char[note]);
		charOutBg(video, bmp, 608, 299, PAL_FORGRND, PAL_BCKGRND, sharpNote2Char[note]);
		charOutBg(video, bmp, 616, 299, PAL_FORGRND, PAL_BCKGRND, octaChar);
	}
	else
	{
		charOutBg(video, bmp, 600, 299, PAL_FORGRND, PAL_BCKGRND, 'C');
		charOutBg(video, bmp, 608, 299, PAL_FORGRND, PAL_BCKGRND, '-');
		charOutBg(video, bmp, 616, 299, PAL_FORGRND, PAL_BCKGRND, '4');
	}

	/* Draw volume envelope sustain point (at 382, 206) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->volEnvSustain : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 206, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Draw volume envelope loop start (at 382, 233) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->volEnvLoopStart : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 233, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Draw volume envelope loop end (at 382, 247) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->volEnvLoopEnd : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 247, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Draw pan envelope sustain point (at 382, 293) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->panEnvSustain : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 293, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Draw pan envelope loop start (at 382, 320) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->panEnvLoopStart : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 320, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Draw pan envelope loop end (at 382, 334) - decimal, matches standalone */
	{
		char str[4];
		uint8_t val = (ins != NULL) ? ins->panEnvLoopEnd : 0;
		if (val > 99) val = 99;
		snprintf(str, sizeof(str), "%02d", val);
		textOutFixed(video, bmp, 382, 334, PAL_FORGRND, PAL_DESKTOP, str);
	}

	/* Update scrollbar positions */
	if (s != NULL)
	{
		setScrollBarPos(inst, widgets, video, SB_INST_VOL, s->volume, false);
		setScrollBarPos(inst, widgets, video, SB_INST_PAN, s->panning, false);
		setScrollBarPos(inst, widgets, video, SB_INST_FTUNE, 128 + s->finetune, false);
	}

	if (ins != NULL)
	{
		setScrollBarPos(inst, widgets, video, SB_INST_FADEOUT, ins->fadeout, false);
		setScrollBarPos(inst, widgets, video, SB_INST_VIBSPEED, ins->autoVibRate, false);
		setScrollBarPos(inst, widgets, video, SB_INST_VIBDEPTH, ins->autoVibDepth, false);
		setScrollBarPos(inst, widgets, video, SB_INST_VIBSWEEP, ins->autoVibSweep, false);

		/* Update radio buttons for vibrato type */
		uncheckRadioButtonGroup(widgets, RB_GROUP_INST_WAVEFORM);
		uint16_t rbID;
		switch (ins->autoVibType)
		{
			default:
			case 0: rbID = RB_INST_WAVE_SINE;   break;
			case 1: rbID = RB_INST_WAVE_SQUARE; break;
			case 2: rbID = RB_INST_WAVE_RAMPDN; break;
			case 3: rbID = RB_INST_WAVE_RAMPUP; break;
		}
		if (rbID < NUM_RADIOBUTTONS)
			widgets->radioButtonState[rbID] = RADIOBUTTON_CHECKED;

		/* Update checkboxes for envelope enable */
		if (CB_INST_VENV < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_VENV] = (ins->volEnvFlags & ENV_ENABLED) ? true : false;
		if (CB_INST_VENV_SUS < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_VENV_SUS] = (ins->volEnvFlags & ENV_SUSTAIN) ? true : false;
		if (CB_INST_VENV_LOOP < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_VENV_LOOP] = (ins->volEnvFlags & ENV_LOOP) ? true : false;
		if (CB_INST_PENV < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_PENV] = (ins->panEnvFlags & ENV_ENABLED) ? true : false;
		if (CB_INST_PENV_SUS < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_PENV_SUS] = (ins->panEnvFlags & ENV_SUSTAIN) ? true : false;
		if (CB_INST_PENV_LOOP < NUM_CHECKBOXES)
			widgets->checkBoxChecked[CB_INST_PENV_LOOP] = (ins->panEnvFlags & ENV_LOOP) ? true : false;
	}
}

void ft2_instr_ed_draw(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	if (video == NULL || video->frameBuffer == NULL)
		return;

	/* Read current instrument/sample from instance editor state */
	int16_t curInstr = inst->editor.curInstr;

	/* Allocate instrument if needed (user selected an empty slot) */
	if (curInstr > 0 && curInstr <= 128)
	{
		if (inst->replayer.instr[curInstr] == NULL)
			ft2_instance_alloc_instr(inst, curInstr);
	}

	/* Draw frameworks - exact match to standalone showInstEditor() */
	drawFramework(video, 0,   173, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(video, 0,   260, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(video, 0,   347, 632,  53, FRAMEWORK_TYPE1);
	drawFramework(video, 438, 173, 194,  45, FRAMEWORK_TYPE1);
	drawFramework(video, 438, 218, 194,  76, FRAMEWORK_TYPE1);
	drawFramework(video, 438, 294, 194,  53, FRAMEWORK_TYPE1);
	drawFramework(video, 2,   188, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(video, 2,   275, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(video, 2,   349, 628,  49, FRAMEWORK_TYPE2);
	drawFramework(video, 593, 296,  36,  15, FRAMEWORK_TYPE2);

	/* Draw text labels - exact match to standalone showInstEditor() */
		textOutShadow(video, bmp, 20,  176, PAL_FORGRND, PAL_DSKTOP2, "Volume envelope:");
		textOutShadow(video, bmp, 153, 176, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
		textOutShadow(video, bmp, 358, 194, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
		textOutShadow(video, bmp, 342, 206, PAL_FORGRND, PAL_DSKTOP2, "Point");
		textOutShadow(video, bmp, 358, 219, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
		textOutShadow(video, bmp, 342, 233, PAL_FORGRND, PAL_DSKTOP2, "Start");
		textOutShadow(video, bmp, 342, 247, PAL_FORGRND, PAL_DSKTOP2, "End");
		textOutShadow(video, bmp, 20,  263, PAL_FORGRND, PAL_DSKTOP2, "Panning envelope:");
		textOutShadow(video, bmp, 152, 263, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
		textOutShadow(video, bmp, 358, 281, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
		textOutShadow(video, bmp, 342, 293, PAL_FORGRND, PAL_DSKTOP2, "Point");
		textOutShadow(video, bmp, 358, 306, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
		textOutShadow(video, bmp, 342, 320, PAL_FORGRND, PAL_DSKTOP2, "Start");
		textOutShadow(video, bmp, 342, 334, PAL_FORGRND, PAL_DSKTOP2, "End");
		textOutShadow(video, bmp, 443, 177, PAL_FORGRND, PAL_DSKTOP2, "Volume");
		textOutShadow(video, bmp, 443, 191, PAL_FORGRND, PAL_DSKTOP2, "Panning");
		textOutShadow(video, bmp, 443, 205, PAL_FORGRND, PAL_DSKTOP2, "F.tune");
		textOutShadow(video, bmp, 442, 222, PAL_FORGRND, PAL_DSKTOP2, "Fadeout");
		textOutShadow(video, bmp, 442, 236, PAL_FORGRND, PAL_DSKTOP2, "Vib.speed");
		textOutShadow(video, bmp, 442, 250, PAL_FORGRND, PAL_DSKTOP2, "Vib.depth");
		textOutShadow(video, bmp, 442, 264, PAL_FORGRND, PAL_DSKTOP2, "Vib.sweep");
		textOutShadow(video, bmp, 442, 299, PAL_FORGRND, PAL_DSKTOP2, "C-4=");
		textOutShadow(video, bmp, 537, 299, PAL_FORGRND, PAL_DSKTOP2, "Rel. note");

		/* Draw vibrato waveforms */
			blitFast(video, 455, 279, &bmp->vibratoWaveforms[0*(12*10)], 12, 10);
			blitFast(video, 485, 279, &bmp->vibratoWaveforms[1*(12*10)], 12, 10);
			blitFast(video, 515, 279, &bmp->vibratoWaveforms[2*(12*10)], 12, 10);
			blitFast(video, 545, 279, &bmp->vibratoWaveforms[3*(12*10)], 12, 10);

	/* Draw envelopes */
	ft2_instr_ed_draw_vol_env(inst);
	ft2_instr_ed_draw_pan_env(inst);

	/* Draw envelope coordinates (tick/value display) */
	{
		ft2_instr_t *ins = NULL;
		if (curInstr > 0 && curInstr < 128)
			ins = inst->replayer.instr[curInstr];

		int16_t volTick = 0, volVal = 0;
		int16_t panTick = 0, panVal = 32;

		int8_t currVolEnvPoint = inst->editor.currVolEnvPoint;
		int8_t currPanEnvPoint = inst->editor.currPanEnvPoint;

		/* Bounds check envelope point indices */
		if (currVolEnvPoint < 0) currVolEnvPoint = 0;
		if (currVolEnvPoint >= 12) currVolEnvPoint = 11;
		if (currPanEnvPoint < 0) currPanEnvPoint = 0;
		if (currPanEnvPoint >= 12) currPanEnvPoint = 11;

		if (ins != NULL && ins->volEnvLength > 0)
		{
			if (currVolEnvPoint >= ins->volEnvLength)
				currVolEnvPoint = ins->volEnvLength - 1;
			volTick = ins->volEnvPoints[currVolEnvPoint][0];
			volVal = ins->volEnvPoints[currVolEnvPoint][1];
		}

		if (ins != NULL && ins->panEnvLength > 0)
		{
			if (currPanEnvPoint >= ins->panEnvLength)
				currPanEnvPoint = ins->panEnvLength - 1;
			panTick = ins->panEnvPoints[currPanEnvPoint][0];
			panVal = ins->panEnvPoints[currPanEnvPoint][1];
		}

		drawVolEnvCoords(video, bmp, volTick, volVal);
		drawPanEnvCoords(video, bmp, panTick, panVal);
	}

	/* Draw piano */
	ft2_instr_ed_draw_piano(inst);

	/* Call update to draw value displays */
	updateInstEditor(inst);
}

void ft2_instr_ed_mouse_click(ft2_instance_t *inst, int x, int y, int button)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_instrument_editor_t *editor = FT2_INSTR_ED(inst);
	editor->lastMouseX = x;
	editor->lastMouseY = y;

	/* Check if click is in volume envelope area - exact match to standalone */
	if (y >= VOL_ENV_Y && y <= VOL_ENV_Y + ENV_HEIGHT && x >= 7 && x <= 334)
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL || ins->volEnvLength == 0)
			return;
		
		uint8_t ant = ins->volEnvLength;
		if (ant > 12)
			ant = 12;
		
		/* Search for a point within ±2 pixels of the click */
		for (uint8_t i = 0; i < ant; i++)
		{
			const int32_t px = 8 + ins->volEnvPoints[i][0];
			const int32_t py = VOL_ENV_Y + 1 + (64 - ins->volEnvPoints[i][1]);
			
			if (x >= px - 2 && x <= px + 2 && y >= py - 2 && y <= py + 2)
			{
				inst->editor.currVolEnvPoint = i;
				editor->saveMouseX = 8 + (editor->lastMouseX - px);
				editor->saveMouseY = (VOL_ENV_Y + 1) + (editor->lastMouseY - py);
				editor->draggingVolEnv = true;
				inst->uiState.updateInstEditor = true;
				return;
			}
		}
		return;
	}

	/* Check if click is in panning envelope area - exact match to standalone */
	if (y >= PAN_ENV_Y && y <= PAN_ENV_Y + ENV_HEIGHT && x >= 7 && x <= 334)
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL || ins->panEnvLength == 0)
			return;
		
		uint8_t ant = ins->panEnvLength;
		if (ant > 12)
			ant = 12;
		
		/* Search for a point within ±2 pixels of the click */
		for (uint8_t i = 0; i < ant; i++)
		{
			const int32_t px = 8 + ins->panEnvPoints[i][0];
			const int32_t py = PAN_ENV_Y + 1 + (63 - ins->panEnvPoints[i][1]);
			
			if (x >= px - 2 && x <= px + 2 && y >= py - 2 && y <= py + 2)
			{
				inst->editor.currPanEnvPoint = i;
				editor->saveMouseX = editor->lastMouseX - px + 8;
				editor->saveMouseY = editor->lastMouseY - py + (PAN_ENV_Y + 1);
				editor->draggingPanEnv = true;
				inst->uiState.updateInstEditor = true;
				return;
			}
		}
		return;
	}

	/* Check if click is on piano - assign current sample to the clicked key */
	if (y >= PIANO_Y && y < PIANO_Y + PIANOKEY_WHITE_H && x >= PIANO_X && x < PIANO_X + (77 * PIANO_OCTAVES))
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL)
			return;
		
		int32_t mx = x - PIANO_X;
		int32_t my = y;
		
		const int32_t quotient = mx / 77;
		const int32_t remainder = mx % 77;
		
		uint8_t octave, key;
		
		/* Black keys threshold: y < 378 (PIANO_Y + 27) for black key area */
		if (my < PIANO_Y + PIANOKEY_BLACK_H)
		{
			/* White keys and black keys (top part) */
			octave = (uint8_t)quotient;
			key = mx2PianoKey[remainder];
		}
		else
		{
			/* White keys only (bottom part) */
			const int32_t whiteKeyWidth = 11;
			octave = (uint8_t)quotient;
			key = whiteKeyIndex[remainder / whiteKeyWidth];
		}
		
		/* Calculate note (0-95 range) and assign current sample to it */
		const uint8_t note = (octave * 12) + key;
		if (note < 96 && ins->note2SampleLUT[note] != inst->editor.curSmp)
		{
			ins->note2SampleLUT[note] = inst->editor.curSmp;
			/* Mark piano for redraw to show the new sample number */
			inst->uiState.updateInstEditor = true;
		}
		
		editor->draggingPiano = true;
		return;
	}

	(void)button;
}

void ft2_instr_ed_mouse_drag(ft2_instance_t *inst, int x, int y)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_instrument_editor_t *editor = FT2_INSTR_ED(inst);

	/* Handle piano dragging - assign samples to keys */
	if (editor->draggingPiano)
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL)
			return;
		
		/* Clamp mouse coordinates to piano area */
		int32_t mx = x - PIANO_X;
		int32_t my = y;
		
		mx = (mx < 0) ? 0 : ((mx >= 77 * PIANO_OCTAVES) ? (77 * PIANO_OCTAVES - 1) : mx);
		my = (my < PIANO_Y) ? PIANO_Y : ((my >= PIANO_Y + PIANOKEY_WHITE_H) ? (PIANO_Y + PIANOKEY_WHITE_H - 1) : my);
		
		const int32_t quotient = mx / 77;
		const int32_t remainder = mx % 77;
		
		uint8_t octave, key;
		
		if (my < PIANO_Y + PIANOKEY_BLACK_H)
		{
			octave = (uint8_t)quotient;
			key = mx2PianoKey[remainder];
		}
		else
		{
			const int32_t whiteKeyWidth = 11;
			octave = (uint8_t)quotient;
			key = whiteKeyIndex[remainder / whiteKeyWidth];
		}
		
		const uint8_t note = (octave * 12) + key;
		if (note < 96 && ins->note2SampleLUT[note] != inst->editor.curSmp)
		{
			ins->note2SampleLUT[note] = inst->editor.curSmp;
			/* Mark piano for redraw */
			inst->uiState.updateInstEditor = true;
		}
		return;
	}

	/* Handle volume envelope point dragging - exact match to standalone */
	if (editor->draggingVolEnv)
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL || ins->volEnvLength == 0)
			return;
		
		uint8_t ant = ins->volEnvLength;
		if (ant > 12)
			ant = 12;
		
		int32_t mx = x;
		int32_t my = y;
		
		/* Handle X movement - constrain between adjacent points */
		int8_t currVolEnvPoint = inst->editor.currVolEnvPoint;
		if (currVolEnvPoint < 0) currVolEnvPoint = 0;
		if (currVolEnvPoint >= 12) currVolEnvPoint = 11;

		if (mx != editor->lastMouseX)
		{
			editor->lastMouseX = mx;
			
			if (ant > 1 && currVolEnvPoint > 0)
			{
				mx -= editor->saveMouseX;
				if (mx < 0) mx = 0;
				if (mx > 324) mx = 324;
				
				int32_t minX, maxX;
				if (currVolEnvPoint == ant - 1)
				{
					minX = ins->volEnvPoints[currVolEnvPoint - 1][0] + 1;
					maxX = 324;
				}
				else
				{
					minX = ins->volEnvPoints[currVolEnvPoint - 1][0] + 1;
					maxX = ins->volEnvPoints[currVolEnvPoint + 1][0] - 1;
				}
				
				if (minX < 0) minX = 0;
				if (minX > 324) minX = 324;
				if (maxX < 0) maxX = 0;
				if (maxX > 324) maxX = 324;
				
				if (mx < minX) mx = minX;
				if (mx > maxX) mx = maxX;
				
				ins->volEnvPoints[currVolEnvPoint][0] = (int16_t)mx;
				inst->uiState.updateInstEditor = true;
			}
		}
		
		/* Handle Y movement - clamp to 0-64 */
		if (my != editor->lastMouseY)
		{
			editor->lastMouseY = my;
			
			my -= editor->saveMouseY;
			if (my < 0) my = 0;
			if (my > 64) my = 64;
			my = 64 - my;
			
			ins->volEnvPoints[currVolEnvPoint][1] = (int16_t)my;
			inst->uiState.updateInstEditor = true;
		}
		return;
	}

	/* Handle panning envelope point dragging - exact match to standalone */
	if (editor->draggingPanEnv)
	{
		if (inst->editor.curInstr == 0)
			return;
		
		ft2_instr_t *ins = getInstrForInst(inst);
		if (ins == NULL || ins->panEnvLength == 0)
			return;
		
		uint8_t ant = ins->panEnvLength;
		if (ant > 12)
			ant = 12;
		
		int32_t mx = x;
		int32_t my = y;
		
		/* Handle X movement - constrain between adjacent points */
		int8_t currPanEnvPoint = inst->editor.currPanEnvPoint;
		if (currPanEnvPoint < 0) currPanEnvPoint = 0;
		if (currPanEnvPoint >= 12) currPanEnvPoint = 11;

		if (mx != editor->lastMouseX)
		{
			editor->lastMouseX = mx;
			
			if (ant > 1 && currPanEnvPoint > 0)
			{
				mx -= editor->saveMouseX;
				if (mx < 0) mx = 0;
				if (mx > 324) mx = 324;
				
				int32_t minX, maxX;
				if (currPanEnvPoint == ant - 1)
				{
					minX = ins->panEnvPoints[currPanEnvPoint - 1][0] + 1;
					maxX = 324;
				}
				else
				{
					minX = ins->panEnvPoints[currPanEnvPoint - 1][0] + 1;
					maxX = ins->panEnvPoints[currPanEnvPoint + 1][0] - 1;
				}
				
				if (minX < 0) minX = 0;
				if (minX > 324) minX = 324;
				if (maxX < 0) maxX = 0;
				if (maxX > 324) maxX = 324;
				
				if (mx < minX) mx = minX;
				if (mx > maxX) mx = maxX;
				
				ins->panEnvPoints[currPanEnvPoint][0] = (int16_t)mx;
				inst->uiState.updateInstEditor = true;
			}
		}
		
		/* Handle Y movement - clamp to 0-63 */
		if (my != editor->lastMouseY)
		{
			editor->lastMouseY = my;
			
			my -= editor->saveMouseY;
			if (my < 0) my = 0;
			if (my > 63) my = 63;
			my = 63 - my;
			
			ins->panEnvPoints[currPanEnvPoint][1] = (int16_t)my;
			inst->uiState.updateInstEditor = true;
		}
		return;
	}
}

void ft2_instr_ed_mouse_up(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_instrument_editor_t *editor = FT2_INSTR_ED(inst);
	editor->draggingVolEnv = false;
	editor->draggingPanEnv = false;
	editor->draggingPiano = false;
}

/* ============ VISIBILITY FUNCTIONS ============ */

void showInstEditor(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;

	/* Hide other bottom screens (matching standalone) */
	if (inst->uiState.extendedPatternEditor)
	{
		exitPatternEditorExtended(inst);
	}
	if (inst->uiState.sampleEditorShown)
	{
		hideSampleEditor(inst);
	}
	if (inst->uiState.sampleEditorExtShown)
	{
		hideSampleEditorExt(inst);
	}
	
	/* Hide pattern editor (including channel scrollbar) */
	hidePatternEditor(inst);
	inst->uiState.instEditorShown = true;

	/* Show scrollbars */
	showScrollBar(widgets, video, SB_INST_VOL);
	showScrollBar(widgets, video, SB_INST_PAN);
	showScrollBar(widgets, video, SB_INST_FTUNE);
	showScrollBar(widgets, video, SB_INST_FADEOUT);
	showScrollBar(widgets, video, SB_INST_VIBSPEED);
	showScrollBar(widgets, video, SB_INST_VIBDEPTH);
	showScrollBar(widgets, video, SB_INST_VIBSWEEP);

	/* Show predef buttons (volume) */
	showPushButton(widgets, video, bmp, PB_INST_VDEF1);
	showPushButton(widgets, video, bmp, PB_INST_VDEF2);
	showPushButton(widgets, video, bmp, PB_INST_VDEF3);
	showPushButton(widgets, video, bmp, PB_INST_VDEF4);
	showPushButton(widgets, video, bmp, PB_INST_VDEF5);
	showPushButton(widgets, video, bmp, PB_INST_VDEF6);

	/* Show predef buttons (panning) */
	showPushButton(widgets, video, bmp, PB_INST_PDEF1);
	showPushButton(widgets, video, bmp, PB_INST_PDEF2);
	showPushButton(widgets, video, bmp, PB_INST_PDEF3);
	showPushButton(widgets, video, bmp, PB_INST_PDEF4);
	showPushButton(widgets, video, bmp, PB_INST_PDEF5);
	showPushButton(widgets, video, bmp, PB_INST_PDEF6);

	/* Show volume envelope buttons */
	showPushButton(widgets, video, bmp, PB_INST_VP_ADD);
	showPushButton(widgets, video, bmp, PB_INST_VP_DEL);
	showPushButton(widgets, video, bmp, PB_INST_VS_UP);
	showPushButton(widgets, video, bmp, PB_INST_VS_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VREPS_UP);
	showPushButton(widgets, video, bmp, PB_INST_VREPS_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VREPE_UP);
	showPushButton(widgets, video, bmp, PB_INST_VREPE_DOWN);

	/* Show panning envelope buttons */
	showPushButton(widgets, video, bmp, PB_INST_PP_ADD);
	showPushButton(widgets, video, bmp, PB_INST_PP_DEL);
	showPushButton(widgets, video, bmp, PB_INST_PS_UP);
	showPushButton(widgets, video, bmp, PB_INST_PS_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_PREPS_UP);
	showPushButton(widgets, video, bmp, PB_INST_PREPS_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_PREPE_UP);
	showPushButton(widgets, video, bmp, PB_INST_PREPE_DOWN);

	/* Show value adjust buttons */
	showPushButton(widgets, video, bmp, PB_INST_VOL_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VOL_UP);
	showPushButton(widgets, video, bmp, PB_INST_PAN_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_PAN_UP);
	showPushButton(widgets, video, bmp, PB_INST_FTUNE_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_FTUNE_UP);
	showPushButton(widgets, video, bmp, PB_INST_FADEOUT_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_FADEOUT_UP);
	showPushButton(widgets, video, bmp, PB_INST_VIBSPEED_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VIBSPEED_UP);
	showPushButton(widgets, video, bmp, PB_INST_VIBDEPTH_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VIBDEPTH_UP);
	showPushButton(widgets, video, bmp, PB_INST_VIBSWEEP_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_VIBSWEEP_UP);

	/* Show control buttons */
	showPushButton(widgets, video, bmp, PB_INST_EXIT);
	showPushButton(widgets, video, bmp, PB_INST_OCT_UP);
	showPushButton(widgets, video, bmp, PB_INST_HALFTONE_UP);
	showPushButton(widgets, video, bmp, PB_INST_OCT_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_HALFTONE_DOWN);

	/* Show envelope checkboxes */
	showCheckBox(widgets, video, bmp, CB_INST_VENV);
	showCheckBox(widgets, video, bmp, CB_INST_VENV_SUS);
	showCheckBox(widgets, video, bmp, CB_INST_VENV_LOOP);
	showCheckBox(widgets, video, bmp, CB_INST_PENV);
	showCheckBox(widgets, video, bmp, CB_INST_PENV_SUS);
	showCheckBox(widgets, video, bmp, CB_INST_PENV_LOOP);

	/* Show vibrato waveform radio buttons */
	showRadioButtonGroup(widgets, video, bmp, RB_GROUP_INST_WAVEFORM);

	inst->uiState.updateInstEditor = true;
}

void hideInstEditor(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->uiState.instEditorShown = false;

	/* Hide scrollbars */
	hideScrollBar(widgets, SB_INST_VOL);
	hideScrollBar(widgets, SB_INST_PAN);
	hideScrollBar(widgets, SB_INST_FTUNE);
	hideScrollBar(widgets, SB_INST_FADEOUT);
	hideScrollBar(widgets, SB_INST_VIBSPEED);
	hideScrollBar(widgets, SB_INST_VIBDEPTH);
	hideScrollBar(widgets, SB_INST_VIBSWEEP);

	/* Hide predef buttons (volume) */
	hidePushButton(widgets, PB_INST_VDEF1);
	hidePushButton(widgets, PB_INST_VDEF2);
	hidePushButton(widgets, PB_INST_VDEF3);
	hidePushButton(widgets, PB_INST_VDEF4);
	hidePushButton(widgets, PB_INST_VDEF5);
	hidePushButton(widgets, PB_INST_VDEF6);

	/* Hide predef buttons (panning) */
	hidePushButton(widgets, PB_INST_PDEF1);
	hidePushButton(widgets, PB_INST_PDEF2);
	hidePushButton(widgets, PB_INST_PDEF3);
	hidePushButton(widgets, PB_INST_PDEF4);
	hidePushButton(widgets, PB_INST_PDEF5);
	hidePushButton(widgets, PB_INST_PDEF6);

	/* Hide volume envelope buttons */
	hidePushButton(widgets, PB_INST_VP_ADD);
	hidePushButton(widgets, PB_INST_VP_DEL);
	hidePushButton(widgets, PB_INST_VS_UP);
	hidePushButton(widgets, PB_INST_VS_DOWN);
	hidePushButton(widgets, PB_INST_VREPS_UP);
	hidePushButton(widgets, PB_INST_VREPS_DOWN);
	hidePushButton(widgets, PB_INST_VREPE_UP);
	hidePushButton(widgets, PB_INST_VREPE_DOWN);

	/* Hide panning envelope buttons */
	hidePushButton(widgets, PB_INST_PP_ADD);
	hidePushButton(widgets, PB_INST_PP_DEL);
	hidePushButton(widgets, PB_INST_PS_UP);
	hidePushButton(widgets, PB_INST_PS_DOWN);
	hidePushButton(widgets, PB_INST_PREPS_UP);
	hidePushButton(widgets, PB_INST_PREPS_DOWN);
	hidePushButton(widgets, PB_INST_PREPE_UP);
	hidePushButton(widgets, PB_INST_PREPE_DOWN);

	/* Hide value adjust buttons */
	hidePushButton(widgets, PB_INST_VOL_DOWN);
	hidePushButton(widgets, PB_INST_VOL_UP);
	hidePushButton(widgets, PB_INST_PAN_DOWN);
	hidePushButton(widgets, PB_INST_PAN_UP);
	hidePushButton(widgets, PB_INST_FTUNE_DOWN);
	hidePushButton(widgets, PB_INST_FTUNE_UP);
	hidePushButton(widgets, PB_INST_FADEOUT_DOWN);
	hidePushButton(widgets, PB_INST_FADEOUT_UP);
	hidePushButton(widgets, PB_INST_VIBSPEED_DOWN);
	hidePushButton(widgets, PB_INST_VIBSPEED_UP);
	hidePushButton(widgets, PB_INST_VIBDEPTH_DOWN);
	hidePushButton(widgets, PB_INST_VIBDEPTH_UP);
	hidePushButton(widgets, PB_INST_VIBSWEEP_DOWN);
	hidePushButton(widgets, PB_INST_VIBSWEEP_UP);

	/* Hide control buttons */
	hidePushButton(widgets, PB_INST_EXIT);
	hidePushButton(widgets, PB_INST_OCT_UP);
	hidePushButton(widgets, PB_INST_HALFTONE_UP);
	hidePushButton(widgets, PB_INST_OCT_DOWN);
	hidePushButton(widgets, PB_INST_HALFTONE_DOWN);

	/* Hide envelope checkboxes */
	hideCheckBox(widgets, CB_INST_VENV);
	hideCheckBox(widgets, CB_INST_VENV_SUS);
	hideCheckBox(widgets, CB_INST_VENV_LOOP);
	hideCheckBox(widgets, CB_INST_PENV);
	hideCheckBox(widgets, CB_INST_PENV_SUS);
	hideCheckBox(widgets, CB_INST_PENV_LOOP);

	/* Hide vibrato waveform radio buttons */
	hideRadioButtonGroup(widgets, RB_GROUP_INST_WAVEFORM);
}

void toggleInstEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	/* Hide sample editor if shown (must call hideSampleEditor to hide buttons) */
	if (inst->uiState.sampleEditorShown)
	{
		hideSampleEditor(inst);
	}

	if (inst->uiState.instEditorShown)
	{
		exitInstEditor(inst);
	}
	else
	{
		inst->uiState.patternEditorShown = false;
		showInstEditor(inst);
	}
}

void exitInstEditor(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	hideInstEditor(inst);
	showPatternEditor(inst);
}

/* ============ EXTENDED INSTRUMENT EDITOR ============ */

void showInstEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.extendedPatternEditor)
		exitPatternEditorExtended(inst);

	/* Hide all other top-left panel overlays (S.E.Ext, Transpose, Adv.Edit, Trim) */
	hideAllTopLeftPanelOverlays(inst);

	inst->uiState.instEditorExtShown = true;
	inst->uiState.scopesShown = false;
}

void hideInstEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_widgets_t *widgets = &((ft2_ui_t *)inst->ui)->widgets;
	inst->uiState.instEditorExtShown = false;

	/* Hide all I.E.Ext widgets */
	hideCheckBox(widgets, CB_INST_EXT_MIDI);
	hideCheckBox(widgets, CB_INST_EXT_MUTE);
	hideScrollBar(widgets, SB_INST_EXT_MIDI_CH);
	hideScrollBar(widgets, SB_INST_EXT_MIDI_PRG);
	hideScrollBar(widgets, SB_INST_EXT_MIDI_BEND);
	hidePushButton(widgets, PB_INST_EXT_MIDI_CH_DOWN);
	hidePushButton(widgets, PB_INST_EXT_MIDI_CH_UP);
	hidePushButton(widgets, PB_INST_EXT_MIDI_PRG_DOWN);
	hidePushButton(widgets, PB_INST_EXT_MIDI_PRG_UP);
	hidePushButton(widgets, PB_INST_EXT_MIDI_BEND_DOWN);
	hidePushButton(widgets, PB_INST_EXT_MIDI_BEND_UP);

	/* Show scopes again and trigger framework redraw */
	inst->uiState.scopesShown = true;

	ft2_ui_t *ui = (ft2_ui_t*)inst->ui;
	if (ui != NULL)
		ui->scopes.needsFrameworkRedraw = true;
}

void toggleInstEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;

	if (inst->uiState.instEditorExtShown)
		hideInstEditorExt(inst);
	else
		showInstEditorExt(inst);
}

/* 2-digit decimal string lookup table for MIDI values */
static const char *instExtDec2StrTab[100] = {
	"00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15",
	"16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31",
	"32","33","34","35","36","37","38","39","40","41","42","43","44","45","46","47",
	"48","49","50","51","52","53","54","55","56","57","58","59","60","61","62","63",
	"64","65","66","67","68","69","70","71","72","73","74","75","76","77","78","79",
	"80","81","82","83","84","85","86","87","88","89","90","91","92","93","94","95",
	"96","97","98","99"
};

/* 3-digit decimal string lookup table for MIDI program */
static const char *instExtDec3StrTab[128] = {
	"000","001","002","003","004","005","006","007","008","009","010","011","012","013","014","015",
	"016","017","018","019","020","021","022","023","024","025","026","027","028","029","030","031",
	"032","033","034","035","036","037","038","039","040","041","042","043","044","045","046","047",
	"048","049","050","051","052","053","054","055","056","057","058","059","060","061","062","063",
	"064","065","066","067","068","069","070","071","072","073","074","075","076","077","078","079",
	"080","081","082","083","084","085","086","087","088","089","090","091","092","093","094","095",
	"096","097","098","099","100","101","102","103","104","105","106","107","108","109","110","111",
	"112","113","114","115","116","117","118","119","120","121","122","123","124","125","126","127"
};

static void drawMIDICh(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	uint8_t val = (ins != NULL) ? (ins->midiChannel + 1) : 1;
	if (val > 16) val = 16;
	textOutFixed(video, bmp, 156, 132, PAL_FORGRND, PAL_DESKTOP, instExtDec2StrTab[val]);
}

static void drawMIDIPrg(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	uint8_t val = (ins != NULL) ? ins->midiProgram : 0;
	if (val > 127) val = 127;
	textOutFixed(video, bmp, 149, 146, PAL_FORGRND, PAL_DESKTOP, instExtDec3StrTab[val]);
}

static void drawMIDIBend(ft2_instance_t *inst, ft2_video_t *video, const ft2_bmp_t *bmp)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	uint8_t val = (ins != NULL) ? ins->midiBend : 0;
	if (val > 36) val = 36;
	textOutFixed(video, bmp, 156, 160, PAL_FORGRND, PAL_DESKTOP, instExtDec2StrTab[val]);
}

void drawInstEditorExt(ft2_instance_t *inst)
{
	if (inst == NULL || inst->ui == NULL)
		return;

	ft2_video_t *video = FT2_VIDEO(inst);
	const ft2_bmp_t *bmp = FT2_BMP(inst);
	ft2_widgets_t *widgets = &FT2_UI(inst)->widgets;
	ft2_instr_t *ins = getInstrForInst(inst);

	/* Draw frameworks - matches standalone ft2_inst_ed.c:2638-2640 */
	drawFramework(video, 0,  92, 291, 17, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 109, 291, 19, FRAMEWORK_TYPE1);
	drawFramework(video, 0, 128, 291, 45, FRAMEWORK_TYPE1);

	/* Draw text labels */
	textOutShadow(video, bmp, 4,   96,  PAL_FORGRND, PAL_DSKTOP2, "Instrument Editor Extension:");
	textOutShadow(video, bmp, 20,  114, PAL_FORGRND, PAL_DSKTOP2, "Instrument MIDI enable");
	textOutShadow(video, bmp, 189, 114, PAL_FORGRND, PAL_DSKTOP2, "Mute computer");
	textOutShadow(video, bmp, 4,   132, PAL_FORGRND, PAL_DSKTOP2, "MIDI transmit channel");
	textOutShadow(video, bmp, 4,   146, PAL_FORGRND, PAL_DSKTOP2, "MIDI program");
	textOutShadow(video, bmp, 4,   160, PAL_FORGRND, PAL_DSKTOP2, "Bender range (halftones)");

	/* Set checkbox and scrollbar states */
	if (ins == NULL)
	{
		widgets->checkBoxChecked[CB_INST_EXT_MIDI] = false;
		widgets->checkBoxChecked[CB_INST_EXT_MUTE] = false;
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_CH, 0, false);
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_PRG, 0, false);
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_BEND, 0, false);
	}
	else
	{
		widgets->checkBoxChecked[CB_INST_EXT_MIDI] = ins->midiOn ? true : false;
		widgets->checkBoxChecked[CB_INST_EXT_MUTE] = ins->mute ? true : false;
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_CH, ins->midiChannel, false);
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_PRG, ins->midiProgram, false);
		setScrollBarPos(inst, widgets, video, SB_INST_EXT_MIDI_BEND, ins->midiBend, false);
	}

	/* Show checkboxes */
	showCheckBox(widgets, video, bmp, CB_INST_EXT_MIDI);
	showCheckBox(widgets, video, bmp, CB_INST_EXT_MUTE);

	/* Show scrollbars */
	showScrollBar(widgets, video, SB_INST_EXT_MIDI_CH);
	showScrollBar(widgets, video, SB_INST_EXT_MIDI_PRG);
	showScrollBar(widgets, video, SB_INST_EXT_MIDI_BEND);

	/* Show pushbuttons */
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_CH_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_CH_UP);
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_PRG_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_PRG_UP);
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_BEND_DOWN);
	showPushButton(widgets, video, bmp, PB_INST_EXT_MIDI_BEND_UP);

	/* Draw MIDI values */
	drawMIDICh(inst, video, bmp);
	drawMIDIPrg(inst, video, bmp);
	drawMIDIBend(inst, video, bmp);
}

/* ============ MIDI CONTROLS ============ */

static ft2_instr_t *getInstrForInst(ft2_instance_t *inst)
{
	if (inst == NULL)
		return NULL;
	
	uint8_t curInstr = inst->editor.curInstr;
	if (curInstr == 0 || curInstr > 128)
		return NULL;
	
	return inst->replayer.instr[curInstr];
}

/* Helper to get current sample */
static ft2_sample_t *getCurSmp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL || inst->editor.curSmp >= 16)
		return NULL;
	return &instr->smp[inst->editor.curSmp];
}

/* ============ INSTRUMENT EDITOR CALLBACKS ============ */

/* Envelope presets - left-click recalls, right-click stores */
void pbVolPreDef1(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 0); }
void pbVolPreDef2(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 1); }
void pbVolPreDef3(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 2); }
void pbVolPreDef4(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 3); }
void pbVolPreDef5(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 4); }
void pbVolPreDef6(ft2_instance_t *inst) { setOrStoreVolEnvPreset(inst, 5); }
void pbPanPreDef1(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 0); }
void pbPanPreDef2(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 1); }
void pbPanPreDef3(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 2); }
void pbPanPreDef4(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 3); }
void pbPanPreDef5(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 4); }
void pbPanPreDef6(ft2_instance_t *inst) { setOrStorePanEnvPreset(inst, 5); }
/* Volume envelope controls */
void pbVolEnvAdd(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	if (inst->editor.curInstr == 0 || ins == NULL)
		return;
	
	const int16_t ant = ins->volEnvLength;
	if (ant >= 12)
		return;
	
	int16_t i = (int16_t)inst->editor.currVolEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant - 1;
		if (i < 0)
			i = 0;
	}
	
	/* Check if there's enough space between adjacent points */
	if (i < ant - 1 && ins->volEnvPoints[i+1][0] - ins->volEnvPoints[i][0] < 2)
		return;
	
	if (ins->volEnvPoints[i][0] >= 323)
		return;
	
	/* Shift all points after i down by one */
	for (int16_t j = ant; j > i; j--)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j-1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j-1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->volEnvSustain > i)
		ins->volEnvSustain++;
	if (ins->volEnvLoopStart > i)
		ins->volEnvLoopStart++;
	if (ins->volEnvLoopEnd > i)
		ins->volEnvLoopEnd++;
	
	/* Calculate new point position */
	if (i < ant - 1)
	{
		ins->volEnvPoints[i+1][0] = (ins->volEnvPoints[i][0] + ins->volEnvPoints[i+2][0]) / 2;
		ins->volEnvPoints[i+1][1] = (ins->volEnvPoints[i][1] + ins->volEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->volEnvPoints[i+1][0] = ins->volEnvPoints[i][0] + 10;
		ins->volEnvPoints[i+1][1] = ins->volEnvPoints[i][1];
	}
	
	if (ins->volEnvPoints[i+1][0] > 324)
		ins->volEnvPoints[i+1][0] = 324;
	
	ins->volEnvLength++;
	inst->uiState.updateInstEditor = true;
}

void pbVolEnvDel(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0 || ins->volEnvLength <= 2)
		return;
	
	int16_t i = (int16_t)inst->editor.currVolEnvPoint;
	if (i < 0 || i >= ins->volEnvLength)
		return;
	
	/* Shift all points after i up by one */
	for (int16_t j = i; j < ins->volEnvLength - 1; j++)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j+1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j+1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->volEnvSustain > i)
		ins->volEnvSustain--;
	if (ins->volEnvLoopStart > i)
		ins->volEnvLoopStart--;
	if (ins->volEnvLoopEnd > i)
		ins->volEnvLoopEnd--;
	
	/* Ensure first point always at X=0 */
	ins->volEnvPoints[0][0] = 0;
	ins->volEnvLength--;
	
	/* Clamp indices to valid range */
	if (ins->volEnvSustain >= ins->volEnvLength)
		ins->volEnvSustain = ins->volEnvLength - 1;
	if (ins->volEnvLoopStart >= ins->volEnvLength)
		ins->volEnvLoopStart = ins->volEnvLength - 1;
	if (ins->volEnvLoopEnd >= ins->volEnvLength)
		ins->volEnvLoopEnd = ins->volEnvLength - 1;
	
	/* Update current point selection */
	if (ins->volEnvLength == 0)
		inst->editor.currVolEnvPoint = 0;
	else if (inst->editor.currVolEnvPoint >= ins->volEnvLength)
		inst->editor.currVolEnvPoint = ins->volEnvLength - 1;
	
	inst->uiState.updateInstEditor = true;
}

void pbVolEnvSusUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvSustain < instr->volEnvLength - 1)
	{
		instr->volEnvSustain++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvSusDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvSustain > 0)
	{
		instr->volEnvSustain--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepSUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopStart < instr->volEnvLoopEnd)
	{
		instr->volEnvLoopStart++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepSDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopStart > 0)
	{
		instr->volEnvLoopStart--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepEUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopEnd < instr->volEnvLength - 1)
	{
		instr->volEnvLoopEnd++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbVolEnvRepEDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->volEnvLoopEnd > instr->volEnvLoopStart)
	{
		instr->volEnvLoopEnd--;
		inst->uiState.updateInstEditor = true;
	}
}

/* Pan envelope controls */
void pbPanEnvAdd(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0)
		return;
	
	const int16_t ant = ins->panEnvLength;
	if (ant >= 12)
		return;
	
	int16_t i = (int16_t)inst->editor.currPanEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant - 1;
		if (i < 0)
			i = 0;
	}
	
	/* Check if there's enough space between adjacent points */
	if (i < ant - 1 && ins->panEnvPoints[i+1][0] - ins->panEnvPoints[i][0] < 2)
		return;
	
	if (ins->panEnvPoints[i][0] >= 323)
		return;
	
	/* Shift all points after i down by one */
	for (int16_t j = ant; j > i; j--)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j-1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j-1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->panEnvSustain > i)
		ins->panEnvSustain++;
	if (ins->panEnvLoopStart > i)
		ins->panEnvLoopStart++;
	if (ins->panEnvLoopEnd > i)
		ins->panEnvLoopEnd++;
	
	/* Calculate new point position */
	if (i < ant - 1)
	{
		ins->panEnvPoints[i+1][0] = (ins->panEnvPoints[i][0] + ins->panEnvPoints[i+2][0]) / 2;
		ins->panEnvPoints[i+1][1] = (ins->panEnvPoints[i][1] + ins->panEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->panEnvPoints[i+1][0] = ins->panEnvPoints[i][0] + 10;
		ins->panEnvPoints[i+1][1] = ins->panEnvPoints[i][1];
	}
	
	if (ins->panEnvPoints[i+1][0] > 324)
		ins->panEnvPoints[i+1][0] = 324;
	
	ins->panEnvLength++;
	inst->uiState.updateInstEditor = true;
}

void pbPanEnvDel(ft2_instance_t *inst)
{
	ft2_instr_t *ins = getInstrForInst(inst);
	if (ins == NULL || inst->editor.curInstr == 0 || ins->panEnvLength <= 2)
		return;
	
	int16_t i = (int16_t)inst->editor.currPanEnvPoint;
	if (i < 0 || i >= ins->panEnvLength)
		return;
	
	/* Shift all points after i up by one */
	for (int16_t j = i; j < ins->panEnvLength - 1; j++)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j+1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j+1][1];
	}
	
	/* Update sustain/loop indices */
	if (ins->panEnvSustain > i)
		ins->panEnvSustain--;
	if (ins->panEnvLoopStart > i)
		ins->panEnvLoopStart--;
	if (ins->panEnvLoopEnd > i)
		ins->panEnvLoopEnd--;
	
	/* Ensure first point always at X=0 */
	ins->panEnvPoints[0][0] = 0;
	ins->panEnvLength--;
	
	/* Clamp indices to valid range */
	if (ins->panEnvSustain >= ins->panEnvLength)
		ins->panEnvSustain = ins->panEnvLength - 1;
	if (ins->panEnvLoopStart >= ins->panEnvLength)
		ins->panEnvLoopStart = ins->panEnvLength - 1;
	if (ins->panEnvLoopEnd >= ins->panEnvLength)
		ins->panEnvLoopEnd = ins->panEnvLength - 1;
	
	/* Update current point selection */
	if (ins->panEnvLength == 0)
		inst->editor.currPanEnvPoint = 0;
	else if (inst->editor.currPanEnvPoint >= ins->panEnvLength)
		inst->editor.currPanEnvPoint = ins->panEnvLength - 1;
	
	inst->uiState.updateInstEditor = true;
}

void pbPanEnvSusUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvSustain < instr->panEnvLength - 1)
	{
		instr->panEnvSustain++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvSusDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvSustain > 0)
	{
		instr->panEnvSustain--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepSUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopStart < instr->panEnvLoopEnd)
	{
		instr->panEnvLoopStart++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepSDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopStart > 0)
	{
		instr->panEnvLoopStart--;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepEUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopEnd < instr->panEnvLength - 1)
	{
		instr->panEnvLoopEnd++;
		inst->uiState.updateInstEditor = true;
	}
}

void pbPanEnvRepEDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL)
		return;
	
	if (instr->panEnvLoopEnd > instr->panEnvLoopStart)
	{
		instr->panEnvLoopEnd--;
		inst->uiState.updateInstEditor = true;
	}
}

/* Sample parameter buttons - these adjust scrollbar positions */
void pbInstVolDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->volume > 0)
	{
		smp->volume--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVolUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->volume < 64)
	{
		smp->volume++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstPanDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->panning > 0)
	{
		smp->panning--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstPanUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->panning < 255)
	{
		smp->panning++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFtuneDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->finetune > -128)
	{
		smp->finetune--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFtuneUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->finetune < 127)
	{
		smp->finetune++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFadeoutDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->fadeout > 0)
	{
		instr->fadeout--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstFadeoutUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->fadeout < 0xFFF)
	{
		instr->fadeout++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSpeedDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibRate > 0)
	{
		instr->autoVibRate--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSpeedUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibRate < 0x3F)
	{
		instr->autoVibRate++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibDepthDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibDepth > 0)
	{
		instr->autoVibDepth--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibDepthUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibDepth < 0x0F)
	{
		instr->autoVibDepth++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSweepDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibSweep > 0)
	{
		instr->autoVibSweep--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstVibSweepUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->autoVibSweep < 0xFF)
	{
		instr->autoVibSweep++;
		ft2_song_mark_modified(inst);
	}
}

/* Relative note */
void pbInstOctUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->relativeNote <= 71 - 12)
		smp->relativeNote += 12;
	else
		smp->relativeNote = 71;
	ft2_song_mark_modified(inst);
}

void pbInstOctDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->relativeNote >= -48 + 12)
		smp->relativeNote -= 12;
	else
		smp->relativeNote = -48;
	ft2_song_mark_modified(inst);
}

void pbInstHalftoneUp(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->relativeNote < 71)
	{
		smp->relativeNote++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstHalftoneDown(ft2_instance_t *inst)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	if (smp->relativeNote > -48)
	{
		smp->relativeNote--;
		ft2_song_mark_modified(inst);
	}
}

/* Exit */
void pbInstExit(ft2_instance_t *inst)
{
	if (inst == NULL) return;
	exitInstEditor(inst);
}

/* ========== INSTRUMENT EDITOR EXTENSION CALLBACKS ========== */

void pbInstExtMidiChDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiChannel > 0)
	{
		instr->midiChannel--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiChUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiChannel < 15)
	{
		instr->midiChannel++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiPrgDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiProgram > 0)
	{
		instr->midiProgram--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiPrgUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiProgram < 127)
	{
		instr->midiProgram++;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiBendDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiBend > 0)
	{
		instr->midiBend--;
		ft2_song_mark_modified(inst);
	}
}

void pbInstExtMidiBendUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	if (instr->midiBend < 36)
	{
		instr->midiBend++;
		ft2_song_mark_modified(inst);
	}
}


/* Instrument scrollbar callbacks */
void sbInstVol(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	smp->volume = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstPan(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	smp->panning = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstFtune(ft2_instance_t *inst, uint32_t pos)
{
	ft2_sample_t *smp = getCurSmp(inst);
	if (smp == NULL) return;
	smp->finetune = (int8_t)(pos - 128);
	ft2_song_mark_modified(inst);
}

void sbInstFadeout(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->fadeout = (uint16_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibSpeed(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibRate = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibDepth(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibDepth = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstVibSweep(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibSweep = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiCh(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->midiChannel = (uint8_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiPrg(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->midiProgram = (int16_t)pos;
	ft2_song_mark_modified(inst);
}

void sbInstExtMidiBend(ft2_instance_t *inst, uint32_t pos)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->midiBend = (int16_t)pos;
	ft2_song_mark_modified(inst);
}

/* Instrument checkbox callbacks */
void cbInstVEnv(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_ENABLED;
	ft2_song_mark_modified(inst);
}

void cbInstVEnvSus(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_SUSTAIN;
	ft2_song_mark_modified(inst);
}

void cbInstVEnvLoop(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->volEnvFlags ^= FT2_ENV_LOOP;
	ft2_song_mark_modified(inst);
}

void cbInstPEnv(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_ENABLED;
	ft2_song_mark_modified(inst);
}

void cbInstPEnvSus(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_SUSTAIN;
	ft2_song_mark_modified(inst);
}

void cbInstPEnvLoop(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->panEnvFlags ^= FT2_ENV_LOOP;
	ft2_song_mark_modified(inst);
}

void cbInstExtMidi(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->midiOn ^= 1;
	ft2_song_mark_modified(inst);
}

void cbInstExtMute(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->mute ^= 1;
	ft2_song_mark_modified(inst);
}

/* Instrument radio button callbacks */
void rbInstWaveSine(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibType = 0;
	ft2_song_mark_modified(inst);
}

void rbInstWaveSquare(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibType = 1;
	ft2_song_mark_modified(inst);
}

void rbInstWaveRampDown(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibType = 2;
	ft2_song_mark_modified(inst);
}

void rbInstWaveRampUp(ft2_instance_t *inst)
{
	ft2_instr_t *instr = getInstrForInst(inst);
	if (instr == NULL) return;
	instr->autoVibType = 3;
	ft2_song_mark_modified(inst);
}
