#include "arr.h"
#include "buf.h"
#include "gfx.h"
#include "lib.h"
#include "menu.h"
#include "platform.h"
#include "rand.h"
#include "str.h"
#include "tab.h"

#include <stdlib.h>
#include <string.h>

value_t lib_btn(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	int btn = btn_translate(cpu, to_string(cpu, ARG(0)));
	int player = 0;
	if (nargs == 2)
		player = num_int(to_number(cpu, ARG(1)));
	return value_bool(btn_is_down(btn, player));
}

value_t lib_btnp(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	int btn = btn_translate(cpu, to_string(cpu, ARG(0)));
	int player = 0;
	if (nargs == 2)
		player = num_int(to_number(cpu, ARG(1)));
	return value_bool(btn_is_pressed(btn, player));
}

value_t lib_cls(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 1)
		argument_error(cpu);
	int c = -1;
	if (nargs == 1)
		c = num_int(to_number(cpu, ARG(0)));
	gfx_cls(console_getgfx(), c);
	cpu->cycles -= CYCLES_PIXELS(WIDTH * HEIGHT);
	return value_undef();
}

value_t lib_camera(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 2)
		argument_error(cpu);
	if (nargs == 0)
		gfx_camera(console_getgfx(), 0, 0);
	else {
		int x = num_int(to_number(cpu, ARG(0)));
		int y = num_int(to_number(cpu, ARG(1)));
		gfx_camera(console_getgfx(), x, y);
	}
	return value_undef();
}

value_t lib_pal(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 2)
		argument_error(cpu);
	struct gfx* gfx = console_getgfx();
	if (nargs == 0) {
		gfx_reset_pal(gfx);
		gfx_reset_palt(gfx);
	}
	else {
		int c = num_int(to_number(cpu, ARG(0)));
		int c1 = num_int(to_number(cpu, ARG(1)));
		if (c >= 0 && c <= 15 && c1 >= 0 && c1 <= 15)
			gfx_pal(gfx, c, c1);
	}
	return value_undef();
}

value_t lib_palt(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0 && nargs != 2)
		argument_error(cpu);
	struct gfx* gfx = console_getgfx();
	if (nargs == 0)
		gfx_reset_palt(gfx);
	else {
		int c = num_int(to_number(cpu, ARG(0)));
		int t = to_bool(cpu, ARG(1));
		if (c >= 0 && c <= 15)
			gfx_palt(gfx, c, t);
	}
	return value_undef();
}

value_t lib_pget(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH)
		return value_undef();
	else
		return value_num(num_int(gfx_getpixel(console_getgfx(), x, y)));
}

value_t lib_pset(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2 && nargs != 3)
		argument_error(cpu);
	int x = num_int(to_number(cpu, ARG(0)));
	int y = num_int(to_number(cpu, ARG(1)));
	int c = 15;
	if (nargs == 3)
		c = num_int(to_number(cpu, ARG(2)));
	if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && c >= 0 && c <= 15)
		gfx_setpixel(console_getgfx(), x, y, c);
	return value_undef();
}

value_t lib_line(struct cpu* cpu, int sp, int nargs) {
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
	cpu->cycles -= CYCLES_PIXELS(abs(x1 - x2) + abs(y1 - y2) + 1);
	return value_undef();
}

value_t lib_rect(struct cpu* cpu, int sp, int nargs) {
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
	cpu->cycles -= CYCLES_PIXELS(2 * (w + h));
	return value_undef();
}

value_t lib_fillRect(struct cpu* cpu, int sp, int nargs) {
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
	cpu->cycles -= CYCLES_PIXELS(w * h);
	return value_undef();
}

value_t lib_spr(struct cpu* cpu, int sp, int nargs) {
	if (nargs < 3 || nargs > 6)
		argument_error(cpu);
	int n = num_int(to_number(cpu, ARG(0)));
	int x = num_int(to_number(cpu, ARG(1)));
	int y = num_int(to_number(cpu, ARG(2)));
	int w = SPRITE_WIDTH;
	int h = SPRITE_HEIGHT;
	int r = 0;
	if (nargs >= 4)
		w = num_int(num_mul(num_kint(SPRITE_WIDTH), to_number(cpu, ARG(3))));
	if (nargs >= 5)
		h = num_int(num_mul(num_kint(SPRITE_HEIGHT), to_number(cpu, ARG(4))));
	if (nargs >= 6) {
		r = num_int(to_number(cpu, ARG(5)));
		if (r % 90 != 0)
			argument_error(cpu);
	}
	int s = SPRITESHEET_WIDTH / SPRITE_WIDTH;
	int sx = n % s * SPRITE_WIDTH;
	int sy = n / s * SPRITE_HEIGHT;
	gfx_spr(console_getgfx(), sx, sy, w, h, x, y, w, h, r);
	cpu->cycles -= CYCLES_PIXELS(w * h);
	return value_undef();
}

value_t lib_sspr(struct cpu* cpu, int sp, int nargs) {
	if (nargs < 6 || nargs > 9)
		argument_error(cpu);
	int sx = num_int(to_number(cpu, ARG(0)));
	int sy = num_int(to_number(cpu, ARG(1)));
	int sw = num_int(to_number(cpu, ARG(2)));
	int sh = num_int(to_number(cpu, ARG(3)));
	int x = num_int(to_number(cpu, ARG(4)));
	int y = num_int(to_number(cpu, ARG(5)));
	int w = sw, h = sh;
	int r = 0;
	if (nargs >= 7)
		w = num_int(to_number(cpu, ARG(6)));
	if (nargs >= 8)
		h = num_int(to_number(cpu, ARG(7)));
	if (nargs >= 9) {
		r = num_int(to_number(cpu, ARG(8)));
		if (r % 90 != 0)
			argument_error(cpu);
	}
	gfx_spr(console_getgfx(), sx, sy, sw, sh, x, y, w, h, r);
	cpu->cycles -= CYCLES_PIXELS(w * h);
	return value_undef();
}

value_t lib_print(struct cpu* cpu, int sp, int nargs) {
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
	cpu->cycles -= CYCLES_PIXELS(16 * str->len);
	return value_undef();
}

value_t lib_abs(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number num = to_number(cpu, ARG(0));
	return value_num(num_abs(num));
}

value_t lib_max(struct cpu* cpu, int sp, int nargs) {
	number ret = NUM_MIN;
	for (int i = 0; i < nargs; i++) {
		number cur = to_number(cpu, ARG(i));
		if ((int32_t)cur > (int32_t)ret)
			ret = cur;
	}
	return value_num(ret);
}

value_t lib_min(struct cpu* cpu, int sp, int nargs) {
	number ret = NUM_MAX;
	for (int i = 0; i < nargs; i++) {
		number cur = to_number(cpu, ARG(i));
		if ((int32_t)cur < (int32_t)ret)
			ret = cur;
	}
	return value_num(ret);
}

value_t lib_ceil(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	return value_num(num_ceil(value));
}

value_t lib_floor(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	return value_num(num_floor(value));
}

value_t lib_srand(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	number value = to_number(cpu, ARG(0));
	rand_seed(cpu, value);
	return value_undef();
}

value_t lib_rand(struct cpu* cpu, int sp, int nargs) {
	if (nargs == 0)
		return value_num(rand_int(cpu, (1 << FRAC_BITS) - 1) << FRAC_SHIFT_BITS);
	else if (nargs == 1) {
		uint32_t max = to_number(cpu, ARG(0)) >> FRAC_SHIFT_BITS;
		return value_num(rand_int(cpu, max - 1) << FRAC_SHIFT_BITS);
	}
	else if (nargs == 2) {
		number min = to_number(cpu, ARG(0));
		number max = to_number(cpu, ARG(1));
		if ((int32_t)min >= (int32_t)max)
			return value_num(min);
		uint32_t rmax = (num_sub(max, min)) >> FRAC_SHIFT_BITS;
		return value_num(num_add(min, (rand_int(cpu, rmax - 1) << FRAC_SHIFT_BITS)));
	}
	else
		argument_error(cpu);
}

value_t lib_statCpu(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	number usage = num_of_division(CYCLES_PER_FRAME - cpu->cycles, CYCLES_PER_FRAME);
	usage = num_add(usage, num_kint(cpu->delayed_frames));
	return value_num(usage);
}

value_t lib_statMem(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	number usage = num_of_division(cpu->alloc.used_memory, 1024);
	return value_num(usage);
}

value_t devlib_key(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* key = to_string(cpu, ARG(0));
	enum key k = key_translate(cpu, key);
	return value_bool(key_is_down(k));
}

value_t devlib_keyp(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* key = to_string(cpu, ARG(0));
	enum key k = key_translate(cpu, key);
	return value_bool(key_is_pressed(k));
}

value_t devlib_mpos(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct tabobj* tab = tab_new(cpu);
	struct io* io = console_getio();
	tab_set(cpu, tab, str_intern(cpu, "x", 1), value_num(io->mousex));
	tab_set(cpu, tab, str_intern(cpu, "y", 1), value_num(io->mousey));
	cpu->cycles -= CYCLES_ALLOC + CYCLES_LOOKUP * 2;
	return value_tab(tab);
}

value_t devlib_mwheel(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct io* io = console_getio();
	return value_num(num_kint(io->mousewheel));
}

value_t devlib_input(struct cpu* cpu, int sp, int nargs) {
	struct io* io = console_getio();
	struct strobj* str;
	if (io->input_size == 0)
		str = LIT(EMPTY);
	else
		str = str_intern(cpu, io->input, io->input_size);
	cpu->cycles -= CYCLES_CHARS(io->input_size);
	return value_str(str);
}

value_t devlib_copy(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, ARG(0));
	platform_copy(str->data, str->len);
	cpu->cycles -= CYCLES_CHARS(str->len);
	return value_undef();
}

value_t devlib_paste(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	struct io* io = console_getio();
	int avail = io->keys[kc_ctrl] && io->keys[kc_v];
	if (!avail)
		return value_str(LIT(EMPTY));
	int len = STR_MAXLEN;
	char* buf = mem_alloc(&cpu->alloc, len);
	if (buf == NULL)
		out_of_memory_error(cpu);
	int r = platform_paste(buf, len);
	value_t ret = value_str(str_intern(cpu, buf, r));
	mem_dealloc(&cpu->alloc, buf);
	cpu->cycles -= CYCLES_CHARS(r);
	return ret;
}

value_t devlib_newbuf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	int size = num_uint(to_number(cpu, ARG(0)));
	cpu->cycles -= CYCLES_ALLOC;
	return value_buf(buf_new(cpu, size));
}

static struct cart get_cartobj(struct cpu* cpu, struct tabobj* tab) {
	struct cart cart;
	value_t code_value = tab_get(cpu, tab, str_intern(cpu, "code", 4));
	if (value_get_type(code_value) != t_str)
		argument_error(cpu);
	struct strobj* code = to_string(cpu, code_value);
	cart.code = code->data;
	cart.codelen = code->len;
	value_t sprite_value = tab_get(cpu, tab, str_intern(cpu, "sprite", 6));
	if (value_get_type(sprite_value) != t_buf)
		cart.sprite = NULL;
	else {
		struct bufobj* buf = (struct bufobj*)value_get_object(sprite_value);
		if (buf->len != SPRITESHEET_BYTES)
			cart.sprite = NULL;
		else
			cart.sprite = (const char*)buf->data;
	}
	value_t assets_value = tab_get(cpu, tab, str_intern(cpu, "asset", 5));
	if (value_get_type(assets_value) != t_arr) {
		cart.assets = NULL;
		cart.asset_cnt = 0;
	}
	else {
		// TODO: Memory leak when runtime error
		struct arrobj* assets = (struct arrobj*)value_get_object(assets_value);
		cart.asset_cnt = assets->len;
		cart.assets = (struct asset*)platform_malloc_fast(sizeof(struct asset) * cart.asset_cnt);
		for (int i = 0; i < cart.asset_cnt; i++) {
			struct asset* cart_asset = &cart.assets[i];
			value_t asset_value = ((value_t*)readptr(assets->data))[i];
			if (value_get_type(asset_value) != t_tab)
				runtime_error(cpu, "Asset is not an object.");
			struct tabobj* asset = (struct tabobj*)value_get_object(asset_value);
			value_t name_value = tab_get(cpu, asset, str_intern(cpu, "name", 4));
			if (value_get_type(name_value) != t_str)
				runtime_error(cpu, "Asset name is not a string.");
			struct strobj* name_str = (struct strobj*)value_get_object(name_value);
			memcpy(cart_asset->name, name_str->data, name_str->len);
			value_t type_value = tab_get(cpu, asset, str_intern(cpu, "type", 4));
			if (value_get_type(type_value) != t_str)
				runtime_error(cpu, "Asset type is not a string.");
			struct strobj* type_str = (struct strobj*)value_get_object(type_value);
			if (str_equal(type_str->data, type_str->len, "map", 3)) {
				value_t width_value = tab_get(cpu, asset, str_intern(cpu, "width", 5));
				if (!value_is_num(width_value))
					runtime_error(cpu, "Map width is not a number.");
				value_t height_value = tab_get(cpu, asset, str_intern(cpu, "height", 6));
				if (!value_is_num(height_value))
					runtime_error(cpu, "Map height is not a number.");
				value_t data_value = tab_get(cpu, asset, str_intern(cpu, "data", 4));
				if (value_get_type(data_value) != t_buf)
					runtime_error(cpu, "Map data is not a buffer.");
				int width = num_uint(value_get_num(width_value));
				int height = num_uint(value_get_num(height_value));
				if (width * height > MAX_MAP_SIZE)
					runtime_error(cpu, "Map too large");
				struct bufobj* data = (struct bufobj*)value_get_object(data_value);
				if (width * height != data->len)
					runtime_error(cpu, "Map and buffer size mismatch.");
				cart_asset->map.width = width;
				cart_asset->map.height = height;
				cart_asset->map.data = data->data;
			}
			else
				runtime_error(cpu, "Unrecognized asset type.");
		}
	}
	return cart;
}

static struct tabobj* parse_cartobj(struct cpu* cpu, const struct cart* cart) {
	struct tabobj* tab = tab_new(cpu);
	value_t code_value = value_str(str_intern(cpu, cart->code, cart->codelen));
	tab_set(cpu, tab, str_intern(cpu, "code", 4), code_value);
	if (cart->sprite) {
		value_t sprite_value = value_buf(buf_new_copydata(cpu, cart->sprite, SPRITESHEET_BYTES));
		tab_set(cpu, tab, str_intern(cpu, "sprite", 6), sprite_value);
	}
	if (cart->asset_cnt) {
		struct arrobj* assets = arr_new(cpu);
		for (int i = 0; i < cart->asset_cnt; i++) {
			struct asset* cart_asset = &cart->assets[i];
			struct tabobj* asset = tab_new(cpu);
			struct strobj* name = str_intern(cpu, cart_asset->name, strlen(cart_asset->name));
			tab_set(cpu, asset, str_intern(cpu, "name", 4), value_str(name));
			switch (cart_asset->type) {
			case at_map:
				tab_set(cpu, asset, str_intern(cpu, "type", 4), value_str(str_intern(cpu, "map", 3)));
				tab_set(cpu, asset, str_intern(cpu, "width", 5), value_num(num_kuint(cart_asset->map.width)));
				tab_set(cpu, asset, str_intern(cpu, "height", 6), value_num(num_kuint(cart_asset->map.height)));
				struct bufobj* buf = buf_new_copydata(cpu, cart_asset->map.data, cart_asset->map.width * cart_asset->map.height);
				tab_set(cpu, asset, str_intern(cpu, "data", 4), value_buf(buf));
				break;
			}
			arr_push(cpu, assets, value_tab(asset));
		}
		tab_set(cpu, tab, str_intern(cpu, "asset", 5), value_arr(assets));
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

static value_t get_run_result(struct cpu* cpu, struct run_result result) {
	struct tabobj* ret = tab_new(cpu);
	if (result.err != NULL) {
		value_t err_value = value_str(str_intern(cpu, result.err, (int)strlen(result.err)));
		tab_set(cpu, ret, str_intern(cpu, "err", 3), err_value);
		value_t line_value = value_num(num_kuint(result.linenum));
		tab_set(cpu, ret, str_intern(cpu, "line", 4), line_value);
	}
	return value_tab(ret);
}

value_t devlib_run(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct tabobj* tab = to_tab(cpu, ARG(0));
	struct cart cart = get_cartobj(cpu, tab);
	struct run_result result = console_run(&cart);
	cpu->cycles -= CYCLES_CARTIO;
	return get_run_result(cpu, result);
}

value_t devlib_save(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	struct strobj* filename = to_string(cpu, ARG(0));
	struct tabobj* tab = to_tab(cpu, ARG(1));
	struct cart cart = get_cartobj(cpu, tab);
	char filename_buf[256];
	struct run_result result = console_save(get_cart_filename(cpu, filename, filename_buf, 256), &cart);
	cpu->cycles -= CYCLES_CARTIO;
	return get_run_result(cpu, result);
}

value_t devlib_load(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	struct strobj* filename = to_string(cpu, ARG(0));
	char filename_buf[256];
	struct cart cart;
	struct run_result result = console_load(get_cart_filename(cpu, filename, filename_buf, 256), &cart);
	cpu->cycles -= CYCLES_CARTIO;
	if (result.err != NULL)
		return get_run_result(cpu, result);
	else {
		struct tabobj* tab = parse_cartobj(cpu, &cart);
		cart_destroy(&cart);
		return value_tab(tab);
	}
}

value_t devlib_fastParse(struct cpu* cpu, int sp, int nargs) {
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
				token_end = i + 2;
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
	tab_set(cpu, tab, str_intern(cpu, "token", 5), value_str(token_str));
	tab_set(cpu, tab, str_intern(cpu, "tokenEnd", 8), value_num(num_kuint(token_end)));
	tab_set(cpu, tab, str_intern(cpu, "lineStart", 9), value_num(num_kuint(line_start)));
	cpu->cycles -= CYCLES_CHARS(str->len) + CYCLES_ALLOC + CYCLES_LOOKUP * 3;
	return value_tab(tab);
}

value_t devlib_closeOverlay(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	console_close_overlay();
	return value_undef();
}

value_t devlib_getTaskId(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 0)
		argument_error(cpu);
	return value_num(num_kint(console_getpid()));
}

value_t devlib_getTaskInfo(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	int pid = num_int(to_number(cpu, ARG(0)));
	struct tabobj* tab = tab_new(cpu);
	tab_set(cpu, tab, str_intern(cpu, "id", 2), value_num(num_kint(pid)));
	tab_set(cpu, tab, str_intern(cpu, "parent", 6), value_num(num_kint(cpu->parent)));
	tab_set(cpu, tab, str_intern(cpu, "vmem", 4), value_buf(buf_new_vmem(cpu, pid)));
	tab_set(cpu, tab, str_intern(cpu, "backvmem", 8), value_buf(buf_new_backvmem(cpu, pid)));
	return value_tab(tab);
}

value_t devlib_killTask(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1)
		argument_error(cpu);
	int pid = num_uint(to_number(cpu, ARG(0)));
	console_kill(pid);
	return value_undef();
}

value_t devlib_getMenu(struct cpu* cpu, int sp, int nargs) {
	struct arrobj* arr = menu_getmenu(cpu);
	return value_arr(arr);
}

value_t devlib_menuOp(struct cpu* cpu, int sp, int nargs) {
	menu_menuop(cpu, sp, nargs);
	return value_undef();
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
	{"pal", cf_lib_pal },
	{"palt", cf_lib_palt },
	{"pget", cf_lib_pget },
	{"pset", cf_lib_pset },
	{"line", cf_lib_line },
	{"rect", cf_lib_rect },
	{"fillRect", cf_lib_fillRect },
	{"spr", cf_lib_spr },
	{"sspr", cf_lib_sspr },
	{"print", cf_lib_print },
	{"abs", cf_lib_abs },
	{"max", cf_lib_max },
	{"min", cf_lib_min },
	{"ceil", cf_lib_ceil },
	{"floor", cf_lib_floor },
	{"srand", cf_lib_srand },
	{"rand", cf_lib_rand },
	{"statCpu", cf_lib_statCpu },
	{"statMem", cf_lib_statMem },
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
	{"dev_closeOverlay", cf_devlib_closeOverlay },
	{"dev_getTaskId", cf_devlib_getTaskId },
	{"dev_getTaskInfo", cf_devlib_getTaskInfo },
	{"dev_killTask", cf_devlib_killTask },
	{"dev_getMenu", cf_devlib_getMenu },
	{"dev_menuOp", cf_devlib_menuOp },
	{ NULL, 0 },
};

void lib_init(struct cpu* cpu) {
	struct tabobj* globals = (struct tabobj*)readptr(cpu->globals);
	for (int i = 0; libdefs[i].name; i++) {
		struct strobj* key = str_intern(cpu, libdefs[i].name, (int)strlen(libdefs[i].name));
		tab_set(cpu, globals, key, value_cfunc(libdefs[i].func));
	}
	for (int i = 0; devlibdefs[i].name; i++) {
		struct strobj* key = str_intern(cpu, devlibdefs[i].name, (int)strlen(devlibdefs[i].name));
		tab_set(cpu, globals, key, value_cfunc(devlibdefs[i].func));
	}
	rand_seed(cpu, platform_seed());
}
