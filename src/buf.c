#include "buf.h"
#include "gc.h"
#include "platform.h"

#include <string.h>

struct bufobj* buf_new(struct cpu* cpu, int size) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj) + size);
	buf->len = size;
	memset(buf->data, 0, size);
	return buf;
}

struct bufobj* buf_new_copydata(struct cpu* cpu, const void* data, int size) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj) + size);
	buf->len = size;
	memcpy(buf->data, data, size);
	return buf;
}

struct bufobj* buf_new_vmem(struct cpu* cpu, int pid) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj) + sizeof(uint32_t));
	buf->len = SBUF_VMEM;
	*(uint32_t*)buf->data = pid;
	return buf;
}

struct bufobj* buf_new_backvmem(struct cpu* cpu, int pid) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj) + sizeof(uint32_t));
	buf->len = SBUF_BACKVMEM;
	*(uint32_t*)buf->data = pid;
	return buf;
}

struct bufobj* buf_new_overlayvmem(struct cpu* cpu) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj));
	buf->len = SBUF_OVERLAYVMEM;
	return buf;
}

void buf_destroy(struct cpu* cpu, struct bufobj* buf) {
	mem_dealloc(&cpu->alloc, buf);
}

static FORCEINLINE void buf_getdata(struct cpu* cpu, struct bufobj* buf, uint8_t** data, uint32_t* len) {
	switch (buf->len) {
	case SBUF_VMEM: {
		int pid = *(uint32_t*)buf->data;
		struct gfx* gfx = console_getgfx_pid(pid);
		*data = gfx->screen[gfx->bufno];
		*len = WIDTH * HEIGHT / 2;
		break;
	}
	case SBUF_BACKVMEM: {
		int pid = *(uint32_t*)buf->data;
		struct gfx* gfx = console_getgfx_pid(pid);
		*data = gfx->screen[!gfx->bufno];
		*len = WIDTH * HEIGHT / 2;
		break;
	}
	case SBUF_OVERLAYVMEM: {
		struct gfx* gfx = console_getgfx_overlay();
		*data = gfx->screen[gfx->bufno];
		*len = WIDTH * HEIGHT / 2;
		break;
	}
	default: {
		*data = buf->data;
		*len = buf->len;
	}
	}
}

value_t buf_get(struct cpu* cpu, struct bufobj* buf, number index) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	int idx = num_uint(index);
	if (unlikely(idx >= len))
		return value_undef();
	else
		return value_num(num_kuint(data[idx]));
}

void buf_set(struct cpu* cpu, struct bufobj* buf, number index, value_t value) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	if (!unlikely(value_is_num(value)))
		return;
	int idx = num_uint(index);
	if (unlikely(idx >= len))
		return;
	int byte = num_uint(value_get_num(value));
	if (unlikely(byte > 255))
		return;
	data[idx] = (uint8_t)byte;
}

value_t buf_fget(struct cpu* cpu, struct bufobj* buf, struct strobj* key) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	if (key == LIT(length))
		return value_num(num_kuint(len));
	else
		return value_undef();
}
