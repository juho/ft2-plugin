/**
 * @file ft2_plugin_modal_panels.h
 * @brief Modal panel manager and consolidated state for sample editor dialogs.
 *
 * Only one panel can be active at a time. Each panel (Volume, Resample, Echo,
 * Mix, Wave, Filter) is a self-contained module; this manager tracks the
 * active panel and routes draw/close calls.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_instance_t;
struct ft2_video_t;
struct ft2_bmp_t;

typedef enum modal_panel_type_t {
	MODAL_PANEL_NONE = 0,
	MODAL_PANEL_VOLUME,
	MODAL_PANEL_RESAMPLE,
	MODAL_PANEL_ECHO,
	MODAL_PANEL_MIX,
	MODAL_PANEL_WAVE,
	MODAL_PANEL_FILTER
} modal_panel_type_t;

/* Wave generator types */
typedef enum wave_type_t {
	WAVE_TYPE_TRIANGLE,
	WAVE_TYPE_SAW,
	WAVE_TYPE_SINE,
	WAVE_TYPE_SQUARE
} wave_type_t;

/* Filter types */
typedef enum filter_type_t {
	FILTER_TYPE_LOWPASS,
	FILTER_TYPE_HIGHPASS
} filter_type_t;

/* Per-panel state structs */
typedef struct {
	bool active;
	double startVol, endVol;
} volume_panel_state_t;

typedef struct {
	bool active;
	int8_t relReSmp;
} resample_panel_state_t;

typedef struct {
	bool active;
	int16_t echoNum;
	int32_t echoDistance;
	int16_t echoVolChange;
	bool echoAddMemory;
} echo_panel_state_t;

typedef struct {
	bool active;
	int8_t mixBalance;
} mix_panel_state_t;

typedef struct {
	bool active;
	wave_type_t waveType;
	char inputBuffer[16];
	int inputCursorPos;
	int32_t lastWaveLength;
} wave_panel_state_t;

typedef struct {
	bool active;
	filter_type_t filterType;
	char inputBuffer[16];
	int inputCursorPos;
	int32_t lastLpCutoff;
	int32_t lastHpCutoff;
} filter_panel_state_t;

/* Consolidated modal panel state (per-instance) */
typedef struct ft2_modal_state_t {
	modal_panel_type_t activePanel;
	volume_panel_state_t volume;
	resample_panel_state_t resample;
	echo_panel_state_t echo;
	mix_panel_state_t mix;
	wave_panel_state_t wave;
	filter_panel_state_t filter;
} ft2_modal_state_t;

/* Initialize modal panel state with defaults */
void ft2_modal_panel_init(ft2_modal_state_t *state);

/* Manager functions (instance-aware) */
bool ft2_modal_panel_is_any_active(struct ft2_instance_t *inst);
modal_panel_type_t ft2_modal_panel_get_active(struct ft2_instance_t *inst);
void ft2_modal_panel_draw_active(struct ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_modal_panel_close_active(struct ft2_instance_t *inst);

/* Called by panel modules on show/hide */
void ft2_modal_panel_set_active(struct ft2_instance_t *inst, modal_panel_type_t type);
void ft2_modal_panel_set_inactive(struct ft2_instance_t *inst, modal_panel_type_t type);

#ifdef __cplusplus
}
#endif
