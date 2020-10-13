#include "assetmap.h"
#include "compiler.h"
#include "cpu.h"
#include "gc.h"
#include "gfx.h"
#include "platform.h"
#include "str.h"
#include "tab.h"

#include <stdarg.h>
#include <string.h>

NORETURN void critical_error(const char* fmt, ...) {
	va_list args;
	char buf[1024];
	va_start(args, fmt);
	str_vsprintf(buf, fmt, args);
	va_end(args);
	platform_error(buf);
}

/* Hard limit of concurrent CPUs, actual limit depends on system memory */
/* CPU 0 is always firmware */
#define MAX_CPUS			64

static struct io g_io;
static struct cpu* g_cpus[MAX_CPUS];
static int g_cur_cpu, g_next_cpu;
static enum {
	overlay_inactive,
	overlay_pending,
	overlay_active,
	overlay_close_pending,
} g_overlay_mode;
static struct gfx g_overlay_gfx;
#ifdef HIERARCHICAL_MEMORY
static struct gfx g_gfx;
#endif

void cart_destroy(struct cart* cart) {
	if (cart->code)
		platform_free_fast((void*)cart->code);
	if (cart->sprite)
		platform_free_fast((void*)cart->sprite);
	if (cart->assets) {
		for (int i = 0; i < cart->asset_cnt; i++) {
			struct asset* asset = &cart->assets[i];
			switch (asset->type) {
			case at_map:
				platform_free(asset->map.data);
				break;
			}
		}
		platform_free((void*)cart->assets);
	}
}

struct parse_context {
	void* file;
	uint32_t filesize;
	char chunk[1024];
	uint32_t p, len;
	char ch;
};

static void next_char(struct parse_context* ctx) {
	if (ctx->p == ctx->len && ctx->filesize == 0)
		ctx->ch = 0;
	else {
		if (ctx->p == ctx->len) {
			int len = ctx->filesize < 1024 ? ctx->filesize : 1024;
			ctx->len = platform_read(ctx->file, ctx->chunk, len);
			ctx->p = 0;
			ctx->filesize -= len;
		}
		ctx->ch = ctx->chunk[ctx->p++];
	}
}

static struct run_result parse_cart(void* file, uint32_t filesize, struct cart* cart) {
#define READ_HEXCHAR(out)	do { \
		int ok; \
		int low = from_hex(ctx.ch, &ok); \
		if (!ok) { \
			ret.err = "Malformed cart data."; \
			goto fail; \
		} \
		next_char(&ctx); \
		int high = from_hex(ctx.ch, &ok); \
		if (!ok) { \
			ret.err = "Malformed cart data."; \
			goto fail; \
		} \
		next_char(&ctx); \
		out = low + (high << 4); \
	} while (0)

#define TEMP_BUF_SIZE	16
	struct parse_context ctx;
	ctx.file = file;
	ctx.filesize = filesize;
	ctx.p = 0;
	ctx.len = 0;
	char temp_buf[TEMP_BUF_SIZE];
	char* code_buf = (char*)platform_malloc_fast(MAX_CODE_SIZE + 1);
	cart->code = code_buf;
	cart->codelen = 0;
	struct run_result ret;
	ret.err = NULL;
	ret.linenum = -1;
	/* Read code section */
	for (;;) {
		next_char(&ctx);
		if (ctx.ch == '\t' && cart->codelen > 0 && code_buf[cart->codelen - 1] == '\n') {
			cart->codelen--;
			break;
		}
		if (ctx.ch == 0)
			break;
		if (ctx.ch == '\r')
			continue;
		if (!is_allowed_char(ctx.ch)) {
			ret.err = "Invalid character in cart file.";
			goto fail;
		}
		if (cart->codelen == MAX_CODE_SIZE + 1)
			break;
		code_buf[cart->codelen++] = ctx.ch;
	}
	if (cart->codelen > MAX_CODE_SIZE) {
		ret.err = "Code section too large.";
		goto fail;
	}
	/* Read auxiliary sections */
	cart->sprite = NULL;
	cart->asset_cnt = 0;
	cart->assets = (struct asset*)platform_malloc(MAX_ASSET_SIZE);
	while (ctx.ch != 0) {
		if (ctx.ch == '\r' || ctx.ch == '\n') {
			next_char(&ctx);
			continue;
		}
		while (ctx.ch == '\t') {
			next_char(&ctx);
			int name_len = 0;
			while (name_len < TEMP_BUF_SIZE && ctx.ch != 0 && ctx.ch != '\n') {
				if (ctx.ch != '\r')
					temp_buf[name_len++] = ctx.ch;
				next_char(&ctx);
			}
			if (ctx.ch == 0) {
				ret.err = "Unexpected end of file.";
				goto fail;
			}
			if (ctx.ch != '\n') {
				ret.err = "Invalid section name.";
				goto fail;
			}
			next_char(&ctx);
			if (str_equal(temp_buf, name_len, SPRITESHEET_MAGIC, sizeof(SPRITESHEET_MAGIC) - 1)) {
				char* sprite = (char*)platform_malloc_fast(SPRITESHEET_BYTES);
				cart->sprite = sprite;
				int t = 0;
				for (int i = 0; i < SPRITESHEET_HEIGHT; i++) {
					if (ctx.ch == '\t' || ctx.ch == 0)
						break;
					for (int j = 0; j < SPRITESHEET_WIDTH; j += 2)
						READ_HEXCHAR(sprite[t++]);
					/* Skip EOLs */
					while (ctx.ch == '\r' || ctx.ch == '\n')
						next_char(&ctx);
				}
				/* Fill remaining data with zero */
				memset(sprite + t, 0, SPRITESHEET_BYTES - t);
			}
			else if (str_equal(temp_buf, name_len, MAP_MAGIC, sizeof(MAP_MAGIC) - 1)) {
				struct asset* asset = &cart->assets[cart->asset_cnt];
				int name_len = 0;
				while (name_len < TEMP_BUF_SIZE && ctx.ch != 0 && ctx.ch != '\n') {
					if (ctx.ch != '\r')
						temp_buf[name_len++] = ctx.ch;
					next_char(&ctx);
				}
				if (name_len > ASSET_NAME_LEN) {
					ret.err = "Asset name too long.";
					goto fail;
				}
				next_char(&ctx);
				memcpy(asset->name, temp_buf, name_len);
				asset->type = at_map;
				char* buf = (char *)&cart->assets[cart->asset_cnt + 1];
				int p = 0;
				// TODO: Length check
				int w = -1;
				int h;
				for (h = 0;; h++) {
					if (ctx.ch == '\n' || ctx.ch == 0)
						break;
					int j;
					for (j = 0;; j++) {
						if (ctx.ch == '\r' || ctx.ch == '\n')
							break;
						READ_HEXCHAR(buf[p++]);
					}
					if (w == -1)
						w = j;
					else if (w != j) {
						ret.err = "Width mismatch in map lines.";
						goto fail;
					}
					/* Skip EOLs */
					while (ctx.ch == '\r' || ctx.ch == '\n')
						next_char(&ctx);
				}
				asset->map.width = w;
				asset->map.height = h;
				asset->map.data = platform_malloc(w * h);
				memcpy(asset->map.data, buf, p);
				cart->asset_cnt++;
			}
			else {
				ret.err = "Invalid section name.";
				goto fail;
			}
		}
	}
	if (!cart->asset_cnt)
		cart->assets = NULL;
	return ret;

fail:
	cart_destroy(cart);
	return ret;
}

struct run_result console_load(const char* filename, struct cart* cart) {
	struct run_result res;
	res.err = NULL;
	res.linenum = -1;
	uint32_t filesize;
	void* file = platform_open(filename, &filesize);
	if (file == NULL) {
		res.err = "Open cartridge file failed.";
		return res;
	}
	return parse_cart(file, filesize, cart);
}

static int get_buf_len(const char* buf, int maxlen) {
	if (buf != NULL) {
		for (int i = maxlen - 1; i >= 0; i--)
			if (buf[i] != 0)
				return i + 1;
	}
	return 0;
}

struct run_result console_save(const char* filename, const struct cart* cart) {
#define WRITE(x, s) do { \
		if (platform_write(file, (const char*)x, s) != s) { \
			platform_close(file); \
			ret.err = "Write cart file failed."; \
			return ret; \
		} \
	} while (0)
#define WRITE_CHAR(x) do { \
		char __v = (x); \
		WRITE(&__v, 1); \
	} while (0)
#define WRITE_HEXCHAR(ch) do { \
		unsigned char c = (unsigned char)ch; \
		WRITE_CHAR(to_hex(c & 15)); \
		WRITE_CHAR(to_hex(c >> 4)); \
	} while (0)
#define WRITE_INT(i) do { \
		char buf[16]; \
		WRITE(buf, int_format(i, buf)); \
	} while (0)

	struct run_result ret;
	ret.err = NULL;
	ret.linenum = -1;
	void* file = platform_create(filename);
	if (file == NULL) {
		ret.err = "Create cart file failed.";
		return ret;
	}
	WRITE(cart->code, cart->codelen);
	int spritesheet_len = get_buf_len(cart->sprite, SPRITESHEET_BYTES);
	if (spritesheet_len > 0) {
		WRITE(SEPARATOR_MAGIC, sizeof(SEPARATOR_MAGIC) - 1);
		WRITE(SPRITESHEET_MAGIC, sizeof(SPRITESHEET_MAGIC) - 1);
		WRITE_CHAR('\n');
		int lines = (spritesheet_len + (SPRITESHEET_WIDTH / 2 - 1)) / (SPRITESHEET_WIDTH / 2);
		int t = 0;
		for (int i = 0; i < lines; i++) {
			for (int j = 0; j < SPRITESHEET_WIDTH; j += 2)
				WRITE_HEXCHAR(cart->sprite[t++]);
			WRITE_CHAR('\n');
		}
	}
	for (int asset_num = 0; asset_num < cart->asset_cnt; asset_num++) {
		WRITE(SEPARATOR_MAGIC, sizeof(SEPARATOR_MAGIC) - 1);
		struct asset* asset = &cart->assets[asset_num];
		switch (asset->type) {
		case at_map: {
			struct mapasset* map = &asset->map;
			WRITE(MAP_MAGIC, sizeof(MAP_MAGIC) - 1);
			WRITE_CHAR('\n');
			WRITE(asset->name, strlen(asset->name));
			WRITE_CHAR('\n');
			int t = 0;
			for (int i = 0; i < map->height; i++) {
				for (int j = 0; j < map->width; j++)
					WRITE_HEXCHAR(map->data[t++]);
				WRITE_CHAR('\n');
			}
			break;
		}
		}
	}
	platform_close(file);
	return ret;
}

static void save_cpu_state() {
	if (g_cur_cpu == -1)
		return;
#ifdef HIERARCHICAL_MEMORY
	memcpy(&g_cpus[g_cur_cpu]->gfx, &g_gfx, sizeof(struct gfx));
#endif
}

static void load_cpu_state() {
#ifdef HIERARCHICAL_MEMORY
	memcpy(&g_gfx, &g_cpus[g_cur_cpu]->gfx, sizeof(struct gfx));
#endif
}

static void console_init_internal(int factory_firmware) {
	g_cur_cpu = -1;
	g_overlay_mode = overlay_inactive;
	key_init(&g_io);
	struct cart cart;
	struct run_result res;
	if (factory_firmware)
		res = console_load(NULL, &cart);
	else {
		res = console_load(FIRMWARE_PATH, &cart);
		if (res.err != NULL)
			res = console_load(NULL, &cart);
	}
	if (res.err != NULL)
		critical_error("Firmware load error:\n%s", res.err);
	res = console_run(&cart);
	cart_destroy(&cart);
	if (res.err != NULL)
		critical_error("Firmware compilation error:\nLine %d: %s", res.linenum + 1, res.err);
	g_cur_cpu = g_next_cpu;
	load_cpu_state();
}

void console_init() {
	console_init_internal(0);
}

void console_factory_init() {
	console_init_internal(1);
}

#ifdef RELATIVE_ADDRESSING
struct run_result console_serialize(void* f) {
#define SERIALIZE(x, s) do { \
		if (platform_write(f, (const char*)x, s) != s) { \
			ret.err = "Write serialization data failed."; \
			return ret; \
		} \
	} while (0)
#define SERIALIZE_INT(x) do { \
		int t = (x); \
		SERIALIZE(&t, 4); \
	} while (0)

	struct run_result ret;
	ret.err = NULL;
	ret.linenum = -1;
	save_cpu_state();
	SERIALIZE_INT(COXEL_STATE_MAGIC);
	SERIALIZE_INT(COXEL_STATE_VERSION);
	int cpu_count = 0;
	for (int i = 0; i < MAX_CPUS; i++) {
		if (g_cpus[i])
			cpu_count++;
	}
	SERIALIZE_INT(cpu_count);
	for (int i = 0; i < MAX_CPUS; i++) {
		if (g_cpus[i]) {
			SERIALIZE_INT(i);
			SERIALIZE(g_cpus[i], CPU_MEM_SIZE);
		}
	}
	SERIALIZE_INT(g_cur_cpu);
	SERIALIZE_INT(g_overlay_mode);
	SERIALIZE(&g_overlay_gfx, sizeof(struct gfx));
	return ret;
}

void console_deserialize_init(void* f) {
#define DESERIALIZE(x, s) do { \
		if (platform_read(f, (char*)x, s) != s) { \
			critical_error("Read serialization data failed."); \
		} \
	} while (0)
#define STATE_CORRUPTED() critical_error("State corrupted.")
	
	g_overlay_mode = overlay_inactive;
	key_init(&g_io);
	int magic;
	DESERIALIZE(&magic, 4);
	if (magic != COXEL_STATE_MAGIC)
		critical_error("State magic mismatch.");
	int version;
	DESERIALIZE(&version, 4);
	if (version != COXEL_STATE_VERSION)
		critical_error("State version mismatch.");
	int cpu_count;
	DESERIALIZE(&cpu_count, 4);
	if (cpu_count > MAX_CPUS)
		STATE_CORRUPTED();
	int last_cpu = -1;
	for (int i = 0; i < cpu_count; i++) {
		int id;
		DESERIALIZE(&id, 4);
		if (id <= last_cpu || id >= MAX_CPUS)
			STATE_CORRUPTED();
		struct cpu* cpu = (struct cpu*)platform_malloc(CPU_MEM_SIZE);
		if (cpu == NULL)
			critical_error("Out of memory.");
		DESERIALIZE(cpu, CPU_MEM_SIZE);
		g_cpus[id] = cpu;
	}
	DESERIALIZE(&g_cur_cpu, 4);
	if (g_cur_cpu < 0 || g_cur_cpu >= MAX_CPUS || g_cpus[g_cur_cpu] == NULL)
		STATE_CORRUPTED();
	DESERIALIZE(&g_overlay_gfx, sizeof(struct gfx));
	DESERIALIZE(&g_overlay_mode, 4);
	g_next_cpu = g_cur_cpu;
	load_cpu_state();
}
#endif

void console_destroy() {
	/* Destroy all CPUs */
	for (int i = 0; i < MAX_CPUS; i++) {
		if (g_cpus[i] != NULL) {
			cpu_destroy(g_cpus[i]);
			g_cpus[i] = NULL;
		}
	}
}

struct run_result console_run(const struct cart* cart) {
	struct run_result ret;
	ret.err = NULL;
	ret.linenum = -1;
	/* Find an empty CPU slot */
	int slot = -1;
	for (int i = 0; i < MAX_CPUS; i++) {
		if (g_cpus[i] == NULL) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		ret.err = "Too many running processes.";
		return ret;
	}
	
	/* Initialize CPU */
	struct cpu* cpu = cpu_new(slot);
	if (cpu == NULL) {
		ret.err = "Out of memory.";
		return ret;
	}

	/* Compile cart code */
	struct compile_err err = compile(cpu, cart->code, cart->codelen);
	if (err.msg != NULL) {
		ret.err = err.msg;
		ret.linenum = err.linenum;
		cpu_destroy(cpu);
		return ret;
	}

#ifdef _DEBUG
	char* buf = (char*)platform_malloc(1048576);
	int len = cpu_dump_code(cpu, cart->code, cart->codelen, buf, 1048576);
	void* dumpfile = platform_create("debug_asm.txt");
	if (dumpfile) {
		platform_write(dumpfile, buf, len);
		platform_close(dumpfile);
	}
	platform_free(buf);
#endif

	/* Copy spritesheet */
	if (cart->sprite)
		memcpy(&cpu->gfx.sprite, cart->sprite, SPRITESHEET_BYTES);
	else
		memset(&cpu->gfx.sprite, 0, SPRITESHEET_BYTES);

	/* Load assets */
	struct tabobj* assets_tab = tab_new(cpu);
	for (int i = 0; i < cart->asset_cnt; i++) {
		struct asset* asset = &cart->assets[i];
		struct strobj* name = str_intern(cpu, asset->name, strlen(asset->name));
		switch (asset->type) {
		case at_map: {
			struct assetmapobj* assetmap = assetmap_new_copydata(cpu, asset->map.width, asset->map.height, asset->map.data);
			tab_set(cpu, assets_tab, name, value_assetmap(assetmap));
			break;
		}
		}
	}
	tab_set(cpu, (struct tabobj*)readptr(cpu->globals), str_intern(cpu, "ASSET", 5), value_tab(assets_tab));

	cpu->parent = g_cur_cpu;
	g_cpus[slot] = cpu;

	/* Set cpu to be run */
	g_next_cpu = slot;

	return ret;
}

void console_open_overlay() {
	if (g_overlay_mode == overlay_inactive) {
		g_overlay_mode = overlay_pending;
		save_cpu_state();
		gfx_init(&g_overlay_gfx);
	}
}

void console_close_overlay() {
	if (g_overlay_mode != overlay_inactive)
		g_overlay_mode = overlay_close_pending;
}

void console_update() {
	key_preupdate(&g_io);
	if (g_cur_cpu != g_next_cpu) {
		save_cpu_state();
		g_cur_cpu = g_next_cpu;
		load_cpu_state();
	}
	if (key_is_pressed(kc_f4))
		console_open_overlay();
	struct cpu* cpu;
	if (g_overlay_mode != overlay_inactive)
		cpu = g_cpus[0];
	else
		cpu = g_cpus[g_cur_cpu];
	if (!cpu->stopped) {
		if (!cpu->top_executed) {
#ifdef DEBUG_TIMING
			cpu_timing_reset();
#endif
			cpu_execute(cpu, (struct funcobj*)readptr(cpu->topfunc), 0);
#ifdef DEBUG_TIMING
			cpu_timing_print_report(-1);
#endif
			cpu->top_executed = 1;
		}
		else {
			if (cpu->paused)
				cpu_continue(cpu);
			else if (g_overlay_mode != overlay_inactive) {
				value_t fval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), str_intern(cpu, "onoverlay", 9));
				if (value_get_type(fval) == t_func) {
					if (g_overlay_mode == overlay_pending) {
						g_overlay_mode = overlay_active;
						cpu->overlay_state = writeptr(tab_new(cpu));
					}
					struct funcobj* fobj = (struct funcobj*)value_get_object(fval);
					struct tabobj* overlay_state = (struct tabobj*)readptr(cpu->overlay_state);
					cpu_execute(cpu, fobj, 1, value_tab(overlay_state));
				}
			}
			else {
				value_t fval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), LIT(onframe));
				if (value_get_type(fval) == t_func) {
					struct funcobj* fobj = (struct funcobj*)value_get_object(fval);
					cpu_execute(cpu, fobj, 0);
				}
			}
		}
		if (cpu->paused)
			cpu->delayed_frames++;
		else {
			cpu->last_delayed_frames = cpu->delayed_frames;
			cpu->delayed_frames = 0;
			gc_collect(cpu);
			if (g_overlay_mode == overlay_close_pending) {
				g_overlay_mode = overlay_inactive;
				cpu->overlay_state = writeptr_nullable(NULL);
				load_cpu_state();
			}
			else {
				struct gfx* gfx = console_getgfx();
				gfx->bufno = !gfx->bufno;
				memcpy(gfx->screen[gfx->bufno], gfx->screen[!gfx->bufno], sizeof(gfx->screen[0]));
			}
		}
#ifdef _DEBUG
		mem_check(&cpu->alloc);
#endif
	}
	key_postupdate(&g_io);
}

struct io* console_getio() {
	return &g_io;
}

struct gfx* console_getgfx() {
	if (g_overlay_mode >= overlay_active)
		return &g_overlay_gfx;
#ifdef HIERARCHICAL_MEMORY
	return &g_gfx;
#else
	return &g_cpus[g_cur_cpu]->gfx;
#endif
}

struct gfx* console_getgfx_pid(int pid) {
#ifdef HIERARCHICAL_MEMORY
	if (pid == g_cur_cpu && g_overlay_mode < overlay_active)
		return &g_gfx;
#endif
	return &g_cpus[pid]->gfx;
}

struct gfx* console_getgfx_overlay() {
	return &g_overlay_gfx;
}

int console_getpid() {
	return g_cur_cpu;
}

void console_kill(int pid) {
	if (pid == 0)
		return;
	int parent = g_cpus[g_cur_cpu]->parent;
	cpu_destroy(g_cpus[g_cur_cpu]);
	g_cpus[g_cur_cpu] = NULL;
	g_cur_cpu = g_next_cpu = parent == -1 ? 0 : parent;
	load_cpu_state();
}

int console_getpixel(int x, int y) {
	struct gfx* gfx = console_getgfx();
	int i = (y * WIDTH + x) / 2;
	if (x % 2)
		return gfx->screen[!gfx->bufno][i] >> 4;
	else
		return gfx->screen[!gfx->bufno][i] & 0x0F;
}
