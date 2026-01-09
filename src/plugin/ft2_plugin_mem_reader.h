/**
 * @file ft2_plugin_mem_reader.h
 * @brief Memory buffer reader for module loaders.
 *
 * File-like API for reading from memory buffers. Used by XM/MOD/S3M loaders.
 */

#ifndef FT2_PLUGIN_MEM_READER_H
#define FT2_PLUGIN_MEM_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct mem_reader_t {
	const uint8_t *data;
	uint32_t size;
	uint32_t pos;
} mem_reader_t;

static inline void mem_init(mem_reader_t *r, const uint8_t *data, uint32_t size)
{
	r->data = data;
	r->size = size;
	r->pos = 0;
}

/* Returns false if read would exceed buffer */
static inline bool mem_read(mem_reader_t *r, void *dst, uint32_t size)
{
	if (r->pos + size > r->size) return false;
	memcpy(dst, &r->data[r->pos], size);
	r->pos += size;
	return true;
}

static inline bool mem_skip(mem_reader_t *r, uint32_t size)
{
	if (r->pos + size > r->size) return false;
	r->pos += size;
	return true;
}

static inline bool mem_seek(mem_reader_t *r, uint32_t pos)
{
	if (pos > r->size) return false;
	r->pos = pos;
	return true;
}

static inline uint32_t mem_tell(mem_reader_t *r) { return r->pos; }
static inline bool mem_eof(mem_reader_t *r) { return r->pos >= r->size; }

/* Direct pointer access at current position (NULL if at/past end) */
static inline const uint8_t *mem_ptr(mem_reader_t *r)
{
	return (r->pos >= r->size) ? NULL : &r->data[r->pos];
}

static inline uint32_t mem_remaining(mem_reader_t *r)
{
	return (r->pos >= r->size) ? 0 : r->size - r->pos;
}

#endif /* FT2_PLUGIN_MEM_READER_H */

