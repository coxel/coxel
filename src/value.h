#ifndef _VALUE_H
#define _VALUE_H

#include "config.h"

#include <stdint.h>

struct cpu;
#ifdef DEBUG_NULLABLE_PTR
static FORCEINLINE void* forcereadptr_impl(struct cpu* cpu, uint32_t aptr) {
	if (aptr == 0)
		__debugbreak();
	return (void*)((uint8_t*)cpu + (aptr));
}
static FORCEINLINE uint32_t forcewriteptr_impl(struct cpu* cpu, void* aptr) {
	if (aptr == NULL)
		__debugbreak();
	return (uint32_t)((uint8_t*)(aptr)-(uint8_t*)cpu);
}
#define forcereadptr(aptr)		forcereadptr_impl(cpu, aptr)
#define forcewriteptr(aptr)		forcewriteptr_impl(cpu, aptr)
#else
#define forcereadptr(aptr)	((void*)((uint8_t*)cpu + (aptr)))
#define forcewriteptr(aptr)	((uint32_t)((uint8_t*)(aptr) - (uint8_t*)cpu))
#endif

static FORCEINLINE void* forcereadptr_nullable_impl(struct cpu* cpu, uint32_t aptr) {
	return aptr == 0 ? NULL : forcereadptr(aptr);
}

static FORCEINLINE uint32_t forcewriteptr_nullable_impl(struct cpu* cpu, void* aptr) {
	return aptr == NULL ? 0 : forcewriteptr(aptr);
}
#define forcereadptr_nullable(aptr)		forcereadptr_nullable_impl(cpu, aptr)
#define forcewriteptr_nullable(aptr)	forcewriteptr_nullable_impl(cpu, aptr)

#ifdef RELATIVE_ADDRESSING
typedef uint32_t ptr_t;
#define readptr(aptr)		forcereadptr(aptr)
#define writeptr(aptr)		forcewriteptr(aptr)
#define readptr_nullable(aptr)		forcereadptr_nullable(aptr)
#define writeptr_nullable(aptr)		forcewriteptr_nullable(aptr)
#define ptr(type)			ptr_t
#else
typedef void* ptr_t;
#define readptr(aptr)		(aptr)
#define writeptr(aptr)		(aptr)
#define readptr_nullable(aptr)		(aptr)
#define writeptr_nullable(aptr)		(aptr)
#define ptr(type)			type*
#endif
#define ptr_nullable(type)	ptr(type)

/* Object header format LSB ... MSB
 * sstttttt | o......o
 * ss: GC mark (white/black/gray)
 * tttttt: Object type 
 * o......o: 24-bit object offset pointer (gcnext)
 */

#define gm_mark0				0
#define gm_mark1				1
#define gm_gray					2
#define gm_white				(cpu->gcwhite)
#define gm_black				(1 - cpu->gcwhite)
#define OBJ_HEADER				uint32_t obj_header
#define CONTAINER_OBJ_HEADER	OBJ_HEADER; ptr_nullable(struct containerobj) graylist

#define obj_get_mark(obj)		((obj)->obj_header & 3)
#define obj_get_type(obj)		(((obj)->obj_header >> 2) & 63)
#define obj_get_gcnext(obj)		forcereadptr_nullable((obj)->obj_header >> 8)
#define obj_mark(obj, mark)		do { \
		uint32_t* _header = (uint32_t*)(obj); \
		*_header = (*_header & ~3) | (mark); \
	} while (0)
#define make_obj_header(mark, type, next)	(mark + ((type) << 2) + (forcewriteptr_nullable(next) << 8))
#define obj_header_get_gcnext(header)		forcereadptr_nullable((header) >> 8)
#define obj_header_set_gcnext(header, next)	(((header) & 255) + (forcewriteptr_nullable(next) << 8))

/* Value format LSB ... MSB
 * 0 f.......f  | i..i : Q16.15 fixed point number
 * 10tttttt | 00000000 : Undefined and null
 * 10tttttt | b 0....0 : Boolean
 * 10tttttt | ss | p.p : Call info: 8-bit stack size, 16-bit pc
 * 11tttttt | o......o : 24-bit object offset pointer
 */
typedef uint32_t value_t;

enum type {
	t_num = 0,
	t_undef = 1,
	t_null = 5,
	t_bool = 9,
	t_cfunc = 13,
	t_callinfo = 17,
	t_str = 19,
	t_striter = 23,
	t_buf = 27,
	t_arr = 31,
	t_arriter = 35,
	t_tab = 39,
	t_func = 43,
	t_upval = 47,
	t_assetmap = 51,
};

#define value_is_num(val)			(((val) & 1) == 0)
#define value_is_object(val)		(((val) & 3) == 3)
#define value_get_num(val)			((number)(val))
#define value_get_type(val)			((val) & 0xFF)
#define value_get_bool(val)			((val) >> 8)
#define value_get_object(val)		(forcereadptr((val) >> 8))
#define value_get_cfunc(val)		((val) >> 8)
#define value_get_ci_stacksize(val)	(((val) >> 8) & 0xFF)
#define value_get_ci_pc(val)		((val) >> 16)

#define value_type_payload(t, p)	((value_t)((t) + ((p) << 8)))
#define value_type_object(t, o)		(value_type_payload((t), forcewriteptr(o)))
#define value_num(num)				((value_t)(num))
#define value_undef()				(value_type_payload(t_undef, 0))
#define value_null()				(value_type_payload(t_null, 0))
#define value_bool(b)				(value_type_payload(t_bool, b))
#define value_cfunc(cfunc)			(value_type_payload(t_cfunc, cfunc))
#define value_callinfo(ss, pc)		(value_type_payload(t_callinfo, ((pc) << 8) + (ss)))
#define value_str(str)				(value_type_object(t_str, str))
#define value_striter(striter)		(value_type_object(t_striter, striter))
#define value_buf(buf)				(value_type_object(t_buf, buf))
#define value_arr(arr)				(value_type_object(t_arr, arr))
#define value_arriter(arriter)		(value_type_object(t_arriter, arriter))
#define value_tab(tab)				(value_type_object(t_tab, tab))
#define value_func(func)			(value_type_object(t_func, func))
#define value_assetmap(map)			(value_type_object(t_assetmap, map))

struct obj {
	OBJ_HEADER;
};

struct containerobj {
	CONTAINER_OBJ_HEADER;
};

struct strobj {
	OBJ_HEADER;
	ptr(struct strobj) next;
	uint32_t hash;
	uint32_t len;
	char data[];
};

struct striterobj {
	OBJ_HEADER;
	ptr(struct strobj) str;
	uint32_t i;
	uint32_t end;
};

struct bufobj {
	OBJ_HEADER;
	uint32_t len;
	uint8_t data[];
};

struct arrobj {
	CONTAINER_OBJ_HEADER;
	uint32_t len, cap;
	ptr_nullable(value_t) data;
};

struct arriterobj {
	OBJ_HEADER;
	ptr(struct arrobj) arr;
	uint32_t i;
	uint32_t end;
};

#define TAB_NULL		((uint16_t)-1)

struct tabent {
	ptr(struct strobj) key;
	value_t value;
	uint16_t next;
};

struct tabobj {
	CONTAINER_OBJ_HEADER;
	int entry_cnt;
	ptr_nullable(struct tabent) entry;
	uint16_t freelist;
	int bucket_cnt;
	ptr_nullable(uint16_t) bucket;
};

struct assetmapobj {
	OBJ_HEADER;
	int width, height;
	ptr(struct bufobj) buf;
};

#endif
