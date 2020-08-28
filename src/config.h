#ifndef _CONFIG_H
#define _CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define COXEL_STATE_MAGIC		' xoc'
#define COXEL_STATE_VERSION		0

//#define DEBUG_TIMING

#ifdef DEBUG_TIMING

#if defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__)
#include <intrin.h>
#define MEASURE_DEFINES()	int64_t measure_begin, measure_end; unsigned int measure_dummy
#define MEASURE_START()		_mm_mfence(); measure_begin = __rdtscp(&measure_dummy); _mm_lfence()
#define MEASURE_END()		_mm_mfence(); measure_end = __rdtscp(&measure_dummy); _mm_lfence()
#define MEASURE_DURATION()	(measure_end - measure_begin)
#elif defined(ESP_PLATFORM)
#include <xtensa/hal.h>
#define MEASURE_DEFINES()	int measure_begin, measure_end
#define MEASURE_START()		measure_begin = xthal_get_ccount(); __sync_synchronize()
#define MEASURE_END()		__sync_synchronize(); measure_end = xthal_get_ccount()
#define MEASURE_DURATION()	(measure_end - measure_begin)
#else
#error Timing primitives not defined for this platform.
#endif

#endif

#if !defined(_DEBUG) && !defined(ESP_PLATFORM)
#define RELATIVE_ADDRESSING
#endif

#if defined(ESP_PLATFORM)
#define HIERARCHICAL_MEMORY
#endif

/* Function attributes */
#ifdef _MSC_VER
#define NORETURN	__declspec(noreturn)
#define FORCEINLINE	__forceinline
#else
#define NORETURN	__attribute__((noreturn))
#define FORCEINLINE	__attribute__((always_inline))
#endif

/* Branch prediction hints */
#if defined(__GNUC__)
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)
#else
#define likely(x)		(x)
#define unlikely(x)		(x)
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
