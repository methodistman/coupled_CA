#include "bitgrid.h"
#include <stdlib.h>
#include <string.h>

/* Precompute Life rule LUT: index = bits[0..7] are 8 neighbor bits, bit[8] = self alive */
uint8_t BITGRID_RULE_LIFE[512];
static int life_lut_initialized = 0;

static void init_life_lut(void) {
    if (life_lut_initialized) return;
    for (int idx = 0; idx < 512; idx++) {
        int alive = (idx >> 8) & 1;
        int count = 0;
        for (int b = 0; b < 8; b++) count += (idx >> b) & 1;
        BITGRID_RULE_LIFE[idx] = alive ? ((count == 2 || count == 3) ? 1 : 0) : (count == 3 ? 1 : 0);
    }
    life_lut_initialized = 1;
}

BitGrid *bitgrid_create(int size) {
    if (size <= 0 || (size & 63) != 0) return NULL; /* must be multiple of 64 */
    BitGrid *g = calloc(1, sizeof(BitGrid));
    if (!g) return NULL;
    g->size = size;
    g->words_per_row = size >> 6;
    int nwords = size * g->words_per_row;
    g->bits = calloc(nwords, sizeof(uint64_t));
    g->next_bits = calloc(nwords, sizeof(uint64_t));
    if (!g->bits || !g->next_bits) {
        bitgrid_destroy(g);
        return NULL;
    }
    init_life_lut();
    return g;
}

void bitgrid_destroy(BitGrid *g) {
    if (!g) return;
    free(g->bits);
    free(g->next_bits);
    free(g);
}

void bitgrid_clear(BitGrid *g) {
    if (!g) return;
    memset(g->bits, 0, g->size * g->words_per_row * sizeof(uint64_t));
}

void bitgrid_randomize(BitGrid *g, uint64_t (*rng)(void)) {
    if (!g) return;
    int nwords = g->size * g->words_per_row;
    for (int i = 0; i < nwords; i++)
        g->bits[i] = rng();
}

void bitgrid_copy(const BitGrid *src, BitGrid *dst) {
    if (!src || !dst) return;
    int nwords = src->size * src->words_per_row;
    memcpy(dst->bits, src->bits, nwords * sizeof(uint64_t));
}

/* Naive Moore count for correctness (can be SIMD/AVX later) */
int bitgrid_moore_count(const BitGrid *g, int x, int y) {
    int sz = g->size;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        int ny = (y + dy + sz) % sz;
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + sz) % sz;
            count += bitgrid_get(g, nx, ny);
        }
    }
    return count;
}

void bitgrid_step_lut(BitGrid *src, BitGrid *dst, const uint8_t *lut) {
    if (!src || !dst) return;
    int sz = src->size;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int nb = 0;
            int idx = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = (x + dx + sz) % sz;
                    int ny = (y + dy + sz) % sz;
                    int bit = bitgrid_get(src, nx, ny);
                    idx |= (bit << (nb++));
                }
            }
            int alive = bitgrid_get(src, x, y);
            idx |= (alive << 8);
            bitgrid_set(dst, x, y, lut[idx]);
        }
    }
    /* swap buffers so src->bits holds the new state */
    uint64_t *tmp = src->bits;
    src->bits = dst->bits;
    dst->bits = tmp;
}

/* Sum 8 neighbor bit-vectors into a 4-bit count per cell using a tree of
   half/full adders, then apply Conway's Life rule.  All operations are
   on 64 cells in parallel. */
static uint64_t life_step_word(uint64_t n_l, uint64_t n,   uint64_t n_r,
                                uint64_t m_l, uint64_t m,   uint64_t m_r,
                                uint64_t s_l, uint64_t s,   uint64_t s_r)
{
    uint64_t v[8] = {n_l, n, n_r, m_l, m_r, s_l, s, s_r};

    /* Level 1: 4 half adders → 4 two-bit numbers (sum bit, carry bit) */
    uint64_t s1[4], c1[4];
    for (int i = 0; i < 4; i++) {
        s1[i] = v[2*i] ^ v[2*i+1];
        c1[i] = v[2*i] & v[2*i+1];
    }

    /* Level 2: Add pairs of 2-bit numbers → 2 three-bit numbers */
    /* (s1[0],c1[0]) + (s1[1],c1[1]) */
    uint64_t b0_01 = s1[0] ^ s1[1];
    uint64_t carry_01 = s1[0] & s1[1];
    uint64_t b1_01 = c1[0] ^ c1[1] ^ carry_01;
    uint64_t b2_01 = (c1[0] & c1[1]) | (carry_01 & (c1[0] ^ c1[1]));

    /* (s1[2],c1[2]) + (s1[3],c1[3]) */
    uint64_t b0_23 = s1[2] ^ s1[3];
    uint64_t carry_23 = s1[2] & s1[3];
    uint64_t b1_23 = c1[2] ^ c1[3] ^ carry_23;
    uint64_t b2_23 = (c1[2] & c1[3]) | (carry_23 & (c1[2] ^ c1[3]));

    /* Level 3: Add two 3-bit numbers → 4-bit count */
    uint64_t cnt0 = b0_01 ^ b0_23;
    uint64_t carry_0123 = b0_01 & b0_23;
    uint64_t cnt1 = b1_01 ^ b1_23 ^ carry_0123;
    uint64_t carry_1 = (b1_01 & b1_23) | (carry_0123 & (b1_01 ^ b1_23));
    uint64_t cnt2 = b2_01 ^ b2_23 ^ carry_1;
    uint64_t cnt3 = (b2_01 & b2_23) | (carry_1 & (b2_01 ^ b2_23));

    /* Life rule: alive if count==3, or (alive and count==2) */
    uint64_t count_eq_3 = ~cnt3 & ~cnt2 & cnt1 & cnt0;
    uint64_t count_eq_2 = ~cnt3 & ~cnt2 & cnt1 & ~cnt0;
    return count_eq_3 | (m & count_eq_2);
}

void bitgrid_step_life(BitGrid *g) {
    if (!g) return;
    int sz = g->size;
    int wpr = g->words_per_row;

    for (int y = 0; y < sz; y++) {
        int py = (y - 1 + sz) % sz;
        int ny = (y + 1) % sz;
        for (int w = 0; w < wpr; w++) {
            uint64_t *row  = &g->bits[y  * wpr];
            uint64_t *prow = &g->bits[py * wpr];
            uint64_t *nrow = &g->bits[ny * wpr];

            uint64_t n = prow[w];
            uint64_t m = row[w];
            uint64_t s = nrow[w];

            /* Cross-word horizontal shifts with toroidal wrap */
            uint64_t prev_n = prow[(w - 1 + wpr) % wpr];
            uint64_t next_n = prow[(w + 1) % wpr];
            uint64_t n_l = (n << 1) | (prev_n >> 63);
            uint64_t n_r = (n >> 1) | (next_n << 63);

            uint64_t prev_m = row[(w - 1 + wpr) % wpr];
            uint64_t next_m = row[(w + 1) % wpr];
            uint64_t m_l = (m << 1) | (prev_m >> 63);
            uint64_t m_r = (m >> 1) | (next_m << 63);

            uint64_t prev_s = nrow[(w - 1 + wpr) % wpr];
            uint64_t next_s = nrow[(w + 1) % wpr];
            uint64_t s_l = (s << 1) | (prev_s >> 63);
            uint64_t s_r = (s >> 1) | (next_s << 63);

            g->next_bits[y * wpr + w] = life_step_word(
                n_l, n, n_r, m_l, m, m_r, s_l, s, s_r);
        }
    }

    /* Swap buffers */
    uint64_t *tmp = g->bits;
    g->bits = g->next_bits;
    g->next_bits = tmp;
}

uint64_t bitgrid_hash(const BitGrid *g) {
    if (!g) return 0;
    uint64_t h = 14695981039346656037ULL; /* FNV offset basis */
    int nwords = g->size * g->words_per_row;
    for (int i = 0; i < nwords; i++) {
        h ^= g->bits[i];
        h *= 1099511628211ULL;
    }
    return h;
}
