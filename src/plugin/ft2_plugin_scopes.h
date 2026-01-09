/*
** FT2 Plugin - Scope Rendering API
** Real-time channel waveform display with interpolation, mute/solo, multi-rec.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;
struct ft2_instance_t;

/* Limits and dimensions */
#define MAX_CHANNELS 32
#define SCOPE_HEIGHT 36
#define SCOPE_HZ 64  /* Update rate in Hz */

/* Fixed-point position (32.32) */
#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE ((int64_t)1 << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

/* Loop types */
#define LOOP_OFF 0
#define LOOP_FORWARD 1
#define LOOP_BIDI 2

/* Interpolation LUT constants (4-point cubic B-spline) */
#define SCOPE_INTRP_WIDTH 4
#define SCOPE_INTRP_WIDTH_BITS 2
#define SCOPE_INTRP_SCALE 32768
#define SCOPE_INTRP_SCALE_BITS 15
#define SCOPE_INTRP_PHASES 512
#define SCOPE_INTRP_PHASES_BITS 9
#define MAX_LEFT_TAPS 2

/* Per-channel scope state */
typedef struct scope_t
{
	volatile bool active;           /* Currently playing */
	const int8_t *base8;            /* 8-bit sample data pointer */
	const int16_t *base16;          /* 16-bit sample data pointer */
	bool wasCleared;                /* Background needs redraw */
	bool sample16Bit;               /* True if 16-bit sample */
	bool samplingBackwards;         /* Bidi loop direction */
	bool hasLooped;                 /* Has passed loop point */
	uint8_t loopType;               /* LOOP_OFF/LOOP_FORWARD/LOOP_BIDI */
	int16_t volume;                 /* Display amplitude scale */
	int32_t loopStart, loopLength, loopEnd, sampleEnd, position;
	uint64_t delta;                 /* Position increment (64 Hz) */
	uint64_t drawDelta;             /* Position increment (display rate) */
	uint64_t positionFrac;          /* Fractional position */
	const int8_t *leftEdgeTaps8;    /* Left padding for interpolation */
	const int16_t *leftEdgeTaps16;
} scope_t;

/* Scopes manager state */
typedef struct ft2_scopes_t
{
	scope_t scopes[MAX_CHANNELS];
	bool channelMuted[MAX_CHANNELS];
	bool multiRecChn[MAX_CHANNELS]; /* Multi-record channel flags */
	int16_t numChannels;            /* Active channel count */
	bool linedScopes;               /* Lined vs dotted display */
	bool ptnChnNumbers;             /* Show channel numbers */
	bool needsFrameworkRedraw;      /* Redraw all scope borders */
	uint8_t interpolation;          /* Interpolation mode */
	int16_t *scopeIntrpLUT;         /* Cubic interpolation table */
	uint64_t lastUpdateTick;        /* Timestamp for 64 Hz timing */
} ft2_scopes_t;

typedef void (*scopeDrawRoutine)(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, struct ft2_video_t *video, struct ft2_scopes_t *scopes);

/* Scope width tables */
extern const uint16_t scopeLenTab[16][32];
extern const uint8_t scopeMuteBMP_Widths[16];
extern const uint8_t scopeMuteBMP_Heights[16];
extern const uint16_t scopeMuteBMP_Offs[16];

/* Lifecycle */
void ft2_scopes_init(ft2_scopes_t *scopes);
void ft2_scopes_free(ft2_scopes_t *scopes);

/* Update (call from UI thread) */
void ft2_scopes_update(ft2_scopes_t *scopes, struct ft2_instance_t *inst);

/* Period conversion */
uint64_t ft2_period_to_scope_delta(struct ft2_instance_t *inst, uint32_t period);
uint64_t ft2_period_to_scope_draw_delta(struct ft2_instance_t *inst, uint32_t period);

/* Scope control */
void ft2_scope_stop(ft2_scopes_t *scopes, int channel);
void ft2_scopes_stop_all(ft2_scopes_t *scopes);

/* Drawing */
void ft2_scopes_draw(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void ft2_scopes_draw_framework(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Mouse handling (left=mute, right=multi-rec, both=solo) */
bool ft2_scopes_mouse_down(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, bool leftButton, bool rightButton);

/* Mute state */
void ft2_scopes_set_mute(ft2_scopes_t *scopes, int channel, bool muted);
bool ft2_scopes_get_mute(ft2_scopes_t *scopes, int channel);

#ifdef __cplusplus
}
#endif
