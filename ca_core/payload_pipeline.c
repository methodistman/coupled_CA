#include "payload_pipeline.h"
#include "payload_intent.h"
#include "payload_coupling.h"
#include "schedule.h"
#include "genome.h"
#include "../utils/rng.h"
#include <stdlib.h>
#include <string.h>

PayloadPipeline *payload_pipeline_create(const char *name) {
    PayloadPipeline *p = calloc(1, sizeof(PayloadPipeline));
    if (p) p->name = name ? name : "unnamed";
    return p;
}

void payload_pipeline_destroy(PayloadPipeline *p) {
    if (!p) return;
    /* Free userdata for known phase types */
    for (int i = 0; i < p->n_phases; i++) {
        PayloadPhase *ph = &p->phases[i];
        if (!ph->userdata) continue;
        if (ph->type == PHASE_EXCHANGE) {
            free(ph->userdata);
        } else if (ph->type == PHASE_MUTATE) {
            free(ph->userdata);
        } else if (ph->type == PHASE_ANALYZE && ph->fn == payload_phase_analyze_local_fitness) {
            payload_local_fitness_config_destroy((PayloadLocalFitnessConfig *)ph->userdata);
        }
    }
    free(p);
}

void payload_pipeline_reset(PayloadPipeline *p) {
    if (p) { p->n_phases = 0; p->tick_count = 0; }
}

int payload_pipeline_add_phase(PayloadPipeline *p, PhaseType type, const char *name,
                                 PayloadPhaseFn fn, void *userdata, int target_grid) {
    if (!p || p->n_phases >= PAYLOAD_PIPELINE_MAX_PHASES) return -1;
    PayloadPhase *ph = &p->phases[p->n_phases];
    ph->type = type; ph->name = name ? name : "phase";
    ph->fn = fn; ph->userdata = userdata; ph->target_grid = target_grid;
    ph->ndeps = 0;
    return p->n_phases++;
}

void payload_pipeline_add_dep(PayloadPipeline *p, int dependent, int prereq) {
    if (!p || dependent < 0 || dependent >= p->n_phases) return;
    if (prereq < 0 || prereq >= p->n_phases) return;
    PayloadPhase *ph = &p->phases[dependent];
    if (ph->ndeps < PAYLOAD_PIPELINE_MAX_DEPS) ph->deps[ph->ndeps++] = prereq;
}

void payload_pipeline_execute(PayloadPipeline *p, PayloadSystem *sys) {
    if (!p || !sys || p->n_phases == 0) return;
    int n = p->n_phases;
    int *done = calloc(n, sizeof(int));
    if (!done) return;
    int completed = 0;
    while (completed < n) {
        int progressed = 0;
        for (int i = 0; i < n; i++) {
            if (done[i]) continue;
            int ready = 1;
            for (int d = 0; d < p->phases[i].ndeps; d++) {
                if (!done[p->phases[i].deps[d]]) { ready = 0; break; }
            }
            if (ready) {
                p->phases[i].fn(sys, &p->phases[i], p->phases[i].userdata);
                done[i] = 1;
                completed++;
                progressed = 1;
            }
        }
        if (!progressed) break;
    }
    free(done);
    p->tick_count++;
}

/* ============================================================
 * Phase implementations
 * ============================================================ */

/* PHASE_RUN: compute next_cells for all (or one) grid, then swap in-place
   (matches legacy payload_sys_step semantics). */
void payload_phase_run(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    int do_swap = 1;
    /* If an EXCHANGE phase will run, it handles the swap. We detect this by
       the convention: target_grid == -2 means "no swap" (used by intent presets). */
    if (phase->target_grid == -2) do_swap = 0;

    for (int g = 0; g < sys->num_grids; g++) {
        if (phase->target_grid >= 0 && phase->target_grid != g) continue;
        PayloadGrid *grid = sys->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_PAYLOAD_RULES) continue;
        const PayloadRule *rule = &PAYLOAD_RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                Cell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = payload_coupling_read(&sys->coupling, sys->grids, g, x, y, dx, dy);
                    }
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
        if (do_swap) {
            memcpy(grid->cells, grid->next_cells, sz * sz * sizeof(Cell));
        }
    }
}

/* PHASE_RUN (genomic variant): per-cell rule select from genome, with fallback to grid->rule_idx */
void payload_phase_run_genomic(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    int do_swap = 1;
    if (phase->target_grid == -2) do_swap = 0;

    for (int g = 0; g < sys->num_grids; g++) {
        if (phase->target_grid >= 0 && phase->target_grid != g) continue;
        PayloadGrid *grid = sys->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        int default_rule_idx = grid->rule_idx;
        if (default_rule_idx < 0 || default_rule_idx >= NUM_PAYLOAD_RULES) continue;
        const PayloadRule *default_rule = &PAYLOAD_RULE_TABLE[default_rule_idx];
        int has_genomes = (grid->genomes != NULL);
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                Cell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = payload_coupling_read(&sys->coupling, sys->grids, g, x, y, dx, dy);
                    }
                }
                const PayloadRule *rule = default_rule;
                if (has_genomes) {
                    int ri = (int)GENOME_RULE_SELECT(grid->genomes[y * sz + x]);
                    rule = &PAYLOAD_RULE_TABLE[((ri % NUM_PAYLOAD_RULES) + NUM_PAYLOAD_RULES) % NUM_PAYLOAD_RULES];
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
        if (do_swap) {
            memcpy(grid->cells, grid->next_cells, sz * sz * sizeof(Cell));
        }
    }
}

/* PHASE_EXCHANGE: apply previous intents to next_cells, swap, capture new intents. */
void payload_phase_exchange_intent(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)phase;
    IntentMode *mode_ptr = (IntentMode *)userdata;
    if (!sys || !mode_ptr) return;
    if (!sys->intent_buf) {
        int max_edge = 0;
        for (int g = 0; g < sys->num_grids; g++) {
            if (sys->grids[g] && sys->grids[g]->size > max_edge)
                max_edge = sys->grids[g]->size;
        }
        sys->intent_buf = payload_intent_buffer_create(sys->num_grids * 4, max_edge);
    }
    for (int i = 0; i < sys->intent_buf->count; i++) {
        PayloadEdgeIntent *ei = &sys->intent_buf->items[i];
        PayloadGrid *dst = sys->grids[ei->dst_grid];
        if (dst && dst->active) payload_intent_apply_to_next(ei, dst, *mode_ptr, 0.0f);
    }
    for (int g = 0; g < sys->num_grids; g++) {
        PayloadGrid *gptr = sys->grids[g];
        if (gptr && gptr->active)
            memcpy(gptr->cells, gptr->next_cells, gptr->size * gptr->size * sizeof(Cell));
    }
    payload_intent_buffer_capture_system(sys, sys->intent_buf);
    if (sys->intent_ring) payload_intent_ring_push(sys->intent_ring, sys);
}

/* PHASE_ANALYZE: density / payload census. Stored in metrics_history if available;
   for now we just compute (no-op storage) — slot is reserved for future PayloadMetricsHistory. */
void payload_phase_analyze_census(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    /* Placeholder: per-grid density and avg payload could be stored here.
       Currently no PayloadMetricsHistory exists; kept as a hook for later. */
}

/* PHASE_ANALYZE: payload local fitness accumulation. */
void payload_phase_analyze_local_fitness(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)phase;
    PayloadLocalFitnessConfig *cfg = (PayloadLocalFitnessConfig *)userdata;
    if (!sys || !cfg) return;
    if (!cfg->prev_alloced) {
        for (int i = 0; i < sys->num_grids; i++) {
            PayloadGrid *g = sys->grids[i];
            if (g && g->active) cfg->prev_cells[i] = calloc(g->size * g->size, sizeof(Cell));
        }
        cfg->prev_alloced = 1;
    }
    const Cell *prev_arr[MAX_PAYLOAD_GRIDS] = {0};
    for (int i = 0; i < sys->num_grids; i++) prev_arr[i] = cfg->prev_cells[i];
    payload_local_fitness_accumulate(sys, prev_arr,
                                      (PayloadLocalFitnessMode)cfg->mode,
                                      cfg->target_grid, (Edge)cfg->target_edge,
                                      cfg->discount);
    for (int i = 0; i < sys->num_grids; i++) {
        PayloadGrid *g = sys->grids[i];
        if (g && g->active && cfg->prev_cells[i]) {
            memcpy(cfg->prev_cells[i], g->cells, g->size * g->size * sizeof(Cell));
        }
    }
}

/* PHASE_MUTATE: run local GA every N ticks. */
void payload_phase_mutate_local_ga(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    if (!sys || !phase || !userdata) return;
    PayloadMutateConfig *cfg = (PayloadMutateConfig *)userdata;
    int interval = cfg->interval;
    if (interval < 1) interval = 1;
    /* Per-instance tick counter: each PayloadMutateConfig owns its own cadence,
       so concurrent or sequential pipelines do not interfere. Init to 0 at
       allocation; presets calloc/zero-init this struct. */
    if ((cfg->tick++ % interval) != 0) return;
    /* Allocate genomes if missing on every mutate call (cheap when present) */
    for (int gi = 0; gi < sys->num_grids; gi++) {
        PayloadGrid *g = sys->grids[gi];
        if (g && g->active) payload_grid_alloc_genomes(g);
    }
    payload_local_ga_step_system(sys, cfg->tournament_k, cfg->mutate_mask, rng_u64);
}

/* ============================================================
 * Configs
 * ============================================================ */

PayloadLocalFitnessConfig *payload_local_fitness_config_create(int mode, int target_grid, int target_edge, double discount) {
    PayloadLocalFitnessConfig *c = calloc(1, sizeof(PayloadLocalFitnessConfig));
    if (!c) return NULL;
    c->mode = mode;
    c->target_grid = target_grid;
    c->target_edge = target_edge;
    c->discount = discount;
    return c;
}

void payload_local_fitness_config_destroy(PayloadLocalFitnessConfig *cfg) {
    if (!cfg) return;
    for (int i = 0; i < MAX_PAYLOAD_GRIDS; i++) free(cfg->prev_cells[i]);
    free(cfg);
}

/* ============================================================
 * Presets
 * ============================================================ */

PayloadPipeline *payload_pipeline_preset_default(void) {
    PayloadPipeline *p = payload_pipeline_create("default");
    if (!p) return NULL;
    payload_pipeline_add_phase(p, PHASE_RUN, "step_all", payload_phase_run, NULL, -1);
    return p;
}

PayloadPipeline *payload_pipeline_preset_intent(IntentMode mode, float alpha) {
    (void)alpha;
    PayloadPipeline *p = payload_pipeline_create("intent");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    if (!mode_ptr) { payload_pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    /* RUN with target_grid=-2 means "no swap" so EXCHANGE handles it */
    int run = payload_pipeline_add_phase(p, PHASE_RUN, "compute_local", payload_phase_run, NULL, -2);
    int ex  = payload_pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", payload_phase_exchange_intent, mode_ptr, -1);
    payload_pipeline_add_dep(p, ex, run);
    return p;
}

PayloadPipeline *payload_pipeline_preset_analyze(IntentMode mode, float alpha) {
    PayloadPipeline *p = payload_pipeline_preset_intent(mode, alpha);
    if (!p) return NULL;
    int ex = 1;
    int census = payload_pipeline_add_phase(p, PHASE_ANALYZE, "census", payload_phase_analyze_census, NULL, -1);
    payload_pipeline_add_dep(p, census, ex);
    return p;
}

PayloadPipeline *payload_pipeline_preset_genomic(IntentMode mode, float alpha, int ga_interval,
                                                   int fitness_mode, int target_grid, int target_edge,
                                                   double fitness_discount, int mutate_mask) {
    (void)alpha;
    PayloadPipeline *p = payload_pipeline_create("genomic");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    PayloadMutateConfig *mut_cfg = calloc(1, sizeof(PayloadMutateConfig));
    if (!mode_ptr || !mut_cfg) { free(mode_ptr); free(mut_cfg); payload_pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    mut_cfg->interval = ga_interval;
    mut_cfg->mutate_mask = mutate_mask;
    mut_cfg->tournament_k = 3;
    mut_cfg->tick = 0;
    int run = payload_pipeline_add_phase(p, PHASE_RUN, "compute_local_genomic", payload_phase_run_genomic, NULL, -2);
    int ex  = payload_pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", payload_phase_exchange_intent, mode_ptr, -1);
    payload_pipeline_add_dep(p, ex, run);
    int prereq_for_mut = ex;
    if (fitness_mode >= 0) {
        PayloadLocalFitnessConfig *cfg = payload_local_fitness_config_create(fitness_mode, target_grid, target_edge, fitness_discount);
        if (cfg) {
            int fit = payload_pipeline_add_phase(p, PHASE_ANALYZE, "local_fitness",
                                                   payload_phase_analyze_local_fitness, cfg, -1);
            payload_pipeline_add_dep(p, fit, ex);
            prereq_for_mut = fit;
        }
    }
    if (ga_interval > 0) {
        int mut = payload_pipeline_add_phase(p, PHASE_MUTATE, "local_ga", payload_phase_mutate_local_ga, mut_cfg, -1);
        payload_pipeline_add_dep(p, mut, prereq_for_mut);
    } else {
        free(mut_cfg);
    }
    return p;
}
