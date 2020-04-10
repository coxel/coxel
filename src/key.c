#include "cpu.h"
#include "key.h"
#include "platform.h"
#include "str.h"

#include <string.h>

#define _ 0
#define X(a, b, c) #a,
static const char* key_name[] = {
	KEYDEF(X)
};
#undef X
#define X(a, b, c) b,
static const char key_char[] = {
	KEYDEF(X)
};
#undef X
#define X(a, b, c) c,
static const char key_shift_char[] = {
	KEYDEF(X)
};
#undef X
#undef _

static const char* btn_name[] = {
	"left",
	"right",
	"up",
	"down",
	"a",
	"b",
	"x",
	"y",
};

static enum key default_keymap[] = {
	kc_left,
	kc_right,
	kc_up,
	kc_down,
	kc_z,
	kc_x,
	kc_a,
	kc_s,
	kc_none,
};

void key_init(struct io* io) {
	for (int i = 0; i < kc_cnt; i++) {
		io->prev_keys[i] = 0;
		io->keys[i] = 0;
	}
	io->last_key = kc_none;
	io->last_key_repeat = 0;
	io->mousex = 0;
	io->mousey = 0;
}

void key_preupdate(struct io* io) {
	if (io->keys[io->last_key]) {
		if (++io->last_key_repeat >= 18) {
			io->last_key_repeat -= 3;
			io->prev_keys[io->last_key] = 0;
		}
	}
}

void key_postupdate(struct io* io) {
	for (int i = 0; i < kc_cnt; i++)
		io->prev_keys[i] = io->keys[i];
	io->input_size = 0;
	io->mousewheel = 0;
}

void key_press(enum key key) {
	struct io* io = console_getio();
	if (!((key >= modbutton_begin && key <= modbutton_end) || (key >= mbutton_begin && key <= mbutton_end)) && !io->prev_keys[key]) {
		io->last_key = key;
		io->last_key_repeat = 0;
	}
	io->keys[key] = 1;
}

void key_release(enum key key) {
	struct io* io = console_getio();
	io->keys[key] = 0;
}

void key_setstate(enum key key, int pressed) {
	if (pressed)
		key_press(key);
	else
		key_release(key);
}

void key_input(char ch) {
	if (ch == 0) // TODO: Check character validity
		return;
	struct io* io = console_getio();
	if (io->input_size < MAX_INPUT)
		io->input[io->input_size++] = ch;
}

enum key key_translate(struct cpu* cpu, struct strobj* key) {
	if (key->len == 1) {
		for (int i = 0; i < kc_cnt; i++)
			if (key_char[i] == key->data[0])
				return i;
	}
	else {
		for (int i = 0; i < kc_cnt; i++) {
			int len = strlen(key_name[i]);
			if (str_equal(key->data, key->len, key_name[i], len))
				return i;
		}
	}
	runtime_error(cpu, "Unknown key name.");
}

int btn_translate(struct cpu* cpu, struct strobj* key) {
	for (int i = 0; i < BTN_CNT; i++) {
		int len = strlen(btn_name[i]);
		if (str_equal(key->data, key->len, btn_name[i], len))
			return i;
	}
	runtime_error(cpu, "Unknown button name.");
}

void btn_standard_update() {
	struct io* io = console_getio();
	int player = 0;
	for (int i = 0; default_keymap[i] != kc_none; i += BTN_CNT) {
		for (int j = 0; j < BTN_CNT; j++)
			io->keys[kc__end + player * BTN_CNT + j] = key_is_down(default_keymap[i + j]);
		++player;
	}
}

int btn_is_down(int btn, int player) {
	return key_is_down(kc__end + player * BTN_CNT + btn);
}

int btn_is_pressed(int btn, int player) {
	return key_is_pressed(kc__end + player * BTN_CNT + btn);
}

int key_is_down(enum key key) {
	struct io* io = console_getio();
	return io->keys[key];
}

int key_is_pressed(enum key key) {
	struct io* io = console_getio();
	return io->keys[key] && !io->prev_keys[key];
}

char key_get_standard_input(enum key key) {
	struct io* io = console_getio();
	if (io->keys[kc_ctrl] || io->keys[kc_alt])
		return 0;
	if (io->keys[kc_shift])
		return key_shift_char[key];
	else
		return key_char[key];
}
