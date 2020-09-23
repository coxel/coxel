#include "alloc.h"
#include "arr.h"
#include "buf.h"
#include "gc.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

struct obj* gc_alloc(struct cpu* cpu, enum type type, uint32_t size) {
	struct obj* obj = (struct obj*)mem_alloc(&cpu->alloc, size);
	obj->gcnext = cpu->gchead;
	obj->type = type;
	cpu->gchead = writeptr(obj);
	return obj;
}

#define gc_mark(obj)	((obj)->gcnext = (ptr_t)((uintptr_t)obj->gcnext | 1))
#define gc_next(obj)	((void*)(((uintptr_t)readptr_nullable((uintptr_t)obj->gcnext & ~1))))
#define gc_clear(obj)	((obj)->gcnext = (ptr_t)((uintptr_t)(obj)->gcnext & ~1))
#define gc_getmark(obj)	((uintptr_t)obj->gcnext & 1)

static void gc_traverse_val(struct cpu* cpu, value_t value);

static void gc_traverse_arr(struct cpu* cpu, struct arrobj* arr) {
	for (int i = 0; i < arr->len; i++)
		gc_traverse_val(cpu, ((value_t*)readptr(arr->data))[i]);
}

static void gc_traverse_tab(struct cpu* cpu, struct tabobj* tab) {
	for (int i = 0; i < tab->bucket_cnt; i++) {
		for (uint16_t p = ((uint16_t*)readptr(tab->bucket))[i]; p != TAB_NULL;) {
			struct tabent* ent = &((struct tabent*)readptr(tab->entry))[p];
			struct strobj* key = (struct strobj*)readptr(ent->key);
			gc_mark(key);
			gc_traverse_val(cpu, ent->value);
			p = ent->next;
		}
	}
}

static void gc_traverse_func(struct cpu* cpu, struct funcobj* func) {
	int upval_cnt = ((struct code*)readptr(func->code))->upval_cnt;
	for (int i = 0; i < upval_cnt; i++) {
		struct upval* uval = readptr(func->upval[i]);
		if (!gc_getmark(uval)) {
			gc_mark(uval);
			gc_traverse_val(cpu, *(value_t*)readptr(uval->val));
		}
	}
}

#define check_mark(obj)	do { if (gc_getmark(obj)) return; gc_mark(obj); } while(0)
static void gc_traverse_val(struct cpu* cpu, value_t value) {
	switch (value_get_type(value)) {
	case t_str: {
		struct strobj* str = (struct strobj*)value_get_object(value);
		gc_mark(str);
		return;
	}
	case t_striter: {
		struct striterobj* striter = (struct striterobj*)value_get_object(value);
		check_mark(striter);
		struct strobj* str = (struct strobj*)readptr(striter->str);
		gc_mark(str);
		return;
	}
	case t_buf: {
		struct bufobj* buf = (struct bufobj*)value_get_object(value);
		gc_mark(buf);
		return;
	}
	case t_arr: {
		struct arrobj* arr = (struct arrobj*)value_get_object(value);
		check_mark(arr);
		gc_traverse_arr(cpu, arr);
		return;
	}
	case t_arriter: {
		struct arriterobj* arriter = (struct arriterobj*)value_get_object(value);
		check_mark(arriter);
		struct arrobj* arr = (struct arrobj*)readptr(arriter->arr);
		check_mark(arr);
		gc_traverse_arr(cpu, arr);
		return;
	}
	case t_tab: {
		struct tabobj* tab = (struct tabobj*)value_get_object(value);
		check_mark(tab);
		gc_traverse_tab(cpu, tab);
		return;
	}
	case t_func: {
		struct funcobj* func = (struct funcobj*)value_get_object(value);
		check_mark(func);
		gc_traverse_func(cpu, func);
		return;
	}
	case t_assetmap: {
		struct assetmapobj* assetmap = (struct assetmapobj*)value_get_object(value);
		check_mark(assetmap);
		struct bufobj* buf = (struct bufobj*)readptr(assetmap->buf);
		gc_mark(buf);
		return;
	}
	default: return;
	}
}

void gc_free(struct cpu* cpu, struct obj* obj) {
	switch (obj->type) {
	case t_str: str_destroy(cpu, (struct strobj*)obj); return;
	case t_striter: mem_dealloc(&cpu->alloc, obj); return;
	case t_buf: buf_destroy(cpu, (struct bufobj*)obj); return;
	case t_arr: arr_destroy(cpu, (struct arrobj*)obj); return;
	case t_arriter: mem_dealloc(&cpu->alloc, obj); return;
	case t_tab: tab_destroy(cpu, (struct tabobj*)obj); return;
	case t_func: func_destroy(cpu, (struct funcobj*)obj); return;
	case t_upval: upval_destroy(cpu, (struct upval*)obj); return;
	default: platform_error("Internal gc error.");
	}
}

void gc_collect(struct cpu* cpu) {
	/* Mark all reachable objects */
	/* Traverse root sets */
	gc_traverse_tab(cpu, (struct tabobj*)readptr(cpu->globals));

	/* Sweep unreachable objects */
	ptr_t* prev = &cpu->gchead;
	for (struct obj* cur = readptr_nullable(*prev); cur;) {
		if (gc_getmark(cur)) {
			/* Clear mark */
			*prev = writeptr(cur);
			prev = &cur->gcnext;
			cur = gc_next(cur);
		}
		else {
			struct obj* next = readptr_nullable(cur->gcnext);
			gc_free(cpu, cur);
			cur = next;
		}
	}
	*prev = writeptr_nullable(NULL);
}
