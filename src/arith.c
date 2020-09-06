#include "arith.h"
#include "config.h"

#include <string.h>

#define LOG2_DATA_BITS		4
/* 1/c, log2(c), Q0.32 format */
static const uint32_t log2_data[1 << LOG2_DATA_BITS][2] = {
	{ 0x00000000, 0x00000000 }, /* 1. */
	{ 0xf0f0f0f1, 0x1663f6fb }, /* 1.0625 */
	{ 0xe38e38e4, 0x2b803474 }, /* 1.125 */
	{ 0xd79435e5, 0x3f782d72 }, /* 1.1875 */
	{ 0xcccccccd, 0x5269e12f }, /* 1.25 */
	{ 0xc30c30c3, 0x646eea24 }, /* 1.3125 */
	{ 0xba2e8ba3, 0x759d4f81 }, /* 1.375 */
	{ 0xb21642c8, 0x86082807 }, /* 1.4375 */
	{ 0xaaaaaaab, 0x95c01a3a }, /* 1.5 */
	{ 0xa3d70a3d, 0xa4d3c25e }, /* 1.5625 */
	{ 0x9d89d89e, 0xb3500472 }, /* 1.625 */
	{ 0x97b425ed, 0xc1404eae }, /* 1.6875 */
	{ 0x92492492, 0xceaecfeb }, /* 1.75 */
	{ 0x8d3dcb09, 0xdba4a47b }, /* 1.8125 */
	{ 0x88888889, 0xe829fb69 }, /* 1.875 */
	{ 0x84210842, 0xf446359b }, /* 1.9375 */
};
static const uint32_t log2_poly[4] = {
	0x7154703b, 0xb8a71e4c, 0x7a8d6172, 0x51c4ad85
};

/* Q0.32 fixed point multiplication */
static inline uint32_t frac_mul(uint32_t a, uint32_t b) {
	return (uint32_t)((uint64_t)a * (uint64_t)b >> 32);
}

/* Calculate high precision log2(x), returns in Q32.32 fixed point number */
static uint64_t highprec_log2(number x) {
	/* Let x = 2^k * r, r in [1, 2)
	 * Then divide the range [1, 2) into several intervals
	 * log(x) = log(r) + k = log1p(r/c-1) + log(c) + k
	 * We make sure r/c is near 1 enough so that log1p(r/c-1) is approximated by
	 * polynomials with good precision.
	 */
	int k = mostbitidx(x) - 16;
	uint32_t r = x << (16 - k); /* Implicit bit in high 32-bit part */
	int ci = r >> (32 - LOG2_DATA_BITS);
	uint32_t invc = log2_data[ci][0];
	uint32_t logc = log2_data[ci][1];
	uint32_t t;
	if (ci == 0) /* invc = 1.0 */
		t = r;
	else {
		t = invc + frac_mul(r, invc);
		/* Underflow: clamp to zero since we don't support negative r/c-1
		 * Mathematically this should never happen.
		 * But in practice it may happen due to rounding errors.
		 */
		if (t > invc)
			t = 0;
	}

	/* Approximate log1p(t) with order 4 polynomial
	 * log1p(t) ~ (1+a1)*t - a2*t^2 + a3*t^3 - a4*t^4
	 *          = t + ((a1 - a2*t) + (a3 - a4*t)*t^2)*t
	 * Note: a1 - a2*t >= 0, a3 - a4* t >= 0
	 */
	uint32_t t2 = frac_mul(t, t);
	uint32_t f1 = log2_poly[0] - frac_mul(log2_poly[1], t);
	uint32_t f2 = frac_mul(log2_poly[2] - frac_mul(log2_poly[3], t), t2);
	uint32_t f = t + frac_mul(f1 + f2, t);
	/* Handle underflow */
	if (f >= 0x80000000)
		f = 0;

	uint64_t result = (int64_t)k << 32;
	result += logc;
	result += f;
	return result;
}

#define EXP2_DATA_BITS		5
/* 2^i for i in [0, 1) step 1/32, Q1.48 format, int bit always 1 */
static const uint64_t exp2_data[1 << EXP2_DATA_BITS] = {
	0x1000000000000,
	0x1059b0d315857,
	0x10b5586cf9891,
	0x111301d0125b5,
	0x1172b83c7d518,
	0x11d4873168b9b,
	0x12387a6e75624,
	0x129e9df51fdee,
	0x1306fe0a31b71,
	0x1371a7373aa9d,
	0x13dea64c12342,
	0x144e086061893,
	0x14bfdad5362a2,
	0x15342b569d4f8,
	0x15ab07dd48543,
	0x16247eb03a558,
	0x16a09e667f3bd,
	0x171f75e8ec5f7,
	0x17a11473eb018,
	0x182589994cce1,
	0x18ace5422aa0e,
	0x193737b0cdc5e,
	0x19c49182a3f09,
	0x1a5503b23e256,
	0x1ae89f995ad3b,
	0x1b7f76f2fb5e4,
	0x1c199bdd8552a,
	0x1cb720dcef907,
	0x1d5818dcfba48,
	0x1dfc97337b9b6,
	0x1ea4afa2a490e,
	0x1f50765b6e454,
};
static const uint32_t exp2_poly[4] = {
	0xb1721934, 0x3d7eb5fe, 0x0e5d1b0d
};

/* Input in Q32.32 fixed point format */
static number highprec_exp2(uint64_t x) {
	/* Let x = k + c + r, k is integer, r in range [0, 1)
	 * Then divide the range [0, 1) into several intervals.
	 * Let c <= r be the largest multiple of 1/N.
	 * exp2(x) = 2^k * exp(c) * exp(r-c)
	 */
	int32_t k = (int32_t)(uint32_t)(x >> 32);
	/* Avoid overflow */
	if (k >= 15)
		return NUM_MAX;
	else if (k <= -16)
		return NUM_MIN;

	uint32_t r = (uint32_t)x;
	int ci = r >> (32 - EXP2_DATA_BITS);
	uint64_t p = exp2_data[ci] >> (16 - k);

	uint32_t t = r << EXP2_DATA_BITS >> EXP2_DATA_BITS;
	/* Approximate exp2(t) with order 3 polynomial
	 * exp2(t) ~ 1 + a1*t + a2*t^2 + a3*t^3
	 *         = 1 + a1*t + (a2+a3*t)*t^2
	 */
	uint32_t t2 = frac_mul(t, t);
	uint32_t f2 = frac_mul(exp2_poly[2], t);
	uint32_t f1 = frac_mul(exp2_poly[0], t);
	f2 = frac_mul(exp2_poly[1] + f2, t2);
	uint32_t f = f1 + f2;

	/* Long multiply two parts : result = p * (1 + f) */
	uint64_t result = p;
	result += frac_mul((uint32_t)p, f);
	result += (uint64_t)(uint32_t)(p >> 32) * f;
	return NUM_ROUND(result >> 16);
}

number num_pow(number a, number b) {
	if (b == 0)
		return num_kuint(1);
	if (a == 0)
		return num_kuint(0);
	int neg = 0;
	uint64_t l;
	if ((int32_t)a >= 0)
		l = highprec_log2(a);
	else {
		if (b & ((1 << (FRAC_BITS + FRAC_SHIFT_BITS)) - 1))
			return 0;
		neg = b & (1 << (FRAC_BITS + FRAC_SHIFT_BITS));
		if (a == NUM_MIN)
			l = (uint64_t)(INT_BITS - 1) << 32;
		else
			l = highprec_log2(num_neg(a));
	}
	/* Signed long multiply two parts : r = l * b */
	int32_t l_hi = (uint32_t)(l >> 32);
	uint32_t l_lo = (uint32_t)l;
	uint64_t r = ((int64_t)l_hi * (int64_t)(int32_t)b) << 16;
	r += ((int64_t)l_lo * (int64_t)(int32_t)b) >> 16;
	number result = highprec_exp2(r);
	if (neg)
		result = num_neg(result);
	return result;
}

static inline int parse_digit(int base, char ch, int* out) {
	if (ch >= '0' && ch <= '9')
		*out = ch - '0';
	else if (ch >= 'A' && ch <= 'Z')
		*out = ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'z')
		*out = ch - 'A' + 10;
	else
		return 0;
	return *out < base;
}

static int num_parse_base(int base2, const char* begin, const char* end, number* outnum) {
	if (begin == end)
		return 0;
	int base = 1 << base2;
	/* integer part */
	const char* p;
	int int_value = 0;
	for (p = begin; p != end; ++p) {
		if (*p == '.')
			break;
		int v;
		if (!parse_digit(base, *p, &v))
			return 0;
		int_value = (int_value << base2) + v;
		if (int_value >= (1 << INT_BITS))
			return NUM_OVERFLOW;
	}
	/* fractional part */
	int frac_value = 0;
	if (p != end) {
		p++;
		int bcnt = 0;
		for (; p != end; ++p) {
			int v;
			if (!parse_digit(base, *p, &v))
				return 0;
			if (bcnt < FRAC_BITS) {
				bcnt += base2;
				frac_value = (frac_value << base2) + v;
			}
		}
		if (bcnt < FRAC_BITS)
			frac_value <<= FRAC_BITS - bcnt;
		if (bcnt > FRAC_BITS)
			frac_value >>= bcnt - FRAC_BITS;
	}
	*outnum = ((int16_t)int_value << INT_SHIFT_BITS) + (frac_value << FRAC_SHIFT_BITS);
	return 1;
}

int num_parse(const char* begin, const char* end, number* outnum) {
	if (begin == end)
		return 0;
	int neg = 0;
	if (*begin == '-') {
		neg = 1;
		begin++;
	}
	else if (*begin == '0') {
		if (begin + 1 == end) {
			*outnum = num_kint(0);
			return 1;
		}
		else if (begin[1] == 'b')
			return num_parse_base(1, begin + 2, end, outnum);
		else if (begin[1] == 'o')
			return num_parse_base(3, begin + 2, end, outnum);
		else if (begin[1] == 'x')
			return num_parse_base(4, begin + 2, end, outnum);
		else if (begin[1] != '.')
			return num_parse_base(3, begin + 1, end, outnum);
	}
	/* integer part */
	int int_value = 0;
	const char* p;
	for (p = begin; p != end; ++p) {
		if (*p == '.')
			break;
		int_value = int_value * 10 + (*p - '0');
		if (int_value >= (1 << INT_BITS))
			return NUM_OVERFLOW;
	}

	/* fractional part */
	int frac_value = 0;
	if (p != end) {
		++p;
		int ref = 1;
		int frac_digits = 0;
		for (int i = 0; i < FRAC_SAFE_DIGITS; i++) {
			ref *= 10;
			frac_digits *= 10;
			if (p + i < end)
				frac_digits += p[i] - '0';
		}
		for (int i = 0; i < FRAC_BITS; i++) {
			frac_value *= 2;
			frac_digits *= 2;
			if (frac_digits >= ref) {
				frac_value += 1;
				frac_digits -= ref;
			}
		}
	}
	*outnum = (int_value << INT_SHIFT_BITS) + (frac_value << FRAC_SHIFT_BITS);
	if (neg)
		*outnum = num_neg(*outnum);
	return 1;
}

int num_format(number num, int frac_digits, char* output) {
	char* outp = output;
	int int_value;
	int frac_value;
	int frac_ref = 1;
	for (int i = 0; i < frac_digits; i++)
		frac_ref *= 10;
	if (num == NUM_OVERFLOW) {
		int_value = 1 << (INT_BITS - 1);
		frac_value = 0;
		*outp++ = '-';
	}
	else {
		if (num & 0x80000000) {
			/* negative */
			num = num_neg(num);
			*outp++ = '-';
		}
		int_value = num >> INT_SHIFT_BITS;
		int frac_binary = (num & ((1 << INT_SHIFT_BITS) - 1)) >> FRAC_SHIFT_BITS;
		frac_value = 0;
		for (int i = 0; i < frac_digits; i++) {
			frac_binary *= 10;
			frac_value = frac_value * 10 + (frac_binary >> FRAC_BITS);
			frac_binary = frac_binary & ((1 << FRAC_BITS) - 1);
		}
		/* rounding */
		if (((frac_binary * 10) >> FRAC_BITS) >= 5) {
			frac_value++;
			if (frac_value == frac_ref) {
				frac_value = 0;
				int_value++;
			}
		}
	}
	/* output int part */
	if (int_value == 0)
		*outp++ = '0';
	else {
		int int_ref = 100000;
		int leading_zero = 1;
		for (int i = 0; i < 5; i++) {
			int_ref /= 10;
			int cur = int_value / int_ref;
			if (cur != 0)
				leading_zero = 0;
			if (!leading_zero)
				*outp++ = cur + '0';
			int_value %= int_ref;
		}
	}
	/* output fractional part */
	if (frac_value) {
		*outp++ = '.';
		for (int i = 0; i < frac_digits; i++) {
			frac_ref /= 10;
			int cur = frac_value / frac_ref;
			*outp++ = cur + '0';
			frac_value %= frac_ref;
		}
		/* remove trailing zeros */
		while (*(outp - 1) == '0')
			--outp;
	}
	return (int)(outp - output);
}

int int_format(int num, char* output) {
	char buf[20];
	if (num == 0) {
		*output = '0';
		return 1;
	}
	else if (num == 0x80000000) {
		memcpy(output, "-2147483648", 11);
		return 11;
	}
	int neg = 0;
	int len = 0;
	if (num < 0) {
		neg = 1;
		num = -num;
		*output++ = '-';
	}
	while (num) {
		buf[len++] = num % 10 + '0';
		num /= 10;
	}
	for (int i = 0; i < len; i++)
		*output++ = buf[len - 1 - i];
	return neg + len;
}

int int64_format(int64_t num, char* output) {
	char buf[20];
	if (num == 0) {
		*output = '0';
		return 1;
	}
	else if (num == 0x8000000000000000LL) {
		memcpy(output, "-9223372036854775808", 20);
		return 20;
	}
	int neg = 0;
	int len = 0;
	if (num < 0) {
		neg = 1;
		num = -num;
		*output++ = '-';
	}
	while (num) {
		buf[len++] = num % 10 + '0';
		num /= 10;
	}
	for (int i = 0; i < len; i++)
		*output++ = buf[len - 1 - i];
	return neg + len;
}

static const char hex[16] = "0123456789abcdef";
char to_hex(int ch) {
	return hex[ch];
}

int from_hex(char ch, int* ok) {
	if (ch >= 'a' && ch <= 'f') {
		*ok = 1;
		return 10 + (ch - 'a');
	}
	else if (ch >= '0' && ch <= '9') {
		*ok = 1;
		return ch - '0';
	}
	else {
		*ok = 0;
		return 0;
	}
}