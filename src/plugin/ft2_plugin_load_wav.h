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

/* Stereo sample load mode (used with stereo loading dialog) */
typedef enum {
	STEREO_LOAD_AS_STEREO = 0,   /* Keep both channels (XM2 only) */
	STEREO_LOAD_LEFT_ONLY = 1,   /* Load left channel only */
	STEREO_LOAD_RIGHT_ONLY = 2,  /* Load right channel only */
	STEREO_LOAD_CONVERT_MONO = 3 /* Average L+R to mono */
} stereo_load_mode_t;

/* Pending stereo sample load data (stored while dialog is shown) */
typedef struct {
	bool pending;
	int16_t instrNum;
	int16_t sampleNum;
	uint8_t *data;           /* Copy of original file data */
	uint32_t dataSize;
	char filename[256];      /* For setting sample name */
} pending_stereo_load_t;

/* Sample load/save */
bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                     const uint8_t *data, uint32_t dataSize, const char *filename);
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
                   uint32_t *outSampleRate, bool *outIsFloat);
bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                    int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                    uint8_t **outData, uint32_t *outSize);

/* Complete a pending stereo sample load with specified mode */
bool ft2_complete_stereo_load(ft2_instance_t *inst, stereo_load_mode_t mode);

/* Cancel a pending stereo sample load */
void ft2_cancel_stereo_load(ft2_instance_t *inst);

/* Set filename for pending stereo load (called by JUCE after ft2_load_sample returns true) */
void ft2_set_pending_stereo_filename(ft2_instance_t *inst, const char *filename);

/* Check if a stereo load is pending */
bool ft2_is_stereo_load_pending(ft2_instance_t *inst);

#endif
