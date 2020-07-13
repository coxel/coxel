#include "alloc.h"
#include "arith.h"
#include "compiler.h"
#include "config.h"
#include "str.h"
#include "sym.h"

#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#define TOKEN_TYPE_DEF(_) \
	_(tk_eof, "<eof>", 0, 0) \
	_(tk_ident, "<identifier>", 0, 0) \
	_(tk_num, "<number>", 0, 0) \
	_(tk_str, "<string>", 0, 0) \
	/* keywords */ \
	_(tk_keyword_begin, "", 0, 0) \
	_(tk_break, "break", 0, 0) \
	_(tk_case, "case", 0, 0) \
	_(tk_continue, "continue", 0, 0) \
	_(tk_default, "default", 0, 0) \
	_(tk_do, "do", 0, 0) \
	_(tk_else, "else", 0, 0) \
	_(tk_false, "false", 0, 0) \
	_(tk_for, "for", 0, 0) \
	_(tk_function, "function", 0, 0) \
	_(tk_if, "if", 0, 0) \
	_(tk_in, "in", op_in, 8) \
	_(tk_let, "let", 0, 0) \
	_(tk_null, "null", 0, 0) \
	_(tk_of, "of", 0, 0) \
	_(tk_return, "return", 0, 0) \
	_(tk_switch, "switch", 0, 0) \
	_(tk_this, "this", 0, 0) \
	_(tk_true, "true", 0, 0) \
	_(tk_undefined, "undefined", 0, 0) \
	_(tk_while, "while", 0, 0) \
	_(tk_keyword_end, "", 0, 0) \
	/* binary operators */ \
	_(tk_add, "+", op_add, 10) \
	_(tk_sub, "-", op_sub, 10) \
	_(tk_mul, "*", op_mul, 11) \
	_(tk_div, "/", op_div, 11) \
	_(tk_mod, "%", op_mod, 11) \
	_(tk_exp, "**", op_exp, 12) \
	/* prefix/postfix operators */ \
	_(tk_inc, "++", op_inc, 0) \
	_(tk_dec, "--", op_dec, 0) \
	/* logical operators */ \
	_(tk_and, "&&", 0, 2) \
	_(tk_or, "||", 0, 1) \
	_(tk_not, "!", 0, 0) \
	/* bitwise logical operators */ \
	_(tk_band, "&", op_band, 6) \
	_(tk_bor, "|", op_bor, 4) \
	_(tk_bxor, "^", op_bxor, 5) \
	_(tk_bnot, "~", 0, 0) \
	_(tk_shl, "<<", op_shl, 9) \
	_(tk_shr, ">>", op_shr, 9) \
	_(tk_ushr, ">>>", op_ushr, 9) \
	/* comparison operators */ \
	_(tk_eq, "==", op_eq, 7) \
	_(tk_ne, "!=", op_ne, 7) \
	_(tk_lt, "<", op_lt, 8) \
	_(tk_le, "<=", op_le, 8) \
	_(tk_gt, ">", op_gt, 8) \
	_(tk_ge, ">=", op_ge, 8) \
	/* assignment operators */ \
	_(tk_assign_begin, "", 0, 0) \
	_(tk_assign, "=", op_mov, 0) \
	_(tk_add_assign, "+=", op_add, 0) \
	_(tk_sub_assign, "-=", op_sub, 0) \
	_(tk_mul_assign, "*=", op_mul, 0) \
	_(tk_div_assign, "/=", op_div, 0) \
	_(tk_mod_assign, "%=", op_mod, 0) \
	_(tk_exp_assign, "**=", op_exp, 0) \
	_(tk_band_assign, "&=", op_band, 0) \
	_(tk_bor_assign, "|=", op_bor, 0) \
	_(tk_bxor_assign, "^=", op_bxor, 0) \
	_(tk_shl_assign, "<<=", op_shl, 0) \
	_(tk_shr_assign, ">>=", op_shr, 0) \
	_(tk_ushr_assign, ">>>=", op_ushr, 0) \
	_(tk_assign_end, "", 0, 0) \
	/* groups */ \
	_(tk_lbrace, "{", 0, 0) \
	_(tk_rbrace, "}", 0, 0) \
	_(tk_lbracket, "[", 0, 0) \
	_(tk_rbracket, "]", 0, 0) \
	_(tk_lparen, "(", 0, 0) \
	_(tk_rparen, ")", 0, 0) \
	/* other tokens */ \
	_(tk_qmark, "?", 0, 0) \
	_(tk_colon, ":", 0, 0) \
	_(tk_semicolon, ";", 0, 0) \
	_(tk_comma, ",", 0, 0) \
	_(tk_dot, ".", 0, 0)

#define X(a, b, c, d) a,
enum token_type {
	TOKEN_TYPE_DEF(X)
};
#undef X

#define X(a, b, c, d) b,
static const char* token_names[] = {
	TOKEN_TYPE_DEF(X)
};
#undef X

#define X(a, b, c, d) c,
static enum opcode token_ops[] = {
	TOKEN_TYPE_DEF(X)
};
#undef X

#define X(a, b, c, d) d,
static uint8_t token_precedence[] = {
	TOKEN_TYPE_DEF(X)
};
#undef X

struct functx {
	struct functx* enfunc;
	int code_id;
	int sym_level;
};

enum patch_type {
	pt_break,
	pt_continue,
};

struct patch {
	enum patch_type type : 8;
	uint16_t pc;
};

struct context {
	struct compile_err err;
	jmp_buf jmp_buf;
	struct cpu* cpu;
	struct alloc* alloc;
	const char* code;
	int codelen;
	int codep;
	int lastlinenum;
	int lastlineins;
	int linenum;
	enum token_type token;
	number token_num;
	const char* token_str_begin;
	const char* token_str_end;
	int sbuf_cnt, sbuf_cap;
	char* sbuf;
	char ch;
	int sp;
	int local_sp;
	struct sym_table sym_table;
	struct functx* topfunc;
	int canbreak, cancontinue;
	int patch_cnt, patch_cap;
	struct patch* patch;
};

static NORETURN void compile_error(struct context* ctx, const char *format, ...) {
	static char buf[SYM_MAX_LEN + 64];
	va_list args;
	va_start(args, format);
	str_vsprintf(buf, format, args);
	va_end(args);
	ctx->err.msg = buf;
	ctx->err.linenum = ctx->linenum;
	longjmp(ctx->jmp_buf, 1);
}

static NORETURN void internal_error(struct context* ctx) {
	compile_error(ctx, "Internal compiler error.");
}

static void next_char(struct context* ctx) {
	if (ctx->codep == ctx->codelen)
		ctx->ch = 0;
	else
		ctx->ch = ctx->code[ctx->codep++];
}

static void next_token(struct context* ctx) {
start:
	while (ctx->ch == ' ' || ctx->ch == '\t' || ctx->ch == '\r' || ctx->ch == '\n') {
		if (ctx->ch == '\n')
			ctx->linenum++;
		next_char(ctx);
	}
	if ((ctx->ch >= 'a' && ctx->ch <= 'z')
		|| (ctx->ch >= 'A' && ctx->ch <= 'Z')
		|| (ctx->ch == '$' || ctx->ch == '_')) {
		ctx->token_str_begin = &ctx->code[ctx->codep - 1];
		do {
			next_char(ctx);
		} while ((ctx->ch >= 'a' && ctx->ch <= 'z')
			|| (ctx->ch >= 'A' && ctx->ch <= 'Z')
			|| (ctx->ch >= '0' && ctx->ch <= '9')
			|| (ctx->ch == '$' || ctx->ch == '_'));
		ctx->token_str_end = &ctx->code[ctx->codep - 1];
		if (ctx->token_str_end - ctx->token_str_begin > SYM_MAX_LEN)
			compile_error(ctx, "Identifier too long");
		ctx->token = tk_ident;
		/* test if the identifier is a known keyword */
		for (enum token_type tk = tk_keyword_begin + 1; tk <= tk_keyword_end - 1; tk++) {
			const char *k = token_names[tk];
			int equal;
			for (const char* p = ctx->token_str_begin;; p++, k++) {
				if (p == ctx->token_str_end && *k == 0) {
					equal = 1;
					break;
				}
				else if (p == ctx->token_str_end || *p != *k) {
					equal = 0;
					break;
				}
			}
			if (equal) {
				ctx->token = tk;
				break;
			}
		}
	}
	else if ((ctx->ch >= '0' && ctx->ch <= '9') || ctx->ch == '.') {
		int dot_encountered = 0;
		int base = 10;
		ctx->token_str_begin = &ctx->code[ctx->codep - 1];
		if (ctx->ch == '0') {
			next_char(ctx);
			if (ctx->ch == 'b') {
				base = 2;
				next_char(ctx);
			}
			else if (ctx->ch == 'o') {
				base = 8;
				next_char(ctx);
			}
			else if (ctx->ch == 'x') {
				base = 16;
				next_char(ctx);
			}
			else if (ctx->ch != '.')
				base = 8;
		}
		if (ctx->ch == '.') {
			dot_encountered = 1;
			next_char(ctx);
		}
		while ((base == 2 && ctx->ch >= '0' && ctx->ch <= '1')
			|| (base == 8 && ctx->ch >= '0' && ctx->ch <= '7')
			|| (base >= 10 && ctx->ch >= '0' && ctx->ch <= '9')
			|| (base == 16 && ((ctx->ch >= 'a' && ctx->ch <= 'f') || (ctx->ch >= 'A' && ctx->ch <= 'F')))) {
			next_char(ctx);
			if (ctx->ch == '.' && !dot_encountered) {
				next_char(ctx);
				dot_encountered = 1;
			}
		}
		ctx->token_str_end = &ctx->code[ctx->codep - 1];
		if (dot_encountered && ctx->token_str_begin + 1 == ctx->token_str_end)
			ctx->token = tk_dot;
		else {
			ctx->token = tk_num;
			if (!num_parse(ctx->token_str_begin, ctx->token_str_end, &ctx->token_num))
				compile_error(ctx, "Invalid number.");
		}
	}
	else if (ctx->ch == '"' || ctx->ch == '\'') {
		const char quote = ctx->ch;
		next_char(ctx);
		ctx->sbuf_cnt = 0;
		while (ctx->ch != 0 && ctx->ch != '\n' && ctx->ch != quote) {
			if (ctx->ch == '\\') {
				next_char(ctx);
				switch (ctx->ch) {
				case 'n': ctx->ch = '\n'; break;
				case '\'': ctx->ch = '\''; break;
				case '"': ctx->ch = '"'; break;
				case '\\': ctx->ch = '\\'; break;
				default: compile_error(ctx, "Invalid escape character '%c'.", ctx->ch);
				}
			}
			vec_add(ctx->alloc, ctx->sbuf, ctx->sbuf_cnt, ctx->sbuf_cap);
			ctx->sbuf[ctx->sbuf_cnt - 1] = ctx->ch;
			next_char(ctx);
		}
		if (ctx->ch != quote)
			compile_error(ctx, "Unenclosed string.");
		next_char(ctx);
		ctx->token = tk_str;
		ctx->token_str_begin = ctx->sbuf;
		ctx->token_str_end = ctx->sbuf + ctx->sbuf_cnt;
	}
	else {
		switch (ctx->ch) {
		case 0: ctx->token = tk_eof; break;
		case '+': {
			next_char(ctx);
			if (ctx->ch == '+') {
				next_char(ctx);
				ctx->token = tk_inc;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_add_assign;
			}
			else
				ctx->token = tk_add;
			break;
		}
		case '-': {
			next_char(ctx);
			if (ctx->ch == '-') {
				next_char(ctx);
				ctx->token = tk_dec;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_sub_assign;
			}
			else
				ctx->token = tk_sub;
			break;
			break;
		}
		case '*': {
			next_char(ctx);
			if (ctx->ch == '*') {
				next_char(ctx);
				if (ctx->ch == '=') {
					next_char(ctx);
					ctx->token = tk_exp_assign;
				}
				else
					ctx->token = tk_exp;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_mul_assign;
			}
			else
				ctx->token = tk_mul;
			break;
		}
		case '/': {
			next_char(ctx);
			if (ctx->ch == '/') {
				while (ctx->ch != 0 && ctx->ch != '\n')
					next_char(ctx);
				goto start;
			}
			else if (ctx->ch == '*') {
				do {
					next_char(ctx);
					while (ctx->ch == '*') {
						next_char(ctx);
						if (ctx->ch == '/') {
							next_char(ctx);
							goto start;
						}
					}
				} while (ctx->ch != 0);
				compile_error(ctx, "Block comment not closed.");
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_div_assign;
			}
			else
				ctx->token = tk_div;
			break;
		}
		case '%': {
			next_char(ctx);
			if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_mod_assign;
			}
			else
				ctx->token = tk_mod;
			break;
		}
		case '&': {
			next_char(ctx);
			if (ctx->ch == '&') {
				next_char(ctx);
				ctx->token = tk_and;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_band_assign;
			}
			else
				ctx->token = tk_band;
			break;
		}
		case '|': {
			next_char(ctx);
			if (ctx->ch == '|') {
				next_char(ctx);
				ctx->token = tk_or;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_bor_assign;
			}
			else
				ctx->token = tk_bor;
			break;
		}
		case '^': {
			next_char(ctx);
			if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_bxor_assign;
			}
			else
				ctx->token = tk_bxor;
			break;
		}
		case '!': {
			next_char(ctx);
			if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_ne;
			}
			else
				ctx->token = tk_not;
			break;
		}
		case '~': next_char(ctx); ctx->token = tk_bnot; break;
		case '<': {
			next_char(ctx);
			if (ctx->ch == '<') {
				next_char(ctx);
				if (ctx->ch == '=') {
					next_char(ctx);
					ctx->token = tk_shl_assign;
				}
				else
					ctx->token = tk_shl;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_le;
			}
			else
				ctx->token = tk_lt;
			break;
		}
		case '>': {
			next_char(ctx);
			if (ctx->ch == '>') {
				next_char(ctx);
				if (ctx->ch == '>') {
					next_char(ctx);
					if (ctx->ch == '=') {
						next_char(ctx);
						ctx->token = tk_ushr_assign;
					}
					else
						ctx->token = tk_ushr;
				}
				else if (ctx->ch == '=') {
					next_char(ctx);
					ctx->token = tk_shr_assign;
				}
				else
					ctx->token = tk_shr;
			}
			else if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_ge;
			}
			else
				ctx->token = tk_gt;
			break;
		}
		case '=': {
			next_char(ctx);
			if (ctx->ch == '=') {
				next_char(ctx);
				ctx->token = tk_eq;
			}
			else
				ctx->token = tk_assign;
			break;
		}
		case '{': next_char(ctx); ctx->token = tk_lbrace; break;
		case '}': next_char(ctx); ctx->token = tk_rbrace; break;
		case '[': next_char(ctx); ctx->token = tk_lbracket; break;
		case ']': next_char(ctx); ctx->token = tk_rbracket; break;
		case '(': next_char(ctx); ctx->token = tk_lparen; break;
		case ')': next_char(ctx); ctx->token = tk_rparen; break;
		case '?': next_char(ctx); ctx->token = tk_qmark; break;
		case ':': next_char(ctx); ctx->token = tk_colon; break;
		case ';': next_char(ctx); ctx->token = tk_semicolon; break;
		case ',': next_char(ctx); ctx->token = tk_comma; break;
		}
	}
}

static void check_token(struct context* ctx, enum token_type token) {
	if (ctx->token != token)
		compile_error(ctx, "Expected '%s', got '%s'.", token_names[token], token_names[ctx->token]);
}

static void require_token(struct context* ctx, enum token_type token) {
	check_token(ctx, token);
	next_token(ctx);
}

enum value_type {
	vt_null, /* null value */
	vt_value, /* generic in-reg value */
	vt_local, /* local variable */
	vt_global, /* global variable */
	vt_upval, /* upvalue */
	vt_member, /* object member */
};

#define sval_in_reg(x)	((x).type == vt_value || (x).type == vt_local)
#define topcode()		(&((struct code*)readptr(ctx->cpu->code))[ctx->topfunc->code_id])

static void add_lineinfo(struct context* ctx, enum licmdtype type, int delta) {
	if (delta < 0)
		internal_error(ctx);
	struct cpu* cpu = ctx->cpu;
	struct code* code = topcode();
	struct licmd* lineinfo = readptr(code->lineinfo);
	while (delta) {
		vec_add(ctx->alloc, lineinfo, code->lineinfo_cnt, code->lineinfo_cap);
		struct licmd* cmd = &lineinfo[code->lineinfo_cnt - 1];
		cmd->type = type;
		if (delta < 128) {
			cmd->delta = delta;
			break;
		}
		else {
			cmd->delta = 127;
			delta -= 127;
		}
	}
	code->lineinfo = writeptr(lineinfo);
}

static struct ins* add_ins(struct context* ctx) {
	struct cpu* cpu = ctx->cpu;
	struct code* code = topcode();
	struct ins* ins = readptr(code->ins);
	vec_add(ctx->alloc, ins, code->ins_cnt, code->ins_cap);
	code->ins = writeptr(ins);
	int ins_id = code->ins_cnt - 1;
	if (ctx->linenum != ctx->lastlinenum) {
		if (ins_id != ctx->lastlineins) {
			add_lineinfo(ctx, li_ins, ins_id - ctx->lastlineins);
			ctx->lastlineins = ins_id;
		}
		add_lineinfo(ctx, li_line, ctx->linenum - ctx->lastlinenum);
		ctx->lastlinenum = ctx->linenum;
	}
	return &ins[ins_id];
}

static void emit(struct context* ctx, enum opcode opcode, uint8_t op1, uint8_t op2, uint8_t op3) {
	struct ins* ins = add_ins(ctx);
	ins->opcode = opcode;
	ins->op1 = op1;
	ins->op2 = op2;
	ins->op3 = op3;
}

static void emit_imm(struct context* ctx, enum opcode opcode, uint8_t op1, uint16_t imm) {
	struct ins* ins = add_ins(ctx);
	ins->opcode = opcode;
	ins->op1 = op1;
	ins->imm = imm;
}

static void emit_rel(struct context* ctx, enum opcode opcode, uint8_t op1, int destpc) {
	struct cpu* cpu = ctx->cpu;
	struct code* code = topcode();
	struct ins* ins = add_ins(ctx);
	ins->opcode = opcode;
	ins->op1 = op1;
	ins->imm = destpc - code->ins_cnt;
}

static inline int current_pc(struct context* ctx) {
	struct cpu* cpu = ctx->cpu;
	return topcode()->ins_cnt;
}

static inline void patch_rel(struct context* ctx, int pc, int target) {
	struct cpu* cpu = ctx->cpu;
	((struct ins*)readptr(topcode()->ins))[pc].imm = target - pc - 1;
}

struct sval {
	enum value_type type : 8;
	uint8_t reg;
	uint8_t field;
};

static inline struct sval sval_null(uint8_t reg) {
	struct sval val;
	val.type = vt_null;
	return val;
}

static inline struct sval sval_value(uint8_t reg) {
	struct sval val;
	val.type = vt_value;
	val.reg = reg;
	return val;
}

static inline struct sval sval_local(uint8_t reg) {
	struct sval val;
	val.type = vt_local;
	val.reg = reg;
	return val;
}

static inline struct sval sval_global(uint8_t reg) {
	struct sval val;
	val.type = vt_global;
	val.reg = reg;
	return val;
}

static inline struct sval sval_upval(uint8_t reg) {
	struct sval val;
	val.type = vt_upval;
	val.reg = reg;
	return val;
}

static inline struct sval sval_member(uint8_t reg, uint8_t field) {
	struct sval val;
	val.type = vt_member;
	val.reg = reg;
	val.field = field;
	return val;
}

static void sval_popone(struct context* ctx, int reg) {
	if (reg >= ctx->local_sp) {
		if (reg == ctx->sp - 1)
			--ctx->sp;
		else
			internal_error(ctx);
	}
}

static void sval_poptwo(struct context* ctx, int a, int b) {
	if (a < b) {
		sval_popone(ctx, b);
		sval_popone(ctx, a);
	}
	else {
		sval_popone(ctx, a);
		sval_popone(ctx, b);
	}
}

static void sval_pop(struct context* ctx, struct sval sval) {
	if (sval.type == vt_upval)
		return;
	if (sval.type == vt_member)
		sval_poptwo(ctx, sval.reg, sval.field);
	else
		sval_popone(ctx, sval.reg);
}

static struct sval sval_get(struct context* ctx, struct sval sval) {
	switch (sval.type) {
	case vt_value:
	case vt_local:
		return sval;
	case vt_global: {
		emit(ctx, op_gget, ctx->sp, sval.reg, 0);
		return sval_value(ctx->sp++);
	}
	case vt_upval: {
		emit(ctx, op_uget, ctx->sp, sval.reg, 0);
		return sval_value(ctx->sp++);
	}
	case vt_member:
		emit(ctx, op_fget, ctx->sp, sval.reg, sval.field);
		return sval_value(ctx->sp++);
	default: internal_error(ctx);
	}
}

static struct sval sval_extract(struct context* ctx, struct sval sval) {
	if (sval_in_reg(sval))
		return sval;
	sval_pop(ctx, sval);
	return sval_get(ctx, sval);
}

static struct sval sval_force_extract(struct context* ctx, struct sval sval) {
	if (sval.type == vt_local) {
		emit(ctx, op_mov, ctx->sp, sval.reg, 0);
		return sval_value(ctx->sp++);
	}
	else
		return sval_extract(ctx, sval);
}

static void sval_set(struct context* ctx, struct sval lval, struct sval rval) {
	if (lval.type == vt_value)
		compile_error(ctx, "Modifying rvalue.");
	if (!sval_in_reg(rval))
		internal_error(ctx);
	switch (lval.type) {
	case vt_local:
		if (lval.reg != rval.reg)
			emit(ctx, op_mov, lval.reg, rval.reg, 0);
		break;
	case vt_global:
		emit(ctx, op_gset, lval.reg, rval.reg, 0);
		break;
	case vt_upval:
		emit(ctx, op_uset, lval.reg, rval.reg, 0);
		break;
	case vt_member:
		emit(ctx, op_fset, lval.reg, lval.field, rval.reg);
		break;
	default: internal_error(ctx);
	}
}

static struct sval sval_str(struct context* ctx, struct strobj* str) {
	/* find duplicate constant */
	struct cpu* cpu = ctx->cpu;
	struct code* code = topcode();
	struct value* k = readptr(code->k);
	int slot = -1;
	for (int i = 0; i < code->k_cnt; i++) {
		if (k[i].type == t_str && (struct strobj*)readptr(k[i].str) == str) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		vec_add(ctx->alloc, k, code->k_cnt, code->k_cap);
		code->k = writeptr(k);
		slot = code->k_cnt - 1;
		struct value* kval = &k[slot];
		kval->type = t_str;
		kval->str = writeptr(str);
	}
	emit_imm(ctx, op_kobj, ctx->sp, slot);
	return sval_value(ctx->sp++);
}

static struct sval sval_tkstr(struct context* ctx) {
	return sval_str(ctx, str_intern_nogc(ctx->cpu, ctx->token_str_begin, (int)(ctx->token_str_end - ctx->token_str_begin)));
}

static void add_patch(struct context* ctx, enum patch_type type, int pc) {
	vec_add(ctx->alloc, ctx->patch, ctx->patch_cnt, ctx->patch_cap);
	struct patch* patch = &ctx->patch[ctx->patch_cnt - 1];
	patch->type = type;
	patch->pc = pc;
}

static int add_code(struct cpu* cpu) {
	struct code* codearr = readptr(cpu->code);
	vec_add(&cpu->alloc, codearr, cpu->code_cnt, cpu->code_cap);
	cpu->code = writeptr(codearr);
	struct code* code = &codearr[cpu->code_cnt - 1];
	code->nargs = 0;
	code->enclosure = -1;
	code->ins_cnt = 0;
	code->ins_cap = 0;
	code->ins = writeptr(NULL);
	code->lineinfo_cnt = 0;
	code->lineinfo_cap = 0;
	code->lineinfo = writeptr(NULL);
	code->k_cnt = 0;
	code->k_cap = 0;
	code->k = writeptr(NULL);
	code->upval_cnt = 0;
	code->upval_cap = 0;
	code->upval = writeptr(NULL);
	return cpu->code_cnt - 1;
}

#define compile_single_expression	compile_assign
static struct sval compile_assign(struct context* ctx);
static struct sval compile_expression(struct context* ctx);
static void compile_statement(struct context* ctx);
static void compile_block(struct context* ctx);

static int add_updef(struct context* ctx, struct functx* fctx, int level, int reg) {
	int in_stack, idx;
	if (level >= fctx->enfunc->sym_level) {
		in_stack = 1;
		idx = reg;
	}
	else {
		in_stack = 0;
		idx = add_updef(ctx, fctx->enfunc, level, reg);
	}
	struct cpu* cpu = ctx->cpu;
	/* find duplicate */
	struct code* code = &((struct code*)readptr(cpu->code))[fctx->code_id];
	struct updef* upvals = readptr(code->upval);
	for (int i = 0; i < code->upval_cnt; i++) {
		struct updef* def = &upvals[i];
		if (def->in_stack == in_stack && def->idx == idx)
			return i;
	}
	/* not found, create one */
	vec_add(ctx->alloc, upvals, code->upval_cnt, code->upval_cap);
	code->upval = writeptr(upvals);
	struct updef* def = &upvals[code->upval_cnt - 1];
	def->in_stack = in_stack;
	def->idx = idx;
	return code->upval_cnt - 1;
}

static struct sval compile_function(struct context* ctx, int global) {
	struct cpu* cpu = ctx->cpu;
	next_token(ctx);
	sym_push(&ctx->sym_table);
	struct sval nval;
	struct strobj* name = NULL;
	if (global) {
		if (ctx->topfunc->enfunc != NULL) // TODO: Wrong hoisting semantics
			compile_error(ctx, "Global function definition must appear at toplevel.");
		check_token(ctx, tk_ident);
		name = str_intern_nogc(ctx->cpu, ctx->token_str_begin, (int)(ctx->token_str_end - ctx->token_str_begin));
		nval = sval_str(ctx, name);
		next_token(ctx);
	}
	else if (ctx->token == tk_ident) {
		/* named function */
		struct sym* sym;
		if (!sym_emplace(&ctx->sym_table, ctx->sym_table.level,
			ctx->token_str_begin, ctx->token_str_end - ctx->token_str_begin, &sym))
			internal_error(ctx);
		name = str_intern_nogc(ctx->cpu, ctx->token_str_begin, (int)(ctx->token_str_end - ctx->token_str_begin));
		sym->reg = 0;
		next_token(ctx);
	}
	require_token(ctx, tk_lparen);
	int nargs = 0;
	if (ctx->token != tk_eof && ctx->token != tk_rparen) {
		do {
			if (nargs)
				require_token(ctx, tk_comma);
			check_token(ctx, tk_ident);
			struct sym* sym;
			if (!sym_emplace(&ctx->sym_table, ctx->sym_table.level,
				ctx->token_str_begin, ctx->token_str_end - ctx->token_str_begin, &sym)) {
				compile_error(ctx, "Parameter redefinition.");
			}
			sym->reg = 1 + ++nargs;
			next_token(ctx);
		} while (ctx->token == tk_comma);
	}
	require_token(ctx, tk_rparen);
	require_token(ctx, tk_lbrace);
	struct functx func;
	func.enfunc = ctx->topfunc;
	func.code_id = add_code(cpu);
	struct code* code = &((struct code*)readptr(cpu->code))[func.code_id];
	code->nargs = nargs;
	code->enclosure = ctx->topfunc->code_id;
	code->name = writeptr(name);
	func.sym_level = ctx->sym_table.level;
	ctx->topfunc = &func;
	int old_sp = ctx->sp;
	int old_local_sp = ctx->local_sp;
	int old_lastlinenum = ctx->lastlinenum;
	int old_lastlineins = ctx->lastlineins;
	ctx->sp = 2 + nargs + 2;
	ctx->local_sp = ctx->sp;
	ctx->lastlinenum = 0;
	ctx->lastlineins = 0;
	compile_block(ctx);
	require_token(ctx, tk_rbrace);
	emit(ctx, op_retu, 0, 0, 0);
	ctx->sp = old_sp;
	ctx->local_sp = old_local_sp;
	ctx->lastlinenum = old_lastlinenum;
	ctx->lastlineins = old_lastlineins;
	ctx->topfunc = func.enfunc;
	sym_pop(&ctx->sym_table);
	emit_imm(ctx, op_func, ctx->sp, func.code_id);
	if (global) {
		emit(ctx, op_gset, nval.reg, ctx->sp, 0);
		sval_pop(ctx, nval);
		return nval;
	}
	else
		return sval_value(ctx->sp++);
}

static struct sval compile_element(struct context* ctx) {
	struct cpu* cpu = ctx->cpu;
	switch (ctx->token) {
	case tk_ident: {
		int level;
		struct sym* sym = sym_find(&ctx->sym_table, 0, ctx->token_str_begin,
			ctx->token_str_end - ctx->token_str_begin, &level);
		if (sym) {
			next_token(ctx);
			if (level >= ctx->topfunc->sym_level)
				return sval_local(sym->reg);
			sym->upval_used = 1;
			return sval_upval(add_updef(ctx, ctx->topfunc, level, sym->reg));
		}
		struct sval key = sval_tkstr(ctx);
		sval_pop(ctx, key);
		next_token(ctx);
		return sval_global(ctx->sp++);
	}
	case tk_num: {
		emit_imm(ctx, op_knum, ctx->sp, ctx->token_num & 0xFFFF);
		emit_imm(ctx, op_data, 0, ctx->token_num >> 16);
		next_token(ctx);
		return sval_value(ctx->sp++);
	}
	case tk_str: {
		struct sval val = sval_tkstr(ctx);
		next_token(ctx);
		return val;
	}
	case tk_lparen: {
		next_token(ctx);
		struct sval val = compile_expression(ctx);
		require_token(ctx, tk_rparen);
		return val;
	}
	case tk_undefined: {
		next_token(ctx);
		emit(ctx, op_kundef, ctx->sp, 0, 0);
		return sval_value(ctx->sp++);
	}
	case tk_null: {
		next_token(ctx);
		emit(ctx, op_knull, ctx->sp, 0, 0);
		return sval_value(ctx->sp++);
	}
	case tk_false: {
		next_token(ctx);
		emit(ctx, op_kfalse, ctx->sp, 0, 0);
		return sval_value(ctx->sp++);
	}
	case tk_true: {
		next_token(ctx);
		emit(ctx, op_ktrue, ctx->sp, 0, 0);
		return sval_value(ctx->sp++);
	}
	case tk_this: {
		next_token(ctx);
		if (ctx->topfunc->enfunc == NULL) {
			struct sval val = sval_str(ctx, LIT(global));
			sval_pop(ctx, val);
			emit(ctx, op_gget, ctx->sp, val.reg, 0);
			return sval_value(ctx->sp++);
		}
		else
			return sval_value(1);
	}
	case tk_lbracket: {
		next_token(ctx);
		emit_imm(ctx, op_arr, ctx->sp, 0);
		struct sval aval = sval_value(ctx->sp++);
		while (ctx->token != tk_eof && ctx->token != tk_rbracket) {
			struct sval val = compile_single_expression(ctx);
			val = sval_extract(ctx, val);
			sval_pop(ctx, val);
			emit(ctx, op_apush, aval.reg, val.reg, 0);
			if (ctx->token != tk_comma)
				break;
			next_token(ctx);
		}
		require_token(ctx, tk_rbracket);
		return aval;
	}
	case tk_lbrace: {
		next_token(ctx);
		emit_imm(ctx, op_tab, ctx->sp, 0);
		struct sval tval = sval_value(ctx->sp++);
		while (ctx->token != tk_eof && ctx->token != tk_rbrace) {
			if (ctx->token != tk_ident && ctx->token != tk_str)
				compile_error(ctx, "Identifier or string expected.");
			struct sval kval = sval_tkstr(ctx);
			next_token(ctx);
			require_token(ctx, tk_colon);
			struct sval val = compile_single_expression(ctx);
			val = sval_extract(ctx, val);
			sval_pop(ctx, val);
			sval_pop(ctx, kval);
			emit(ctx, op_fset, tval.reg, kval.reg, val.reg);
			if (ctx->token != tk_comma)
				break;
			next_token(ctx);
		}
		require_token(ctx, tk_rbrace);
		return tval;
	}
	case tk_function:
		return compile_function(ctx, 0);
	default:
		compile_error(ctx, "Invalid token.");
	}
}

static struct sval compile_member(struct context* ctx) {
	struct sval val = compile_element(ctx);
	for (;;) {
		if (ctx->token == tk_dot) {
			val = sval_extract(ctx, val);
			next_token(ctx);
			check_token(ctx, tk_ident);
			struct sval rval = sval_tkstr(ctx);
			next_token(ctx);
			val = sval_member(val.reg, rval.reg);
		}
		else if (ctx->token == tk_lbracket) {
			val = sval_extract(ctx, val);
			next_token(ctx);
			struct sval rval = compile_expression(ctx);
			require_token(ctx, tk_rbracket);
			rval = sval_extract(ctx, rval);
			val = sval_member(val.reg, rval.reg);
		}
		else if (ctx->token == tk_lparen) {
			int sp;
			int param_count = 0;
			if (val.type == vt_member) {
				sval_pop(ctx, val);
				sp = ctx->sp;
				if (val.reg == ctx->sp) {
					emit(ctx, op_fget, ctx->sp + 1, val.reg, val.field);
					emit(ctx, op_xchg, ctx->sp, ctx->sp + 1, 0);
					ctx->sp += 2;
				}
				else {
					emit(ctx, op_fget, ctx->sp++, val.reg, val.field);
					emit(ctx, op_mov, ctx->sp++, val.reg, 0);
				}
			}
			else {
				val = sval_force_extract(ctx, val);
				sp = ctx->sp - 1;
				emit(ctx, op_kundef, ctx->sp++, 0, 0);
			}
			next_token(ctx);
			while (ctx->token != tk_rparen) {
				if (param_count)
					require_token(ctx, tk_comma);
				struct sval pval = compile_single_expression(ctx);
				sval_force_extract(ctx, pval);
				param_count++;
			}
			// TODO: maximum number of param_count
			next_token(ctx);
			ctx->sp = sp;
			emit(ctx, op_call, sp, param_count, 0);
			val = sval_value(ctx->sp++);
		}
		else
			break;
	}
	return val;
}

static struct sval compile_postfix(struct context* ctx) {
	struct sval val = compile_member(ctx);
	if (ctx->token == tk_inc || ctx->token == tk_dec) {
		enum opcode op = token_ops[ctx->token];
		next_token(ctx);
		switch (val.type) {
		case vt_value: compile_error(ctx, "Modifying rvalue.");
		case vt_local: {
			emit(ctx, op_mov, ctx->sp, val.reg, 0);
			emit(ctx, op, val.reg, val.reg, 0);
			return sval_value(ctx->sp++);
		}
		default: {
			struct sval mval = sval_get(ctx, val);
			emit(ctx, op, ctx->sp, mval.reg, 0);
			sval_set(ctx, val, sval_value(ctx->sp));
			sval_pop(ctx, mval);
			sval_pop(ctx, val);
			emit(ctx, op_mov, ctx->sp, mval.reg, 0);
			return sval_value(ctx->sp++);
		}
		}
	}
	else
		return val;
}

static struct sval compile_unary(struct context* ctx) {
	if (ctx->token == tk_not || ctx->token == tk_bnot || ctx->token == tk_add || ctx->token == tk_sub) {
		enum opcode op;
		switch (ctx->token) {
		case tk_not: op = op_not; break;
		case tk_bnot: op = op_bnot; break;
		case tk_add: op = op_plus; break;
		case tk_sub: op = op_neg; break;
		default: internal_error(ctx);
		}
		next_token(ctx);
		struct sval val = compile_unary(ctx);
		val = sval_extract(ctx, val);
		sval_pop(ctx, val);
		emit(ctx, op, ctx->sp, val.reg, 0);
		return sval_value(ctx->sp++);
	}
	else if (ctx->token == tk_inc || ctx->token == tk_dec) {
		enum opcode op = token_ops[ctx->token];
		next_token(ctx);
		struct sval val = compile_postfix(ctx);
		switch (val.type) {
		case vt_value: compile_error(ctx, "Modifying rvalue.");
		case vt_local:
			emit(ctx, op, val.reg, val.reg, 0);
			emit(ctx, op_mov, ctx->sp, val.reg, 0);
			return sval_value(ctx->sp++);
		default: {
			struct sval mval = sval_get(ctx, val);
			emit(ctx, op, mval.reg, mval.reg, 0);
			sval_set(ctx, val, mval);
			sval_pop(ctx, mval);
			sval_pop(ctx, val);
			emit(ctx, op_mov, ctx->sp, mval.reg, 0);
			return sval_value(ctx->sp++);
		}
		}
	}
	else
		return compile_postfix(ctx);
}

static struct sval compile_binary_subexp(struct context* ctx, struct sval lval) {
	lval = sval_extract(ctx, lval);
	int precedence = token_precedence[ctx->token];
	while (token_precedence[ctx->token] == precedence) {
		enum token_type token = ctx->token;
		enum opcode op = token_ops[ctx->token];
		next_token(ctx);
		if (token == tk_and || token == tk_or) {
			lval = sval_force_extract(ctx, lval);
			int pc = current_pc(ctx);
			if (token == tk_and)
				emit_imm(ctx, op_jfalse, lval.reg, 0);
			else
				emit_imm(ctx, op_jtrue, lval.reg, 0);
			sval_pop(ctx, lval);
			struct sval rval = compile_unary(ctx);
			while (token_precedence[ctx->token] > precedence || (op == op_exp && ctx->token == tk_exp))
				rval = compile_binary_subexp(ctx, rval);
			rval = sval_force_extract(ctx, rval);
			sval_pop(ctx, rval);
			patch_rel(ctx, pc, current_pc(ctx));
			lval = sval_value(ctx->sp++);
		}
		else {
			struct sval rval = compile_unary(ctx);
			while (token_precedence[ctx->token] > precedence || (op == op_exp && ctx->token == tk_exp))
				rval = compile_binary_subexp(ctx, rval);
			rval = sval_extract(ctx, rval);
			sval_pop(ctx, rval);
			sval_pop(ctx, lval);
			emit(ctx, op, ctx->sp, lval.reg, rval.reg);
			lval = sval_value(ctx->sp++);
		}
	}
	return lval;
}

static struct sval compile_binary(struct context* ctx) {
	struct sval lval = compile_unary(ctx);
	while (token_precedence[ctx->token] > 0)
		lval = compile_binary_subexp(ctx, lval);
	return lval;
}

static struct sval compile_ternary(struct context* ctx) {
	struct sval val = compile_binary(ctx);
	if (ctx->token == tk_qmark) {
		next_token(ctx);
		val = sval_extract(ctx, val);
		sval_pop(ctx, val);
		int pc = current_pc(ctx);
		emit_imm(ctx, op_jfalse, val.reg, 0);
		struct sval true_val = compile_ternary(ctx);
		true_val = sval_force_extract(ctx, true_val);
		sval_pop(ctx, true_val);
		int pc2 = current_pc(ctx);
		emit_imm(ctx, op_j, 0, 0);
		require_token(ctx, tk_colon);
		patch_rel(ctx, pc, current_pc(ctx));
		struct sval false_val = compile_ternary(ctx);
		false_val = sval_force_extract(ctx, false_val);
		sval_pop(ctx, false_val);
		patch_rel(ctx, pc2, current_pc(ctx));
		return sval_value(ctx->sp++);
	}
	else
		return val;
}

static struct sval compile_assign(struct context* ctx) {
	struct sval val = compile_ternary(ctx);
	if (ctx->token >= tk_assign_begin && ctx->token <= tk_assign_end) {
		enum opcode op = token_ops[ctx->token];
		next_token(ctx);
		switch (val.type) {
		case vt_value: compile_error(ctx, "Bad assignment.");
		case vt_local: {
			struct sval rval = compile_assign(ctx);
			rval = sval_extract(ctx, rval);
			if (op == op_mov)
				emit(ctx, op, val.reg, rval.reg, 0);
			else
				emit(ctx, op, val.reg, val.reg, rval.reg);
			sval_pop(ctx, rval);
			return sval_local(val.reg);
		}
		default: {
			if (op == op_mov) {
				struct sval rval = compile_assign(ctx);
				rval = sval_extract(ctx, rval);
				sval_set(ctx, val, rval);
				sval_pop(ctx, rval);
				sval_pop(ctx, val);
				if (ctx->sp != rval.reg)
					emit(ctx, op_mov, ctx->sp, rval.reg, 0);
				return sval_value(ctx->sp++);
			}
			else {
				struct sval mval = sval_get(ctx, val);
				struct sval rval = compile_assign(ctx);
				rval = sval_extract(ctx, rval);
				emit(ctx, op, mval.reg, mval.reg, rval.reg);
				sval_set(ctx, val, mval);
				sval_pop(ctx, rval);
				sval_pop(ctx, mval);
				sval_pop(ctx, val);
				if (ctx->sp != mval.reg)
					emit(ctx, op_mov, ctx->sp, mval.reg, 0);
				return sval_value(ctx->sp++);
			}
		}
		}
	}
	else
		return val;
}

static struct sval compile_expression(struct context* ctx) {
	struct sval val = compile_assign(ctx);
	while (ctx->token == tk_comma) {
		next_token(ctx);
		sval_pop(ctx, val);
		val = compile_assign(ctx);
	}
	return val;
}

static void compile_let(struct context* ctx, struct sval* single_sval) {
	int cnt = 0;
	do {
		next_token(ctx);
		check_token(ctx, tk_ident);
		struct sym* sym;
		if (!sym_emplace(&ctx->sym_table, ctx->sym_table.level,
			ctx->token_str_begin, ctx->token_str_end - ctx->token_str_begin, &sym)) {
			compile_error(ctx, "Identifier redefinition.");
		}
		sym->reg = ctx->sp;
		++ctx->sp;
		++ctx->local_sp;
		next_token(ctx);
		if (ctx->token == tk_assign) {
			next_token(ctx);
			struct sval val = compile_assign(ctx);
			val = sval_extract(ctx, val);
			sval_pop(ctx, val);
			sval_set(ctx, sval_local(sym->reg), val);
		}
		else if (single_sval && cnt == 0 && ctx->token != tk_comma) {
			*single_sval = sval_local(sym->reg);
			return;
		}
		else
			emit(ctx, op_kundef, sym->reg, 0, 0);
		++cnt;
	} while (ctx->token == tk_comma);
	if (single_sval)
		*single_sval = sval_null(ctx);
}

static void patch_break_continue(struct context* ctx, int patch_base, int break_pc, int continue_pc, int close_sp) {
	struct cpu* cpu = ctx->cpu;
	int j = patch_base;
	for (int i = patch_base; i < ctx->patch_cnt; i++) {
		struct patch* patch = &ctx->patch[i];
		if (patch->type == pt_break) {
			if (break_pc == -1)
				ctx->patch[j++] = *patch;
			else
				patch_rel(ctx, patch->pc, break_pc);
		}
		else if (patch->type == pt_continue) {
			if (continue_pc == -1)
				ctx->patch[j++] = *patch;
			else
				patch_rel(ctx, patch->pc, continue_pc);
		}
		if (close_sp != -1) {
			struct ins* ins = &((struct ins*)readptr(topcode()->ins))[patch->pc];
			ins->opcode = op_closej;
			ins->op1 = close_sp;
		}
	}
	ctx->patch_cnt = j;
}

static void compile_case_block(struct context* ctx) {
	for (;;) {
		if (ctx->token == tk_eof || ctx->token == tk_case || ctx->token == tk_default || ctx->token == tk_rbrace)
			return;
		if (ctx->token == tk_let)
			compile_error(ctx, "Cannot define variable in unclosed case block.");
		compile_statement(ctx);
	}
}

static void compile_statement(struct context* ctx) {
	switch (ctx->token) {
	case tk_let: {
		compile_let(ctx, NULL);
		break;
	}
	case tk_if: {
		next_token(ctx);
		require_token(ctx, tk_lparen);
		struct sval val = compile_expression(ctx);
		require_token(ctx, tk_rparen);
		val = sval_extract(ctx, val);
		sval_pop(ctx, val);
		int pc = current_pc(ctx);
		emit_imm(ctx, op_jfalse, val.reg, 0);
		compile_statement(ctx);
		if (ctx->token == tk_else) {
			next_token(ctx);
			int pc2 = current_pc(ctx);
			emit_imm(ctx, op_j, 0, 0);
			patch_rel(ctx, pc, current_pc(ctx));
			compile_statement(ctx);
			patch_rel(ctx, pc2, current_pc(ctx));
		}
		else
			patch_rel(ctx, pc, current_pc(ctx));
		return;
	}
	case tk_switch: {
		int old_sp = ctx->sp;
		int old_canbreak = ctx->canbreak;
		int patch_base = ctx->patch_cnt;
		ctx->canbreak = 1;
		next_token(ctx);
		require_token(ctx, tk_lparen);
		struct sval val = compile_expression(ctx);
		require_token(ctx, tk_rparen);
		val = sval_extract(ctx, val);
		require_token(ctx, tk_lbrace);
		int cond_pc = -1, passthrough_pc = -1;
		while (ctx->token == tk_case) {
			next_token(ctx);
			if (cond_pc != -1)
				patch_rel(ctx, cond_pc, current_pc(ctx));
			int old_sp = ctx->sp;
			struct sval cval = compile_expression(ctx);
			cval = sval_extract(ctx, cval);
			require_token(ctx, tk_colon);
			emit(ctx, op_eq, ctx->sp, val.reg, cval.reg);
			cond_pc = current_pc(ctx);
			emit_imm(ctx, op_jfalse, ctx->sp, 0);
			sval_pop(ctx, cval);
			sval_pop(ctx, val);
			if (passthrough_pc != -1)
				patch_rel(ctx, passthrough_pc, current_pc(ctx));
			compile_case_block(ctx);
			passthrough_pc = current_pc(ctx);
			emit_imm(ctx, op_j, 0, 0);
			ctx->sp = old_sp;
		}
		sval_pop(ctx, val);
		if (cond_pc != -1)
			patch_rel(ctx, cond_pc, current_pc(ctx));
		if (ctx->token == tk_default) {
			next_token(ctx);
			require_token(ctx, tk_colon);
			compile_case_block(ctx);
		}
		if (passthrough_pc != -1)
			patch_rel(ctx, passthrough_pc, current_pc(ctx));
		require_token(ctx, tk_rbrace);
		patch_break_continue(ctx, patch_base, current_pc(ctx), -1, old_sp);
		ctx->canbreak = old_canbreak;
		return;
	}
	case tk_for: {
		int old_sp = ctx->sp;
		int old_localsp = ctx->local_sp;
		int old_canbreak = ctx->canbreak;
		int old_cancontinue = ctx->cancontinue;
		int patch_base = ctx->patch_cnt;
		ctx->canbreak = 1;
		ctx->cancontinue = 1;
		next_token(ctx);
		require_token(ctx, tk_lparen);
		sym_push(&ctx->sym_table);
		struct sval for_val = sval_null(ctx);
		if (ctx->token == tk_let)
			compile_let(ctx, &for_val);
		else if (ctx->token != tk_semicolon)
			for_val = compile_expression(ctx);
		int continue_pc;
		if (ctx->token == tk_semicolon) {
			// Regular for loop
			if (for_val.type != vt_null)
				sval_pop(ctx, for_val);
			next_token(ctx);
			int cond_pc = -1, cond_false_pc = -1, cond_true_pc = -1;
			if (ctx->token == tk_semicolon)
				next_token(ctx);
			else {
				cond_pc = current_pc(ctx);
				struct sval cond = compile_expression(ctx);
				require_token(ctx, tk_semicolon);
				cond = sval_extract(ctx, cond);
				sval_pop(ctx, cond);
				cond_false_pc = current_pc(ctx);
				emit_imm(ctx, op_jfalse, cond.reg, 0);
			}
			int update_pc = -1;
			if (ctx->token != tk_rparen) {
				cond_true_pc = current_pc(ctx);
				emit_imm(ctx, op_j, 0, 0);
				update_pc = current_pc(ctx);
				struct sval update = compile_expression(ctx);
				sval_pop(ctx, update);
				if (cond_pc != -1)
					emit_rel(ctx, op_j, 0, cond_pc);
				patch_rel(ctx, cond_true_pc, current_pc(ctx));
			}
			require_token(ctx, tk_rparen);
			int loop_pc = current_pc(ctx);
			compile_statement(ctx);
			if (sym_level_needclose(&ctx->sym_table))
				emit(ctx, op_close, old_sp, 0, 0);
			if (update_pc != -1)
				continue_pc = update_pc;
			else if (cond_pc != -1)
				continue_pc = cond_pc;
			else
				continue_pc = loop_pc;
			emit_rel(ctx, op_j, 0, continue_pc);
			if (cond_false_pc != -1)
				patch_rel(ctx, cond_false_pc, current_pc(ctx));
		}
		else if (ctx->token == tk_of && for_val.type != vt_null) {
			// for...of loop
			next_token(ctx);
			struct sval iterable = compile_expression(ctx);
			iterable = sval_extract(ctx, iterable);
			sval_pop(ctx, iterable);
			int iterable_reg = ctx->sp++;
			emit(ctx, op_iter, iterable_reg, iterable.reg, 0);
			int done = ctx->sp++;
			ctx->local_sp += 2;
			continue_pc = current_pc(ctx);
			if (for_val.type == vt_local)
				emit(ctx, op_next, done, for_val.reg, iterable_reg);
			else {
				struct sval val = sval_local(ctx->sp++);
				emit(ctx, op_next, done, val.reg, iterable_reg);
				sval_set(ctx, for_val, val);
				sval_pop(ctx, val);
			}
			int cond_pc = current_pc(ctx);
			emit_rel(ctx, op_jtrue, done, 0);
			require_token(ctx, tk_rparen);
			compile_statement(ctx);
			if (sym_level_needclose(&ctx->sym_table))
				emit(ctx, op_close, old_sp, 0, 0);
			emit_rel(ctx, op_j, 0, continue_pc);
			patch_rel(ctx, cond_pc, current_pc(ctx));
		}
		else
			compile_error(ctx, "Unexpected token.");
		sym_pop(&ctx->sym_table);
		ctx->sp = old_sp;
		ctx->local_sp = old_localsp;
		ctx->canbreak = old_canbreak;
		ctx->cancontinue = old_cancontinue;
		patch_break_continue(ctx, patch_base, current_pc(ctx), continue_pc, old_sp);
		return;
	}
	case tk_while: {
		int old_sp = ctx->sp;
		int old_canbreak = ctx->canbreak;
		int old_cancontinue = ctx->cancontinue;
		int patch_base = ctx->patch_cnt;
		ctx->canbreak = 1;
		ctx->cancontinue = 1;
		next_token(ctx);
		require_token(ctx, tk_lparen);
		int loop_pc = current_pc(ctx);
		struct sval val = compile_expression(ctx);
		val = sval_extract(ctx, val);
		sval_pop(ctx, val);
		int cond_pc = current_pc(ctx);
		emit_imm(ctx, op_jfalse, val.reg, 0);
		require_token(ctx, tk_rparen);
		compile_statement(ctx);
		emit_rel(ctx, op_j, 0, loop_pc);
		patch_rel(ctx, cond_pc, current_pc(ctx));
		ctx->canbreak = old_canbreak;
		ctx->cancontinue = old_cancontinue;
		patch_break_continue(ctx, patch_base, current_pc(ctx), loop_pc, old_sp);
		return;
	}
	case tk_do: {
		int old_sp = ctx->sp;
		int old_canbreak = ctx->canbreak;
		int old_cancontinue = ctx->cancontinue;
		int patch_base = ctx->patch_cnt;
		ctx->canbreak = 1;
		ctx->cancontinue = 1;
		next_token(ctx);
		int loop_pc = current_pc(ctx);
		compile_statement(ctx);
		require_token(ctx, tk_while);
		require_token(ctx, tk_lparen);
		int cond_pc = current_pc(ctx);
		struct sval val = compile_expression(ctx);
		val = sval_extract(ctx, val);
		sval_pop(ctx, val);
		emit_rel(ctx, op_jtrue, val.reg, loop_pc);
		require_token(ctx, tk_rparen);
		ctx->canbreak = old_canbreak;
		ctx->cancontinue = old_cancontinue;
		patch_break_continue(ctx, patch_base, current_pc(ctx), cond_pc, old_sp);
		break;
	}
	case tk_break: {
		next_token(ctx);
		if (!ctx->canbreak)
			compile_error(ctx, "Cannot break.");
		add_patch(ctx, pt_break, current_pc(ctx));
		emit_imm(ctx, op_j, 0, 0);
		break;
	}
	case tk_continue: {
		next_token(ctx);
		if (!ctx->cancontinue)
			compile_error(ctx, "Cannot continue.");
		add_patch(ctx, pt_continue, current_pc(ctx));
		emit_imm(ctx, op_j, 0, 0);
		break;
	}
	case tk_function:
		compile_function(ctx, 1);
		return;
	case tk_return: {
		next_token(ctx);
		if (ctx->token == tk_semicolon)
			emit(ctx, op_retu, 0, 0, 0);
		else {
			struct sval val = compile_expression(ctx);
			val = sval_extract(ctx, val);
			sval_pop(ctx, val);
			emit(ctx, op_ret, val.reg, 0, 0);
		}
		break;
	}
	case tk_lbrace: {
		next_token(ctx);
		compile_block(ctx);
		require_token(ctx, tk_rbrace);
		return;
	}
	case tk_semicolon:
		break;
	default: {
		struct sval val = compile_expression(ctx);
		sval_pop(ctx, val);
		break;
	}
	}
	require_token(ctx, tk_semicolon);
}

static void compile_block(struct context* ctx) {
	int old_sp = ctx->sp;
	sym_push(&ctx->sym_table);
	while (ctx->token != tk_eof && ctx->token != tk_rbrace)
		compile_statement(ctx);
	if (sym_level_needclose(&ctx->sym_table))
		emit(ctx, op_close, old_sp, 0, 0);
	sym_pop(&ctx->sym_table);
	ctx->sp = old_sp;
	ctx->local_sp = old_sp;
}

struct compile_err compile(struct cpu* cpu, const char* code, int codelen) {
	struct context ctx;
	ctx.err.msg = NULL;
	ctx.alloc = &cpu->alloc;
	ctx.cpu = cpu;
	ctx.code = code;
	ctx.codelen = codelen;
	ctx.codep = 0;
	ctx.lastlinenum = 0;
	ctx.lastlineins = 0;
	ctx.linenum = 0;
	ctx.sp = 4;
	ctx.local_sp = 4;
	ctx.sbuf = NULL;
	ctx.sbuf_cnt = 0;
	ctx.sbuf_cap = 0;
	sym_init(&ctx.sym_table);
	next_char(&ctx);
	next_token(&ctx);
	struct functx func;
	func.enfunc = NULL;
	func.sym_level = 0;
	func.code_id = add_code(cpu);
	ctx.topfunc = &func;
	ctx.canbreak = 0;
	ctx.cancontinue = 0;
	ctx.patch = NULL;
	ctx.patch_cnt = 0;
	ctx.patch_cap = 0;
	if (setjmp(ctx.jmp_buf) == 0) {
		compile_block(&ctx);
		if (ctx.token == tk_rbrace)
			compile_error(&ctx, "Unexpected right brace.");
		if (ctx.token != tk_eof)
			compile_error(&ctx, "Unexpected token.");
		emit(&ctx, op_retu, 0, 0, 0);
		struct funcobj* topfunc = (struct funcobj*)mem_malloc(&cpu->alloc, sizeof(struct funcobj));
		topfunc->code = writeptr(&((struct code*)readptr(cpu->code))[func.code_id]);
		cpu->topfunc = writeptr(topfunc);
		mem_free(ctx.alloc, ctx.sbuf);
	}

	return ctx.err;
}
