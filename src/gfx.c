#include "gfx.h"
#include "platform.h"

#include <string.h>

uint32_t palette[16] = {
	0x0E0E0E,
	0x632850,
	0xE65845,
	0xF06F8B,
	0x493C2B,
	0xAB5236,
	0xFFA000,
	0xE6D63A,
	0x29366F,
	0x125DA8,
	0x31A6F6,
	0xA3EFF7,
	0x44891A,
	0x50E645,
	0x9D9D9D,
	0xFDFDF8,
};

void gfx_setpixel(struct gfx* gfx, int x, int y, int c) {
	if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
		return;
	c = gfx->pal[c];
	int i = (y * WIDTH + x) / 2;
	if (x % 2)
		gfx->screen[gfx->bufno][i] = (gfx->screen[gfx->bufno][i] & 0x0F) + (c << 4);
	else
		gfx->screen[gfx->bufno][i] = (gfx->screen[gfx->bufno][i] & 0xF0) + c;
}

int gfx_getpixel(struct gfx* gfx, int x, int y) {
	int i = (y * WIDTH + x) / 2;
	if (x % 2)
		return gfx->screen[gfx->bufno][i] >> 4;
	else
		return gfx->screen[gfx->bufno][i] & 0x0F;
}

void gfx_init(struct gfx* gfx) {
	gfx->cx = 0;
	gfx->cy = 0;
	gfx->color = 15;
	gfx->cam_x = 0;
	gfx->cam_y = 0;
	gfx->bufno = 0;
	memset(gfx->screen, 0, sizeof(gfx->screen));
	gfx_reset_pal(gfx);
	gfx_reset_palt(gfx);
}

void gfx_cls(struct gfx* gfx, int c) {
	if (c == -1)
		c = 0;
	gfx->cx = 0;
	gfx->cy = 0;
	memset(gfx->screen, c * 16 + c, WIDTH * HEIGHT / 2);
}

void gfx_camera(struct gfx* gfx, int x, int y) {
	gfx->cam_x = x;
	gfx->cam_y = y;
}

void gfx_reset_pal(struct gfx* gfx) {
	for (int i = 0; i < 16; i++)
		gfx->pal[i] = i;
}

void gfx_pal(struct gfx* gfx, int c, int c1) {
	gfx->pal[c] = c1;
}

void gfx_reset_palt(struct gfx* gfx) {
	for (int i = 0; i < 16; i++)
		gfx->palt[i] = 0;
	gfx->palt[0] = 1;
}

void gfx_palt(struct gfx* gfx, int c, int t) {
	gfx->palt[c] = t;
}

void gfx_line(struct gfx* gfx, int x1, int y1, int x2, int y2, int c) {
	if (c == -1)
		c = gfx->color;
	x1 -= gfx->cam_x;
	y1 -= gfx->cam_y;
	x2 -= gfx->cam_x;
	y2 -= gfx->cam_y;
	/* Bresenham line drawing */
	int dx = x1 < x2 ? x2 - x1 : x1 - x2;
	int sx = x1 < x2 ? 1 : -1;
	int dy = y1 < y2 ? y1 - y2 : y2 - y1;
	int sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		gfx_setpixel(gfx, x1, y1, c);
		if (x1 == x2 && y1 == y2)
			break;
		int e2 = err * 2;
		if (e2 >= dy) {
			err += dy;
			x1 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y1 += sy;
		}
	}
}

void gfx_rect(struct gfx* gfx, int x, int y, int w, int h, int c) {
	if (c == -1)
		c = gfx->color;
	x -= gfx->cam_x;
	y -= gfx->cam_y;
	for (int i = 0; i < w; i++) {
		gfx_setpixel(gfx, x + i, y, c);
		gfx_setpixel(gfx, x + i, y + h - 1, c);
	}
	for (int i = 0; i < h; i++) {
		gfx_setpixel(gfx, x, y + i, c);
		gfx_setpixel(gfx, x + w - 1, y + i, c);
	}
}

void gfx_fill_rect(struct gfx* gfx, int x, int y, int w, int h, int c) {
	if (c == -1)
		c = gfx->color;
	x -= gfx->cam_x;
	y -= gfx->cam_y;
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++)
			gfx_setpixel(gfx, x + j, y + i, c);
}

void gfx_spr(struct gfx* gfx, int sx, int sy, int sw, int sh, int x, int y, int w, int h, int r) {
	x -= gfx->cam_x;
	y -= gfx->cam_y;
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++) {
			int ty = sy + i * sh / h;
			int tx = sx + j * sw / w;
			if (ty >= SPRITESHEET_HEIGHT || tx >= SPRITESHEET_WIDTH)
				continue;
			int c = gfx->sprite[(ty * SPRITESHEET_WIDTH + tx) / 2];
			if (tx % 2 == 0)
				c = c & 0xF;
			else
				c = c >> 4;
			if (gfx->palt[c])
				continue;
			if (r == 0)
				gfx_setpixel(gfx, x + j, y + i, c);
			else if (r == 90)
				gfx_setpixel(gfx, x + i, y + w - j - 1, c);
			else if (r == 180)
				gfx_setpixel(gfx, x + w - j - 1, y + h - i - 1, c);
			else if (r == 270)
				gfx_setpixel(gfx, x + h - i - 1, y + j, c);
		}
}

void gfx_map(struct gfx* gfx, int mapw, int maph, uint8_t* mapdata, int cx, int cy, int cw, int ch, int x, int y) {
	x -= gfx->cam_x;
	y -= gfx->cam_y;
	for (int i = 0; i < ch; i++) {
		if (cy + i < 0 || cy + i >= maph)
			continue;
		for (int j = 0; j < cw; j++) {
			int tx = x + SPRITE_WIDTH * j;
			int ty = y + SPRITE_HEIGHT * i;
			if (cx + j < 0 || cx + j >= mapw)
				continue;
			int spr_id = mapdata[(cy + i) * mapw + (cx + j)];
			if (spr_id == 0)
				continue;
			int sx = spr_id % (SPRITESHEET_WIDTH / SPRITE_WIDTH) * SPRITE_WIDTH;
			int sy = spr_id / (SPRITESHEET_WIDTH / SPRITE_WIDTH);
			for (int si = 0; si < SPRITE_HEIGHT; si++) {
				for (int sj = 0; sj < SPRITE_WIDTH; sj++) {
					int c = gfx->sprite[((sy + si) * SPRITESHEET_WIDTH + (sx + sj)) / 2];
					if (tx % 2 == 0)
						c = c & 0xF;
					else
						c = c >> 4;
					gfx_setpixel(gfx, tx + sj, ty + si, c);
				}
			}
		}
	}
}

extern const char font[][3];

static void gfx_vscroll(struct gfx* gfx, int up_amount) {
	int bufno = gfx->bufno;
	if (up_amount < HEIGHT)
		memmove(&gfx->screen[bufno][0], &gfx->screen[bufno][up_amount * WIDTH / 2], (HEIGHT - up_amount) * WIDTH / 2);
	memset(&gfx->screen[bufno][(HEIGHT - up_amount) * WIDTH / 2], 0, up_amount * WIDTH / 2);
}

static void gfx_print_internal(struct gfx* gfx, const char* str, int len, int x, int y, int c, int newline) {
	if (c == -1)
		c = gfx->color;
	int use_cursor = 0;
	if (x == -1 || y == -1) {
		x = gfx->cx;
		y = gfx->cy;
		use_cursor = 1;
	}
	for (int i = 0; i < len; i++) {
		char ch = str[i];
		/* cursor movement */
		if (use_cursor) {
			if (x + 3 > WIDTH || ch == '\n') {
				x = 0;
				y += 7;
			}
			if (y + 7 > HEIGHT) {
				gfx_vscroll(gfx, 7);
				y -= 7;
			}
		}
		if (ch < 32 || ch >= 127)
			continue;
		for (int fx = 0; fx < 3; fx++) {
			uint8_t f = font[ch - 32][fx];
			for (int fy = 0; fy < 6; fy++) {
				int b = (f & (1 << fy)) > 0;
				if (b)
					gfx_setpixel(gfx, x + fx - gfx->cam_x, y + fy - gfx->cam_y, c);
			}
		}
		/* output character */
		x += 4;
	}
	if (use_cursor) {
		if (newline) {
			x = 0;
			y += 7;
			if (y + 7 > HEIGHT) {
				gfx_vscroll(gfx, 7);
				y -= 7;
			}
		}
		gfx->cx = x;
		gfx->cy = y;
	}
}

void gfx_print(struct gfx* gfx, const char* str, int len, int x, int y, int c) {
	gfx_print_internal(gfx, str, len, x, y, c, 1);
}

void gfx_print_simple(struct gfx* gfx, const char* str, int len, int c) {
	gfx_print_internal(gfx, str, len, -1, -1, c, 0);
}
