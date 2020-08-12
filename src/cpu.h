#ifndef _CPU_H
#define _CPU_H

#include "alloc.h"
#include "arith.h"
#include "cfunc.h"
#include "config.h"
#include "key.h"

#include <stdint.h>

#define MAX_STACKTRACE		10
#define SPRITE_WIDTH		8
#define SPRITE_HEIGHT		8
#define SPRITESHEET_WIDTH	64
#define SPRITESHEET_HEIGHT	256
#define SPRITESHEET_BYTES	(SPRITESHEET_WIDTH * SPRITESHEET_HEIGHT / 2)
#define MAX_CODE_SIZE		65535
#define SYM_MAX_LEN			256
#define MAX_K				65535
#define MAX_KOP				255

#define forcereadptr(aptr)	(((aptr) == 0) ? NULL : (void*)((uint8_t*)cpu + (aptr)))
#define forcewriteptr(aptr)	(((aptr) == NULL) ? 0 : (uint32_t)((uint8_t*)(aptr) - (uint8_t*)cpu))
#ifdef RELATIVE_ADDRESSING
typedef uint32_t ptr_t;
#define readptr(aptr)		forcereadptr(aptr)
#define writeptr(aptr)		forcewriteptr(aptr)
#define ptr(type)			ptr_t
#else
typedef void* ptr_t;
#define readptr(aptr)		(aptr)
#define writeptr(aptr)		(aptr)
#define ptr(type)			type*
#endif

struct cpu;

#define OBJ_HEADER	ptr(struct obj) gcnext; enum type type

enum type {
	t_undef,
	t_null,
	t_bool,
	t_num,
	t_str,
	t_striter,
	t_buf,
	t_arr,
	t_arriter,
	t_tab,
	t_func,
	t_cfunc,
	t_callinfo1,
	t_callinfo2,
	t_upval,
	t_cnt,
};

struct callinfo1 {
	ptr(struct funcobj) func;
};

struct callinfo2 {
	uint8_t stack_size;
	uint16_t pc;
};

struct value {
	enum type type;
	union {
		int b;
		number num;
		ptr(struct obj) obj;
		ptr(struct strobj) str;
		ptr(struct striterobj) striter;
		ptr(struct bufobj) buf;
		ptr(struct arrobj) arr;
		ptr(struct arriterobj) arriter;
		ptr(struct tabobj) tab;
		ptr(struct funcobj) func;
		enum cfuncname cfunc;
		struct callinfo1 ci1;
		struct callinfo2 ci2;
	};
};

struct obj {
	OBJ_HEADER;
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
	OBJ_HEADER;
	uint32_t len, cap;
	ptr(struct value) data;
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
	struct value value;
	uint16_t next;
};

struct tabobj {
	OBJ_HEADER;
	int entry_cnt;
	ptr(struct tabent) entry;
	uint16_t freelist;
	int bucket_cnt;
	ptr(uint16_t) bucket;
};

#define OPCODE_DEF(X) \
	X(op_kundef, "kundef", REG, _, _) \
	X(op_knull, "knull", REG, _, _) \
	X(op_kfalse, "kfalse", REG, _, _) \
	X(op_ktrue, "ktrue", REG, _, _) \
	X(op_knum, "knum", REG, IMMNUM, _) \
	X(op_kstr, "kstr", REG, IMMSTR, _) \
	X(op_mov, "mov", REG, REG, _) \
	X(op_xchg, "xchg", REG, REG, _) \
	/* arithmetic */ \
	X(op_add, "add", REG, REG, REG) \
	X(op_addrn, "add", REG, REG, NUM) \
	X(op_addnr, "add", REG, NUM, REG) \
	X(op_sub, "sub", REG, REG, REG) \
	X(op_subrn, "sub", REG, REG, NUM) \
	X(op_subnr, "sub", REG, NUM, REG) \
	X(op_mul, "mul", REG, REG, REG) \
	X(op_mulrn, "mul", REG, REG, NUM) \
	X(op_mulnr, "mul", REG, NUM, REG) \
	X(op_div, "div", REG, REG, REG) \
	X(op_divrn, "div", REG, REG, NUM) \
	X(op_divnr, "div", REG, NUM, REG) \
	X(op_mod, "mod", REG, REG, REG) \
	X(op_modrn, "mod", REG, REG, NUM) \
	X(op_modnr, "mod", REG, NUM, REG) \
	X(op_exp, "exp", REG, REG, REG) \
	X(op_exprn, "exp", REG, REG, NUM) \
	X(op_expnr, "exp", REG, NUM, REG) \
	X(op_inc, "inc", REG, REG, _) \
	X(op_dec, "dec", REG, REG, _) \
	X(op_plus, "plus", REG, REG, _) \
	X(op_neg, "neg", REG, REG, _) \
	/* logical */ \
	X(op_not, "not", REG, REG, _) \
	/* bitwise logical */ \
	X(op_band, "band", REG, REG, REG) \
	X(op_bandrn, "band", REG, REG, NUM) \
	X(op_bandnr, "band", REG, NUM, REG) \
	X(op_bor, "bor", REG, REG, REG) \
	X(op_borrn, "bor", REG, REG, NUM) \
	X(op_bornr, "bor", REG, NUM, REG) \
	X(op_bxor, "bxor", REG, REG, REG) \
	X(op_bxorrn, "bxor", REG, REG, NUM) \
	X(op_bxornr, "bxor", REG, NUM, REG) \
	X(op_bnot, "bnot", REG, REG, _) \
	X(op_shl, "shl", REG, REG, REG) \
	X(op_shlrn, "shl", REG, REG, NUM) \
	X(op_shlnr, "shl", REG, NUM, REG) \
	X(op_shr, "shr", REG, REG, REG) \
	X(op_shrrn, "shr", REG, REG, NUM) \
	X(op_shrnr, "shr", REG, NUM, REG) \
	X(op_ushr, "ushr", REG, REG, REG) \
	X(op_ushrrn, "ushr", REG, REG, NUM) \
	X(op_ushrnr, "ushr", REG, NUM, REG) \
	/* comparison */ \
	X(op_eq, "eq", REG, REG, REG) \
	X(op_eqrn, "eq", REG, REG, NUM) \
	X(op_eqnr, "eq", REG, NUM, REG) \
	X(op_ne, "ne", REG, REG, REG) \
	X(op_nern, "ne", REG, REG, NUM) \
	X(op_nenr, "ne", REG, NUM, REG) \
	X(op_lt, "lt", REG, REG, REG) \
	X(op_ltrn, "lt", REG, REG, NUM) \
	X(op_ltnr, "lt", REG, NUM, REG) \
	X(op_le, "le", REG, REG, REG) \
	X(op_lern, "le", REG, REG, NUM) \
	X(op_lenr, "le", REG, NUM, REG) \
	X(op_gt, "gt", REG, REG, REG) \
	X(op_gtrn, "gt", REG, REG, NUM) \
	X(op_gtnr, "gt", REG, NUM, REG) \
	X(op_ge, "ge", REG, REG, REG) \
	X(op_gern, "ge", REG, REG, NUM) \
	X(op_genr, "ge", REG, NUM, REG) \
	X(op_in, "in", REG, REG, REG) \
	/* object creation */ \
	X(op_arr, "arr", REG, _, _) \
	X(op_apush, "apush", REG, REG, _) \
	X(op_tab, "tab", REG, _, _) \
	/* global access */ \
	X(op_gget, "gget", REG, REG, _) \
	X(op_ggets, "gget", REG, STR, _) \
	X(op_gset, "gset", REG, REG, _) \
	X(op_gsets, "gset", STR, REG, _) \
	/* field access */ \
	X(op_fget, "fget", REG, REG, REG) \
	X(op_fgets, "fget", REG, REG, STR) \
	X(op_fgetn, "fget", REG, REG, NUM) \
	X(op_fset, "fset", REG, REG, REG) \
	X(op_fsets, "fset", REG, STR, REG) \
	X(op_fsetn, "fset", REG, NUM, REG) \
	/* upvalue access */ \
	X(op_uget, "uget", REG, IMM8, _) \
	X(op_uset, "uset", IMM8, REG, _) \
	/* iterator */ \
	X(op_iter, "iter", REG, REG, _) \
	X(op_next, "next", REG, REG, REG) \
	/* control flow */ \
	X(op_j, "j", _, REL, _) \
	X(op_closej, "closej", REG, REL, _) \
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
	unsigned int op1 : 8;
	union {
		struct {
			unsigned int op2 : 8;
			unsigned int op3 : 8;
		};
		int imm : 16;
	};
};

struct updef {
	uint8_t in_stack;
	uint8_t idx;
};

/* Line info VM format
 * Every byte is a command in a simple line info state machine.
 * The state machine has two state: instruction pointer and line pointer.
 * Then it executes a series of VM commands to determine which source code line.
 * each instruction corresponds to.
 * 0uuuuuuu:  Instruction delta: add current instruction pointer by delta.
 * All instructions in this range maps to current line pointer.
 * 1uuuuuuu:  Line delta: add current line pointer by delta.
 * Deltas larger than 7-bits are simply splitted into multiple independent instructions,
 * no complex variable-byte encoding are used as this should not happen often in practice.
 */
enum licmdtype {
	li_ins,
	li_line,
};

struct licmd {
	unsigned int delta : 7;
	unsigned int type : 1;
};

struct code {
	int nargs;
	int enclosure;
	ptr(struct strobj) name;
	/* instructions */
	int ins_cnt, ins_cap;
	ptr(struct ins) ins;
	/* line info */
	int lineinfo_cnt, lineinfo_cap;
	ptr(struct licmd) lineinfo;
	/* constants */
	int k_cnt, k_cap;
	ptr(uint32_t) k;
	/* upvalues */
	int upval_cnt, upval_cap;
	ptr(struct updef) upval;
};

struct upval {
	OBJ_HEADER;
	ptr(struct value) val;
	union {
		/* open field */
		struct {
			ptr(struct upval) prev;
			ptr(struct upval) next;
		};
		/* close field */
		struct value val_holder;
	};
};

struct funcobj {
	OBJ_HEADER;
	ptr(struct code) code;
	ptr(struct upval) upval[];
};

#define CPU_MEM_SIZE	1048576
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
	X(slice) \
	X(substr) \
	X(true) \
	X(undefined)

struct gfx {
	uint8_t screen[WIDTH * HEIGHT / 2];
	uint8_t sprite[SPRITESHEET_BYTES];
	int color; /* foreground color */
	int cx, cy; /* cursor location */
	int cam_x, cam_y; /* camera location */
	uint8_t pal[16];
	uint8_t palt[16];
};

struct cpu {
	struct alloc alloc;

	/* code objects */
	int code_cnt, code_cap;
	ptr(struct code) code;

	/* main function object */
	ptr(struct funcobj) topfunc;
	int parent;

	/* execution state */
	int top_executed;
	int cycles;
	int paused;
	int stopped;
	ptr(struct funcobj) curfunc;
	int curpc;

	/* interned string hash table */
	ptr(ptr(struct strobj)) strtab;
	int strtab_size, strtab_cnt;

	/* interned string literals */
	ptr(struct strobj) _lit_EMPTY;
#define X(s) ptr(struct strobj) _lit_##s;
	STRLIT_DEF(X)
#undef X

	/* global table object (strong ref) */
	ptr(struct tabobj) globals;

	/* garbage collection */
	ptr(struct obj) gchead;

	/* stack additional info */
	int sp, stack_cap;
	ptr(struct value) stack;

	/* points to first open upvalue (weak ref) */
	ptr(struct upval) upval_open;

	/* frame counter */
	int completed_frames;
	int delayed_frames;
	int last_delayed_frames;

	/* gfx state */
	struct gfx gfx;

	/* prng state */
	uint32_t prng_s[4];
};

#define MAX_INPUT_CHARS	16
struct io {
	/* mouse/key */
	uint8_t prev_keys[kc_cnt];
	uint8_t keys[kc_cnt];
	char input[MAX_INPUT_CHARS];
	int input_size;
	enum key last_key;
	int last_key_repeat;
	int mousex, mousey;
	int mousewheel;
};

#define ARG(i)		((struct value*)readptr(cpu->stack))[sp + 2 + (i)]
#define THIS		((struct value*)readptr(cpu->stack))[sp + 1]
#define LIT(x)		((struct strobj*)readptr(cpu->_lit_##x))

NORETURN void runtime_error(struct cpu* cpu, const char* msg);
NORETURN void argument_error(struct cpu* cpu);
NORETURN void out_of_memory_error(struct cpu* cpu);
struct cpu* cpu_new();
void cpu_destroy(struct cpu* cpu);
void cpu_execute(struct cpu* cpu, struct funcobj* func);
void cpu_continue(struct cpu* cpu);
int cpu_dump_code(struct cpu* cpu, const char* codebuf, int codelen, char* buf, int buflen);

int to_bool(struct cpu* cpu, struct value val);
number to_number(struct cpu* cpu, struct value val);
struct strobj* to_string(struct cpu* cpu, struct value val);
struct arrobj* to_arr(struct cpu* cpu, struct value val);
struct tabobj* to_tab(struct cpu* cpu, struct value val);
void func_destroy(struct cpu* cpu, struct funcobj* func);
void upval_destroy(struct cpu* cpu, struct upval* upval);

static inline struct value value_undef(struct cpu* cpu) {
	struct value val;
	val.type = t_undef;
	return val;
}

static inline struct value value_null(struct cpu* cpu) {
	struct value val;
	val.type = t_null;
	return val;
}

static inline struct value value_bool(struct cpu* cpu, int b) {
	struct value val;
	val.type = t_bool;
	val.b = b;
	return val;
}

static inline struct value value_num(struct cpu* cpu, number num) {
	struct value val;
	val.type = t_num;
	val.num = num;
	return val;
}

static inline struct value value_str(struct cpu* cpu, struct strobj* str) {
	struct value val;
	val.type = t_str;
	val.str = writeptr(str);
	return val;
}

static inline struct value value_striter(struct cpu* cpu, struct striterobj* striter) {
	struct value val;
	val.type = t_striter;
	val.striter = writeptr(striter);
	return val;
}

static inline struct value value_buf(struct cpu* cpu, struct bufobj* buf) {
	struct value val;
	val.type = t_buf;
	val.buf = writeptr(buf);
	return val;
}

static inline struct value value_arr(struct cpu* cpu, struct arrobj* arr) {
	struct value val;
	val.type = t_arr;
	val.arr = writeptr(arr);
	return val;
}

static inline struct value value_arriter(struct cpu* cpu, struct arriterobj* arriter) {
	struct value val;
	val.type = t_arriter;
	val.arriter = writeptr(arriter);
	return val;
}

static inline struct value value_tab(struct cpu* cpu, struct tabobj* tab) {
	struct value val;
	val.type = t_tab;
	val.tab = writeptr(tab);
	return val;
}

static inline struct value value_func(struct cpu* cpu, struct funcobj* func) {
	struct value val;
	val.type = t_func;
	val.func = writeptr(func);
	return val;
}

static inline struct value value_cfunc(struct cpu* cpu, enum cfuncname cfunc) {
	struct value val;
	val.type = t_cfunc;
	val.cfunc = cfunc;
	return val;
}

/* General CPU instruction cycles design
 * Each instruction executed costs 1 base cycle
 * - Memory allocation costs 2 additional cycles
 * - Array lookup costs 1 additional cycles
 * - Table lookup costs 3 additional cycles (including potential memory allocation when setting new values)
 * - Traversing 4 upvalues costs 1 additional cycle
 * - Drawing 8 pixels costs 1 additional cycle
 * - Processing 4 string characters costs 1 additional cycle
 * - Copying 1 value costs 1 additional cycle
 * - Any cart IO related functions costs 16384 additional cycles
 * string to number conversion anywhere costs 1 additional cycle
 * number/bool to string conversion anywhere costs 1 plus memory allocation cycles
 * Library functions define their own cycle counts
 */
#define CYCLES_BASE			1
#define CYCLES_ALLOC		2
#define CYCLES_ARRAY_LOOKUP	1
#define CYCLES_LOOKUP		3
#define UPVALUES_PER_CYCLE	4
#define CYCLES_UPVALUES(x)	(((x) + UPVALUES_PER_CYCLE - 1) / UPVALUES_PER_CYCLE)
#define PIXELS_PER_CYCLE	8
#define CYCLES_PIXELS(x)	(((x) + PIXELS_PER_CYCLE - 1) / PIXELS_PER_CYCLE)
#define CHARS_PER_CYCLE		4
#define CYCLES_CHARS(x)		(((x) + CHARS_PER_CYCLE - 1) / CHARS_PER_CYCLE)
#define VALUES_PER_CYCLE	1
#define CYCLES_VALUES(x)	(((x) + VALUES_PER_CYCLE - 1) / VALUES_PER_CYCLE)
#define CYCLES_CARTIO		16384
#define CYCLES_STR2NUM		1
#define CYCLES_NUM2STR		(CYCLES_ALLOC + 1)

/* How many cycles can be run in a frame */
#define CYCLES_PER_FRAME	139810
/* 139,810 * 60 fps ~= 8 MHz */

#endif
