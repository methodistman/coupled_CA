#include "genome.h"

CellGenome genome_random(uint64_t (*rng)(void)) {
    return (CellGenome)(rng() & 0xFFFFFFFFFFFFULL); /* 48 bits */
}

CellGenome genome_mutate(CellGenome g, double prob, int mask, uint64_t (*rng)(void)) {
    if ((double)(rng() % 1000000) / 1000000.0 >= prob) return g;
    if (mask == 0) mask = GENOME_MUTATE_ALL;
    int candidates[9];
    int ncand = 0;
    if (mask & GENOME_MUTATE_RULE)          candidates[ncand++] = 0;
    if (mask & GENOME_MUTATE_WEIGHT)        candidates[ncand++] = 1;
    if (mask & GENOME_MUTATE_MUTATION)      candidates[ncand++] = 2;
    if (mask & GENOME_MUTATE_ORIENT_MODE0)  candidates[ncand++] = 3;
    if (mask & GENOME_MUTATE_ORIENT_MODE1)  candidates[ncand++] = 4;
    if (mask & GENOME_MUTATE_ORIENT_MODE2)  candidates[ncand++] = 5;
    if (mask & GENOME_MUTATE_ORIENT_MODE3)  candidates[ncand++] = 6;
    if (mask & GENOME_MUTATE_ALT_RULE)      candidates[ncand++] = 7;
    if (mask & GENOME_MUTATE_DIR_MASK)      candidates[ncand++] = 8;
    if (ncand == 0) return g;
    int field = candidates[(int)(rng() % ncand)];
    int delta = (rng() & 1) ? 1 : -1;
    switch (field) {
        case 0: { int v = (int)(GENOME_RULE_SELECT(g) + delta) & 0xFF;     GENOME_SET_RULE_SELECT(g, v); break; }
        case 1: { int v = (int)(GENOME_COUPLING_WEIGHT(g) + delta) & 0xFF;  GENOME_SET_COUPLING_WEIGHT(g, v); break; }
        case 2: { int v = (int)(GENOME_MUTATION_RATE(g) + delta) & 0xFF;    GENOME_SET_MUTATION_RATE(g, v); break; }
        case 3: { int v = (int)(GENOME_ORIENT_MODE0(g) + delta) & 0x07;    GENOME_SET_ORIENT_MODE0(g, v); break; }
        case 4: { int v = (int)(GENOME_ORIENT_MODE1(g) + delta) & 0x07;    GENOME_SET_ORIENT_MODE1(g, v); break; }
        case 5: { int v = (int)(GENOME_ORIENT_MODE2(g) + delta) & 0x07;    GENOME_SET_ORIENT_MODE2(g, v); break; }
        case 6: { int v = (int)(GENOME_ORIENT_MODE3(g) + delta) & 0x07;    GENOME_SET_ORIENT_MODE3(g, v); break; }
        case 7: { int v = (int)(GENOME_ALT_RULE_SELECT(g) + delta) & 0x0F; GENOME_SET_ALT_RULE_SELECT(g, v); break; }
        case 8: { int v = (int)(GENOME_DIR_MASK(g) + delta) & 0x0F;       GENOME_SET_DIR_MASK(g, v); break; }
    }
    return g;
}

CellGenome genome_crossover(CellGenome a, CellGenome b, uint64_t (*rng)(void)) {
    uint64_t mask = rng() & 0xFFFFFFFFFFFFULL;
    return (a & mask) | (b & ~mask);
}

CellGenome genome_crossover_1pt(CellGenome a, CellGenome b, uint64_t (*rng)(void)) {
    int pt = (int)(rng() % 49); /* 0..48 */
    uint64_t mask = (pt == 48) ? 0xFFFFFFFFFFFFULL : ((1ULL << pt) - 1);
    return (a & mask) | (b & ~mask);
}

int genome_tournament_select(const CellGenome *arr, const double *fitness, int n, int k, uint64_t (*rng)(void)) {
    if (n <= 0) return 0;
    int best = (int)(rng() % n);
    double best_f = fitness ? fitness[best] : 0.0;
    for (int i = 1; i < k; i++) {
        int idx = (int)(rng() % n);
        double f = fitness ? fitness[idx] : 0.0;
        if (f > best_f) { best = idx; best_f = f; }
    }
    return best;
}

CellGenome genome_pack(int rule, int weight, int mutation) {
    CellGenome g = 0;
    GENOME_SET_RULE_SELECT(g, rule);
    GENOME_SET_COUPLING_WEIGHT(g, weight);
    GENOME_SET_MUTATION_RATE(g, mutation);
    return g;
}

CellGenome genome_pack_oriented(int rule, int weight, int mutation, int orientation) {
    CellGenome g = genome_pack(rule, weight, mutation);
    GENOME_SET_ORIENTATION(g, orientation);
    return g;
}

CellGenome genome_pack_full(int rule, int weight, int mutation,
                            int o0, int o1, int o2, int o3,
                            int alt_rule, int dir_mask) {
    CellGenome g = 0;
    GENOME_SET_RULE_SELECT(g, rule);
    GENOME_SET_COUPLING_WEIGHT(g, weight);
    GENOME_SET_MUTATION_RATE(g, mutation);
    GENOME_SET_ORIENT_MODE0(g, o0);
    GENOME_SET_ORIENT_MODE1(g, o1);
    GENOME_SET_ORIENT_MODE2(g, o2);
    GENOME_SET_ORIENT_MODE3(g, o3);
    GENOME_SET_ALT_RULE_SELECT(g, alt_rule);
    GENOME_SET_DIR_MASK(g, dir_mask);
    return g;
}
