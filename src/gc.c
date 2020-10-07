#include "alloc.h"
#include "arr.h"
#include "buf.h"
#include "gc.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

struct obj* gc_alloc(struct cpu* cpu, enum type type, uint32_t size) {
	struct obj* obj = (struct obj*)mem_alloc(&cpu->alloc, size);
	obj->obj_header = make_obj_header(gm_white, type, readptr(cpu->gchead));
	cpu->gchead = writeptr(obj);
	return obj;
}

static void gc_mark_gray(struct cpu* cpu, struct containerobj* obj) {
	if (obj_get_mark(obj) == gm_white) {
		obj_mark(obj, gm_gray);
		obj->graylist = cpu->grayhead;
		cpu->grayhead = writeptr(obj);
	}
}

#define gc_mark_black(cpu, obj)		obj_mark(obj, gm_black)

void gc_mark_value(struct cpu* cpu, value_t value) {
	switch (value_get_type(value)) {
	case t_str:
	case t_buf:
		gc_mark_black(cpu, (struct obj*)value_get_object(value));
		break;

	case t_striter: {
		struct striterobj* striter = (struct striterobj*)value_get_object(value);
		gc_mark_black(cpu, striter);
		gc_mark_black(cpu, (struct strobj*)readptr(striter->str));
		break;
	}

	case t_arriter: {
		struct arriterobj* arriter = (struct arriterobj*)value_get_object(value);
		gc_mark_black(cpu, arriter);
		gc_mark_gray(cpu, (struct containerobj*)readptr(arriter->arr));
		break;
	}

	case t_assetmap: {
		struct assetmapobj* assetmap = (struct assetmapobj*)value_get_object(value);
		gc_mark_black(cpu, assetmap);
		gc_mark_black(cpu, (struct containerobj*)readptr(assetmap->buf));
		break;
	}

	case t_arr:
	case t_tab:
	case t_func: {
		gc_mark_gray(cpu, (struct containerobj*)value_get_object(value));
		break;
	}
	default: platform_error("Internal gc error.");
	}
}

#define mark_value(value)	do { \
	if (value_is_object(value)) \
		gc_mark_value(cpu, value); \
	} while (0)

static void gc_traverse_obj(struct cpu* cpu, struct containerobj* obj) {
	switch (obj_get_type(obj)) {
	case t_arr: {
		struct arrobj* arr = (struct arrobj*)obj;
		value_t* data = (value_t*)readptr(arr->data);
		for (int i = 0; i < arr->len; i++)
			mark_value(data[i]);
		cpu->cycles -= arr->len * CYCLES_TRAVERSE;
		break;
	}
	case t_tab: {
		struct tabobj* tab = (struct tabobj*)obj;
		uint16_t* bucket = (uint16_t*)readptr(tab->bucket);
		struct tabent* entries = (struct tabent*)readptr(tab->entry);
		cpu->cycles -= tab->bucket_cnt * CYCLES_TRAVERSE;
		for (int i = 0; i < tab->bucket_cnt; i++) {
			for (uint16_t p = bucket[i]; p != TAB_NULL;) {
				struct tabent* ent = &entries[p];
				struct strobj* key = (struct strobj*)readptr(ent->key);
				gc_mark_black(cpu, key);
				mark_value(ent->value);
				p = ent->next;
				cpu->cycles -= CYCLES_TRAVERSE;
			}
		}
		break;
	}
	case t_func: {
		struct funcobj* func = (struct funcobj*)obj;
		int upval_cnt = ((struct code*)readptr(func->code))->upval_cnt;
		for (int i = 0; i < upval_cnt; i++) {
			struct upval* upval = (struct upval*)readptr(func->upval[i]);
			if (obj_get_mark(upval) == gm_white) {
				gc_mark_black(cpu, upval);
				if (readptr(upval->val) == &upval->val_holder)
					mark_value(upval->val_holder);
			}
		}
		cpu->cycles -= upval_cnt * CYCLES_TRAVERSE;
		break;
	}
	}
	gc_mark_black(cpu, obj);
}

void gc_free(struct cpu* cpu, struct obj* obj) {
	switch (obj_get_type(obj)) {
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
	switch (cpu->gcstate) {
	case gs_reset: {
		cpu->gcstate = gs_mark;
		/* Mark root set */
		gc_mark_gray(cpu, (struct containerobj*)readptr(cpu->globals));
		/* passthrough */
	}
	case gs_mark: {
		while (cpu->grayhead) {
			if (cpu->cycles < 0)
				return;
			struct containerobj* obj = (struct containerobj*)readptr(cpu->grayhead);
			check(obj_get_mark(obj) == gm_gray);
			cpu->grayhead = obj->graylist;
			gc_traverse_obj(cpu, obj);
		}
		cpu->gcstate = gs_sweep;
		/* Move all current objects to sweep list */
		cpu->sweephead = make_obj_header(0, 0, readptr_nullable(cpu->gchead));
		cpu->gchead = writeptr_nullable(NULL);
		cpu->sweepcur = writeptr(&cpu->sweephead);
		/* Flip white/black bits */
		cpu->gcwhite = 1 - cpu->gcwhite;
		/* passthrough */
	}
	case gs_sweep: {
		uint32_t* prev = (uint32_t*)readptr(cpu->sweepcur);
		int oldwhite = 1 - gm_white;
		for (struct obj* cur = obj_header_get_gcnext(*prev); cur;) {
			if (cpu->cycles < 0) {
				cpu->sweepcur = writeptr(prev);
				return;
			}
			cpu->cycles -= CYCLES_ALLOC;
			struct obj* next = obj_get_gcnext(cur);
			check(obj_get_mark(cur) != gm_gray);
			if (obj_get_mark(cur) == oldwhite)
				gc_free(cpu, cur);
			else {
				*prev = obj_header_set_gcnext(*prev, cur);
				prev = &cur->obj_header;
			}
			cur = next;
		}
		/* Relink objects created during the incremental sweep phase */
		*prev = obj_header_set_gcnext(*prev, (struct obj*)readptr_nullable(cpu->gchead));
		cpu->gchead = writeptr_nullable((struct obj*)obj_header_get_gcnext(cpu->sweephead));
		cpu->sweephead = 0;
		cpu->sweepcur = writeptr_nullable(NULL);
		cpu->gcstate = gs_reset;
		break;
	}
	}
}
