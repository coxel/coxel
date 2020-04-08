#ifndef _ARRAY_H
#define _ARRAY_H

#include "cpu.h"

struct arrobj* arr_new(struct cpu* cpu);
void arr_destroy(struct cpu* cpu, struct arrobj* arr);
struct value arr_get(struct cpu* cpu, struct arrobj* arr, number index);
void arr_set(struct cpu* cpu, struct arrobj* arr, number index, struct value value);
void arr_push(struct cpu* cpu, struct arrobj* arr, struct value value);
struct value arr_pop(struct cpu* cpu, struct arrobj* arr);
struct value arr_fget(struct cpu* cpu, struct arrobj* arr, struct strobj* key);

#endif
