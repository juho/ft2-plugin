/*
** FT2 Plugin - Replayer Core API
** Instance-based XM playback engine operating on ft2_instance_t.
*/

#pragma once

#include "ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Main tick/mix */
void ft2_replayer_tick(ft2_instance_t *inst);
void ft2_update_voices(ft2_instance_t *inst);
void ft2_mix_voices(ft2_instance_t *inst, int32_t bufferPos, int32_t samplesToMix);
void ft2_mix_voices_multiout(ft2_instance_t *inst, int32_t bufferPos, int32_t samplesToMix);

/* Tempo/timing */
void ft2_set_bpm(ft2_instance_t *inst, int32_t bpm);
void ft2_set_interpolation(ft2_instance_t *inst, uint8_t type);
uint64_t ft2_period_to_delta(ft2_instance_t *inst, uint32_t period);

/* Voice control */
void ft2_trigger_voice(ft2_instance_t *inst, int32_t voiceNum, ft2_sample_t *smp, int32_t startPos);
void ft2_stop_voice(ft2_instance_t *inst, int32_t voiceNum);
void ft2_stop_all_voices(ft2_instance_t *inst);
void ft2_fadeout_all_voices(ft2_instance_t *inst);
void ft2_stop_sample_voices(ft2_instance_t *inst, struct ft2_sample_t *smp);
void ft2_voice_update_volumes(ft2_instance_t *inst, int32_t voiceNum, uint8_t status);
void ft2_reset_ramp_volumes(ft2_instance_t *inst);

/* Channel helpers (for keyjazz/preview) */
void ft2_channel_reset_volumes(ft2_channel_t *ch);
void ft2_channel_trigger_instrument(ft2_channel_t *ch);
void ft2_channel_update_vol_pan_autovib(ft2_instance_t *inst, ft2_channel_t *ch);

#ifdef __cplusplus
}
#endif

