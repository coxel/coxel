#include "alloc.h"
#include "arith.h"
#include "gc.h"
#include "platform.h"
#include "rand.h"
#include "str.h"

#include <string.h>

/* murmurhash3 x86_32 */
uint32_t str_hash(const void* key, int len) {
	const uint8_t* data = (const uint8_t*)key;
	const int nblocks = len / 4;

	uint32_t h1 = 0; /* seed */

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

	for (int i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = ROTL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = ROTL32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
	uint32_t k1 = 0;
	switch (len & 3)
	{
	case 3: k1 ^= tail[2] << 16; /* fallthrough */
	case 2: k1 ^= tail[1] << 8; /* fallthrough */
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
	};
	h1 ^= len;
	h1 = fmix32(h1);
	return h1;
}

int str_equal(const void* key1, int len1, const void* key2, int len2) {
	if (len1 != len2)
		return 0;
	const uint8_t* k1 = (const uint8_t*)key1;
	const uint8_t* k2 = (const uint8_t*)key2;
	for (int i = 0; i < len1; i++)
		if (k1[i] != k2[i])
			return 0;
	return 1;
}

int str_vsprintf(char* buf, const char* format, va_list args) {
	char* p = buf;
	while (*format) {
		if (*format == '%') {
			format++;
			switch (*format++) {
			case '%': *p++ = '%'; break;
			case 'd': {
				int num = va_arg(args, int);
				p += int_format(num, p);
				break;
			}
			case 'f': {
				number num = va_arg(args, number);
				p += num_format(num, 4, p);
				break;
			}
			case 'c': {
				char ch = va_arg(args, int);
				*p++ = ch;
				break;
			}
			case 's': {
				const char* str = va_arg(args, const char*);
				int len = (int)strlen(str);
				memcpy(p, str, len);
				p += len;
				break;
			}
			case 'S': {
				const char* str = va_arg(args, const char*);
				int len = va_arg(args, int);
				memcpy(p, str, len);
				p += len;
				break;
			}
			default: platform_error("str_vsprintf() format error.");
			}
		}
		else
			*p++ = *format++;
	}
	*p = 0;
	return (int)(p - buf);
}

#define INITIAL_STRTAB_SIZE		1024 /* must be power of 2 */

void strtab_init(struct cpu* cpu) {
	cpu->strtab_cnt = 0;
	cpu->strtab_size = INITIAL_STRTAB_SIZE;
	cpu->strtab = (struct strobj**)mem_malloc(cpu->alloc, cpu->strtab_size * sizeof(struct strobj*));
	for (int i = 0; i < cpu->strtab_size; i++)
		cpu->strtab[i] = 0;
}

static void strtab_grow(struct cpu* cpu) {
	platform_error("Not implemented.");
}

void str_destroy(struct cpu* cpu, struct strobj* str) {
	/* Remove from intern table */
	uint32_t bucket = str->hash % cpu->strtab_size;
	struct strobj** prev = &cpu->strtab[bucket];
	for (struct strobj* p = *prev; p; prev = &p->next, p = *prev) {
		if (p == str) {
			*prev = p->next;
			mem_free(cpu->alloc, p);
			return;
		}
	}
	platform_error("Internal error.");
}

static struct strobj* str_intern_impl(struct cpu* cpu, const char* str, int len, int nogc) {
	uint32_t hash = str_hash(str, len);
	uint32_t bucket = hash % cpu->strtab_size;
	for (struct strobj* p = cpu->strtab[bucket]; p; p = p->next) {
		if (str_equal(p->data, p->len, str, len))
			return p;
	}
	if (cpu->strtab_cnt * 4 >= cpu->strtab_size * 3) { /* >75% load? */
		/* rehash */
		strtab_grow(cpu);
		bucket = hash % cpu->strtab_size;
	}
	struct strobj* obj;
	if (nogc)
		obj = (struct strobj*)mem_malloc(cpu->alloc, sizeof(struct strobj) + len);
	else
		obj = (struct strobj*)gc_alloc(cpu, t_str, sizeof(struct strobj) + len);
	obj->hash = hash;
	obj->len = len;
	memcpy(obj->data, str, len);
	obj->next = cpu->strtab[bucket];
	cpu->strtab[bucket] = obj;
	return obj;
}

struct strobj* str_intern(struct cpu* cpu, const char* str, int len) {
	return str_intern_impl(cpu, str, len, 0);
}

struct strobj* str_intern_nogc(struct cpu* cpu, const char* str, int len) {
	return str_intern_impl(cpu, str, len, 1);
}

struct strobj* str_concat(struct cpu* cpu, struct strobj* lval, struct strobj* rval) {
	// TODO: Reduce temporary memory consumption
	if (lval->len == 0)
		return rval;
	if (rval->len == 0)
		return lval;
	int totlen = lval->len + rval->len;
	char* temp = (char*)mem_malloc(cpu->alloc, totlen);
	memcpy(temp, lval->data, lval->len);
	memcpy(temp + lval->len, rval->data, rval->len);
	struct strobj* retval = str_intern(cpu, temp, lval->len + rval->len);
	mem_free(cpu->alloc, temp);
	return retval;
}

struct value str_get(struct cpu* cpu, struct strobj* str, number index) {
	int idx = num_uint(index);
	if (idx >= str->len)
		return value_undef(cpu);
	else
		return value_str(cpu, str_intern(cpu, &str->data[idx], 1));
}

static void libstr_indexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = 0;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1))) {
		RET = value_num(cpu, num_kint(-1));
		return;
	}
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		RET = value_num(cpu, num_kuint(ret));
		return;
	}
	else if (start < str->len) {
		if (pat->len == 1) {
			void* p = memchr(str->data + start, pat->data[0], str->len - start);
			if (p != NULL) {
				RET = value_num(cpu, num_kuint((char*)p - str->data));
				return;
			}
		}
		else {
			for (int i = start; i <= str->len - pat->len; i++) {
				int ok = 1;
				for (int j = 0; j < pat->len; j++)
					if (str->data[i + j] != pat->data[j]) {
						ok = 0;
						break;
					}
				if (ok) {
					RET = value_num(cpu, num_kuint(i));
					return;
				}
			}
		}
	}
	RET = value_num(cpu, num_kint(-1));
}

static void libstr_lastIndexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = str->len - pat->len;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1))) {
		RET = value_num(cpu, num_kint(-1));
		return;
	}
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		RET = value_num(cpu, num_kuint(ret));
		return;
	}
	else {
		if (start > str->len - pat->len)
			start = str->len - pat->len;
		for (int i = start; i >= 0; i--) {
			int ok = 1;
			for (int j = 0; j < pat->len; j++)
				if (str->data[i + j] != pat->data[j]) {
					ok = 0;
					break;
				}
			if (ok) {
				RET = value_num(cpu, num_kuint(i));
				return;
			}
		}
	}
	RET = value_num(cpu, num_kint(-1));
}

static void libstr_substr(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	int start = num_uint(to_number(cpu, ARG(0)));
	if (start > str->len)
		start = str->len;
	int count = nargs == 1 ? str->len - start : num_uint(to_number(cpu, ARG(1)));
	if (start + count > str->len)
		count = str->len - start;
	RET = value_str(cpu, str_intern(cpu, str->data + start, count));
}

struct value str_fget(struct cpu* cpu, struct strobj* str, struct strobj* key) {
	if (key == cpu->_lit_indexOf)
		return value_cfunc(cpu, libstr_indexOf);
	else if (key == cpu->_lit_lastIndexOf)
		return value_cfunc(cpu, libstr_lastIndexOf);
	else if (key == cpu->_lit_substr)
		return value_cfunc(cpu, libstr_substr);
	else if (key == cpu->_lit_length)
		return value_num(cpu, num_kuint(str->len));
	else
		return value_undef(cpu);
}
