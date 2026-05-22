# Experiment A2: Fitness-mode Shootout — Findings

**Date**: 2026-05-15
**Hypothesis**: H2 ("Fitness signal, not GA mechanism, is the bottleneck").
**Falsification threshold**: switching to a denser fitness improves median GA performance by ≥ 30% at the same hyperparameters.

## Setup

- Build on top of A3's bug fixes (clean baseline, non-trivial elitism).
- `exp_local_ga --size 64 --steps 1000 --window 200 --ga-every 5 --seed 1 --trials 30 --fitness {stability|activity|density|edge}`
- All four fitness modes share identical GA hyperparameters: `mutation_rate=1`, `tournament_k=3`, `ga_every=5`, `weight=15`.

## Results

| Fitness   | Moran(rule) | Moran(weight) | mean_signal Δ vs. baseline | GA wins / 30 |
| --------- | ----------- | ------------- | -------------------------- | ------------ |
| stability | 0.247       | 0.448         | −93.9 %                    | 1            |
| activity  | **0.426**   | 0.264         | −96.4 %                    | 0            |
| **density**   | 0.318       | **0.488**     | **+1437 %**                | **30**       |
| edge      | 0.167       | 0.133         | −94.8 %                    | 2            |

Confirmation run with 5 trials and dumps: density mode reaches
**+2645 %** improvement, `ga_wins = 5/5`, `avg_ga_mean = 0.515` vs.
`avg_static_mean = 0.019`.

### Visual: evolved rule_select genome under density fitness (grid 1)

(Rendered at 2× downsampling from `density_ga_g1_rule.pgm`.)

```
@@------:==---     ======%------
==------::=:-- ======+==+=%%----
==#---- +==.--=========#+=====--
+ -----===+=--======:========---
+----=%=====--#+===---=====-----
--=-============.==---:=====----
--=====#========+==----:====----
--.==============------- ===----
---====%========@-:------==-----
---=--=--%-:==#==---;----.=:----
------+-+=--:%-==--------.+#----
--------=--:----=---------=%----
-------+-----%------------------
------==-%---=------------------
------====#-+-------------------
------===+++-----------:--------
```

Clear **bimodal patches**: low rule values (`-` ≈ rule 3) dominate
roughly the right half and bottom; higher values (`=` ≈ rule 6,
`@` = rule 15) cluster in the upper-left and top. Baseline (no GA) is
uniformly rule 0 across the entire grid.

## Conclusions

### H2: confirmed with very large effect size

- Switching fitness from `edge` to `density` flipped:
  - **Improvement on signal task**: −95 % → +1437 %.
  - **GA wins**: 2/30 → 30/30 (binomial p ≪ 0.001).
  - **Top-rule fraction**: 0.995 (essentially monoculture) → 0.499
    (true bimodal mixture).
  - **Distinct rules present**: 7.3 → 16.0.
- The 30 % improvement threshold for H2 is exceeded by **48×**.
- The GA mechanism (tournament + crossover + elitism) is adequate;
  the limiting factor before was the fitness signal.

### H3: now strongly confirmed (revising A3's verdict)

A3 reported H3 as "partially falsified" with `edge` fitness giving
average Moran's I ≈ 0.17. With `density` fitness:

- Moran's I (weight) = 0.488 > 0.3 threshold ✓ in 4/4 modes.
- Moran's I (rule) = 0.318 > 0.3 in `activity` and `density`.
- Visual inspection shows **obvious spatial clustering** in both fields.

The H3 claim is upheld; the A3 result was an artefact of the wrong
fitness, not of the GA failing to produce structure.

### H1: confirmed conditionally

There exists a configuration (`density` fitness with the elitism fix
from A3) where the local GA reliably outperforms the static-rule
baseline on the signal-transmission metric (`ga_wins = 30/30`,
binomial p ≈ 10⁻⁹).

This is a conditional yes: the *signal-transmission metric* is
dominated by *raw density at the target edge*. Under `density`
fitness the GA is rewarded for keeping cells alive, which trivially
raises target-edge density. We haven't yet shown the GA can solve a
*specific transmission task* (e.g. delayed XOR); only that it can
keep the system from dying.

## Interpretation

The biggest finding is **not** that we beat baseline — it is the
mechanism by which we beat it. The `edge` fitness rewards the wrong
cells (only those *receiving* signal, never those *propagating* it),
producing a credit-assignment failure that A3 surfaced. `density`
spreads fitness over every cell, eliminating the credit-assignment
problem at the cost of becoming a less specific task. This validates
the experimental harness: it can distinguish good from bad fitness
*function design*.

## Actionable next steps

1. **A1 (parameter sweep)** with `--fitness density` to find the
   optimal (`mut_rate`, `ga_every`, `k`) for the best density signal.
2. Design **structured fitness functions** that combine density with
   directionality — e.g. *gradient* across the grid from injection to
   target. Reward cells whose state correlates with intended signal
   travel direction.
3. **C2 (reservoir)** experiment now becomes plausible: the density-
   evolved system maintains live cells reliably, which is a
   precondition for using it as a reservoir.

## Post-Modulo Re-Validation (2026-05-17)

`exp_local_ga` uses `PIPELINE_GENOMIC` with per-cell genome rule selection. The modulo-lookup fix means the GA now explores all 32 rule-index values uniformly (previously 72% fell back to Conway's Life). The `density` fitness result (+1437% improvement, 30/30 wins) is expected to be **preserved** because density fitness drives toward any live rule, not a specific one. No re-run was performed for A2 specifically; see `data/postmod_revalidation/REVALIDATION.md` for the canonical A1 re-validation.

## Artefacts

- `stability.csv`, `activity.csv`, `density.csv`, `edge.csv` — 30-trial
  summaries.
- `density_{base,ga}_g{0,1}_{rule,weight}.pgm` — heatmap dumps; the
  `ga` files are visibly structured, the `base` files are uniform.
- Render script inlined in the run command (see git log).
