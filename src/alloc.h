#ifndef _ALLOC_H
#define _ALLOC_H

#include "config.h"

#include <stdint.h>

#define SMALL_CHUNK_SIZE	256
#define CHUNK_OVERHEAD		4
#define CHUNK_GRANULARITY	8
#define MIN_CHUNK_SIZE		16
#define SMALL_BINS			(SMALL_CHUNK_SIZE / CHUNK_GRANULARITY)
#define SMALL_REQUEST		(SMALL_CHUNK_SIZE - CHUNK_OVERHEAD)

typedef uint32_t ptr_t;
typedef uint32_t bitmap_t;

struct binhdr {
	ptr_t prev, next;
};

struct chunk {
	uint32_t size;
	struct binhdr b;
};

struct alloc {
	int used_memory;
	uint32_t size;
	struct binhdr smallbins[SMALL_BINS];
	bitmap_t smallmap;
	struct binhdr largebin;
	ptr_t top;
	uint32_t topsize;
};

struct alloc* mem_new(uint32_t size, uint32_t reserved_size);
void mem_destroy(struct alloc* alloc);
void* mem_malloc(struct alloc* alloc, int size);
void* mem_realloc(struct alloc* alloc, void* ptr, int size);
void mem_free(struct alloc* alloc, void* ptr);
#ifdef _DEBUG
/* Check allocator integrity */
void mem_check(struct alloc* alloc);
#endif

#define vec_init(alloc, vec, cnt, cap) do { (vec) = 0; (cnt) = 0; (cap) = 0; } while(0)
#define vec_add(alloc, vec, cnt, cap) do { \
		if ((cnt) == (cap)) { \
			int new_cap = (cap) == 0 ? 16 : (cap) * 2; \
			int elem_size = sizeof((vec)[0]); \
			(vec) = mem_realloc((alloc), (vec), new_cap * elem_size); \
			(cap) = new_cap; \
		} \
		++(cnt); \
	} while(0)

#endif
