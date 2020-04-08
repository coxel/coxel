#ifndef _RAND_H
#define _RAND_H

#include "cpu.h"

uint32_t fmix32(uint32_t h);
void rand_seed(struct cpu* cpu, uint32_t seed);
uint32_t rand_next(struct cpu* cpu);
uint32_t rand_int(struct cpu* cpu, uint32_t max);

#endif
