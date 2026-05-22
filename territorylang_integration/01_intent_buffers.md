---
description: Phase 1 — Replace inline edge coupling with asynchronous intent buffers
tags: [territorylang, coupling, intent, phase1]
---

# Phase 1: Intent Buffers for Decoupled Coupling

## TerritoryLang Concept

From `03-execution-and-scheduling.md`:
> Non-local actions are expressed as **append-only intent records**, produced in decision phases, consumed in apply phases.

In TerritoryLang, cross-grid or cross-cell communication is not synchronous read-write. Instead:
1. Grid A's update kernel **produces intents** at its boundary (e.g., "edge state at tick t was X")
2. An explicit `exchange` phase **moves intents** across the coupling
3. Grid B's update kernel **consumes intents** when computing its next state

This decouples the two grids' update order and enables more complex information flow.

## Current Problem

In `ca_core/engine.c:sys_step()`, coupling is inline and synchronous:

```c
/* Grid 0 updates, then Grid 1 reads Grid 0's NEW state */
for (g = 0; g < sys->n_grids; g++) {
    for each edge connection {
        coupling_read(src, dst, edge, scale_mode);  /* reads SRC's current state */
    }
    grid_step(sys->grids[g]);  /* then advances DST */
}
```

**Issues:**
- Grid 1 reads Grid 0's already-updated state (not the state that caused the transition)
- No history — can't compute temporal correlations, transfer entropy, or delayed signaling
- All coupling is single-step, single-direction, synchronous

## Proposed Design

### 1. Intent Record Type

```c
/* ca_core/intent.h */
typedef struct {
    int tick;          /* when this intent was produced */
    int src_grid;      /* producer */
    int src_edge;      /* which edge of producer */
    int dst_grid;      /* consumer */
    int dst_edge;      /* which edge of consumer */
    uint8_t *cells;    /* flattened edge cells (length = edge_size) */
    int n_cells;
} EdgeIntent;

typedef struct {
    EdgeIntent *items;
    int count;
    int capacity;
} IntentBuffer;
```

### 2. Two-Phase Update

Replace `sys_step()` with:

```c
void sys_step_phased(System *sys) {
    /* PHASE 1: All grids compute their next state from current state */
    for (int g = 0; g < sys->n_grids; g++) {
        grid_compute_next(sys->grids[g]);  /* read from current, write to NEXT buffer */
    }

    /* PHASE 2: Produce edge intents from computed next-states */
    for each connection (src -> dst) {
        edge_intent_produce(sys, src, dst);  /* snapshot src edge into intent buffer */
    }

    /* PHASE 3: Apply edge intents as boundary conditions before swap */
    for each connection (src -> dst) {
        edge_intent_consume(sys, dst, src);  /* write intent cells into dst's edge */
    }

    /* PHASE 4: Swap all grids */
    for (int g = 0; g < sys->n_grids; g++) {
        grid_swap(sys->grids[g]);
    }
}
```

### 3. Consumption Modes

The `consume` phase supports multiple modes, selectable per connection:

| Mode | Behavior | Use Case |
|------|----------|----------|
| `REPLACE` | Overwrite destination edge with intent cells | Direct signal injection |
| `ADD` | Cell-wise OR | Activity accumulation |
| `WEIGHTED` | `dst = alpha * intent + (1-alpha) * dst` | Proportional coupling |
| `THRESHOLD` | `dst = (intent > 0 && intent >= alpha) ? 1 : 0` | Gated transfer; alpha acts as threshold |

### 4. Temporal Buffering

Store last N intents per connection to enable:
- **Delayed coupling**: consume intent from tick `t-k` instead of tick `t`
- **Transfer entropy**: compute `H(dst_t | dst_{t-1}, src_{t-1})`
- **Pulse detection**: detect rising/falling edges in intent history

```c
typedef struct {
    IntentBuffer ring[N];
    int head;
    int delay;  /* consume from head - delay */
} TemporalIntentQueue;
```

## Integration Path

### Files Modified

1. **New**: `ca_core/intent.h/c` — intent buffer types and operations ✅
2. **Modify**: `ca_core/engine.h/c` — add `sys_step_intent()` and `sys_step_intent_delayed()` ✅
3. **Modify**: `experiments/exp_signal.c` — opt-in via `--intent` and `--intent-mode` flags ✅
4. **New**: `tests/test_intent.c` — unit tests for intent buffer capture and apply ✅
5. **New**: `metrics/transfer_entropy.h/c` — binary TE computation from intent ring history ✅
6. **Fix**: `IntentRing` ownership moved from leaky `static` to per-`System` field ✅

### Step-by-Step Migration (Status)

```
Step 1: Add intent.h/c with basic EdgeIntent + IntentBuffer          [DONE]
Step 2: Add sys_step_intent() — compute->apply->swap->capture         [DONE]
Step 3: Add consumption modes (REPLACE, ADD, WEIGHTED, THRESHOLD)    [DONE]
Step 4: Port one experiment (exp_signal) to use phased stepping       [DONE]
Step 5: Add temporal ring buffer with configurable delay            [DONE]
Step 6: Add transfer entropy computation module (binary time series)  [DONE]
Step 7: Fix IntentRing ownership: per-System, destroyed in sys_destroy [DONE]
Step 8: Benchmark: phased vs inline coupling for correctness        [PENDING]
```

## API Sketch

```c
/* Setup */
IntentBuffer *ib = intent_buffer_create(max_edges, max_edge_size);
sys_set_intent_buffer(sys, ib);

/* Configure a connection with mode and delay */
Connection *conn = sys_connection(sys, 0, EDGE_RIGHT, 1, EDGE_LEFT);
conn_set_mode(conn, MODE_WEIGHTED, 0.3);
conn_set_delay(conn, 2);  /* consume intent from 2 ticks ago */

/* Step with intent buffers (one-tick delay) */
sys_step_intent(sys, INTENT_MODE_REPLACE, 0.0f);

/* Step with configurable delay via IntentRing (ring owned by System) */
sys_step_intent_delayed(sys, INTENT_MODE_REPLACE, 0.0f, /*delay=*/2);

/* Compute transfer entropy from ring history */
double te = te_compute_from_ring(sys->intent_ring,
    0, EDGE_RIGHT, 1, EDGE_LEFT, /*delay=*/0, /*history=*/10);
```

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Memory overhead from intent buffers | Intent is just a byte array per edge; for 64×64 grids, one edge = 64 bytes. With 2 grids and N=4 delay, total = 2×4×64 = 512 bytes — negligible |
| Existing experiments break | Keep `sys_step()` as default; phased stepping is opt-in via `sys_set_step_mode(sys, STEP_PHASED)` |
| Performance regression from extra copy | Phase 2+3 are just `memcpy` of edge strips; main cost is still `grid_step()` kernel. Profile before/after |
| Correctness: does phased stepping change dynamics? | Yes, intentionally. Document the semantic difference: inline = Grid 1 sees Grid 0's NEW state; phased = Grid 1 sees Grid 0's OLD state. Both are valid coupling models |

## Success Criteria

- [x] `sys_step_intent()` runs without errors alongside existing `sys_step()`
- [x] `exp_signal` supports `--intent` flag for opt-in phased stepping
- [x] Default vs intent coupling produce measurably different dynamics (confirmed via diff)
- [x] Transfer entropy between edge time-series is measurable (`te_compute_from_ring`)
- [x] Delayed coupling via `sys_step_intent_delayed()` with configurable delay
- [x] `IntentRing` is owned by `System`, cleaned up in `sys_destroy`
- [ ] Delayed coupling produces observable wave propagation effects (needs experiment)
- [ ] No performance regression > 10% on 128×128 grids (needs formal benchmark)

## Relation to Other Phases

- **Phase 2 (Tick Pipeline)**: Intent buffers are the data passed between pipeline phases
- **Phase 3 (Per-Cell Genomes)**: Genomes can modulate `conn_set_mode()` per region
- **Phase 4 (TerritoryLang DSL)**: Intent buffers become first-class language constructs
