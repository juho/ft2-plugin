#pragma once

/**
 * @file ft2_plugin_diskop.h
 * @brief Disk operations for the plugin (load/save modules, samples).
 *
 * Provides FT2-style file browser with native dialog fallback when sandboxed.
 * Supports loading/saving modules, instruments, samples, patterns, and tracks.
 */

#include <stdint.h>
#include <stdbool.h>
#include "ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;

/* Screen visibility */
void showDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideDiskOpScreen(ft2_instance_t *inst);
void toggleDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void drawDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Directory navigation callbacks */
void pbDiskOpParent(ft2_instance_t *inst);
void pbDiskOpRoot(ft2_instance_t *inst);
void pbDiskOpHome(ft2_instance_t *inst);
void pbDiskOpRefresh(ft2_instance_t *inst);
void pbDiskOpShowAll(ft2_instance_t *inst);
void pbDiskOpSetPath(ft2_instance_t *inst);
void pbDiskOpExit(ft2_instance_t *inst);

/* File operations callbacks */
void pbDiskOpSave(ft2_instance_t *inst);
void pbDiskOpDelete(ft2_instance_t *inst);
void pbDiskOpRename(ft2_instance_t *inst);
void pbDiskOpMakeDir(ft2_instance_t *inst);

/* List navigation callbacks */
void pbDiskOpListUp(ft2_instance_t *inst);
void pbDiskOpListDown(ft2_instance_t *inst);
void sbDiskOpSetPos(ft2_instance_t *inst, uint32_t pos);

/* Item type selection callbacks */
void rbDiskOpModule(ft2_instance_t *inst);
void rbDiskOpInstr(ft2_instance_t *inst);
void rbDiskOpSample(ft2_instance_t *inst);
void rbDiskOpPattern(ft2_instance_t *inst);
void rbDiskOpTrack(ft2_instance_t *inst);

/* Save format selection callbacks */
void rbDiskOpModSaveMod(ft2_instance_t *inst);
void rbDiskOpModSaveXm(ft2_instance_t *inst);
void rbDiskOpModSaveWav(ft2_instance_t *inst);
void rbDiskOpSmpSaveRaw(ft2_instance_t *inst);
void rbDiskOpSmpSaveIff(ft2_instance_t *inst);
void rbDiskOpSmpSaveWav(ft2_instance_t *inst);

/* Directory operations (JUCE-backed) */
void diskOpReadDirectory(ft2_instance_t *inst);
void diskOpDrawFilelist(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void diskOpDrawDirectory(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* File list mouse handling */
bool diskOpTestMouseDown(ft2_instance_t *inst, int32_t mouseX, int32_t mouseY);
void diskOpHandleItemClick(ft2_instance_t *inst, int32_t entryIndex);

/* Free disk op resources */
void freeDiskOp(ft2_instance_t *inst);

/* File format types */
typedef enum ft2_file_format
{
	FT2_FORMAT_UNKNOWN = 0,
	FT2_FORMAT_XM,       /* FastTracker 2 Module */
	FT2_FORMAT_MOD,      /* ProTracker Module */
	FT2_FORMAT_S3M,      /* Scream Tracker 3 Module */
	FT2_FORMAT_XI,       /* FastTracker 2 Instrument */
	FT2_FORMAT_WAV,      /* WAV Audio */
	FT2_FORMAT_AIFF,     /* AIFF Audio */
	FT2_FORMAT_RAW,      /* Raw sample data */
	FT2_FORMAT_PAT       /* Pattern data */
} ft2_file_format;

/**
 * @brief Detect file format from file extension.
 * @param filename The filename (with extension).
 * @return The detected format, or FT2_FORMAT_UNKNOWN if not recognized.
 */
ft2_file_format ft2_detect_format_by_ext(const char *filename);

/**
 * @brief Detect file format from file header.
 * @param data Pointer to file data.
 * @param dataSize Size of file data.
 * @return The detected format, or FT2_FORMAT_UNKNOWN if not recognized.
 */
ft2_file_format ft2_detect_format_by_header(const uint8_t *data, uint32_t dataSize);

/* Module operations */

/**
 * @brief Load module from memory buffer.
 * @param inst The FT2 instance.
 * @param data Pointer to module data.
 * @param dataSize Size of data.
 * @return true on success, false on failure.
 */
bool ft2_load_module(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

/**
 * @brief Save module to memory buffer.
 * @param inst The FT2 instance.
 * @param outData Output pointer to allocated data (caller must free).
 * @param outSize Output size of data.
 * @return true on success, false on failure.
 */
bool ft2_save_module(ft2_instance_t *inst, uint8_t **outData, uint32_t *outSize);

/* Instrument operations */

/**
 * @brief Load instrument from memory buffer.
 * @param inst The FT2 instance.
 * @param instrNum Instrument slot (1-128).
 * @param data Pointer to instrument data.
 * @param dataSize Size of data.
 * @return true on success, false on failure.
 */
bool ft2_load_instrument(ft2_instance_t *inst, int16_t instrNum, 
                          const uint8_t *data, uint32_t dataSize);

/**
 * @brief Save instrument to memory buffer.
 * @param inst The FT2 instance.
 * @param instrNum Instrument slot (1-128).
 * @param outData Output pointer to allocated data (caller must free).
 * @param outSize Output size of data.
 * @return true on success, false on failure.
 */
bool ft2_save_instrument(ft2_instance_t *inst, int16_t instrNum,
                          uint8_t **outData, uint32_t *outSize);

/* Sample operations */

/**
 * @brief Load sample from memory buffer (WAV, AIFF, or raw).
 * @param inst The FT2 instance.
 * @param instrNum Instrument slot (1-128).
 * @param sampleNum Sample slot (0-15).
 * @param data Pointer to sample data.
 * @param dataSize Size of data.
 * @return true on success, false on failure.
 */
bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      const uint8_t *data, uint32_t dataSize);

/**
 * @brief Set sample name from a filename.
 *
 * Extracts the filename without path/extension and sets it as the sample name.
 *
 * @param inst The FT2 instance.
 * @param instrNum Instrument slot (1-128).
 * @param sampleNum Sample slot (0-15).
 * @param filename The full file path or filename.
 */
void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                        int16_t sampleNum, const char *filename);

/**
 * @brief Save sample to memory buffer (as WAV).
 * @param inst The FT2 instance.
 * @param instrNum Instrument slot (1-128).
 * @param sampleNum Sample slot (0-15).
 * @param outData Output pointer to allocated data (caller must free).
 * @param outSize Output size of data.
 * @return true on success, false on failure.
 */
bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      uint8_t **outData, uint32_t *outSize);

/* Pattern operations */

/**
 * @brief Load pattern from memory buffer.
 * @param inst The FT2 instance.
 * @param pattNum Pattern slot (0-255).
 * @param data Pointer to pattern data.
 * @param dataSize Size of data.
 * @return true on success, false on failure.
 */
bool ft2_load_pattern(ft2_instance_t *inst, int16_t pattNum,
                       const uint8_t *data, uint32_t dataSize);

/**
 * @brief Save pattern to memory buffer.
 * @param inst The FT2 instance.
 * @param pattNum Pattern slot (0-255).
 * @param outData Output pointer to allocated data (caller must free).
 * @param outSize Output size of data.
 * @return true on success, false on failure.
 */
bool ft2_save_pattern(ft2_instance_t *inst, int16_t pattNum,
                       uint8_t **outData, uint32_t *outSize);

/* WAV helpers */

/**
 * @brief Parse WAV file header and get audio data info.
 * @param data Pointer to WAV data.
 * @param dataSize Size of WAV data.
 * @param outAudioData Output pointer to audio data start.
 * @param outAudioSize Output size of audio data.
 * @param outChannels Output number of channels (1 or 2).
 * @param outBitsPerSample Output bits per sample (8 or 16).
 * @param outSampleRate Output sample rate.
 * @return true on success, false on failure.
 */
bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                    const uint8_t **outAudioData, uint32_t *outAudioSize,
                    int16_t *outChannels, int16_t *outBitsPerSample,
                    uint32_t *outSampleRate);

/**
 * @brief Create WAV file from sample data.
 * @param sampleData Pointer to sample data.
 * @param sampleLength Number of samples.
 * @param channels Number of channels (1 or 2).
 * @param bitsPerSample Bits per sample (8 or 16).
 * @param sampleRate Sample rate.
 * @param outData Output pointer to allocated WAV data (caller must free).
 * @param outSize Output size of WAV data.
 * @return true on success, false on failure.
 */
bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                     int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                     uint8_t **outData, uint32_t *outSize);

/**
 * @brief Request loading a dropped module file with unsaved changes check.
 *
 * If the current song has unsaved changes, shows a confirmation dialog.
 * If confirmed (or no changes), sets requestDropLoad to signal JUCE to load.
 *
 * @param inst The FT2 instance.
 * @param path Full path to the dropped module file.
 */
void ft2_diskop_request_drop_load(ft2_instance_t *inst, const char *path);

#ifdef __cplusplus
}
#endif

