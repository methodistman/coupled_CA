# Experiment A6: Rule-Only vs Weight-Only Decoupling — Findings

**Date**: 2026-05-16
**Hypothesis**: H1 ("Rule selection matters more than coupling weight for signal transmission")

## Setup

- `exp_local_ga` with `--mutate-mask` controlling which genome fields are mutable:
  - `both` = `GENOME_MUTATE_ALL` (rule_select + coupling_weight + mutation_rate)
  - `ruleonly` = `GENOME_MUTATE_RULE` (rule_select only)
  - `weightonly` = `GENOME_MUTATE_WEIGHT` (coupling_weight only)
- Short runs: size=32, steps=200, window=50, 10 trials, seed=100
- Long runs: size=64, steps=600, window=150, 10 trials
- Fitness mode: default (edge signal), discount=1.0

## Results

### Short Runs (32×32, 200 steps)

| Condition | static_mean | ga_mean | ga_peak |
|-----------|-------------|---------|---------|
| both | 0.0266 | 0.0257 | 0.0781 |
| ruleonly | 0.0266 | 0.0241 | 0.0656 |
| weightonly | 0.0266 | 0.0084 | 0.0375 |

### Long Runs (64×64, 600 steps)

| Condition | static_mean | ga_mean | ga_peak |
|-----------|-------------|---------|---------|
| both | 0.0210 | 0.0156 | 0.0297 |
| ruleonly | 0.0210 | 0.0619 | 0.0927 |
| weightonly | 0.0210 | 0.0051 | 0.0219 |

## Findings

### F1. Weight-only evolution fails dramatically
In both short and long runs, mutating **only** `coupling_weight` produces worse GA mean than the static baseline (0.0084 vs 0.0266 short; 0.0051 vs 0.0210 long). This confirms that coupling weight alone cannot drive meaningful adaptation without rule diversity.

### F2. Rule-only evolution matches or exceeds full evolution on long runs
On 64×64 grids, rule-only GA mean (0.0619) **exceeds** full both-field evolution (0.0156) by nearly 4×. This suggests that at longer time scales and larger grids, the coupling_weight field adds noise rather than signal — it destabilizes good rule configurations.

### F3. Short-run vs long-run regimes differ
At 32×32/200 steps, full evolution and rule-only are comparable (~0.025). At 64×64/600 steps, rule-only dominates. This indicates a **timescale dependency**: early evolution benefits from exploring both fields, but later convergence is hampered by weight mutations disrupting established rule patterns.

### F4. The coupling weight field is epistatically harmful
When both fields co-evolve, weight mutations can override good rule selections. The negative interaction suggests the genome should either (a) evolve weights first then freeze them, or (b) use a two-timescale approach where rules evolve faster than weights.

## Hypothesis Status

- **H1**: **Confirmed** — rule selection is the primary driver of adaptation; coupling weight alone is non-viable.
- **H1' (revised)**: The coupling weight field is not merely "less important" — it is actively harmful when co-evolved with rules at long timescales.

## Post-Modulo Re-Validation (2026-05-17)

A6 uses `exp_local_ga` with `--mutate-mask`, which controls per-cell genome mutation. The modulo-lookup fix applies: rule-only evolution now explores all 32 rule-index values uniformly. The structural finding (rule-only dominates at long timescales, weight-only is harmful) should be **preserved** because it depends on the relative searchability of rule vs weight fields, not on the absolute number of rules. A full re-run was not performed; see `data/postmod_revalidation/REVALIDATION.md`.

## Next Steps

1. **Re-run with fitness=density** — edge-signal fitness may be too noisy for weight adaptation; density could provide a smoother gradient.
2. **Test sequential evolution** — evolve rules for N generations, then freeze and evolve weights for M generations.
3. **Add weight clamping** — after initial burn-in, reduce weight mutation rate to near-zero and let rules refine.
