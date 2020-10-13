#ifndef _BUF_H
#define _BUF_H

#include "cpu.h"

enum special_buf_id {
	SBUF_VMEM = -1,
	SBUF_BACKVMEM = -2,
	SBUF_OVERLAYVMEM = -3,
};

struct bufobj* buf_new(struct cpu* cpu, int size);
struct bufobj* buf_new_copydata(struct cpu* cpu, const void* data, int size);
struct bufobj* buf_new_vmem(struct cpu* cpu, int pid);
struct bufobj* buf_new_backvmem(struct cpu* cpu, int pid);
struct bufobj* buf_new_overlayvmem(struct cpu* cpu);
void buf_destroy(struct cpu* cpu, struct bufobj* buf);
value_t buf_get(struct cpu* cpu, struct bufobj* buf, number index);
void buf_set(struct cpu* cpu, struct bufobj* buf, number index, value_t value);
value_t buf_fget(struct cpu* cpu, struct bufobj* buf, struct strobj* key);

#endif
