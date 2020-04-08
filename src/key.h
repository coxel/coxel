#ifndef _KEY_H
#define _KEY_H

#define BTN_CNT		8
#define PLAYER_CNT	8

#define KEYDEF(X) \
	X(none, _, _) \
	X(up, _, _) \
	X(down, _, _) \
	X(left, _, _) \
	X(right, _, _) \
	X(a, 'a', 'A') \
	X(b, 'b', 'B') \
	X(c, 'c', 'C') \
	X(d, 'd', 'D') \
	X(e, 'e', 'E') \
	X(f, 'f', 'F') \
	X(g, 'g', 'G') \
	X(h, 'h', 'H') \
	X(i, 'i', 'I') \
	X(j, 'j', 'J') \
	X(k, 'k', 'K') \
	X(l, 'l', 'L') \
	X(m, 'm', 'M') \
	X(n, 'n', 'N') \
	X(o, 'o', 'O') \
	X(p, 'p', 'P') \
	X(q, 'q', 'Q') \
	X(r, 'r', 'R') \
	X(s, 's', 'S') \
	X(t, 't', 'T') \
	X(u, 'u', 'U') \
	X(v, 'v', 'V') \
	X(w, 'w', 'W') \
	X(x, 'x', 'X') \
	X(y, 'y', 'Y') \
	X(z, 'z', 'Z') \
	X(1, '1', '!') \
	X(2, '2', '@') \
	X(3, '3', '#') \
	X(4, '4', '$') \
	X(5, '5', '%') \
	X(6, '6', '^') \
	X(7, '7', '&') \
	X(8, '8', '*') \
	X(9, '9', '(') \
	X(0, '0', ')') \
	X(esc, _, _) \
	X(return, _, _) \
	X(backtick, '`', '~') \
	X(dash, '-', '_') \
	X(equal, '=', '+') \
	X(tab, _, _) \
	X(backspace, _, _) \
	X(space, ' ', ' ') \
	X(lbracket, '[', '{') \
	X(rbracket, ']', '}') \
	X(lslash, '/', '?') \
	X(rslash, '\\', '|') \
	X(semicolon, ';', ':') \
	X(quote, '\'', '"') \
	X(comma, ',', '<') \
	X(period, '.', '>') \
	X(f1, _, _) \
	X(f2, _, _) \
	X(f3, _, _) \
	X(f4, _, _) \
	X(f5, _, _) \
	X(f6, _, _) \
	X(f7, _, _) \
	X(f8, _, _) \
	X(f9, _, _) \
	X(f10, _, _) \
	X(f11, _, _) \
	X(f12, _, _) \
	X(insert, _, _) \
	X(delete, _, _) \
	X(home, _, _) \
	X(end, _, _) \
	X(pgup, _, _) \
	X(pgdn, _, _) \
	X(ctrl, _, _) \
	X(alt, _, _) \
	X(shift, _, _) \
	X(mleft, _, _) \
	X(mright, _, _) \
	X(mmiddle, _, _) \
	X(_end, _, _)

#define modbutton_begin	key_ctrl
#define modbutton_end	key_shift
#define mbutton_begin	key_mleft
#define mbutton_end		key_mmiddle

#define X(a, b, c) key_##a,
enum key {
	KEYDEF(X)
	key_cnt = key__end + BTN_CNT * PLAYER_CNT
};
#undef X

struct cpu;
struct io;
struct strobj;
void key_init(struct io* io);
void key_preupdate(struct io* io);
void key_postupdate(struct io* io);
void key_press(enum key key);
void key_release(enum key key);
void key_setstate(enum key key, int pressed);
void key_input(char ch);
enum key key_translate(struct cpu* cpu, struct strobj* key);
int btn_translate(struct cpu* cpu, struct strobj* key);
void btn_standard_update();
int btn_is_down(int btn, int player);
int btn_is_pressed(int btn, int player);
int key_is_down(enum key key);
int key_is_pressed(enum key key);
char key_get_standard_input(enum key key);

#endif
