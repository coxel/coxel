#include "buf.h"
#include "gfx.h"
#include "lib.h"
#include "platform.h"
#include "rand.h"
#include "str.h"
#include "tab.h"

#include <string.h>

struct value lib_btn(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	int btn = btn_translate(cpu, to_string(cpu, ARG(0)));
	int player = 0;
	if (nargs == 2)
		player = num_int(to_number(cpu, ARG(1)));
	return value_bool(cpu, btn_is_down(btn, player));
}

struct value lib_btnp(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	int btn = btn_translate(cpu, to_string(cpu, ARG(0)));
	int player = 0;
	if (nargs == 2)
		player = num_int(to_number(cpu, ARG(1)));
	return value_bool(cpu, btn_is_pressed(btn, player));
}

struct value lib_cls(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 1)
		argument_error(cpu);
	int c = -1;
	if (nargs == 1)
		c = num_int(to_number(cpu, ARG(0)));
	gfx_cls(console_getgfx(), c);
	return value_undef(cpu);
}

struct value lib_camera(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 2)
		argument_error(cpu);
	if (nargs == 0)
		gfx_camera(console_getgfx(), 0, 0);
	else {
		int x = num_int(to_number(cpu, ARG(0)));
		int y = num_int(to_number(cpu, ARG(1)));
		gfx_camera(console_getgfx(), x, y);
	}
	return value_undef(cpu);
}

struct value lib_pget(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH)
		return value_undef(cpu);
	else
		return value_num(cpu, num_int(gfx_getpixel(console_getgfx(), x, y)));
}

struct value lib_pset(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2 && nargs != 3)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	int c = 15;
	if (nargs == 3)
		c = num_int(to_number(cpu, ARG(2)));
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && c >= 0 && c <= 15)
		gfx_setpixel(console_getgfx(), x, y, c);
	return value_undef(cpu);
}

struct value lib_line(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 4 && nargs != 5)
		argument_error(cpu);
	int x1 = num_int(to_number(cpu, ARG(0)));
	int y1 = num_int(to_number(cpu, ARG(1)));
	int x2 = num_int(to_number(cpu, ARG(2)));
	int y2 = num_int(to_number(cpu, ARG(3)));
	int c = -1;
	if (nargs == 5)
		c = num_int(to_number(cpu, ARG(4)));
	gfx_line(console_getgfx(), x1, y1, x2, y2, c);
	return value_undef(cpu);
}

struct value lib_rect(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 4 && nargs != 5)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	int w = num_int(to_number(cpu, ARG(2)));
	int h = num_int(to_number(cpu, ARG(3)));
	int c = -1;
	if (nargs == 5)
		c = num_int(to_number(cpu, ARG(4)));
	gfx_rect(console_getgfx(), x, y, w, h, c);
	return value_undef(cpu);
}

struct value lib_fillRect(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 4 && nargs != 5)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	int w = num_int(to_number(cpu, ARG(2)));
	int h = num_int(to_number(cpu, ARG(3)));
	int c = -1;
	if (nargs == 5)
		c = num_int(to_number(cpu, ARG(4)));
	gfx_fill_rect(console_getgfx(), x, y, w, h, c);
	return value_undef(cpu);
}

struct value lib_spr(struct cpu* cpu, int sp, int nargs) {
	if (nargs < 3 || nargs > 5)
		argument_error(cpu);
	int n = num_int(to_number(cpu, ARG(0)));
	int x = num_int(to_number(cpu, ARG(1)));
	int y = num_int(to_number(cpu, ARG(2)));
	int w = 16;
	int h = 16;
	if (nargs >= 4) {
		w = num_int(num_mul(num_kint(SPRITE_WIDTH), to_number(cpu, ARG(3))));
		if (nargs >= 5)
			h = num_int(num_mul(num_kint(SPRITE_HEIGHT), to_number(cpu, ARG(4))));
	}
	int s = SPRITESHEET_WIDTH / SPRITE_WIDTH;
	int sx = n % s * SPRITE_WIDTH;
	int sy = n / s * SPRITE_HEIGHT;
	gfx_spr(console_getgfx(), sx, sy, x, y, w, h);
	return value_undef(cpu);
}

struct value lib_print(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 3 && nargs != 4)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, ARG(0));
	int x = -1;
	int y = -1;
	int c = -1;
	if (nargs >= 3) {
		x = num_int(to_number(cpu, ARG(1)));
		y = num_int(to_number(cpu, ARG(2)));
	}
	if (nargs == 4)
		c = num_int(to_number(cpu, ARG(3)));
	gfx_print(console_getgfx(), str->data, str->len, x, y, c);
	return value_undef(cpu);
}

struct value lib_abs(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number num = to_number(cpu, ARG(0));
	return value_num(cpu, num_abs(num));
}

struct value lib_max(struct cpu* cpu, int sp, int nargs) {
	number ret = 0x80000000;
	for (int i = 0; i < nargs; i++) {
		number cur = to_number(cpu, ARG(i));
		if ((int32_t)cur > (int32_t)ret)
			ret = cur;
	}
	return value_num(cpu, ret);
}

struct value lib_min(struct cpu* cpu, int sp, int nargs) {
	number ret = 0x7FFFFFFF;
	for (int i = 0; i < nargs; i++) {
		number cur = to_number(cpu, ARG(i));
		if ((int32_t)cur < (int32_t)ret)
			ret = cur;
	}
	return value_num(cpu, ret);
}

struct value lib_ceil(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	return value_num(cpu, num_ceil(value));
}

struct value lib_floor(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	return value_num(cpu, num_floor(value));
}

struct value lib_srand(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	rand_seed(cpu, value);
	return value_undef(cpu);
}

struct value lib_rand(struct cpu* cpu, int sp, int nargs) {
	if (nargs == 0)
		return value_num(cpu, rand_int(cpu, num_kint(1) - 1));
	else if (nargs == 1) {
		uint32_t max = to_number(cpu, ARG(0)) - 1;
		return value_num(cpu, rand_int(cpu, max));
	}
	else if (nargs == 2) {
		number min = to_number(cpu, ARG(0));
		number max = to_number(cpu, ARG(1));
		return value_num(cpu, min + rand_int(cpu, max - min - 1));
	}
	else
		argument_error(cpu);
}

struct value devlib_key(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* key = to_string(cpu, ARG(0));
	enum key k = key_translate(cpu, key);
	return value_bool(cpu, key_is_down(k));
}

struct value devlib_keyp(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* key = to_string(cpu, ARG(0));
	enum key k = key_translate(cpu, key);
	return value_bool(cpu, key_is_pressed(k));
}

struct value devlib_mpos(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct tabobj* tab = tab_new(cpu);
	struct io* io = console_getio();
	tab_set(cpu, tab, str_intern(cpu, "x", 1), value_num(cpu, io->mousex));
	tab_set(cpu, tab, str_intern(cpu, "y", 1), value_num(cpu, io->mousey));
	return value_tab(cpu, tab);
}

struct value devlib_mwheel(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct io* io = console_getio();
	return value_num(cpu, num_kint(io->mousewheel));
}

struct value devlib_input(struct cpu* cpu, int sp, int nargs) {
	struct io* io = console_getio();
	struct strobj* str;
	if (io->input_size == 0)
		str = LIT(EMPTY);
	else
		str = str_intern(cpu, io->input, io->input_size);
	return value_str(cpu, str);
}

struct value devlib_copy(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, ARG(0));
	platform_copy(str->data, str->len);
	return value_undef(cpu);
}

struct value devlib_paste(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct io* io = console_getio();
	int avail = io->keys[kc_ctrl] && io->keys[kc_v];
	if (!avail)
		return value_str(cpu, LIT(EMPTY));
	int len = STR_MAXLEN;
	char* buf = mem_malloc(&cpu->alloc, len);
	if (buf == NULL)
		out_of_memory_error(cpu);
	int r = platform_paste(buf, len);
	struct value ret;
	if (r < 0)
		ret = value_undef(cpu);
	else
		ret = value_str(cpu, str_intern(cpu, buf, r));
	mem_free(&cpu->alloc, buf);
	return ret;
}

struct value devlib_newbuf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	int size = num_uint(to_number(cpu, ARG(0)));
	return value_buf(cpu, buf_new(cpu, size));
}

static struct cart get_cartobj(struct cpu* cpu, struct tabobj* tab) {
	struct cart cart;
	struct value code_value = tab_get(cpu, tab, str_intern(cpu, "code", 4));
	if (code_value.type != t_str)
		argument_error(cpu);
	struct strobj* code = to_string(cpu, code_value);
	cart.code = code->data;
	cart.codelen = code->len;
	struct value sprite_value = tab_get(cpu, tab, str_intern(cpu, "sprite", 6));
	if (sprite_value.type != t_buf)
		cart.sprite = NULL;
	else {
		struct bufobj* buf = (struct bufobj*)readptr(sprite_value.buf);
		if (buf->len != SPRITESHEET_BYTES)
			cart.sprite = NULL;
		else
			cart.sprite = buf->data;
	}
	return cart;
}

static struct tabobj* parse_cartobj(struct cpu* cpu, const struct cart* cart) {
	struct tabobj* tab = tab_new(cpu);
	struct value code_value = value_str(cpu, str_intern(cpu, cart->code, cart->codelen));
	tab_set(cpu, tab, str_intern(cpu, "code", 4), code_value);
	if (cart->sprite) {
		struct value sprite_value = value_buf(cpu, buf_new_copydata(cpu, cart->sprite, SPRITESHEET_BYTES));
		tab_set(cpu, tab, str_intern(cpu, "sprite", 6), sprite_value);
	}
	return tab;
}

static const char* get_cart_filename(struct cpu* cpu, struct strobj* str, char* buf, int buflen) {
	/* Add file extension if not present */
	int ext = 0;
	if (str->len < 4
		|| str->data[str->len - 4] != '.'
		|| str->data[str->len - 3] != 'c'
		|| str->data[str->len - 2] != 'o'
		|| str->data[str->len - 1] != 'x')
		ext = 4;
	if (str->len + ext + 1 > buflen)
		runtime_error(cpu, "Filename too long.");
	memcpy(buf, str->data, str->len);
	if (ext)
		memcpy(buf + str->len, ".cox", 4);
	buf[str->len + ext] = 0;
	return buf;
}

static struct value get_run_result(struct cpu* cpu, struct run_result result) {
	struct tabobj* ret = tab_new(cpu);
	if (result.err != NULL) {
		struct value err_value = value_str(cpu, str_intern(cpu, result.err, (int)strlen(result.err)));
		tab_set(cpu, ret, str_intern(cpu, "err", 3), err_value);
		struct value line_value = value_num(cpu, num_kuint(result.linenum));
		tab_set(cpu, ret, str_intern(cpu, "line", 4), line_value);
	}
	return value_tab(cpu, ret);
}

struct value devlib_run(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct tabobj* tab = to_tab(cpu, ARG(0));
	struct cart cart = get_cartobj(cpu, tab);
	struct run_result result = console_run(&cart);
	return get_run_result(cpu, result);
}

struct value devlib_save(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	struct strobj* filename = to_string(cpu, ARG(0));
	struct tabobj* tab = to_tab(cpu, ARG(1));
	struct cart cart = get_cartobj(cpu, tab);
	char filename_buf[256];
	struct run_result result = console_save(get_cart_filename(cpu, filename, filename_buf, 256), &cart);
	return get_run_result(cpu, result);
}

struct value devlib_load(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* filename = to_string(cpu, ARG(0));
	char filename_buf[256];
	struct cart cart;
	struct run_result result = console_load(get_cart_filename(cpu, filename, filename_buf, 256), &cart);
	if (result.err != NULL)
		return get_run_result(cpu, result);
	else {
		struct tabobj* tab = parse_cartobj(cpu, &cart);
		cart_destroy(&cart);
		return value_tab(cpu, tab);
	}
}

struct value devlib_fastParse(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, ARG(0));
	int line = num_uint(to_number(cpu, ARG(1)));
	const char* code = str->data;
	enum parse_type {
		pt_none,
		pt_string,
		pt_comment,
	} token = pt_none;
	int token_end = 0;
	int line_start = 0;
	for (int i = 0; i < str->len; i++) {
		if (line <= 0)
			break;
		else if (code[i] == '\n') {
			line--;
			if (line == 0)
				line_start = i + 1;
		}
		else if (code[i] == '\'' || code[i] == '"') {
			char quote = code[i];
			for (++i; i < str->len; i++) {
				if (code[i] == '\n') {
					line--;
					if (line == 0)
						line_start = i + 1;
					break;
				}
				else if (code[i] == quote)
					break;
				if (i + 1 < str->len && code[i] == '\\' && code[i + 1] != '\n')
					i++;
			}
			token = pt_string;
			token_end = i + 1;
		}
		else if (i + 1 < str->len && code[i] == '/') {
			if (code[i + 1] == '/') {
				for (i += 2; i < str->len; i++)
					if (code[i] == '\n') {
						line--;
						if (line == 0)
							line_start = i + 1;
						break;
					}
				token = pt_comment;
				token_end = i + 1;
			}
			else if (code[i + 1] == '*') {
				for (i += 2; i < str->len; i++) {
					if (code[i] == '\n') {
						line--;
						if (line == 0)
							line_start = i + 1;
					}
					else if (i + 1 < str->len && code[i] == '*' && code[i + 1] == '/')
						break;
				}
				token = pt_comment;
				token_end = i + 1;
			}
		}
	}
	struct tabobj* tab = tab_new(cpu);
	struct strobj* token_str = NULL;
	switch (token) {
	case pt_none: token_str = str_intern(cpu, "none", 4); break;
	case pt_string: token_str = str_intern(cpu, "string", 6); break;
	case pt_comment: token_str = str_intern(cpu, "comment", 7); break;
	}
	tab_set(cpu, tab, str_intern(cpu, "token", 5), value_str(cpu, token_str));
	tab_set(cpu, tab, str_intern(cpu, "tokenEnd", 8), value_num(cpu, num_kuint(token_end)));
	tab_set(cpu, tab, str_intern(cpu, "lineStart", 9), value_num(cpu, num_kuint(line_start)));
	return value_tab(cpu, tab);
}

struct libdef {
	const char* name;
	enum cfuncname func;
};

static const struct libdef libdefs[] = {
	{"btn", cf_lib_btn },
	{"btnp", cf_lib_btnp },
	{"cls", cf_lib_cls },
	{"camera", cf_lib_camera },
	{"pget", cf_lib_pget },
	{"pset", cf_lib_pset },
	{"line", cf_lib_line },
	{"rect", cf_lib_rect },
	{"fillRect", cf_lib_fillRect },
	{"spr", cf_lib_spr },
	{"print", cf_lib_print },
	{"abs", cf_lib_abs },
	{"max", cf_lib_max },
	{"min", cf_lib_min },
	{"ceil", cf_lib_ceil },
	{"floor", cf_lib_floor },
	{"srand", cf_lib_srand },
	{"rand", cf_lib_rand },
	{ NULL, 0 },
};

static const struct libdef devlibdefs[] = {
	{"dev_key", cf_devlib_key },
	{"dev_keyp", cf_devlib_keyp },
	{"dev_mpos", cf_devlib_mpos },
	{"dev_mwheel", cf_devlib_mwheel },
	{"dev_input", cf_devlib_input },
	{"dev_copy", cf_devlib_copy },
	{"dev_paste", cf_devlib_paste },
	{"dev_newbuf", cf_devlib_newbuf },
	{"dev_fastParse", cf_devlib_fastParse },
	{"dev_run", cf_devlib_run },
	{"dev_save", cf_devlib_save },
	{"dev_load", cf_devlib_load },
	{ NULL, 0 },
};

void lib_init(struct cpu* cpu) {
	struct tabobj* globals = (struct tabobj*)readptr(cpu->globals);
	for (int i = 0; libdefs[i].name; i++) {
		struct strobj* key = str_intern(cpu, libdefs[i].name, (int)strlen(libdefs[i].name));
		tab_set(cpu, globals, key, value_cfunc(cpu, libdefs[i].func));
	}
	for (int i = 0; devlibdefs[i].name; i++) {
		struct strobj* key = str_intern(cpu, devlibdefs[i].name, (int)strlen(devlibdefs[i].name));
		tab_set(cpu, globals, key, value_cfunc(cpu, devlibdefs[i].func));
	}
	rand_seed(cpu, platform_seed());
}
