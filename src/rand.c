#include "rand.h"

uint32_t fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static uint32_t splitmix32(uint32_t h) {
	return fmix32(h + 0x9e3779b9);
}

void rand_seed(struct cpu* cpu, uint32_t seed) {
	cpu->prng_s[0] = splitmix32(seed);
	cpu->prng_s[1] = splitmix32(cpu->prng_s[0]);
	cpu->prng_s[2] = splitmix32(cpu->prng_s[1]);
	cpu->prng_s[3] = splitmix32(cpu->prng_s[2]);
}

/* xoshiro128++ by David Blackman and Sebastiano Vigna */
uint32_t rand_next(struct cpu* cpu) {
	uint32_t* s = cpu->prng_s;
	const uint32_t result = ROTL32(s[0] + s[3], 7) + s[0];

	const uint32_t t = s[1] << 9;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;

	s[3] = ROTL32(s[3], 11);

	return result;
}

/* Daniel Lemire's fast uniform int distribution */
uint32_t rand_int(struct cpu* cpu, uint32_t max) {
	uint64_t m = (uint64_t)rand_next(cpu) * (uint64_t)max;
	if ((uint32_t)m < max) {
		uint64_t t = -(uint64_t)max % (uint64_t)max;
		while ((uint32_t)m < t)
			m = (uint64_t)rand_next(cpu) * (uint64_t)max;
	}
	return (uint32_t)(m >> 32);
}
