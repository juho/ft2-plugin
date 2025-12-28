/**
 * @file ft2_plugin_mem_reader.h
 * @brief Memory buffer reader utilities for module loaders.
 *
 * Shared utilities for reading module data from memory buffers
 * instead of file handles.
 */

#ifndef FT2_PLUGIN_MEM_READER_H
#define FT2_PLUGIN_MEM_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/**
 * Memory reader context for parsing module data from buffers.
 */
typedef struct mem_reader_t
{
	const uint8_t *data;
	uint32_t size;
	uint32_t pos;
} mem_reader_t;

/**
 * Initialize a memory reader.
 */
static inline void mem_init(mem_reader_t *r, const uint8_t *data, uint32_t size)
{
	r->data = data;
	r->size = size;
	r->pos = 0;
}

/**
 * Read bytes from memory buffer.
 * @return true if successful, false if would read past end
 */
static inline bool mem_read(mem_reader_t *r, void *dst, uint32_t size)
{
	if (r->pos + size > r->size)
		return false;
	memcpy(dst, &r->data[r->pos], size);
	r->pos += size;
	return true;
}

/**
 * Skip bytes in memory buffer.
 * @return true if successful, false if would skip past end
 */
static inline bool mem_skip(mem_reader_t *r, uint32_t size)
{
	if (r->pos + size > r->size)
		return false;
	r->pos += size;
	return true;
}

/**
 * Seek to absolute position in memory buffer.
 * @return true if successful, false if position is past end
 */
static inline bool mem_seek(mem_reader_t *r, uint32_t pos)
{
	if (pos > r->size)
		return false;
	r->pos = pos;
	return true;
}

/**
 * Get current position in memory buffer.
 */
static inline uint32_t mem_tell(mem_reader_t *r)
{
	return r->pos;
}

/**
 * Check if at end of memory buffer.
 */
static inline bool mem_eof(mem_reader_t *r)
{
	return r->pos >= r->size;
}

/**
 * Get pointer to current position in buffer (for direct access).
 * @return pointer to current position, or NULL if at/past end
 */
static inline const uint8_t *mem_ptr(mem_reader_t *r)
{
	if (r->pos >= r->size)
		return NULL;
	return &r->data[r->pos];
}

/**
 * Get remaining bytes in buffer.
 */
static inline uint32_t mem_remaining(mem_reader_t *r)
{
	if (r->pos >= r->size)
		return 0;
	return r->size - r->pos;
}

#endif /* FT2_PLUGIN_MEM_READER_H */

