/**
 * @file ft2_plugin_load_wav.h
 * @brief WAV sample loading and saving functions.
 *
 * Handles loading WAV files into sample slots, saving samples as WAV,
 * and WAV file format parsing/creation utilities.
 */

#ifndef FT2_PLUGIN_LOAD_WAV_H
#define FT2_PLUGIN_LOAD_WAV_H

#include <stdint.h>
#include <stdbool.h>
#include "ft2_instance.h"

/* Sample load/save */
bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                     const uint8_t *data, uint32_t dataSize);
bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                     uint8_t **outData, uint32_t *outSize);
void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                       int16_t sampleNum, const char *filename);

/* Sample tuning helper (sets relativeNote and finetune from sample rate) */
void ft2_set_sample_c4hz(ft2_sample_t *s, double dC4Hz);

/* WAV helpers */
bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                   const uint8_t **outAudioData, uint32_t *outAudioSize,
                   int16_t *outChannels, int16_t *outBitsPerSample,
                   uint32_t *outSampleRate);
bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                    int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                    uint8_t **outData, uint32_t *outSize);

#endif
