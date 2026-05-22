#include "pipeline.h"
#include "../metrics/metrics.h"
#include "../metrics/transfer_entropy.h"
#include "../metrics/history.h"
#include "../utils/thread_pool.h"
#include "../utils/rng.h"
#include "local_ga.h"
#include <stdlib.h>
#include <string.h>

Pipeline *pipeline_create(const char *name) {
    Pipeline *p = calloc(1, sizeof(Pipeline));
    if (p) p->name = name ? name : "unnamed";
    return p;
}
void pipeline_destroy(Pipeline *p) { free(p); }
void pipeline_reset(Pipeline *p) { if (p) { p->n_phases = 0; p->tick_count = 0; } }

int pipeline_add_phase(Pipeline *p, PhaseType type, const char *name,
                       PhaseFn fn, void *userdata, int target_grid) {
    if (!p || p->n_phases >= PIPELINE_MAX_PHASES) return -1;
    Phase *ph = &p->phases[p->n_phases];
    ph->type = type; ph->name = name ? name : "phase";
    ph->fn = fn; ph->userdata = userdata; ph->target_grid = target_grid;
    ph->ndeps = 0;
    return p->n_phases++;
}

void pipeline_add_dep(Pipeline *p, int dependent, int prereq) {
    if (!p || dependent < 0 || dependent >= p->n_phases) return;
    if (prereq < 0 || prereq >= p->n_phases) return;
    Phase *ph = &p->phases[dependent];
    if (ph->ndeps < PIPELINE_MAX_DEPS) ph->deps[ph->ndeps++] = prereq;
}

void pipeline_execute(Pipeline *p, System *sys) {
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
        if (!progressed) break; /* deadlock or cycle */
    }
    free(done);
    p->tick_count++;
    if (sys->metrics_history) sys->metrics_history->length++;
}

/* ============================================================
 * Built-in phase functions
 * ============================================================ */

/* Kernel configuration flags for grid_step_kernel. */
#define KERNEL_USE_GENOMES   (1u << 0)  /* rule from g->genomes[idx], coupling weight from genome */
#define KERNEL_INPLACE_SWAP  (1u << 1)  /* memcpy(cells <- next_cells) after compute */
#define KERNEL_USE_ORIENTATION (1u << 2)  /* weight neighbors by cos²(φ−θ) from genome orientation */
#define KERNEL_ORIENTATION_MODE_SHIFT 3
#define KERNEL_ORIENTATION_MODE_MASK  (3u << KERNEL_ORIENTATION_MODE_SHIFT)
#define KERNEL_USE_DIR_RULES   (1u << 5)  /* use alt_rule_select per dir_mask */

/* Anisotropic coupling lookup: ALIGNMENT_SCALE[orientation][neighbor_dir]
   Scale factors: 16=1.0 (aligned), 8=0.5 (45° off), 0=0.0 (90° off).
   Orientation 0=E,1=NE,2=N,3=NW,4=W,5=SW,6=S,7=SE.
   Neighbor dir  0=E,1=NE,2=N,3=NW,4=W,5=SW,6=S,7=SE. */
static const int DIR_MAP[3][3] = {
    {3, 2, 1},
    {4, -1, 0},
    {5, 6, 7}
};
static const int ALIGNMENT_SCALE[8][8] = {
    {16, 8, 0, 8, 16, 8, 0, 8},
    {8, 16, 8, 0, 8, 16, 8, 0},
    {0, 8, 16, 8, 0, 8, 16, 8},
    {8, 0, 8, 16, 8, 0, 8, 16},
    {16, 8, 0, 8, 16, 8, 0, 8},
    {8, 16, 8, 0, 8, 16, 8, 0},
    {0, 8, 16, 8, 0, 8, 16, 8},
    {8, 0, 8, 16, 8, 0, 8, 16},
};

/* Single-grid step kernel. Reads g->cells (and neighbors via coupling_read),
   writes g->next_cells. If KERNEL_INPLACE_SWAP, also performs the swap.
   When KERNEL_USE_GENOMES is set and g->genomes is non-NULL, per-cell rule
   and probabilistic coupling weight are used; weight=15 (=1.0) takes the
   fast path with no RNG consumption. Per-grid RNG state is used for
   determinism across pipeline presets.

   Marked always_inline so call-site constant flags fold away branches:
   the non-genomic fast path becomes identical to a hand-specialized kernel. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
static inline void grid_step_kernel(System *sys, int gi, unsigned flags) {
    Grid *g = sys->grids[gi];
    if (!g || !g->active) return;
    int sz = g->size;
    int has_genomes = (flags & KERNEL_USE_GENOMES) && g->genomes != NULL;
    int use_orientation = (flags & KERNEL_USE_ORIENTATION) && has_genomes;
    int orient_mode = (flags & KERNEL_ORIENTATION_MODE_MASK) >> KERNEL_ORIENTATION_MODE_SHIFT;
    int use_dir_rules = (flags & KERNEL_USE_DIR_RULES) && has_genomes;
    /* Default rule (used when no genomes or out-of-range genome rule) */
    int default_rule_idx = g->rule_idx;
    if (default_rule_idx < 0 || default_rule_idx >= NUM_RULES) return;
    const CARule *default_rule = &RULE_TABLE[default_rule_idx];

    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int idx = y * sz + x;
            const CARule *rule = default_rule;
            int weight4 = 15; /* full coupling */
            int orientation = 0;
            int dir_mask = 0;
            CellGenome genome = 0;
            if (has_genomes) {
                genome = g->genomes[idx];
                int ri = (int)GENOME_RULE_SELECT(genome);
                rule = &RULE_TABLE[((ri % NUM_RULES) + NUM_RULES) % NUM_RULES];
                weight4 = (int)GENOME_COUPLING_WEIGHT(genome);
                switch (orient_mode) {
                    case 0: orientation = (int)GENOME_ORIENT_MODE0(genome); break;
                    case 1: orientation = (int)GENOME_ORIENT_MODE1(genome); break;
                    case 2: orientation = (int)GENOME_ORIENT_MODE2(genome); break;
                    case 3: orientation = (int)GENOME_ORIENT_MODE3(genome); break;
                }
                dir_mask = (int)GENOME_DIR_MASK(genome);
            }
            if (rule->is_1d) {
                int l = coupling_read(&sys->coupling, sys->grids, gi, x, y, -1, 0);
                int c = g->cells[idx];
                int r = coupling_read(&sys->coupling, sys->grids, gi, x, y, 1, 0);
                int pattern = (l << 2) | (c << 1) | r;
                g->next_cells[idx] = (rule->rule_1d >> pattern) & 1;
            } else {
                int count = 0;
                int masked_count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx || dy) {
                            int val = coupling_read(&sys->coupling, sys->grids, gi, x, y, dx, dy);
                            if (val && weight4 < 15) {
                                /* Per-grid RNG: deterministic across presets and threads */
                                uint64_t r = rng_splitmix64(&g->rng_state);
                                int threshold = weight4 * (0xFFFF / 15); /* 0..0xFFFF */
                                if (use_orientation) {
                                    int dir = DIR_MAP[dy + 1][dx + 1];
                                    int scale = ALIGNMENT_SCALE[orientation][dir];
                                    threshold = (threshold * scale) / 16;
                                }
                                val = ((int)(r & 0xFFFF) < threshold) ? 1 : 0;
                            }
                            count += val;
                            /* Cardinal directions: N=0,S=1,E=2,W=3 encoded in dir_mask bits */
                            if (use_dir_rules && dir_mask) {
                                int cardinal = -1;
                                if (dx == 0 && dy == -1) cardinal = 0; /* N */
                                else if (dx == 0 && dy == 1) cardinal = 1; /* S */
                                else if (dx == 1 && dy == 0) cardinal = 2; /* E */
                                else if (dx == -1 && dy == 0) cardinal = 3; /* W */
                                if (cardinal >= 0 && (dir_mask & (1 << cardinal))) {
                                    masked_count += val;
                                }
                            }
                        }
                    }
                }
                /* Directional rule: if masked-direction neighbors are active, switch to alt rule */
                if (use_dir_rules && masked_count > 0) {
                    int alt_ri = (int)GENOME_ALT_RULE_SELECT(genome);
                    rule = &RULE_TABLE[((alt_ri % NUM_RULES) + NUM_RULES) % NUM_RULES];
                }
                int alive = g->cells[idx];
                g->next_cells[idx] = alive ? rule->survive[count] : rule->born[count];
            }
        }
    }
    if (flags & KERNEL_INPLACE_SWAP) {
        memcpy(g->cells, g->next_cells, sz * sz * sizeof(uint8_t));
    }
}

void phase_grid_step(System *sys, const Phase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    if (phase->target_grid >= 0) {
        grid_step_kernel(sys, phase->target_grid, KERNEL_INPLACE_SWAP);
    } else {
        for (int gi = 0; gi < sys->num_grids; gi++) {
            grid_step_kernel(sys, gi, KERNEL_INPLACE_SWAP);
        }
    }
}

/* Step all grids using per-cell genomes for rule selection and coupling weight.
   Writes to next_cells without swapping; downstream EXCHANGE phase performs swap. */
void phase_grid_step_genomic(System *sys, const Phase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        if (phase->target_grid >= 0 && phase->target_grid != gi) continue;
        /* NOTE: legacy behavior swapped in place; we preserve it here for
           bit-compat with existing tests. Future cleanup: drop the swap and
           require pipeline_preset_genomic to include exchange_intent (it does). */
        grid_step_kernel(sys, gi, KERNEL_USE_GENOMES | KERNEL_INPLACE_SWAP);
    }
}

/* Step all grids using per-cell genomes for rule selection, coupling weight,
   AND anisotropic orientation weighting. Neighbors aligned with the cell's
   preferred direction (cos² ≈ 1) contribute fully; orthogonal neighbors
   (cos² ≈ 0) are ignored. */
void phase_grid_step_genomic_orientation(System *sys, const Phase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        if (phase->target_grid >= 0 && phase->target_grid != gi) continue;
        grid_step_kernel(sys, gi, KERNEL_USE_GENOMES | KERNEL_USE_ORIENTATION | KERNEL_INPLACE_SWAP);
    }
}

/* Mode-specific orientation phases. Each reads a different orientation field
   from the 48-bit genome, allowing the same grid to host 4 directional
   computation modes simultaneously. */
static void phase_grid_step_genomic_orientation_mode(System *sys, const Phase *phase, void *userdata, int mode) {
    (void)userdata;
    if (!sys || !phase) return;
    unsigned flags = KERNEL_USE_GENOMES | KERNEL_USE_ORIENTATION | KERNEL_INPLACE_SWAP
                   | ((unsigned)mode << KERNEL_ORIENTATION_MODE_SHIFT);
    for (int gi = 0; gi < sys->num_grids; gi++) {
        if (phase->target_grid >= 0 && phase->target_grid != gi) continue;
        grid_step_kernel(sys, gi, flags);
    }
}

void phase_grid_step_genomic_orientation_m0(System *sys, const Phase *phase, void *userdata) {
    phase_grid_step_genomic_orientation_mode(sys, phase, userdata, 0);
}
void phase_grid_step_genomic_orientation_m1(System *sys, const Phase *phase, void *userdata) {
    phase_grid_step_genomic_orientation_mode(sys, phase, userdata, 1);
}
void phase_grid_step_genomic_orientation_m2(System *sys, const Phase *phase, void *userdata) {
    phase_grid_step_genomic_orientation_mode(sys, phase, userdata, 2);
}
void phase_grid_step_genomic_orientation_m3(System *sys, const Phase *phase, void *userdata) {
    phase_grid_step_genomic_orientation_mode(sys, phase, userdata, 3);
}

/* Directional rule modification: cells switch to alt_rule_select when any
   neighbor from a masked cardinal direction is active. */
void phase_grid_step_genomic_dir_rules(System *sys, const Phase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    unsigned flags = KERNEL_USE_GENOMES | KERNEL_USE_ORIENTATION | KERNEL_USE_DIR_RULES | KERNEL_INPLACE_SWAP;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        if (phase->target_grid >= 0 && phase->target_grid != gi) continue;
        grid_step_kernel(sys, gi, flags);
    }
}

/* Direct mode-specific genomic step for experiments.
   Bypasses pipeline system to allow per-evaluation mode switching. */
void sys_step_genomic_mode(System *sys, int mode, int use_dir_rules) {
    if (!sys) return;
    unsigned flags = KERNEL_USE_GENOMES | KERNEL_USE_ORIENTATION | KERNEL_INPLACE_SWAP
                   | ((unsigned)mode << KERNEL_ORIENTATION_MODE_SHIFT);
    if (use_dir_rules) flags |= KERNEL_USE_DIR_RULES;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        grid_step_kernel(sys, gi, flags);
    }
}

LocalFitnessConfig *local_fitness_config_create(int mode, int target_grid, int target_edge, double discount) {
    LocalFitnessConfig *c = calloc(1, sizeof(LocalFitnessConfig));
    if (!c) return NULL;
    c->mode = mode;
    c->target_grid = target_grid;
    c->target_edge = target_edge;
    c->discount = discount;
    return c;
}

void local_fitness_config_destroy(LocalFitnessConfig *cfg) {
    if (!cfg) return;
    for (int i = 0; i < MAX_GRIDS; i++) free(cfg->prev_cells[i]);
    free(cfg);
}

void phase_analyze_local_fitness(System *sys, const Phase *phase, void *userdata) {
    (void)phase;
    LocalFitnessConfig *cfg = (LocalFitnessConfig *)userdata;
    if (!sys || !cfg) return;
    /* Allocate prev_cells lazily */
    if (!cfg->prev_alloced) {
        for (int i = 0; i < sys->num_grids; i++) {
            Grid *g = sys->grids[i];
            if (g && g->active) cfg->prev_cells[i] = calloc(g->size * g->size, sizeof(uint8_t));
        }
        cfg->prev_alloced = 1;
    }
    const uint8_t *prev_arr[MAX_GRIDS] = {0};
    for (int i = 0; i < sys->num_grids; i++) prev_arr[i] = cfg->prev_cells[i];
    local_fitness_accumulate(sys, prev_arr, (LocalFitnessMode)cfg->mode,
                              cfg->target_grid, (Edge)cfg->target_edge, cfg->discount);
    /* Snapshot current cells as prev for next tick */
    for (int i = 0; i < sys->num_grids; i++) {
        Grid *g = sys->grids[i];
        if (g && g->active && cfg->prev_cells[i]) {
            memcpy(cfg->prev_cells[i], g->cells, g->size * g->size * sizeof(uint8_t));
        }
    }
}

/* Local GA mutation phase: runs every N ticks.
   userdata points to a struct with interval and mutate_mask. */
typedef struct {
    int interval;
    int mutate_mask;
} MutateConfig;

void phase_mutate_local_ga(System *sys, const Phase *phase, void *userdata) {
    if (!sys || !phase || !userdata) return;
    MutateConfig *cfg = (MutateConfig *)userdata;
    int interval = cfg->interval;
    if (interval < 1) interval = 1;
    if (sys->pipeline && (sys->pipeline->tick_count % interval) != 0) return;
    local_ga_step_system_alloc(sys, 3, cfg->mutate_mask, rng_u64);
}

void phase_modulate_rule(System *sys, const Phase *phase, void *userdata) {
    (void)phase;
    if (!sys || !userdata) return;
    ModulateConfig *cfg = (ModulateConfig *)userdata;
    if (cfg->ctrl_grid < 0 || cfg->ctrl_grid >= sys->num_grids) return;
    if (cfg->target_grid < 0 || cfg->target_grid >= sys->num_grids) return;
    Grid *ctrl = sys->grids[cfg->ctrl_grid];
    Grid *target = sys->grids[cfg->target_grid];
    if (!ctrl || !ctrl->active || !target || !target->active) return;
    if (!target->genomes) return;
    int n = target->size * target->size;
    for (int i = 0; i < n; i++) {
        int rule = ctrl->cells[i] ? cfg->rule_if_alive : cfg->rule_if_dead;
        GENOME_SET_RULE_SELECT(target->genomes[i], rule);
    }
}

void phase_exchange_intent(System *sys, const Phase *phase, void *userdata) {
    (void)phase;
    IntentMode *mode = (IntentMode *)userdata;
    if (!sys || !mode) return;
    if (!sys->intent_buf) {
        int max_edge = 0;
        for (int g = 0; g < sys->num_grids; g++) {
            if (sys->grids[g] && sys->grids[g]->size > max_edge)
                max_edge = sys->grids[g]->size;
        }
        sys->intent_buf = intent_buffer_create(sys->num_grids * 4, max_edge);
    }
    /* Apply previous intents to next_cells edges, then swap, then capture */
    for (int i = 0; i < sys->intent_buf->count; i++) {
        EdgeIntent *ei = &sys->intent_buf->items[i];
        Grid *dst = sys->grids[ei->dst_grid];
        if (dst && dst->active) intent_apply_to_next(ei, dst, *mode, 0.0f);
    }
    for (int g = 0; g < sys->num_grids; g++) {
        Grid *gptr = sys->grids[g];
        if (gptr && gptr->active)
            memcpy(gptr->cells, gptr->next_cells, gptr->size * gptr->size * sizeof(uint8_t));
    }
    intent_buffer_capture_system(sys, sys->intent_buf);
    if (sys->intent_ring) intent_ring_push(sys->intent_ring, sys);
}

void phase_analyze_census(System *sys, const Phase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    int gi = phase->target_grid;
    if (gi < 0) {
        for (int i = 0; i < sys->num_grids; i++) {
            Grid *g = sys->grids[i];
            if (g && g->active) {
                GridMetrics m;
                metrics_census(g, &m);
                if (sys->metrics_history) metrics_history_append(sys->metrics_history, i, &m);
            }
        }
    } else {
        Grid *g = sys->grids[gi];
        if (g && g->active) {
            GridMetrics m;
            metrics_census(g, &m);
            if (sys->metrics_history) metrics_history_append(sys->metrics_history, gi, &m);
        }
    }
}

void phase_analyze_te(System *sys, const Phase *phase, void *userdata) {
    (void)phase; (void)userdata;
    if (!sys || !sys->intent_ring) return;
    if (sys->num_grids >= 2 && sys->intent_ring->size >= 3) {
        double te = te_compute_from_ring(sys->intent_ring, 0, EDGE_RIGHT, 1, EDGE_LEFT, 0, sys->intent_ring->size);
        if (sys->metrics_history) metrics_history_append_te(sys->metrics_history, 0, 1, te);
    }
}

/* ============================================================
 * Presets
 * ============================================================ */

Pipeline *pipeline_preset_default(void) {
    Pipeline *p = pipeline_create("default");
    if (!p) return NULL;
    /* Phase 0: step all grids with inline coupling (replicates sys_step() scalar path) */
    pipeline_add_phase(p, PHASE_RUN, "step_all", phase_grid_step, NULL, -1);
    return p;
}

Pipeline *pipeline_preset_intent(IntentMode mode, float alpha) {
    (void)alpha;
    Pipeline *p = pipeline_create("intent");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    if (!mode_ptr) { pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    /* Phase 0: compute next_cells locally for all grids */
    pipeline_add_phase(p, PHASE_RUN, "compute_local", phase_grid_step, NULL, -1);
    /* Phase 1: apply previous intents, swap, capture new intents */
    int ex = pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", phase_exchange_intent, mode_ptr, -1);
    pipeline_add_dep(p, ex, 0); /* exchange after compute */
    return p;
}

Pipeline *pipeline_preset_analyze(IntentMode mode, float alpha) {
    Pipeline *p = pipeline_preset_intent(mode, alpha);
    if (!p) return NULL;
    int ex = 1; /* exchange phase index from preset_intent */
    int census = pipeline_add_phase(p, PHASE_ANALYZE, "census", phase_analyze_census, NULL, -1);
    pipeline_add_dep(p, census, ex);
    int te = pipeline_add_phase(p, PHASE_ANALYZE, "transfer_entropy", phase_analyze_te, NULL, -1);
    pipeline_add_dep(p, te, ex);
    return p;
}

typedef struct {
    System *sys;
    int grid_idx;
} GridStepArg;

static void parallel_grid_step(void *arg) {
    GridStepArg *a = arg;
    grid_step_kernel(a->sys, a->grid_idx, KERNEL_INPLACE_SWAP);
}

static ThreadPool *g_parallel_tp = NULL;

void phase_grid_step_parallel(System *sys, const Phase *phase, void *userdata) {
    (void)phase; (void)userdata;
    if (!sys) return;
    if (!g_parallel_tp) g_parallel_tp = tp_create(tp_hw_concurrency());
    if (!g_parallel_tp) { /* fallback to sequential */
        for (int gi = 0; gi < sys->num_grids; gi++) {
            GridStepArg a = {sys, gi};
            parallel_grid_step(&a);
        }
        return;
    }
    GridStepArg *args = calloc(sys->num_grids, sizeof(GridStepArg));
    Task *tasks = calloc(sys->num_grids, sizeof(Task));
    if (!args || !tasks) { free(args); free(tasks); return; }
    for (int gi = 0; gi < sys->num_grids; gi++) {
        args[gi].sys = sys; args[gi].grid_idx = gi;
        tasks[gi].fn = parallel_grid_step; tasks[gi].arg = &args[gi];
    }
    tp_submit_batch(g_parallel_tp, tasks, sys->num_grids);
    tp_wait(g_parallel_tp);
    free(args); free(tasks);
}

Pipeline *pipeline_preset_parallel(IntentMode mode, float alpha) {
    (void)alpha;
    Pipeline *p = pipeline_create("parallel");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    if (!mode_ptr) { pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    int run = pipeline_add_phase(p, PHASE_RUN, "compute_parallel", phase_grid_step_parallel, NULL, -1);
    int ex = pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", phase_exchange_intent, mode_ptr, -1);
    pipeline_add_dep(p, ex, run);
    return p;
}

Pipeline *pipeline_preset_genomic(IntentMode mode, float alpha, int ga_interval,
                                   int fitness_mode, int target_grid, int target_edge,
                                   double fitness_discount, int mutate_mask) {
    (void)alpha;
    Pipeline *p = pipeline_create("genomic");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    MutateConfig *mut_cfg = malloc(sizeof(MutateConfig));
    if (!mode_ptr || !mut_cfg) { free(mode_ptr); free(mut_cfg); pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    mut_cfg->interval = ga_interval;
    mut_cfg->mutate_mask = mutate_mask;
    int run = pipeline_add_phase(p, PHASE_RUN, "compute_genomic", phase_grid_step_genomic, NULL, -1);
    int ex = pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", phase_exchange_intent, mode_ptr, -1);
    pipeline_add_dep(p, ex, run);
    int prereq_for_mut = ex;
    if (fitness_mode >= 0) {
        LocalFitnessConfig *cfg = local_fitness_config_create(fitness_mode, target_grid, target_edge, fitness_discount);
        if (cfg) {
            int fit = pipeline_add_phase(p, PHASE_ANALYZE, "local_fitness",
                                          phase_analyze_local_fitness, cfg, -1);
            pipeline_add_dep(p, fit, ex);
            prereq_for_mut = fit;
        }
    }
    /* ga_interval <= 0 means "no GA" — skip the MUTATE phase entirely.
       Otherwise the phase would still run on tick 0 (since 0 % N == 0). */
    if (ga_interval > 0) {
        int mut = pipeline_add_phase(p, PHASE_MUTATE, "local_ga", phase_mutate_local_ga, mut_cfg, -1);
        pipeline_add_dep(p, mut, prereq_for_mut);
    } else {
        free(mut_cfg); /* unused */
    }
    return p;
}

/* Genomic pipeline with anisotropic orientation weighting.
   Uses phase_grid_step_genomic_orientation instead of phase_grid_step_genomic.
   All other parameters (intent mode, fitness, mutation) are identical. */
Pipeline *pipeline_preset_genomic_orientation(IntentMode mode, float alpha, int ga_interval,
                                                 int fitness_mode, int target_grid, int target_edge,
                                                 double fitness_discount, int mutate_mask) {
    (void)alpha;
    Pipeline *p = pipeline_create("genomic_orientation");
    if (!p) return NULL;
    IntentMode *mode_ptr = malloc(sizeof(IntentMode));
    MutateConfig *mut_cfg = malloc(sizeof(MutateConfig));
    if (!mode_ptr || !mut_cfg) { free(mode_ptr); free(mut_cfg); pipeline_destroy(p); return NULL; }
    *mode_ptr = mode;
    mut_cfg->interval = ga_interval;
    mut_cfg->mutate_mask = mutate_mask;
    int run = pipeline_add_phase(p, PHASE_RUN, "compute_genomic_orientation", phase_grid_step_genomic_orientation, NULL, -1);
    int ex = pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent", phase_exchange_intent, mode_ptr, -1);
    pipeline_add_dep(p, ex, run);
    int prereq_for_mut = ex;
    if (fitness_mode >= 0) {
        LocalFitnessConfig *cfg = local_fitness_config_create(fitness_mode, target_grid, target_edge, fitness_discount);
        if (cfg) {
            int fit = pipeline_add_phase(p, PHASE_ANALYZE, "local_fitness",
                                          phase_analyze_local_fitness, cfg, -1);
            pipeline_add_dep(p, fit, ex);
            prereq_for_mut = fit;
        }
    }
    if (ga_interval > 0) {
        int mut = pipeline_add_phase(p, PHASE_MUTATE, "local_ga", phase_mutate_local_ga, mut_cfg, -1);
        pipeline_add_dep(p, mut, prereq_for_mut);
    } else {
        free(mut_cfg);
    }
    return p;
}
