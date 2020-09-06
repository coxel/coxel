#ifndef _ARITH_H
#define _ARITH_H

#include <stdint.h>

/* 16.15 fixed point format, last bit ignored
 */
#define INT_BITS			16
#define FRAC_BITS			15
#define FRAC_SHIFT_BITS		1
#define INT_SHIFT_BITS		(FRAC_BITS + FRAC_SHIFT_BITS)
#define FRAC_SAFE_DIGITS	8
#define NUM_OVERFLOW		(number)0x80000000
#define NUM_MAX				(number)0x7FFFFFFE
#define NUM_MIN				(number)0x80000000
typedef uint32_t number;

static inline number num_kint(int16_t x) {
	return x << INT_SHIFT_BITS;
}

static inline number num_kuint(uint16_t x) {
	return x << INT_SHIFT_BITS;
}

static inline int16_t num_int(number x) {
	if ((int32_t)x > 0)
		return (int32_t)x >> INT_SHIFT_BITS;
	else
		return (int32_t)(x + (((1 << FRAC_BITS) - 1) << FRAC_SHIFT_BITS)) >> INT_SHIFT_BITS;
}

static inline uint16_t num_uint(number x) {
	return (uint16_t)num_int(x);
}

static inline number NUM_ROUND(uint32_t x) {
	if (x == 0x7FFFFFFF)
		return NUM_MAX;
	else
		return x + (x & 1);
}

static inline number num_of_division(int64_t a, int64_t b) {
	return NUM_ROUND((a << INT_SHIFT_BITS) / b);
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
	return (number)NUM_ROUND((uint64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b) >> INT_SHIFT_BITS);
}

static inline number num_div(number a, number b) {
	if (b == 0)
		return (int32_t)a >= 0 ? NUM_MAX : NUM_MIN;
	return (number)NUM_ROUND(((int64_t)(int32_t)a << INT_SHIFT_BITS) / (int64_t)(int32_t)b);
}

static inline number num_mod(number a, number b) {
	if (b == 0)
		return 0;
	return (number)((int64_t)(int32_t)a % (int64_t)(int32_t)b);
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
	return a & -(1 << INT_SHIFT_BITS);
}

static inline number num_ceil(number a) {
	return num_floor(a + ((1 << INT_SHIFT_BITS) - 1));
}

number num_pow(number a, number b);

/* positive numbers only */
int num_parse(const char* begin, const char* end, number* outnum);
/* positive and negative numbers ok, return number of characters, no terminating NULL */
int num_format(number num, int frac_digits, char* output);
int int_format(int num, char* output);
int int64_format(int64_t num, char* output);
char to_hex(int ch);
int from_hex(char ch, int* ok);

#endif
