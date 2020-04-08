#ifndef _CPU_H
#define _CPU_H

#include "alloc.h"
#include "arith.h"
#include "config.h"
#include "key.h"

#include <stdint.h>

struct cpu;

#define OBJ_HEADER	struct obj* gcnext; enum type type

enum type {
	t_undef,
	t_null,
	t_bool,
	t_num,
	t_str,
	t_buf,
	t_arr,
	t_tab,
	t_func,
	t_cfunc,
	t_callinfo1,
	t_callinfo2,
	t_upval,
	t_cnt,
};

typedef void (*cfunc)(struct cpu* cpu, int sp, int nargs);

struct callinfo1 {
	struct funcobj* func;
};

struct callinfo2 {
	uint8_t stack_size;
	uint16_t pc;
};

struct value {
	union {
		int b;
		number num;
		struct obj* obj;
		struct strobj* str;
		struct bufobj* buf;
		struct arrobj* arr;
		struct tabobj* tab;
		struct funcobj* func;
		cfunc cfunc;
		struct callinfo1 ci1;
		struct callinfo2 ci2;
	};
	enum type type : 8;
};

struct obj {
	OBJ_HEADER;
};

struct strobj {
	OBJ_HEADER;
	struct strobj* next;
	uint32_t hash;
	uint32_t len;
	char data[];
};

struct bufobj {
	OBJ_HEADER;
	uint32_t len;
	uint8_t data[];
};

struct arrobj {
	OBJ_HEADER;
	uint32_t len, cap;
	struct value* data;
};

#define TAB_NULL		((uint16_t)-1)

struct tabent {
	struct strobj* key;
	struct value value;
	uint16_t next;
};

struct tabobj {
	OBJ_HEADER;
	int entry_cnt;
	struct tabent* entry;
	uint16_t freelist;
	int bucket_cnt;
	uint16_t* bucket;
};

static inline struct value value_undef() {
	struct value val;
	val.type = t_undef;
	return val;
}

static inline struct value value_null() {
	struct value val;
	val.type = t_null;
	return val;
}

static inline struct value value_bool(int b) {
	struct value val;
	val.type = t_bool;
	val.b = b;
	return val;
}

static inline struct value value_num(number num) {
	struct value val;
	val.type = t_num;
	val.num = num;
	return val;
}

static inline struct value value_str(struct strobj* str) {
	struct value val;
	val.type = t_str;
	val.str = str;
	return val;
}

static inline struct value value_buf(struct bufobj* buf) {
	struct value val;
	val.type = t_buf;
	val.buf = buf;
	return val;
}

static inline struct value value_arr(struct arrobj* arr) {
	struct value val;
	val.type = t_arr;
	val.arr = arr;
	return val;
}

static inline struct value value_tab(struct tabobj* tab) {
	struct value val;
	val.type = t_tab;
	val.tab = tab;
	return val;
}

static inline struct value value_func(struct funcobj* func) {
	struct value val;
	val.type = t_func;
	val.func = func;
	return val;
}

static inline struct value value_cfunc(cfunc cfunc) {
	struct value val;
	val.type = t_cfunc;
	val.cfunc = cfunc;
	return val;
}

#define OPCODE_DEF(X) \
	X(op_data, "data", _, _, _) \
	X(op_kundef, "kundef", REG, _, _) \
	X(op_knull, "knull", REG, _, _) \
	X(op_kfalse, "kfalse", REG, _, _) \
	X(op_ktrue, "ktrue", REG, _, _) \
	X(op_knum, "knum", REG, IMMNUM, _) \
	X(op_kobj, "kobj", REG, IMMK, _) \
	X(op_mov, "mov", REG, REG, _) \
	X(op_xchg, "xchg", REG, REG, _) \
	/* arithmetic */ \
	X(op_add, "add", REG, REG, REG) \
	X(op_sub, "sub", REG, REG, REG) \
	X(op_mul, "mul", REG, REG, REG) \
	X(op_div, "div", REG, REG, REG) \
	X(op_mod, "mod", REG, REG, REG) \
	X(op_exp, "exp", REG, REG, REG) \
	X(op_inc, "inc", REG, REG, _) \
	X(op_dec, "dec", REG, REG, _) \
	X(op_plus, "plus", REG, REG, _) \
	X(op_neg, "neg", REG, REG, _) \
	/* logical */ \
	X(op_not, "not", REG, REG, _) \
	/* bitwise logical */ \
	X(op_band, "band", REG, REG, REG) \
	X(op_bor, "bor", REG, REG, REG) \
	X(op_bxor, "bxor", REG, REG, REG) \
	X(op_bnot, "bnot", REG, REG, REG) \
	X(op_shl, "shl", REG, REG, REG) \
	X(op_shr, "shr", REG, REG, REG) \
	X(op_ushr, "ushr", REG, REG, REG) \
	/* comparison */ \
	X(op_eq, "eq", REG, REG, REG) \
	X(op_ne, "ne", REG, REG, REG) \
	X(op_lt, "lt", REG, REG, REG) \
	X(op_le, "le", REG, REG, REG) \
	X(op_gt, "gt", REG, REG, REG) \
	X(op_ge, "ge", REG, REG, REG) \
	X(op_in, "in", REG, REG, REG) \
	/* object creation */ \
	X(op_arr, "arr", REG, _, _) \
	X(op_apush, "apush", REG, REG, _) \
	X(op_tab, "tab", REG, _, _) \
	/* global access */ \
	X(op_gget, "gget", REG, REG, _) \
	X(op_gset, "gset", REG, REG, _) \
	/* field access */ \
	X(op_fget, "fget", REG, REG, REG) \
	X(op_fset, "fset", REG, REG, REG) \
	/* upvalue access */ \
	X(op_uget, "uget", REG, IMM8, _) \
	X(op_uset, "uset", IMM8, REG, _) \
	/* control flow */ \
	X(op_j, "j", _, REL, _) \
	X(op_jtrue, "jtrue", REG, REL, _) \
	X(op_jfalse, "jfalse", REG, REL, _) \
	/* function */ \
	X(op_func, "func", REG, IMMFUNC, _) \
	X(op_close, "close", REG, _, _) \
	X(op_call, "call", REG, IMM8, _) \
	X(op_ret, "ret", REG, _, _) \
	X(op_retu, "retu", _, _, _)

#define X(a, b, c, d, e) a,
enum opcode {
	OPCODE_DEF(X)
};
#undef X

struct ins {
	enum opcode opcode : 8;
	int op1 : 8;
	union {
		struct {
			int op2 : 8;
			int op3 : 8;
		};
		int imm : 16;
	};
};

struct updef {
	uint8_t in_stack;
	uint8_t idx;
};

struct code {
	int nargs;
	int enclosure;
	/* instructions */
	int ins_cnt, ins_cap;
	struct ins* ins;
	/* constants */
	int k_cnt, k_cap;
	struct value* k;
	/* upvalues */
	int upval_cnt, upval_cap;
	struct updef* upval;
};

struct upval {
	OBJ_HEADER;
	struct value* val;
	union {
		/* open field */
		struct {
			struct upval* prev;
			struct upval* next;
		};
		/* close field */
		struct value val_holder;
	};
};

struct funcobj {
	OBJ_HEADER;
	struct code* code;
	struct upval* upval[];
};

#define WIDTH	160
#define HEIGHT	144

#define STRLIT_DEF(X) \
	X(false) \
	X(global) \
	X(indexOf) \
	X(lastIndexOf) \
	X(length) \
	X(null) \
	X(onframe) \
	X(pop) \
	X(push) \
	X(substr) \
	X(true) \
	X(undefined)

struct gfx {
	uint8_t screen[WIDTH * HEIGHT / 2];
	int color; /* foreground color */
	int cx, cy; /* cursor location */
	int cam_x, cam_y; /* camera location */
};

struct cpu {
	struct alloc* alloc;
	/* functions */
	int code_cnt, code_cap;
	struct code* code;

	/* main function object */
	struct funcobj* topfunc;
	int parent;
	int top_executed;
	int stopped;

	/* interned string hash table */
	struct strobj** strtab;
	int strtab_size, strtab_cnt;

	/* interned string literals */
	struct strobj* _lit_EMPTY;
#define X(s) struct strobj* _lit_##s;
	STRLIT_DEF(X)
#undef X

	/* global table object (strong ref) */
	struct tabobj* globals;

	/* garbage collection */
	struct obj* gchead;

	/* stack */
	int sp, stack_cap;
	struct value* stack;

	/* points to first open upvalue (weak ref) */
	struct upval* upval_open;

	/* frame counter */
	int frame;

	/* gfx state */
	struct gfx gfx;

	/* prng state */
	uint32_t prng_s[4];
};

#define MAX_INPUT	16
struct io {
	/* mouse/key */
	uint8_t prev_keys[key_cnt];
	uint8_t keys[key_cnt];
	char input[MAX_INPUT];
	int input_size;
	enum key last_key;
	int last_key_repeat;
	int mousex, mousey;
	int mousewheel;
};

#define RET			cpu->stack[sp]
#define ARG(i)		cpu->stack[sp + 2 + (i)]
#define THIS		cpu->stack[sp + 1]

NORETURN void runtime_error(struct cpu* cpu, const char* msg);
NORETURN void argument_error(struct cpu* cpu);
NORETURN void out_of_memory_error(struct cpu* cpu);
struct cpu* cpu_new();
void cpu_destroy(struct cpu* cpu);
void cpu_execute(struct cpu* cpu, struct funcobj* func);
int cpu_dump_code(struct cpu* cpu, char* buf, int buflen);

int to_bool(struct cpu* cpu, struct value val);
number to_number(struct cpu* cpu, struct value val);
struct strobj* to_string(struct cpu* cpu, struct value val);
struct arrobj* to_arr(struct cpu* cpu, struct value val);
struct tabobj* to_tab(struct cpu* cpu, struct value val);
void func_destroy(struct cpu* cpu, struct funcobj* func);
void upval_destroy(struct cpu* cpu, struct upval* upval);

#endif
