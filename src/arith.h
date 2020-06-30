#ifndef _ARITH_H
#define _ARITH_H

#include <stdint.h>

#define INT_BITS			16
#define FRAC_BITS			(32 - INT_BITS)
#define FRAC_SAFE_DIGITS	8
#define NUM_OVERFLOW		(number)0x80000000
typedef uint32_t number;

static inline number num_kint(int16_t x) {
	return x << FRAC_BITS;
}

static inline number num_kuint(uint16_t x) {
	return x << FRAC_BITS;
}

static inline int16_t num_int(number x) {
	if ((int32_t)x > 0)
		return (int32_t)x >> FRAC_BITS;
	else
		return (int32_t)(x + ((1 << FRAC_BITS) - 1)) >> FRAC_BITS;
}

static inline uint16_t num_uint(number x) {
	return (uint16_t)num_int(x);
}

static inline number num_neg(number a) {
	return -a;
}

static inline number num_abs(number a) {
	return (int32_t)a > 0 ? a : -a;
}

static inline number num_add(number a, number b) {
	return a + b;
}

static inline number num_sub(number a, number b) {
	return a - b;
}

static inline number num_mul(number a, number b) {
	return (number)((uint64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b) >> FRAC_BITS);
}

static inline number num_div(number a, number b) {
	// TODO: Corner cases
	return (number)(((int64_t)(int32_t)a << FRAC_BITS) / (int64_t)(int32_t)b);
}

static inline number num_mod(number a, number b) {
	// TODO: Corner cases
	return (number)((int64_t)(int32_t)a % (int64_t)(int32_t)b);
}

static inline number num_exp(number a, number b) {
	return 0;
}

static inline number num_and(number a, number b) {
	return num_kint(num_int(a) & num_int(b));
}

static inline number num_or(number a, number b) {
	return num_kint(num_int(a) | num_int(b));
}

static inline number num_xor(number a, number b) {
	return num_kint(num_int(a) ^ num_int(b));
}

static inline number num_not(number a) {
	return num_kint(~num_int(a));
}

static inline number num_shl(number a, number b) {
	return num_kint(num_int(a) << (num_int(b) & 0x0F));
}

static inline number num_shr(number a, number b) {
	return num_kint(num_int(a) >> (num_int(b) & 0x0F));
}

static inline number num_ushr(number a, number b) {
	return num_kint((uint16_t)num_int(a) >> ((uint16_t)num_int(b) & 0x0F));
}

static inline number num_floor(number a) {
	return a & 0xFFFF0000;
}

static inline number num_ceil(number a) {
	return (a + 0xFFFF) & 0xFFFF0000;
}

/* positive numbers only */
int num_parse(const char* begin, const char* end, number* outnum);
/* positive and negative numbers ok, return number of characters, no terminating NULL */
int num_format(number num, int frac_digits, char* output);
int int_format(int num, char* output);
char to_hex(int ch);
int from_hex(char ch, int* ok);

#endif
