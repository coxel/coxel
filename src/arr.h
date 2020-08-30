#ifndef _ARRAY_H
#define _ARRAY_H

#include "cpu.h"

struct arrobj* arr_new(struct cpu* cpu);
void arr_destroy(struct cpu* cpu, struct arrobj* arr);
value_t arr_get(struct cpu* cpu, struct arrobj* arr, number index);
void arr_set(struct cpu* cpu, struct arrobj* arr, number index, value_t value);
void arr_push(struct cpu* cpu, struct arrobj* arr, value_t value);
value_t arr_pop(struct cpu* cpu, struct arrobj* arr);
value_t arr_fget(struct cpu* cpu, struct arrobj* arr, struct strobj* key);

#endif
