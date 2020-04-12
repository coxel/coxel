#include "arith.h"

#include <string.h>

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
	*outnum = ((int16_t)int_value << FRAC_BITS) + frac_value;
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
	*outnum = (int_value << FRAC_BITS) + frac_value;
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
		int_value = num >> FRAC_BITS;
		int frac_binary = num & ((1 << FRAC_BITS) - 1);
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
