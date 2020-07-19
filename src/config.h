#ifndef _CONFIG_H
#define _CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define COXEL_STATE_MAGIC		' xoc'
#define COXEL_STATE_VERSION		0

#if !defined(_DEBUG) && !defined(ESP_PLATFORM)
#define RELATIVE_ADDRESSING
#endif

#if defined(ESP_PLATFORM)
#define HIERARCHICAL_MEMORY
#endif

#ifdef _MSC_VER
#define NORETURN	__declspec(noreturn)
#define FORCEINLINE	__forceinline
#else
#define NORETURN	__attribute__((noreturn))
#define FORCEINLINE	__attribute__((always_inline))
#endif

/* Preprocessor concat string */
#define CONCAT_IMPL(x, y)	x ## y
#define CONCAT(x, y)		CONCAT_IMPL(x, y)

/* Bitwise rotation */
#if defined(_MSC_VER)

#include <stdlib.h>
#pragma intrinsic(_rotl)
#define ROTL32(x,y)	_rotl(x,y)

#else

static FORCEINLINE uint32_t rotl32(uint32_t x, int8_t r) {
	return (x << r) | (x >> (32 - r));
}

#define	ROTL32(x,y)	rotl32(x,y)

#endif

/* Count trailing zeros */
#if defined(__GNUC__)
#define leastbitidx(x)	__builtin_ctz(x)
#elif defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
static FORCEINLINE int leastbitidx(uint32_t x) {
	unsigned long index;
	_BitScanForward(&index, x);
	return (int)index;
}
#else
#error Unsupported operation.
#endif

#endif
