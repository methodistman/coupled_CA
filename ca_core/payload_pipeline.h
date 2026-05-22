#ifndef PAYLOAD_PIPELINE_H
#define PAYLOAD_PIPELINE_H

#include "payload_engine.h"
#include "payload_local_ga.h"
#include "intent.h"
#include "pipeline.h"   /* for PhaseType enum */

#define PAYLOAD_PIPELINE_MAX_PHASES 32
#define PAYLOAD_PIPELINE_MAX_DEPS   8

typedef struct PayloadPhase PayloadPhase;
typedef struct PayloadPipeline PayloadPipeline;

typedef void (*PayloadPhaseFn)(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);

struct PayloadPhase {
    PhaseType type;
    const char *name;
    PayloadPhaseFn fn;
    void *userdata;
    int target_grid;     /* -1 = all grids */
    int deps[PAYLOAD_PIPELINE_MAX_DEPS];
    int ndeps;
};

struct PayloadPipeline {
    PayloadPhase phases[PAYLOAD_PIPELINE_MAX_PHASES];
    int n_phases;
    int tick_count;
    const char *name;
};

/* Lifecycle */
PayloadPipeline *payload_pipeline_create(const char *name);
void payload_pipeline_destroy(PayloadPipeline *p);
void payload_pipeline_reset(PayloadPipeline *p);

/* Construction */
int  payload_pipeline_add_phase(PayloadPipeline *p, PhaseType type, const char *name,
                                  PayloadPhaseFn fn, void *userdata, int target_grid);
void payload_pipeline_add_dep(PayloadPipeline *p, int dependent, int prereq);

/* Execute one tick using topological sort. */
void payload_pipeline_execute(PayloadPipeline *p, PayloadSystem *sys);

/* Built-in phase functions */
void payload_phase_run(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);
void payload_phase_run_genomic(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);
void payload_phase_exchange_intent(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);
void payload_phase_analyze_census(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);
void payload_phase_analyze_local_fitness(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);
void payload_phase_mutate_local_ga(PayloadSystem *sys, const PayloadPhase *phase, void *userdata);

/* Configuration for payload_phase_analyze_local_fitness */
typedef struct {
    int mode;            /* PayloadLocalFitnessMode (cast to int) */
    int target_grid;
    int target_edge;
    double discount;
    Cell *prev_cells[MAX_PAYLOAD_GRIDS];
    int prev_alloced;
} PayloadLocalFitnessConfig;

PayloadLocalFitnessConfig *payload_local_fitness_config_create(int mode, int target_grid, int target_edge, double discount);
void payload_local_fitness_config_destroy(PayloadLocalFitnessConfig *cfg);

typedef struct {
    int interval;
    int mutate_mask;
    int tournament_k;
    int tick;            /* internal: incremented each time the phase runs; init to 0 */
} PayloadMutateConfig;

/* Presets */
PayloadPipeline *payload_pipeline_preset_default(void);
PayloadPipeline *payload_pipeline_preset_intent(IntentMode mode, float alpha);
PayloadPipeline *payload_pipeline_preset_analyze(IntentMode mode, float alpha);
PayloadPipeline *payload_pipeline_preset_genomic(IntentMode mode, float alpha, int ga_interval,
                                                   int fitness_mode, int target_grid, int target_edge,
                                                   double fitness_discount, int mutate_mask);

#endif
