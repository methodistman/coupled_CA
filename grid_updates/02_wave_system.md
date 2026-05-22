# Grid Update 02: WaveSystem Engine, Coupling, and Hybrid Cross-Connections

## Implementation Status: MOSTLY COMPLETE

Done:
- `WaveCoupling` (`ca_core/wave_coupling.{h,c}`): init / connect / disconnect / read.
- `WaveSystem` (`ca_core/wave_engine.{h,c}`): init, destroy, step, step_intent, randomize, recompute_nb. `nb_genome_interval` defaults to 10 and is honored by both step functions.
- `WaveIntent` (`ca_core/wave_intent.{h,c}`): buffer + ring, capture + apply.
- `WaveRule` table (`ca_core/wave_rules.{h,c}`): `wave_rule_life`, `wave_rule_diffuse`, `wave_rule_genomic_life`.
- Hybrid integration (`ca_core/hybrid_engine.{h,c}`): `HYBRID_WAVE` enum value, `WaveSystem wave` field in `HybridSystem`, all four cross-type transfer functions (binary <-> wave, payload <-> wave), and `hybrid_sys_step` dispatch over `HybridXConn` for every direction.

Gap:
- **`hybrid_coupling_read` does not handle `HYBRID_WAVE` as a source type** (see `ca_core/hybrid_engine.c:189-254`; only `HYBRID_BINARY` and `HYBRID_PAYLOAD` branches exist). Reading neighbors from a wave grid across a cross-type edge therefore returns `{0,0}`. This is the only remaining gap for this update.
- Test coverage for the wave branch of `hybrid_coupling_read` is correspondingly absent.

## Objective
Create a complete `WaveSystem` engine that can run standalone WaveGrid simulations and participate in `HybridSystem` cross-type coupling with binary `Grid` and payload `PayloadGrid` instances.

## Files
- **Create**: `ca_core/wave_engine.h`, `ca_core/wave_engine.c`
- **Create**: `ca_core/wave_coupling.h`, `ca_core/wave_coupling.c`
- **Modify**: `ca_core/hybrid_engine.h`, `ca_core/hybrid_engine.c`
- **Reference**: `ca_core/wave_grid.h`, `ca_core/payload_engine.h`, `ca_core/engine.h`

## 1. WaveCoupling

### Header (`wave_coupling.h`)

```c
#ifndef WAVE_COUPLING_H
#define WAVE_COUPLING_H

#include "wave_grid.h"
#include "coupling.h"

typedef struct {
    int target_grid;
    int target_edge;
    float strength;        /* 0.0–1.0: how much wave/nb_genome transfers */
} WaveEdgeConn;

typedef struct {
    WaveEdgeConn conn[MAX_WAVE_GRIDS][4];
    int num_grids;
} WaveCoupling;

void wave_coupling_init(WaveCoupling *c, int num_grids);
void wave_coupling_connect(WaveCoupling *c, int src, Edge src_e, int dst, Edge dst_e, float strength);
void wave_coupling_disconnect(WaveCoupling *c, int src, Edge src_e);

/* Read neighbor with toroidal wrap and edge coupling. Returns a full WaveCell. */
WaveCell wave_coupling_read(const WaveCoupling *c, WaveGrid *const *grids, int g, int x, int y, int dx, int dy);

#endif
```

### Implementation Notes

`wave_coupling_read()` behaves like `payload_coupling_read()` but returns a `WaveCell` instead of a `Cell`:
- If `dx == 0 && dy == 0`, return `grids[g]->cells[y*sz + x]`.
- If on an edge and a `WaveEdgeConn` exists for that edge:
  - Read from the target grid's corresponding edge.
  - Blend `alive`, `wave`, `genome`, `nb_genome` based on `strength`.
- Otherwise, wrap toroidally within the same grid.

**Blending semantics**:
- `alive`: thresholded weighted average (`strength` of source alive vs destination).
- `wave`: bitwise interpolation. Option A: `wave = (src.wave * strength + dst.wave * (1-strength))` truncated. Option B: bit-by-bit probabilistic mix using `rng_state`. Start with Option A for simplicity.
- `genome`: no blending in read; the coupling only affects `alive` and `wave`. Genomes are read as-is from the coupled grid's edge cell.

## 2. WaveEngine

### Header (`wave_engine.h`)

```c
#ifndef WAVE_ENGINE_H
#define WAVE_ENGINE_H

#include "wave_grid.h"
#include "wave_coupling.h"
#include "intent.h"

struct WaveIntentBuffer;
struct WaveIntentRing;
struct WaveMetricsHistory;

typedef struct WaveSystem {
    WaveGrid *grids[MAX_WAVE_GRIDS];
    int num_grids;
    WaveCoupling coupling;
    struct WaveIntentBuffer *intent_buf;
    struct WaveIntentRing *intent_ring;
    struct WaveMetricsHistory *metrics_history;
    int nb_genome_interval;   /* recompute nb_genomes every N ticks */
    int tick_counter;         /* increments every step */
} WaveSystem;

void wave_sys_init(WaveSystem *s, int num_grids, int grid_size);
void wave_sys_destroy(WaveSystem *s);
void wave_sys_step(WaveSystem *s);
void wave_sys_step_intent(WaveSystem *s, IntentMode mode, float alpha);
void wave_sys_randomize(WaveSystem *s, uint64_t (*rng)(void));

/* Force a nb_genome recompute regardless of interval. */
void wave_sys_recompute_nb(WaveSystem *s, int mode);

#endif
```

### Implementation (`wave_engine.c`)

#### `wave_sys_init`
- Set `num_grids`, init `coupling`, create each `WaveGrid`.
- Set `nb_genome_interval = 10`, `tick_counter = 0`.
- Leave `intent_buf`, `intent_ring`, `metrics_history` as `NULL`.

#### `wave_sys_destroy`
- Destroy all grids, destroy intent buffer/ring if allocated.

#### `wave_sys_step`
```
for each active grid g:
    for each cell (x,y):
        read 3x3 Moore neighborhood as WaveCell[9] via wave_coupling_read()
        apply rule_fn(nb, &next_cells[y*sz+x], &rng_state)

swap cells <-> next_cells for all grids

tick_counter++
if tick_counter % nb_genome_interval == 0:
    for each grid: wave_grid_recompute_nb_genomes(grid, /*mode from system config*/ 0)
```

**Rule question**: What is the `WaveRule`? For now, reuse the `PayloadRule` concept but with a `WaveRuleFn` signature:
```c
typedef void (*WaveRuleFn)(const WaveCell nb[9], WaveCell *out, uint64_t *rng_state);
```

This allows rules to see `alive`, `wave`, `genome`, and `nb_genome` of all 9 neighbors. The output is a full `WaveCell`.

**Initial rules to implement** (in `wave_rules.h/c`):
1. `wave_rule_life` — Conway's Life on `alive`, ignore `wave`.
2. `wave_rule_diffuse` — average `wave` of alive neighbors, threshold `alive`.
3. `wave_rule_genomic_life` — Life, but effective born/survive threshold is modulated by `coherence`. High coherence → stricter (need more alive neighbors). Low coherence → more permissive.

#### `wave_sys_step_intent`
Mirror `payload_sys_step_intent` exactly but with `WaveCell` intents:
1. Compute `next_cells` locally (no coupling read on edges).
2. Apply captured `WaveIntent` edges to `next_cells`.
3. Swap.
4. Capture new intents from current `cells`.

A `WaveIntentBuffer` stores `WaveCell *` per edge. The `WaveIntentRing` mirrors the `PayloadIntentRing`.

**Create**: `ca_core/wave_intent.h`, `ca_core/wave_intent.c` (analogous to payload_intent).

## 3. Hybrid Cross-Connection Support

### Modify `hybrid_engine.h`

Add `WAVE_GRID` to the `HybridGridType` enum:

```c
typedef enum {
    HYBRID_GRID_BINARY,
    HYBRID_GRID_PAYLOAD,
    HYBRID_GRID_WAVE,        /* NEW */
} HybridGridType;
```

Add `WaveSystem` pointer to `HybridSystem`:

```c
typedef struct {
    System binary;
    PayloadSystem payload;
    WaveSystem wave;         /* NEW */
    HybridXConn xconns[HYBRID_MAX_XCONNS];
    int n_xconns;
} HybridSystem;
```

Add transfer functions:
```c
void transfer_binary_to_wave(const Grid *src, WaveGrid *dst, float strength);
void transfer_wave_to_binary(const WaveGrid *src, Grid *dst, float strength);
void transfer_payload_to_wave(const PayloadGrid *src, WaveGrid *dst, float strength);
void transfer_wave_to_payload(const WaveGrid *src, PayloadGrid *dst, float strength);
```

### Modify `hybrid_engine.c`

Implement the four transfer functions:

- `transfer_binary_to_wave`: Map `alive` (0/1) to `WaveCell.alive`. Set `wave` to a default pattern (e.g., `alive ? WAVE_MASK : 0`). Copy a neutral `genome`.
- `transfer_wave_to_binary`: Threshold `WaveCell.alive` to `uint8_t`. Ignore `wave` and genomes.
- `transfer_payload_to_wave`: Map `Cell.alive` → `WaveCell.alive`, `Cell.payload` → lower 8 bits of `wave`.
- `transfer_wave_to_payload`: Map `WaveCell.alive` → `Cell.alive`, lower 8 bits of `wave` → `Cell.payload`.

Update `hybrid_sys_init` to optionally initialize `wave` if `num_wave_grids > 0`.

## Testing Strategy

Test coverage is consolidated in `tests/test_wave.c` (not split into `test_wave_engine.c` / `test_wave_grid.c` as originally drafted). It exercises: WaveGrid lifecycle, init_random, payload accessors, all three nb_genome consensus modes, coherence, basic WaveSystem stepping with two rules, local_ga_step_system, and (after Update 03 engine wiring) slow-layer mutation gating and two-time-scale separation.

Gap: no test currently exercises a `HybridSystem` with a wave grid driven by a binary cross-edge. Add when `hybrid_coupling_read` grows a `HYBRID_WAVE` branch (the residual gap of this update).

## Estimated Effort
- WaveCoupling: 45 min
- WaveEngine + rules: 1.5 hours
- WaveIntent: 45 min
- Hybrid integration: 1 hour
- Tests: 1 hour
- **Total: ~5 hours**
