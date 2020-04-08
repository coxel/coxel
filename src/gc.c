#include "alloc.h"
#include "arr.h"
#include "buf.h"
#include "gc.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

struct obj* gc_alloc(struct cpu* cpu, enum type type, uint32_t size) {
	struct obj* obj = (struct obj*)mem_malloc(cpu->alloc, size);
	obj->gcnext = cpu->gchead;
	obj->type = type;
	cpu->gchead = obj;
	return obj;
}

#define gc_mark(obj)	((obj)->gcnext = (void*)((uintptr_t)(obj)->gcnext | 1))
#define gc_next(obj)	((void*)((uintptr_t)(obj)->gcnext & ~1))
#define gc_clear(obj)	((obj)->gcnext = gc_next(obj))
#define gc_getmark(obj)	((uintptr_t)(obj)->gcnext & 1)

static void gc_traverse_val(struct cpu* cpu, struct value* value);

static void gc_traverse_arr(struct cpu* cpu, struct arrobj* arr) {
	for (int i = 0; i < arr->len; i++)
		gc_traverse_val(cpu, &arr->data[i]);
}

static void gc_traverse_tab(struct cpu* cpu, struct tabobj* tab) {
	for (int i = 0; i < tab->bucket_cnt; i++) {
		for (uint16_t p = tab->bucket[i]; p != TAB_NULL;) {
			struct tabent* ent = &tab->entry[p];
			gc_mark(ent->key);
			gc_traverse_val(cpu, &ent->value);
			p = ent->next;
		}
	}
}

static void gc_traverse_func(struct cpu* cpu, struct funcobj* func) {
	int upval_cnt = func->code->upval_cnt;
	for (int i = 0; i < upval_cnt; i++) {
		struct upval* uval = func->upval[i];
		if (!gc_getmark(uval)) {
			gc_mark(uval);
			gc_traverse_val(cpu, uval->val);
		}
	}
}

#define check_mark(obj)	do { if (gc_getmark(obj)) return; gc_mark(obj); } while(0)
static void gc_traverse_val(struct cpu* cpu, struct value* value) {
	switch (value->type) {
	case t_str: gc_mark(value->str); return;
	case t_buf: gc_mark(value->buf); return;
	case t_arr: check_mark(value->arr); gc_traverse_arr(cpu, value->arr); return;
	case t_tab: check_mark(value->tab); gc_traverse_tab(cpu, value->tab); return;
	case t_func: check_mark(value->func); gc_traverse_func(cpu, value->func); return;
	default: return;
	}
}

void gc_free(struct cpu* cpu, struct obj* obj) {
	switch (obj->type) {
	case t_str: str_destroy(cpu, (struct strobj*)obj); return;
	case t_buf: buf_destroy(cpu, (struct bufobj*)obj); return;
	case t_arr: arr_destroy(cpu, (struct arrobj*)obj); return;
	case t_tab: tab_destroy(cpu, (struct tabobj*)obj); return;
	case t_func: func_destroy(cpu, (struct funcobj*)obj); return;
	case t_upval: upval_destroy(cpu, (struct upval*)obj); return;
	default: platform_error("Internal gc error.");
	}
}

void gc_collect(struct cpu* cpu) {
	/* Mark all reachable objects */
	/* Traverse root sets */
	gc_traverse_tab(cpu, cpu->globals);

	/* Sweep unreachable objects */
	struct obj** prev = &cpu->gchead;
	for (struct obj* cur = *prev; cur;) {
		if (gc_getmark(cur)) {
			/* Clear mark */
			*prev = cur;
			prev = &cur->gcnext;
			cur = gc_next(cur);
		}
		else {
			struct obj* next = cur->gcnext;
			gc_free(cpu, cur);
			cur = next;
		}
	}
	*prev = NULL;
}
