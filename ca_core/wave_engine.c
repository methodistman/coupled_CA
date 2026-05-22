#include "wave_engine.h"
#include "wave_local_ga.h"
#include "genome.h"
#include <string.h>

/* Slow-layer hook: recompute neighborhood consensus, then optionally mutate it.
   Called from wave_sys_step / wave_sys_step_intent at multiples of
   nb_genome_interval. Mutation is skipped when rng_fn or mutate_mask is zero. */
static void wave_sys_slow_layer(WaveSystem *s) {
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        wave_grid_recompute_nb_genomes(grid, s->consensus_mode);
        if (s->rng_fn && s->mutate_mask != 0 && s->nb_mutation_rate > 0.0) {
            wave_local_ga_step_nb(grid, s->nb_mutation_rate,
                                  s->mutate_mask, s->rng_fn);
        }
    }
}

void wave_sys_init(WaveSystem *s, int num_grids, int grid_size) {
    memset(s, 0, sizeof(*s));
    s->num_grids = num_grids;
    s->nb_genome_interval = 10;
    s->tick_counter = 0;
    s->consensus_mode = 0;
    s->nb_mutation_rate = 0.0;
    s->mutate_mask = 0;
    s->rng_fn = NULL;
    wave_coupling_init(&s->coupling, num_grids);
    for (int g = 0; g < num_grids; g++) {
        s->grids[g] = wave_grid_create(grid_size);
        if (!s->grids[g]) {
            for (int j = g - 1; j >= 0; j--) wave_grid_destroy(s->grids[j]);
            s->num_grids = 0;
            return;
        }
    }
}

void wave_sys_destroy(WaveSystem *s) {
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            wave_grid_destroy(s->grids[g]);
            s->grids[g] = NULL;
        }
    }
    if (s->intent_buf) {
        wave_intent_buffer_destroy(s->intent_buf);
        s->intent_buf = NULL;
    }
    if (s->intent_ring) {
        wave_intent_ring_destroy(s->intent_ring);
        s->intent_ring = NULL;
    }
    s->num_grids = 0;
}

void wave_sys_step(WaveSystem *s) {
    if (!s || s->num_grids < 1) return;

    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_WAVE_RULES) continue;
        const WaveRule *rule = &WAVE_RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                WaveCell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = wave_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }

    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(WaveCell));
    }

    s->tick_counter++;
    if (s->nb_genome_interval > 0 && s->tick_counter % s->nb_genome_interval == 0) {
        wave_sys_slow_layer(s);
    }
}

/* Genomic variant: per-cell rule select from genome, with fallback to grid->rule_idx */
void wave_sys_step_genomic(WaveSystem *s) {
    if (!s || s->num_grids < 1) return;

    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        int default_rule_idx = grid->rule_idx;
        if (default_rule_idx < 0 || default_rule_idx >= NUM_WAVE_RULES) continue;
        const WaveRule *default_rule = &WAVE_RULE_TABLE[default_rule_idx];
        int has_genomes = (grid->cells[0].genome != 0); /* heuristic: check if genomes ever set */
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                WaveCell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = wave_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                const WaveRule *rule = default_rule;
                CellGenome cg = grid->cells[y * sz + x].genome;
                if (has_genomes || cg != 0) {
                    int ri = (int)GENOME_RULE_SELECT(cg);
                    rule = &WAVE_RULE_TABLE[((ri % NUM_WAVE_RULES) + NUM_WAVE_RULES) % NUM_WAVE_RULES];
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }

    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(WaveCell));
    }

    s->tick_counter++;
    if (s->nb_genome_interval > 0 && s->tick_counter % s->nb_genome_interval == 0) {
        wave_sys_slow_layer(s);
    }
}

void wave_sys_step_intent(WaveSystem *s, IntentMode mode, float alpha) {
    if (!s || s->num_grids < 1) return;

    if (!s->intent_buf) {
        int max_edge = 0;
        for (int g = 0; g < s->num_grids; g++) {
            if (s->grids[g] && s->grids[g]->size > max_edge)
                max_edge = s->grids[g]->size;
        }
        s->intent_buf = wave_intent_buffer_create(s->num_grids * 4, max_edge);
    }

    /* 1. Compute next_cells locally */
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_WAVE_RULES) continue;
        const WaveRule *rule = &WAVE_RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                WaveCell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = wave_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }

    /* 2. Apply captured intents from previous tick to next_cells edges */
    if (s->intent_buf) {
        for (int i = 0; i < s->intent_buf->count; i++) {
            WaveEdgeIntent *ei = &s->intent_buf->items[i];
            WaveGrid *dst = s->grids[ei->dst_grid];
            if (dst && dst->active) {
                wave_intent_apply_to_next(ei, dst, mode, alpha);
            }
        }
    }

    /* 3. Swap all grids */
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(WaveCell));
    }

    /* 4. Capture new intents for next tick */
    if (s->intent_buf) {
        wave_intent_buffer_capture_system(s, s->intent_buf);
    }

    s->tick_counter++;
    if (s->nb_genome_interval > 0 && s->tick_counter % s->nb_genome_interval == 0) {
        wave_sys_slow_layer(s);
    }
}

void wave_sys_randomize(WaveSystem *s, uint64_t (*rng)(void)) {
    if (!s || !rng) return;
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            wave_grid_init_random(s->grids[g], rng);
            s->grids[g]->rng_state = rng();
        }
    }
}

void wave_sys_recompute_nb(WaveSystem *s, int mode) {
    if (!s) return;
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (grid && grid->active) wave_grid_recompute_nb_genomes(grid, mode);
    }
}
