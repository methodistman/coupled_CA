# Grid Update 04: Pipeline Integration for PayloadSystem

## Implementation Status: COMPLETE (with one documented future-work item)

Done (in `ca_core/payload_pipeline.{h,c}`):
- `PayloadPipeline` / `PayloadPhase` data model with dependency edges.
- `payload_pipeline_create` / `_destroy` / `_reset` / `_add_phase` / `_add_dep` / `_execute` (topological sort by repeated dependency-satisfaction scan).
- Phase functions: `payload_phase_run`, `payload_phase_exchange_intent`, `payload_phase_analyze_census` (stub), `payload_phase_analyze_local_fitness`, `payload_phase_mutate_local_ga`.
- Presets: `_preset_default`, `_preset_intent`, `_preset_analyze`, `_preset_genomic` (full RUN → EXCHANGE → ANALYZE → MUTATE chain).
- Engine integration: `PayloadSystem` carries a `struct PayloadPipeline *pipeline` field; `payload_sys_step` short-circuits to `payload_pipeline_execute()` when set.
- Tests: `tests/test_payload_pipeline.c`.

Fixed:
- The shared static tick counter in `payload_phase_mutate_local_ga` has been replaced with `PayloadMutateConfig::tick` (one counter per phase instance). `_preset_genomic` now `calloc`s the config so the counter starts at zero.

Future work:
- **No `PayloadMetricsHistory`, `metrics/payload_metrics.*`, or `metrics/payload_history.*` files exist.** `payload_phase_analyze_census` is intentionally a stub that computes nothing. The spec section "Metrics History for Payload" remains future work.

Out of scope:
- `payload_pipeline_preset_parallel` is omitted (spec already marked it "not yet supported").

## Objective
Port the existing `Pipeline` / `Phase` architecture from the binary `System` to the `PayloadSystem`, enabling payload grids to use composable tick presets: `PHASE_RUN → PHASE_EXCHANGE → PHASE_ANALYZE → PHASE_MUTATE`.

## Files
- **Create**: `ca_core/payload_pipeline.h`, `ca_core/payload_pipeline.c`
- **Modify**: `ca_core/payload_engine.h`, `ca_core/payload_engine.c`
- **Reference**: `ca_core/pipeline.h`, `ca_core/pipeline.c`, `ca_core/payload_local_ga.h`

## Background: Binary Pipeline

The binary pipeline (`ca_core/pipeline.h`) defines four phase types:

```c
@/home/gregory/code/wavebuffer/coupled_ca/ca_core/pipeline.h:12-18
typedef enum {
    PHASE_RUN,      /* compute next_cells */
    PHASE_EXCHANGE, /* apply intents / coupling */
    PHASE_ANALYZE,  /* compute metrics */
    PHASE_MUTATE,   /* run local GA */
} PhaseType;
```

A `Pipeline` is a DAG of `Phase` nodes executed in topological order each tick. The `System` can either run legacy `sys_step()` or delegate to `pipeline_execute()`.

## Payload Pipeline Requirements

Payload grids need the same four phase types but with payload-specific implementations:

| Phase | Binary | Payload |
|-------|--------|---------|
| `RUN` | `grid_step` / `bitgrid_step_life` | `payload_sys_step` local compute (3x3 payload rules) |
| `EXCHANGE` | `intent_apply_to_next` / `coupling_read` | `payload_intent_apply_to_next` / `payload_coupling_read` |
| `ANALYZE` | `metrics_compute` + `metrics_history_append` | Payload metrics + history append |
| `MUTATE` | `local_ga_step_system` | `payload_local_ga_step_system` |

## API Design

### Header (`payload_pipeline.h`)

```c
#ifndef PAYLOAD_PIPELINE_H
#define PAYLOAD_PIPELINE_H

#include "payload_engine.h"
#include "payload_local_ga.h"
#include "intent.h"

/* Reuse the PhaseType enum from pipeline.h */
#include "pipeline.h"

typedef struct PayloadPhase PayloadPhase;
typedef struct PayloadPipeline PayloadPipeline;

/* Phase function signature: receives the PayloadSystem and opaque user data */
typedef void (*PayloadPhaseFn)(PayloadSystem *sys, void *user_data);

struct PayloadPhase {
    PhaseType type;            /* RUN, EXCHANGE, ANALYZE, MUTATE */
    PayloadPhaseFn fn;
    void *user_data;
    const char *name;
    int num_deps;
    int max_deps;
    PayloadPhase **deps;       /* execution dependencies */
};

struct PayloadPipeline {
    PayloadPhase *phases;
    int num_phases;
    int capacity;
    PayloadPhase **order;      /* topologically sorted execution order */
    int order_len;
};

/* Lifecycle */
PayloadPipeline *payload_pipeline_create(int max_phases);
void payload_pipeline_destroy(PayloadPipeline *p);

/* Build */
PayloadPhase *payload_pipeline_add(PayloadPipeline *p, PhaseType type,
                                     const char *name, PayloadPhaseFn fn, void *user_data);
int payload_pipeline_depends(PayloadPipeline *p, PayloadPhase *phase, PayloadPhase *on);
int payload_pipeline_build(PayloadPipeline *p);  /* topologically sort, return 0 on success */

/* Execute */
void payload_pipeline_execute(PayloadPipeline *p, PayloadSystem *sys);

/* Built-in phase functions (declared here, defined in .c) */
void payload_phase_run(PayloadSystem *sys, void *user_data);      /* user_data = NULL */
void payload_phase_exchange(PayloadSystem *sys, void *user_data); /* user_data = IntentMode* + alpha* */
void payload_phase_analyze(PayloadSystem *sys, void *user_data);  /* user_data = MetricsHistory* */
void payload_phase_mutate(PayloadSystem *sys, void *user_data);   /* user_data = GAParams* */

/* Preset constructors */
PayloadPipeline *payload_pipeline_preset_default(PayloadSystem *sys);
PayloadPipeline *payload_pipeline_preset_intent(PayloadSystem *sys, IntentMode mode, float alpha);
PayloadPipeline *payload_pipeline_preset_parallel(PayloadSystem *sys);  /* not yet supported */
PayloadPipeline *payload_pipeline_preset_genomic(PayloadSystem *sys, IntentMode mode, float alpha,
                                                   int tournament_k, int mutate_mask, uint64_t (*rng)(void));

#endif
```

### Implementation (`payload_pipeline.c`)

#### Phase Functions

**`payload_phase_run`**:
- Iterates all payload grids.
- For each grid, collects 3x3 `Cell` neighborhood via `payload_coupling_read`.
- Applies the grid's `PAYLOAD_RULE_TABLE[rule_idx].fn`.
- Writes to `next_cells`.
- Does **not** swap yet (swap happens in a separate phase or at end).

**`payload_phase_exchange`**:
- User data should be a small struct: `{ IntentMode mode; float alpha; }`
- If `sys->intent_buf` is NULL, auto-create it.
- Apply captured intents from previous tick to `next_cells` edges.
- Then swap `cells <-> next_cells` for all grids.
- Then capture new intents from current `cells` into `sys->intent_buf`.

**`payload_phase_analyze`**:
- User data points to a `PayloadMetricsHistory` (to be defined, or reuse binary `MetricsHistory` with a wrapper).
- For each grid, compute density (`alive` count), average payload, payload entropy.
- Append to history.

**`payload_phase_mutate`**:
- User data is a `GAParams` struct: `{ int tournament_k; int mutate_mask; uint64_t (*rng)(void); }`
- Calls `payload_local_ga_step_system(sys, params->tournament_k, params->mutate_mask, params->rng)`.

#### Presets

**`payload_pipeline_preset_default`**:
```
RUN ──► (end)
```
Equivalent to legacy `payload_sys_step`.

**`payload_pipeline_preset_intent`**:
```
RUN ──► EXCHANGE
```
Equivalent to `payload_sys_step_intent`.

**`payload_pipeline_preset_genomic`**:
```
RUN ──► EXCHANGE ──► ANALYZE ──► MUTATE
```
Full tick with delayed coupling, metrics, and local GA.

## Engine Integration

Modify `PayloadSystem` to optionally hold a pipeline:

```c
@/home/gregory/code/wavebuffer/coupled_ca/ca_core/payload_engine.h:10-20
typedef struct PayloadSystem {
    PayloadGrid *grids[MAX_PAYLOAD_GRIDS];
    int num_grids;
    PayloadCoupling coupling;
    PayloadIntentBuffer *intent_buf;
    PayloadIntentRing *intent_ring;
    struct PayloadMetricsHistory *metrics_history;
    struct PayloadPipeline *pipeline;   /* NEW: NULL = legacy payload_sys_step */
} PayloadSystem;
```

Modify `payload_sys_step`:
```c
void payload_sys_step(PayloadSystem *s) {
    if (s->pipeline) {
        payload_pipeline_execute(s->pipeline, s);
    } else {
        /* legacy scalar path */
        ...
    }
}
```

## Metrics History for Payload

The binary `MetricsHistory` stores density, entropy, activity, gliders, oscillators, and a TE matrix. For payload grids, we need:
- `alive_density` (same as binary density)
- `avg_payload` (mean of `cell.payload` across grid)
- `payload_entropy` (entropy of payload histogram)
- `alive_payload_corr` (correlation between alive and payload)

Options:
1. **Extend** the existing `MetricsHistory` with payload fields (breaks binary experiments).
2. **Create** `PayloadMetricsHistory` with parallel arrays.
3. **Reuse** `MetricsHistory` but ignore payload-specific fields in binary context, and add a separate `PayloadMetrics` struct.

**Recommendation**: Create `metrics/payload_metrics.h` and `metrics/payload_history.h` with a `PayloadMetricsHistory` that mirrors `MetricsHistory` but stores payload-aware fields. Keep it separate to avoid cross-contamination.

## Testing Strategy

1. **Preset default**: Create pipeline, run 10 ticks, compare output to legacy `payload_sys_step` on identical initial state. Must match bit-for-bit.

2. **Preset intent**: Create pipeline with `INTENT_MODE_REPLACE`, alpha=1.0. Run 10 ticks. Compare to `payload_sys_step_intent`. Must match.

3. **Preset genomic**: Create pipeline with all four phases. Verify that:
   - `next_cells` are computed in RUN.
   - Intents are applied in EXCHANGE before swap.
   - Metrics are appended in ANALYZE.
   - Genomes are mutated in MUTATE.
   - The `payload_sys_step` wrapper produces the same result as manual phase execution.

4. **Dependency validation**: Create a pipeline with `MUTATE` depending on `RUN` and `EXCHANGE` depending on `RUN`. Verify topological sort rejects cycles (e.g., `MUTATE → RUN → MUTATE`).

## Makefile Update
```makefile
PCORE   = ca_core/payload_grid.o ca_core/payload_rules.o \
          ca_core/payload_coupling.o ca_core/payload_engine.o \
          ca_core/payload_intent.o ca_core/payload_local_ga.o \
          ca_core/payload_pipeline.o
METRICS = metrics/metrics.o metrics/export.o metrics/glossary.o \
          metrics/transfer_entropy.o metrics/history.o \
          metrics/payload_metrics.o metrics/payload_history.o
```

## Estimated Effort
- Pipeline header + core logic: 1.5 hours
- Built-in phase functions: 1 hour
- Presets: 30 min
- Payload metrics/history: 1 hour
- Engine integration + tests: 1.5 hours
- **Total: ~5.5 hours**

## Dependencies
- Requires **Grid Update 02** (WaveEngine) for the pattern of how to integrate a pipeline into an engine.
- Can be developed in parallel with WaveSystem if the binary pipeline is used as a direct template.
