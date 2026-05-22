# Experiment B3: Per-Tick Analyze Trace — Findings

**Date**: 2026-05-16
**Hypothesis**: H7 ("At least three temporally distinct regimes within one run")

## Setup

- `exp_signal --size 64 --grids 2 --steps 1000 --seed 42 --pipeline analyze`
- Single canonical run (seed=42) with full per-tick density and activity.

## Results

The trace records `step,grid0_density,grid0_activity,grid1_density,grid1_activity` for 1000 ticks.

Key regime boundaries identified from the trajectory:

| Regime | Ticks | g0_density | g1_density | Character |
|--------|-------|------------|------------|-----------|
| **I. Transient collapse** | 0–~50 | 0.195 → 0.095 | 0.205 → 0.115 | Rapid decay from random IC; activity spikes then falls |
| **II. Quasi-stable ash** | ~50–~400 | 0.07–0.10 | 0.06–0.10 | Oscillatory; density fluctuates ±0.02; no long-term trend |
| **III. Slow extinction** | ~400–1000 | 0.06 → 0.03 | 0.05 → 0.03 | Monotonic decline toward static equilibrium |

## Findings

### F1. Three regimes confirmed
The run does exhibit at least three qualitatively distinct temporal phases, satisfying the pre-registered success criterion.

### F2. No regime corresponds to "computation"
None of the three regimes show sustained structure, glider transit, or information flow. This is a **baseline trace of decay**, not a reference run of emergent dynamics. The canonical reference run should use:
- A1's best GA-evolved ICs (maintain ~64% density), or
- A periodic glider injector (driven signal), or
- A known stable configuration (e.g. puffer train, memory loop)

### F3. Regime II is the most informative
The quasi-stable ash phase (ticks 50–400) shows the highest activity-to-density ratio, suggesting transient local structures (blinkers, small still lifes) that persist before eventual extinction. This is where coupling effects would be most detectable.

### F4. Activity leads density
Grid activity peaks in the first 5–10 ticks and then decays faster than density. Activity is a more sensitive early-warning indicator of regime change than density.

## Interpretation

This trace confirms that random-IC Life on a 64×64 grid is fundamentally a **decay process** with three phases: initial collapse, statistical equilibrium of small debris, and terminal freeze. Any experiment using random ICs as the baseline must account for this trajectory.

The trace is still valuable as a **negative control**: it defines what "no computation" looks like, against which GA-evolved or driven runs can be compared.

## GA-IC Trace (2026-05-16)

- Command: `exp_local_ga --size 64 --steps 1000 --window 200 --trials 1 --fitness density --ga-every 1 --initial-mutation 2 --seed 42 --dump-metrics trace_ga.csv`
- Output: `trace_ga.csv_ga.csv` (1000 ticks, 14 columns including density, entropy, activity, gliders, oscillators, TE matrix).
- **Key difference**: GA-IC trace starts with high density (~0.20) and maintains it, whereas random IC trace collapses to ash. This confirms the precondition issue identified in B1.

**Post-modulo note**: The GA-IC trace uses `exp_local_ga` with per-cell genome rule selection. The modulo-lookup fix means the evolved genomes now explore all 32 rule-index values uniformly. The structural finding (GA maintains high density where random ICs decay) is **preserved** because it depends on the GA's ability to find *any* live configuration, not a specific rule.

## Artefacts

- `data/b3/trace.csv` — 1000-tick canonical reference (random ICs).
- `data/b3/trace_ga.csv_ga.csv` — 1000-tick GA-evolved reference.

## Next Steps

1. **~~Produce GA-IC trace~~** Done.
2. Add TE (0→1) and glider census columns to the trace output.
3. Use the two traces (random vs GA-evolved) as Figure 1 for Paper 2.
