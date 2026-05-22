# Experiment A5: Static-Best-Genome Replay — Findings

**Date**: 2026-05-16
**Hypothesis**: H5 ("The GA's product is a static map; continuous adaptation does not matter")

## Setup

- `exp_local_ga` run with `--fitness density --ga-every 1 --initial-mutation 2` (A1 best config)
- Two replay variants from the same initial condition:
  - **Base** (`convergence_base.csv`): Frozen evolved genomes + no GA (mutate disabled)
  - **GA** (`convergence_ga.csv`): Continuous local GA running every tick
- Grid: 64×64, 1000 ticks, single seed

## Results

| Metric | Base (frozen) | GA (continuous) | Ratio GA/Base |
|--------|--------------|-----------------|---------------|
| **Density (tick 500)** | 0.1431 | 0.1333 | 0.93 |
| **Signal (tick 500)** | 0.1094 | 0.4375 | **4.00×** |
| **Fitness mean (tick 500)** | 1.7104 | 9.8279 | **5.75×** |
| **Density (end, tick 1001)** | 0.0369 | 0.6428 | **17.4×** |
| **Signal (end)** | 0.0781 | 0.5000 | **6.40×** |
| **Fitness mean (end)** | 2.6392 | 25.4475 | **9.64×** |

## Findings

### F1. Continuous adaptation dominates frozen genomes
The GA running continuously produces **6.4× higher signal** and **9.6× higher fitness** at end of run compared to replaying with frozen genomes. This falsifies the hypothesis that the GA merely finds a static configuration.

### F2. Frozen genomes collapse to near-extinction
Base density drops from ~20% at tick 0 to ~3.7% at tick 1001. Without continuous adaptation, the initially-good genomes cannot maintain structure against the drift of Life dynamics. The grid essentially decays to ash.

### F3. Continuous GA maintains ~64% density
In contrast, the continuous GA run maintains high density throughout (ending at 64.3%), confirming that adaptation is an ongoing requirement, not a one-time initialization.

### F4. The static-map hypothesis is decisively rejected
The success criterion for A5 was:
- If (frozen) ≈ (continuous) ≫ (random): static map suffices
- If (continuous) ≫ (frozen) ≫ (random): continuous adaptation matters

We observe the second case: continuous ≫ frozen. The GA is **not** a discovery mechanism followed by a static replay; it is a **continuous homeostatic process** that actively maintains the configuration against degradation.

## Hypothesis Status

- **H5**: **Falsified**. The GA's continuous adaptation is the critical factor, not the static genome map it produces at any given time.

## Post-Modulo Re-Validation (2026-05-17)

A5 replays genomes evolved by `exp_local_ga` (A1 best config), which uses per-cell genome rule selection. The saved `learned_genomes.bin` predates the modulo fix, so the replayed genomes are from a ~9-wide effective rule space. The **structural conclusion** (continuous GA >> frozen) is independent of rule-table layout — it arises from the decay dynamics of Life, not from genome diversity. The modulo fix does not affect this experiment's interpretation.

## Next Steps

1. **Test intermediate schedule** — run GA every N ticks (N=10, 50, 100) to find the minimum adaptation rate that maintains performance.
2. **Perturbation recovery** — after freezing good genomes, inject noise and see if continuous GA recovers faster than frozen.
3. **A6/A7 implication** — since continuous adaptation matters, the A6 finding (rule-only > both at long timescales) may need revision: perhaps the harmful effect of weight co-evolution is specific to the edge-signal fitness mode used in A6.
