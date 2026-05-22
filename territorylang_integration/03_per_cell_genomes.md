---
description: Phase 3 — Evolve per-cell parameters (rules, coupling weights) not just initial conditions
tags: [territorylang, genome, ga, evolution, per-cell, phase3]
---

# Phase 3: Per-Cell Evolvable Parameters

## TerritoryLang Concept

From `02-core-model.md`:
> Each herd or livestock-related cell can carry a small **genome** describing: effective numeric precision, preferred fuzzy arithmetic schemes... Over many ticks, separate **genetic algorithm kernels** evaluate fitness and mutate genomes.

This is not one genome per simulation. It is **one genome per cell**, evolved in parallel.

## Current Problem

`exp_ga_ic.c` evolves **only the initial condition**:
- Genome = flattened bit array of the initial grid state
- Fitness = signal + census bonus after `steps` ticks
- Evolution happens once, offline, before the simulation runs

**Issues:**
- IC-only evolution is a one-shot search; no adaptation during simulation
- Can't evolve coupling strength, rule selection, or update order
- Genome size = grid_size^2 bits; for 64×64 = 4096 bits. Not scalable to larger grids
- No concept of "inheritance" — best IC doesn't produce children that inherit its structure

## Proposed Design

### 1. Cell Genome Structure

Each cell carries a small fixed-size genome (8-32 bits):

```c
/* ca_core/genome.h */
#define GENOME_BITS 16

typedef struct {
    uint16_t rule_select : 5;      /* index into rule table (0-31) */
    uint16_t coupling_weight : 4;  /* 0-15 scaled to [0,1] */
    uint16_t mutation_rate : 4;    /* local mutation probability */
    uint16_t reserved : 3;         /* for future use */
} CellGenome;
```

Stored as a parallel layer (`CellGenome *genomes` alongside `uint8_t *bits` in Grid).

### 2. Genome-Aware Grid Step

```c
void grid_step_genomic(Grid *g) {
    int sz = g->size;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            int idx = y * sz + x;
            CellGenome *cg = &g->genomes[idx];

            /* Select rule based on genome */
            Rule *r = rule_table[cg->rule_select % rule_count];

            /* Compute neighborhood with coupling */
            float weight = cg->coupling_weight / 15.0f;
            int neighbors = count_moore(g, x, y);
            int coupled = count_coupled(g, x, y) * weight;

            /* Apply rule */
            g->next[idx] = r->transition(neighbors + coupled, g->bits[idx]);
        }
    }
}
```

### 3. Local Fitness Evaluation

Instead of one global fitness, compute **local fitness** per cell or per region:

```c
typedef enum {
    FITNESS_STABILITY,    /* cell state doesn't change = high fitness */
    FITNESS_ACTIVITY,     /* cell oscillates with period 2 = high fitness */
    FITNESS_SIGNAL,       /* cell is on target edge when signal arrives */
    FITNESS_STRUCTURE,    /* cell is part of a recognized pattern */
} LocalFitnessMode;
```

Local fitness is computed every tick and accumulated:

```c
void evaluate_local_fitness(System *sys) {
    for each cell in target grid {
        if (cell_is_on_target_edge(cell))
            cell->fitness_accum += signal_strength_at(cell);
        if (cell_is_blinker_or_glider(cell))
            cell->fitness_accum += FITNESS_STRUCTURE_BONUS;
    }
}
```

### 4. Local GA Kernel

Run a lightweight GA every N ticks on a per-cell or per-region basis:

```c
void local_ga_step(System *sys) {
    for each cell {
        /* Tournament selection among neighbors */
        CellGenome best = select_best_neighbor(sys, x, y);

        /* Crossover with current genome */
        CellGenome child = genome_crossover(&g->genomes[idx], &best);

        /* Mutate */
        if (rng_prob() < child.mutation_rate / 15.0f) {
            genome_mutate(&child, MUTATE_BIT_FLIP);
        }

        /* Elitism: only replace if child fitness >= parent */
        if (cell_fitness(child) >= cell_fitness(g->genomes[idx])) {
            g->next_genomes[idx] = child;
        } else {
            g->next_genomes[idx] = g->genomes[idx];
        }
    }
    swap_genome_buffers(g);
}
```

### 5. Hierarchical Evolution: IC + Per-Cell

Two timescales:
- **Slow**: Global GA evolves initial conditions (as in `exp_ga_ic`), every generation
- **Fast**: Local GA evolves per-cell genomes, every tick or every N ticks

The global GA optimizes the **starting point**; the local GA optimizes **adaptation**.

## Integration Path

### Files to Modify

1. **New**: `ca_core/genome.h/c` — `CellGenome` struct, mutation, crossover
2. **New**: `ca_core/local_ga.h/c` — per-cell tournament selection and replacement
3. **Modify**: `ca_core/grid.h/c` — add `genomes` and `fitness` layers to Grid
4. **Modify**: `ca_core/engine.h/c` — `sys_step_genomic()` variant
5. **Modify**: `experiments/exp_ga_ic.c` — add `--local-ga` flag; run local GA alongside global GA
6. **Modify**: `ca_core/rules.h/c` — rule table must be indexable by `rule_select`

### Migration Strategy

```
Step 1: Add CellGenome to Grid (parallel array, 2 bytes per cell)
Step 2: Initialize genomes: all 0s (default rule, default coupling)
Step 3: Add grid_step_genomic() — uses rule_select and coupling_weight
Step 4: Add local fitness accumulation (read-only, per tick)
Step 5: Add local GA kernel (runs every N ticks, mutates genomes in-place)
Step 6: Benchmark: does local GA improve signal transmission vs static rule?
Step 7: Integrate with global GA: global evolves IC, local evolves adaptation
```

## Concrete Use Cases

### Use Case 1: Self-Organizing Signal Amplification

Goal: Grid 0 should evolve cells near its right edge that use HighLife (rule 1) instead of Life (rule 0), because HighLife's replicator dynamics may amplify signals.

Without per-cell genomes: entire grid uses one rule, set at compile time.
With per-cell genomes: edge cells autonomously discover HighLife improves coupling.

### Use Case 2: Adaptive Coupling Strength

Goal: Grid 1 should reduce coupling weight when it detects chaotic input, increase it when it detects periodic input.

Per-cell `coupling_weight` genome evolves based on local entropy: high entropy → weight down; low entropy (periodic) → weight up.

### Use Case 3: Rule Modulation (H12)

This is the TerritoryLang "one grid controls another's rule" idea:
- Grid 0 cells' state determines Grid 1 cells' `rule_select`
- Grid 1's `rule_select` is not evolved independently; it is **modulated** by Grid 0

```c
/* In Grid 1's step: rule_select = f(Grid 0's coupled cell state) */
CellGenome *cg1 = &grid1->genomes[idx];
int ctrl = grid0_coupled_state;  /* 0 or 1 from Grid 0 */
cg1->rule_select = ctrl ? RULE_LIFE : RULE_HIGHLIFE;
```

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Memory overhead: 2 bytes/cell | For 64×64 = 8KB per grid — negligible. For 1024×1024 = 2MB — still fine |
| Premature convergence | Use spatially local tournaments (3×3 neighborhoods) instead of global selection; maintains diversity |
| Genomes don't correlate with behavior | Start with simple genomes (just rule_select); add fields only when experiment shows they matter |
| Hard to debug why a cell chose rule X | Add `--dump-genomes` flag; export genome layers as heatmaps |
| Local GA overhead | Run every N ticks (e.g., N=10); local fitness accumulates between GA steps |

## Relation to Other Phases

- **Phase 1 (Intent Buffers)**: Genomes can modulate `conn_set_mode()` and `conn_set_delay()` per region
- **Phase 2 (Tick Pipeline)**: Local GA is a MUTATE phase; genome initialization is a RUN phase
- **Phase 4 (TerritoryLang DSL)**: `CellGenome` becomes a typed layer in the territory definition

## Success Criteria

- [x] Grid with per-cell genomes runs without segfaults (rule_select = 0 everywhere = default behavior)
- [x] Local GA on a 32×32 grid runs end-to-end (`exp_local_ga`, see `experiments/exp_local_ga.c`)
- [~] Signal transmission fitness with local GA > signal transmission fitness without local GA
       — infrastructure complete; with default tuning (mut=1, ga_every=10, tournament_k=3) the
       GA wins on some seeds and loses on others. Tuning + better fitness shaping is open research.
- [x] Genome heatmap shows spatial structure (not random noise) — `exp_local_ga --dump-genomes` produces PGMs; A2/A3 measured Moran's I 0.32-0.49 with density fitness (`@/home/gregory/code/wavebuffer/coupled_ca/data/a2/FINDINGS.md`).
- [x] Rule modulation (H12): Grid 0 state deterministically selects Grid 1 rule — `phase_modulate_rule` + `exp_rule_mod.c` implemented.
- [x] Phase C: `rule_select` expanded from 4 to 5 bits; per-cell genomic rule selection added to payload and wave engines.
- [x] Phase D: Population-level GA experiments for logic-gate evolution on binary, payload, wave, and hybrid grids (`exp_binary_logic_ga`, `exp_payload_logic_ga`, `exp_wave_logic_ga`, `exp_hybrid_logic`).

## Implementation Status (as of this revision)

| Component | Status | File |
|---|---|---|
| `CellGenome` packed struct + pack/unpack/mutate/crossover/tournament | Done | `ca_core/genome.h`, `genome.c` |
| `Grid->genomes` and `Grid->fitness` arrays | Done | `ca_core/grid.h`, `grid.c` |
| `grid_alloc_genomes`, `grid_clear_fitness` | Done | `ca_core/grid.c` |
| `phase_grid_step_genomic` (per-cell rule + weighted coupling) | Done | `ca_core/pipeline.c` |
| `phase_analyze_local_fitness` (per-tick fitness accumulator) | Done | `ca_core/pipeline.c` |
| `phase_mutate_local_ga` (tournament + crossover + mutation) | Done | `ca_core/pipeline.c` |
| `pipeline_preset_genomic(...)` wiring all phases with deps | Done | `ca_core/pipeline.c` |
| Per-grid stateful RNG via `rng_splitmix64(&g->rng_state)` | Done | `utils/rng.h` |
| Consolidated kernel (`grid_step_kernel` with flags) | Done | `ca_core/pipeline.c` |
| `exp_local_ga` (baseline vs GA, multi-seed averaging) | Done | `experiments/exp_local_ga.c` |
| `test_genome`, `test_local_ga` unit tests | Done | `tests/` |
| `--dump-genomes` heatmap export | Done | `experiments/exp_local_ga.c` |
| Rule modulation (H12) cross-grid genome update | Done | `ca_core/pipeline.c` (`phase_modulate_rule`), `experiments/exp_rule_mod.c` |
| Phase C: 5-bit `rule_select`, payload genomic rules | Done | `ca_core/genome.h`, `payload_pipeline.c`, `payload_rules.c` |
| Phase C: wave genomic rules (5 new rules) | Done | `ca_core/wave_rules.c`, `wave_engine.c` |
| Phase D: binary logic gate GA | Done | `experiments/exp_binary_logic_ga.c` |
| Phase D: payload logic gate GA | Done | `experiments/exp_payload_logic_ga.c` |
| Phase D: wave logic gate GA | Done | `experiments/exp_wave_logic_ga.c` |
| Phase D: hybrid cross-grid logic GA | Done | `experiments/exp_hybrid_logic.c` |
| Delayed intent stepping with per-cell genomic rules | Done | `ca_core/engine.c` (`sys_step_intent_delayed_genomic`) |

## Open Questions

1. Should genomes be on the binary grid, payload grid, or both?
2. Should crossover happen between spatial neighbors or between coupled cells (cross-grid)?
3. What is the minimal viable genome size? (rule_select only = 5 bits)
4. Can genomes encode **topology** (which edges to connect) or only **parameters** (rule, weight)?
