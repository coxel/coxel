/* Two way string matching algorithm
 * Constant space and worst case linear running time
 */

/* CMP(x, xlen, i, y, ylen, j) return x[i] - y[j]. */
#ifndef SUFFIX
#error Please define SUFFIX before include this header
#endif
#ifndef CMP
#error Please define CMP before include this header
#endif

#include "config.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

static int CONCAT(get_critical_factorization, SUFFIX)(const char* str, int len, int* period) {
	int i, i_rev, j, k, p, p_rev;
	/* Maximal suffix on <= */
	i = -1, j = 0, k = 1, p = 0;
	while (j + k < len) {
		int c = CMP(str, len, j + k, str, len, i + k);
		if (c < 0) { /* b < a */
			j += k;
			k = 1;
			p = j - i;
		}
		else if (c == 0) { /* b == a */
			if (k != p)
				k++;
			else {
				j += p;
				k = 1;
			}
		}
		else { /* b > a */
			i = j++;
			k = p = 1;
		}
	}

	/* Maximal suffix on >= */
	i_rev = -1, j = 0, k = 1, p_rev = 0;
	while (j + k < len) {
		int c = CMP(str, len, j + k, str, len, i_rev + k);
		if (c > 0) { /* b > a */
			j += k;
			k = 1;
			p_rev = j - i_rev;
		}
		else if (c == 0) { /* b == a */
			if (k != p_rev)
				k++;
			else {
				j += p_rev;
				k = 1;
			}
		}
		else { /* b < a */
			i_rev = j++;
			k = p_rev = 1;
		}
	}
	if (i >= i_rev) {
		*period = p;
		return i;
	}
	else {
		*period = p_rev;
		return i_rev;
	}
}

static int CONCAT(mem_compare, SUFFIX)(const char* x, int xlen, int p1, int p2, int len) {
	for (int i = 0; i < len; i++) {
		int c = CMP(x, xlen, p1 + i, x, xlen, p2 + i);
		if (c)
			return c;
	}
	return 0;
}

static int CONCAT(index_of, SUFFIX)(const char* t, int tlen, const char* x, int xlen, int start) {
	int i, j, pos, s, s0;
	int l, p;
	l = CONCAT(get_critical_factorization, SUFFIX)(x, xlen, &p);
	if (l + p + 1 <= xlen && CONCAT(mem_compare, SUFFIX)(x, xlen, 0, p, l + 1) == 0)
		s0 = xlen - p - 1;
	else {
		s0 = -1;
		p = max(l, xlen - l - 1) + 1;
	}
	pos = start, s = -1;
	while (pos + xlen <= tlen) {
		i = max(l, s) + 1;
		while (i < xlen && CMP(x, xlen, i, t, tlen, pos + i) == 0)
			i++;
		if (i < xlen) {
			pos += i - l;
			s = -1;
		}
		else {
			j = l;
			while (j > s && CMP(x, xlen, j, t, tlen, pos + j) == 0)
				j--;
			if (j == s)
				return pos;
			pos += p;
			s = s0;
		}
	}
	return -1;
}