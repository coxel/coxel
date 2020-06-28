#include "compiler.h"
#include "cpu.h"
#include "gc.h"
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
#ifdef HIERARCHICAL_MEMORY
static struct gfx g_gfx;
#endif

#define SEPARATOR_MAGIC		"\n~!@#$%^&*()_"

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

static struct cart parse_cart(void* file, uint32_t filesize) {
	struct parse_context ctx;
	ctx.file = file;
	ctx.filesize = filesize;
	ctx.p = 0;
	ctx.len = 0;
	struct cart cart;
	char* code_buf = (char*)platform_malloc(65536);
	cart.code = code_buf;
	cart.codelen = 0;
	/* Read code section */
	int sep = 0;
	for (;;) {
		next_char(&ctx);
		if (ctx.ch == 0)
			return cart;
		if (ctx.ch == SEPARATOR_MAGIC[sep]) {
			// TODO: Code too big detection
			sep++;
			if (sep == sizeof(SEPARATOR_MAGIC)) {
				cart.codelen -= sizeof(SEPARATOR_MAGIC);
				break;
			}
		}
		else if (ctx.ch == SEPARATOR_MAGIC[0])
			sep = 1;
		else
			sep = 0;
		// TODO: Check for other invalid characters
		if (ctx.ch != '\r')
			code_buf[cart.codelen++] = ctx.ch;
	}
	platform_error("Unimplemented.");
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
	*cart = parse_cart(file, filesize);
	return res;
}

struct run_result console_save(const char* filename, const struct cart* cart) {
	struct run_result ret;
	ret.err = NULL;
	ret.linenum = -1;
	void* file = platform_create(filename);
	if (file == NULL)
		ret.err = "Create file failed.";
	else {
		if (platform_write(file, cart->code, cart->codelen) != cart->codelen)
			ret.err = "Write file failed.";
		platform_close(file);
	}
	return ret;
}

static void console_init_internal(int factory_firmware) {
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
	if (res.err != NULL)
		critical_error("Firmware compilation error:\nLine %d: %s", res.linenum + 1, res.err);
}

void console_init() {
	console_init_internal(0);
}

void console_factory_init() {
	console_init_internal(1);
}

static void save_cpu_state() {
#ifdef HIERARCHICAL_MEMORY
	memcpy(&g_cpus[g_cur_cpu]->gfx, &g_gfx, sizeof(struct gfx));
#endif
}

static void load_cpu_state() {
#ifdef HIERARCHICAL_MEMORY
	memcpy(&g_gfx, &g_cpus[g_cur_cpu]->gfx, sizeof(struct gfx));
#endif
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
	return ret;
}

void console_deserialize_init(void* f) {
#define DESERIALIZE(x, s) do { \
		if (platform_read(f, (char*)x, s) != s) { \
			critical_error("Read serialization data failed."); \
		} \
	} while (0)
#define STATE_CORRUPTED() critical_error("State corrupted.")

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
	g_cpus[slot] = cpu_new();
	if (g_cpus[slot] == NULL) {
		ret.err = "Out of memory.";
		return ret;
	}

	/* Compile cart code */
	struct compile_err err = compile(g_cpus[slot], cart->code, cart->codelen);
	if (err.msg != NULL) {
		ret.err = err.msg;
		ret.linenum = err.linenum;
		cpu_destroy(g_cpus[slot]);
		g_cpus[slot] = NULL;
		return ret;
	}

#ifdef _DEBUG
	char* buf = (char*)platform_malloc(65536);
	int len = cpu_dump_code(g_cpus[slot], buf, 65536);
	void* dumpfile = platform_create("debug_asm.txt");
	if (dumpfile) {
		platform_write(dumpfile, buf, len);
		platform_close(dumpfile);
	}
	platform_free(buf);
#endif

	g_cpus[slot]->parent = g_cur_cpu;

	/* Set cpu to be run */
	g_next_cpu = slot;

	return ret;
}

void console_update() {
	key_preupdate(&g_io);
	if (g_cur_cpu != g_next_cpu) {
		save_cpu_state();
		g_cur_cpu = g_next_cpu;
		load_cpu_state();
	}
	if (key_is_pressed(kc_esc) && g_cur_cpu != 0) {
		int parent = g_cpus[g_cur_cpu]->parent;
		cpu_destroy(g_cpus[g_cur_cpu]);
		g_cpus[g_cur_cpu] = NULL;
		g_cur_cpu = g_next_cpu = parent == -1 ? 0 : parent;
		load_cpu_state();
	}
	struct cpu* cpu = g_cpus[g_cur_cpu];
	if (!cpu->stopped) {
		if (!cpu->top_executed) {
			cpu_execute(cpu, (struct funcobj*)readptr(cpu->topfunc));
			cpu->top_executed = 1;
			gc_collect(cpu);
		}
		else {
			struct value fval = tab_get(cpu, (struct tabobj*)readptr(cpu->globals), LIT(onframe));
			if (fval.type == t_func) {
				struct funcobj* fobj = readptr(fval.func);
				cpu_execute(cpu, fobj);
				cpu->frame++;
				if (cpu->frame % 60 == 0)
					gc_collect(cpu);
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
#ifdef HIERARCHICAL_MEMORY
	return &g_gfx;
#else
	return &g_cpus[g_cur_cpu]->gfx;
#endif
}
