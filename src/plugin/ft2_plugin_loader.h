/**
 * @file ft2_plugin_loader.h
 * @brief Module loader (auto-detects XM, MOD, S3M).
 */

#pragma once

#include "../ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Auto-detect format and load */
bool ft2_load_module(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

/* Load XM directly */
bool ft2_load_xm_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

#ifdef __cplusplus
}
#endif
