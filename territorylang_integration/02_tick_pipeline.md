---
description: Phase 2 — Replace hardcoded sys_step() with a composable DAG of phases
tags: [territorylang, pipeline, tick, dag, phase2]
---

# Phase 2: Structured Tick Pipeline

## TerritoryLang Concept

From `03-execution-and-scheduling.md`:
> A tick is a **directed acyclic graph (DAG)** of **phases**... The compiler checks that any kernel reading neighborhoods of radius `r` is preceded by a sufficiently large `exchange`.

Key insight: the simulation is not a single hardcoded loop. It is a **pipeline definition** that the runtime executes.

## Current Problem

`sys_step()` in `ca_core/engine.c` is a monolithic function:

```c
void sys_step(System *sys) {
    for (g = 0; g < sys->n_grids; g++) {
        for each connection { coupling_read(...); }
        grid_step(sys->grids[g]);
    }
}
```

**Issues:**
- Census, metrics, and GA fitness are bolted on *outside* the step, not part of it
- Can't insert read-only analysis phases (census, TE) without re-running the simulation
- Can't parallelize grid updates because coupling order is hardcoded
- No way to express "run Grid 0 and Grid 1 in parallel, then sync"

## Proposed Design

### 1. Phase Types

```c
/* ca_core/pipeline.h */
typedef enum {
    PHASE_RUN,      /* execute a kernel over a grid or system */
    PHASE_EXCHANGE, /* sync/halos/intent exchange */
    PHASE_ANALYZE,  /* read-only compute (census, metrics, TE) */
    PHASE_MUTATE,   /* GA or rule modulation */
} PhaseType;

typedef struct Phase Phase;
struct Phase {
    PhaseType type;
    const char *name;
    void (*fn)(System*, void*);  /* kernel function */
    void *userdata;
    int target_grid;  /* -1 = all grids */
    Phase **deps;     /* must complete before this phase runs */
    int ndeps;
};
```

### 2. Pipeline Definition

A tick is a `Pipeline` — an array of phases with dependency edges:

```c
typedef struct {
    Phase *phases;
    int n_phases;
    int tick_count;
} Pipeline;
```

### 3. Example: Current Behavior as Pipeline

```c
Pipeline *pipeline_default_coupling(void) {
    Pipeline *p = pipeline_create(4);
    /* Phase 0: Read coupling from Grid 0 -> Grid 1 */
    p->phases[0] = (Phase){
        .type = PHASE_EXCHANGE, .name = "couple_0_to_1",
        .fn = phase_coupling_read, .target_grid = 0
    };
    /* Phase 1: Step Grid 0 */
    p->phases[1] = (Phase){
        .type = PHASE_RUN, .name = "step_0",
        .fn = phase_grid_step, .target_grid = 0,
        .deps = &p->phases[0], .ndeps = 1
    };
    /* Phase 2: Read coupling from Grid 1 -> Grid 0 */
    p->phases[2] = (Phase){
        .type = PHASE_EXCHANGE, .name = "couple_1_to_0",
        .fn = phase_coupling_read, .target_grid = 1
    };
    /* Phase 3: Step Grid 1 */
    p->phases[3] = (Phase){
        .type = PHASE_RUN, .name = "step_1",
        .fn = phase_grid_step, .target_grid = 1,
        .deps = &p->phases[2], .ndeps = 1
    };
    return p;
}
```

### 4. Example: Parallel Grid Updates with Post-Step Census

```c
Pipeline *pipeline_parallel_with_census(void) {
    Pipeline *p = pipeline_create(5);
    /* Phase 0: Exchange all edges */
    p->phases[0] = (Phase){.type = PHASE_EXCHANGE, .name = "exchange_all"};
    /* Phase 1: Step Grid 0 (depends on exchange) */
    p->phases[1] = (Phase){.type = PHASE_RUN, .name = "step_0",
                            .fn = phase_grid_step, .target_grid = 0,
                            .deps = &p->phases[0], .ndeps = 1};
    /* Phase 2: Step Grid 1 (depends on exchange, parallel with step_0) */
    p->phases[2] = (Phase){.type = PHASE_RUN, .name = "step_1",
                            .fn = phase_grid_step, .target_grid = 1,
                            .deps = &p->phases[0], .ndeps = 1};
    /* Phase 3: Census on Grid 1 (read-only, parallel-safe) */
    p->phases[3] = (Phase){.type = PHASE_ANALYZE, .name = "census_1",
                            .fn = phase_census, .target_grid = 1,
                            .deps = &p->phases[2], .ndeps = 1};
    /* Phase 4: TE computation (read-only, parallel-safe) */
    p->phases[4] = (Phase){.type = PHASE_ANALYZE, .name = "transfer_entropy",
                            .fn = phase_te, .target_grid = -1,
                            .deps = &p->phases[1], .ndeps = 1};  /* needs both grids stepped */
    return p;
}
```

### 5. Execution Engine

```c
void pipeline_execute(Pipeline *p, System *sys) {
    /* Simple topological sort execution */
    int *done = calloc(p->n_phases, sizeof(int));
    int completed = 0;
    while (completed < p->n_phases) {
        for (int i = 0; i < p->n_phases; i++) {
            if (done[i]) continue;
            int ready = 1;
            for (int d = 0; d < p->phases[i].ndeps; d++)
                if (!done[p->phases[i].deps[d] - p->phases]) ready = 0;
            if (ready) {
                p->phases[i].fn(sys, p->phases[i].userdata);
                done[i] = 1;
                completed++;
            }
        }
    }
    free(done);
    p->tick_count++;
}
```

## Integration Path

### Files to Modify

1. **New**: `ca_core/pipeline.h/c` — Phase, Pipeline types + topological executor ✅
2. **New**: Built-in phase functions in `pipeline.c` (`phase_grid_step`, `phase_exchange_intent`, `phase_analyze_census`, `phase_analyze_te`) ✅
3. **Modify**: `ca_core/engine.h/c` — `sys_step()` delegates to `pipeline_execute()` when `sys->pipeline` is set ✅
4. **Modify**: `experiments/*.c` — experiments define their own pipeline or use presets [PENDING]

### Migration Strategy

```
Step 1: Create pipeline.h/c with Phase + Pipeline structs and topological executor      [DONE]
Step 2: Implement built-in phase functions (grid_step, exchange_intent, census, TE)      [DONE]
Step 3: Add pipeline presets:                                                              [DONE]
        - PIPELINE_DEFAULT (replicates current behavior via phase_grid_step)
        - PIPELINE_INTENT (uses Phase 1 intent buffers)
        - PIPELINE_ANALYZE (intent + per-tick census + TE)
Step 4: Modify sys_step() to delegate to pipeline executor when sys->pipeline is set       [DONE]
Step 5: Add CLI flags to experiments: --pipeline=default|parallel|analyze                   [DONE]
Step 6: Port exp_ga_ic to use PIPELINE_ANALYZE so census is per-tick, not per-gen          [DONE]
```

## Key Feature: Per-Tick Metrics

With the analyze phase, metrics become part of the tick:

```c
/* Before: census only at generation end */
int gliders = census_gliders(tg);
int oscillators = census_oscillators(tg);

/* After: census runs every tick, stored in time series */
Phase census_phase = {.type = PHASE_ANALYZE, .fn = phase_census};
/* Time series accessible for TE, Fourier analysis, etc. */
double *glider_series = sys->metrics->gliders_over_time;
double *oscillator_series = sys->metrics->oscillators_over_time;
```

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Complexity explosion | Presets hide DAG complexity; most users never define a custom pipeline |
| Overhead from phase dispatch | Phase fn is a direct function pointer call; overhead is O(n_phases) per tick, negligible vs O(N^2) grid update |
| Existing C code doesn't compose well | Phase functions take `(System*, void*)`; userdata can hold any struct. No template/generics needed |
| Parallel execution correctness | Start with sequential topological sort. Parallel executor (thread pool) is Phase 2.5 |

## Relation to Other Phases

- **Phase 1 (Intent Buffers)**: EXCHANGE phases move intents; RUN phases consume them
- **Phase 3 (Per-Cell Genomes)**: MUTATE phases run GA kernels on genome buffers
- **Phase 4 (TerritoryLang DSL)**: Pipeline becomes the primary language construct; phases are user-defined kernels

## Success Criteria

- [x] `sys_step()` delegates to `pipeline_execute()` with zero behavioral change (PIPELINE_DEFAULT)
- [x] `PIPELINE_DEFAULT` preset produces bit-identical results to legacy `sys_step()` (verified for 50 ticks)
- [x] `PIPELINE_PARALLEL` matches `PIPELINE_INTENT` bit-for-bit (50 ticks verified)
- [x] `PIPELINE_INTENT` preset runs without crash and uses intent buffers
- [x] `exp_ga_ic` with `--pipeline analyze --per-tick-census` produces per-tick census time series
- [x] `--pipeline` CLI flag added to `exp_signal.c` and `exp_ga_ic.c` (default|intent|analyze|parallel)
- [x] Adding a new phase requires only defining a `PhaseFn` and calling `pipeline_add_phase`
- [x] Per-tick time-series storage (`MetricsHistory`) integrated into `System` and populated by ANALYZE phases
- [x] No performance regression > 5% for PIPELINE_DEFAULT (benchmarked: ~29% faster on 128x128)
