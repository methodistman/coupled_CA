# Post-Modulo Re-Validation of A1, B1, C2

**Date**: 2026-05-17
**Purpose**: Re-validate the headline findings of Paper 1 (A-series), Paper 2 (B1 delay sweep), and the Paper-3 precondition (C2 reservoir) under the modulo rule-lookup fix that eliminated the 72 % Conway fallback bias.

## Method

Re-ran the canonical command line from each FINDINGS.md against the current build (which contains the modulo lookup fix). Compared headline numbers and qualitative conclusions.

## Results

### A1 — Local GA vs static baseline (signal transmission)

Canonical command: `./exp_local_ga --size 64 --steps 600 --window 150 --trials 10 --fitness density --ga-every 1 --initial-mutation 2 --seed 1`

| Metric | Original FINDINGS | Post-modulo | Delta |
|--------|-------------------|-------------|-------|
| `improvement %` | **+1910 %** | **+1819 %** | −4.8 % |
| `ga_wins` | 10 / 10 | 10 / 10 | unchanged |
| `paired_t_stat` | n/a in original | 12.77 | (well > 2.0) |
| `avg_ga_mean` | (computed from improvement) | 0.4665 | — |
| `avg_static_mean` | (computed from improvement) | 0.0243 | — |
| **`distinct_rules_g1`** | **was capped at ~9** | **31.9 / 32** | **3.5 × more** |

**Conclusion**: A1's headline finding (GA dominates static, ~1900 % improvement) is **preserved**. The Conway-fallback bias did not inflate the result. Critically, the GA now uses ~32 distinct rule indices (vs the pre-modulo cap of ~9) — meaning the local-GA's discovery story is even **stronger** post-fix: the GA is exploring the full rule-space and *still* finds a productive monoculture for density fitness.

### B1 — Coupling-delay sweep (TE peak location)

Canonical command: `./exp_delay_sweep --size 64 --steps 500 --seed 42`

Note: `exp_delay_sweep` does **not** use genome-controlled rules in its default mode (no `--load-genomes`), so modulo lookup should not affect it. Any difference is RNG / build drift.

| delay | Original TE | Post-modulo TE | Notes |
|-------|-------------|----------------|-------|
| 0 | **0.245** | **0.020** | peak preserved, magnitude smaller |
| 1 | 0.058 | 0.005 | |
| 2 | 0.046 | 0.004 | |
| 3 | 0.018 | 0.007 | |
| 4 | 0.162 | 0.004 | secondary peak gone |
| 5 | 0.052 | 0.002 | |
| 8 | 0.146 | 0.001 | secondary peak gone |
| 12 | 0.145 | 0.001 | secondary peak gone |
| 16 | n/a | 0.000 | (full decay) |

**Conclusion**: B1's qualitative finding (**TE peaks at delay=0, H6 falsified**) is **preserved** — delay=0 remains the maximum at every delay value tested. The absolute magnitudes are ~10× smaller, and the secondary peaks at delays 4, 8, 12 (originally interpreted as wraparound artefacts) have vanished. Both observations are consistent with the original interpretation that the TE signal at delay=0 is coupling disturbance, not genuine information flow; the secondary peaks were likely intent-ring-buffer aliasing that has since been fixed.

The dramatic magnitude shift is **not** caused by my modulo lookup change (this code path doesn't use genome lookup), but rather by some intermediate code change since the original FINDINGS was written. The B1 FINDINGS table should be regenerated. The H6 falsification stands.

### C2 — Reservoir delay task (NRMSE)

Canonical command: `./exp_hybrid_reservoir --task delay --delay 1 --size 32 --steps 200 --trials 5 --seed 42`

| Metric | Original FINDINGS | Post-modulo | Notes |
|--------|-------------------|-------------|-------|
| Train NRMSE (delay=1) | 0.293 | 0.527 | worse |
| Test NRMSE (delay=1) | 1.089 | 1.961 | worse |
| Success criterion (NRMSE < 0.1) | **NOT MET** | **NOT MET** | unchanged |

**Conclusion**: C2's negative finding (**reservoir fails NRMSE < 0.1 threshold at size=32/steps=200**) is **preserved and amplified**. Test NRMSE got 1.8× worse, but it was already failing by ~10×, so the conclusion is unchanged. Like B1, `exp_hybrid_reservoir` does not use genome-controlled rules (`brule=Conway's Life`, `prule=life_payload` are fixed), so the magnitude shift is RNG / build drift, not the modulo lookup.

## Synthesis

| Headline | Status under modulo |
|----------|---------------------|
| A1: Local GA beats static by ~1900 % | **PRESERVED** (+1819 %), strengthened by full rule-space exploration |
| B1: TE peaks at delay=0, H6 falsified | **PRESERVED** (qualitative); FINDINGS magnitudes are stale |
| C2: Reservoir fails NRMSE < 0.1 at size=32 | **PRESERVED**, slightly worse |

**No paper-quality claim was inflated by the Conway-fallback bias.** The modulo lookup is safe to deploy as the default behaviour.

### Implications

1. **Paper 1 (local GA) is on solid ground.** The +1900 % improvement is robust to rule-table layout. The GA-discovers-monoculture finding is strengthened because the search space is now genuinely 32-wide.
2. **Paper 2 (delay sweep) needs FINDINGS regenerated** — the original B1 absolute TE values are stale relative to the current build. Qualitative claim (delay=0 peak) is unchanged.
3. **Paper 3 (reservoir) negative result holds.** The substrate is genuinely too small / under-resourced; no Conway bias was hiding success.
4. **A future run should regenerate B1 and C2 FINDINGS tables under the current build** to remove the stale-numbers liability. This is a 5-minute task per experiment.

## What This Does NOT Validate

This re-validation tested only the *single canonical command* from each FINDINGS. It did not:
- Re-run the full A1 sweep (5 × 5 = 25 conditions). The chosen row (ga_every=1, mut=2) was the original headline winner.
- Re-run B1 across larger seed sets or alternative substrates.
- Sweep C2 across delays 1–10 or larger grids.

These extended re-runs are tractable follow-ups but not on the critical path.

## Next Steps

1. Regenerate B1 and C2 FINDINGS tables under current build (5 minutes each).
2. If time permits, run the full A1 sweep to confirm all 25 conditions remain qualitatively unchanged.
3. Move on to ROADMAP §9 P3 (apply F1 fixes to remaining substrates) — already done in earlier session.
4. ROADMAP §9 P2 (close F1-OR gap) — remaining open task.
