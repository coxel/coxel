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
	int i = (y * WIDTH + x) / 2;
	if (x % 2)
		gfx->screen[i] = (gfx->screen[i] & 0x0F) + (c << 4);
	else
		gfx->screen[i] = (gfx->screen[i] & 0xF0) + c;
}

int gfx_getpixel(struct gfx* gfx, int x, int y) {
	int i = (y * WIDTH + x) / 2;
	if (x % 2)
		return gfx->screen[i] >> 4;
	else
		return gfx->screen[i] & 0x0F;
}

void gfx_init(struct gfx* gfx) {
	gfx->cx = 0;
	gfx->cy = 0;
	gfx->color = 15;
	gfx->cam_x = 0;
	gfx->cam_y = 0;
	memset(gfx->screen, 0, sizeof(gfx->screen));
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

void gfx_spr(struct gfx* gfx, int sx, int sy, int x, int y, int w, int h) {
	for (int i = 0; i < h && sy + i < SPRITESHEET_HEIGHT; i++)
		for (int j = 0; j < w && sx + j < SPRITESHEET_WIDTH; j++) {
			int c = gfx->sprite[(i * SPRITESHEET_WIDTH + j) / 2];
			if (j % 2 == 0)
				c = c & 0xF;
			else
				c = c >> 4;
			gfx_setpixel(gfx, x + j, y + i, c);
		}
}

extern const char font[][3];

static void gfx_vscroll(struct gfx* gfx, int up_amount) {
	if (up_amount < HEIGHT)
		memmove(&gfx->screen[0], &gfx->screen[up_amount * WIDTH / 2], (HEIGHT - up_amount) * WIDTH / 2);
	memset(&gfx->screen[(HEIGHT - up_amount) * WIDTH / 2], 0, up_amount * WIDTH / 2);
}

void gfx_print(struct gfx* gfx, const char* str, int len, int x, int y, int c) {
	if (c == -1)
		c = gfx->color;
	int use_cursor = 0;
	if (x == -1 || y == -1) {
		x = gfx->cx;
		y = gfx->cy;
		use_cursor = 1;
	}
	for (int i = 0; i < len; i++) {
		/* cursor movement */
		if (use_cursor) {
			if (x + 3 > WIDTH) {
				x = 0;
				y += 7;
			}
			if (y + 7 > HEIGHT) {
				gfx_vscroll(gfx, 7);
				y -= 7;
			}
		}
		char ch = str[i];
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
		x = 0;
		y += 7;
		if (y + 7 > HEIGHT) {
			gfx_vscroll(gfx, 7);
			y -= 7;
		}
		gfx->cx = x;
		gfx->cy = y;
	}
}
