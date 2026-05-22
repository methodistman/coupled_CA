/* composable_ruleset.h — 6-layer composable CA ruleset DNA.
 *
 * A 56-bit ruleset DNA (packed in a uint64_t, upper 8 bits unused) parameterizes
 * an outer-totalistic-style step with anisotropic gating, refractory dynamics,
 * and noise. The 6 layers (+ 1 meta byte) are:
 *
 *   p[0]  birth_mask     — bit k set => birth when neighbor count == k
 *   p[1]  survive_mask   — bit k set => alive cell survives at count == k
 *   p[2]  aniso_strength — 0..255 blends raw count <-> orientation-gated count
 *   p[3]  card_bias      — 0..255 weights cardinal vs diagonal neighbors
 *                          (255 = cardinal-only, 0 = diagonal-only, 128 = neutral)
 *   p[4]  refractory     — high 3 bits = ticks of post-death birth suppression (0..7)
 *   p[5]  noise          — random-flip probability = p[5] / 65535 per cell per tick
 *   p[6]  meta           — high 4 bits = layer enable mask (bits 4..7 control layers 2..5);
 *                          low 4 bits = global bias added/subtracted from count threshold
 *
 * See COMPOSABLE_RULESET_ROADMAP.md for the full design.
 */
#ifndef COMPOSABLE_RULESET_H
#define COMPOSABLE_RULESET_H

#include <stdint.h>
#include "grid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t p[7]; /* p[0..5] = layer params, p[6] = meta */
} Ruleset;

/* Pack/unpack to a 56-bit value for genome storage / serialization. */
static inline uint64_t ruleset_pack(const Ruleset *r) {
    uint64_t v = 0;
    for (int i = 0; i < 7; i++) v |= ((uint64_t)r->p[i]) << (i * 8);
    return v;
}

static inline Ruleset ruleset_unpack(uint64_t v) {
    Ruleset r;
    for (int i = 0; i < 7; i++) r.p[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
    return r;
}

/* Predefined rulesets — useful as baselines and for tests. */
Ruleset ruleset_conway(void);             /* B3/S23, isotropic */
Ruleset ruleset_life_without_death(void); /* B3/S012345678, isotropic */
Ruleset ruleset_propagation_seed(void);   /* permissive, refractory, weak aniso */

/* Random initialization for GA. */
Ruleset ruleset_random(uint64_t (*rng)(void));

/* GA operators. */
Ruleset ruleset_mutate(Ruleset r, double per_byte_prob, uint64_t (*rng)(void));
Ruleset ruleset_crossover(Ruleset a, Ruleset b, uint64_t (*rng)(void));

/* Step a single cell. Returns next-state (0 or 1).
 *
 * Inputs:
 *   r          ruleset
 *   alive      current cell state (0/1)
 *   nb_alive   neighbor states in canonical 8-direction order:
 *              index 0=E,1=NE,2=N,3=NW,4=W,5=SW,6=S,7=SE
 *   orient     current cell's orientation for the active mode (0..7)
 *   refr_age   current refractory age (will be decremented externally)
 *   rng_state  pointer to splitmix64 state for noise layer
 *
 * Side-channel outputs:
 *   *new_refr_age receives the new refractory age (caller should write into buffer).
 */
int ruleset_step_cell(const Ruleset *r,
                      int alive,
                      const uint8_t nb_alive[8],
                      int orient,
                      uint8_t refr_age,
                      uint64_t *rng_state,
                      uint8_t *new_refr_age);

/* Step an entire grid using the composable ruleset for the given mode.
 * Reads orientations from g->genomes[i] (the mode's orientation field).
 * Writes next-states into g->next_cells and swaps in-place.
 *
 * refractory_buf: optional uint8_t[size*size] side-buffer; pass NULL to disable
 *                 refractory tracking (layer 4 still reads 0 = no suppression).
 */
void ruleset_step_grid(const Ruleset *r,
                       Grid *g,
                       int mode,
                       uint8_t *refractory_buf);

/* Convenience: pretty-print a ruleset to stderr (1 line). */
void ruleset_print(const Ruleset *r);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSABLE_RULESET_H */
