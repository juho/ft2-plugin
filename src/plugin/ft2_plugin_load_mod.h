/**
 * @file ft2_plugin_load_mod.h
 * @brief MOD module loader for plugin architecture.
 */

#ifndef FT2_PLUGIN_LOAD_MOD_H
#define FT2_PLUGIN_LOAD_MOD_H

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;

/**
 * Load a MOD module from memory buffer.
 * 
 * @param inst The FT2 instance to load into
 * @param data Pointer to MOD file data
 * @param dataSize Size of the data buffer
 * @return true if successful, false on error
 */
bool load_mod_from_memory(struct ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

/**
 * Check if data appears to be a MOD file.
 * 
 * @param data Pointer to file data (needs at least 1084 bytes)
 * @param dataSize Size of the data buffer
 * @return true if appears to be MOD format
 */
bool detect_mod_format(const uint8_t *data, uint32_t dataSize);

#endif /* FT2_PLUGIN_LOAD_MOD_H */

