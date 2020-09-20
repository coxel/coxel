#ifndef _STR_H
#define _STR_H

#include <stdarg.h>
#include <stdint.h>
#include "cpu.h"

#define STR_MAXLEN      65535

struct str_part {
	const char* data;
	uint32_t len;
};

int is_allowed_char(char ch);
int copy_allowed_chars(char* dest, int destlen, const char* src, int srclen);
uint32_t str_hash(const void* key, int len);
uint32_t str_parts_hash(const struct str_part* parts, int nparts);
int str_equal(const void* key1, int len1, const void* key2, int len2);
/* Supported formats:
 * %%: '%' symbol
 * %d: 32-bit signed integer
 * %lld: 64-bit signed integer
 * %f: number
 * %c: char
 * %s: NULL-terminated string
 * %S: lengthed string (str, len)
 */
int str_vsprintf(char* buf, const char* format, va_list args);
void strtab_init(struct cpu* cpu);
void str_destroy(struct cpu* cpu, struct strobj* str);
struct strobj* str_intern(struct cpu* cpu, const char* str, int len);
struct strobj* str_intern_nogc(struct cpu* cpu, const char* str, int len);
struct strobj* str_parts_intern(struct cpu* cpu, const struct str_part* parts, int nparts);
struct strobj* str_parts_intern_nogc(struct cpu* cpu, const struct str_part* parts, int nparts);
struct strobj* str_concat(struct cpu* cpu, struct strobj* lval, struct strobj* rval);
value_t str_get(struct cpu* cpu, struct strobj* str, number index);
value_t str_fget(struct cpu* cpu, struct strobj* str, struct strobj* key);

#endif
