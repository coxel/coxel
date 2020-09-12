#include "assetmap.h"
#include "buf.h"
#include "cpu.h"
#include "gc.h"
#include "gfx.h"
#include "platform.h"

struct assetmapobj* assetmap_new(struct cpu* cpu, int w, int h) {
	struct assetmapobj* assetmap = (struct assetmapobj*)gc_alloc(cpu, t_assetmap, w * h);
	assetmap->width = w;
	assetmap->height = h;
	assetmap->buf = writeptr(buf_new(cpu, w * h));
	return assetmap;
}

struct assetmapobj* assetmap_new_copydata(struct cpu* cpu, int w, int h, const uint8_t* data) {
	struct assetmapobj* assetmap = (struct assetmapobj*)gc_alloc(cpu, t_assetmap, w * h);
	assetmap->width = w;
	assetmap->height = h;
	assetmap->buf = writeptr(buf_new_copydata(cpu, data, w * h));
	return assetmap;
}

void assetmap_destroy(struct cpu* cpu, struct assetmapobj* assetmap) {
	mem_dealloc(&cpu->alloc, assetmap);
}

value_t libassetmap_draw(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 6)
		argument_error(cpu);
	struct assetmapobj* assetmap = to_assetmap(cpu, THIS);
	int cx = num_int(to_number(cpu, ARG(0)));
	int cy = num_int(to_number(cpu, ARG(1)));
	int cw = num_int(to_number(cpu, ARG(2)));
	int ch = num_int(to_number(cpu, ARG(3)));
	int x = num_int(to_number(cpu, ARG(4)));
	int y = num_int(to_number(cpu, ARG(5)));
	struct bufobj* buf = (struct bufobj*)readptr(assetmap->buf);
	gfx_map(console_getgfx(), assetmap->width, assetmap->height, buf->data, cx, cy, cw, ch, x, y);
	cpu->cycles -= CYCLES_PIXELS(SPRITE_WIDTH * SPRITE_HEIGHT * cw * ch);
	return value_undef();
}

value_t libassetmap_get(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 2)
		argument_error(cpu);
	struct assetmapobj* assetmap = to_assetmap(cpu, THIS);
	int x = num_uint(to_number(cpu, ARG(0)));
	int y = num_uint(to_number(cpu, ARG(1)));
	if (unlikely(x >= assetmap->width))
		return value_undef();
	if (unlikely(y >= assetmap->height))
		return value_undef();
	struct bufobj* buf = (struct bufobj*)readptr(assetmap->buf);
	cpu->cycles -= CYCLES_ARRAY_LOOKUP;
	int value = buf->data[y * assetmap->height + x];
	return value_num(num_kuint(value));
}

value_t libassetmap_set(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 3)
		argument_error(cpu);
	struct assetmapobj* assetmap = to_assetmap(cpu, THIS);
	int x = num_uint(to_number(cpu, ARG(0)));
	int y = num_uint(to_number(cpu, ARG(1)));
	int value = num_uint(to_number(cpu, ARG(2)));
	if (unlikely(x >= assetmap->width))
		return value_undef();
	if (unlikely(y >= assetmap->height))
		return value_undef();
	if (unlikely(value > 255))
		return value_undef();
	struct bufobj* buf = (struct bufobj*)readptr(assetmap->buf);
	buf->data[y * assetmap->height + x] = value;
	cpu->cycles -= CYCLES_ARRAY_LOOKUP;
	return value_undef();
}

value_t assetmap_fget(struct cpu* cpu, struct assetmapobj* assetmap, struct strobj* key) {
	if (key == LIT(draw))
		return value_cfunc(cf_libassetmap_draw);
	else if (key == LIT(get))
		return value_cfunc(cf_libassetmap_get);
	else if (key == LIT(set))
		return value_cfunc(cf_libassetmap_set);
	else if (key == LIT(width))
		return value_num(num_kuint(assetmap->width));
	else if (key == LIT(height))
		return value_num(num_kuint(assetmap->height));
	else if (key == LIT(data))
		return value_buf(readptr(assetmap->buf));
	else
		return value_undef();
}
