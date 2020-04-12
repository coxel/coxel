#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "config.h"

NORETURN void platform_error(const char* msg);
NORETURN void platform_exit(int code);
void* platform_malloc(int size);
void platform_free(void* ptr);

void platform_copy(const char* ptr, int len);
int platform_paste(char* ptr, int len);

uint32_t platform_seed();

void* platform_open(const char* filename, uint32_t* filesize);
void* platform_create(const char* filename);
int platform_read(void* file, char* data, int len);
int platform_write(void* file, const char* data, int len);
void platform_close(void* file);

struct cart {
	const char* code;
	int codelen;
};
struct run_result {
	const char* err;
	int linenum;
};
NORETURN void critical_error(const char* fmt, ...);
struct run_result console_load(const char* filename, struct cart* cart);
struct run_result console_save(const char* filename, const struct cart* cart);
void console_init();
void console_factory_init();
void console_destroy();
struct run_result console_run(const struct cart* cart);
void console_update();
struct gfx* console_getgfx();
struct io* console_getio();

#endif
