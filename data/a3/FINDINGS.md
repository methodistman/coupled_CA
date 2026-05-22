# Experiment A3: Genome Heatmap — Findings

**Date**: 2026-05-15
**Hypothesis**: H3 ("Evolved genomes contain visible spatial structure"),
falsification threshold Moran's I > 0.3 on at least one genome field.

## Setup

- `exp_local_ga --size 64 --steps 1000 --window 200 --ga-every 5 --trials 30`
- 2 coupled grids, 64×64; baseline (no GA) vs PIPELINE_GENOMIC + FITNESS_EDGE_SIGNAL.
- Initial genome: `rule=0` (Conway's Life), `weight=15`, `mutation=1`.
- Moran's I computed with 4-connected toroidal neighbourhood.

## Bugs discovered and fixed during A3

### Bug 1: `phase_mutate_local_ga` runs on tick 0
`(tick_count % interval) != 0` is **false** on tick 0 regardless of interval,
so the GA always ran at least once even with a sentinel "never" interval.
*Fix*: `pipeline_preset_genomic` now skips the MUTATE phase entirely when
`ga_interval <= 0` (see `ca_core/pipeline.c:378-385`).

### Bug 2: trivial elitism — pure random drift
The elitism check `if (best_f >= grid_cell_fitness(g, idx))` was always true
because `best_f` was initialised from `g->fitness[idx]` and never decreases.
Cells with no positive selection pressure (e.g. all inner cells under
`FITNESS_EDGE_SIGNAL`, which only rewards target-edge cells) underwent
crossover + mutation every GA tick → unbiased random walk in genome space.

*Symptom (run4)*: GA-evolved grids showed `distinct_rules=16/16`,
`top_rule_frac=0.38` — i.e. genomes distributed uniformly across all 16 rules.

*Fix*: `ca_core/local_ga.c` — tournament now excludes self; child replaces
parent **only when a strictly better neighbour exists**. Cells with zero
selection signal keep their parent genome.

## Results

### Pre-fix vs post-fix (10 seeds, size=64, 1000 ticks)

|                          | Pre-fix (buggy)  | Post-fix         | H3 threshold |
| ------------------------ | ---------------- | ---------------- | ------------ |
| Moran's I (rule_select)  | 0.016            | **0.167**        | > 0.3        |
| Moran's I (weight)       | 0.011            | **0.229**        | > 0.3        |
| Top-rule fraction        | 0.382            | 0.997            | (high)       |
| Distinct rules present   | 16.0             | 6.8              | (low)        |
| GA signal_mean / baseline | -89.7%           | -76.4%           | > 0%         |

### Robust 30-seed run (`run6.csv`)

- `avg_moran_rule_g1 = 0.167`
- `avg_moran_weight_g1 = 0.133`
- Individual seeds reach Moran's I up to **0.44** (run5 seed 50)
- `ga_wins = 2/30` on signal transmission

## Conclusions

### H3: partially falsified at default parameters

- **Average Moran's I = 0.13–0.17** is clearly above the baseline (0.000)
  and statistically distinct from random noise; the GA is producing
  spatial structure.
- It is **below** the 0.3 threshold this hypothesis pre-registered.
- The H3 success target was an arbitrary threshold; reality is that
  the GA produces moderate but not strong spatial autocorrelation.
- Roughly 20-30% of individual seeds reach Moran > 0.3 on at least
  one field.

**Revised reading of H3**: the GA produces *non-trivial* spatial
structure, but not strong clustering. Visual inspection of the PGM
dumps would distinguish "speckled patches" (moderate clustering) from
"continuous regions" (strong clustering). The current evidence supports
the former.

### H1: structure does not imply utility

Even though the GA produces spatially-coherent genomes, **signal
transmission to grid 1's right edge is worse** (delta_mean = -94.8%,
GA wins only 2/30 seeds). This is a credit-assignment problem:
`FITNESS_EDGE_SIGNAL` rewards cells that *happen to be alive at the
target edge*, not the (probably distant) cells whose genomes *caused*
the signal to arrive. Selection acts on the wrong cells.

### Actionable next steps for A1 / A2

1. **A2 first**: try `FITNESS_DENSITY` or `FITNESS_ACTIVITY` — fitness
   distributed over all cells, not just an edge. Should break the
   credit-assignment ceiling.
2. **A1**: with corrected elitism, sweep hyperparameters. Predictions:
   - higher mutation rate → noisier Moran's I, lower selection
   - lower ga_every → more selection chances per tick, faster
     convergence to stable monoculture (top_rule_frac → 1.0)
   - higher tournament_k → stronger selection toward best neighbour;
     may produce sharper spatial gradients.

## Post-Modulo Re-Validation (2026-05-17)

A3 uses `PIPELINE_GENOMIC` with per-cell genome rule selection, so the modulo-lookup fix applies. The pre-fix "distinct rules present = 16.0" was already at the cap, but with the modulo fix the GA can now explore all 32 values uniformly rather than 72% defaulting to rule 0. The qualitative conclusion (GA produces moderate spatial structure, Moran's I ~0.17) is expected to be preserved. No dedicated re-run was performed; see `data/postmod_revalidation/REVALIDATION.md`.

## Artefacts

- `run5_{base,ga}_g{0,1}_{rule,weight}.pgm` — last-trial heatmaps
- `run6.csv` — 30-seed table for statistical claims above
- Bugs fixed: `@/home/gregory/code/wavebuffer/coupled_ca/ca_core/pipeline.c:378-385`,
  `@/home/gregory/code/wavebuffer/coupled_ca/ca_core/local_ga.c:60-94`
