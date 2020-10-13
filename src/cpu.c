#include "arr.h"
#include "arith.h"
#include "assetmap.h"
#include "buf.h"
#include "cpu.h"
#include "gc.h"
#include "gfx.h"
#include "lib.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

static jmp_buf g_jmp_buf;

static NOINLINE void print_name(struct cpu* cpu, struct code* code) {
	struct gfx* gfx = console_getgfx();
	if (code->enclosure > 0) {
		print_name(cpu, &((struct code*)readptr(cpu->code))[code->enclosure]);
		gfx_print_simple(gfx, ":", 1, 14);
	}
	struct strobj* name = (struct strobj*)readptr_nullable(code->name);
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
	longjmp(g_jmp_buf, 1);
}

static NOINLINE void print_stack_trace(struct cpu* cpu, struct code* code, int pc) {
	struct gfx* gfx = console_getgfx();
	gfx_print(gfx, "Stack trace:", 12, -1, -1, 14);
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

		value_t* frame = &((value_t*)readptr(cpu->stack))[sp];
		struct funcobj* func = (struct funcobj*)value_get_object(frame[2 + code->nargs]);
		value_t ci = frame[3 + code->nargs];
		pc = value_get_ci_pc(ci);
		sp -= value_get_ci_stacksize(ci);
		code = (struct code*)readptr(func->code);
	}
	if (sp != 0)
		gfx_print_simple(gfx, "...", 3, 14);
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

struct cpu* cpu_new(int pid) {
	struct cpu* cpu = (struct cpu*)mem_new(CPU_MEM_SIZE, sizeof(struct cpu));
	if (cpu == NULL)
		return NULL;
	cpu->gchead = writeptr_nullable(NULL);
	cpu->grayhead = writeptr_nullable(NULL);
	cpu->sweephead = 0;
	cpu->sweepcur = writeptr_nullable(NULL);
	cpu->gcstate = gs_reset;
	cpu->gcwhite = 0;
	cpu->parent = -1;
	cpu->cycles = 0;
	cpu->top_executed = 0;
	cpu->paused = 0;
	cpu->stopped = 0;
	strtab_init(cpu);
	struct tabobj* globals = tab_new(cpu);
	cpu->globals = writeptr(globals);
	cpu->overlay_state = writeptr_nullable(NULL);
	cpu->code = writeptr_nullable(NULL);
	cpu->code_cnt = 0;
	cpu->code_cap = 0;
	cpu->stack = writeptr_nullable(NULL);
	cpu->sp = 0;
	cpu->stack_cap = 0;
	cpu->upval_open = writeptr_nullable(NULL);
	cpu->delayed_frames = 0;
	cpu->last_delayed_frames = 0;

	cpu->_lit_EMPTY = writeptr(str_intern_nogc(cpu, "", 0));
#define X(s) cpu->_lit_##s = writeptr(str_intern_nogc(cpu, #s, (sizeof #s) - 1));
	STRLIT_DEF(X);
#undef X
	lib_init(cpu);
	gfx_init(&cpu->gfx);
	tab_set(cpu, globals, LIT(global), value_tab(globals));

	struct bufobj* vmem = buf_new_vmem(cpu, pid);
	tab_set(cpu, globals, str_intern(cpu, "VMEM", 4), value_buf(vmem));
	struct bufobj* overlay_vmem = buf_new_overlayvmem(cpu);
	tab_set(cpu, globals, str_intern(cpu, "OVMEM", 5), value_buf(overlay_vmem));

	return cpu;
}

void cpu_destroy(struct cpu* cpu) {
	mem_destroy(&cpu->alloc);
}

static void cpu_growstack(struct cpu* cpu) {
	int new_cap = cpu->stack_cap == 0 ? 1024 : cpu->stack_cap * 2;
	value_t* old_stack = (value_t*)readptr_nullable(cpu->stack);
	value_t* new_stack = (value_t*)mem_realloc(&cpu->alloc, old_stack, new_cap * sizeof(value_t));
	cpu->stack = writeptr(new_stack);
	cpu->stack_cap = new_cap;
	/* adjust open upvalues */
	for (struct upval* val = readptr_nullable(cpu->upval_open); val; val = readptr(val->next))
		val->val = writeptr((value_t*)readptr(val->val) - old_stack + new_stack);
}

FORCEINLINE int to_bool(struct cpu* cpu, value_t val) {
	if (value_is_num(val))
		return value_get_num(val) != 0;
	switch (value_get_type(val)) {
	case t_undef: return 0;
	case t_null: return 0;
	case t_bool: return value_get_bool(val);
	case t_str: return ((struct strobj*)value_get_object(val))->len != 0;
	default: return 1;
	}
}

FORCEINLINE number to_number(struct cpu* cpu, value_t val) {
	if (likely(value_is_num(val)))
		return value_get_num(val);
	switch (value_get_type(val)) {
	case t_null: return num_kint(0);
	case t_bool: return num_kint(value_get_bool(val));
	case t_str: {
		number num;
		struct strobj* str = (struct strobj*)value_get_object(val);
		if (!num_parse(str->data, str->data + str->len, &num))
			runtime_error(cpu, "Invalid number.");
		cpu->cycles -= CYCLES_STR2NUM;
		return num;
	}
	default: runtime_error(cpu, "Cannot convert to number.");
	}
}

static FORCEINLINE struct strobj* num_to_str(struct cpu* cpu, number num) {
	char buf[20];
	int len = num_format(num, 4, buf);
	cpu->cycles -= CYCLES_NUM2STR;
	return str_intern(cpu, buf, len);
}

FORCEINLINE struct strobj* to_string(struct cpu* cpu, value_t val) {
	if (value_is_num(val))
		return num_to_str(cpu, value_get_num(val));
	switch (value_get_type(val)) {
	case t_undef: return LIT(undefined);
	case t_null: return LIT(null);
	case t_bool: {
		if (value_get_bool(val))
			return LIT(true);
		else
			return LIT(false);
	}
	case t_str: return (struct strobj*)value_get_object(val);
	default: runtime_error(cpu, "Cannot convert to string.");
	}
}

FORCEINLINE struct arrobj* to_arr(struct cpu* cpu, value_t val) {
	if (unlikely(value_get_type(val) != t_arr))
		runtime_error(cpu, "Not an array.");
	return (struct arrobj*)value_get_object(val);
}

FORCEINLINE struct tabobj* to_tab(struct cpu* cpu, value_t val) {
	if (unlikely(value_get_type(val) != t_tab))
		runtime_error(cpu, "Not an object.");
	return (struct tabobj*)value_get_object(val);
}

struct assetmapobj* to_assetmap(struct cpu* cpu, value_t val) {
	if (unlikely(value_get_type(val) != t_assetmap))
		runtime_error(cpu, "Not an asset map object.");
	return (struct assetmapobj*)value_get_object(val);
}

static FORCEINLINE int strict_equal(struct cpu* cpu, value_t lval, value_t rval) {
	return lval == rval;
}

static FORCEINLINE int strict_equal_num(struct cpu* cpu, value_t lval, number num) {
	return value_get_num(lval) == num;
}

static FORCEINLINE struct strobj* get_typeof(struct cpu* cpu, value_t val) {
	if (value_is_num(val))
		return LIT(number);
	switch (value_get_type(val)) {
	case t_undef: return LIT(undefined);
	case t_bool: return LIT(boolean);
	case t_str: return LIT(string);
	case t_func: return LIT(function);
	default: return LIT(object);
	}
}

static FORCEINLINE value_t fget(struct cpu* cpu, value_t obj, value_t field) {
	switch (value_get_type(obj)) {
	case t_str: {
		struct strobj* str = (struct strobj*)value_get_object(obj);
		if (likely(value_is_num(field))) {
			cpu->cycles -= CYCLES_ALLOC;
			return str_get(cpu, str, value_get_num(field));
		}
		else {
			cpu->cycles -= CYCLES_LOOKUP;
			return str_fget(cpu, str, to_string(cpu, field));
		}
	}
	case t_buf: {
		struct bufobj* buf = (struct bufobj*)value_get_object(obj);
		if (likely(value_is_num(field))) {
			cpu->cycles -= CYCLES_ARRAY_LOOKUP;
			return buf_get(cpu, buf, value_get_num(field));
		}
		else {
			cpu->cycles -= CYCLES_LOOKUP;
			return buf_fget(cpu, buf, to_string(cpu, field));
		}
	}
	case t_arr: {
		struct arrobj* arr = (struct arrobj*)value_get_object(obj);
		if (likely(value_is_num(field))) {
			cpu->cycles -= CYCLES_ARRAY_LOOKUP;
			return arr_get(cpu, arr, value_get_num(field));
		}
		else {
			cpu->cycles -= CYCLES_LOOKUP;
			return arr_fget(cpu, arr, to_string(cpu, field));
		}
	}
	case t_tab: {
		struct tabobj* tab = (struct tabobj*)value_get_object(obj);
		cpu->cycles -= CYCLES_LOOKUP;
		return tab_get(cpu, tab, to_string(cpu, field));
	}
	case t_assetmap: {
		struct assetmapobj* map = (struct assetmapobj*)value_get_object(obj);
		cpu->cycles -= CYCLES_LOOKUP;
		return assetmap_fget(cpu, map, to_string(cpu, field));
	}
	default: runtime_error(cpu, "Not an object.");
	}
}

static FORCEINLINE value_t fgetnum(struct cpu* cpu, value_t obj, number field) {
	switch (value_get_type(obj)) {
	case t_str: {
		cpu->cycles -= CYCLES_ARRAY_LOOKUP;
		return str_get(cpu, (struct strobj*)value_get_object(obj), field);
	}
	case t_buf: {
		cpu->cycles -= CYCLES_ARRAY_LOOKUP;
		return buf_get(cpu, (struct bufobj*)value_get_object(obj), field);
	}

	case t_arr: {
		cpu->cycles -= CYCLES_ARRAY_LOOKUP;
		return arr_get(cpu, (struct arrobj*)value_get_object(obj), field);
	}

	case t_tab: {
		cpu->cycles -= CYCLES_LOOKUP;
		return tab_get(cpu, (struct tabobj*)value_get_object(obj), num_to_str(cpu, field));
	}

	case t_assetmap:
		return value_undef();

	default: runtime_error(cpu, "Not an object.");
	}
}

static FORCEINLINE value_t fgetstr(struct cpu* cpu, value_t obj, struct strobj* field) {
	cpu->cycles -= CYCLES_LOOKUP;
	switch (value_get_type(obj)) {
	case t_str: return str_fget(cpu, (struct strobj*)value_get_object(obj), field);
	case t_buf: return buf_fget(cpu, (struct bufobj*)value_get_object(obj), field);
	case t_arr: return arr_fget(cpu, (struct arrobj*)value_get_object(obj), field);
	case t_tab: return tab_get(cpu, (struct tabobj*)value_get_object(obj), field);
	case t_assetmap: return assetmap_fget(cpu, (struct assetmapobj*)value_get_object(obj), field);
	default: runtime_error(cpu, "Not an object.");
	}
}

static FORCEINLINE void fset(struct cpu* cpu, value_t obj, value_t field, value_t value) {
	switch (value_get_type(obj)) {
	case t_str: runtime_error(cpu, "Cannot set string element.");
	case t_buf: buf_set(cpu, (struct bufobj*)value_get_object(obj), to_number(cpu, field), value); cpu->cycles -= CYCLES_ARRAY_LOOKUP; break;
	case t_arr: arr_set(cpu, (struct arrobj*)value_get_object(obj), to_number(cpu, field), value); cpu->cycles -= CYCLES_ARRAY_LOOKUP; break;
	case t_tab: tab_set(cpu, (struct tabobj*)value_get_object(obj), to_string(cpu, field), value); cpu->cycles -= CYCLES_LOOKUP;  break;
	case t_assetmap: runtime_error(cpu, "Setting immutable object.");
	default: runtime_error(cpu, "Not an object.");
	}
}

static FORCEINLINE void fsetn(struct cpu* cpu, value_t obj, number field, value_t value) {
	switch (value_get_type(obj)) {
	case t_str: runtime_error(cpu, "Cannot set string element.");
	case t_buf: buf_set(cpu, (struct bufobj*)value_get_object(obj), field, value); cpu->cycles -= CYCLES_ARRAY_LOOKUP; break;
	case t_arr: arr_set(cpu, (struct arrobj*)value_get_object(obj), field, value); cpu->cycles -= CYCLES_ARRAY_LOOKUP; break;
	case t_tab: tab_set(cpu, (struct tabobj*)value_get_object(obj), num_to_str(cpu, field), value); cpu->cycles -= CYCLES_LOOKUP; break;
	case t_assetmap: runtime_error(cpu, "Setting immutable object.");
	default: runtime_error(cpu, "Not an object.");
	}
}

static FORCEINLINE void fsets(struct cpu* cpu, value_t obj, struct strobj* field, value_t value) {
	switch (value_get_type(obj)) {
	case t_str:
	case t_buf:
	case t_arr:
		runtime_error(cpu, "Can only set string field on tables.");

	case t_tab:
		tab_set(cpu, (struct tabobj*)value_get_object(obj), field, value);
		cpu->cycles -= CYCLES_LOOKUP;
		break;

	case t_assetmap: runtime_error(cpu, "Setting immutable object.");
	default: runtime_error(cpu, "Not an object.");
	}
}

static void upval_unlink(struct cpu* cpu, struct upval* val) {
	struct upval* prev = readptr_nullable(val->prev);
	if (prev)
		prev->next = val->next;
	else
		cpu->upval_open = val->next;
	struct upval* next = readptr_nullable(val->next);
	if (next)
		next->prev = val->prev;
}

static void close_upvals(struct cpu* cpu, value_t* frame, int base) {
	struct upval* val;
	int cnt = 0;
	for (val = readptr_nullable(cpu->upval_open); val && (value_t*)readptr(val->val) >= frame;) {
		value_t* v = readptr(val->val);
		struct upval* next = readptr_nullable(val->next);
		if (v - frame >= base) {
			upval_unlink(cpu, val);
			val->val_holder = *v;
			val->val = writeptr(&val->val_holder);
		}
		val = next;
		cnt++;
	}
	cpu->cycles -= CYCLES_UPVALUES(cnt);
}

static value_t get_iter(struct cpu* cpu, value_t val) {
	switch (value_get_type(val)) {
	case t_str: {
		struct strobj* str = (struct strobj*)value_get_object(val);
		struct striterobj* striter = (struct striterobj*)gc_alloc(cpu, t_striter, sizeof(struct striterobj));
		striter->str = writeptr(str);
		striter->i = 0;
		striter->end = str->len;
		return value_striter(striter);
	}

	case t_arr: {
		struct arrobj* arr = (struct arrobj*)value_get_object(val);
		struct arriterobj* arriter = (struct arriterobj*)gc_alloc(cpu, t_arriter, sizeof(struct arriterobj));
		arriter->arr = writeptr(arr);
		arriter->i = 0;
		arriter->end = arr->len;
		return value_arriter(arriter);
	}

	default:
		runtime_error(cpu, "The object does not have built-in iterator support.");
	}
}

static int iter_next(struct cpu* cpu, value_t iterval, value_t* val) {
	switch (value_get_type(iterval)) {
	case t_striter: {
		struct striterobj* striter = (struct striterobj*)value_get_object(iterval);
		struct strobj* str = (struct strobj*)readptr(striter->str);
		if (striter->i == -1 || striter->i >= str->len) {
			striter->i = -1;
			*val = value_undef();
			return 1;
		}
		else {
			*val = value_str(str_intern(cpu, &str->data[striter->i], 1));
			if (++striter->i == str->len)
				striter->i = -1;
			return 0;
		}
	}
	case t_arriter: {
		struct arriterobj* arriter = (struct arriterobj*)value_get_object(iterval);
		struct arrobj* arr = (struct arrobj*)readptr(arriter->arr);
		if (arriter->i == -1 || arriter->i >= arr->len) {
			arriter->i = -1;
			*val = value_undef();
			return 1;
		}
		else {
			struct arrobj* arr = (struct arrobj*)readptr(arriter->arr);
			*val = ((value_t*)readptr(arr->data))[arriter->i];
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
	mem_dealloc(&cpu->alloc, func);
}

void upval_destroy(struct cpu* cpu, struct upval* upval) {
	/* Open upval ? unlink it */
	if (readptr(upval->val) != &upval->val_holder)
		upval_unlink(cpu, upval);
	mem_dealloc(&cpu->alloc, upval);
}

#define update_stack()	do { \
	if (unlikely(cpu->sp + 256 > cpu->stack_cap)) \
		cpu_growstack(cpu); \
		frame = &((value_t*)readptr(cpu->stack))[cpu->sp]; \
	} while (0)

#define iopcode		OPCODE(ins)
#define iop1		OP1(ins)
#define iop2		OP2(ins)
#define iop3		OP3(ins)
#define iimm		IMM(ins)
#define retval		frame[iop1]
#define lval		frame[iop2]
#define rval		frame[iop3]
#define retvalbool	to_bool(cpu, retval)
#define lvalbool	to_bool(cpu, lval)
#define retvalnum	to_number(cpu, retval)
#define lvalnum		to_number(cpu, lval)
#define rvalnum		to_number(cpu, rval)
#define retvalstr	to_string(cpu, retval)
#define lvalstr		to_string(cpu, lval)
#define rvalstr		to_string(cpu, rval)
#define lnum		((number)ktable[iop2])
#define rnum		((number)ktable[iop3])
#define retstr		forcereadptr(ktable[iop1])
#define lstr		forcereadptr(ktable[iop2])
#define rstr		forcereadptr(ktable[iop3])

#ifdef DEBUG_TIMING
static NOINLINE void cpu_timing_record(enum opcode opcode, int64_t duration);
#endif

void cpu_execute(struct cpu* cpu, struct funcobj* func, int nargs, ...) {
	value_t* frame;
	update_stack();
	value_t* stack = (value_t*)readptr(cpu->stack);
	stack[0] = value_func(func);
	stack[1] = value_undef(); // this
	int fnargs = ((struct code*)readptr(func->code))->nargs;
	if (fnargs < nargs)
		nargs = fnargs;
	va_list va;
	va_start(va, nargs);
	for (int i = 0; i < nargs; i++) {
		value_t arg = va_arg(va, value_t);
		stack[2 + i] = arg;
	}
	va_end(va);
	for (int i = nargs; i < fnargs; i++)
		stack[2 + i] = value_undef();
	stack[fnargs + 2] = value_undef(); // call info 1
	stack[fnargs + 3] = value_undef(); // call info 2
	cpu->curfunc = writeptr(func);
	cpu->curpc = 0;
	cpu->cycles = CYCLES_PER_FRAME;
	cpu_continue(cpu);
}

#if !defined(_MSC_VER)
#define USE_COMPUTED_GOTO
#endif

#if defined(DEBUG_TIMING)
#define COMPUTED_GOTO_DEBUG_TIMING_UPDATE() do { \
		MEASURE_END(); \
		cpu_timing_record(iopcode, MEASURE_DURATION()); \
		MEASURE_START(); \
	} while (0)
#else
#define COMPUTED_GOTO_DEBUG_TIMING_UPDATE()
#endif

#if defined(USE_COMPUTED_GOTO)
#define DISPATCH()	do { \
		COMPUTED_GOTO_DEBUG_TIMING_UPDATE(); \
		if (likely(cpu->cycles > 0)) { \
			cpu->cycles -= CYCLES_BASE; \
			ins = *pc++; \
			goto *local_dispatch_table[iopcode]; \
		} \
		cpu->dummy = __COUNTER__; \
		goto timeout; \
	} while (0)
#define CASE(op)	target_##op:
#else
#define DISPATCH()	break
#define CASE(op)	case op:
#endif

void cpu_continue(struct cpu* cpu) {
	value_t* frame;
	struct funcobj* func = (struct funcobj*)readptr(cpu->curfunc);
	struct code* code = NULL;
	uint32_t* ktable = NULL;
#ifdef ESP_PLATFORM
	register uint32_t* pc asm("a3") = NULL;
#else
	uint32_t* pc = NULL;
#endif
	if (setjmp(g_jmp_buf) != 0) {
		print_stack_trace(cpu, code, (uint16_t)(pc - (uint32_t*)readptr(code->ins)));
		cpu->stopped = 1;
		return;
	}
	code = (struct code*)readptr(func->code);
	ktable = (uint32_t*)readptr(code->k);
	pc = &((uint32_t*)readptr(code->ins))[cpu->curpc];
	update_stack();
#if defined(DEBUG_TIMING)
	MEASURE_DEFINES();
	enum opcode last_opcode = -1;
#endif
#if defined(USE_COMPUTED_GOTO)
#include "cpu_dispatch.h"
#ifdef ESP_PLATFORM
	/* Force it into a register */
	register void** local_dispatch_table asm("a2") = dispatch_table;
#else
	void** local_dispatch_table = dispatch_table;
#endif
#else
	for (;;) {
#endif
		uint32_t ins = 0;
#if defined(USE_COMPUTED_GOTO)
		DISPATCH();
		target_default: internal_error(cpu);
#else
#if defined(DEBUG_TIMING)
		MEASURE_END();
		if (likely(last_opcode != -1))
			cpu_timing_record(last_opcode, MEASURE_DURATION());
		MEASURE_START();
#endif
		if (unlikely(cpu->cycles <= 0))
			goto timeout;
		cpu->cycles -= CYCLES_BASE;
		ins = *pc++;
#if defined(DEBUG_TIMING)
		last_opcode = iopcode;
#endif
		switch (iopcode) {
		default: internal_error(cpu);
#endif
		CASE(op_kundef) retval = value_undef(); DISPATCH();
		CASE(op_knull) retval = value_null(); DISPATCH();
		CASE(op_kfalse) retval = value_bool(0); DISPATCH();
		CASE(op_ktrue) retval = value_bool(1); DISPATCH();
		CASE(op_knum) {
			uint32_t val = ktable[iimm];
			retval = value_num((number)val);
			DISPATCH();
		}
		CASE(op_kstr) {
			uint32_t ptr = ktable[iimm];
			struct strobj* str = (struct strobj*)((uint8_t*)cpu + ptr);
			retval = value_str(str);
			DISPATCH();
		}
		CASE(op_mov) retval = lval; DISPATCH();
		CASE(op_xchg) {
			value_t tmp = retval;
			retval = lval;
			lval = tmp;
			DISPATCH();
		}
		CASE(op_add) {
			if (unlikely(value_get_type(lval) == t_str || value_get_type(rval) == t_str)) {
				struct strobj* l = lvalstr;
				struct strobj* r = rvalstr;
				retval = value_str(str_concat(cpu, l, r));
				cpu->cycles -= CYCLES_CHARS(l->len + r->len) + CYCLES_ALLOC;
			}
			else
				retval = value_num(num_add(lvalnum, rvalnum));
			DISPATCH();
		}
		CASE(op_addrn) {
			if (unlikely(value_get_type(lval) == t_str)) {
				struct strobj* l = lvalstr;
				struct strobj* r = num_to_str(cpu, rnum);
				retval = value_str(str_concat(cpu, l, r));
				cpu->cycles -= CYCLES_CHARS(l->len + r->len) + CYCLES_ALLOC;
			}
			else
				retval = value_num(num_add(lvalnum, rnum));
			DISPATCH();
		}
		CASE(op_addnr) {
			if (unlikely(value_get_type(rval) == t_str)) {
				struct strobj* l = num_to_str(cpu, lnum);
				struct strobj* r = rvalstr;
				retval = value_str(str_concat(cpu, l, r));
				cpu->cycles -= CYCLES_CHARS(l->len + r->len) + CYCLES_ALLOC;
			}
			else
				retval = value_num(num_add(lnum, rvalnum));
			DISPATCH();
		}
		CASE(op_sub) retval = value_num(num_sub(lvalnum, rvalnum)); DISPATCH();
		CASE(op_subrn) retval = value_num(num_sub(lvalnum, rnum)); DISPATCH();
		CASE(op_subnr) retval = value_num(num_sub(lnum, rvalnum)); DISPATCH();
		CASE(op_mul) retval = value_num(num_mul(lvalnum, rvalnum)); DISPATCH();
		CASE(op_mulrn) retval = value_num(num_mul(lvalnum, rnum)); DISPATCH();
		CASE(op_mulnr) retval = value_num(num_mul(lnum, rvalnum)); DISPATCH();
		CASE(op_div) retval = value_num(num_div(lvalnum, rvalnum)); DISPATCH();
		CASE(op_divrn) retval = value_num(num_div(lvalnum, rnum)); DISPATCH();
		CASE(op_divnr) retval = value_num(num_div(lnum, rvalnum)); DISPATCH();
		CASE(op_mod) retval = value_num(num_mod(lvalnum, rvalnum)); DISPATCH();
		CASE(op_modrn) retval = value_num(num_mod(lvalnum, rnum)); DISPATCH();
		CASE(op_modnr) retval = value_num(num_mod(lnum, rvalnum)); DISPATCH();
		CASE(op_pow) retval = value_num(num_pow(lvalnum, rvalnum)); DISPATCH();
		CASE(op_powrn) retval = value_num(num_pow(lvalnum, rnum)); DISPATCH();
		CASE(op_pownr) retval = value_num(num_pow(lnum, rvalnum)); DISPATCH();
		CASE(op_inc) retval = value_num(num_add(lvalnum, num_kint(1))); DISPATCH();
		CASE(op_dec) retval = value_num(num_sub(lvalnum, num_kint(1))); DISPATCH();
		CASE(op_plus) retval = value_num(lvalnum); DISPATCH();
		CASE(op_neg) retval = value_num(num_neg(lvalnum)); DISPATCH();
		CASE(op_not) retval = value_bool(!lvalbool); DISPATCH();
		CASE(op_band) retval = value_num(num_and(lvalnum, rvalnum)); DISPATCH();
		CASE(op_bandrn) retval = value_num(num_and(lvalnum, rnum)); DISPATCH();
		CASE(op_bandnr) retval = value_num(num_and(lnum, rvalnum)); DISPATCH();
		CASE(op_bor) retval = value_num(num_or(lvalnum, rvalnum)); DISPATCH();
		CASE(op_borrn) retval = value_num(num_or(lvalnum, rnum)); DISPATCH();
		CASE(op_bornr) retval = value_num(num_or(lnum, rvalnum)); DISPATCH();
		CASE(op_bxor) retval = value_num(num_xor(lvalnum, rvalnum)); DISPATCH();
		CASE(op_bxorrn) retval = value_num(num_xor(lvalnum, rnum)); DISPATCH();
		CASE(op_bxornr) retval = value_num(num_xor(lnum, rvalnum)); DISPATCH();
		CASE(op_bnot) retval = value_num(num_not(lvalnum)); DISPATCH();
		CASE(op_shl) retval = value_num(num_shl(lvalnum, rvalnum)); DISPATCH();
		CASE(op_shlrn) retval = value_num(num_shl(lvalnum, rnum)); DISPATCH();
		CASE(op_shlnr) retval = value_num(num_shl(lnum, rvalnum)); DISPATCH();
		CASE(op_shr) retval = value_num(num_shr(lvalnum, rvalnum)); DISPATCH();
		CASE(op_shrrn) retval = value_num(num_shr(lvalnum, rnum)); DISPATCH();
		CASE(op_shrnr) retval = value_num(num_shr(lnum, rvalnum)); DISPATCH();
		CASE(op_ushr) retval = value_num(num_ushr(lvalnum, rvalnum)); DISPATCH();
		CASE(op_ushrrn) retval = value_num(num_ushr(lvalnum, rnum)); DISPATCH();
		CASE(op_ushrnr) retval = value_num(num_ushr(lnum, rvalnum)); DISPATCH();
		// TODO) String comparisons
		CASE(op_eq) retval = value_bool(strict_equal(cpu, lval, rval)); DISPATCH();
		CASE(op_eqrn) retval = value_bool(strict_equal_num(cpu, lval, rnum)); DISPATCH();
		CASE(op_eqnr) retval = value_bool(strict_equal_num(cpu, rval, lnum)); DISPATCH();
		CASE(op_ne) retval = value_bool(!strict_equal(cpu, lval, rval)); DISPATCH();
		CASE(op_nern) retval = value_bool(!strict_equal_num(cpu, lval, rnum)); DISPATCH();
		CASE(op_nenr) retval = value_bool(!strict_equal_num(cpu, rval, lnum)); DISPATCH();
		CASE(op_lt) retval = value_bool((int32_t)lvalnum < (int32_t)rvalnum); DISPATCH();
		CASE(op_ltrn) retval = value_bool((int32_t)lvalnum < (int32_t)rnum); DISPATCH();
		CASE(op_ltnr) retval = value_bool((int32_t)lnum < (int32_t)rvalnum); DISPATCH();
		CASE(op_le) retval = value_bool((int32_t)lvalnum <= (int32_t)rvalnum); DISPATCH();
		CASE(op_lern) retval = value_bool((int32_t)lvalnum <= (int32_t)rnum); DISPATCH();
		CASE(op_lenr) retval = value_bool((int32_t)lnum <= (int32_t)rvalnum); DISPATCH();
		CASE(op_gt) retval = value_bool((int32_t)lvalnum > (int32_t)rvalnum); DISPATCH();
		CASE(op_gtrn) retval = value_bool((int32_t)lvalnum > (int32_t)rnum); DISPATCH();
		CASE(op_gtnr) retval = value_bool((int32_t)lnum > (int32_t)rvalnum); DISPATCH();
		CASE(op_ge) retval = value_bool((int32_t)lvalnum >= (int32_t)rvalnum); DISPATCH();
		CASE(op_gern) retval = value_bool((int32_t)lvalnum >= (int32_t)rnum); DISPATCH();
		CASE(op_genr) retval = value_bool((int32_t)lnum >= (int32_t)rvalnum); DISPATCH();
		CASE(op_in) retval = value_bool(tab_in(cpu, to_tab(cpu, rval), lvalstr)); DISPATCH();
		CASE(op_typeof) retval = value_str(get_typeof(cpu, lval)); DISPATCH();
		CASE(op_arr) retval = value_arr(arr_new(cpu)); cpu->cycles -= CYCLES_ALLOC; DISPATCH();
		CASE(op_apush) arr_push(cpu, to_arr(cpu, retval), lval); cpu->cycles -= CYCLES_ALLOC; DISPATCH();
		CASE(op_tab) retval = value_tab(tab_new(cpu)); cpu->cycles -= CYCLES_ALLOC; DISPATCH();
		CASE(op_fget) retval = fget(cpu, lval, rval); DISPATCH();
		CASE(op_fgetn) retval = fgetnum(cpu, lval, rnum); DISPATCH();
		CASE(op_fgets) retval = fgetstr(cpu, lval, rstr); DISPATCH();
		CASE(op_fset) fset(cpu, retval, lval, rval); DISPATCH();
		CASE(op_fsetn) fsetn(cpu, retval, lnum, rval); DISPATCH();
		CASE(op_fsets) fsets(cpu, retval, lstr, rval); DISPATCH();
		CASE(op_gget) retval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), lvalstr); cpu->cycles -= CYCLES_LOOKUP; DISPATCH();
		CASE(op_ggets) retval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), lstr); cpu->cycles -= CYCLES_LOOKUP; DISPATCH();
		CASE(op_gset) tab_set(cpu, (struct tabobj*)readptr(cpu->globals), retvalstr, lval); cpu->cycles -= CYCLES_LOOKUP; DISPATCH();
		CASE(op_gsets) tab_set(cpu, (struct tabobj*)readptr(cpu->globals), retstr, lval); cpu->cycles -= CYCLES_LOOKUP; DISPATCH();
		CASE(op_uget) retval = *(value_t*)readptr(((struct upval*)readptr(func->upval[iop2]))->val); DISPATCH();
		CASE(op_uset) *(value_t*)readptr(((struct upval*)readptr(func->upval[iop1]))->val) = lval; DISPATCH();
		CASE(op_iter) retval = get_iter(cpu, lval); cpu->cycles -= CYCLES_ALLOC; DISPATCH();
		CASE(op_next) retval = value_bool(iter_next(cpu, rval, &lval)); DISPATCH();
		CASE(op_j) pc += iimm; DISPATCH();
		CASE(op_closej) {
			close_upvals(cpu, frame, iop1);
			pc += iimm;
			DISPATCH();
		}
		CASE(op_jtrue) if (retvalbool) pc += iimm; DISPATCH();
		CASE(op_jfalse) if (!retvalbool) pc += iimm; DISPATCH();
		CASE(op_func) {
			struct code* code = &((struct code*)readptr(cpu->code))[iimm];
			struct funcobj* f = (struct funcobj*)gc_alloc(cpu, t_func,
				sizeof(struct funcobj) + sizeof(struct upval) * code->upval_cnt);
			f->code = writeptr(code);
			struct updef* defs = readptr_nullable(code->upval);
			for (int i = 0; i < code->upval_cnt; i++) {
				struct updef* def = &defs[i];
				if (def->in_stack) {
					/* find existing open upvalue */
					struct upval* val = readptr_nullable(cpu->upval_open);
					while (val) {
						value_t* v = readptr(val->val);
						if (v >= frame) {
							int idx = (int)(v - frame);
							if (idx == def->idx) {
								f->upval[i] = writeptr(val);
								break;
							}
						}
						val = readptr_nullable(val->next);
					}
					if (!val) {
						/* not found, create new one */
						val = (struct upval*)gc_alloc(cpu, t_upval, sizeof(struct upval));
						val->val = writeptr(&frame[def->idx]);
						val->prev = writeptr_nullable(NULL);
						val->next = cpu->upval_open;
						struct upval* next = readptr_nullable(val->next);
						if (next)
							next->prev = writeptr(val);
						cpu->upval_open = writeptr(val);
						f->upval[i] = writeptr(val);
					}
				}
				else
					f->upval[i] = func->upval[def->idx];
			}
			retval = value_func(f);
			cpu->cycles -= CYCLES_ALLOC * (1 + code->upval_cnt);
			DISPATCH();
		}
		CASE(op_close) {
			close_upvals(cpu, frame, iop1);
			DISPATCH();
		}
		CASE(op_call) {
			if (value_get_type(retval) == t_cfunc) {
				int sp = cpu->sp + iop1;
				cfunc func = cfunc_get(value_get_cfunc(retval));
				((value_t*)readptr(cpu->stack))[sp] = func(cpu, sp, iop2);
			}
			else if (value_get_type(retval) == t_func) {
				struct funcobj* f = (struct funcobj*)value_get_object(retval);
				/* Fill missing arguments to undefined */
				int nargs = ((struct code*)readptr(f->code))->nargs;
				for (int i = iop2; i < nargs; i++)
					frame[iop1 + 2 + i] = value_undef();
				cpu->cycles -= CYCLES_BASE * (nargs - iop2);
				int stack_size = iop1;
				cpu->sp += iop1;
				update_stack();
				value_t* stack = (value_t*)readptr(cpu->stack);
				int ci = cpu->sp + 2 + nargs;
				stack[ci] = value_func(func);
				stack[ci + 1] = value_callinfo(stack_size, (uint16_t)(pc - (uint32_t*)readptr(code->ins)));
				func = f;
				code = (struct code*)readptr(func->code);
				ktable = (uint32_t*)readptr_nullable(code->k);
				pc = (uint32_t*)readptr(code->ins);
			}
			else
				runtime_error(cpu, "Not callable object.");
			DISPATCH();
		}
		CASE(op_retu)
		CASE(op_ret) {
			close_upvals(cpu, frame, 0);
			if (cpu->sp == 0) {
				cpu->paused = 0;
				return;
			}
			int nargs = ((struct code*)readptr(func->code))->nargs;
			func = (struct funcobj*)value_get_object(frame[2 + nargs]);
			value_t ci = frame[3 + nargs];
			if (iopcode == op_retu)
				frame[0] = value_undef();
			else
				frame[0] = retval;
			cpu->sp -= value_get_ci_stacksize(ci);
			update_stack();
			code = (struct code*)readptr(func->code);
			ktable = (uint32_t*)readptr_nullable(code->k);
			pc = &((uint32_t*)readptr(code->ins))[value_get_ci_pc(ci)];
			DISPATCH();
		}
#if !defined(USE_COMPUTED_GOTO)
		} /* switch */
	}
#endif
timeout:
	cpu->cycles += CYCLES_PER_FRAME;
	cpu->curfunc = writeptr(func);
	cpu->curpc = (uint16_t)(pc - (uint32_t*)readptr(code->ins));
	cpu->paused = 1;
}

enum operand_type {
	ot__,
	ot_REG,
	ot_NUM,
	ot_STR,
	ot_IMM8,
	ot_IMM16,
	ot_IMMNUM,
	ot_IMMSTR,
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

static char* dump_str(struct strobj* str, char* buf) {
	int len = str->len;
	if (len > 20)
		len = 20;
	*buf++ = '"';
	memcpy(buf, str->data, len);
	buf += len;
	*buf++ = '"';
	if (len < str->len) {
		*buf++ = '.';
		*buf++ = '.';
		*buf++ = '.';
	}
	return buf;
}

static char* dump_operand(struct cpu* cpu, uint32_t* k, enum operand_type ot, int value, char* buf) {
	if (ot == ot_NUM) {
		number num = (number)k[value];
		return buf + num_format(num, 4, buf);
	}
	else if (ot == ot_STR)
		return dump_str((struct strobj*)forcereadptr(k[value]), buf);
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
			cur = dump_operand(cpu, (uint32_t*)readptr(code->k), desc->op1, ins->op1, cur);
			comma = 1;
		}
		if (desc->op2 != ot__) {
			if (comma) {
				*cur++ = ',';
				*cur++ = ' ';
			}
			switch (desc->op2) {
			case ot_IMMSTR: {
				uint32_t ptr = ((uint32_t*)readptr(code->k))[ins->imm];
				cur = dump_str((struct strobj*)forcereadptr(ptr), cur);
				break;
			}
			case ot_IMM16:
			case ot_IMMFUNC:
				cur += int_format(ins->imm, cur);
				break;
			case ot_IMMNUM: {
				number num = (number)((uint32_t*)readptr(code->k))[ins->imm];
				cur += num_format(num, 4, cur);
				break;
			}
			case ot_REL: {
				cur += int_format(pc + ins->imm, cur);
				break;
			}
			default: {
				cur = dump_operand(cpu, (uint32_t*)readptr(code->k), desc->op2, ins->op2, cur);
				if (desc->op3 != ot__) {
					*cur++ = ',';
					*cur++ = ' ';
					cur = dump_operand(cpu, (uint32_t*)readptr(code->k), desc->op3, ins->op3, cur);
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

#ifdef DEBUG_TIMING
#define X(a, b, c, d, e)	#a,
static const char* const opname[] = {
	OPCODE_DEF(X)
};
#undef X

struct timing_record_item {
	int count;
	int64_t tot_duration;
	int64_t min_duration;
	int64_t max_duration;
};

static struct timing_record_item g_timing_record_items[op_CNT];
static NOINLINE void cpu_timing_record(enum opcode opcode, int64_t duration) {
	struct timing_record_item* item = &g_timing_record_items[opcode];
	item->count++;
	/* Ignore first time for each opcode */
	if (item->count == 0)
		return;
	item->tot_duration += duration;
	if (duration < item->min_duration)
		item->min_duration = duration;
	if (duration > item->max_duration)
		item->max_duration = duration;
}

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#endif
#include <stdio.h>

static void console_printf(const char* format, ...) {
	static char buf[256];
	va_list ap;
	va_start(ap, format);
	int size = str_vsprintf(buf, format, ap);
	va_end(ap);
#ifdef WIN32
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD written;
	WriteConsoleA(console, buf, size, &written, NULL);
#else
	buf[size] = 0;
	printf("%s", buf);
#endif
}

void cpu_timing_reset() {
	for (int i = 0; i < op_CNT; i++) {
		struct timing_record_item* item = &g_timing_record_items[i];
		item->count = -1;
		item->tot_duration = 0;
		item->min_duration = INT64_MAX;
		item->max_duration = 0;
	}
}

void cpu_timing_print_report(int report_tag) {
#ifdef WIN32
	AllocConsole();
#endif
	console_printf("Coxel CPU Timing report:\n");
	console_printf("Report tag: %d\n", report_tag);
	MEASURE_DEFINES();
	const int times = 10;
	MEASURE_START();
	MEASURE_END();
	int64_t overhead = 0;
	for (int i = 0; i < times; i++) {
		MEASURE_START();
		MEASURE_END();
		overhead += MEASURE_DURATION();
	}
	overhead /= times;
	console_printf("Measure overhead: %lld\n", overhead);
	for (int i = 0; i < op_CNT; i++) {
		struct timing_record_item* item = &g_timing_record_items[i];
		if (item->count <= 0)
			continue;
		number avg = num_of_division(item->tot_duration, item->count);
		avg = num_sub(avg, num_kint(overhead));
		console_printf("%s: %d times, tot %lld, avg %f, min %lld, max %lld\n",
			opname[i],
			item->count,
			item->tot_duration - overhead * item->count,
			avg,
			item->min_duration - overhead,
			item->max_duration - overhead);
	}
}
#endif
