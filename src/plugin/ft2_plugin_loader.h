#pragma once

/**
 * @file ft2_plugin_loader.h
 * @brief Memory-based module loader for plugin architecture.
 *
 * Supports XM, MOD, and S3M formats. Operates directly on
 * memory buffers rather than files.
 */

#include "../ft2_instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a module from memory, auto-detecting format.
 * Supports XM, MOD, and S3M formats.
 * 
 * @param inst The instance to load into.
 * @param data Pointer to the module file data.
 * @param dataSize Size of the data in bytes.
 * @return true on success, false on failure.
 */
bool ft2_load_module_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

/**
 * Load an XM module from memory.
 * 
 * @param inst The instance to load into.
 * @param data Pointer to the XM file data.
 * @param dataSize Size of the data in bytes.
 * @return true on success, false on failure.
 */
bool ft2_load_xm_from_memory(ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

#ifdef __cplusplus
}
#endif
