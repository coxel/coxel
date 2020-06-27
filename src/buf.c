#include "buf.h"
#include "gc.h"
#include <string.h>

struct bufobj* buf_new(struct cpu* cpu, int size) {
	struct bufobj* buf = (struct bufobj*)gc_alloc(cpu, t_buf, sizeof(struct bufobj) + size);
	buf->len = size;
	memset(buf->data, 0, size);
	return buf;
}

void buf_destroy(struct cpu* cpu, struct bufobj* buf) {
	mem_free(cpu->alloc, buf);
}

struct value buf_get(struct cpu* cpu, struct bufobj* buf, number index) {
	int idx = num_uint(index);
	if (idx >= buf->len)
		return value_undef(cpu);
	else
		return value_num(cpu, num_kuint(buf->data[idx]));
}

void buf_set(struct cpu* cpu, struct bufobj* buf, number index, struct value value) {
	if (value.type != t_num)
		return;
	int idx = num_uint(index);
	if (idx >= buf->len)
		return;
	int byte = num_uint(value.num);
	buf->data[idx] = (uint8_t)byte;
}

struct value buf_fget(struct cpu* cpu, struct bufobj* buf, struct strobj* key) {
	if (key == LIT(length))
		return value_num(cpu, num_uint(buf->len));
	else
		return value_undef(cpu);
}
