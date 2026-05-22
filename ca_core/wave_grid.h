#ifndef WAVE_GRID_H
#define WAVE_GRID_H

/* ============================================================================
 * WaveGrid — Acoustic Cellular Automaton with Cell + Neighborhood Genomes
 * ============================================================================
 *
 * Brainstorm / design header for a new grid type that unifies three concepts:
 *   1. 26-bit acoustic payload: each bit = one segment of a waveform.
 *   2. Cell-level genome (fast adaptation) + neighborhood-level genome
 *      (slow territorial consensus).
 *   3. Dual-time-scale evolution: cell genomes mutate every tick;
 *      neighborhood genomes mutate every N ticks and act as a
 *      "species boundary" that modulates how cell genomes are expressed.
 *
 * Payload bit layout (26 active bits inside a uint32_t):
 *   bits  0-7  : coordinate X  (one acoustic "coordinate" = 8 segments)
 *   bits  8-15 : coordinate Y
 *   bits 16-23 : coordinate Z
 *   bits 24-25 : meta / segment select (e.g. which octave, waveform bank)
 *   bits 26-31 : RESERVED (must be zero for canonical operations)
 *
 * Semantics:
 *   - Each bit represents the presence/absence of energy in one segment
 *     of a decomposed acoustic waveform (e.g. FFT bin, wavelet coefficient).
 *   - Any 8 consecutive bits can be read as a scalar coordinate (0-255).
 *   - Any 24 consecutive bits form a 3-D vector (3 x 8-bit coordinates).
 *   - The payload is therefore simultaneously:
 *       (a) a binary acoustic fingerprint,
 *       (b) a spatial coordinate, and
 *       (c) a directional vector.
 *
 * Genome architecture:
 *   - CellGenome (16-bit, existing): per-cell rule select, coupling weight,
 *     mutation rate. Evolves at the fast time scale.
 *   - NeighborhoodGenome (16-bit): same layout as CellGenome, but
 *     represents the consensus genotype of the 3x3 Moore neighborhood.
 *     Evolves at the slow time scale (e.g. every 10 ticks).
 *   - Expression: the effective rule applied to a cell is a blend of its
 *     own genome and its neighborhood genome. For example:
 *       effective_rule = (nb_genome.rule_select * w + cell_genome.rule_select * (1-w))
 *     where w is a spatial coherence weight (higher when neighbors agree).
 *
 * Why two levels?
 *   - Cell genome = local adaptation to micro-environment (fast).
 *   - Neighborhood genome = territorial identity / species (slow).
 *   - If a cell's genome diverges too far from its neighborhood genome,
 *     its effective fitness is penalized. This creates an "immune system"
 *     that preserves patch coherence while still allowing micro-specialization.
 *
 * Data layout:
 *   The grid is deliberately fat: each cell carries
 *     uint8_t alive + uint32_t wave + 16-bit cell_genome + 16-bit nb_genome + double fitness
 *   = 1 + 4 + 2 + 2 + 8 = 17 bytes/cell (plus next_cells copy).
 *   This is acceptable for research grids up to 256x256 (~1 MB per grid).
 *   A BitGrid fast path is NOT planned for this type; the richness of the
 *   per-cell state defeats bit-parallel compression.
 * ============================================================================ */

#include <stdint.h>
#include <math.h>
#include "genome.h"

/* --- Acoustic payload accessors --- */

#define WAVE_BITS_USED      26
#define WAVE_COORD_BITS     8
#define WAVE_EXTRA_BITS     2
#define WAVE_MASK           ((1u << WAVE_BITS_USED) - 1)

/* Coordinate extraction: 8-bit scalar from the 26-bit wave field */
#define WAVE_COORD_X(w)     (((w) >> 0) & 0xFFu)
#define WAVE_COORD_Y(w)     (((w) >> 8) & 0xFFu)
#define WAVE_COORD_Z(w)     (((w) >> 16) & 0xFFu)

/* 3-D vector as three packed 8-bit coordinates */
#define WAVE_VEC3(w, xvar, yvar, zvar) do { \
    (xvar) = (uint8_t)WAVE_COORD_X(w);      \
    (yvar) = (uint8_t)WAVE_COORD_Y(w);      \
    (zvar) = (uint8_t)WAVE_COORD_Z(w);      \
} while (0)

/* Meta / segment-select bits (e.g. octave index, waveform bank) */
#define WAVE_EXTRA(w)       (((w) >> 24) & 0x3u)

/* Bit-test for a specific acoustic segment (0-25) */
#define WAVE_SEGMENT(w, n)  (((w) >> (n)) & 1u)

/* Count active segments (population count on lower 26 bits) */
static inline int wave_popcount(uint32_t w) {
    w &= WAVE_MASK;
    /* __builtin_popcount is available in GCC/Clang */
    return __builtin_popcount(w);
}

/* Cosine similarity between two 26-bit acoustic fingerprints (0.0 to 1.0) */
static inline double wave_similarity(uint32_t a, uint32_t b) {
    uint32_t ab = a & b;
    uint32_t ua = a & WAVE_MASK;
    uint32_t ub = b & WAVE_MASK;
    int pab = __builtin_popcount(ab);
    int pa  = __builtin_popcount(ua);
    int pb  = __builtin_popcount(ub);
    if (pa == 0 && pb == 0) return 1.0;
    if (pa == 0 || pb == 0) return 0.0;
    return (double)pab / (sqrt((double)pa * (double)pb));
}

/* --- Cell and neighborhood genomes --- */

typedef CellGenome NeighborhoodGenome;

typedef struct {
    uint8_t  alive;         /* binary state */
    uint32_t wave;          /* 26-bit acoustic payload */
    CellGenome      genome; /* fast-evolving cell genome */
    NeighborhoodGenome nb_genome; /* slow-evolving neighborhood consensus */
    double   fitness;       /* selection signal */
} WaveCell;

/* --- Grid --- */

#define MAX_WAVE_GRIDS 4
#define MAX_WAVE_SIZE  128

typedef struct {
    int active;
    int size;
    int rule_idx;           /* base rule index */
    uint64_t rng_state;     /* for stochastic rules */
    WaveCell *cells;
    WaveCell *next_cells;
} WaveGrid;

WaveGrid *wave_grid_create(int size);
void wave_grid_destroy(WaveGrid *g);
void wave_grid_clear(WaveGrid *g);
void wave_grid_init_random(WaveGrid *g, uint64_t (*rng)(void));
void wave_grid_copy(const WaveGrid *src, WaveGrid *dst);

static inline WaveCell wave_grid_get(const WaveGrid *g, int x, int y) {
    return g->cells[y * g->size + x];
}

static inline void wave_grid_set(WaveGrid *g, int x, int y, WaveCell c) {
    g->cells[y * g->size + x] = c;
}

/* Recompute neighborhood genomes from cell genomes using a consensus kernel.
   Modes:
     0 = majority vote per field (rule_select, weight, mutation)
     1 = average per field (rounded)
     2 = copy from fittest cell in neighborhood */
void wave_grid_recompute_nb_genomes(WaveGrid *g, int mode);

/* Compute spatial coherence: fraction of Moore neighbors whose cell genome
   matches this cell's genome within a tolerance. Used to modulate
   cell-genome expression. */
double wave_grid_coherence(const WaveGrid *g, int x, int y);

#endif
