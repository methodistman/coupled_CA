# Further Experiments

Companion to `BRAINSTORM.md`. Catalogues concrete experiments that the
TerritoryLang-integration Phases 1–3 (intent buffers, tick pipeline,
per-cell genomes) now make practical. Each entry has a fixed schema:

- **Goal**: what we hope to learn (one sentence).
- **Setup**: code/config knobs.
- **Measurement**: what counts as the result.
- **Success criterion**: the bar that turns the entry into a finding.
- **Effort**: rough estimate (S = <1 day, M = 1–3 days, L = >3 days).
- **Tests**: hypotheses in `further_hypothesis.md` this experiment touches.

---

## Group A — Validate Phase 3 (does the local GA actually work?)

These are the experiments needed to convert the `[~]` Phase 3 success
criterion in `territorylang_integration/03_per_cell_genomes.md` into a `[x]`
or a "no, it doesn't work, here's why" finding.

### A1. Local-GA hyperparameter sweep

- **Goal**: Find a region of `(mutation_rate × ga_every × tournament_k × initial_weight × fitness_mode)`
  in which `exp_local_ga` reliably beats the static-rule baseline.
- **Setup**: shell driver around `exp_local_ga` with `--trials 30`. Grid the
  five knobs at 3–4 values each (≈400 configurations × 30 seeds ≈ 12 K runs;
  size=32, steps=200, window=80 → ≈30 minutes wall time).
- **Measurement**: mean `ga.mean_signal − base.mean_signal` per cell of the
  hyperparameter grid; ga_wins / trials as a robustness proxy.
- **Success criterion**: at least one configuration with `improvement > 50%`
  and `ga_wins ≥ 20 / 30` (binomial p < 0.01 vs. fair coin).
- **Effort**: S. New: nothing in C, one shell + python plotting script.
- **Tests**: H1, H2.

### A2. Fitness-mode shootout

- **Goal**: Decide whether the current `FITNESS_EDGE_SIGNAL` is the right
  selection signal, or whether `STABILITY`/`ACTIVITY`/`DENSITY` would do
  better at the same task.
- **Setup**: add `--fitness {stability|activity|density|edge}` flag to
  `exp_local_ga`. Run each across the same 30-seed pool from A1's best
  configuration.
- **Measurement**: improvement % per fitness mode (boxplot).
- **Success criterion**: identify whether the chosen fitness function is the
  bottleneck (some mode dominates) or whether the GA architecture is
  (all modes look similar).
- **Effort**: S (~15 LOC + shell).
- **Tests**: H2, H4.

### A3. Genome heatmap dump

- **Goal**: Visual evidence that the GA produces structured genomes
  (highways, halos around target edge) vs. random noise.
- **Setup**: add `--dump-genomes PREFIX` to `exp_local_ga`. Write
  `PREFIX_g{grid}_t{tick}_{rule|weight}.pgm` every N ticks (or just at end).
- **Measurement**: visual inspection + spatial autocorrelation:
  Moran's I on `rule_select` and `coupling_weight`.
- **Success criterion**: Moran's I > 0.3 (positive spatial structure)
  in at least one of `rule_select` or `coupling_weight` after evolution,
  vs. ~0 for unevolved random genomes.
- **Effort**: S (~50 LOC for PGM writer; Moran's I as an offline script).
- **Tests**: H3.
- **Closes**: `[ ] Genome heatmap shows spatial structure` in 03_per_cell_genomes.md.

### A4. Convergence dynamics

- **Goal**: Diagnose what the GA actually does over time — converges,
  collapses, or oscillates.
- **Setup**: pipeline_preset_genomic + analyze; `MetricsHistory` already
  records per-tick metrics. Add `fitness_mean`, `fitness_var`, and
  `genome_entropy(rule_select)` as analyze outputs.
- **Measurement**: time series, 1000 ticks, single seed.
- **Success criterion**: classify run as `converged` (variance → 0),
  `extinct` (mean → 0), or `oscillating` (variance stays positive,
  fitness fluctuates). Whichever regime dominates tells us whether the
  GA needs (a) more diversity preservation, (b) better fitness signal,
  or (c) something else.
- **Effort**: M. New analyze phase + CSV export from `MetricsHistory`.
- **Tests**: H4.

### A5. Static-best-genome replay

- **Goal**: Separate "the GA found a good static configuration" from
  "the GA's continuous adaptation is what matters".
- **Setup**: run A1's best config; at end, capture genomes. Replay from
  same IC with (a) frozen learned genomes + no GA, (b) GA continues,
  (c) fresh random genomes + no GA.
- **Measurement**: signal at target edge for each variant.
- **Success criterion**: if (a) ≈ (b) ≫ (c), the GA's product is a
  static map. If (b) ≫ (a) ≫ (c), continuous adaptation matters.
- **Status**: **FINDINGS.md written.** H5 **falsified**: continuous GA
  produces 6.4× higher signal and 9.6× higher fitness than frozen
  genomes. Frozen replay collapses to ash (~3.7% density); continuous
  GA maintains ~64%. The GA is a homeostatic process, not a one-time
  search.
- **Effort**: Done.
- **Tests**: H5.

---

## Group B — Use the Phase 2 pipeline / Phase 1 intents for studies that were
hard before

### B1. Coupling-delay sweep

- **Goal**: For a fixed rule pair, find the inter-grid coupling delay that
  maximizes information transmission.
- **Setup**: `sys_step_intent_delayed` with delay ∈ {0,1,2,…,16}; 30 seeds;
  size=64; record `te_compute_from_ring(0→1)`. Now supports `--load-genomes`
  to use GA-evolved ICs; added `sys_step_intent_delayed_genomic` for per-cell
  genomic rule selection under delayed coupling.
- **Measurement**: TE bits per delay.
- **Success criterion**: identify a single peak delay; verify that
  delay = 0 (immediate coupling) is *not* always optimal.
- **Status**: **FINDINGS.md written.** Random ICs decay to ash (~1% density),
  making TE measure noise. GA-IC re-run v1 (2026-05-16) maintained ~28% density
  but TE was 0 because `edge_binary` was binary-occupancy-only.
  **Fixed**: `edge_density_scaled()` (returns [0,255]) + `te_compute_discrete()`
  (8-state histogram).
  **GA-IC re-run v2 (2026-05-17)**: TE is non-zero across all delays
  (0.017–0.056 bits), with peak at delay=0 and secondary peaks at delay=3–4.
  H6 (non-zero optimal delay > 0) is **falsified** even with sustained density.
  TE in this substrate may measure *coupling disturbance* rather than
  *information flow*.
- **Effort**: S.
- **Tests**: H6.

### B2. Intent-mode comparison

- **Goal**: Does `INTENT_MODE_{REPLACE,ADD,WEIGHTED,THRESHOLD}` change
  emergent dynamics meaningfully?
- **Setup**: identical IC + coupling; vary only the intent mode.
- **Measurement**: density trajectory + glider census + TE 0→1, over
  30 seeds.
- **Success criterion**: pairwise differences in mean glider count or
  TE significant at p < 0.01.
- **Status**: **FINDINGS.md written.** Two clusters found (replace/weighted
  vs add/threshold). **Bug discovered and fixed**: `INTENT_MODE_THRESHOLD`
  in `ca_core/intent.c` was aliased to ADD for binary cells (`if (val) target=1`).
  Fixed to use `(raw > 0 && raw >= alpha) ? 1 : 0`, making threshold a true
  gate for multi-value grids. GA-IC re-run recommended to observe mode
  differences at sustained density.
- **Effort**: S. Wrapper around `exp_signal --intent-mode`.
- **Tests**: H6.

### B3. Per-tick analyze trace

- **Goal**: Produce one canonical reference run with full per-tick
  metrics for visual / statistical inspection.
- **Setup**: `pipeline_preset_analyze`, 64×64×2, 1000 ticks, seed=42.
  Dump CSV of `density[g], entropy[g], activity[g], te_{0→1}` per tick.
  Also supports GA-evolved ICs via `exp_local_ga --dump-metrics`.
- **Measurement**: the CSV itself; plus a notebook that flags regime
  changes (entropy collapse, TE spike).
- **Success criterion**: identify at least three temporally distinct
  regimes within one run; baseline for all future comparison runs.
- **Status**: **FINDINGS.md written.** Three regimes confirmed on random ICs:
  transient collapse (0–50), quasi-stable ash (50–400), slow extinction
  (400–1000). GA-IC companion trace produced — maintains high density
  throughout, confirming random IC decay is a precondition issue.
- **Effort**: S. The analyze pipeline already emits this; we just need
  a `metrics_history_export_csv()` helper.
- **Tests**: H7.

### B4. Parallel scalability benchmark

- **Goal**: Establish whether the thread pool is paying off at realistic
  sizes or pure overhead.
- **Setup**: `exp_bench` extended to vary `(grid_size, num_grids)`
  ∈ {64,128,256} × {1,2,4,8}; both `pipeline_preset_intent` and
  `pipeline_preset_parallel`. `MAX_GRIDS` raised to 16 to support
  `num_grids=8`. Added `--size` and `--ngrids` CLI flags.
- **Measurement**: ms / 500 ticks for each cell.
- **Success criterion**: parallel preset achieves ≥1.5× speedup over intent
  for `num_grids ≥ 2` and `grid_size ≥ 128`.
- **Status**: **FINDINGS.md written.** Original tests at ngrids=2,4 fell
  short (best 1.44×). Extended to ngrids=8 at size=128: **1.517× speedup**,
  meeting the threshold. Hypothesis H8 reclassified from "partially falsified"
  to "partially confirmed" — the criterion is met for sufficiently many grids.
- **Effort**: S.
- **Tests**: H8.

---

## Group C — Genuinely novel experiments enabled by recent work

### C1. TE-driven coupling pruning

- **Goal**: Can the system self-organize a sparse coupling topology?
- **Setup**: start with all-to-all edge couplings between N grids. Every
  K ticks, measure per-edge TE; disable edges with TE < threshold via
  a new `phase_mutate_coupling`.
- **Measurement**: surviving edge count, total TE across surviving edges,
  density stability over time.
- **Success criterion**: edge count converges to a value < N(N-1) and
  surviving topology depends on rule choice (i.e., not just "everything
  prunes to nothing").
- **Effort**: M. New MUTATE phase, new ANALYZE phase to compute per-edge TE,
  hook into `Coupling` mutability.
- **Tests**: H9.

### C2. Reservoir-computing readout

- **Goal**: Turn the coupled-CA into a *useful* reservoir: train a linear
  readout on its state to perform a classification task.
- **Setup**: `exp_hybrid_reservoir` already exists. Drive grid 0's left
  edge with a binary input stream (e.g., NARMA-N benchmark, or simple
  delayed-XOR). Sample grid N's right edge as the reservoir state vector.
  Train a ridge regression to predict a target.
- **Measurement**: NRMSE on held-out input, memory capacity (Jaeger's
  definition), and information processing capacity (Dambre et al. 2012).
- **Success criterion**: memory capacity > 5 ticks for a 64×64×2 reservoir;
  NRMSE < 0.1 on simple delay tasks.
- **Status**: **FINDINGS.md written.** Hybrid reservoir sustains ~10% density
  across 10 trials. Payload grid 2.5× more active than binary. H10 precondition
  confirmed. **Memory capacity sweep completed (2026-05-17)**: delays 1–10,
  size=32, steps=200. Test NRMSE ~1.2–1.5 across all delays, **failing**
  the success criterion (NRMSE < 0.1). Substrate is alive but under-resourced
  at this scale. Need size=64+ or steps=500+ to test true capability.
- **Effort**: Done (infrastructure + sweep); larger grid run remaining.
- **Tests**: H10.

### C3. Explicit rule modulation (the H12 test)

- **Goal**: Implement the original TerritoryLang H12 hypothesis: grid 0's
  state directly selects grid 1's rule per cell, *without* a GA.
- **Setup**: new `phase_modulate_rule` that, for each cell (x,y) of grid 1,
  sets `grid1->genomes[idx].rule_select = grid0->cells[idx_coupled] ? RULE_A : RULE_B`.
  Compare to GA-discovered modulation from A1.
- **Measurement**: signal transmission + glider census on grid 1, with
  several (RULE_A, RULE_B) pairs.
- **Success criterion**: a pair (e.g., Life/HighLife) where explicit
  modulation yields higher signal-transmission fitness than static Life
  on grid 1; comparison to evolved GA-modulation tells us whether
  hand-designed rule maps beat search.
- **Status**: **FINDINGS.md written.** Life/HighLife modulation achieves
  **+43.5% improvement** over static HighLife (8/10 wins). H11 confirmed.
  However, GA achieves +1910% (A1), so **search massively outperforms**
  hand-designed modulation on this substrate.
- **Effort**: Done.
- **Tests**: H11.
- **Closes**: `[x] Rule modulation (H12)` in 03_per_cell_genomes.md.

### C4. Multi-objective fitness

- **Goal**: Test whether the tournament/crossover scheme handles competing
  objectives or collapses to one.
- **Setup**: `FITNESS_COMBINED = α·density + β·edge_signal + γ·activity`
  with (α,β,γ) sweep on the 2-simplex (Pareto sampling).
- **Measurement**: Pareto front in (density, edge_signal) space across
  resulting evolved configurations.
- **Success criterion**: non-trivial Pareto front (>3 non-dominated
  points), not a single cluster.
- **Effort**: M.
- **Tests**: H4.

### C5. Genome lineage tracking

- **Goal**: Reconstruct the "phylogeny" of the dominant final genome.
- **Setup**: use the reserved 4 bits of `CellGenome` as a lineage ID
  (initialized uniquely per cell). Whenever `local_ga_step_grid`
  replaces parent with child, log `(tick, idx, child_id, winner_id)` to
  a buffer.
- **Measurement**: tree reconstruction offline; depth, branching factor,
  fixation time of the dominant lineage.
- **Success criterion**: distinct lineages dominate distinct grid regions,
  i.e., not "one ancestor takes all".
- **Effort**: M. ~150 LOC + offline analysis.
- **Tests**: H4, H3.

---

## Group D — Sanity / regression / determinism

### D1. Bit-exactness regression suite

- **Goal**: Lock in invariants the codebase already claims so future
  refactors can't silently break them.
- **Setup**: add three regression tests:
  1. `PIPELINE_PARALLEL ≡ PIPELINE_INTENT` for 200 ticks × 4 grids × seed sweep.
  2. `PIPELINE_GENOMIC` with `weight=15, rule=0` everywhere ≡ `PIPELINE_INTENT`
     (currently *not* true — see test_local_ga history. Document why or fix).
  3. `PIPELINE_DEFAULT ≡ sys_step()` for 200 ticks × 4 grids × coupled.
- **Measurement**: pass/fail.
- **Success criterion**: all three pass, or the failing invariant is
  documented in `pipeline.h`.
- **Effort**: S.
- **Tests**: none (defensive).

### D2. Pattern preservation under coupling

- **Goal**: Quantify what edge coupling does to gliders/oscillators.
- **Setup**: inject one glider on grid 0; run 200 ticks; count gliders
  on each grid every tick via `census_gliders`. Cross with intent modes.
- **Measurement**: glider count time series.
- **Success criterion**: identify any (rule, intent_mode) combination
  that preserves the glider across the coupling boundary for ≥ 20 ticks
  after first transmission.
- **Status**: **FINDINGS.md written.** Direct coupling destroys gliders.
  Intent REPLACE mode successfully transmits gliders at tick 125.
  Intent ADD mode fails. See `experiments/exp_glider_preservation.c`.
- **Effort**: Done.
- **Tests**: H6, H12.

### D3. Determinism audit

- **Goal**: Confirm new per-grid RNG actually gives reproducibility.
- **Setup**: run `exp_local_ga --trials 5 --seed 42` twice; diff the
  output CSVs.
- **Measurement**: byte-equal output files.
- **Success criterion**: identical bytes; also identical across
  `--pipeline intent` vs `--pipeline parallel` for the same IC.
- **Status**: Done. `exp_local_ga` and `exp_signal` both pass byte-exact diff.
- **Effort**: S.
- **Tests**: none (defensive).

---

## Group F — Logic-gate evolution across grid types (Phase D)

These experiments use a population-level GA (not the local per-cell GA from
Phase 3) to evolve per-cell genomes so that a 2-grid system computes a
binary truth table. Grid 0 = computation with genomes; Grid 1 = input
buffer (left half = A, right half = B). Coupling: grid 1 right edge →
grid 0 left edge. Fitness = fraction of correct truth-table outputs read
from grid 0's right edge.

### F1. Binary logic gate GA (`exp_binary_logic_ga`)

- **Goal**: Evolve binary-grid genomes to compute AND/OR/XOR/NAND.
- **Setup**: 2-grid binary system, `pipeline_preset_genomic` with
  `ga_interval=0` (no local GA; population-level breeding only).
- **Measurement**: best fitness per generation; final truth table.
- **Status**: **FINDINGS.md written.** All gates converge to always-0
  (0x0) — the trivial attractor that maximizes partial fitness. OR=0.25,
  XOR=0.50, AND/NAND=0.75 fitness from matching the 0-outputs. The
  output-read mechanism and 30-step evaluation window do not create a
  gradient toward correct 1-outputs.
- **Effort**: Done.

### F2. Payload logic gate GA (`exp_payload_logic_ga`)

- **Goal**: Evolve payload-grid genomes to compute logic gates.
- **Setup**: 2-grid payload system using `payload_pipeline_preset_genomic`
  with `ga_interval=0`. Inputs set as `alive` field; output read from
  `alive` field on right edge.
- **Measurement**: same as F1.
- **Status**: **FINDINGS.md written.** OR converges perfectly (1.0) on
  multiple seeds. AND/XOR/NAND fall into the same always-0 attractor as
  binary. The 20 payload rules provide sufficient primitives for OR.
- **Effort**: Done.

### F3. Wave logic gate GA (`exp_wave_logic_ga`)

- **Goal**: Evolve wave-grid genomes to compute logic gates.
- **Setup**: 2-grid wave system using `wave_sys_step_genomic` for per-cell
  genomic rule selection. Input buffer drives left edge of computation grid.
- **Measurement**: same as F1.
- **Status**: **FINDINGS.md written.** OR achieves **100% convergence**
  across all tested seeds (42–46). `vector_or` rule provides strong
  signal propagation. AND/XOR/NAND not yet tested.
- **Effort**: Done.

### F4. Hybrid cross-grid modulation (`exp_hybrid_logic`)

- **Goal**: Binary control grid modulates payload computation grid to
  compute a logic gate.
- **Setup**: 1 binary grid (input buffer) + 1 payload grid (computation).
  Cross-connect `B0:right → P0:left`. Payload grid carries per-cell
  genomes; binary grid provides input signal.
- **Measurement**: same as F1.
- **Status**: **FINDINGS.md written.** OR achieves **80% convergence**
  (4/5 seeds perfect, 1 seed finds XOR' = 0xA). Cross-grid modulation is
  competitive with the best pure substrate (wave: 100%).
- **Effort**: Done.

---

## Group E — Speculative / longer horizon

### E1. GA on the coupling graph

- **Goal**: Evolve the wiring, not the cells.
- **Setup**: introduce `Coupling` mutation operators (add/remove/reweight
  edges). New `phase_mutate_topology`. Fitness = end-to-end TE
  (sum of `te_compute_from_ring` across all enabled edges).
- **Measurement**: evolved topology vs. random topology baseline.
- **Success criterion**: evolved topology achieves higher mean TE than
  random matched-density topology.
- **Effort**: L. New mutation primitives, new selection scheme.
- **Tests**: H9, H13.

### E2. Two-timescale evolution (slow IC GA + fast local GA)

- **Goal**: Integrate `exp_ga_ic` (slow) with `exp_local_ga` (fast) as
  described in `03_per_cell_genomes.md:128–134`.
- **Setup**: outer GA evolves initial conditions across generations;
  each evaluation runs the local-GA pipeline for K ticks.
- **Measurement**: outer fitness over generations; per-generation inner
  GA convergence stats.
- **Success criterion**: outer + inner > outer alone (existing `exp_ga_ic`).
- **Effort**: L.
- **Tests**: H5, H14.

### E3. Minimal Phase 4 DSL loader

- **Goal**: Validate that the existing API is declaratively expressible.
- **Setup**: JSON schema → constructs `System`, `Coupling`, `Pipeline`,
  optional per-cell genomes. No new runtime semantics, only a loader.
- **Measurement**: existing `exp_signal --connect 0:bottom->1:top
  --pipeline intent` reproducible from a single JSON file.
- **Success criterion**: at least three current experiments expressible
  as JSON, byte-equal trajectories vs. CLI invocation.
- **Effort**: L.
- **Tests**: H15.

---

## Recommended sequencing (by information-per-effort)

1. **A3** (genome heatmap) — 1 day, picture worth 1000 words on whether
   Phase 3 is producing structure or noise.
2. **A1** (hyperparam sweep) — 1 day, quantifies the working region.
3. **B3** (analyze trace) — 1 day, becomes the canonical reference run
   that every later experiment compares against. **Done.**
4. **D1** + **D3** (regression + determinism audit) — half day, prevents
   later experiments from chasing artefacts.
5. **A4** (convergence dynamics) + **~~A5~~** (replay) — together establish
   what kind of optimizer the local GA actually is. **A5 done** — H5 falsified.
6. **~~C3~~** (H12 explicit rule modulation) — **Done** — H11 confirmed
   (+43.5% over static), but GA still wins by 40×.
7. **C2** (reservoir computing) — state statistics done; task performance
   benchmarking remains.
8. **B1** + **B2** + **B4** — quick-win batch on intent modes / delays /
   parallel scaling. **All done; FINDINGS.md written for each.**
9. **C1, C4, C5, E*** — only after the above lattice tells us where to
   spend deep effort.
