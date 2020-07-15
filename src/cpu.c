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
static int g_pc;
static struct code* g_code;

static void print_name(struct cpu* cpu, struct code* code) {
	struct gfx* gfx = console_getgfx();
	if (code->enclosure > 0) {
		print_name(cpu, &((struct code*)readptr(cpu->code))[code->enclosure]);
		gfx_print_simple(gfx, ":", 1, 14);
	}
	struct strobj* name = (struct strobj*)readptr(code->name);
	if (name)
		gfx_print_simple(gfx, name->data, name->len, 14);
	else if (code->enclosure == -1)
		gfx_print_simple(gfx, "<top>", 5, 14);
	else
		gfx_print_simple(gfx, "<anon>", 6, 14);
}

NORETURN void runtime_error(struct cpu* cpu, const char* msg) {
	struct gfx* gfx = console_getgfx();
	gfx_cls(gfx, 0);
	gfx_reset_pal(gfx);
	gfx_reset_palt(gfx);
	gfx_camera(gfx, 0, 0);
	gfx->cx = 0;
	gfx->cy = 0;
	gfx_print(gfx, "RUNTIME ERROR", 13, -1, -1, 2);
	gfx_print(gfx, msg, (int)strlen(msg), -1, -1, 2);
	gfx_print(gfx, "Stack trace:", 12, -1, -1, 14);
	struct code* code = g_code;
	int pc = g_pc;
	int sp = cpu->sp;
	for (int depth = 0; depth < MAX_STACKTRACE; depth++) {
		pc--;
		print_name(cpu, code);
		struct licmd* lineinfo = (struct licmd*)readptr(code->lineinfo);
		int line = 0;
		int ins_id = 0;
		for (int i = 0; i < code->lineinfo_cnt; i++) {
			struct licmd* cmd = &lineinfo[i];
			if (cmd->type == li_line)
				line += cmd->delta;
			else if (cmd->type == li_ins) {
				ins_id += cmd->delta;
				if (pc < ins_id)
					break;
			}
		}
		char linestr[10];
		gfx_print_simple(gfx, ":", 1, 14);
		gfx_print_simple(gfx, linestr, int_format(line + 1, linestr), 14);
		gfx_print_simple(gfx, "\n", 1, 14);
		if (sp == 0)
			break;

		struct value* frame = &((struct value*)readptr(cpu->stack))[sp];
		struct callinfo1 ci1 = frame[2 + code->nargs].ci1;
		struct callinfo2 ci2 = frame[3 + code->nargs].ci2;
		struct funcobj* func = readptr(ci1.func);
		pc = ci2.pc;
		sp -= ci2.stack_size;
		code = (struct code*)readptr(func->code);
	}
	if (sp != 0)
		gfx_print_simple(gfx, "...", 3, 14);
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
	struct cpu* cpu = (struct cpu*)mem_new(CPU_MEM_SIZE, sizeof(struct cpu));
	if (cpu == NULL)
		return NULL;
	cpu->gchead = writeptr(NULL);
	cpu->parent = -1;
	cpu->top_executed = 0;
	cpu->stopped = 0;
	strtab_init(cpu);
	struct tabobj* globals = tab_new(cpu);
	cpu->globals = writeptr(globals);
	cpu->code = writeptr(NULL);
	cpu->code_cnt = 0;
	cpu->code_cap = 0;
	cpu->stack = writeptr(NULL);
	cpu->sp = 0;
	cpu->stack_cap = 0;
	cpu->upval_open = writeptr(NULL);
	cpu->frame = 0;

	cpu->_lit_EMPTY = writeptr(str_intern_nogc(cpu, "", 0));
#define X(s) cpu->_lit_##s = writeptr(str_intern_nogc(cpu, #s, (sizeof #s) - 1));
	STRLIT_DEF(X);
#undef X
	lib_init(cpu);
	gfx_init(&cpu->gfx);
	tab_set(cpu, globals, LIT(global), value_tab(cpu, globals));

	return cpu;
}

void cpu_destroy(struct cpu* cpu) {
	mem_destroy(&cpu->alloc);
}

static void cpu_growstack(struct cpu* cpu) {
	int new_cap = cpu->stack_cap == 0 ? 1024 : cpu->stack_cap * 2;
	struct value* old_stack = (struct value*)readptr(cpu->stack);
	struct value* new_stack = (struct value*)mem_realloc(&cpu->alloc, old_stack, new_cap * sizeof(struct value));
	cpu->stack = writeptr(new_stack);
	cpu->stack_cap = new_cap;
	/* adjust open upvalues */
	for (struct upval* val = readptr(cpu->upval_open); val; val = readptr(val->next))
		val->val = writeptr((struct value*)readptr(val->val) - old_stack + new_stack);
}

int to_bool(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_undef: return 0;
	case t_null: return 0;
	case t_bool: return val.b;
	case t_num: return val.num != 0;
	case t_str: return ((struct strobj*)readptr(val.str))->len != 0;
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
		struct strobj* str = (struct strobj*)readptr(val.str);
		if (!num_parse(str->data, str->data + str->len, &num))
			runtime_error(cpu, "Invalid number.");
		cpu->cycles += CYCLES_STR2NUM;
		return num;
	}
	default: runtime_error(cpu, "Cannot convert to number.");
	}
}

struct strobj* to_string(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_undef: return LIT(undefined);
	case t_null: return LIT(null);
	case t_bool: {
		if (val.b)
			return LIT(true);
		else
			return LIT(false);
	}
	case t_num: {
		char buf[20];
		int len = num_format(val.num, 4, buf);
		cpu->cycles += CYCLES_NUM2STR;
		return str_intern(cpu, buf, len);
	}
	case t_str: return (struct strobj*)readptr(val.str);
	default: runtime_error(cpu, "Cannot convert to string.");
	}
}

struct arrobj* to_arr(struct cpu* cpu, struct value val) {
	if (val.type != t_arr)
		runtime_error(cpu, "Not an array.");
	return (struct arrobj*)readptr(val.arr);
}

struct tabobj* to_tab(struct cpu* cpu, struct value val) {
	if (val.type != t_tab)
		runtime_error(cpu, "Not an object.");
	return (struct tabobj*)readptr(val.tab);
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
		struct strobj* str = (struct strobj*)readptr(obj.str);
		if (field.type == t_num)
			return str_get(cpu, str, to_number(cpu, field));
		else
			return str_fget(cpu, str, to_string(cpu, field));
	}
	case t_buf: {
		struct bufobj* buf = (struct bufobj*)readptr(obj.buf);
		if (field.type == t_num)
			return buf_get(cpu, buf, to_number(cpu, field));
		else
			return buf_fget(cpu, buf, to_string(cpu, field));
	}
	case t_arr: {
		struct arrobj* arr = (struct arrobj*)readptr(obj.arr);
		if (field.type == t_num)
			return arr_get(cpu, arr, to_number(cpu, field));
		else
			return arr_fget(cpu, arr, to_string(cpu, field));
	}
	case t_tab: {
		struct tabobj* tab = (struct tabobj*)readptr(obj.tab);
		return tab_get(cpu, tab, to_string(cpu, field));
	}
	default: runtime_error(cpu, "Not an object.");
	}
}

static void fset(struct cpu* cpu, struct value obj, struct value field, struct value value) {
	switch (obj.type) {
	case t_buf: buf_set(cpu, (struct bufobj*)readptr(obj.buf), to_number(cpu, field), value); break;
	case t_arr: arr_set(cpu, (struct arrobj*)readptr(obj.arr), to_number(cpu, field), value); break;
	case t_tab: tab_set(cpu, (struct tabobj*)readptr(obj.tab), to_string(cpu, field), value); cpu->cycles += CYCLES_LOOKUP;  break;
	default: runtime_error(cpu, "Not an object.");
	}
}

static void upval_unlink(struct cpu* cpu, struct upval* val) {
	struct upval* prev = readptr(val->prev);
	if (prev)
		prev->next = val->next;
	else
		cpu->upval_open = val->next;
	struct upval* next = readptr(val->next);
	if (next)
		next->prev = val->prev;
}

static void close_upvals(struct cpu* cpu, struct value* frame, int base) {
	struct upval* val;
	int cnt = 0;
	for (val = readptr(cpu->upval_open); val && (struct value*)readptr(val->val) >= frame;) {
		struct value* v = readptr(val->val);
		struct upval* next = readptr(val->next);
		if (v - frame >= base) {
			upval_unlink(cpu, val);
			val->val_holder = *v;
			val->val = writeptr(&val->val_holder);
		}
		val = next;
		cnt++;
	}
	cpu->cycles += CYCLES_UPVALUES(cnt);
}

static struct value get_iter(struct cpu* cpu, struct value val) {
	switch (val.type) {
	case t_str: {
		struct strobj* str = (struct strobj*)readptr(val.str);
		struct striterobj* striter = (struct striterobj*)gc_alloc(cpu, t_striter, sizeof(struct striterobj));
		striter->str = writeptr(str);
		striter->i = 0;
		striter->end = str->len;
		return value_striter(cpu, striter);
	}

	case t_arr: {
		struct arrobj* arr = (struct arrobj*)readptr(val.arr);
		struct arriterobj* arriter = (struct arriterobj*)gc_alloc(cpu, t_arriter, sizeof(struct arriterobj));
		arriter->arr = writeptr(arr);
		arriter->i = 0;
		arriter->end = arr->len;
		return value_arriter(cpu, arriter);
	}

	default:
		runtime_error(cpu, "The object does not have built-in iterator support.");
	}
}

static int iter_next(struct cpu* cpu, struct value iterval, struct value* val) {
	switch (iterval.type) {
	case t_striter: {
		struct striterobj* striter = (struct striterobj*)readptr(iterval.striter);
		struct strobj* str = (struct strobj*)readptr(striter->str);
		if (striter->i == -1 || striter->i >= str->len) {
			striter->i = -1;
			*val = value_undef(cpu);
			return 1;
		}
		else {
			*val = value_str(cpu, str_intern(cpu, &str->data[striter->i], 1));
			if (++striter->i == str->len)
				striter->i = -1;
			return 0;
		}
	}
	case t_arriter: {
		struct arriterobj* arriter = (struct arriterobj*)readptr(iterval.arriter);
		struct arrobj* arr = (struct arrobj*)readptr(arriter->arr);
		if (arriter->i == -1 || arriter->i >= arr->len) {
			arriter->i = -1;
			*val = value_undef(cpu);
			return 1;
		}
		else {
			struct arrobj* arr = (struct arrobj*)readptr(arriter->arr);
			*val = ((struct value*)readptr(arr->data))[arriter->i];
			if (++arriter->i == arr->len)
				arriter->i = -1;
			return 0;
		}
	}
	default:
		runtime_error(cpu, "Not an iterator object.");
	}
}

void func_destroy(struct cpu* cpu, struct funcobj* func) {
	mem_free(&cpu->alloc, func);
}

void upval_destroy(struct cpu* cpu, struct upval* upval) {
	/* Open upval ? unlink it */
	if (readptr(upval->val) != &upval->val_holder)
		upval_unlink(cpu, upval);
	mem_free(&cpu->alloc, upval);
}

#define update_stack()	do { \
	if (cpu->sp + 256 > cpu->stack_cap) \
		cpu_growstack(cpu); \
		frame = &((struct value*)readptr(cpu->stack))[cpu->sp]; \
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
	{
		struct value* stack = (struct value*)readptr(cpu->stack);
		stack[0] = value_func(cpu, func);
		stack[1] = value_undef(cpu);
		stack[2] = value_undef(cpu); // call info 1
		stack[3] = value_undef(cpu); // call info 2
	}
	g_code = readptr(func->code);
	cpu->cycles = 0;
	for (g_pc = 0;;) {
		struct ins* ins = &((struct ins*)readptr(g_code->ins))[g_pc++];
		cpu->cycles += CYCLES_BASE;
		switch (ins->opcode) {
		case op_kundef: retval = value_undef(cpu); break;
		case op_knull: retval = value_null(cpu); break;
		case op_kfalse: retval = value_bool(cpu, 0); break;
		case op_ktrue: retval = value_bool(cpu, 1); break;
		case op_knum: {
			struct ins* next = &((struct ins*)readptr(g_code->ins))[g_pc++];
			uint16_t low = ins->imm;
			uint16_t high = next->imm;
			number num = (number)low + ((number)high << 16);
			retval = value_num(cpu, num);
			break;
		}
		case op_kobj: retval = ((struct value*)readptr(g_code->k))[ins->imm]; break;
		case op_mov: retval = lval; break;
		case op_xchg: {
			struct value tmp = retval;
			retval = lval;
			lval = tmp;
			break;
		}
		case op_add: {
			if (lval.type == t_str || rval.type == t_str) {
				struct strobj* l = lvalstr;
				struct strobj* r = rvalstr;
				retval = value_str(cpu, str_concat(cpu, l, r));
				cpu->cycles += CYCLES_CHARS(l->len + r->len) + CYCLES_ALLOC;
			}
			else
				retval = value_num(cpu, num_add(lvalnum, rvalnum)); break;
			break;
		}
		case op_sub: retval = value_num(cpu, num_sub(lvalnum, rvalnum)); break;
		case op_mul: retval = value_num(cpu, num_mul(lvalnum, rvalnum)); break;
		case op_div: retval = value_num(cpu, num_div(lvalnum, rvalnum)); break;
		case op_mod: retval = value_num(cpu, num_mod(lvalnum, rvalnum)); break;
		case op_exp: retval = value_num(cpu, num_exp(lvalnum, rvalnum)); break;
		case op_inc: retval = value_num(cpu, num_add(lvalnum, num_kint(1))); break;
		case op_dec: retval = value_num(cpu, num_sub(lvalnum, num_kint(1))); break;
		case op_plus: retval = value_num(cpu, lvalnum); break;
		case op_neg: retval = value_num(cpu, num_neg(lvalnum)); break;
		case op_not: retval = value_bool(cpu, !lvalbool); break;
		case op_band: retval = value_num(cpu, num_and(lvalnum, rvalnum)); break;
		case op_bor: retval = value_num(cpu, num_or(lvalnum, rvalnum)); break;
		case op_bxor: retval = value_num(cpu, num_xor(lvalnum, rvalnum)); break;
		case op_bnot: retval = value_num(cpu, num_not(lvalnum)); break;
		case op_shl: retval = value_num(cpu, num_shl(lvalnum, rvalnum)); break;
		case op_shr: retval = value_num(cpu, num_shr(lvalnum, rvalnum)); break;
		case op_ushr: retval = value_num(cpu, num_ushr(lvalnum, rvalnum)); break;
		case op_eq: retval = value_bool(cpu, strict_equal(cpu, lval, rval)); break;
		case op_ne: retval = value_bool(cpu, !strict_equal(cpu, lval, rval)); break;
		case op_lt: retval = value_bool(cpu, (int32_t)lvalnum < (int32_t)rvalnum); break;
		case op_le: retval = value_bool(cpu, (int32_t)lvalnum <= (int32_t)rvalnum); break;
		case op_gt: retval = value_bool(cpu, (int32_t)lvalnum > (int32_t)rvalnum); break;
		case op_ge: retval = value_bool(cpu, (int32_t)lvalnum >= (int32_t)rvalnum); break;
		case op_in: retval = value_bool(cpu, tab_in(cpu, to_tab(cpu, rval), lvalstr)); break;
		case op_arr: retval = value_arr(cpu, arr_new(cpu)); cpu->cycles += CYCLES_ALLOC; break;
		case op_apush: arr_push(cpu, to_arr(cpu, retval), lval); cpu->cycles += CYCLES_ALLOC; break;
		case op_tab: retval = value_tab(cpu, tab_new(cpu)); cpu->cycles += CYCLES_ALLOC; break;
		case op_fget: retval = fget(cpu, lval, rval); cpu->cycles += CYCLES_LOOKUP; break;
		case op_fset: fset(cpu, retval, lval, rval); break;
		case op_gget: retval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), lvalstr); cpu->cycles += CYCLES_LOOKUP; break;
		case op_gset: tab_set(cpu, (struct tabobj*)readptr(cpu->globals), retvalstr, lval); cpu->cycles += CYCLES_LOOKUP; break;
		case op_uget: retval = *(struct value*)readptr(((struct upval*)readptr(func->upval[ins->op2]))->val); break;
		case op_uset: *(struct value*)readptr(((struct upval*)readptr(func->upval[ins->op1]))->val) = lval; break;
		case op_iter: retval = get_iter(cpu, lval); cpu->cycles += CYCLES_ALLOC; break;
		case op_next: retval = value_bool(cpu, iter_next(cpu, rval, &lval)); break;
		case op_j: g_pc += ins->imm; break;
		case op_closej: {
			close_upvals(cpu, frame, ins->op1);
			g_pc += ins->imm; break;
			break;
		}
		case op_jtrue: if (retvalbool) g_pc += ins->imm; break;
		case op_jfalse: if (!retvalbool) g_pc += ins->imm; break;
		case op_func: {
			struct code* code = &((struct code*)readptr(cpu->code))[ins->imm];
			struct funcobj* f = (struct funcobj*)gc_alloc(cpu, t_func,
				sizeof(struct funcobj) + sizeof(struct upval) * code->upval_cnt);
			f->code = writeptr(code);
			struct updef* defs = readptr(code->upval);
			for (int i = 0; i < code->upval_cnt; i++) {
				struct updef* def = &defs[i];
				if (def->in_stack) {
					/* find existing open upvalue */
					struct upval* val = readptr(cpu->upval_open);
					while (val) {
						struct value* v = readptr(val->val);
						if (v >= frame) {
							int idx = (int)(v - frame);
							if (idx == def->idx) {
								f->upval[i] = writeptr(val);
								break;
							}
						}
						val = readptr(val->next);
					}
					if (!val) {
						/* not found, create new one */
						val = (struct upval*)gc_alloc(cpu, t_upval, sizeof(struct upval));
						val->val = writeptr(&frame[def->idx]);
						val->prev = writeptr(NULL);
						val->next = cpu->upval_open;
						struct upval* next = readptr(val->next);
						if (next)
							next->prev = writeptr(val);
						cpu->upval_open = writeptr(val);
						f->upval[i] = writeptr(val);
					}
				}
				else
					f->upval[i] = func->upval[def->idx];
			}
			retval = value_func(cpu, f);
			cpu->cycles += CYCLES_ALLOC * (1 + code->upval_cnt);
			break;
		}
		case op_close: {
			close_upvals(cpu, frame, ins->op1);
			break;
		}
		case op_call: {
			if (retval.type == t_cfunc) {
				int sp = cpu->sp + ins->op1;
				cfunc func = cfunc_get(retval.cfunc);
				((struct value*)readptr(cpu->stack))[sp] = func(cpu, sp, ins->op2);
			}
			else if (retval.type == t_func) {
				struct funcobj* f = (struct funcobj*)readptr(retval.func);
				/* Fill missing arguments to undefined */
				int nargs = ((struct code*)readptr(f->code))->nargs;
				for (int i = ins->op2; i < nargs; i++)
					frame[ins->op1 + 2 + i] = value_undef(cpu);
				cpu->cycles += CYCLES_BASE * (nargs - ins->op2);
				int stack_size = ins->op1;
				cpu->sp += ins->op1;
				update_stack();
				struct value* stack = (struct value*)readptr(cpu->stack);
				int ci = cpu->sp + 2 + nargs;
				stack[ci].type = t_callinfo1;
				struct callinfo1* ci1 = &stack[ci].ci1;
				ci1->func = writeptr(func);
				stack[ci + 1].type = t_callinfo2;
				struct callinfo2* ci2 = &stack[ci + 1].ci2;
				ci2->pc = g_pc;
				ci2->stack_size = stack_size;
				g_pc = 0;
				func = f;
				g_code = readptr(func->code);
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
			int nargs = ((struct code*)readptr(func->code))->nargs;
			struct callinfo1 ci1 = frame[2 + nargs].ci1;
			struct callinfo2 ci2 = frame[3 + nargs].ci2;
			if (ins->opcode == op_retu)
				frame[0] = value_undef(cpu);
			else
				frame[0] = retval;
			func = readptr(ci1.func);
			g_pc = ci2.pc;
			cpu->sp -= ci2.stack_size;
			update_stack();
			g_code = readptr(func->code);
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

#define CHECKSPACEN(n) do { if (cur - buf + (n) > buflen) return (int)(cur - buf); } while(0)
#define CHECKSPACE() CHECKSPACEN(80)
static int dump_func(struct cpu* cpu, const char* codebuf, int codelen, int func_id, char* buf, int buflen) {
	struct code* code = &((struct code*)readptr(cpu->code))[func_id];
	char* cur = buf;
	CHECKSPACEN(80 + SYM_MAX_LEN);
	*cur++ = 'F';
	*cur++ = 'u';
	*cur++ = 'n';
	*cur++ = 'c';
	*cur++ = ' ';
	*cur++ = '#';
	cur += int_format(func_id, cur);
	struct strobj* name = (struct strobj*)readptr(code->name);
	if (name) {
		*cur++ = ':';
		*cur++ = ' ';
		memcpy(cur, name->data, name->len);
		cur += name->len;
		*cur++ = '(';
		*cur++ = ')';
	}
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
	struct updef* upvals = readptr(code->upval);
	for (int i = 0; i < code->upval_cnt; i++) {
		*cur++ = 'u';
		*cur++ = 'p';
		*cur++ = ' ';
		struct updef* upval = &upvals[i];
		cur += int_format(i, cur);
		*cur++ = ':';
		*cur++ = ' ';
		if (upval->in_stack)
			*cur++ = 'S';
		cur += int_format(upval->idx, cur);
		*cur++ = '\n';
	}
	struct ins* inss = (struct ins*)readptr(code->ins);
	int output_linenum = 0;
	int linenum = 0;
	int next_lineinfo_pc = 0;
	int lineinfo_id = 0;
	struct licmd* lineinfo = (struct licmd*)readptr(code->lineinfo);
	int linepos = 0;
	for (int pc = 0; pc < code->ins_cnt;) {
		if (pc == next_lineinfo_pc) {
			while (lineinfo_id < code->lineinfo_cnt) {
				struct licmd* cmd = &lineinfo[lineinfo_id++];
				if (cmd->type == li_line) {
					linenum += cmd->delta;
					for (int i = 0; i < cmd->delta; i++) {
						while (codebuf[linepos++] != '\n')
							continue;
					}
				}
				else if (cmd->type == li_ins) {
					next_lineinfo_pc += cmd->delta;
					break;
				}
			}
		}
		if (linenum >= output_linenum) {
			/* Dump current line */
			int lineend = linepos;
			while (lineend < codelen && codebuf[lineend] != '\n')
				lineend++;
			CHECKSPACEN(lineend - linepos + 1);
			memcpy(cur, &codebuf[linepos], lineend - linepos);
			cur += lineend - linepos;
			*cur++ = '\n';
			output_linenum = linenum + 1;
		}
		CHECKSPACE();
		struct ins* ins = &inss[pc];
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
				struct value val = ((struct value*)readptr(code->k))[ins->imm];
				if (val.type == t_str) {
					struct strobj* str = (struct strobj*)readptr(val.str);
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
				number num = ins->imm + (inss[pc++].imm << 16);
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

int cpu_dump_code(struct cpu* cpu, const char* codebuf, int codelen, char* buf, int buflen) {
	int totlen = 0;
	for (int i = 0; i < cpu->code_cnt; i++) {
		totlen += dump_func(cpu, codebuf, codelen, i, buf + totlen, buflen - totlen);
		if (totlen < buflen)
			buf[totlen++] = '\n';
	}
	return totlen;
}
