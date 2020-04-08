#ifndef _GFX_H
#define _GFX_H

#include "cpu.h"

extern uint32_t palette[16];

void gfx_init(struct gfx* gfx);
void gfx_cls(struct gfx* gfx, int c);
void gfx_camera(struct gfx* gfx, int x, int y);
void gfx_line(struct gfx* gfx, int x1, int y1, int x2, int y2, int c);
void gfx_rect(struct gfx* gfx, int x, int y, int w, int h, int c);
void gfx_fill_rect(struct gfx* gfx, int x, int y, int w, int h, int c);
void gfx_print(struct gfx* gfx, const char* str, int len, int x, int y, int c);
void gfx_setpixel(struct gfx* gfx, int x, int y, int c);
int gfx_getpixel(struct gfx* gfx, int x, int y);

#endif
