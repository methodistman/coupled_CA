/* composable_ruleset.c — see composable_ruleset.h and COMPOSABLE_RULESET_ROADMAP.md */
#include "composable_ruleset.h"
#include "coupling.h"
#include "../utils/rng.h"
#include <stdio.h>
#include <string.h>

/* Anisotropic alignment table, identical to ALIGNMENT_SCALE in pipeline.c.
 * Orientation/neighbor-dir encoding: 0=E,1=NE,2=N,3=NW,4=W,5=SW,6=S,7=SE.
 * scale 16 = aligned, 8 = 45° off, 0 = perpendicular. */
static const int ALIGN_SCALE[8][8] = {
    {16, 8, 0, 8, 16, 8, 0, 8},
    {8, 16, 8, 0, 8, 16, 8, 0},
    {0, 8, 16, 8, 0, 8, 16, 8},
    {8, 0, 8, 16, 8, 0, 8, 16},
    {16, 8, 0, 8, 16, 8, 0, 8},
    {8, 16, 8, 0, 8, 16, 8, 0},
    {0, 8, 16, 8, 0, 8, 16, 8},
    {8, 0, 8, 16, 8, 0, 8, 16},
};

/* Mapping from (dx,dy) neighbor offset to direction index 0..7.
 * Index ordering matches the kernel's traversal: we'll build a small
 * lookup keyed by (dy+1)*3 + (dx+1) (skipping center). */
static const int DIR_FROM_DXDY[9] = {
    /* dy=-1: dx=-1,0,+1  => NW,N,NE */
    3, 2, 1,
    /* dy= 0: dx=-1,0,+1  => W, -, E */
    4, -1, 0,
    /* dy=+1: dx=-1,0,+1  => SW, S, SE */
    5, 6, 7,
};

static inline int is_cardinal(int dir) {
    return dir == 0 || dir == 2 || dir == 4 || dir == 6;
}

/* ===== Predefined rulesets ===== */

Ruleset ruleset_conway(void) {
    Ruleset r = {0};
    r.p[0] = 0x08;   /* birth at count==3 */
    r.p[1] = 0x0C;   /* survive at 2,3 */
    r.p[2] = 0;      /* isotropic */
    r.p[3] = 128;    /* neutral card/diag */
    r.p[4] = 0;      /* no refractory */
    r.p[5] = 0;      /* no noise */
    r.p[6] = 0xF8;   /* meta: all layers enabled (top 4 bits), threshold-bias=8/neutral */
    return r;
}

Ruleset ruleset_life_without_death(void) {
    Ruleset r = ruleset_conway();
    r.p[1] = 0xFF;   /* survive at any count */
    return r;
}

Ruleset ruleset_propagation_seed(void) {
    /* A hand-tuned starting point for Stage-0 GA:
     * permissive birth (any 1-3 neighbors), persistent (LWD-like survival),
     * partial anisotropy, short refractory, no noise. */
    Ruleset r = {0};
    r.p[0] = 0x0E;   /* B1, B2, B3 */
    r.p[1] = 0xFF;   /* survive always */
    r.p[2] = 96;     /* ~38% anisotropy */
    r.p[3] = 128;    /* neutral card/diag */
    r.p[4] = (uint8_t)(1 << 5); /* 1 tick refractory */
    r.p[5] = 0;      /* no noise */
    r.p[6] = 0xF8;
    return r;
}

Ruleset ruleset_random(uint64_t (*rng)(void)) {
    Ruleset r;
    for (int i = 0; i < 7; i++) r.p[i] = (uint8_t)(rng() & 0xFF);
    /* Ensure birth has at least one bit set (otherwise nothing ever propagates). */
    if (r.p[0] == 0) r.p[0] = (uint8_t)(1u << ((rng() & 7)));
    /* Force layer-enable mask on for first build (top 4 bits of meta = 0xF0). */
    r.p[6] = (uint8_t)((r.p[6] & 0x0F) | 0xF0);
    return r;
}

/* ===== GA operators ===== */

Ruleset ruleset_mutate(Ruleset r, double per_byte_prob, uint64_t (*rng)(void)) {
    for (int i = 0; i < 7; i++) {
        /* Use 16 bits of entropy per byte for the probability draw. */
        double u = (double)(rng() & 0xFFFF) / 65536.0;
        if (u < per_byte_prob) {
            /* Two mutation modes: 50% bit-flip, 50% Gaussian-ish delta. */
            if ((rng() & 1) == 0) {
                int bit = (int)(rng() & 7);
                r.p[i] ^= (uint8_t)(1u << bit);
            } else {
                int delta = (int)((rng() & 0x1F)) - 16; /* -16..+15 */
                int v = (int)r.p[i] + delta;
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                r.p[i] = (uint8_t)v;
            }
        }
    }
    /* Preserve invariants: at least one birth bit, layer mask on. */
    if (r.p[0] == 0) r.p[0] = 0x08;
    r.p[6] = (uint8_t)((r.p[6] & 0x0F) | 0xF0);
    return r;
}

Ruleset ruleset_crossover(Ruleset a, Ruleset b, uint64_t (*rng)(void)) {
    Ruleset c;
    for (int i = 0; i < 7; i++) {
        c.p[i] = ((rng() & 1) == 0) ? a.p[i] : b.p[i];
    }
    if (c.p[0] == 0) c.p[0] = a.p[0] ? a.p[0] : b.p[0];
    c.p[6] = (uint8_t)((c.p[6] & 0x0F) | 0xF0);
    return c;
}

/* ===== Step function ===== */

int ruleset_step_cell(const Ruleset *r,
                      int alive,
                      const uint8_t nb_alive[8],
                      int orient,
                      uint8_t refr_age,
                      uint64_t *rng_state,
                      uint8_t *new_refr_age) {
    uint8_t birth_mask   = r->p[0];
    uint8_t survive_mask = r->p[1];
    int aniso_strength   = r->p[2];           /* 0..255 */
    int card_bias        = r->p[3];           /* 0..255; 128=neutral */
    int refr_ticks       = (r->p[4] >> 5);    /* 0..7 */
    int noise_thresh     = r->p[5];           /* prob = p5/65535 */
    uint8_t meta         = r->p[6];
    int layer_aniso_on  = (meta & 0x10) != 0;
    int layer_bias_on   = (meta & 0x20) != 0;
    int layer_refr_on   = (meta & 0x40) != 0;
    int layer_noise_on  = (meta & 0x80) != 0;
    int thresh_bias     = (int)(meta & 0x0F) - 8;

    /* Layer 5: refractory — if still cooling down, force dead, count down. */
    if (layer_refr_on && refr_age > 0) {
        *new_refr_age = (uint8_t)(refr_age - 1);
        return 0;
    }

    /* Compute weighted neighbor count.
     * Two parallel sums:
     *   raw_count       — plain alive count (×16 to share fixed-point scale)
     *   weighted_count  — alive count modulated by ALIGN_SCALE[orient][dir]
     *                     and card/diag bias.
     * Then blend by aniso_strength.
     */
    int raw16 = 0;
    int weighted = 0;
    int weight_total = 0;
    for (int dir = 0; dir < 8; dir++) {
        int a = nb_alive[dir] ? 1 : 0;
        raw16 += a * 16;
        int scale = ALIGN_SCALE[orient & 7][dir];
        /* Layer 2: cardinal/diagonal bias. card_bias 128 = neutral. */
        if (layer_bias_on) {
            int card_w = card_bias;          /* 0..255 */
            int diag_w = 255 - card_bias;
            int w = is_cardinal(dir) ? card_w : diag_w;
            /* Normalize so the average weight is ~128 (neutral when card_bias=128). */
            scale = (scale * w) / 128;
        }
        weighted += a * scale;
        weight_total += scale;
    }
    /* Renormalize weighted to be comparable to raw16 (sum-of-weights ~= 8*16=128 in neutral). */
    int weighted16 = weight_total > 0 ? (weighted * 8 * 16) / weight_total : 0;

    /* Blend raw <-> weighted by aniso_strength. */
    int blended16;
    if (layer_aniso_on) {
        blended16 = (raw16 * (255 - aniso_strength) + weighted16 * aniso_strength) / 255;
    } else {
        blended16 = raw16;
    }
    /* Convert to integer neighbor count in [0..8] with bias offset. */
    int count = (blended16 + 8) / 16; /* round to nearest */
    count += thresh_bias;
    if (count < 0) count = 0;
    if (count > 8) count = 8;

    /* Birth / survive lookup. Bit 8 is treated as bit 7 (count==8 maps to bit 7
     * if mask bit 7 is set; we use saturation rather than wrap-around). */
    int idx = (count >= 8) ? 7 : count;
    int born    = (birth_mask  >> idx) & 1;
    int survive = (survive_mask >> idx) & 1;
    int next = alive ? survive : born;

    /* Layer 6: noise. */
    if (layer_noise_on && noise_thresh > 0) {
        uint64_t z = rng_splitmix64(rng_state);
        int r16 = (int)(z & 0xFFFF);
        if (r16 < noise_thresh) next ^= 1;
    }

    /* Update refractory age: if we just died, set refractory; else clamp to 0. */
    if (layer_refr_on && alive && !next) {
        *new_refr_age = (uint8_t)refr_ticks;
    } else {
        *new_refr_age = 0;
    }

    return next;
}

/* ===== Grid-level step ===== */

void ruleset_step_grid(const Ruleset *r,
                       Grid *g,
                       int mode,
                       uint8_t *refractory_buf) {
    if (!g || !g->active) return;
    int sz = g->size;
    uint8_t nb[8];

    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int idx = y * sz + x;
            int alive = g->cells[idx];

            /* Read 8 neighbors using DIR_FROM_DXDY ordering. */
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    int dir = DIR_FROM_DXDY[(dy + 1) * 3 + (dx + 1)];
                    int nx = x + dx, ny = y + dy;
                    uint8_t v = 0;
                    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                        v = g->cells[ny * sz + nx];
                    }
                    nb[dir] = v;
                }
            }

            /* Orientation for this cell and mode. */
            int orient = 0;
            if (g->genomes) {
                CellGenome cg = g->genomes[idx];
                switch (mode) {
                    case 0: orient = (int)GENOME_ORIENT_MODE0(cg); break;
                    case 1: orient = (int)GENOME_ORIENT_MODE1(cg); break;
                    case 2: orient = (int)GENOME_ORIENT_MODE2(cg); break;
                    case 3: orient = (int)GENOME_ORIENT_MODE3(cg); break;
                }
            }

            uint8_t refr_age = refractory_buf ? refractory_buf[idx] : 0;
            uint8_t new_refr = 0;
            int next = ruleset_step_cell(r, alive, nb, orient, refr_age,
                                         &g->rng_state, &new_refr);
            g->next_cells[idx] = (uint8_t)next;
            if (refractory_buf) refractory_buf[idx] = new_refr;
        }
    }

    memcpy(g->cells, g->next_cells, sz * sz * sizeof(uint8_t));
}

void ruleset_print(const Ruleset *r) {
    fprintf(stderr,
        "Ruleset[B=0x%02X S=0x%02X aniso=%u card=%u refr=%u noise=%u meta=0x%02X]\n",
        r->p[0], r->p[1], r->p[2], r->p[3], r->p[4] >> 5, r->p[5], r->p[6]);
}
