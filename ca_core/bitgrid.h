#ifndef BITGRID_H
#define BITGRID_H

#include <stdint.h>

typedef struct {
    int size;              /* grid dimension (must be multiple of 64 for now) */
    int words_per_row;     /* size / 64 */
    uint64_t *bits;        /* row-major, each row = words_per_row uint64_t */
    uint64_t *next_bits;
} BitGrid;

BitGrid *bitgrid_create(int size);
void bitgrid_destroy(BitGrid *g);
void bitgrid_clear(BitGrid *g);
void bitgrid_randomize(BitGrid *g, uint64_t (*rng)(void));
void bitgrid_copy(const BitGrid *src, BitGrid *dst);

/* inline get/set */
static inline int bitgrid_get(const BitGrid *g, int x, int y) {
    int word = x >> 6;
    int bit  = x & 63;
    return (g->bits[y * g->words_per_row + word] >> bit) & 1ULL;
}

static inline void bitgrid_set(BitGrid *g, int x, int y, int v) {
    int word = x >> 6;
    int bit  = x & 63;
    uint64_t mask = 1ULL << bit;
    uint64_t *w = &g->bits[y * g->words_per_row + word];
    if (v) *w |= mask; else *w &= ~mask;
}

/* count neighbors using bit-parallel operations (fast path) */
int bitgrid_moore_count(const BitGrid *g, int x, int y);

/* step a BitGrid using a standard 9-bit rule code (512-entry LUT).
   src->bits is updated in-place (buffers are swapped internally). */
void bitgrid_step_lut(BitGrid *src, BitGrid *dst, const uint8_t *lut);

/* Bit-parallel Conway's Life step. Processes 64 cells per word.
   g->bits is updated in-place (buffers are swapped internally).
   Toroidal wrap is applied both horizontally (cross-word) and vertically. */
void bitgrid_step_life(BitGrid *g);

/* standard Life rule as precomputed LUT (populated at runtime) */
extern uint8_t BITGRID_RULE_LIFE[512];

/* hash for period detection */
uint64_t bitgrid_hash(const BitGrid *g);

#endif
