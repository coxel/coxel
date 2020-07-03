#include "arr.h"
#include "gc.h"

struct arrobj* arr_new(struct cpu* cpu) {
	struct arrobj* arr = (struct arrobj*)gc_alloc(cpu, t_arr, sizeof(struct arrobj));
	arr->len = 0;
	arr->cap = 0;
	arr->data = writeptr(NULL);
	return arr;
}

void arr_destroy(struct cpu* cpu, struct arrobj* arr) {
	mem_free(&cpu->alloc, readptr(arr->data));
	mem_free(&cpu->alloc, arr);
}

struct value arr_get(struct cpu* cpu, struct arrobj* arr, number index) {
	int idx = num_uint(index);
	if (idx >= arr->len)
		return value_undef(cpu);
	else
		return ((struct value*)readptr(arr->data))[idx];
}

void arr_set(struct cpu* cpu, struct arrobj* arr, number index, struct value value) {
	int idx = num_uint(index);
	if (idx >= arr->len)
		runtime_error(cpu, "Index out of bound.");
	else
		((struct value*)readptr(arr->data))[idx] = value;
}

void arr_push(struct cpu* cpu, struct arrobj* arr, struct value value) {
	struct value* data = (struct value*)readptr(arr->data);
	vec_add(&cpu->alloc, data, arr->len, arr->cap);
	arr->data = writeptr(data);
	data[arr->len - 1] = value;
}

struct value arr_pop(struct cpu* cpu, struct arrobj* arr) {
	if (arr->len == 0)
		return value_undef(cpu);
	else
		return ((struct value*)readptr(arr->data))[--arr->len];
}

struct value libarr_push(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	arr_push(cpu, arr, ARG(0));
	return value_undef(cpu);
}

struct value libarr_pop(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	return arr_pop(cpu, arr);
}

static inline int normalize_index(int index, int len) {
	if (index < 0)
		index += len;
	if (index < 0)
		return 0;
	else if (index >= len)
		return len;
	else
		return index;
}

struct value libarr_slice(struct cpu* cpu, int sp, int nargs) {
	if (nargs > 2)
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	int start = 0;
	int end = arr->len;
	if (nargs >= 1) {
		start = num_int(to_number(cpu, ARG(0)));
		start = normalize_index(start, arr->len);
		if (nargs == 2) {
			end = num_int(to_number(cpu, ARG(1)));
			end = normalize_index(end, arr->len);
		}
	}
	struct value* values = (struct value*)readptr(arr->data);
	struct arrobj* narr = arr_new(cpu);
	for (int i = start; i < end; i++)
		arr_push(cpu, narr, values[i]);
	return value_arr(cpu, narr);
}

struct value arr_fget(struct cpu* cpu, struct arrobj* arr, struct strobj* key) {
	if (key == LIT(pop))
		return value_cfunc(cpu, cf_libarr_pop);
	else if (key == LIT(push))
		return value_cfunc(cpu, cf_libarr_push);
	else if (key == LIT(length))
		return value_num(cpu, num_kuint(arr->len));
	else if (key == LIT(slice))
		return value_cfunc(cpu, cf_libarr_slice);
	else
		return value_undef(cpu);
}
