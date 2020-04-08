#ifndef _CONFIG_H
#define _CONFIG_H

#include <stddef.h>
#include <stdint.h>

#ifdef _MSC_VER
#define NORETURN	__declspec(noreturn)
#define FORCEINLINE	__forceinline
#else
#define NORETURN	__attribute__((noreturn))
#define FORCEINLINE	__attribute__((always_inline))
#endif

#if defined(_MSC_VER)

#include <stdlib.h>
#pragma intrinsic(_rotl)
#define ROTL32(x,y)	_rotl(x,y)

#else

static inline uint32_t rotl32(uint32_t x, int8_t r) {
	return (x << r) | (x >> (32 - r));
}

#define	ROTL32(x,y)	rotl32(x,y)

#endif

#endif
