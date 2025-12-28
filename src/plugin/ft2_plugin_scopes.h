/**
 * @file ft2_plugin_scopes.h
 * @brief Exact port of FT2 scope rendering for the plugin.
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

#define MAX_CHANNELS 32
#define SCOPE_HEIGHT 36
#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE ((int64_t)1 << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

/* Loop types */
#define LOOP_OFF 0
#define LOOP_FORWARD 1
#define LOOP_BIDI 2

/* Scope interpolation constants */
#define SCOPE_INTRP_WIDTH 4
#define SCOPE_INTRP_WIDTH_BITS 2
#define SCOPE_INTRP_SCALE 32768
#define SCOPE_INTRP_SCALE_BITS 15
#define SCOPE_INTRP_PHASES 512
#define SCOPE_INTRP_PHASES_BITS 9

/* Maximum left edge taps for interpolation */
#define MAX_LEFT_TAPS 2

/* Scope update rate (matching standalone) */
#define SCOPE_HZ 64

/**
 * Scope state for one channel
 */
typedef struct scope_t
{
	volatile bool active;
	const int8_t *base8;
	const int16_t *base16;
	bool wasCleared, sample16Bit, samplingBackwards, hasLooped;
	uint8_t loopType;
	int16_t volume;
	int32_t loopStart, loopLength, loopEnd, sampleEnd, position;
	uint64_t delta, drawDelta, positionFrac;
	const int8_t *leftEdgeTaps8;
	const int16_t *leftEdgeTaps16;
} scope_t;

/**
 * Plugin scopes state
 */
typedef struct ft2_scopes_t
{
	scope_t scopes[MAX_CHANNELS];
	bool channelMuted[MAX_CHANNELS];
	bool multiRecChn[MAX_CHANNELS];
	int16_t numChannels;
	bool linedScopes;
	bool ptnChnNumbers;
	bool needsFrameworkRedraw;
	uint8_t interpolation;
	int16_t *scopeIntrpLUT;
	uint64_t lastUpdateTick;
} ft2_scopes_t;

/* Scope draw routine function pointer type */
typedef void (*scopeDrawRoutine)(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w, struct ft2_video_t *video, struct ft2_scopes_t *scopes);

/* Scope length tables (indexed by [numChannels/2 - 1][channel]) */
extern const uint16_t scopeLenTab[16][32];

/* Scope mute bitmap tables */
extern const uint8_t scopeMuteBMP_Widths[16];
extern const uint8_t scopeMuteBMP_Heights[16];
extern const uint16_t scopeMuteBMP_Offs[16];

/**
 * Initialize scopes
 */
void ft2_scopes_init(ft2_scopes_t *scopes);

/**
 * Free scopes resources
 */
void ft2_scopes_free(ft2_scopes_t *scopes);

/**
 * Update scopes from audio state
 */
void ft2_scopes_update(ft2_scopes_t *scopes, struct ft2_instance_t *inst);

/**
 * Calculate scope delta from period (matching standalone period2ScopeDelta)
 */
uint64_t ft2_period_to_scope_delta(struct ft2_instance_t *inst, uint32_t period);

/**
 * Calculate scope draw delta from period (matching standalone period2ScopeDrawDelta)
 */
uint64_t ft2_period_to_scope_draw_delta(struct ft2_instance_t *inst, uint32_t period);

/**
 * Stop a scope
 */
void ft2_scope_stop(ft2_scopes_t *scopes, int channel);

/**
 * Stop all scopes
 */
void ft2_scopes_stop_all(ft2_scopes_t *scopes);

/**
 * Draw scopes
 */
void ft2_scopes_draw(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Draw scope framework
 */
void ft2_scopes_draw_framework(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/**
 * Handle mouse click on scopes (mute toggle)
 */
bool ft2_scopes_mouse_down(ft2_scopes_t *scopes, struct ft2_video_t *video, const struct ft2_bmp_t *bmp, int32_t mouseX, int32_t mouseY, bool leftButton, bool rightButton);

/**
 * Toggle channel mute
 */
void ft2_scopes_set_mute(ft2_scopes_t *scopes, int channel, bool muted);

/**
 * Get channel mute state
 */
bool ft2_scopes_get_mute(ft2_scopes_t *scopes, int channel);

#ifdef __cplusplus
}
#endif
