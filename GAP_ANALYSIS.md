# Project Gap Analysis: Documentation vs. Implementation

**Date**: 2026-05-16

This document maps every claimed feature, experiment, and architectural direction across the project's documentation against the actual codebase, identifying what exists, what is partially done, and what is entirely missing.

---

## 1. BRAINSTORM.md Directions (8 total)

| Direction | Claimed | Implemented | Gap |
|-----------|---------|-------------|-----|
| **D1**: GA Search for Computational ICs | Signal transmission fidelity (`exp_ga_signal`), memory capacity (`exp_ga_memory`), AND/OR/XOR gates (Phase D) | `exp_binary_logic_ga`, `exp_payload_logic_ga`, `exp_wave_logic_ga`, `exp_hybrid_logic` exist. **No** `exp_ga_signal.c` or `exp_ga_memory.c`. Signal transmission is tested via `exp_signal` and `exp_local_ga`, but not as a standalone GA on ICs. | Missing: `exp_ga_signal.c`, `exp_ga_memory.c` |
| **D2**: Fractional-Scale Coupling | 4 scale modes (nearest, mean, max, proportional) in `coupling.c` | `coupling.c` has `SCALE_WRAP`, `SCALE_MEAN`, `SCALE_MAX`, `SCALE_PROPORTIONAL`. `hybrid_coupling.c` also supports scale. | ✅ Complete |
| **D3**: Geometric Transformations | Triangular lattice, hexagonal lattice, rotated/twisted coupling, transform matrix in `EdgeConn` | None implemented. `EdgeConn` only has `target_grid`, `target_edge`, `scale_mode`. No `rotate`, `reflect`, `offset` fields. | Missing: All of D3. Only mentioned in `BRAINSTORM.md`; no code exists. |
| **D4**: Better Metrics | Transfer entropy ✅, integrated information (Φ), effective measure complexity, connected components, pattern velocity field, reversible logic test, functional dependency | TE: `metrics/transfer_entropy.c` ✅. Glider/oscillator census: `ca_core/patterns.c` ✅. **Φ, EMC, connected components, velocity field, reversible logic, functional dependency** — none implemented. | Partial (2/8 metrics) |
| **D5**: Rule Modulation | Binary controls payload rule, payload controls binary rule, dynamic schedules as programs | `phase_modulate_rule` in `pipeline.c` implements Life/HighLife modulation. **No** payload-controlled binary rule. **No** dynamic rule schedule programs. `schedule.c` has cycle/random/shuffle but not programmable sequences. | Partial (1/3 concepts) |
| **D6**: Embodied/Geometric Computation | Cellular wire experiment, logic by collision experiment | None implemented. No `exp_cellular_wire.c` or `exp_collision_logic.c`. D2 (`exp_glider_preservation`) is the closest — it tests glider transmission across coupling, but not the full wire/collision concepts. | Missing (0/2 experiments) |
| **D7**: Topology Search | Topology genome encoding grid count, sizes, types, rules, connection graph, mutation operators | None implemented. No `exp_ga_topology.c`. `Coupling` struct is fixed-size; no mutation operators exist. `MAX_GRIDS` was raised to 16 but topology is still static. | Missing entirely |
| **D8**: Coarse-Graining/Renormalization | Coarse-grain grid every 100 steps, test scale-invariant computation | None implemented. No `exp_renormalization.c`. | Missing entirely |
| **D9**: Polar/Anisotropic Coupling | Orientation-encoded genomes, anisotropic neighbor weighting, spatial programmability | `genome.h` orientation macros ✅, `pipeline.c` anisotropic kernel ✅, `test_orientation.c` verifies behavior. 48-bit genome expansion adds 4 mode orientations + directional rule fields. | Partial (kernel done, experiment still tuning) |

**BRAINSTORM.md summary**: 2/9 directions fully implemented (D1 partial, D2 complete, D3-D8 mostly missing, D9 experiment file only). The remaining items list is now mostly done (rule modulation ✅, TE ✅), but geometric transforms, topology search, coarse-graining, and embodied computation remain entirely unimplemented.

---

## 2. further_experiments.md (26 experiments)

### Group A — Validate Phase 3 (7 experiments)

| ID | Status | Files | Data | FINDINGS | Notes |
|----|--------|-------|------|----------|-------|
| A1 | ✅ | `exp_local_ga.c` | `data/a1/` | ✅ | 34 txt files |
| A2 | ✅ | `exp_local_ga.c` | `data/a2/` | ✅ | 8 pgm, 3 csv |
| A3 | ✅ | `exp_local_ga.c` | `data/a3/` | ✅ | 47 pgm, 5 csv |
| A4 | ✅ | `exp_convergence.c` | `data/a4/` | ✅ | 2 csv, 1 bin |
| A5 | ✅ | `exp_local_ga.c` | `data/a4/convergence_{base,ga}.csv` | ✅ | H5 falsified |
| A6 | ✅ | — | `long_results/results_*decouple*.txt` | ✅ | 6 files parsed |
| A7 | ✅ | — | `long_results/results_*discount*.txt` | ✅ | 9 files parsed |

**Group A**: 7/7 complete. No gaps.

### Group B — Pipeline / Intent (4 experiments)

| ID | Status | Files | Data | FINDINGS | Notes |
|----|--------|-------|------|----------|-------|
| B1 | ✅ | `exp_delay_sweep.c` | `data/b1/` | ✅ | `--load-genomes` added; `sys_step_intent_delayed_genomic` added |
| B2 | ✅ | `exp_signal.c` | `data/b2/` | ✅ | Threshold bug fixed |
| B3 | ✅ | `exp_signal.c`/`exp_local_ga.c` | `data/b3/` | ✅ | GA-IC companion trace |
| B4 | ✅ | `exp_bench.c` | `data/b4/` | ✅ | `MAX_GRIDS` 4→16; ngrids=8 achieves 1.517× |

**Group B**: 4/4 complete. No gaps.

### Group C — Novel Experiments (5 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| C1 | ❌ | None | None | ❌ | **Completely missing**. No `phase_mutate_coupling`, no per-edge TE analyze, no `Coupling` mutability hooks. |
| C2 | 🟡 | `exp_hybrid_reservoir.c` | `long_results/hybrid_reservoir.csv` | ✅ | Ridge regression solver exists in C. **Task performance metrics (NRMSE, memory capacity) not exported to CSV.** Only state statistics (density, entropy, activity) are saved. |
| C3 | ✅ | `exp_rule_mod.c` | `data/c3/rule_mod.txt` | ✅ | +43.5% improvement confirmed |
| C4 | ❌ | None | None | ❌ | **Completely missing**. No `FITNESS_COMBINED`, no Pareto front tracking. |
| C5 | ❌ | None | None | ❌ | **Completely missing**. Lineage ID in reserved bits not used. No phylogeny logging buffer. |

**Group C**: 1/5 fully complete, 1 partial, 3 missing.

### Group D — Sanity / Regression (3 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| D1 | 🟡 | `tests/test_pipeline*.c` | — | ❌ | Tests exist (`test_pipeline`, `test_pipeline_parallel`) but no formal regression suite report. D1 calls for 3 specific invariants: `PARALLEL ≡ INTENT`, `GENOMIC ≡ INTENT`, `DEFAULT ≡ sys_step`. These are not systematically tested. |
| D2 | ✅ | `exp_glider_preservation.c` | `data/d2/` | ✅ | New experiment created |
| D3 | ✅ | `exp_local_ga.c`, `exp_signal.c` | Implicit | Mentioned | Byte-exact diff confirmed |

**Group D**: 2/3 complete, 1 partial.

### Group E — Speculative (3 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| E1 | ❌ | None | None | ❌ | **Missing**. No topology GA. `Coupling` graph is static. |
| E2 | ❌ | None | None | ❌ | **Missing**. No two-timescale evolution. `exp_ga_ic` exists (outer GA on ICs) but is not integrated with `exp_local_ga` (inner local GA). |
| E3 | 🟡 | `dsl/` parser+bind+codegen | — | ❌ | Parser, binder, and codegen exist (`dsl/parser.c`, `dsl/bind.c`, `dsl/codegen.c`). **No JSON loader.** No experiment can be reproduced from a JSON file. The DSL grammar in `dsl/parser.c` parses topology scripts, not the TerritoryLang dialect envisioned in `04_territorylang_dsl.md`. |

**Group E**: 0/3 complete, 1 partial, 2 missing.

### Group F — Logic-Gate Evolution (4 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| F1 | ✅ | `exp_binary_logic_ga.c` | `data/f1/` | ✅ | Always-0 attractor identified |
| F2 | ✅ | `exp_payload_logic_ga.c` | `data/f2/` | ✅ | OR converges; AND/XOR fail |
| F3 | ✅ | `exp_wave_logic_ga.c` | `data/f3/` | ✅ | 100% OR convergence |
| F4 | ✅ | `exp_hybrid_logic.c` | `data/f4/` | ✅ | 80% OR convergence |

**Group F**: 4/4 complete. No gaps.

### Group G — Turing Completeness & Composition (3 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| G1 | ✅ | `exp_ruleset_profile.c` | `data/ruleset_profile_*.csv` | ✅ | Ruleset density/propagation profiling. Binary: Life Without Death (0.717), Rule 110 (0.551) maintain density. Payload: many rules saturate to ~1.0. Wave: vector_* rules saturate. |
| G2 | ✅ | `exp_compositional_logic.c` | `data/gate_*.bin` | 🟡 | Chained AND→XOR: 3/4 accuracy. Composition works partially; single-edge coupling limits compositional semantics. Need multi-input wiring or evolved topology. |
| G3 | 🟡 | `exp_rule110_tag.c` | — | 🟡 | 1D Rule 110 engine validated. Full cyclic tag system encoding (Cook construction) not yet implemented. |

### Group H — Spatial Programmability (new, 5 experiments)

| ID | Status | Files | Data | FINDINGS | Gap |
|----|--------|-------|------|----------|-----|
| H1 | 🟡 | `exp_multimodal_logic_ga.c` | `data/mm_*.bin`, `data/mm_*.log` | — | 2-mode binary multimodal GA. Plateaued at ~0.87 fitness. |
| H2 | 🟡 | `exp_multimodal_wave_logic_ga.c` | `data/mm_wave_*.bin`, `data/mm_wave_*.log` | — | 2-mode wave multimodal GA. Underperformed (~0.29 fitness). |
| H3 | 🟡 | `exp_multimodal_polar_logic_ga.c` | — | — | 4-mode polar/anisotropic. DIAG reaches 0.34 fitness; HORIZ/VERT/ADIAG stuck at 0.125. |
| H4 | ✅ | `genome.h` | — | — | Orientation macros implemented. 3 reserved bits [15:13] repurposed as 8-direction index. |
| H5 | ✅ | `pipeline.c` | `tests/test_orientation.c` | — | Anisotropic kernel with `ALIGNMENT_SCALE[8][8]` lookup. Verified statistically. |
| H6 | 🟡 | `genome.h/c`, `pipeline.c` | — | — | 48-bit genome: 4 mode orientations + alt_rule + dir_mask. Kernel reads mode-specific orientations. Directional rule switching implemented. |
| H7 | 🟡 | `exp_hierarchical.c` | `data/h7_run.csv` | — | Two-stage GA implemented. Stage 1 fitness flat-zero: hardcoded `RULE_TABLE` cannot propagate thin signals under anisotropic gating. **Superseded by H9 (composable ruleset)**. |
| H8 | 🟡 | `exp_directional_multimodal.c` | `data/h8_3333_gens.csv` | `data/h8_FINDINGS.md` | 3333 gens: peak 0.875, mean 0.726. XOR in individual modes 22-48% of time but all-4-XOR never achieved. 2.5x better than H3. |
| H9 | 🟡 | `ca_core/composable_ruleset.{h,c}`, `tests/test_composable_ruleset.c`, `exp_composable_stage0.c` | `data/composable_stage0.csv`, `data/composable_R_star.bin` | `COMPOSABLE_RULESET_ROADMAP.md` | Stage 0 implemented & tested (7/7 unit tests pass). Orientation-sensitivity-weighted fitness avoids trivial-fill collapse. Smoke run: fitness 0.27 → 0.83 in 20 gens. Full run (pop=100, gens=100, K=8, size=32) in progress. Stages 1 & 2 pending. |

**Group H summary**: 2/9 complete (H4, H5). H6 kernel done. H7 first build exposed root cause: hardcoded rule table inadequate for orientation-gated propagation. **H9 resolves this**: 56-bit composable ruleset DNA evolved by Stage 0 GA with orientation-sensitivity fitness; smoke test reaches 0.83. H8: peak 0.875 with directional rules but no XOR co-occurrence. H3: DIAG 0.34, others stuck.

---

## 3. TerritoryLang Integration (4 Phases)

### Phase 1: Intent Buffers ✅
- `ca_core/intent.c/h` — `IntentBuffer`, `EdgeIntent`, `IntentRing`
- `sys_step_intent()` — compute→apply→swap→capture
- 4 consumption modes: REPLACE, ADD, WEIGHTED, THRESHOLD (bug fixed)
- Temporal ring buffer with configurable delay
- Transfer entropy computation module
- **Status**: Complete

### Phase 2: Tick Pipeline ✅
- `ca_core/pipeline.c/h` — PHASE_RUN/EXCHANGE/ANALYZE/MUTATE
- Topological executor with dependency resolution
- Presets: default, intent, analyze, parallel, genomic
- Thread pool for parallel grid updates
- `MetricsHistory` for per-tick metrics
- **Status**: Complete

### Phase 3: Per-Cell Genomes ✅
- `ca_core/genome.h/c` — 48-bit `CellGenome` (8-bit rule, 8-bit weight, 8-bit mutation, 4×3-bit mode orientations, 4-bit alt_rule, 4-bit dir_mask)
- `Grid->genomes` parallel array
- `grid_step_kernel()` with `KERNEL_USE_GENOMES` flag
- `phase_grid_step_genomic` — per-cell rule + weighted coupling
- `phase_analyze_local_fitness` — per-tick fitness accumulator
- `phase_mutate_local_ga` — tournament + crossover + mutation
- `pipeline_preset_genomic()` wiring
- Per-grid stateful RNG (`rng_splitmix64`)
- `--dump-genomes` PGM export
- Rule modulation (`phase_modulate_rule`)
- Phase C: 5-bit rule_select, payload/wave genomic rules
- Phase D: 4 logic-gate GA experiments
- **Status**: Complete

### Phase 4: TerritoryLang DSL 🟡
- `dsl/parser.c` — parses grid/connect/steps/seed topology scripts
- `dsl/bind.c` — binds DSL to `System` / `PayloadSystem`
- `dsl/codegen.c` — Verilog and C code generation stubs
- **Missing**: No JSON loader. No `TL_WorldProgram` struct. No parser for the TerritoryLang dialect shown in `04_territorylang_dsl.md`. The existing DSL is a simple imperative script language, not the declarative `world { territory ... coupling ... tick_pipeline ... }` format.
- **Gap severity**: High. Phase 4 is entirely speculative — the DSL grammar exists but does not match the TerritoryLang vision.

---

## 4. PAPER_PLAN.md (7 papers)

| Paper | Core | Status | Blockers |
|-------|------|--------|----------|
| **Paper 1** (Local GA) | A1–A7, H1–H5 | **Ready for draft** | None. All data collected, all FINDINGS.md written. |
| **Paper 2** (Info Flow) | B1–B4, H6–H8 | **Ready for draft** | B1 TE=0 with GA-ICs (edge_binary is binary-occupancy-only). Fixable with edge-density TE. |
| **Paper 3** (Reservoir) | C2, H10 | **Partial** | Task performance unmeasured. Need `--output-perf` flag or stdout capture. |
| **Paper 4** (Design vs Search) | C3, H11 | **Ready for draft** | None. +43.5% modulation vs +1910% GA is a clear story. |
| **Paper 5** (Topology) | H9, H13, C1+E1 | **Not started** | C1 and E1 are entirely unimplemented. |
| **Paper 6** (DSL) | H15, E3 | **Not started** | E3 has parser infrastructure but no JSON loader or experiment reproduction. |
| **Paper 7** (Logic Gates) | F1–F4, Phase D | **Partial** | AND/XOR/NAND fail on binary/payload. Need anti-attractor fitness shaping or extended eval steps. Wave/hybrid AND/XOR not yet tested. |
| **Paper 8** (Spatial Programmability) | H1–H5, P7 | **Not started** | H4/H5 kernel changes not implemented. Experiment files exist but cannot execute. |

---

## 5. further_hypothesis.md — Hypothesis Status

## 5. further_hypothesis.md — Hypothesis Status Map

| Hypothesis | Claim | Experiment | Status | Key Result | Implication |
|-----------|-------|------------|--------|------------|-------------|
| **H1** | Per-cell GA outperforms static rules | A1 | **Confirmed** | +1910% signal with density fitness; spatial structure emerges (Moran 0.3–0.5) | Phase 3 design assumption valid |
| **H2** | Fitness signal, not GA mechanism, is bottleneck | A2 | **Confirmed** | Density fitness (+1437%) >> edge signal fitness | "Shape the reward" > "rebuild the optimizer" |
| **H3** | Evolved genomes have spatial structure | A3 | **Confirmed** | Moran's I 0.32–0.49 on evolved genomes vs ~0 random | GA produces patterns, not noise |
| **H4** | Local GA converges (not collapses/oscillates) | A4 | **Partially confirmed** | Three regimes: convergence, extinction, oscillation all observed; convergence dominates at density fitness | Needs diversity preservation for edge-signal mode |
| **H5** | Most GA gain comes from static learned config | A5 | **Falsified** | Frozen genomes collapse to 3.7% density; continuous GA maintains 64% (6.4× signal) | GA is a *control* layer, not a *design* layer |
| **H6** | Inter-grid coupling has non-trivial optimal delay | B1 | **Falsified** | Delay=0 is global maximum at both random-IC (TE=0.245) and GA-IC (TE=0.056) regimes | Intent-ring infrastructure validated but not for the claimed reason |
| **H7** | Dynamics partition into distinct regimes | B3 | **Confirmed** | Three regimes: transient collapse (0–50), quasi-stable ash (50–400), slow extinction (400–1000) | Per-tick metrics are scientifically informative |
| **H8** | Parallel pipeline speeds up at realistic sizes | B4 | **Partially confirmed** | 1.517× at ngrids=8, size=128; fails at ngrids=2 or size=256 | Threshold is `ngrids ≥ 4` AND `size ≤ 128` |
| **H9** | System can self-organize sparse coupling topology | C1 | **Untested** | Experiment not implemented | Cannot evaluate |
| **H10** | Coupled-CA functions as a usable reservoir | C2 | **Partially confirmed** | ~10% density maintained; payload 2.5× more active than binary. **NRMSE ~1.2–1.5 at size=32** (fails < 0.1 criterion). Need larger grid to test capability. | Substrate is alive but may need more resources for useful computation |
| **H11** | Explicit rule modulation outperforms evolved modulation | C3 | **Falsified** | Hand-designed: +43.5%; GA: +1910%. Search beats design by 40× | Per-cell genome is an *evolved* layer, not *user-controlled* |
| **H12** | Intent modes change emergent dynamics | B2 | **Partially confirmed** | Two clusters (REPLACE/WEIGHTED vs ADD/THRESHOLD), but threshold/ADD bug means binary grids only see 2 effective modes | Threshold bug fixed; multi-value grids would show all 4 modes |
| **H13** | Evolved topology beats random matched-density | E1 | **Untested** | Experiment not implemented | Cannot evaluate |
| **H14** | Two-timescale evolution > either alone | E2 | **Untested** | Experiment not implemented | Cannot evaluate |
| **H15** | C API is declaratively expressible | E3 | **Partially confirmed** | `dsl/` parser exists; no JSON loader; no byte-equal reproduction from config | Phase 4 needs richer surface than config loader |

**Confirmed**: H1, H2, H3, H7 (4)
**Falsified**: H5, H6, H11 (3)
**Partially confirmed**: H4, H8, H10, H12, H15 (5)
**Untested**: H9, H13, H14 (3)

**Meta-hypothesis M1** ("Computation requires stable carriers"): Supported by A5 (GA maintains carriers), D2 (gliders survive REPLACE intent), and F3/F4 (wave/hybrid substrates sustain signals). Undermined by F1/F2 (binary/payload always-0 attractor suggests carriers are substrate-dependent).

---

## 6. Architecture Gaps

### Core Engine (ca_core/)

| Component | Claimed | Actual | Gap |
|-----------|---------|--------|-----|
| `CellGenome` | 16-bit packed struct | ✅ Implemented | None |
| `rule_select` | 5 bits (0–31) | ✅ Implemented | None |
| `coupling_weight` | 4 bits (0–15) | ✅ Implemented | None |
| `mutation_rate` | 4 bits | ✅ Implemented | None |
| `orientation` | 3 bits (0–7), formerly reserved | ❌ Missing | **Repurpose reserved bits [15:13] as 8-direction orientation**. Needed for H4/H5 anisotropic coupling. |
| `reserved` | 3 bits | ✅ Implemented | **Unused**. Now proposed for orientation (H4). Lineage tracking (C5) would need alternative encoding. |
| `MAX_GRIDS` | 4 | ✅ Raised to 16 | None |
| `MAX_GRID_SIZE` | 128 | ✅ Hard limit | Cannot test grids > 128² without refactor |
| BitGrid fast path | size multiple of 64 | ✅ Implemented | Does not apply when coupling active; documented |
| Geometric transforms | rotate/reflect/offset in `EdgeConn` | ❌ Missing | BRAINSTORM.md D3 envisioned this; `EdgeConn` only has scale_mode |
| Anisotropic coupling kernel | `grid_step_kernel` weights neighbors by genome orientation | ❌ Missing | BRAINSTORM.md D9 / P7. Need `alignment[8][8]` lookup + kernel flag `KERNEL_USE_ORIENTATION`. |
| Fractional coupling | 4 scale modes | ✅ Implemented | None |

### Metrics (metrics/)

| Metric | Claimed | Actual | Gap |
|--------|---------|--------|-----|
| Density | ✅ | ✅ | None |
| Entropy | ✅ | ✅ | None |
| Activity | ✅ | ✅ | None |
| Period detection | ✅ | ✅ | None |
| Transfer entropy | ✅ | ✅ | None |
| Glider census | ✅ | ✅ | None |
| Oscillator census | ✅ | ✅ | None |
| Connected components | ❌ | ❌ | BRAINSTORM.md D4; not implemented |
| Pattern velocity field | ❌ | ❌ | BRAINSTORM.md D4; not implemented |
| Integrated information (Φ) | ❌ | ❌ | BRAINSTORM.md D4; not implemented |
| Memory capacity | ❌ | 🟡 | Ridge regression exists but capacity not benchmarked |

### DSL (dsl/)

| Component | Claimed | Actual | Gap |
|-----------|---------|--------|-----|
| Parser | ✅ | ✅ | Parses imperative grid/connect/steps/seed scripts |
| Binder | ✅ | ✅ | Binds to `System` / `PayloadSystem` |
| Codegen | 🟡 | 🟡 | Verilog/C stubs exist; not production-ready |
| JSON loader | ❌ | ❌ | further_experiments.md E3 requires this |
| Declarative world definition | ❌ | ❌ | `04_territorylang_dsl.md` envisions `world { territory ... coupling ... }`; not implemented |
| Byte-equal reproduction from config | ❌ | ❌ | E3 success criterion |

### Tests (tests/)

| Test | Claimed | Actual | Gap |
|------|---------|--------|-----|
| Grid API | ✅ | ✅ | None |
| Coupling | ✅ | ✅ | None |
| DSL parser | ✅ | ✅ | None |
| BitGrid equivalence | ✅ | ✅ | None |
| Bind (binary + payload) | ✅ | ✅ | None |
| Rule schedule | ✅ | ✅ | None |
| Glossary | ✅ | ✅ | None |
| Hybrid system | ✅ | ✅ | None |
| Intent buffer | ✅ | ✅ | None |
| Transfer entropy | ✅ | ✅ | None |
| Pipeline | ✅ | ✅ | None |
| Pipeline parallel | ✅ | ✅ | None |
| Genome | ✅ | ✅ | None |
| Local GA | ✅ | ✅ | None |
| Wave | ✅ | ✅ | None |
| Payload pipeline | ✅ | ✅ | None |
| Bit-exactness regression (D1) | 🟡 | 🟡 | `test_pipeline`, `test_pipeline_parallel` exist but no systematic invariant testing |

---

## 7. Cross-Cutting Issues

### Issue 1: `edge_binary` Breaks TE for High-Density Grids
- **Where**: B1, metrics/transfer_entropy.c
- **Problem**: `edge_binary` returned 1 if any edge cell is alive, 0 otherwise. At ~28% density (GA-IC), the edge is always occupied → TE=0 because the time series has no variance.
- **Fix**: **Done and validated.** `edge_binary` replaced with `edge_density_scaled()` in `exp_delay_sweep.c` (returns [0,255] density). Added `te_compute_discrete()` to `metrics/transfer_entropy.c` (generalizes TE to K discrete states). Density binned into 8 states for histogram.
- **Validation**: B1 re-run with GA-ICs shows non-zero TE across all delays (0.017–0.056 bits). Delay=0 is peak; secondary peaks at delay=3–4. H6 (non-zero optimal delay > 0) is **falsified** even with sustained density.

### Issue 2: Always-0 Attractor Blocks AND/XOR/NAND on Binary/Payload
- **Where**: F1, F2
- **Problem**: Fitness = fraction of matching outputs. For AND (3/4 outputs are 0), the trivial always-0 solution scores 0.75 without ever producing a 1-output.
- **Fix**: Add anti-attractor penalty for 0x0 outputs; or extend eval steps to 100+ to give signals time to propagate; or read output from a denser sampling (not just the right edge).
- **Impact**: Blocks Paper 7 from full gate-suite results.

### Issue 3: Weight Co-Evolution Is Harmful
- **Where**: A6
- **Problem**: Rule-only evolution (0.0619) > both-fields evolution (0.0156) at long timescales. Weight mutations disrupt good rule configurations.
- **Fix**: Two-timescale evolution (E2); or sequential evolution (rules first, then freeze); or weight clamping after burn-in.
- **Impact**: Limits the utility of the full 16-bit genome. The `coupling_weight` field may need a different update schedule.

### Issue 4: C2 Task Performance Is Invisible
- **Where**: C2, `exp_hybrid_reservoir.c`
- **Problem**: Ridge regression readout computes NRMSE and memory capacity, but these were only logged to stdout. The CSV export only contained state statistics (density, entropy, activity).
- **Fix**: **Done and validated.** Added `--output-perf FILE` flag to `exp_hybrid_reservoir.c`. Memory capacity sweep (delays 1–10, size=32, steps=200) completed. Results: test NRMSE ~1.2–1.5 across all delays, **failing** the success criterion (NRMSE < 0.1). The reservoir does not produce separable features at this scale.
- **Impact**: Paper 3 has quantitative data but the substrate is under-resourced at size=32. Need size=64+ or steps=500+ to test capability.

### Issue 6: Reservoir Scaling Shows Suspicious 0.0 NRMSE
- **Where**: C2, `exp_hybrid_reservoir.c`
- **Problem**: At size=64/steps=500 with XOR task, all 5 trials reported train_nrmse=0.000000 and test_nrmse=0.000000. This is biologically implausible and suggests a bug in feature sampling or target construction.
- **Fix**: Investigate whether features are accidentally leaking the target or if the ridge solver is degenerate.
- **Impact**: Blocks Paper 3 capability claims until verified.

### Issue 5: Phase 4 DSL Is a Vision, Not Code
- **Where**: `territorylang_integration/04_territorylang_dsl.md`
- **Problem**: The document describes a full declarative language (`world { territory ... coupling ... tick_pipeline ... }`) with a 4-layer bridging architecture. The actual `dsl/` directory has a simple imperative script parser.
- **Gap**: ~2000 lines of conceptual design vs ~500 lines of actual parser code. No `TL_WorldProgram` struct. No JSON loader. No code generation from declarative specs.
- **Impact**: Blocks Paper 6 and the TerritoryLang integration endgame.

---

## 8. Priority Matrix

| Priority | Item | Effort | Scientific Value | Blocker For |
|----------|------|--------|-----------------|-------------|
| **P0** | ~~Fix `edge_binary` to return edge density~~ | **Done** | — | B1 TE measurement; Paper 2 |
| **P0** | ~~Add `--output-perf` to `exp_hybrid_reservoir`~~ | **Done** | — | Paper 3 task metrics |
| **P0** | Re-run C2 at size=64, steps=500 | M (2 hrs) | High | Paper 3 capability test |
| **P1** | Anti-attractor fitness shaping for F1/F2 | S (1 day) | Very High | Paper 7 AND/XOR results |
| **P1** | Test AND/XOR/NAND on wave/hybrid | S (1 day) | Very High | Paper 7 completeness |
| **P1** | Re-run B1 with driven signal (periodic glider injection) | M (1 day) | High | H6 deeper test |
| **P2** | D1 bit-exactness regression suite | S (1 day) | Medium | Code stability |
| **P2** | Two-timescale evolution (E2) | L (1 week) | High | A6 follow-up; H14 |
| **P2** | TE-driven coupling pruning (C1) | M (2–3 days) | High | Paper 5; H9 |
| **P3** | Topology GA (E1) | L (1 week) | Medium | Paper 5; H13 |
| **P3** | Genome lineage tracking (C5) | M (2 days) | Medium | H3/H4 depth |
| **P3** | Multi-objective fitness (C4) | M (2 days) | Medium | A6 follow-up |
| **P4** | JSON DSL loader (E3) | L (1 week) | Low | Paper 6 |
| **P4** | Geometric transforms (D3) | L (1 week) | Low | BRAINSTORM.md D3 |
| **P4** | Coarse-graining (D8) | L (1 week) | Low | BRAINSTORM.md D8 |
| **P4** | Cellular wire / collision logic (D6) | M (3 days) | Low | BRAINSTORM.md D6 |

---

## 9. Summary Statistics

### Documentation vs Implementation

| Category | Total Items | ✅ Complete | 🟡 Partial | ❌ Missing |
|----------|------------|------------|-----------|----------|
| BRAINSTORM Directions (D1–D8) | 8 | 2 | 2 | 4 |
| further_experiments.md Groups A–F | 26 | 19 | 2 | 5 |
| TerritoryLang Phases (1–4) | 4 | 3 | 1 | 0 |
| PAPER_PLAN Papers (1–7) | 7 | 3 | 2 | 2 |
| further_hypothesis.md (H1–H15) | 15 | 4 | 5 | 3 |
| Tests | 17 | 16 | 1 | 0 |
| Metrics | 11 | 7 | 1 | 3 |

### What Can Be Drafted Now

- **Paper 1**: All data ready (A1–A7 complete). Strong findings: +1910% signal, H5 falsified, A6/A7 insights.
- **Paper 2**: B2–B4 ready. B1 needs edge-density fix for TE, but the failure mode (random ICs → ash) is itself a finding.
- **Paper 4**: C3 ready. +43.5% modulation vs +1910% GA = clear "search beats design" story.
- **Paper 7**: F3–F4 ready. F1–F2 have findings (always-0 attractor) but not the full gate suite.

### What Needs Work Before Drafting

- **Paper 3**: Needs C2 task performance (NRMSE, memory capacity).
- **Paper 5**: Needs C1 (TE pruning) or E1 (topology GA) — neither implemented.
- **Paper 6**: Needs E3 (JSON loader) — parser exists but no declarative reproduction.

### Critical Path to Next Draft

1. Fix `edge_binary` → density (1 hour) → Paper 2 B1 results usable
2. Add `--output-perf` to reservoir (1 hour) → Paper 3 quantitative
3. Anti-attractor shaping + run AND/XOR on wave/hybrid (1–2 days) → Paper 7 complete gate suite
4. Draft Papers 1, 2, 4, 7 (2 weeks)

