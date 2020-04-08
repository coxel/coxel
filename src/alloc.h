#ifndef _ALLOC_H
#define _ALLOC_H

#include "config.h"

#include <stdint.h>

struct alloc* mem_new(uint32_t size);
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
