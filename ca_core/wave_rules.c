#include "wave_rules.h"
#include <string.h>

/* --- Rule implementations --- */

/* Conway's Life on alive field, preserving wave payload from center */
void wave_rule_life(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    int alive_count = 0;
    WaveCell center = nb[4];
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        alive_count += nb[i].alive ? 1 : 0;
    }
    int alive = center.alive ? (alive_count == 2 || alive_count == 3) : (alive_count == 3);
    out->alive = alive ? 1 : 0;
    out->wave = center.wave;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Average wave of alive neighbors, threshold alive */
void wave_rule_diffuse(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    int alive_count = 0;
    uint64_t wave_sum = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) {
            alive_count++;
            wave_sum += nb[i].wave;
        }
    }
    WaveCell center = nb[4];
    int alive = alive_count >= 2 && alive_count <= 4;
    out->alive = alive ? 1 : 0;
    if (alive_count > 0) {
        out->wave = (uint32_t)((wave_sum / (uint64_t)alive_count) & WAVE_MASK);
    } else {
        out->wave = center.wave;
    }
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Life with born/survive threshold modulated by neighborhood genome coherence.
   Coherence = fraction of neighbors with identical genome to center. */
void wave_rule_genomic_life(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    int alive_count = 0;
    WaveCell center = nb[4];
    int match_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        alive_count += nb[i].alive ? 1 : 0;
        if (nb[i].genome == center.genome) match_count++;
    }
    /* High coherence (many identical genomes) -> stricter threshold */
    float coherence = (float)match_count / 8.0f;
    int born_thresh = 3;
    if (coherence > 0.75f) born_thresh = 4;      /* very coherent patches need more support */
    else if (coherence < 0.25f) born_thresh = 2;  /* chaotic patches are more permissive */
    int alive = center.alive ? (alive_count == 2 || alive_count == 3) : (alive_count == born_thresh);
    out->alive = alive ? 1 : 0;
    out->wave = center.wave;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Bitwise AND of wave fields from alive neighbors */
void wave_rule_vector_and(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    uint32_t val = nb[4].wave;
    int alive_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) { val &= nb[i].wave; alive_count++; }
    }
    WaveCell center = nb[4];
    out->alive = (alive_count > 0) ? 1 : 0;
    out->wave  = val & WAVE_MASK;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Bitwise OR of wave fields from alive neighbors */
void wave_rule_vector_or(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    uint32_t val = nb[4].wave;
    int alive_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) { val |= nb[i].wave; alive_count++; }
    }
    WaveCell center = nb[4];
    out->alive = (alive_count > 0) ? 1 : 0;
    out->wave  = val & WAVE_MASK;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Bitwise XOR of wave fields from alive neighbors */
void wave_rule_vector_xor(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    uint32_t val = nb[4].wave;
    int alive_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) { val ^= nb[i].wave; alive_count++; }
    }
    WaveCell center = nb[4];
    out->alive = (alive_count > 0) ? 1 : 0;
    out->wave  = val & WAVE_MASK;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Alive if at least 2 alive neighbors AND average wave > 0.5*WAVE_MASK */
void wave_rule_threshold_and(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    int alive_count = 0;
    uint64_t wave_sum = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) { alive_count++; wave_sum += nb[i].wave; }
    }
    WaveCell center = nb[4];
    uint32_t avg = alive_count ? (uint32_t)(wave_sum / alive_count) : 0;
    int alive = (alive_count >= 2) && (avg > (WAVE_MASK >> 1));
    out->alive = alive ? 1 : 0;
    out->wave  = alive ? (avg & WAVE_MASK) : center.wave;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* Alive if any alive neighbor OR center wave > 0.75*WAVE_MASK */
void wave_rule_threshold_or(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state) {
    (void)rng_state;
    int alive_count = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 4) continue;
        if (nb[i].alive) alive_count++;
    }
    WaveCell center = nb[4];
    int alive = (alive_count > 0) || (center.wave > ((WAVE_MASK * 3) >> 2));
    out->alive = alive ? 1 : 0;
    out->wave  = center.wave;
    out->genome = center.genome;
    out->nb_genome = center.nb_genome;
    out->fitness = center.fitness;
}

/* --- Rule table --- */

const WaveRule WAVE_RULE_TABLE[] = {
    {"life",           wave_rule_life},
    {"diffuse",        wave_rule_diffuse},
    {"genomic_life",   wave_rule_genomic_life},
    {"vector_and",     wave_rule_vector_and},
    {"vector_or",      wave_rule_vector_or},
    {"vector_xor",     wave_rule_vector_xor},
    {"threshold_and",  wave_rule_threshold_and},
    {"threshold_or",   wave_rule_threshold_or},
};

const int NUM_WAVE_RULES = sizeof(WAVE_RULE_TABLE) / sizeof(WAVE_RULE_TABLE[0]);

const WaveRule *wave_rules_lookup(const char *name) {
    for (int i = 0; i < NUM_WAVE_RULES; i++) {
        if (strcmp(WAVE_RULE_TABLE[i].name, name) == 0)
            return &WAVE_RULE_TABLE[i];
    }
    return NULL;
}

int wave_rules_index_by_name(const char *name) {
    for (int i = 0; i < NUM_WAVE_RULES; i++) {
        if (strcmp(WAVE_RULE_TABLE[i].name, name) == 0)
            return i;
    }
    return -1;
}
