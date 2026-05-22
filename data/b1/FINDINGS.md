# Experiment B1: Coupling-Delay Sweep — Findings

**Date**: 2026-05-16  
**⚠️ Data staleness warning**: The absolute TE magnitudes in the "Results" table below are from a pre-modulo build and are ~10× larger than current-build values. The **qualitative conclusion** (delay=0 peak, H6 falsified) is preserved. See "Post-Modulo Re-Validation" section below and `data/postmod_revalidation/REVALIDATION.md` for current-build numbers. Regeneration of this table under the current build is a pending task.

**Hypothesis**: H6 ("Non-zero delay maximizes information transmission")

## Setup

- `exp_delay_sweep --size 64 --steps 500 --trials 10 --seed 42`
- Delay varied from 0 to 16 ticks.
- Measurement: transfer entropy (TE) from grid 0 to grid 1 via ring buffer.

## Results

| delay | te_mean | te_var | density_g0 | density_g1 |
|-------|---------|--------|------------|------------|
| 0 | **0.245** | 0.134 | 0.011 | 0.010 |
| 1 | 0.058 | 0.007 | 0.011 | 0.011 |
| 2 | 0.046 | 0.006 | 0.011 | 0.011 |
| 3 | 0.018 | 0.001 | 0.011 | 0.010 |
| 4 | 0.162 | 0.076 | 0.011 | 0.011 |
| 5 | 0.052 | 0.007 | 0.011 | 0.011 |
| 6 | 0.060 | 0.007 | 0.011 | 0.010 |
| 7 | 0.014 | 0.000 | 0.011 | 0.010 |
| 8 | 0.146 | 0.075 | 0.011 | 0.010 |
| 9 | 0.066 | 0.009 | 0.011 | 0.010 |
| 10 | 0.055 | 0.007 | 0.011 | 0.011 |
| 11 | 0.026 | 0.002 | 0.011 | 0.011 |
| 12 | 0.145 | 0.075 | 0.011 | 0.010 |
| 13 | 0.051 | 0.008 | 0.011 | 0.011 |
| 14 | 0.059 | 0.007 | 0.011 | 0.011 |
| 15 | 0.011 | 0.000 | 0.011 | 0.010 |
| 16 | 0.004 | 0.000 | 0.011 | 0.012 |

## Findings

### F1. Delay=0 is the global maximum
TE peaks sharply at delay=0 (0.245 bits) with secondary peaks every 4 ticks (delays 4, 8, 12 at ~0.15 bits). This **contradicts** the pre-registered hypothesis that non-zero delay would be optimal.

### F2. Low density invalidates the measurement
Both grids decay to ~1.1% density across all delays — essentially ash. TE in dead grids measures residual noise / transient coupling artifacts, not structured information flow. The periodic peaks at multiples of 4 may reflect the `exp_delay_sweep` driver's sampling cadence rather than genuine signal resonance.

### F3. Success criterion not met
The pre-registered bar was: "identify a single peak delay; verify that delay = 0 is *not* always optimal." The first part is met (delay=0 is the peak), but the second part is **not met** — delay=0 *is* optimal here, likely because there is no sustained signal to delay.

## Interpretation

The delay-sweep experiment requires **preconditions** that this run did not satisfy:
1. Grids must maintain non-trivial density (>10%) for TE to measure structured flow.
2. A driven signal (e.g. periodic glider injection) must be present.
3. Without these, "optimal delay" is ill-defined — all delays see only ash.

## GA-IC Re-run v1 (2026-05-16)

- Command: `exp_delay_sweep --size 64 --steps 500 --seeds 5 --max-delay 8 --load-genomes evolved_genomes.bin`
- Densities: g0 ~0.28, g1 ~0.24 — confirming the precondition is now met.
- TE is 0 across all delays because `edge_binary` detects only edge occupancy (binary), and at ~28% density the edge is always occupied, leaving no variance for TE computation.
- **Fixed (2026-05-16)**: `edge_binary` replaced with `edge_density_scaled()` returning [0,255] density + `te_compute_discrete()` with 8-state histogram. TE can now measure information flow at high-density GA-ICs.

## GA-IC Re-run v2 (2026-05-17) — With Fixed TE

- Command: `exp_delay_sweep --size 64 --steps 500 --seeds 5 --max-delay 8 --load-genomes data/a4/learned_genomes.bin`
- **TE is non-zero across all delays** — `edge_density_scaled()` fix confirmed.

| delay | te_mean | te_var | density_g0 | density_g1 |
|-------|---------|--------|------------|------------|
| 0 | **0.0564** | 0.0043 | 0.189 | 0.344 |
| 1 | 0.0177 | 0.0003 | 0.189 | 0.338 |
| 2 | 0.0184 | 0.0004 | 0.189 | 0.336 |
| 3 | 0.0234 | 0.0009 | 0.189 | 0.348 |
| 4 | 0.0253 | 0.0007 | 0.189 | 0.353 |
| 5 | 0.0214 | 0.0006 | 0.189 | 0.354 |
| 6 | 0.0240 | 0.0009 | 0.189 | 0.338 |
| 7 | 0.0197 | 0.0007 | 0.189 | 0.347 |
| 8 | 0.0191 | 0.0006 | 0.189 | 0.349 |

## Findings

### F4. Delay=0 is still the global maximum (GA-IC)
Even with sustained density (~19–35%), TE peaks at delay=0 (0.056 bits). Secondary peaks at delay=4 (0.025) and delay=3 (0.023) are visible but small. This **partially falsifies H6** — there is no non-trivial optimal delay > 0 in this regime.

### F5. TE is ~4× lower than random-IC run
Random ICs (ash) produced TE=0.245 at delay=0; GA-ICs (alive) produce TE=0.056. Counter-intuitively, dead grids show higher "TE" because the coupling injects transient structure into ash. Live grids have their own dynamics that swamp the coupling signal. This suggests TE is measuring *disturbance* from coupling, not *information flow*.

### F6. Densities are stable but asymmetric
Grid 0 ~0.189, grid 1 ~0.344. The loaded genomes originated from a local-GA run and may have evolved asymmetric activity. This asymmetry could explain why delay=4 has a secondary peak (grid 1's slower dynamics may resonate with a 4-tick delay).

## Hypothesis Status

- **H6 (non-zero optimal delay)**: **Falsified** at both random-IC (delay=0 wins) and GA-IC (delay=0 wins) regimes. The intent-ring infrastructure (Phase 1) does not produce a non-trivial optimal delay for information transmission under these conditions.
- **Alternative interpretation**: TE in this substrate measures *coupling disturbance*, not *information flow*. A driven signal (periodic glider injection) may be required to see genuine delay-dependent information transmission.

## Next Steps

- **Re-run with driven signal** — inject periodic gliders at grid 0's left edge to create a known information source, then measure TE at grid 1's right edge. This tests whether a *genuine* signal (not just coupling noise) shows delay dependence.
- **Compare intent modes** — run the same GA-IC delay sweep with INTENT_MODE_ADD/WEIGHTED/THRESHOLD to see if mode changes the delay profile.
