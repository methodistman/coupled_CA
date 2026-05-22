#ifndef PIPELINE_H
#define PIPELINE_H

#include "engine.h"

/* ============================================================
 * Pipeline Contract
 * ============================================================
 *
 * A Pipeline is a DAG of Phases executed once per tick by
 * pipeline_execute(). Phases declare dependencies via
 * pipeline_add_dep(); the executor topologically sorts them.
 *
 * Phase types and their responsibilities:
 *
 *   PHASE_RUN
 *     Computes the next grid state from the current state and
 *     external coupling reads. Writes output to one of:
 *       (a) g->cells in-place after a local memcpy from
 *           g->next_cells (legacy, "default" preset), OR
 *       (b) g->next_cells, leaving g->cells unchanged for the
 *           subsequent EXCHANGE phase to swap (intent presets).
 *     The choice is per-phase and must match the EXCHANGE phase
 *     downstream. The two built-in conventions:
 *       - phase_grid_step           : in-place (a). No EXCHANGE
 *                                     needed; produces results
 *                                     bit-equal to legacy sys_step().
 *       - phase_grid_step_parallel  : (b). MUST be followed by
 *                                     phase_exchange_intent.
 *       - phase_grid_step_genomic   : (b). MUST be followed by
 *                                     phase_exchange_intent.
 *
 *   PHASE_EXCHANGE
 *     Reads g->next_cells, applies pending intents from
 *     sys->intent_buf onto g->next_cells edges, then performs
 *     g->cells <- g->next_cells. Finally captures NEW intents
 *     from the just-swapped g->cells into sys->intent_buf and
 *     pushes a snapshot into sys->intent_ring (for TE/delayed
 *     coupling). On entry sys->intent_buf may be NULL; the phase
 *     allocates it lazily.
 *
 *   PHASE_ANALYZE
 *     Read-only with respect to grid state. May write to
 *     sys->metrics_history (per-tick time-series) and may read
 *     from sys->intent_ring (for TE). Must not modify cells,
 *     next_cells, or genomes.
 *
 *   PHASE_MUTATE
 *     Modifies g->genomes (and possibly g->fitness scratch).
 *     Must not modify g->cells or g->next_cells. Reads
 *     g->fitness as the selection signal — therefore an
 *     ANALYZE phase that accumulates fitness should be
 *     declared as a prereq.
 *
 * Tick boundary invariants (after pipeline_execute returns):
 *   - g->cells holds the new state for tick T+1.
 *   - g->next_cells contents are unspecified.
 *   - sys->intent_buf holds intents captured FROM tick T+1's
 *     g->cells, ready to be applied at the next tick.
 *   - sys->intent_ring has one new snapshot pushed.
 *   - sys->metrics_history->length has incremented (if non-NULL).
 *   - p->tick_count has incremented.
 *
 * Phase ordering responsibility:
 *   - PHASE_EXCHANGE must depend on the PHASE_RUN that wrote
 *     next_cells.
 *   - PHASE_ANALYZE that reads census/intents must depend on
 *     the PHASE_EXCHANGE that finalized them.
 *   - PHASE_MUTATE that consumes fitness must depend on the
 *     PHASE_ANALYZE that accumulated it.
 * pipeline_add_dep() does NOT verify these — presets are the
 * source of truth.
 * ============================================================ */

/* Maximum phases and dependencies per pipeline */
#define PIPELINE_MAX_PHASES 32
#define PIPELINE_MAX_DEPS   8

typedef enum {
    PHASE_RUN,      /* execute a kernel over a grid or system */
    PHASE_EXCHANGE, /* sync / halos / intent exchange */
    PHASE_ANALYZE,  /* read-only compute (census, metrics, TE) */
    PHASE_MUTATE,   /* GA or rule modulation */
} PhaseType;

typedef struct Phase Phase;
typedef struct Pipeline Pipeline;

/* Phase function signature: (system, phase, userdata) */
typedef void (*PhaseFn)(System *sys, const Phase *phase, void *userdata);

struct Phase {
    PhaseType type;
    const char *name;
    PhaseFn fn;       /* kernel function */
    void *userdata;   /* passed to fn */
    int target_grid;  /* -1 = all grids, >=0 = specific grid index */
    int deps[PIPELINE_MAX_DEPS]; /* indices of phases that must complete before this one */
    int ndeps;
};

struct Pipeline {
    Phase phases[PIPELINE_MAX_PHASES];
    int n_phases;
    int tick_count;
    const char *name;
};

/* --- Lifecycle --- */
Pipeline *pipeline_create(const char *name);
void pipeline_destroy(Pipeline *p);
void pipeline_reset(Pipeline *p);

/* --- Construction --- */
/* Add a phase to the pipeline. Returns phase index, or -1 if full. */
int pipeline_add_phase(Pipeline *p, PhaseType type, const char *name,
                       PhaseFn fn, void *userdata, int target_grid);
/* Add a dependency: phase 'dependent' waits for phase 'prereq' */
void pipeline_add_dep(Pipeline *p, int dependent, int prereq);

/* --- Execution --- */
/* Execute one tick of the pipeline using topological sort. */
void pipeline_execute(Pipeline *p, System *sys);

/* --- Built-in phase functions --- */
void phase_grid_step(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_parallel(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_orientation(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_orientation_m0(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_orientation_m1(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_orientation_m2(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_orientation_m3(System *sys, const Phase *phase, void *userdata);
void phase_grid_step_genomic_dir_rules(System *sys, const Phase *phase, void *userdata);

/* Direct mode-specific genomic step for experiments that bypass the pipeline.
   mode: 0-3 selects which orientation field (mode0..mode3).
   use_dir_rules: non-zero enables directional rule switching (alt_rule on dir_mask match). */
void sys_step_genomic_mode(System *sys, int mode, int use_dir_rules);

void phase_exchange_intent(System *sys, const Phase *phase, void *userdata);
void phase_analyze_census(System *sys, const Phase *phase, void *userdata);
void phase_analyze_te(System *sys, const Phase *phase, void *userdata);
void phase_analyze_local_fitness(System *sys, const Phase *phase, void *userdata);
void phase_mutate_local_ga(System *sys, const Phase *phase, void *userdata);

/* Configuration for phase_modulate_rule, passed via userdata.
   ctrl_grid's cell state selects target_grid's per-cell genome rule. */
typedef struct {
    int ctrl_grid;
    int target_grid;
    int rule_if_alive;
    int rule_if_dead;
} ModulateConfig;

void phase_modulate_rule(System *sys, const Phase *phase, void *userdata);

/* Configuration for phase_analyze_local_fitness, passed via userdata. */
typedef struct {
    int mode;          /* LocalFitnessMode (cast to int) */
    int target_grid;   /* for FITNESS_EDGE_SIGNAL */
    int target_edge;   /* Edge enum (cast to int) */
    double discount;   /* fitness discount factor [0,1], 1.0 = accumulate */
    /* Internal: previous-tick cells for stability/activity modes. */
    uint8_t *prev_cells[MAX_GRIDS];
    int prev_alloced;
} LocalFitnessConfig;

LocalFitnessConfig *local_fitness_config_create(int mode, int target_grid, int target_edge, double discount);
void local_fitness_config_destroy(LocalFitnessConfig *cfg);

/* --- Presets --- */
/* Replicates current sys_step() behavior: sequential grid steps with inline coupling */
Pipeline *pipeline_preset_default(void);

/* Uses intent buffers: compute all grids, apply intents, swap, capture */
Pipeline *pipeline_preset_intent(IntentMode mode, float alpha);

/* Intent + per-tick census and TE analysis */
Pipeline *pipeline_preset_analyze(IntentMode mode, float alpha);

/* Parallel compute with intent buffers */
Pipeline *pipeline_preset_parallel(IntentMode mode, float alpha);

/* Genomic step + local fitness accumulation + local GA mutation every N ticks.
   If fitness_mode < 0, no fitness phase is added (legacy behavior).
   fitness_discount: [0,1] discount factor for fitness accumulation (1.0 = default accumulate).
   mutate_mask: bitmask of GENOME_MUTATE_* controlling which genome fields evolve. */
Pipeline *pipeline_preset_genomic(IntentMode mode, float alpha, int ga_interval,
                                   int fitness_mode, int target_grid, int target_edge,
                                   double fitness_discount, int mutate_mask);
Pipeline *pipeline_preset_genomic_orientation(IntentMode mode, float alpha, int ga_interval,
                                               int fitness_mode, int target_grid, int target_edge,
                                               double fitness_discount, int mutate_mask);

#endif
