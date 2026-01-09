/**
 * @file ft2_plugin_diskop.h
 * @brief Disk operations: file browser, load/save for modules, instruments, samples, patterns.
 *
 * FT2-style file browser with JUCE-backed directory I/O.
 * Item types: Module, Instrument, Sample, Pattern, Track.
 * Delete/Rename removed in plugin (sandbox restrictions).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ft2_video_t;
struct ft2_bmp_t;

/* Screen management */
void showDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void hideDiskOpScreen(ft2_instance_t *inst);
void toggleDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);
void drawDiskOpScreen(ft2_instance_t *inst, struct ft2_video_t *video, const struct ft2_bmp_t *bmp);

/* Navigation callbacks */
void pbDiskOpParent(ft2_instance_t *inst);
void pbDiskOpRoot(ft2_instance_t *inst);
void pbDiskOpHome(ft2_instance_t *inst);
#ifdef _WIN32
void pbDiskOpDrive1(ft2_instance_t *inst);
void pbDiskOpDrive2(ft2_instance_t *inst);
void pbDiskOpDrive3(ft2_instance_t *inst);
void pbDiskOpDrive4(ft2_instance_t *inst);
void pbDiskOpDrive5(ft2_instance_t *inst);
void pbDiskOpDrive6(ft2_instance_t *inst);
void pbDiskOpDrive7(ft2_instance_t *inst);
#endif
void pbDiskOpRefresh(ft2_instance_t *inst);
void pbDiskOpShowAll(ft2_instance_t *inst);
void pbDiskOpSetPath(ft2_instance_t *inst);
void pbDiskOpExit(ft2_instance_t *inst);

/* File operations */
void pbDiskOpSave(ft2_instance_t *inst);
void pbDiskOpDelete(ft2_instance_t *inst);  /* Disabled in plugin */
void pbDiskOpRename(ft2_instance_t *inst);  /* Disabled in plugin */
void pbDiskOpMakeDir(ft2_instance_t *inst);

/* List scrolling */
void pbDiskOpListUp(ft2_instance_t *inst);
void pbDiskOpListDown(ft2_instance_t *inst);
void sbDiskOpSetPos(ft2_instance_t *inst, uint32_t pos);

/* Item type selection */
void rbDiskOpModule(ft2_instance_t *inst);
void rbDiskOpInstr(ft2_instance_t *inst);
void rbDiskOpSample(ft2_instance_t *inst);
void rbDiskOpPattern(ft2_instance_t *inst);
void rbDiskOpTrack(ft2_instance_t *inst);

/* Save format selection */
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

/* Mouse handling */
bool diskOpTestMouseDown(ft2_instance_t *inst, int32_t mouseX, int32_t mouseY);
void diskOpHandleItemClick(ft2_instance_t *inst, int32_t entryIndex);

void freeDiskOp(ft2_instance_t *inst);

/* File format detection */
typedef enum ft2_file_format {
	FT2_FORMAT_UNKNOWN = 0,
	FT2_FORMAT_XM,    /* FastTracker 2 Module */
	FT2_FORMAT_MOD,   /* ProTracker Module */
	FT2_FORMAT_S3M,   /* Scream Tracker 3 Module */
	FT2_FORMAT_XI,    /* FastTracker 2 Instrument */
	FT2_FORMAT_WAV,   /* WAV Audio */
	FT2_FORMAT_AIFF,  /* AIFF Audio */
	FT2_FORMAT_RAW,   /* Raw sample data */
	FT2_FORMAT_PAT    /* Pattern data */
} ft2_file_format;

ft2_file_format ft2_detect_format_by_ext(const char *filename);
ft2_file_format ft2_detect_format_by_header(const uint8_t *data, uint32_t dataSize);

/* Module save (load in ft2_plugin_loader.h) */
bool ft2_save_module(ft2_instance_t *inst, uint8_t **outData, uint32_t *outSize);

/* Instrument load/save (XI format) */
bool ft2_load_instrument(ft2_instance_t *inst, int16_t instrNum,
                          const uint8_t *data, uint32_t dataSize);
bool ft2_save_instrument(ft2_instance_t *inst, int16_t instrNum,
                          uint8_t **outData, uint32_t *outSize);

/* Sample load/save (WAV format) */
bool ft2_load_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      const uint8_t *data, uint32_t dataSize);
bool ft2_save_sample(ft2_instance_t *inst, int16_t instrNum, int16_t sampleNum,
                      uint8_t **outData, uint32_t *outSize);
void ft2_set_sample_name_from_filename(ft2_instance_t *inst, int16_t instrNum,
                                        int16_t sampleNum, const char *filename);

/* Pattern load/save (XP format) */
bool ft2_load_pattern(ft2_instance_t *inst, int16_t pattNum,
                       const uint8_t *data, uint32_t dataSize);
bool ft2_save_pattern(ft2_instance_t *inst, int16_t pattNum,
                       uint8_t **outData, uint32_t *outSize);

/* WAV helpers */
bool ft2_parse_wav(const uint8_t *data, uint32_t dataSize,
                    const uint8_t **outAudioData, uint32_t *outAudioSize,
                    int16_t *outChannels, int16_t *outBitsPerSample,
                    uint32_t *outSampleRate);
bool ft2_create_wav(const int8_t *sampleData, int32_t sampleLength,
                     int16_t channels, int16_t bitsPerSample, uint32_t sampleRate,
                     uint8_t **outData, uint32_t *outSize);

/* Drag-and-drop: confirms if unsaved changes, then sets requestDropLoad */
void ft2_diskop_request_drop_load(ft2_instance_t *inst, const char *path);

#ifdef __cplusplus
}
#endif

