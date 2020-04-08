#include "arr.h"
#include "gc.h"

struct arrobj* arr_new(struct cpu* cpu) {
	struct arrobj* arr = (struct arrobj*)gc_alloc(cpu, t_arr, sizeof(struct arrobj));
	arr->len = 0;
	arr->cap = 0;
	arr->data = NULL;
	return arr;
}

void arr_destroy(struct cpu* cpu, struct arrobj* arr) {
	mem_free(cpu->alloc, arr->data);
	mem_free(cpu->alloc, arr);
}

struct value arr_get(struct cpu* cpu, struct arrobj* arr, number index) {
	int idx = num_uint(index);
	if (idx >= arr->len)
		return value_undef();
	else
		return arr->data[idx];
}

void arr_set(struct cpu* cpu, struct arrobj* arr, number index, struct value value) {
	int idx = num_uint(index);
	if (idx >= arr->len)
		runtime_error(cpu, "Index out of bound.");
	else
		arr->data[idx] = value;
}

void arr_push(struct cpu* cpu, struct arrobj* arr, struct value value) {
	vec_add(cpu->alloc, arr->data, arr->len, arr->cap);
	arr->data[arr->len - 1] = value;
}

struct value arr_pop(struct cpu* cpu, struct arrobj* arr) {
	if (arr->len == 0)
		return value_undef();
	else
		return arr->data[--arr->len];
}

static void libarr_push(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	arr_push(cpu, arr, ARG(0));
}

static void libarr_pop(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	RET = arr_pop(cpu, arr);
}

struct value arr_fget(struct cpu* cpu, struct arrobj* arr, struct strobj* key) {
	if (key == cpu->_lit_pop)
		return value_cfunc(libarr_pop);
	else if (key == cpu->_lit_push)
		return value_cfunc(libarr_push);
	else if (key == cpu->_lit_length)
		return value_num(num_kuint(arr->len));
	else
		return value_undef();
}
