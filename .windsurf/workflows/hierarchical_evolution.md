---
description: Hierarchical two-stage GA for multimodal polar logic
---

# Hierarchical Evolution: Orientation Highways + Logic Rules

## Stage 1: Evolve Orientation Highways

**Goal**: Find a per-cell orientation pattern that creates 4 clean signal highways (horizontal, vertical, anti-diagonal, diagonal) across the grid.

**Fitness**: Signal reachability, not logic truth tables.

```
For each mode m in {HORIZ, VERT, ADIAG, DIAG}:
  Inject signal at source corner for mode m
  Run CA for N steps with current orientation pattern
  Fitness_m = fraction of sink cells that received the signal
Overall fitness = min(Fitness_0, Fitness_1, Fitness_2, Fitness_3)
```

Why `min` not `sum`: we need ALL 4 highways to work. No mode can be ignored.

**Genome**: Only orientation fields mutate (rule fixed to Life, weight fixed to high value).

**Population**: 200-500 individuals, tournament selection.

**Termination**: All 4 modes achieve >0.95 reachability, or max generations.

## Stage 2: Fix Orientations, Evolve Rules

**Goal**: With the best orientation pattern from Stage 1 fixed, evolve rule_select and coupling_weight to compute correct logic on each highway.

**Fitness**: Same as current `exp_multimodal_polar_logic_ga` — shaped fitness across all 4 truth tables.

**Genome**: rule_select and coupling_weight mutate; orientations are frozen from Stage 1 winner.

**Population**: Same size as Stage 1.

**Termination**: Any mode reaches fitness >0.99, or max generations.

## Architecture

```
Stage 1 GA ──► best_orientation_pattern.bin
                    │
                    ▼
Stage 2 GA ──► load_orientations(fixed)
               evolve_rules_and_weights()
               ──► best_multimodal_genome.bin
```

## Key Files to Create

- `experiments/exp_polar_stage1.c` — orientation highway evolution
- `experiments/exp_polar_stage2.c` — rule evolution on fixed orientations
- `ca_core/genome.h` — add `GENOME_MUTATE_MODE_ORIENT_0..3` flags

## Success Criteria

- Stage 1: All 4 modes achieve >0.95 signal reachability
- Stage 2: At least one mode achieves perfect truth table (fitness=1.0)
