/*
** FT2 Plugin - Volume Adjustment Modal Panel
** Applies linear volume ramp (fade in/out) to sample data.
** Range -200% to +200%, supports selection or full sample.
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
#include "ft2_plugin_ui.h"
#include "../ft2_instance.h"

typedef struct {
	bool active;
	ft2_instance_t *instance;
	double startVol, endVol; /* -200 to +200 percent */
} volume_panel_state_t;

static volume_panel_state_t state = { .active = false, .instance = NULL, .startVol = 100.0, .endVol = 100.0 };

static void onApplyClick(ft2_instance_t *inst);
static void onGetMaxScaleClick(ft2_instance_t *inst);
static void onExitClick(ft2_instance_t *inst);
static void onStartVolScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onEndVolScrollbar(ft2_instance_t *inst, uint32_t pos);
static void onStartVolDown(ft2_instance_t *inst);
static void onStartVolUp(ft2_instance_t *inst);
static void onEndVolDown(ft2_instance_t *inst);
static void onEndVolUp(ft2_instance_t *inst);

/* ------------------------------------------------------------------------- */
/*                             HELPERS                                       */
/* ------------------------------------------------------------------------- */

static ft2_sample_t *getCurrentSample(void)
{
	if (!state.instance) return NULL;
	ft2_instance_t *inst = state.instance;
	if (inst->editor.curInstr == 0 || inst->editor.curInstr > 128) return NULL;
	ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
	return instr ? &instr->smp[inst->editor.curSmp] : NULL;
}

/* ------------------------------------------------------------------------- */
/*                          WIDGET SETUP                                     */
/* ------------------------------------------------------------------------- */

static void setupWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	if (!inst) return;
	ft2_widgets_t *widgets = inst->ui ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;

	pushButton_t *p;
	scrollBar_t *s;

	/* Apply button */
	p = &widgets->pushButtons[PB_RES_1]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Apply"; p->x = 171; p->y = 262; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onApplyClick; widgets->pushButtonVisible[PB_RES_1] = true;

	/* Get maximum scale button */
	p = &widgets->pushButtons[PB_RES_2]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Get maximum scale"; p->x = 245; p->y = 262; p->w = 143; p->h = 16;
	p->callbackFuncOnUp = onGetMaxScaleClick; widgets->pushButtonVisible[PB_RES_2] = true;

	/* Exit button */
	p = &widgets->pushButtons[PB_RES_3]; memset(p, 0, sizeof(pushButton_t));
	p->caption = "Exit"; p->x = 389; p->y = 262; p->w = 73; p->h = 16;
	p->callbackFuncOnUp = onExitClick; widgets->pushButtonVisible[PB_RES_3] = true;

	/* Start volume arrows */
	p = &widgets->pushButtons[PB_RES_4]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 292; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onStartVolDown;
	widgets->pushButtonVisible[PB_RES_4] = true;

	p = &widgets->pushButtons[PB_RES_5]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 439; p->y = 234; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onStartVolUp;
	widgets->pushButtonVisible[PB_RES_5] = true;

	/* End volume arrows */
	p = &widgets->pushButtons[PB_RES_6]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_LEFT_STRING; p->x = 292; p->y = 248; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEndVolDown;
	widgets->pushButtonVisible[PB_RES_6] = true;

	p = &widgets->pushButtons[PB_RES_7]; memset(p, 0, sizeof(pushButton_t));
	p->caption = ARROW_RIGHT_STRING; p->x = 439; p->y = 248; p->w = 23; p->h = 13;
	p->preDelay = 1; p->delayFrames = 3; p->callbackFuncOnDown = onEndVolUp;
	widgets->pushButtonVisible[PB_RES_7] = true;

	/* Start volume scrollbar (range 0-400 maps to -200% to +200%) */
	s = &widgets->scrollBars[SB_RES_1]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 315; s->y = 234; s->w = 124; s->h = 13; s->callbackFunc = onStartVolScrollbar;
	widgets->scrollBarState[SB_RES_1].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_1, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_1, 400);
	setScrollBarPos(inst, widgets, NULL, SB_RES_1, (uint32_t)(state.startVol + 200), false);

	/* End volume scrollbar */
	s = &widgets->scrollBars[SB_RES_2]; memset(s, 0, sizeof(scrollBar_t));
	s->x = 315; s->y = 248; s->w = 124; s->h = 13; s->callbackFunc = onEndVolScrollbar;
	widgets->scrollBarState[SB_RES_2].visible = true;
	setScrollBarPageLength(inst, widgets, NULL, SB_RES_2, 1);
	setScrollBarEnd(inst, widgets, NULL, SB_RES_2, 400);
	setScrollBarPos(inst, widgets, NULL, SB_RES_2, (uint32_t)(state.endVol + 200), false);
}

static void hideWidgets(void)
{
	ft2_instance_t *inst = state.instance;
	ft2_widgets_t *widgets = (inst && inst->ui) ? &((ft2_ui_t *)inst->ui)->widgets : NULL;
	if (!widgets) return;
	for (int i = 0; i < 8; i++) hidePushButton(widgets, PB_RES_1 + i);
	for (int i = 0; i < 3; i++) hideScrollBar(widgets, SB_RES_1 + i);
}

/* ------------------------------------------------------------------------- */
/*                          CALLBACKS                                        */
/* ------------------------------------------------------------------------- */

static void onApplyClick(ft2_instance_t *inst) { (void)inst; ft2_volume_panel_apply(); }
static void onGetMaxScaleClick(ft2_instance_t *inst) { (void)inst; ft2_volume_panel_find_max_scale(); }
static void onExitClick(ft2_instance_t *inst) { (void)inst; ft2_volume_panel_hide(); }
static void onStartVolScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.startVol = (double)((int32_t)pos - 200); }
static void onEndVolScrollbar(ft2_instance_t *inst, uint32_t pos) { (void)inst; state.endVol = (double)((int32_t)pos - 200); }
static void onStartVolDown(ft2_instance_t *inst) { (void)inst; if (state.startVol > -200.0) state.startVol -= 1.0; }
static void onStartVolUp(ft2_instance_t *inst) { (void)inst; if (state.startVol < 200.0) state.startVol += 1.0; }
static void onEndVolDown(ft2_instance_t *inst) { (void)inst; if (state.endVol > -200.0) state.endVol -= 1.0; }
static void onEndVolUp(ft2_instance_t *inst) { (void)inst; if (state.endVol < 200.0) state.endVol += 1.0; }

/* ------------------------------------------------------------------------- */
/*                           DRAWING                                         */
/* ------------------------------------------------------------------------- */

/* Helper: draw signed volume value with variable-width formatting */
static void drawVolValue(ft2_video_t *video, const ft2_bmp_t *bmp, int16_t x, int16_t y, int32_t vol)
{
	if (vol > 200)
	{
		charOut(video, bmp, x, y, PAL_FORGRND, '>');
		charOut(video, bmp, x+7, y, PAL_FORGRND, '2');
		charOut(video, bmp, x+14, y, PAL_FORGRND, '0');
		charOut(video, bmp, x+21, y, PAL_FORGRND, '0');
		return;
	}

	char sign = (vol == 0) ? ' ' : (vol < 0) ? '-' : '+';
	uint32_t val = (vol < 0) ? -vol : vol;

	if (val > 99) {
		charOut(video, bmp, x, y, PAL_FORGRND, sign);
		charOut(video, bmp, x+7, y, PAL_FORGRND, '0' + (char)(val / 100));
		charOut(video, bmp, x+14, y, PAL_FORGRND, '0' + ((val / 10) % 10));
		charOut(video, bmp, x+21, y, PAL_FORGRND, '0' + (val % 10));
	} else if (val > 9) {
		charOut(video, bmp, x+7, y, PAL_FORGRND, sign);
		charOut(video, bmp, x+14, y, PAL_FORGRND, '0' + (char)(val / 10));
		charOut(video, bmp, x+21, y, PAL_FORGRND, '0' + (val % 10));
	} else {
		charOut(video, bmp, x+14, y, PAL_FORGRND, sign);
		charOut(video, bmp, x+21, y, PAL_FORGRND, '0' + (val % 10));
	}
}

static void drawFrame(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	const int16_t x = 166, y = 230, w = 301, h = 52;

	/* Background and borders */
	fillRect(video, x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
	vLine(video, x, y, h - 1, PAL_BUTTON1);
	hLine(video, x + 1, y, w - 2, PAL_BUTTON1);
	vLine(video, x + w - 1, y, h, PAL_BUTTON2);
	hLine(video, x, y + h - 1, w - 1, PAL_BUTTON2);
	vLine(video, x + 2, y + 2, h - 5, PAL_BUTTON2);
	hLine(video, x + 3, y + 2, w - 6, PAL_BUTTON2);
	vLine(video, x + w - 3, y + 2, h - 4, PAL_BUTTON1);
	hLine(video, x + 2, y + h - 3, w - 4, PAL_BUTTON1);

	/* Labels */
	textOutShadow(video, bmp, 172, 236, PAL_FORGRND, PAL_BUTTON2, "Start volume");
	textOutShadow(video, bmp, 172, 250, PAL_FORGRND, PAL_BUTTON2, "End volume");
	charOutShadow(video, bmp, 282, 236, PAL_FORGRND, PAL_BUTTON2, '%');
	charOutShadow(video, bmp, 282, 250, PAL_FORGRND, PAL_BUTTON2, '%');

	/* Values */
	drawVolValue(video, bmp, 253, 236, (int32_t)state.startVol);
	drawVolValue(video, bmp, 253, 250, (int32_t)state.endVol);
}

/* ------------------------------------------------------------------------- */
/*                      VOLUME APPLICATION                                   */
/* ------------------------------------------------------------------------- */

/* Get sample editor range or default to full sample */
static bool getRange(ft2_instance_t *inst, ft2_sample_t *s, int32_t *outX1, int32_t *outX2)
{
	ft2_sample_editor_t *ed = (inst && inst->ui) ? FT2_SAMPLE_ED(inst) : NULL;
	if (ed && ed->hasRange && ed->rangeStart < ed->rangeEnd)
	{
		*outX1 = (ed->rangeStart < 0) ? 0 : ed->rangeStart;
		*outX2 = (ed->rangeEnd > s->length) ? s->length : ed->rangeEnd;
	}
	else
	{
		*outX1 = 0;
		*outX2 = s->length;
	}
	return (*outX2 > *outX1);
}

/*
** Apply linear volume ramp from startVol to endVol across sample range.
** Handles 8-bit and 16-bit samples with clipping.
*/
static void applyVolumeToSample(void)
{
	if (!state.instance) return;
	ft2_instance_t *inst = state.instance;
	ft2_sample_t *s = getCurrentSample();
	if (!s || !s->dataPtr || !s->length) return;

	int32_t x1, x2;
	if (!getRange(inst, s, &x1, &x2)) return;
	const int32_t len = x2 - x1;
	if (state.startVol == 100.0 && state.endVol == 100.0) return;

	ft2_stop_sample_voices(inst, s);
	ft2_unfix_sample(s);

	const bool ramp = (state.startVol != state.endVol);
	const double dVolDelta = ramp ? ((state.endVol - state.startVol) / 100.0) / len : 0.0;
	double dVol = state.startVol / 100.0;

	if (s->flags & FT2_SAMPLE_16BIT)
	{
		int16_t *ptr = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++, dVol += dVolDelta)
		{
			int32_t v = (int32_t)(ptr[i] * dVol);
			ptr[i] = (v < -32768) ? -32768 : (v > 32767) ? 32767 : (int16_t)v;
		}
	}
	else
	{
		int8_t *ptr = s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++, dVol += dVolDelta)
		{
			int32_t v = (int32_t)(ptr[i] * dVol);
			ptr[i] = (v < -128) ? -128 : (v > 127) ? 127 : (int8_t)v;
		}
	}

	ft2_fix_sample(s);
	inst->uiState.updateSampleEditor = true;
}

/* Find maximum amplification that won't clip (normalize calculation) */
static double calculateMaxScale(void)
{
	ft2_sample_t *s = getCurrentSample();
	if (!s || !s->dataPtr || !s->length) return 100.0;

	int32_t x1, x2;
	if (!getRange(state.instance, s, &x1, &x2)) return 100.0;
	uint32_t len = x2 - x1;

	ft2_unfix_sample(s);

	int32_t maxAmp = 0;
	if (s->flags & FT2_SAMPLE_16BIT)
	{
		const int16_t *ptr = (const int16_t *)s->dataPtr + x1;
		for (uint32_t i = 0; i < len; i++)
		{
			int32_t a = (ptr[i] < 0) ? -ptr[i] : ptr[i];
			if (a > maxAmp) maxAmp = a;
		}
	}
	else
	{
		const int8_t *ptr = s->dataPtr + x1;
		for (uint32_t i = 0; i < len; i++)
		{
			int32_t a = (ptr[i] < 0) ? -ptr[i] : ptr[i];
			if (a > maxAmp) maxAmp = a;
		}
	}

	ft2_fix_sample(s);

	double scale = 100.0;
	if (maxAmp > 0)
		scale = ((s->flags & FT2_SAMPLE_16BIT) ? 32767.0 : 127.0) / maxAmp * 100.0;
	return (scale < 100.0) ? 100.0 : scale;
}

/* ------------------------------------------------------------------------- */
/*                          PUBLIC API                                       */
/* ------------------------------------------------------------------------- */

void ft2_volume_panel_show(ft2_instance_t *inst)
{
	if (!inst) return;
	ft2_sample_t *s = NULL;
	if (inst->editor.curInstr != 0 && inst->editor.curInstr <= 128)
	{
		ft2_instr_t *instr = inst->replayer.instr[inst->editor.curInstr];
		if (instr) s = &instr->smp[inst->editor.curSmp];
	}
	if (!s || !s->dataPtr || !s->length) return;

	state.active = true;
	state.instance = inst;
	state.startVol = state.endVol = 100.0;
	setupWidgets();
	ft2_modal_panel_set_active(MODAL_PANEL_VOLUME);
}

void ft2_volume_panel_hide(void)
{
	if (!state.active) return;
	hideWidgets();
	state.active = false;
	if (state.instance) state.instance->uiState.updateSampleEditor = true;
	state.instance = NULL;
	ft2_modal_panel_set_inactive(MODAL_PANEL_VOLUME);
}

bool ft2_volume_panel_is_active(void) { return state.active; }

void ft2_volume_panel_draw(ft2_video_t *video, const ft2_bmp_t *bmp)
{
	if (!state.active || !video) return;
	drawFrame(video, bmp);

	ft2_widgets_t *widgets = (state.instance && state.instance->ui) ?
		&((ft2_ui_t *)state.instance->ui)->widgets : NULL;
	if (!widgets) return;

	setScrollBarPos(state.instance, widgets, video, SB_RES_1, (uint32_t)(state.startVol + 200), false);
	setScrollBarPos(state.instance, widgets, video, SB_RES_2, (uint32_t)(state.endVol + 200), false);

	for (int i = 0; i < 8; i++)
		if (widgets->pushButtonVisible[PB_RES_1 + i])
			drawPushButton(widgets, video, bmp, PB_RES_1 + i);
	for (int i = 0; i < 2; i++)
		if (widgets->scrollBarState[SB_RES_1 + i].visible)
			drawScrollBar(widgets, video, SB_RES_1 + i);
}

void ft2_volume_panel_apply(void)
{
	if (!state.active) return;
	applyVolumeToSample();
	ft2_volume_panel_hide();
}

void ft2_volume_panel_find_max_scale(void)
{
	if (!state.active) return;
	double maxScale = calculateMaxScale();
	state.startVol = state.endVol = maxScale;
}

double ft2_volume_panel_get_start_vol(void) { return state.startVol; }
void ft2_volume_panel_set_start_vol(double vol) { state.startVol = vol; }
double ft2_volume_panel_get_end_vol(void) { return state.endVol; }
void ft2_volume_panel_set_end_vol(double vol) { state.endVol = vol; }
