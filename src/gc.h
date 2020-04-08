#ifndef _GC_H
#define _GC_H

#include "cpu.h"

struct obj* gc_alloc(struct cpu* cpu, enum type type, uint32_t size);
void gc_collect(struct cpu* cpu);

#endif
