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

uint32_t str_hash(const void* key, int len) {
	struct str_part part = { .data = key, .len = len };
	return str_parts_hash(&part, 1);
}

/* Derived from murmurhash3 x86_32 algorithm */
uint32_t str_parts_hash(const struct str_part* parts, int nparts) {
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	uint32_t tot_len = 0;
	uint32_t h1 = 0;
	uint32_t k1 = 0;
	uint32_t leftover_bytes = 0;
	for (int ipart = 0; ipart < nparts; ipart++) {
		const struct str_part* part = &parts[ipart];
		uint32_t plen = part->len;
		const uint8_t* data = (const uint8_t*)part->data;
		tot_len += plen;
		if (leftover_bytes + plen >= 4) {
			const uint32_t consume_bytes = 4 - leftover_bytes;
			uint32_t p = 0;
			switch (consume_bytes) {
			case 4: p += data[3] << 24; /* fallthrough */
			case 3: p += data[2] << 16; /* fallthrough */
			case 2: p += data[1] << 8; /* fallthrough */
			case 1: p += data[0];
			}
			k1 += p << (leftover_bytes * 8);

			k1 *= c1;
			k1 = ROTL32(k1, 15);
			k1 *= c2;

			h1 ^= k1;
			h1 = ROTL32(h1, 13);
			h1 = h1 * 5 + 0xe6546b64;

			data += consume_bytes;
			plen -= consume_bytes;

			const int nblocks = plen / 4;
			const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
			for (int i = -nblocks; i; i++) {
				k1 = blocks[i];

				k1 *= c1;
				k1 = ROTL32(k1, 15);
				k1 *= c2;

				h1 ^= k1;
				h1 = ROTL32(h1, 13);
				h1 = h1 * 5 + 0xe6546b64;
			}
			k1 = 0;
			leftover_bytes = 0;
			data = (const uint8_t*)blocks;
		}
		uint32_t p = 0;
		switch (plen & 3) {
		case 3: p += data[2] << 16; /* fallthrough */
		case 2: p += data[1] << 8; /* fallthrough */
		case 1: p += data[0];
		}
		k1 += p << (leftover_bytes * 8);
		leftover_bytes += plen & 3;
	}
	if (leftover_bytes) {
		k1 *= c1;
		k1 = ROTL32(k1, 15);
		k1 *= c2;
		h1 ^= k1;
	}
	h1 ^= tot_len;
	h1 = fmix32(h1);
	return h1;
}

int str_equal(const void* key1, int len1, const void* key2, int len2) {
	if (len1 != len2)
		return 0;
	const uint8_t* k1 = (const uint8_t*)key1;
	const uint8_t* k2 = (const uint8_t*)key2;
	return memcmp(k1, k2, len1) == 0;
}

int str_parts_equal(const struct str_part* parts, int nparts, const void* key, int len) {
	const uint8_t* k = (const uint8_t*)key;
	for (int i = 0; i < nparts; i++) {
		const struct str_part* part = &parts[i];
		if (part->len > len)
			return 0;
		if (memcmp(part->data, k, part->len))
			return 0;
		k += part->len;
		len -= part->len;
	}
	return len == 0;
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
			case 'l': {
				if (*format++ != 'l')
					goto format_error;
				if (*format++ != 'd')
					goto format_error;
				int64_t num = va_arg(args, int64_t);
				p += int64_format(num, p);
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
			default: format_error: platform_error("str_vsprintf() format error.");
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
	ptr(struct strobj)* strtab = (ptr(struct strobj)*)mem_alloc(&cpu->alloc, cpu->strtab_size * sizeof(ptr_t));
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
			mem_dealloc(&cpu->alloc, p);
			return;
		}
	}
	platform_error("Internal error.");
}

static struct strobj* str_parts_intern_impl(struct cpu* cpu, const struct str_part* parts, int nparts, int nogc) {
	uint32_t hash = str_parts_hash(parts, nparts);
	uint32_t bucket = hash % cpu->strtab_size;
	ptr(struct strobj)* strtab = (ptr(struct strobj)*)readptr(cpu->strtab);
	for (struct strobj* p = readptr(strtab[bucket]); p; p = readptr(p->next)) {
		if (hash == p->hash && str_parts_equal(parts, nparts, p->data, p->len))
			return p;
	}
	if (cpu->strtab_cnt * 4 >= cpu->strtab_size * 3) { /* >75% load? */
		/* rehash */
		strtab_grow(cpu);
		strtab = (ptr(struct strobj)*)readptr(cpu->strtab);
		bucket = hash % cpu->strtab_size;
	}
	uint32_t len = 0;
	for (int i = 0; i < nparts; i++)
		len += parts[i].len;
	struct strobj* obj;
	if (nogc)
		obj = (struct strobj*)mem_alloc(&cpu->alloc, sizeof(struct strobj) + len);
	else
		obj = (struct strobj*)gc_alloc(cpu, t_str, sizeof(struct strobj) + len);
	obj->hash = hash;
	obj->len = len;
	char* p = obj->data;
	for (int i = 0; i < nparts; i++) {
		const struct str_part* part = &parts[i];
		memcpy(p, part->data, part->len);
		p += part->len;
	}
	obj->next = strtab[bucket];
	strtab[bucket] = writeptr(obj);
	return obj;
}

struct strobj* str_intern(struct cpu* cpu, const char* str, int len) {
	struct str_part part = { .data = str, .len = len };
	return str_parts_intern_impl(cpu, &part, 1, 0);
}

struct strobj* str_intern_nogc(struct cpu* cpu, const char* str, int len) {
	struct str_part part = { .data = str, .len = len };
	return str_parts_intern_impl(cpu, &part, 1, 1);
}

struct strobj* str_parts_intern(struct cpu* cpu, const struct str_part* parts, int nparts) {
	return str_parts_intern_impl(cpu, parts, nparts, 0);
}

struct strobj* str_parts_intern_nogc(struct cpu* cpu, const struct str_part* parts, int nparts) {
	return str_parts_intern_impl(cpu, parts, nparts, 1);
}

struct strobj* str_concat(struct cpu* cpu, struct strobj* lval, struct strobj* rval) {
	if (lval->len == 0)
		return rval;
	if (rval->len == 0)
		return lval;
	struct str_part parts[2] = {
		{.data = lval->data, .len = lval->len},
		{.data = rval->data, .len = rval->len},
	};
	return str_parts_intern(cpu, parts, 2);
}

value_t str_get(struct cpu* cpu, struct strobj* str, number index) {
	int idx = num_uint(index);
	if (idx >= str->len)
		return value_undef();
	else
		return value_str(str_intern(cpu, &str->data[idx], 1));
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

value_t libstr_indexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = 0;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1)))
		return value_num(num_kint(-1));
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		return value_num(num_kuint(ret));
	}
	else {
		int pos = index_of(str->data, str->len, pat->data, pat->len, start);
		cpu->cycles -= get_index_of_cycles(start, str->len, pos, pat->len);
		return value_num(num_kuint(pos));
	}
}

value_t libstr_lastIndexOf(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	struct strobj* pat = to_string(cpu, ARG(0));
	int start = str->len - pat->len;
	if (nargs == 2)
		start = num_uint(to_number(cpu, ARG(1)));
	if (start == num_uint(num_kint(-1)))
		return value_num(num_kint(-1));
	else if (pat->len == 0) {
		int ret = start <= str->len ? start : str->len;
		return value_num(num_kuint(ret));
	}
	else {
		int rev_start = str->len - pat->len - start;
		if (rev_start < 0)
			rev_start = 0;
		int pos = index_of_rev(str->data, str->len, pat->data, pat->len, rev_start);
		cpu->cycles -= get_index_of_cycles(rev_start, str->len, pos, pat->len);
		if (pos >= 0)
			pos = str->len - pat->len - pos;
		return value_num(num_kuint(pos));
	}
}

value_t libstr_substr(struct cpu* cpu, int sp, int nargs) {
	if (nargs != 1 && nargs != 2)
		argument_error(cpu);
	struct strobj* str = to_string(cpu, THIS);
	int start = num_uint(to_number(cpu, ARG(0)));
	if (start > str->len)
		start = str->len;
	int count = nargs == 1 ? str->len - start : num_uint(to_number(cpu, ARG(1)));
	if (start + count > str->len)
		count = str->len - start;
	cpu->cycles -= CYCLES_CHARS(count);
	return value_str(str_intern(cpu, str->data + start, count));
}

value_t str_fget(struct cpu* cpu, struct strobj* str, struct strobj* key) {
	if (key == LIT(indexOf))
		return value_cfunc(cf_libstr_indexOf);
	else if (key == LIT(lastIndexOf))
		return value_cfunc(cf_libstr_lastIndexOf);
	else if (key == LIT(substr))
		return value_cfunc(cf_libstr_substr);
	else if (key == LIT(length))
		return value_num(num_kuint(str->len));
	else
		return value_undef();
}
