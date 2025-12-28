/**
 * @file ft2_plugin_load_s3m.h
 * @brief S3M module loader for plugin architecture.
 */

#ifndef FT2_PLUGIN_LOAD_S3M_H
#define FT2_PLUGIN_LOAD_S3M_H

#include <stdint.h>
#include <stdbool.h>

struct ft2_instance_t;

/**
 * Load an S3M module from memory buffer.
 * 
 * @param inst The FT2 instance to load into
 * @param data Pointer to S3M file data
 * @param dataSize Size of the data buffer
 * @return true if successful, false on error
 */
bool load_s3m_from_memory(struct ft2_instance_t *inst, const uint8_t *data, uint32_t dataSize);

/**
 * Check if data appears to be an S3M file.
 * 
 * @param data Pointer to file data (needs at least 48 bytes)
 * @param dataSize Size of the data buffer
 * @return true if appears to be S3M format
 */
bool detect_s3m_format(const uint8_t *data, uint32_t dataSize);

#endif /* FT2_PLUGIN_LOAD_S3M_H */

