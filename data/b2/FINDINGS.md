# Experiment B2: Intent-Mode Comparison — Findings

**Date**: 2026-05-16
**Hypothesis**: H6 ("Intent modes produce statistically distinct dynamics")

## Setup

- `exp_signal --size 64 --steps 500 --grids 2 --connect 0:bottom->1:top --pipeline analyze --intent-mode {replace,add,weighted,threshold}`
- 30 seeds per mode.
- Measurement: final density and activity on both grids.

## Results

| Mode | seeds | g0_density | g0_activity | g1_density | g1_activity |
|------|-------|------------|-------------|------------|-------------|
| replace | 30 | 0.0405 | 0.0194 | 0.0395 | 0.0159 |
| add | 30 | 0.0514 | 0.0268 | 0.0555 | 0.0298 |
| weighted | 30 | 0.0409 | 0.0179 | 0.0413 | 0.0193 |
| threshold | 30 | 0.0514 | 0.0268 | 0.0555 | 0.0298 |

## Findings

### F1. Two distinct clusters, not four
- **Cluster 1 (higher density/activity)**: `add` and `threshold` — identical statistics.
- **Cluster 2 (lower density/activity)**: `replace` and `weighted` — similar statistics.

### F2. Bug: `add` and `threshold` produce byte-identical outputs — **FIXED**
`md5sum add.csv == md5sum threshold.csv` (2f90c5c5...). Root cause: `intent.c` `INTENT_MODE_THRESHOLD` was `if (val) target = 1`, which for binary cells (0/1) is identical to ADD (`target = target OR val`).

**Fix applied** (`ca_core/intent.c`): THRESHOLD now uses `(raw > 0 && (float)raw >= alpha) ? 1 : 0`. For binary grids this makes no difference, but for multi-value grids (payload/wave) the threshold gate now correctly uses `alpha` as a threshold.

### F3. `replace` mode suppresses activity
`replace` has the lowest activity (0.0159 on g1) vs `add` (0.0298). This is expected: replace overwrites destination state rather than superimposing, reducing cell-turnover rate.

### F4. Density decay in all modes
All final densities are 4–6%, confirming the same ash-decay problem seen in B1. Without sustained structure, intent-mode differences are small and possibly artifactual.

## Hypothesis Status

- **H6**: **Partially confirmed for replace vs. add** (activity differs ~2×), but **confounded by the add/threshold bug** and the low-density regime.
- The pre-registered criterion ("pairwise differences significant at p < 0.01") is unverifiable without a proper statistical test, but the effect size between clusters is large enough (density: 0.040 vs 0.051 = ~25% relative) to be biologically meaningful if the system were alive.

## Next Steps

1. **~~Fix the threshold bug~~** Done in `ca_core/intent.c`.
2. **Re-run with GA-evolved ICs** (A1 best config) to maintain high density.
3. **Add TE measurement** to the B2 protocol — the pre-registered metric included TE and glider census, neither of which `--pipeline analyze` currently outputs.
