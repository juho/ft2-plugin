/**
 * @file ft2_plugin_volume_panel.c
 * @brief Volume adjustment modal panel implementation
 */

#include <string.h>
#include "ft2_plugin_volume_panel.h"
#include "ft2_plugin_modal_panels.h"
#include "ft2_plugin_video.h"
#include "ft2_plugin_bmp.h"
#include "ft2_plugin_pushbuttons.h"
#include "ft2_plugin_scrollbars.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_replayer.h"
#include "../ft2_instance.h"

/* Panel state */
typedef struct {
	bool active;
	ft2_instance_t *instance;
	double startVol;  /* -200 to +200 */
	double endVol;    /* -200 to +200 */
} volume_panel_state_t;

static volume_panel_state_t state = {
	.active = false,
	.instance = NULL,
	.startVol = 100.0,
	.endVol = 100.0
};

/* Forward declarations for callbacks */
static void onApplyClick(ft2_instance_t *inst);
static void onGetMaxScaleClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onStartVolScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onEndVolScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onStartVolDown(ft2_instance_t *inst);
static void onStartVolUp(ft2_instance_t *inst);
static void onEndVolDown(ft2_instance_t *inst);
static void onEndVolUp(ft2_instance_t *inst);

/* Helper to get current sample */
static ft2_sample_t *getCurrentSample(void)
{
	if (state.instance == NULL)
		return NULL;
	
	ft2_instance_t *inst = state.instance;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128)
		return NULL;
	
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	if (instr == NULL)
		return NULL;
	
	return &instr->smp[inst->editor.curSmp];
}

/* Set up widgets when panel opens */
static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (inst == NULL)
		return;
	
	pushButton_t *p;
	scrollBar_t *s;
	
	/* "Apply" pushbutton */
	p = &pushButtons[PB_RES_1];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Apply";
	p->x = 171;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = onApplyClick;
	p->visible = true;
	
	/* "Get maximum scale" pushbutton */
	p = &pushButtons[PB_RES_2];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Get maximum scale";
	p->x = 245;
	p->y = 262;
	p->w = 143;
	p->h = 16;
	p->callbackFuncOnUp = onGetMaxScaleClick;
	p->visible = true;
	
	/* "Exit" pushbutton */
	p = &pushButtons[PB_RES_3];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit";
	p->x = 389;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = onExitClick;
	p->visible = true;
	
	/* Start volume left arrow */
	p = &pushButtons[PB_RES_4];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onStartVolDown;
	p->visible = true;
	
	/* Start volume right arrow */
	p = &pushButtons[PB_RES_5];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onStartVolUp;
	p->visible = true;
	
	/* End volume left arrow */
	p = &pushButtons[PB_RES_6];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onEndVolDown;
	p->visible = true;
	
	/* End volume right arrow */
	p = &pushButtons[PB_RES_7];
	memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = onEndVolUp;
	p->visible = true;
	
	/* Start volume scrollbar */
	s = &scrollBars[SB_RES_1];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 315;
	s->y = 234;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = onStartVolScrollbar;
	s->visible = true;
	setScrollBarPageLength(inst, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, NULL, SB_RES_1, 400);
	setScrollBarPos(inst, NULL, SB_RES_1, (uint32_t)(state.startVol + 200), false);
	
	/* End volume scrollbar */
	s = &scrollBars[SB_RES_2];
	memset(s, 0, sizeof(scrollBar_t));
	s->x = 315;
	s->y = 248;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = onEndVolScrollbar;
	s->visible = true;
	setScrollBarPageLength(inst, NULL, SB_RES_2, 1);
	setScrollBarEnd(inst, NULL, SB_RES_2, 400);
	setScrollBarPos(inst, NULL, SB_RES_2, (uint32_t)(state.endVol + 200), false);
}

/* Hide widgets when panel closes */
static void hideWidgets(void)
{
	for (int i = 0; i < 8; i++)
		hidePushButton(PB_RES_1 + i);
	
	for (int i = 0; i < 3; i++)
		hideScrollBar(SB_RES_1 + i);
}

/* Widget callbacks */
static void onApplyClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_volume_panel_apply();
}

static void onGetMaxScaleClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_volume_panel_find_max_scale();
}

static void onExitClick(ft2_instance_t *inst)
{
	(void)inst;
	ft2_volume_panel_hide();
}

static void onStartVolScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	(void)inst;
	state.startVol = (double)((int32_t)pos - 200);
}

static void onEndVolScrollbar(ft2_instance_t *inst, uint32_t pos)
{
	(void)inst;
	state.endVol = (double)((int32_t)pos - 200);
}

static void onStartVolDown(ft2_instance_t *inst)
{
	(void)inst;
	if (state.startVol > -200.0)
		state.startVol -= 1.0;
}

static void onStartVolUp(ft2_instance_t *inst)
{
	(void)inst;
	if (state.startVol < 200.0)
		state.startVol += 1.0;
}

static void onEndVolDown(ft2_instance_t *inst)
{
	(void)inst;
	if (state.endVol > -200.0)
		state.endVol -= 1.0;
}

static void onEndVolUp(ft2_instance_t *inst)
{
	(void)inst;
	if (state.endVol < 200.0)
		state.endVol += 1.0;
}

/* Draw the panel frame and labels */
static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 166;
	const int16_t y = 230;
	const int16_t w = 301;
	const int16_t h = 52;
	
	/* Main fill */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	
	/* Outer border */
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	
	/* Inner border */
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);
	
	/* Labels */
	textOutShadow(video, bmp, 172, 236, PAL_FORGRND, PAL_BUTTON2, "Start volume");
	textOutShadow(video, bmp, 172, 250, PAL_FORGRND, PAL_BUTTON2, "End volume");
	charOutShadow(video, bmp, 282, 236, PAL_FORGRND, PAL_BUTTON2, '%');
	charOutShadow(video, bmp, 282, 250, PAL_FORGRND, PAL_BUTTON2, '%');
	
	/* Draw volume values */
	const int32_t startVol = (int32_t)state.startVol;
	const int32_t endVol = (int32_t)state.endVol;
	
	/* Start volume value */
	char sign;
	uint32_t val;
	
	if (startVol > 200)
	{
		charOut(video, bmp, 253, 236, PAL_FORGRND, '>');
		charOut(video, bmp, 260, 236, PAL_FORGRND, '2');
		charOut(video, bmp, 267, 236, PAL_FORGRND, '0');
		charOut(video, bmp, 274, 236, PAL_FORGRND, '0');
	}
	else
	{
		if (startVol == 0) sign = ' ';
		else if (startVol < 0) sign = '-';
		else sign = '+';
		
		val = (startVol < 0) ? -startVol : startVol;
		if (val > 99)
		{
			charOut(video, bmp, 253, 236, PAL_FORGRND, sign);
			charOut(video, bmp, 260, 236, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(video, bmp, 267, 236, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(video, bmp, 274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(video, bmp, 260, 236, PAL_FORGRND, sign);
			charOut(video, bmp, 267, 236, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(video, bmp, 274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(video, bmp, 267, 236, PAL_FORGRND, sign);
			charOut(video, bmp, 274, 236, PAL_FORGRND, '0' + (val % 10));
		}
	}
	
	/* End volume value */
	if (endVol > 200)
	{
		charOut(video, bmp, 253, 250, PAL_FORGRND, '>');
		charOut(video, bmp, 260, 250, PAL_FORGRND, '2');
		charOut(video, bmp, 267, 250, PAL_FORGRND, '0');
		charOut(video, bmp, 274, 250, PAL_FORGRND, '0');
	}
	else
	{
		if (endVol == 0) sign = ' ';
		else if (endVol < 0) sign = '-';
		else sign = '+';
		
		val = (endVol < 0) ? -endVol : endVol;
		if (val > 99)
		{
			charOut(video, bmp, 253, 250, PAL_FORGRND, sign);
			charOut(video, bmp, 260, 250, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(video, bmp, 267, 250, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(video, bmp, 274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(video, bmp, 260, 250, PAL_FORGRND, sign);
			charOut(video, bmp, 267, 250, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(video, bmp, 274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(video, bmp, 267, 250, PAL_FORGRND, sign);
			charOut(video, bmp, 274, 250, PAL_FORGRND, '0' + (val % 10));
		}
	}
}

/* Apply volume to sample */
static void applyVolumeToSample(void)
{
	if (state.instance == NULL)
		return;
	
	ft2_instance_t *inst = state.instance;
	ft2_sample_t *s = getCurrentSample();
	if (s == NULL || s->dataPtr == NULL || s->length == 0)
		return;
	
	/* Get range from sample editor */
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	int32_t x1, x2;
	
	if (ed != NULL && ed->hasRange && ed->rangeStart < ed->rangeEnd)
	{
		x1 = ed->rangeStart;
		x2 = ed->rangeEnd;
		if (x2 > s->length) x2 = s->length;
		if (x1 < 0) x1 = 0;
		if (x2 <= x1) return;
	}
	else
	{
		x1 = 0;
		x2 = s->length;
	}
	
	const int32_t len = x2 - x1;
	if (len <= 0)
		return;
	
	if (state.startVol == 100.0 && state.endVol == 100.0)
		return;
	
	/* Stop voices playing this sample before modifying */
	ft2_stop_sample_voices(inst, s);
	
	ft2_unfix_sample(s);
	
	const bool mustInterpolate = (state.startVol != state.endVol);
	const double dVolDelta = ((state.endVol - state.startVol) / 100.0) / len;
	double dVol = state.startVol / 100.0;
	
	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	
	if (sample16Bit)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				if (smp32 < -32768) smp32 = -32768;
				if (smp32 > 32767) smp32 = 32767;
				ptr16[i] = (int16_t)smp32;
				dVol += dVolDelta;
			}
		}
		else
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr16[i] * dVol);
				if (smp32 < -32768) smp32 = -32768;
				if (smp32 > 32767) smp32 = 32767;
				ptr16[i] = (int16_t)smp32;
			}
		}
	}
	else
	{
		int8_t *ptr8 = s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				if (smp32 < -128) smp32 = -128;
				if (smp32 > 127) smp32 = 127;
				ptr8[i] = (int8_t)smp32;
				dVol += dVolDelta;
			}
		}
		else
		{
			for (int32_t i = 0; i < len; i++)
			{
				int32_t smp32 = (int32_t)((int32_t)ptr8[i] * dVol);
				if (smp32 < -128) smp32 = -128;
				if (smp32 > 127) smp32 = 127;
				ptr8[i] = (int8_t)smp32;
			}
		}
	}
	
	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Calculate maximum scale value */
static double calculateMaxScale(void)
{
	ft2_sample_t *s = getCurrentSample();
	if (s == NULL || s->dataPtr == NULL || s->length == 0)
		return 100.0;
	
	ft2_sample_editor_t *ed = ft2_sample_ed_get_current();
	int32_t x1, x2;
	
	if (ed != NULL && ed->hasRange && ed->rangeStart < ed->rangeEnd)
	{
		x1 = ed->rangeStart;
		x2 = ed->rangeEnd;
		if (x2 > s->length) x2 = s->length;
		if (x1 < 0) x1 = 0;
		if (x2 <= x1)
			return 100.0;
	}
	else
	{
		x1 = 0;
		x2 = s->length;
	}
	
	uint32_t len = x2 - x1;
	if (len == 0)
		return 100.0;
	
	double dVolChange = 100.0;
	bool sample16Bit = (s->flags & FT2_SAMPLE_16BIT) != 0;
	
	ft2_unfix_sample(s);
	
	int32_t maxAmp = 0;
	if (sample16Bit)
	{
		const int16_t *ptr16 = (const int16_t *)s->dataPtr + x1;
		for (uint32_t i = 0; i < len; i++)
		{
			int32_t absSmp = ptr16[i];
			if (absSmp < 0) absSmp = -absSmp;
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}
		
		if (maxAmp > 0)
			dVolChange = (32767.0 / maxAmp) * 100.0;
	}
	else
	{
		const int8_t *ptr8 = (const int8_t *)&s->dataPtr[x1];
		for (uint32_t i = 0; i < len; i++)
		{
			int32_t absSmp = ptr8[i];
			if (absSmp < 0) absSmp = -absSmp;
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}
		
		if (maxAmp > 0)
			dVolChange = (127.0 / maxAmp) * 100.0;
	}
	
	ft2_fix_sample(s);
	
	if (dVolChange < 100.0)
		dVolChange = 100.0;
	
	return dVolChange;
}

/* Public API */

void ft2_volume_panel_show(ft2_instance_t *inst)
{
	if (inst == NULL)
		return;
	
	ft2_sample_t *s = NULL;
	if (inst->editor.curInstr != 0 && inst->editor.curInstr <= 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
		if (instr != NULL)
			s = &instr->smp[inst->editor.curSmp];
	}
	
	if (s == NULL || s->dataPtr == NULL || s->length == 0)
		return;
	
	state.active = true;
	state.instance = inst;
	state.startVol = 100.0;
	state.endVol = 100.0;
	
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_VOLUME);
}

void ft2_volume_panel_hide(void)
{
	if (!state.active)
		return;
	
	hideWidgets();
	state.active = false;
	
	if (state.instance != NULL)
		state.instance->uiState.updateSampleEditor = true;
	
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_VOLUME);
}

bool ft2_volume_panel_is_active(void)
{
	return state.active;
}

void ft2_volume_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || video == NULL)
		return;
	
	drawFrame(video, bmp);
	
	/* Update scrollbar positions */
	if (state.instance != NULL)
	{
		setScrollBarPos(state.instance, video, SB_RES_1, (uint32_t)(state.startVol + 200), false);
		setScrollBarPos(state.instance, video, SB_RES_2, (uint32_t)(state.endVol + 200), false);
	}
	
	/* Draw widgets */
	for (int i = 0; i < 8; i++)
	{
		if (pushButtons[PB_RES_1 + i].visible)
			drawPushButton(video, bmp, PB_RES_1 + i);
	}
	
	for (int i = 0; i < 2; i++)
	{
		if (scrollBars[SB_RES_1 + i].visible)
			drawScrollBar(video, SB_RES_1 + i);
	}
}

void ft2_volume_panel_apply(void)
{
	if (!state.active)
		return;
	
	applyVolumeToSample();
	ft2_volume_panel_hide();
}

void ft2_volume_panel_find_max_scale(void)
{
	if (!state.active)
		return;
	
	double maxScale = calculateMaxScale();
	state.startVol = maxScale;
	state.endVol = maxScale;
}

double ft2_volume_panel_get_start_vol(void)
{
	return state.startVol;
}

void ft2_volume_panel_set_start_vol(double vol)
{
	state.startVol = vol;
}

double ft2_volume_panel_get_end_vol(void)
{
	return state.endVol;
}

void ft2_volume_panel_set_end_vol(double vol)
{
	state.endVol = vol;
}

