#include "wave_grid.h"
#include <stdlib.h>
#include <string.h>

WaveGrid *wave_grid_create(int size) {
    WaveGrid *g = calloc(1, sizeof(WaveGrid));
    if (!g) return NULL;
    g->active = 1;
    g->size = size;
    g->rule_idx = 0;
    int n = size * size;
    g->cells = calloc(n, sizeof(WaveCell));
    g->next_cells = calloc(n, sizeof(WaveCell));
    if (!g->cells || !g->next_cells) {
        wave_grid_destroy(g);
        return NULL;
    }
    return g;
}

void wave_grid_destroy(WaveGrid *g) {
    if (!g) return;
    free(g->cells);
    free(g->next_cells);
    free(g);
}

void wave_grid_clear(WaveGrid *g) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    memset(g->cells, 0, n * sizeof(WaveCell));
}

void wave_grid_init_random(WaveGrid *g, uint64_t (*rng)(void)) {
    if (!g || !g->active || !rng) return;
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) {
        g->cells[i].alive = (rng() % 5 == 0) ? 1 : 0;
        g->cells[i].wave = (uint32_t)(rng() & WAVE_MASK);
        g->cells[i].genome = genome_random(rng);
        g->cells[i].nb_genome = g->cells[i].genome;
        g->cells[i].fitness = 0.0;
    }
    g->rng_state = rng();
}

void wave_grid_copy(const WaveGrid *src, WaveGrid *dst) {
    if (!src || !dst || !src->active || !dst->active) return;
    if (src->size != dst->size) return;
    int n = src->size * src->size;
    memcpy(dst->cells, src->cells, n * sizeof(WaveCell));
}

/* --- Neighborhood genome consensus kernels --- */

static int moore_neighbor_idx(int sz, int x, int y, int dx, int dy) {
    int nx = (x + dx + sz) % sz;
    int ny = (y + dy + sz) % sz;
    return ny * sz + nx;
}

/* Mode 0: majority vote per field */
static CellGenome consensus_vote(const WaveGrid *g, int x, int y) {
    int sz = g->size;
    int freq_rule[32] = {0};
    int freq_wt[32]   = {0};
    int freq_mut[32]  = {0};

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            CellGenome ng = g->cells[ni].genome;
            freq_rule[GENOME_RULE_SELECT(ng)]++;
            freq_wt[GENOME_COUPLING_WEIGHT(ng)]++;
            freq_mut[GENOME_MUTATION_RATE(ng)]++;
        }
    }

    int best_rule = 0, best_rule_cnt = freq_rule[0];
    int best_wt   = 0, best_wt_cnt   = freq_wt[0];
    int best_mut  = 0, best_mut_cnt  = freq_mut[0];

    for (int v = 1; v < 32; v++) {
        if (freq_rule[v] > best_rule_cnt) { best_rule = v; best_rule_cnt = freq_rule[v]; }
        if (freq_wt[v]   > best_wt_cnt)   { best_wt   = v; best_wt_cnt   = freq_wt[v]; }
        if (freq_mut[v]  > best_mut_cnt)  { best_mut  = v; best_mut_cnt  = freq_mut[v]; }
    }

    return genome_pack(best_rule, best_wt, best_mut);
}

/* Mode 1: average per field (rounded) */
static CellGenome consensus_avg(const WaveGrid *g, int x, int y) {
    int sz = g->size;
    int sum_rule = 0, sum_wt = 0, sum_mut = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            CellGenome ng = g->cells[ni].genome;
            sum_rule += GENOME_RULE_SELECT(ng);
            sum_wt   += GENOME_COUPLING_WEIGHT(ng);
            sum_mut  += GENOME_MUTATION_RATE(ng);
        }
    }
    uint8_t avg_rule = (uint8_t)((sum_rule + 4) / 8);
    uint8_t avg_wt   = (uint8_t)((sum_wt   + 4) / 8);
    uint8_t avg_mut  = (uint8_t)((sum_mut  + 4) / 8);
    return genome_pack(avg_rule, avg_wt, avg_mut);
}

/* Mode 2: copy from fittest neighbor */
static CellGenome consensus_fittest(const WaveGrid *g, int x, int y) {
    int sz = g->size;
    int idx = y * sz + x;
    int best_idx = idx;
    double best_f = g->cells[idx].fitness;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            double f = g->cells[ni].fitness;
            if (f > best_f) { best_idx = ni; best_f = f; }
        }
    }
    return g->cells[best_idx].genome;
}

void wave_grid_recompute_nb_genomes(WaveGrid *g, int mode) {
    if (!g || !g->active) return;
    int sz = g->size;
    int n = sz * sz;
    /* Write into next_cells.nb_genome as scratch, then copy back */
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int idx = y * sz + x;
            CellGenome nb;
            switch (mode) {
                case 0: nb = consensus_vote(g, x, y); break;
                case 1: nb = consensus_avg(g, x, y);  break;
                case 2: nb = consensus_fittest(g, x, y); break;
                default: nb = g->cells[idx].genome; break;
            }
            g->next_cells[idx].nb_genome = nb;
        }
    }
    /* Copy nb_genome from next_cells back to cells */
    for (int i = 0; i < n; i++) {
        g->cells[i].nb_genome = g->next_cells[i].nb_genome;
    }
}

/* --- Spatial coherence --- */

static inline int genome_distance(CellGenome a, CellGenome b) {
    int d = 0;
    if (GENOME_RULE_SELECT(a) != GENOME_RULE_SELECT(b)) d++;
    if (GENOME_COUPLING_WEIGHT(a) != GENOME_COUPLING_WEIGHT(b)) d++;
    if (GENOME_MUTATION_RATE(a) != GENOME_MUTATION_RATE(b)) d++;
    return d;
}

double wave_grid_coherence(const WaveGrid *g, int x, int y) {
    int sz = g->size;
    int idx = y * sz + x;
    CellGenome me = g->cells[idx].genome;
    int total_dist = 0;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            total_dist += genome_distance(me, g->cells[ni].genome);
            count++;
        }
    }
    if (count == 0) return 1.0;
    /* max distance per neighbor = 3, max total = 24 */
    return 1.0 - ((double)total_dist / 24.0);
}
