# Experiment C2: Reservoir Computing with Coupled CA — Findings

**Date**: 2026-05-16  
**⚠️ Data staleness warning**: The NRMSE values in the "Memory Capacity Sweep" table below are from a pre-modulo build. A post-modulo re-run shows similar or slightly worse NRMSE (~1.96 for delay=1 test). The **qualitative conclusion** (reservoir fails NRMSE < 0.1 at size=32/steps=200) is preserved. See `data/postmod_revalidation/REVALIDATION.md` and regenerate under current build for updated absolute values.

**Hypothesis**: H10 ("Density-evolved system maintains live cells — precondition for reservoir")

## Setup

- `exp_hybrid_reservoir` with ridge regression readout on hybrid binary+payload system
- Binary grid: Conway's Life; Payload grid: `life_payload` rule
- Task: `delay` (predict input delayed by N ticks) and `xor`
- Size: 32×32, steps: 100, trials: 10, seeds 42–51

## Results

### Reservoir State Statistics (from `hybrid_reservoir.csv`)

| Metric | Binary Grid Mean | Binary Std | Payload Grid Mean | Payload Std |
|--------|-----------------|------------|-------------------|-------------|
| Density | 0.0971 | 0.0229 | 0.1034 | 0.0273 |
| Entropy | 0.4397 | 0.0449 | 0.4754 | 0.0838 |
| Activity | 0.0705 | 0.0217 | 0.1798 | 0.0294 |

### Task Performance

**No task-performance CSV exists.** The ridge regression readout metrics (NRMSE, memory capacity, ridge weights) are logged to stdout during `exp_hybrid_reservoir` runs. The `hybrid_reservoir.csv` file contains only per-trial reservoir state statistics (mean/variance of density, entropy, activity), not readout accuracy.

To obtain task performance, re-run with stdout capture:
```bash
./exp_hybrid_reservoir --task delay --delay 1 --size 32 --steps 100 --trials 10 --seed 42 > reservoir_delay.txt 2>&1
./exp_hybrid_reservoir --task xor --size 32 --steps 100 --trials 10 --seed 42 > reservoir_xor.txt 2>&1
```

## Findings

### F1. Reservoir maintains non-trivial density (~10%)
Both binary and payload grids sustain ~10% density across all 10 trials. This confirms the precondition for reservoir computing: the system is "alive" and produces dynamic, non-stationary features.

### F2. Payload grid is more active than binary
Payload activity (0.18) is ~2.5× higher than binary activity (0.07). The payload grid's `life_payload` rule (which carries 8-bit payload values) produces more cell-turnover than pure binary Life. Higher activity = richer feature space for the readout.

### F3. Low variance across trials
Standard deviations are modest (~20–30% of means), suggesting the reservoir dynamics are relatively stable across different random initial conditions. This is desirable for reproducible readout training.

### F4. Precondition confirmed, but task performance unmeasured
H10 states that density-evolved systems maintain live cells. This is confirmed. However, the **success criterion** for C2 (memory capacity > 5 ticks, NRMSE < 0.1) requires task-performance data that is not in the current CSV.

## Hypothesis Status

- **H10 (precondition)**: **Confirmed**. Reservoir maintains ~10% density across trials.
- **C2 task performance**: **Now measurable** via `--output-perf FILE` flag (added 2026-05-16). Smoke-tested NRMSE values for delay task. Memory capacity sweep (delays 1–10) still pending.

## Memory Capacity Sweep (2026-05-17)

Run: delays 1–10, size=32, steps=200, trials=5, seeds 42–46, `--output-perf`.

| Delay | Train NRMSE (mean) | Train Std | Test NRMSE (mean) | Test Std |
|-------|-------------------|-----------|-------------------|----------|
| 1 | 0.293 | 0.271 | 1.089 | 1.043 |
| 2 | 0.320 | 0.304 | 1.286 | 1.201 |
| 3 | 0.358 | 0.334 | 1.248 | 1.159 |
| 4 | 0.313 | 0.292 | 1.519 | 1.486 |
| 5 | 0.311 | 0.299 | 1.376 | 1.252 |
| 6 | 0.309 | 0.294 | 1.257 | 1.201 |
| 7 | 0.333 | 0.314 | 1.246 | 1.156 |
| 8 | 0.335 | 0.321 | 1.129 | 1.027 |
| 9 | 0.337 | 0.317 | 1.333 | 1.321 |
| 10 | 0.344 | 0.329 | 1.219 | 1.114 |

### F5. Test NRMSE >> 0.1 across all delays
The success criterion (NRMSE < 0.1) is **not met** at size=32, steps=200. Test NRMSE ranges 1.1–1.5, ~10× above threshold. Train NRMSE (~0.3) is also above 0.1, so the reservoir itself is not producing separable features at this scale.

### F6. Slight delay-dependence, no clear peak
Test NRMSE dips at delay=1 (1.089) and delay=8 (1.129), with a peak at delay=4 (1.519). The effect is weak (1.4× variation) and masked by high trial-to-trial variance. No clear "memory capacity cutoff" is visible — the reservoir is too small or the readout too shallow to show a characteristic forgetting curve.

### F7. Overfitting gap
Train–test gap is ~3–4×, indicating overfitting to the training window. Possible causes: (a) 200 steps is too short for stable reservoir dynamics, (b) 64 sampled features from 32×32 grids is too few, (c) ridge lambda=0.01 may be too weak.

## Hypothesis Status

- **H10 (precondition)**: **Confirmed**. Reservoir maintains ~10% density across trials.
- **C2 task performance**: **Measured** — NRMSE ~1.2, **fails** success criterion (NRMSE < 0.1) at size=32/steps=200. Need larger grid (64×64) or more steps (500+) to test if the substrate is capable.

## Next Steps

1. **Re-run at size=64, steps=500** — the current sweep may be under-resourced.
2. **Test XOR task** — `./exp_hybrid_reservoir --task xor --output-perf xor_perf.csv`
3. **Tune ridge lambda** — sweep lambda ∈ {0.001, 0.01, 0.1, 1.0} at fixed delay=1.
4. **Compare binary vs payload-only reservoir** — isolate which grid type carries the signal.
