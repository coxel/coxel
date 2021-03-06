#include "alloc.h"
#include "platform.h"

#include <string.h>

#ifdef RELATIVE_ADDRESSING
#define readptr(aptr)			((void*)((uint8_t*)alloc + (aptr)))
#define writeptr(ptr, aptr)		((ptr) = (ptr_t)((uint8_t*)(aptr) - (uint8_t*)alloc))
#else
#define readptr(aptr)			((void*)(aptr))
#define writeptr(ptr, aptr)		((ptr) = (ptr_t)(aptr))
#endif

#define mem2chunk(mem)		((struct chunk*)((uint8_t*)mem - CHUNK_OVERHEAD))
#define chunk2mem(chunk)	((void*)((uint8_t*)chunk + CHUNK_OVERHEAD))
#define pad_request(size)	(((size) + CHUNK_OVERHEAD + CHUNK_GRANULARITY - 1) & -CHUNK_GRANULARITY)
#define request2size(size)	((size) < (MIN_CHUNK_SIZE - CHUNK_OVERHEAD) ? MIN_CHUNK_SIZE : pad_request(size))

#define smallidx(size)		((size) / CHUNK_GRANULARITY)
#define smallsize(idx)		((idx) * CHUNK_GRANULARITY)
#define PUSE_BIT			1 /* Previous chunk is in use */
#define CUSE_BIT			2 /* Current chunk is in use */
#define FLAG_MASK			(PUSE_BIT | CUSE_BIT)
#define chunksize(chunk)	((chunk)->size & ~FLAG_MASK)
#define nextchunk(c)		((struct chunk*)((uint8_t*)(c) + chunksize(c)))

static FORCEINLINE void set_inuse_pinuse(struct alloc* alloc, struct chunk* chunk) {
	struct chunk* next = nextchunk(chunk);
	next->size |= PUSE_BIT;
	chunk->size |= PUSE_BIT | CUSE_BIT;
}

static FORCEINLINE void set_footer(struct alloc* alloc, struct chunk* chunk) {
	uint32_t size = chunksize(chunk);
	*(uint32_t*)((uint8_t*)chunk + size - 4) = size;
}

static FORCEINLINE void set_size(struct alloc* alloc, struct chunk* chunk, uint32_t size) {
	check((int32_t)size >= 0);
	chunk->size = size | (chunk->size & FLAG_MASK);
}

static FORCEINLINE void set_size_inuse(struct alloc* alloc, struct chunk* chunk, uint32_t size) {
	check((int32_t)size >= 0);
	chunk->size = size | PUSE_BIT | CUSE_BIT;
}

static FORCEINLINE void set_size_free(struct alloc* alloc, struct chunk* chunk, uint32_t size) {
	check((int32_t)size >= 0);
	chunk->size = size | PUSE_BIT;
	set_footer(alloc, chunk);
}

static FORCEINLINE void list_unlink_chunk(struct alloc* alloc, struct binhdr* hdr) {
	struct binhdr* prev = (struct binhdr*)readptr(hdr->prev);
	struct binhdr* next = (struct binhdr*)readptr(hdr->next);
	writeptr(prev->next, next);
	writeptr(next->prev, prev);
}

static struct binhdr* unlink_small_head(struct alloc* alloc, int idx) {
	struct binhdr* bin = &alloc->smallbins[idx];
	struct binhdr* head = (struct binhdr*)readptr(bin->next);
	list_unlink_chunk(alloc, head);
	if (readptr(bin->next) == bin)
		alloc->smallmap &= ~(1 << idx);
	return head;
}

static void unlink_small_chunk(struct alloc* alloc, struct chunk* chunk) {
	int idx = smallidx(chunksize(chunk));
	list_unlink_chunk(alloc, &chunk->b);
	struct binhdr* bin = &alloc->smallbins[idx];
	if (readptr(bin->next) == bin)
		alloc->smallmap &= ~(1 << idx);
}

static void unlink_large_chunk(struct alloc* alloc, struct chunk* chunk) {
	struct binhdr* hdr = &chunk->b;
	list_unlink_chunk(alloc, hdr);
}

static FORCEINLINE void unlink_chunk(struct alloc* alloc, struct chunk* chunk) {
	if (chunksize(chunk) <= SMALL_REQUEST)
		unlink_small_chunk(alloc, chunk);
	else
		unlink_large_chunk(alloc, chunk);
}

static FORCEINLINE void list_link_chunk(struct alloc* alloc, struct binhdr* bin, struct binhdr* hdr) {
	hdr->next = bin->next;
	writeptr(((struct binhdr*)readptr(hdr->next))->prev, hdr);
	writeptr(bin->next, hdr);
	writeptr(hdr->prev, bin);
}

static void insert_small_chunk(struct alloc* alloc, struct chunk* chunk) {
	uint32_t idx = smallidx(chunksize(chunk));
	alloc->smallmap |= 1 << idx;
	struct binhdr* bin = &alloc->smallbins[idx];
	struct binhdr* hdr = &chunk->b;
	list_link_chunk(alloc, bin, hdr);
}

static void insert_large_chunk(struct alloc* alloc, struct chunk* chunk) {
	struct binhdr* hdr = &chunk->b;
	list_link_chunk(alloc, &alloc->largebin, hdr);
}

static FORCEINLINE void insert_chunk(struct alloc* alloc, struct chunk* chunk) {
	if (chunksize(chunk) <= SMALL_REQUEST)
		insert_small_chunk(alloc, chunk);
	else
		insert_large_chunk(alloc, chunk);
}

static struct binhdr* large_alloc(struct alloc* alloc, uint32_t size) {
	struct binhdr* hdr = &alloc->largebin;
	struct binhdr* best = NULL;
	uint32_t bestsize = UINT32_MAX;
	for (struct binhdr* p = readptr(hdr->next); p != hdr; p = readptr(p->next)) {
		struct chunk* chunk = mem2chunk(p);
		uint32_t csize = chunksize(chunk);
		if (csize >= size && csize < bestsize) {
			bestsize = csize;
			best = p;
		}
	}
	if (best == NULL)
		return NULL;
	struct chunk* chunk = mem2chunk(best);
	unlink_large_chunk(alloc, chunk);
	uint32_t rsize = chunksize(chunk) - size;
	if (rsize < MIN_CHUNK_SIZE) {
		set_inuse_pinuse(alloc, chunk);
		alloc->used_memory += chunksize(chunk);
	}
	else {
		set_size_inuse(alloc, chunk, size);
		struct chunk* next = nextchunk(chunk);
		set_size_free(alloc, next, rsize);
		insert_chunk(alloc, next);
		alloc->used_memory += size;
	}
	return best;
}

struct alloc* mem_new(uint32_t size, uint32_t reserved_size) {
	reserved_size = (reserved_size + MIN_CHUNK_SIZE - 1) & ~(MIN_CHUNK_SIZE - 1);
	struct alloc* alloc = (struct alloc*)platform_malloc(size);
	if (alloc == NULL)
		return NULL;
	alloc->used_memory = reserved_size;
	alloc->size = size;
	alloc->reserved_size = reserved_size;
	for (int i = 0; i < SMALL_BINS; i++) {
		struct binhdr* hdr = &alloc->smallbins[i];
		writeptr(hdr->prev, hdr);
		writeptr(hdr->next, hdr);
	}
	alloc->smallmap = 0;
	struct binhdr* hdr = &alloc->largebin;
	writeptr(hdr->prev, hdr);
	writeptr(hdr->next, hdr);
	writeptr(alloc->top, (struct chunk*)((uint8_t*)alloc + reserved_size));
	check((uintptr_t)readptr(alloc->top) % MIN_CHUNK_SIZE == 0);
	alloc->topsize = size - reserved_size;
	return alloc;
}

void mem_destroy(struct alloc* alloc) {
	platform_free(alloc);
}

void* mem_alloc(struct alloc* alloc, int size) {
	size = request2size(size);
	if (size <= SMALL_REQUEST) {
		int idx = smallidx(size);
		bitmap_t bits = alloc->smallmap >> idx;
		if (bits & 3) { /* Fits in one chunk */
			idx += ~bits & 1;
			struct binhdr* mem = unlink_small_head(alloc, idx);
			struct chunk* chunk = mem2chunk(mem);
			set_inuse_pinuse(alloc, chunk);
			alloc->used_memory += smallsize(idx);
			return mem;
		}
		else {
			if (bits) { /* Use next non-empty small bin */
				bitmap_t highbits = bits << idx;
				int idx = leastbitidx(highbits);
				struct binhdr* mem = unlink_small_head(alloc, idx);
				/* Split chunk */
				struct chunk* chunk = mem2chunk(mem);
				uint32_t rsize = chunk->size - size;
				set_size_inuse(alloc, chunk, size);
				struct chunk* next = nextchunk(chunk);
				set_size_free(alloc, next, rsize);
				insert_small_chunk(alloc, next);
				alloc->used_memory += size;
				return mem;
			}
			else { /* Try allocate from large bin */
				struct binhdr* mem = large_alloc(alloc, size);
				if (mem)
					return mem;
			}
		}
	}
	else { /* Try allocate from large bin */
		struct binhdr* mem = large_alloc(alloc, size);
		if (mem)
			return mem;
	}
	/* Allocate from top */
	if (size <= alloc->topsize) {
		uint32_t rsize = alloc->topsize - size;
		if (rsize < MIN_CHUNK_SIZE) {
			size = alloc->topsize;
			rsize = 0;
		}
		struct chunk* chunk = (struct chunk*)readptr(alloc->top);
		set_size_inuse(alloc, chunk, size);
		writeptr(alloc->top, (uint8_t*)chunk + size);
		alloc->topsize = rsize;
		alloc->used_memory += size;
		return chunk2mem(chunk);
	}
	/* Fail */
	return NULL;
}

static void free_chunk_merge_next(struct alloc* alloc, struct chunk* chunk, uint32_t size, struct chunk* next) {
	if (next == readptr(alloc->top)) {
		writeptr(alloc->top, chunk);
		alloc->topsize += size;
	}
	else {
		if (next->size & CUSE_BIT)
			next->size &= ~PUSE_BIT;
		else {
			size += chunksize(next);
			unlink_chunk(alloc, next);
		}
		set_size_free(alloc, chunk, size);
		insert_chunk(alloc, chunk);
	}
}

void* mem_realloc(struct alloc* alloc, void* ptr, int request_size) {
	if (ptr == NULL)
		return mem_alloc(alloc, request_size);
	uint32_t size = request2size(request_size);
	struct chunk* chunk = mem2chunk(ptr);
	if (size <= chunksize(chunk)) {
		/* Already done */
		uint32_t rsize = chunksize(chunk) - size;
		if (rsize >= MIN_CHUNK_SIZE) {
			/* Shrink, split chunk */
			struct chunk* next = nextchunk(chunk);
			set_size(alloc, chunk, size);
			chunk = nextchunk(chunk);
			free_chunk_merge_next(alloc, chunk, rsize, next);
			alloc->used_memory -= rsize;
		}
		return ptr;
	}
	/* Try enlarge current chunk */
	struct chunk* next = nextchunk(chunk);
	if (next == readptr(alloc->top)) {
		uint32_t totsize = chunksize(chunk) + alloc->topsize;
		uint32_t rsize = totsize - size;
		if (rsize < MIN_CHUNK_SIZE) {
			size = totsize;
			rsize = 0;
		}
		alloc->used_memory += size - chunksize(chunk);
		set_size(alloc, chunk, size);
		writeptr(alloc->top, (uint8_t*)chunk + size);
		alloc->topsize = rsize;
		return ptr;
	}
	else if (!(next->size & CUSE_BIT)) {
		uint32_t totsize = chunksize(chunk) + chunksize(next);
		if (size <= totsize) {
			uint32_t rsize = totsize - size;
			unlink_chunk(alloc, next);
			if (rsize < MIN_CHUNK_SIZE) {
				alloc->used_memory += totsize - chunksize(chunk);
				set_size(alloc, chunk, totsize);
				next = nextchunk(chunk);
				if (next != readptr(alloc->top))
					next->size |= PUSE_BIT;
			}
			else {
				alloc->used_memory += size - chunksize(chunk);
				set_size(alloc, chunk, size);
				next = nextchunk(chunk);
				set_size_free(alloc, next, rsize);
				insert_chunk(alloc, next);
			}
			return ptr;
		}
	}
	/* Cannot realloc in place */
	void* new_ptr = mem_alloc(alloc, request_size);
	if (new_ptr == NULL)
		return NULL;
	memcpy(new_ptr, ptr, request_size);
	mem_dealloc(alloc, ptr);
	return new_ptr;
}

void mem_dealloc(struct alloc* alloc, void* ptr) {
	if (ptr == NULL)
		return;
	struct chunk* chunk = mem2chunk(ptr);
	struct chunk* next = nextchunk(chunk);
	uint32_t size = chunksize(chunk);
	alloc->used_memory -= size;
	/* Merge previous chunk */
	if (!(chunk->size & PUSE_BIT)) {
		uint32_t prevsize = *((uint32_t*)chunk - 1);
		struct chunk* prev = (struct chunk*)((uint8_t*)chunk - prevsize);
		unlink_chunk(alloc, prev);
		chunk = prev;
		size += prevsize;
	}
	/* Merge next chunk */
	free_chunk_merge_next(alloc, chunk, size, next);
}

#ifdef _DEBUG
static void check_free_chunk(struct alloc* alloc, struct chunk* chunk) {
	uint32_t size = chunksize(chunk);
	check(*(uint32_t*)((uint8_t*)chunk + size - 4) == size);
	struct chunk* next = nextchunk(chunk);
	check(chunk->size & PUSE_BIT);
	if (next != readptr(alloc->top)) {
		check(next->size & CUSE_BIT);
		check(!(next->size & PUSE_BIT));
	}
}

void mem_check(struct alloc* alloc) {
	int free_chunk_cnt = 0;
	for (int i = 0; i < SMALL_BINS; i++) {
		struct binhdr* hdr = &alloc->smallbins[i];
		int expected = readptr(hdr->next) != hdr;
		check(((alloc->smallmap >> i) & 1) == expected);
		for (struct binhdr* bin = readptr(hdr->next); bin != hdr; bin = readptr(bin->next)) {
			check_free_chunk(alloc, mem2chunk(bin));
			free_chunk_cnt++;
		}
	}
	struct binhdr* hdr = &alloc->largebin;
	for (struct binhdr* bin = readptr(hdr->next); bin != hdr; bin = readptr(bin->next)) {
		check_free_chunk(alloc, mem2chunk(bin));
		free_chunk_cnt++;
	}
	/* Check all chunks */
	int prev_inuse = 1;
	uint32_t used_memory = alloc->reserved_size;
	for (struct chunk* chunk = (struct chunk*)((uint8_t*)alloc + alloc->reserved_size); chunk != readptr(alloc->top); chunk = nextchunk(chunk)) {
		if (chunk->size & CUSE_BIT) {
			used_memory += chunksize(chunk);
			struct chunk* next = nextchunk(chunk);
			if (prev_inuse)
				check(chunk->size & PUSE_BIT);
			if (next != readptr(alloc->top))
				check(next->size & PUSE_BIT);
			prev_inuse = 1;
		}
		else {
			check_free_chunk(alloc, chunk);
			prev_inuse = 0;
			free_chunk_cnt--;
		}
	}
	check(free_chunk_cnt == 0);
	check(used_memory == alloc->used_memory);
}
#endif
