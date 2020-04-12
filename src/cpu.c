#include "arr.h"
#include "arith.h"
#include "buf.h"
#include "cpu.h"
#include "gc.h"
#include "gfx.h"
#include "lib.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

#include <setjmp.h>
#include <string.h>

static jmp_buf g_jmp_buf;

NORETURN void runtime_error(struct cpu* cpu, const char* msg) {
	struct gfx* gfx = console_getgfx();
	gfx_cls(gfx, 0);
	gfx->cx = 0;
	gfx->cy = 0;
	gfx_print(gfx, "RUNTIME ERROR", 13, -1, -1, 2);
	gfx_print(gfx, msg, (int)strlen(msg), -1, -1, 2);
	longjmp(g_jmp_buf, 1);
}

NORETURN void argument_error(struct cpu* cpu) {
	runtime_error(cpu, "Wrong number of arguments.");
}

NORETURN void out_of_memory_error(struct cpu* cpu) {
	runtime_error(cpu, "Out of memory.");
}

static NORETURN void internal_error(struct cpu* cpu) {
	runtime_error(cpu, "Internal CPU error occurred.");
}

struct cpu* cpu_new() {
	struct alloc* alloc = (struct alloc*)mem_new(1048576);
	if (alloc == NULL)
		return NULL;
	struct cpu* cpu = mem_malloc(alloc, sizeof(struct cpu));
	cpu->alloc = alloc;
	cpu->parent = -1;
	cpu->top_executed = 0;
	cpu->stopped = 0;
	strtab_init(cpu);
	cpu->globals = tab_new(cpu);
	cpu->sp = 0;
	cpu->stack_cap = 0;
	cpu->stack = NULL;
	cpu->code = NULL;
	cpu->code_cnt = 0;
	cpu->code_cap = 0;
	cpu->stack = NULL;
	cpu->sp = 0;
	cpu->stack_cap = 0;
	cpu->upval_open = NULL;
	cpu->frame = 0;

	cpu->_lit_EMPTY = str_intern_nogc(cpu, "", 0);
#define X(s) cpu->_lit_##s = str_intern_nogc(cpu, #s, (sizeof #s) - 1);
	STRLIT_DEF(X);
#undef X
	lib_init(cpu);
	gfx_init(&cpu->gfx);
	tab_set(cpu, cpu->globals, cpu->_lit_global, value_tab(cpu->globals));

	return cpu;
}

void cpu_destroy(struct cpu* cpu) {
	mem_destroy(cpu->alloc);
}

static void cpu_growstack(struct cpu* cpu) {
	int new_cap = cpu->stack_cap == 0 ? 1024 : cpu->stack_cap * 2;
	struct value* old_stack = cpu->stack;
	cpu->stack = (struct value*)mem_realloc(cpu->alloc, cpu->stack, new_cap * sizeof(struct value));
	cpu->stack_cap = new_cap;
	/* adjust open upvalues */
	for (struct upval* val = cpu->upval_open; val; val = val->next)
		val->val = val->val - old_stack + cpu->stack;
}

int to_bool(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_undef: return 0;
	case t_null: return 0;
	case t_bool: return val.b;
	case t_num: return val.num != 0;
	case t_str: return val.str->len != 0;
	default: return 1;
	}
}

number to_number(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_null: return num_kint(0);
	case t_bool: return num_kint(val.b);
	case t_num: return val.num;
	case t_str: {
		number num;
		if (!num_parse(val.str->data, val.str->data + val.str->len, &num))
			runtime_error(cpu, "Invalid number.");
		return num;
	}
	default: runtime_error(cpu, "Cannot convert to number.");
	}
}

struct strobj* to_string(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_undef: return cpu->_lit_undefined;
	case t_null: return cpu->_lit_null;
	case t_bool: {
		if (val.b)
			return cpu->_lit_true;
		else
			return cpu->_lit_false;
	}
	case t_num: {
		char buf[20];
		int len = num_format(val.num, 4, buf);
		return str_intern(cpu, buf, len);
	}
	case t_str: return val.str;
	default: runtime_error(cpu, "Cannot convert to string.");
	}
}

struct arrobj* to_arr(struct cpu* cpu, struct value val) {
	if (val.type != t_arr)
		runtime_error(cpu, "Not an array.");
	return val.arr;
}

struct tabobj* to_tab(struct cpu* cpu, struct value val) {
	if (val.type != t_tab)
		runtime_error(cpu, "Not an object.");
	return val.tab;
}

static int strict_equal(struct cpu* cpu, struct value lval, struct value rval) {
	if (lval.type != rval.type)
		return 0;
	switch (lval.type) {
	case t_undef: return 1;
	case t_null: return 1;
	case t_bool: return lval.b == rval.b;
	case t_num: return lval.num == rval.num;
	case t_str: return lval.str == rval.str;
	case t_buf: return lval.buf == rval.buf;
	case t_arr: return lval.arr == rval.arr;
	case t_tab: return lval.tab == rval.tab;
	case t_func: return lval.func == rval.func;
	case t_cfunc: return lval.cfunc == rval.cfunc;
	default: internal_error(cpu);
	}
}

static struct value fget(struct cpu* cpu, struct value obj, struct value field) {
	switch (obj.type) {
	case t_str: {
		if (field.type == t_num)
			return str_get(cpu, obj.str, to_number(cpu, field));
		else
			return str_fget(cpu, obj.str, to_string(cpu, field));
	}
	case t_buf: {
		if (field.type == t_num)
			return buf_get(cpu, obj.buf, to_number(cpu, field));
		else
			return buf_fget(cpu, obj.buf, to_string(cpu, field));
	}
	case t_arr: {
		if (field.type == t_num)
			return arr_get(cpu, obj.arr, to_number(cpu, field));
		else
			return arr_fget(cpu, obj.arr, to_string(cpu, field));
	}
	case t_tab: return tab_get(cpu, obj.tab, to_string(cpu, field));
	default: runtime_error(cpu, "Not an object.");
	}
}

static void fset(struct cpu* cpu, struct value obj, struct value field, struct value value) {
	switch (obj.type) {
	case t_buf: buf_set(cpu, obj.buf, to_number(cpu, field), value); break;
	case t_arr: arr_set(cpu, obj.arr, to_number(cpu, field), value); break;
	case t_tab: tab_set(cpu, obj.tab, to_string(cpu, field), value); break;
	default: runtime_error(cpu, "Not an object.");
	}
}

static void upval_unlink(struct cpu* cpu, struct upval* val) {
	if (val->prev)
		val->prev->next = val->next;
	else
		cpu->upval_open = val->next;
	if (val->next)
		val->next->prev = val->prev;
}

static void close_upvals(struct cpu* cpu, struct value* frame, int base) {
	struct upval* val;
	for (val = cpu->upval_open; val && val->val >= frame;) {
		if (val->val - frame >= base) {
			struct upval* next = val->next;
			upval_unlink(cpu, val);
			val->val_holder = *val->val;
			val->val = &val->val_holder;
			val = next;
		}
		else
			val = val->next;
	}
}

void func_destroy(struct cpu* cpu, struct funcobj* func) {
	mem_free(cpu->alloc, func);
}

void upval_destroy(struct cpu* cpu, struct upval* upval) {
	/* Open upval ? unlink it */
	if (upval->val != &upval->val_holder)
		upval_unlink(cpu, upval);
	mem_free(cpu->alloc, upval);
}

#define update_stack()	do { \
	if (cpu->sp + 256 > cpu->stack_cap) \
		cpu_growstack(cpu); \
		frame = &cpu->stack[cpu->sp]; \
	} while (0)

#define retval		frame[ins->op1]
#define lval		frame[ins->op2]
#define rval		frame[ins->op3]
#define retvalbool	to_bool(cpu, retval)
#define lvalbool	to_bool(cpu, lval)
#define retvalnum	to_number(cpu, retval)
#define lvalnum		to_number(cpu, lval)
#define rvalnum		to_number(cpu, rval)
#define retvalstr	to_string(cpu, retval)
#define lvalstr		to_string(cpu, lval)
#define rvalstr		to_string(cpu, rval)

void cpu_execute(struct cpu* cpu, struct funcobj* func) {
	if (setjmp(g_jmp_buf) != 0) {
		cpu->stopped = 1;
		return;
	}
	struct value* frame;
	update_stack();
	cpu->stack[0] = value_func(func);
	cpu->stack[1] = value_undef();
	cpu->stack[2] = value_undef(); // call info 1
	cpu->stack[3] = value_undef(); // call info 2
	struct code* code = func->code;
	for (int pc = 0;;) {
		struct ins* ins = &code->ins[pc++];
		switch (ins->opcode) {
		case op_kundef: retval = value_undef(); break;
		case op_knull: retval = value_null(); break;
		case op_kfalse: retval = value_bool(0); break;
		case op_ktrue: retval = value_bool(1); break;
		case op_knum: {
			struct ins* next = &code->ins[pc++];
			uint16_t low = ins->imm;
			uint16_t high = next->imm;
			number num = (number)low + ((number)high << 16);
			retval = value_num(num);
			break;
		}
		case op_kobj: retval = code->k[ins->imm]; break;
		case op_mov: retval = lval; break;
		case op_xchg: {
			struct value tmp = retval;
			retval = lval;
			lval = tmp;
			break;
		}
		case op_add: {
			if (lval.type == t_str || rval.type == t_str)
				retval = value_str(str_concat(cpu, lvalstr, rvalstr));
			else
				retval = value_num(num_add(lvalnum, rvalnum)); break;
			break;
		}
		case op_sub: retval = value_num(num_sub(lvalnum, rvalnum)); break;
		case op_mul: retval = value_num(num_mul(lvalnum, rvalnum)); break;
		case op_div: retval = value_num(num_div(lvalnum, rvalnum)); break;
		case op_mod: retval = value_num(num_mod(lvalnum, rvalnum)); break;
		case op_exp: retval = value_num(num_exp(lvalnum, rvalnum)); break;
		case op_inc: retval = value_num(num_add(lvalnum, num_kint(1))); break;
		case op_dec: retval = value_num(num_sub(lvalnum, num_kint(1))); break;
		case op_plus: retval = value_num(lvalnum); break;
		case op_neg: retval = value_num(num_neg(lvalnum)); break;
		case op_not: retval = value_bool(!lvalbool); break;
		case op_band: retval = value_num(num_and(lvalnum, rvalnum)); break;
		case op_bor: retval = value_num(num_or(lvalnum, rvalnum)); break;
		case op_bxor: retval = value_num(num_xor(lvalnum, rvalnum)); break;
		case op_bnot: retval = value_num(num_not(lvalnum)); break;
		case op_shl: retval = value_num(num_shl(lvalnum, rvalnum)); break;
		case op_shr: retval = value_num(num_shr(lvalnum, rvalnum)); break;
		case op_ushr: retval = value_num(num_ushr(lvalnum, rvalnum)); break;
		case op_eq: retval = value_bool(strict_equal(cpu, lval, rval)); break;
		case op_ne: retval = value_bool(!strict_equal(cpu, lval, rval)); break;
		case op_lt: retval = value_bool((int32_t)lvalnum < (int32_t)rvalnum); break;
		case op_le: retval = value_bool((int32_t)lvalnum <= (int32_t)rvalnum); break;
		case op_gt: retval = value_bool((int32_t)lvalnum > (int32_t)rvalnum); break;
		case op_ge: retval = value_bool((int32_t)lvalnum >= (int32_t)rvalnum); break;
		case op_in: retval = value_bool(tab_in(cpu, to_tab(cpu, rval), lvalstr)); break;
		case op_arr: retval = value_arr(arr_new(cpu)); break;
		case op_apush: arr_push(cpu, to_arr(cpu, retval), lval); break;
		case op_tab: retval = value_tab(tab_new(cpu)); break;
		case op_fget: retval = fget(cpu, lval, rval); break;
		case op_fset: fset(cpu, retval, lval, rval); break;
		case op_gget: retval = tab_get(cpu, cpu->globals, lvalstr); break;
		case op_gset: tab_set(cpu, cpu->globals, retvalstr, lval); break;
		case op_uget: retval = *func->upval[ins->op2]->val; break;
		case op_uset: *func->upval[ins->op1]->val = lval; break;
		case op_j: pc += ins->imm; break;
		case op_jtrue: if (retvalbool) pc += ins->imm; break;
		case op_jfalse: if (!retvalbool) pc += ins->imm; break;
		case op_func: {
			struct code* code = &cpu->code[ins->imm];
			struct funcobj* f = (struct funcobj*)gc_alloc(cpu, t_func,
				sizeof(struct funcobj) + sizeof(struct upval) * code->upval_cnt);
			f->code = code;
			for (int i = 0; i < code->upval_cnt; i++) {
				struct updef* def = &code->upval[i];
				if (def->in_stack) {
					/* find existing open upvalue */
					struct upval* val = cpu->upval_open;
					while (val) {
						if (val->val >= frame) {
							int idx = (int)(val->val - frame);
							if (idx == def->idx) {
								f->upval[i] = val;
								break;
							}
						}
						val = val->next;
					}
					if (!val) {
						/* not found, create new one */
						val = (struct upval*)gc_alloc(cpu, t_upval, sizeof(struct upval));
						val->val = &frame[def->idx];
						val->prev = NULL;
						val->next = cpu->upval_open;
						if (val->next)
							val->next->prev = val;
						cpu->upval_open = val;
						f->upval[i] = val;
					}
				}
				else
					f->upval[i] = func->upval[def->idx];
			}
			retval = value_func(f);
			break;
		}
		case op_close: {
			close_upvals(cpu, frame, ins->op1);
			break;
		}
		case op_call: {
			if (retval.type == t_cfunc)
				retval.cfunc(cpu, cpu->sp + ins->op1, ins->op2);
			else if (retval.type == t_func) {
				struct funcobj* f = retval.func;
				/* Fill missing arguments to undefined */
				for (int i = ins->op2; i < f->code->nargs; i++)
					frame[ins->op1 + 2 + i] = value_undef();
				int stack_size = ins->op1;
				cpu->sp += ins->op1;
				update_stack();
				int ci = cpu->sp + 2 + f->code->nargs;
				cpu->stack[ci].type = t_callinfo1;
				struct callinfo1* ci1 = &cpu->stack[ci].ci1;
				ci1->func = func;
				cpu->stack[ci + 1].type = t_callinfo2;
				struct callinfo2* ci2 = &cpu->stack[ci + 1].ci2;
				ci2->pc = pc;
				ci2->stack_size = stack_size;
				pc = 0;
				func = f;
				code = func->code;
			}
			else
				runtime_error(cpu, "Not callable object.");
			break;
		}
		case op_retu:
		case op_ret: {
			close_upvals(cpu, frame, 0);
			if (cpu->sp == 0)
				return;
			struct callinfo1 ci1 = frame[2 + func->code->nargs].ci1;
			struct callinfo2 ci2 = frame[3 + func->code->nargs].ci2;
			if (ins->opcode == op_retu)
				frame[0] = value_undef();
			else
				frame[0] = retval;
			func = ci1.func;
			pc = ci2.pc;
			cpu->sp -= ci2.stack_size;
			update_stack();
			code = func->code;
			break;
		}
		default: internal_error(cpu);
		}
	}
}

enum operand_type {
	ot__,
	ot_REG,
	ot_IMM8,
	ot_IMM16,
	ot_IMMNUM,
	ot_IMMK,
	ot_IMMFUNC,
	ot_REL,
};

struct opcode_desc {
	const char* name;
	enum operand_type op1, op2, op3;
};
#define X(a, b, c, d, e)	{ b, ot_##c, ot_##d, ot_##e },
static const struct opcode_desc opcode_desc[] = {
	OPCODE_DEF(X)
};
#undef X

static char* dump_operand(enum operand_type ot, int value, char* buf) {
	if (ot == ot_REG)
		*buf++ = 'r';
	return buf + int_format(value, buf);
}

#define CHECKSPACE() do { if (cur - buf + 80 > buflen) return (int)(cur - buf); } while(0)
static int dump_func(struct cpu* cpu, int func_id, char* buf, int buflen) {
	struct code* code = &cpu->code[func_id];
	char* cur = buf;
	CHECKSPACE();
	*cur++ = 'F';
	*cur++ = 'u';
	*cur++ = 'n';
	*cur++ = 'c';
	*cur++ = ' ';
	*cur++ = '#';
	cur += int_format(func_id, cur);
	*cur++ = ' ';
	*cur++ = 'n';
	*cur++ = 'a';
	*cur++ = 'r';
	*cur++ = 'g';
	*cur++ = ':';
	cur += int_format(code->nargs, cur);
	*cur++ = ' ';
	*cur++ = 'e';
	*cur++ = 'n';
	*cur++ = 'c';
	*cur++ = 'l';
	*cur++ = 'o';
	*cur++ = 's';
	*cur++ = 'u';
	*cur++ = 'r';
	*cur++ = 'e';
	*cur++ = ':';
	cur += int_format(code->enclosure, cur);
	*cur++ = '\n';
	for (int i = 0; i < code->upval_cnt; i++) {
		*cur++ = 'u';
		*cur++ = 'p';
		*cur++ = ' ';
		struct updef* upval = &code->upval[i];
		cur += int_format(i, cur);
		*cur++ = ':';
		*cur++ = ' ';
		if (upval->in_stack)
			*cur++ = 'S';
		cur += int_format(upval->idx, cur);
		*cur++ = '\n';
	}
	for (int pc = 0; pc < code->ins_cnt;) {
		CHECKSPACE();
		struct ins* ins = &code->ins[pc];
		cur += int_format(pc++, cur);
		*cur++ = '\t';
		const struct opcode_desc* desc = &opcode_desc[ins->opcode];
		int namelen = (int)strlen(desc->name);
		memcpy(cur, desc->name, namelen);
		cur += namelen;
		for (int i = namelen; i < 8; i++)
			*cur++ = ' ';
		int comma = 0;
		if (desc->op1 != ot__) {
			cur = dump_operand(desc->op1, ins->op1, cur);
			comma = 1;
		}
		if (desc->op2 != ot__) {
			if (comma) {
				*cur++ = ',';
				*cur++ = ' ';
			}
			switch (desc->op2) {
			case ot_IMMK: {
				struct value val = code->k[ins->imm];
				if (val.type == t_str) {
					struct strobj* str = val.str;
					int len = str->len;
					if (len > 20)
						len = 20;
					*cur++ = '"';
					memcpy(cur, str->data, len);
					cur += len;
					*cur++ = '"';
					if (len < str->len) {
						*cur++ = '.';
						*cur++ = '.';
						*cur++ = '.';
					}
				}
				else
					cur += int_format(ins->imm, cur);
				break;
			}
			case ot_IMM16:
			case ot_IMMFUNC:
				cur += int_format(ins->imm, cur);
				break;
			case ot_IMMNUM: {
				number num = ins->imm + (code->ins[pc++].imm << 16);
				cur += num_format(num, 4, cur);
				break;
			}
			case ot_REL: {
				cur += int_format(pc + ins->imm, cur);
				break;
			}
			default: {
				cur = dump_operand(desc->op2, ins->op2, cur);
				if (desc->op3 != ot__) {
					*cur++ = ',';
					*cur++ = ' ';
					cur = dump_operand(desc->op3, ins->op3, cur);
				}
			}
			}
		}
		*cur++ = '\n';
	}
	return (int)(cur - buf);
}

int cpu_dump_code(struct cpu* cpu, char* buf, int buflen) {
	int totlen = 0;
	for (int i = 0; i < cpu->code_cnt; i++) {
		totlen += dump_func(cpu, i, buf + totlen, buflen - totlen);
		if (totlen < buflen)
			buf[totlen++] = '\n';
	}
	return totlen;
}
