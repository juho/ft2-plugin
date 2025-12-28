#pragma once

/**
 * @file ft2_plugin_replayer.h
 * @brief Instance-based replayer core for plugin architecture.
 *
 * This module provides the core replayer tick and mixing functions
 * that operate on an ft2_instance_t rather than global state.
 */

#include "ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Executes one tick of the replayer.
 * @param inst The instance.
 */
void ft2_replayer_tick(ft2_instance_t *inst);

/**
 * @brief Updates voice parameters from channel state.
 * @param inst The instance.
 */
void ft2_update_voices(ft2_instance_t *inst);

/**
 * @brief Mixes voices to the instance's mix buffer.
 * @param inst The instance.
 * @param bufferPos Starting position in the mix buffer.
 * @param samplesToMix Number of samples to mix.
 */
void ft2_mix_voices(ft2_instance_t *inst, int32_t bufferPos, int32_t samplesToMix);

/**
 * @brief Sets the BPM for the instance.
 * @param inst The instance.
 * @param bpm The BPM value (32-255).
 */
void ft2_set_bpm(ft2_instance_t *inst, int32_t bpm);

/**
 * @brief Calculates the voice delta from a period value.
 * @param inst The instance.
 * @param period The period value.
 * @return The delta value for the mixer.
 */
uint64_t ft2_period_to_delta(ft2_instance_t *inst, uint32_t period);

/**
 * @brief Triggers a voice with sample data.
 * @param inst The instance.
 * @param voiceNum Voice number.
 * @param smp Sample pointer.
 * @param startPos Starting position in samples.
 */
void ft2_trigger_voice(ft2_instance_t *inst, int32_t voiceNum, ft2_sample_t *smp, int32_t startPos);

/**
 * @brief Stops a voice.
 * @param inst The instance.
 * @param voiceNum Voice number.
 */
void ft2_stop_voice(ft2_instance_t *inst, int32_t voiceNum);

/**
 * @brief Stops all voices (main and fadeout).
 * @param inst The instance.
 */
void ft2_stop_all_voices(ft2_instance_t *inst);

/**
 * @brief Stops all voices playing a specific sample.
 * @param inst The instance.
 * @param smp The sample to stop.
 */
void ft2_stop_sample_voices(ft2_instance_t *inst, struct ft2_sample_t *smp);

/**
 * @brief Updates a voice's volume with ramping.
 * @param inst The instance.
 * @param voiceNum Voice number.
 * @param status Channel status flags.
 */
void ft2_voice_update_volumes(ft2_instance_t *inst, int32_t voiceNum, uint8_t status);

/**
 * @brief Resets volume ramps (called at start of each tick).
 * @param inst The instance.
 */
void ft2_reset_ramp_volumes(ft2_instance_t *inst);

/**
 * @brief Sets the interpolation type for the mixer.
 * @param inst The instance.
 * @param type 0 = no interpolation (point), 1 = linear.
 */
void ft2_set_interpolation(ft2_instance_t *inst, uint8_t type);

/**
 * @brief Resets channel volumes from sample defaults.
 * @param ch Channel to reset.
 */
void ft2_channel_reset_volumes(ft2_channel_t *ch);

/**
 * @brief Initializes instrument state (envelopes, fadeout, auto-vibrato).
 * @param ch Channel to initialize.
 */
void ft2_channel_trigger_instrument(ft2_channel_t *ch);

/**
 * @brief Updates volume, panning, and auto-vibrato for a channel.
 * @param inst The instance.
 * @param ch Channel to update.
 */
void ft2_channel_update_vol_pan_autovib(ft2_instance_t *inst, ft2_channel_t *ch);

#ifdef __cplusplus
}
#endif

