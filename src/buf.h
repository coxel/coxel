#ifndef _BUF_H
#define _BUF_H

#include "cpu.h"

struct bufobj* buf_new(struct cpu* cpu, int size);
struct bufobj* buf_new_copydata(struct cpu* cpu, const void* data, int size);
void buf_destroy(struct cpu* cpu, struct bufobj* buf);
struct value buf_get(struct cpu* cpu, struct bufobj* buf, number index);
void buf_set(struct cpu* cpu, struct bufobj* buf, number index, struct value value);
struct value buf_fget(struct cpu* cpu, struct bufobj* buf, struct strobj* key);

#endif
