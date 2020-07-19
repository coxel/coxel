#include "alloc.h"
#include "arith.h"
#include "gc.h"
#include "platform.h"
#include "rand.h"
#include "str.h"

#include <string.h>

int is_allowed_char(char ch) {
	return (ch >= 32 && ch <= 126) || (ch == '\n');
}

int copy_allowed_chars(char* dest, int destlen, const char* src, int srclen) {
	int n = 0;
	for (int i = 0; i < srclen && n < destlen; i++)
		if (is_allowed_char(src[i]))
			dest[n++] = src[i];
	return n;
}

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
	ptr(struct strobj)* strtab = (ptr(struct strobj)*)mem_malloc(&cpu->alloc, cpu->strtab_size * sizeof(ptr_t));
	for (int i = 0; i < cpu->strtab_size; i++)
		strtab[i] = writeptr(NULL);
	cpu->strtab = writeptr(strtab);
}

static void strtab_grow(struct cpu* cpu) {
	platform_error("Not implemented.");
}

void str_destroy(struct cpu* cpu, struct strobj* str) {
	/* Remove from intern table */
	uint32_t bucket = str->hash % cpu->strtab_size;
	ptr(struct strobj)* strtab = (ptr(struct strobj)*)readptr(cpu->strtab);
	ptr_t* prev = &strtab[bucket];
	for (struct strobj* p = readptr(*prev); p; prev = &p->next, p = readptr(*prev)) {
		if (p == str) {
			*prev = p->next;
			mem_free(&cpu->alloc, p);
			return;
		}
	}
	platform_error("Internal error.");
}

static struct strobj* str_intern_impl(struct cpu* cpu, const char* str, int len, int nogc) {
	uint32_t hash = str_hash(str, len);
	uint32_t bucket = hash % cpu->strtab_size;
	ptr(struct strobj)* strtab = (ptr(struct strobj)*)readptr(cpu->strtab);
	for (struct strobj* p = readptr(strtab[bucket]); p; p = readptr(p->next)) {
		if (str_equal(p->data, p->len, str, len))
			return p;
	}
	if (cpu->strtab_cnt * 4 >= cpu->strtab_size * 3) { /* >75% load? */
		/* rehash */
		strtab_grow(cpu);
		strtab = (ptr(struct strobj)*)readptr(cpu->strtab);
		bucket = hash % cpu->strtab_size;
	}
	struct strobj* obj;
	if (nogc)
		obj = (struct strobj*)mem_malloc(&cpu->alloc, sizeof(struct strobj) + len);
	else
		obj = (struct strobj*)gc_alloc(cpu, t_str, sizeof(struct strobj) + len);
	obj->hash = hash;
	obj->len = len;
	memcpy(obj->data, str, len);
	obj->next = strtab[bucket];
	strtab[bucket] = writeptr(obj);
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
	char* temp = (char*)mem_malloc(&cpu->alloc, totlen);
	memcpy(temp, lval->data, lval->len);
	memcpy(temp + lval->len, rval->data, rval->len);
	struct strobj* retval = str_intern(cpu, temp, lval->len + rval->len);
	mem_free(&cpu->alloc, temp);
	return retval;
}

struct value str_get(struct cpu* cpu, struct strobj* str, number index) {
	int idx = num_uint(index);
	if (idx >= str->len)
		return value_undef(cpu);
	else
		return value_str(cpu, str_intern(cpu, &str->data[idx], 1));
}

#define SUFFIX
#define CMP(x, xlen, i, y, ylen, j) (x[i] - y[j])
#include "strstr.h"
#undef CMP
#undef SUFFIX

#define SUFFIX _rev
#define CMP(x, xlen, i, y, ylen, j) (x[xlen - 1 - (i)] - y[ylen - 1 - (j)])
#include "strstr.h"
#undef CMP
#undef SUFFIX

static int get_index_of_cycles(int start, int len, int pos, int patlen) {
	if (pos == -1)
		pos = len;
	else
		pos += patlen;
	int chars = (pos - start) + 3 * patlen;
	return CYCLES_CHARS(chars);
}

struct value libstr_indexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = 0;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1)))
		return value_num(cpu, num_kint(-1));
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		return value_num(cpu, num_kuint(ret));
	}
	else {
		int pos = index_of(str->data, str->len, pat->data, pat->len, start);
		cpu->cycles += get_index_of_cycles(start, str->len, pos, pat->len);
		return value_num(cpu, num_kuint(pos));
	}
}

struct value libstr_lastIndexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = str->len - pat->len;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1)))
		return value_num(cpu, num_kint(-1));
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		return value_num(cpu, num_kuint(ret));
	}
	else {
		int rev_start = str->len - pat->len - start;
		if (rev_start < 0)
			rev_start = 0;
		int pos = index_of_rev(str->data, str->len, pat->data, pat->len, rev_start);
		cpu->cycles += get_index_of_cycles(rev_start, str->len, pos, pat->len);
		if (pos >= 0)
			pos = str->len - pat->len - pos;
		return value_num(cpu, num_kuint(pos));
	}
}

struct value libstr_substr(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	int start = num_uint(to_number(cpu, ARG(0)));
	if (start > str->len)
		start = str->len;
	int count = nargs == 1 ? str->len - start : num_uint(to_number(cpu, ARG(1)));
	if (start + count > str->len)
		count = str->len - start;
	cpu->cycles += CYCLES_CHARS(count);
	return value_str(cpu, str_intern(cpu, str->data + start, count));
}

struct value str_fget(struct cpu* cpu, struct strobj* str, struct strobj* key) {
	if (key == LIT(indexOf))
		return value_cfunc(cpu, cf_libstr_indexOf);
	else if (key == LIT(lastIndexOf))
		return value_cfunc(cpu, cf_libstr_lastIndexOf);
	else if (key == LIT(substr))
		return value_cfunc(cpu, cf_libstr_substr);
	else if (key == LIT(length))
		return value_num(cpu, num_kuint(str->len));
	else
		return value_undef(cpu);
}
