/**
 * @file ft2_plugin_load_mod.h
 * @brief MOD loader (ProTracker, NoiseTracker, StarTrekker, multi-channel).
 */

#ifndef FT2_PLUGIN_LOAD_MOD_H
#define FT2_PLUGIN_LOAD_MOD_H

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;

bool load_mod_from_memory(struct ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);
bool detect_mod_format(const uint8_t *data, uint32_t dataSize);

#endif /* FT2_PLUGIN_LOAD_MOD_H */

