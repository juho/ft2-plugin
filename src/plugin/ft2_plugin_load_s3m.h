/**
 * @file ft2_plugin_load_s3m.h
 * @brief S3M loader (Scream Tracker 3).
 */

#ifndef FT2_PLUGIN_LOAD_S3M_H
#define FT2_PLUGIN_LOAD_S3M_H

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;

bool load_s3m_from_memory(struct ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);
bool detect_s3m_format(const uint8_t *data, uint32_t dataSize);

#endif /* FT2_PLUGIN_LOAD_S3M_H */

