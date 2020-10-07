#include "arr.h"
#include "gc.h"

struct arrobj* arr_new(struct cpu* cpu) {
	struct arrobj* arr = (struct arrobj*)gc_alloc(cpu, t_arr, sizeof(struct arrobj));
	arr->len = 0;
	arr->cap = 0;
	arr->data = writeptr_nullable(NULL);
	return arr;
}

void arr_destroy(struct cpu* cpu, struct arrobj* arr) {
	mem_dealloc(&cpu->alloc, readptr_nullable(arr->data));
	mem_dealloc(&cpu->alloc, arr);
}

value_t arr_get(struct cpu* cpu, struct arrobj* arr, number index) {
	int idx = num_uint(index);
	if (unlikely(idx >= arr->len))
		return value_undef();
	else
		return ((value_t*)readptr(arr->data))[idx];
}

void arr_set(struct cpu* cpu, struct arrobj* arr, number index, value_t value) {
	int idx = num_uint(index);
	if (unlikely(idx >= arr->len))
		runtime_error(cpu, "Index out of bound.");
	else {
		((value_t*)readptr(arr->data))[idx] = value;
		gc_barrier(cpu, arr, value);
	}
}

void arr_push(struct cpu* cpu, struct arrobj* arr, value_t value) {
	value_t* data = (value_t*)readptr_nullable(arr->data);
	vec_add(&cpu->alloc, data, arr->len, arr->cap);
	arr->data = writeptr(data);
	data[arr->len - 1] = value;
	gc_barrier(cpu, arr, value);
}

value_t arr_pop(struct cpu* cpu, struct arrobj* arr) {
	if (unlikely(arr->len == 0))
		return value_undef();
	else
		return ((value_t*)readptr(arr->data))[--arr->len];
}

value_t libarr_push(struct cpu* cpu, int sp, int nargs) {
	if (unlikely(nargs != 1))
		argument_error(cpu);
	struct arrobj* arr = to_arr(cpu, THIS);
	arr_push(cpu, arr, ARG(0));
	cpu->cycles -= CYCLES_ALLOC;
	return value_undef();
}

value_t libarr_pop(struct cpu* cpu, int sp, int nargs) {
	if (unlikely(nargs != 0))
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

value_t libarr_slice(struct cpu* cpu, int sp, int nargs) {
	if (unlikely(nargs > 2))
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
	value_t* values = (value_t*)readptr_nullable(arr->data);
	struct arrobj* narr = arr_new(cpu);
	// TODO: Optimization
	for (int i = start; i < end; i++)
		arr_push(cpu, narr, values[i]);
	cpu->cycles -= CYCLES_ALLOC + CYCLES_VALUES(end - start);
	return value_arr(narr);
}

value_t arr_fget(struct cpu* cpu, struct arrobj* arr, struct strobj* key) {
	if (key == LIT(pop))
		return value_cfunc(cf_libarr_pop);
	else if (key == LIT(push))
		return value_cfunc(cf_libarr_push);
	else if (key == LIT(length))
		return value_num(num_kuint(arr->len));
	else if (key == LIT(slice))
		return value_cfunc(cf_libarr_slice);
	else
		return value_undef();
}
