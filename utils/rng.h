#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void rng_seed(uint64_t seed);
uint64_t rng_u64(void);

/* Stateful splitmix64 — caller owns the state pointer. Useful for per-grid
   or per-thread deterministic streams that don't interact with the global LCG. */
static inline uint64_t rng_splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

#endif
