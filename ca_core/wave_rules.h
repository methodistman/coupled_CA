#ifndef WAVE_RULES_H
#define WAVE_RULES_H

#include "wave_grid.h"

typedef void (*WaveRuleFn)(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);

typedef struct {
    const char *name;
    WaveRuleFn fn;
} WaveRule;

extern const WaveRule WAVE_RULE_TABLE[];
extern const int NUM_WAVE_RULES;

/* Built-in rule implementations */
void wave_rule_life(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_diffuse(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_genomic_life(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_vector_and(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_vector_or(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_vector_xor(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_threshold_and(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
void wave_rule_threshold_or(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);

/* Lookup by name; returns NULL if not found */
const WaveRule *wave_rules_lookup(const char *name);
int wave_rules_index_by_name(const char *name);

#endif
