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

struct bufobj* buf_new_special(struct cpu* cpu, enum special_buf_id id) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj));
	buf->len = id;
	return buf;
}

void buf_destroy(struct cpu* cpu, struct bufobj* buf) {
	mem_dealloc(&cpu->alloc, buf);
}

static FORCEINLINE void buf_getdata(struct cpu* cpu, struct bufobj* buf, uint8_t** data, uint32_t* len) {
	if (buf->len == SBUF_VMEM) {
		*data = console_getgfx()->screen;
		*len = WIDTH * HEIGHT / 2;
	}
	else {
		*data = buf->data;
		*len = buf->len;
	}
}

struct value buf_get(struct cpu* cpu, struct bufobj* buf, number index) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	int idx = num_uint(index);
	if (unlikely(idx >= len))
		return value_undef(cpu);
	else
		return value_num(cpu, num_kuint(data[idx]));
}

void buf_set(struct cpu* cpu, struct bufobj* buf, number index, struct value value) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	if (unlikely(value.type != t_num))
		return;
	int idx = num_uint(index);
	if (unlikely(idx >= len))
		return;
	int byte = num_uint(value.num);
	data[idx] = (uint8_t)byte;
}

struct value buf_fget(struct cpu* cpu, struct bufobj* buf, struct strobj* key) {
	uint8_t* data;
	uint32_t len;
	buf_getdata(cpu, buf, &data, &len);
	if (key == LIT(length))
		return value_num(cpu, num_kuint(len));
	else
		return value_undef(cpu);
}
