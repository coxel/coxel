#ifndef _GC_H
#define _GC_H

#include "cpu.h"

#define gc_barrier(cpu, container, value) do { \
		value_t _value = (value); \
		struct containerobj* _container = (struct containerobj*)(container); \
		if (value_is_object(_value) && obj_get_mark(_container) == gm_black) { \
			struct obj* _obj = value_get_object(_value); \
			if (obj_get_mark(_obj) == gm_white) \
				gc_mark_value(cpu, value); \
		} \
	} while (0)
/* Special case for tab_set(key, value) */
#define gc_barrier_kv(cpu, container, key, value) do { \
		struct strobj* _key = (key); \
		value_t _value = (value); \
		struct containerobj* _container = (struct containerobj*)(container); \
		if (obj_get_mark(_container) == gm_black) { \
			obj_mark(_key, gm_black); \
			if (value_is_object(_value)) { \
				struct obj* _obj = value_get_object(_value); \
				if (obj_get_mark(_obj) == gm_white) \
					gc_mark_value(cpu, _value); \
			} \
		} \
	} while (0)

struct obj* gc_alloc(struct cpu* cpu, enum type type, uint32_t size);
void gc_mark_value(struct cpu* cpu, value_t value);
void gc_collect(struct cpu* cpu);

#endif
