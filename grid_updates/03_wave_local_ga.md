# Grid Update 03: Dual-Time-Scale Local GA for WaveGrid

## Implementation Status: COMPLETE

Done (in `ca_core/wave_local_ga.{h,c}`):
- `wave_genome_penalty`, `wave_effective_fitness`.
- `wave_local_fitness_accumulate` covering all six `WaveLocalFitnessMode` values.
- `wave_local_ga_step_grid` (fast layer: tournament_k + crossover + mutation, gated by effective fitness).
- `wave_local_ga_step_nb` (slow layer: in-place mutation of `nb_genome`).
- `wave_local_ga_step_system` (system-wide fast-layer wrapper).

Engine integration (in `ca_core/wave_engine.{h,c}`):
- `WaveSystem` now carries `consensus_mode`, `nb_mutation_rate`, `mutate_mask`, and a `rng_fn` function pointer. All initialized to safe defaults (vote consensus, no mutation, NULL rng).
- A new internal `wave_sys_slow_layer()` invokes `wave_grid_recompute_nb_genomes(grid, consensus_mode)` followed by `wave_local_ga_step_nb()` when `rng_fn` is set and `mutate_mask != 0` and `nb_mutation_rate > 0`.
- Both `wave_sys_step` and `wave_sys_step_intent` call the slow layer at multiples of `nb_genome_interval`.

Tests (`tests/test_wave.c`):
- Test 9: slow-layer mutation is gated on `rng_fn` — skipped when NULL, applied when set.
- Test 10: two-time-scale separation — `nb_genome` is stable between recompute intervals.

The fast-layer GA still has no auto-driver in the engine; callers (future `exp_wave_*` experiments) invoke `wave_local_fitness_accumulate` + `wave_local_ga_step_system` themselves, as with the binary and payload stacks.

## Objective
Implement `wave_local_ga.c` that evolves **both cell genomes (fast)** and **neighborhood genomes (slow)** within a `WaveSystem`. The fast layer adapts to local micro-environment; the slow layer maintains territorial identity.

## Files
- **Create**: `ca_core/wave_local_ga.h`, `ca_core/wave_local_ga.c`
- **Modify**: `Makefile` (add new object)
- **Reference**: `ca_core/wave_grid.h`, `ca_core/wave_engine.h`, `ca_core/genome.h`, `ca_core/local_ga.h`

## Concept: Two-Time-Scale Evolution

### Fast Time Scale (every tick, or every `ga_every` ticks)
- **What evolves**: `CellGenome genome` (per-cell)
- **Mechanism**: Tournament selection among 8 Moore neighbors, crossover, mutation
- **Expression**: The cell's genome directly influences its rule parameters (`rule_select`, `coupling_weight`, `mutation_rate`) when the engine applies rules.

### Slow Time Scale (every `nb_genome_interval` ticks, e.g., 10)
- **What evolves**: `NeighborhoodGenome nb_genome` (per-cell, but represents patch consensus)
- **Mechanism**: After `wave_grid_recompute_nb_genomes()` computes a consensus, perturb it with a low-rate mutation.
- **Expression**: The `nb_genome` acts as a "species boundary." If a cell's `genome` diverges too far from its `nb_genome`, its effective fitness is penalized.

### Fitness Penalty Function

```c
static double genome_penalty(CellGenome cell_g, CellGenome nb_g) {
    int d = 0;
    if (GENOME_RULE_SELECT(cell_g) != GENOME_RULE_SELECT(nb_g)) d++;
    if (GENOME_COUPLING_WEIGHT(cell_g) != GENOME_COUPLING_WEIGHT(nb_g)) d++;
    if (GENOME_MUTATION_RATE(cell_g) != GENOME_MUTATION_RATE(nb_g)) d++;
    /* d in [0,3]; penalty ranges from 0.0 (identical) to -1.0 (all different) */
    return -0.33 * d;
}
```

The **effective fitness** used during selection:
```c
double effective_f = g->cells[idx].fitness + genome_penalty(g->cells[idx].genome,
                                                              g->cells[idx].nb_genome);
```

This creates a **selection pressure for genome-neighborhood alignment** while still allowing local specialization.

## API Signatures

```c
#ifndef WAVE_LOCAL_GA_H
#define WAVE_LOCAL_GA_H

#include "wave_grid.h"
#include "wave_engine.h"

typedef enum {
    WAVE_FITNESS_STABILITY,
    WAVE_FITNESS_ACTIVITY,
    WAVE_FITNESS_DENSITY,
    WAVE_FITNESS_EDGE_SIGNAL,
    WAVE_FITNESS_WAVE_MAGNITUDE,     /* new: rewards high wave popcount */
    WAVE_FITNESS_COHERENCE,          /* new: rewards high genome-neighborhood alignment */
} WaveLocalFitnessMode;

/* Accumulate fitness for all cells in all wave grids.
   discount in [0,1]: 1.0 = accumulate forever, <1.0 = exponential decay. */
void wave_local_fitness_accumulate(WaveSystem *sys,
                                   const WaveCell * const *prev_cells,
                                   WaveLocalFitnessMode mode,
                                   int target_grid, Edge target_edge,
                                   double discount);

/* Evolve cell genomes (fast). Operates on next_genomes scratch buffer. */
void wave_local_ga_step_grid(WaveGrid *g, CellGenome *next_genomes,
                               int tournament_k, int mutate_mask,
                               uint64_t (*rng)(void));

/* Evolve neighborhood genomes (slow). Mutates existing nb_genome in-place. */
void wave_local_ga_step_nb(WaveGrid *g, double nb_mutation_rate,
                             int mutate_mask, uint64_t (*rng)(void));

/* Convenience: run fast GA on all grids in a WaveSystem. */
void wave_local_ga_step_system(WaveSystem *sys, int tournament_k,
                                int mutate_mask, uint64_t (*rng)(void));

/* Allocate genomes if missing, then run fast GA. */
void wave_local_ga_step_system_alloc(WaveSystem *sys, int tournament_k,
                                      int mutate_mask, uint64_t (*rng)(void));

#endif
```

## Implementation Details

### 1. `wave_local_fitness_accumulate`

Loop over all grids, all cells. For each cell, compute `delta` based on mode:

- **STABILITY**: `alive` unchanged from `prev_cells` → `+0.1`, changed → `-0.05`
- **ACTIVITY**: `alive` changed → `+0.1`, unchanged → `-0.02`
- **DENSITY**: `alive == 1` → `+0.05`
- **EDGE_SIGNAL**: `alive == 1` on target edge of target grid → `+0.5`
- **WAVE_MAGNITUDE**: `delta = (wave_popcount(wave) / 26.0) * 0.1`
- **COHERENCE**: `delta = wave_grid_coherence(g, x, y) * 0.1`

After computing `delta`, apply `discount`:
```c
if (discount < 1.0 && discount >= 0.0)
    g->cells[idx].fitness = discount * g->cells[idx].fitness + delta;
else
    g->cells[idx].fitness += delta;
```

### 2. `wave_local_ga_step_grid` (Fast Layer)

Identical in structure to `payload_local_ga_step_grid` and `local_ga_step_grid`, but uses **effective fitness**:

```c
static double effective_fitness(const WaveGrid *g, int idx) {
    double base = g->cells[idx].fitness;
    return base + genome_penalty(g->cells[idx].genome, g->cells[idx].nb_genome);
}
```

Tournament: among 8 Moore neighbors (and self), select the one with highest `effective_fitness`. If the winner is a neighbor, copy their `genome`. Then mutate with the winner's mutation rate.

If `tournament_k == 0`, skip tournament and just mutate in place (pure diffusion).

### 3. `wave_local_ga_step_nb` (Slow Layer)

Operates directly on `nb_genome` (in-place mutation, no selection):

```c
void wave_local_ga_step_nb(WaveGrid *g, double nb_mutation_rate,
                            int mutate_mask, uint64_t (*rng)(void)) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) {
        g->cells[i].nb_genome = genome_mutate(g->cells[i].nb_genome,
                                               nb_mutation_rate,
                                               mutate_mask, rng);
    }
}
```

**When to call**: In the engine, after `wave_grid_recompute_nb_genomes()` and only every `nb_genome_interval` ticks.

**Why no selection?** The `nb_genome` is already a consensus of the local population. Selection happens implicitly through cell-level GA: cells that align with the current `nb_genome` have higher effective fitness and survive better. The `nb_genome` just needs occasional drift to allow the patch to explore genotype space slowly.

### 4. `wave_local_ga_step_system`

```c
void wave_local_ga_step_system(WaveSystem *sys, int tournament_k,
                                int mutate_mask, uint64_t (*rng)(void)) {
    if (!sys || !rng) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        WaveGrid *g = sys->grids[gi];
        if (!g || !g->active) continue;
        int n = g->size * g->size;
        CellGenome *tmp = malloc(n * sizeof(CellGenome));
        if (!tmp) continue;
        wave_local_ga_step_grid(g, tmp, tournament_k, mutate_mask, rng);
        /* copy back into cells[].genome */
        for (int i = 0; i < n; i++) g->cells[i].genome = tmp[i];
        free(tmp);
    }
}
```

## Engine Integration

In `wave_sys_step` (or a pipeline phase):

```
if (sys->tick_counter % ga_every == 0) {
    wave_local_fitness_accumulate(sys, prev_cells, mode, ...);
    wave_local_ga_step_system(sys, tournament_k, mutate_mask, rng);
}
if (sys->tick_counter % nb_genome_interval == 0) {
    for each grid:
        wave_grid_recompute_nb_genomes(grid, consensus_mode);
        wave_local_ga_step_nb(grid, nb_mutation_rate, mutate_mask, rng);
}
```

## Testing Strategy

1. **Coherence reward**: Initialize a grid with random genomes and one fixed `nb_genome`. Run fitness accumulation in `COHERENCE` mode. Verify cells whose genomes happen to match `nb_genome` gain more fitness.

2. **Penalty strength sweep**: Run a small grid (8x8) for 100 ticks with different penalty weights (`0.0`, `-0.33`, `-1.0`). Measure average `genome_distance(cell, nb)` at the end. Expect lower distance with stronger penalty.

3. **Two-time-scale separation**: Set `ga_every = 1`, `nb_genome_interval = 20`. Verify that `genome` changes rapidly (every tick) while `nb_genome` changes only at multiples of 20.

## Estimated Effort
- Header + core logic: 1 hour
- Engine integration: 30 min
- Tests: 45 min
- **Total: ~2.5 hours**
