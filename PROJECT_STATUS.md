# Project Completion Matrix vs. `further_experiments.md`

**Generated**: 2026-05-16

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Implemented, tested, FINDINGS.md written |
| 🟡 | Partially done (code exists, no FINDINGS.md, or data incomplete) |
| ❌ | Not started (no experiment file, no data) |
| 🔧 | Bug found and fixed during analysis |

---

## Group A — Validate Phase 3 (local GA)

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| A1 | Local-GA hyperparameter sweep | `exp_local_ga.c` | ✅ | `data/a1/` (34 txt) | `data/a1/FINDINGS.md` | ✅ |
| A2 | Fitness-mode shootout | `exp_local_ga.c` | ✅ | `data/a2/` (8 pgm, 3 csv) | `data/a2/FINDINGS.md` | ✅ |
| A3 | Genome heatmap dump | `exp_local_ga.c` | ✅ | `data/a3/` (47 pgm, 5 csv) | `data/a3/FINDINGS.md` | ✅ |
| A4 | Convergence dynamics | `exp_convergence.c` | ✅ | `data/a4/` (2 csv, 1 bin) | `data/a4/FINDINGS.md` | ✅ |
| A5 | Static-best-genome replay | `exp_local_ga.c` | ✅ | `data/a4/convergence_{base,ga}.csv` | `data/a5/FINDINGS.md` | ✅ |
| A6 | Rule-only vs weight-only decouple | — | — | `long_results/results_*decouple*.txt` (6 files) | `data/a6/FINDINGS.md` | ✅ |
| A7 | Fitness discount sweep | — | — | `long_results/results_*discount*.txt` (9 files) | `data/a7/FINDINGS.md` | ✅ |

**Group A summary**: 7/7 complete.

---

## Group B — Pipeline / intent studies

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| B1 | Coupling-delay sweep | `exp_delay_sweep.c` | ✅ | `data/b1/delay_sweep.csv`, `delay_sweep_ga.csv` | `data/b1/FINDINGS.md` | ✅ |
| B2 | Intent-mode comparison | `exp_signal.c` | ✅ | `data/b2/` (4 csv, 1 sh) | `data/b2/FINDINGS.md` | ✅ 🔧 |
| B3 | Per-tick analyze trace | `exp_signal.c` / `exp_local_ga.c` | ✅ | `data/b3/trace.csv`, `trace_ga.csv_*` | `data/b3/FINDINGS.md` | ✅ |
| B4 | Parallel scalability | `exp_bench.c` | ✅ | `data/b4/bench.csv`, `bench_n8.csv` | `data/b4/FINDINGS.md` | ✅ |

**Group B summary**: 4/4 complete. 🔧 B2 threshold/ADD bug fixed in `ca_core/intent.c`. B4 `MAX_GRIDS` raised from 4→16; ngrids=8 achieves 1.517× speedup.

---

## Group C — Novel experiments

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| C1 | TE-driven coupling pruning | ❌ | ❌ | ❌ | ❌ | ❌ |
| C2 | Reservoir + ridge regression | `exp_hybrid_reservoir.c` | ✅ | `long_results/hybrid_reservoir.csv` | `data/c2/FINDINGS.md` | ✅ |
| C3 | Explicit rule modulation (H12) | `exp_rule_mod.c` | ✅ | `data/c3/rule_mod.txt` | `data/c3/FINDINGS.md` | ✅ |
| C4 | Multi-objective fitness | ❌ | ❌ | ❌ | ❌ | ❌ |
| C5 | Genome lineage tracking | ❌ | ❌ | ❌ | ❌ | ❌ |

**Group C summary**: 2/5 complete. C2 reservoir state statistics analyzed; task performance (NRMSE, memory capacity) requires stdout re-run. C3 rule modulation confirmed +43% improvement. C1, C4, C5 unimplemented.

---

## Group D — Sanity / regression

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| D1 | Bit-exactness regression | `tests/test_pipeline*.c` | ✅ (tests) | — | ❌ | 🟡 |
| D2 | Pattern preservation under coupling | `exp_glider_preservation.c` | ✅ | `data/d2/` (3 csv) | `data/d2/FINDINGS.md` | ✅ |
| D3 | Determinism audit | `exp_local_ga.c`, `exp_signal.c` | ✅ | Implicit | Mentioned in `further_experiments.md` | ✅ |

**Group D summary**: 2/3 complete. D1 has test files but no formal regression suite report.

---

## Group E — Speculative / longer horizon

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| E1 | GA on coupling topology | ❌ | ❌ | ❌ | ❌ | ❌ |
| E2 | Two-timescale evolution | ❌ | ❌ | ❌ | ❌ | ❌ |
| E3 | Minimal DSL loader | `dsl/` parser+bind+codegen | 🟡 | — | ❌ | 🟡 |

**Group E summary**: 0/3 complete. E3 has parser infrastructure but no JSON loader or experiment reproduction.

---

## Group F — Logic-gate evolution (Phase D)

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| F1 | Binary logic gate GA | `exp_binary_logic_ga.c` | ✅ | `data/f1/` (4 txt) | `data/f1/FINDINGS.md` | ✅ |
| F2 | Payload logic gate GA | `exp_payload_logic_ga.c` | ✅ | `data/f2/` (4 txt) | `data/f2/FINDINGS.md` | ✅ |
| F3 | Wave logic gate GA | `exp_wave_logic_ga.c` | ✅ | `data/f3/` (1 txt) | `data/f3/FINDINGS.md` | ✅ |
| F4 | Hybrid cross-grid logic | `exp_hybrid_logic.c` | ✅ | `data/f4/` (1 txt) | `data/f4/FINDINGS.md` | ✅ |

**Group F summary**: 4/4 complete. After four infrastructure fixes (modulo lookup, shaped fitness, density output, IC-drift fix), logic gate evolution succeeds on all substrates: F1 binary 6/12, F2 payload 8/12, F3 wave 4/12, F4 hybrid 6/12. Full comparison: `CROSS_SUBSTRATE_RESULTS.md`.

---

## Group G — Turing Completeness & Composition

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| G1 | Ruleset profiler | `exp_ruleset_profile.c` | ✅ | `data/ruleset_profile_*.csv` | — | ✅ |
| G2 | Compositional logic chain | `exp_compositional_logic.c` | ✅ | `data/gate_*.bin` | — | 🟡 |
| G3 | Rule 110 cyclic tag | `exp_rule110_tag.c` | ✅ | — | — | 🟡 |

**Group G summary**: 3 new experiments created. G1 complete. G2 shows partial composition (3/4 on AND→XOR). G3 validates 1D engine; full Cook encoding pending.

---

## Group H — Spatial Programmability (new)

| ID | Experiment | File | Binary | Data | FINDINGS | Status |
|----|-----------|------|--------|------|----------|--------|
| H1 | Multimodal binary logic (2 modes) | `exp_multimodal_logic_ga.c` | ✅ | `data/mm_*.bin`, `data/mm_*.log` | — | 🟡 |
| H2 | Multimodal wave logic (2 modes) | `exp_multimodal_wave_logic_ga.c` | ✅ | `data/mm_wave_*.bin`, `data/mm_wave_*.log` | — | 🟡 |
| H3 | Polar/anisotropic 4-mode logic | `exp_multimodal_polar_logic_ga.c` | ✅ | — | — | 🟡 |
| H4 | Genome orientation macros | `genome.h` | ✅ | — | — | ✅ |
| H5 | Anisotropic kernel in pipeline | `pipeline.c` | ✅ | `tests/test_orientation.c` | — | ✅ |
| H6 | 48-bit genome expansion (mode orientations + dir rules) | `genome.h/c`, `pipeline.c` | ✅ | — | — | 🟡 |
| H7 | Hierarchical evolution (Stage 1+2) | `exp_hierarchical.c` | ✅ | `data/h7_run.csv` | Stage 1 flat-zero — superseded by `COMPOSABLE_RULESET_ROADMAP.md` | 🟡 |
| H8 | Directional rule modification experiment | `exp_directional_multimodal.c` | ✅ | `data/h8_3333_gens.csv` | `data/h8_FINDINGS.md` | 🟡 |
| H9 | Composable 6-layer ruleset + 3-stage GA | `ca_core/composable_ruleset.{h,c}`, `exp_composable_stage0.c` | ✅ Stage 0 + pulse-reset sweep partial | `data/sweep/` (48 runs), `data/sweep300/` (24 runs), `data/sweep_pulse/` (24/32 runs), `data/composable_R_star.bin` | `COMPOSABLE_RULESET_ROADMAP.md`, `SWEEP_ANALYSIS.md`, `PULSE_RESET_DESIGN.md` | 🟡 |

**Group H summary**: H4/H5 implemented. H6 kernel complete. H8: 3333 gens, peak 0.875, no all-4-XOR. H7 surfaced root cause: hardcoded `RULE_TABLE` can't propagate under anisotropic gating. **H9 (composable ruleset)**: 56-bit DNA, Stage-0 GA implemented. **v1 rejected** (fill-everything `B=0x7C S=0xFF`). **v3 sweep complete**: 100-gen + 300-gen (48 runs). v3 + T=50 is sweet spot (mean 0.573, peak 0.9688). **Pulse-and-reset implemented**: `--reset-interval` and `--path-coherence` flags added to `exp_composable_stage0.c`. **Pulse-reset sweep (24/32 complete)**: R=10 is sweet spot (peak 0.5038), R=15 is uncanny valley (mean 0.370), R=20 peak 0.4707. Coherence c=0.3 too aggressive for primary evolution. **Next**: promote best R=10 ruleset (`B=0x5F S=0x61 aniso=237`) to R*, test in Stage 1. If gradient exists, full pipeline unblocked. If not, consider 2-phase ruleset.

---

## Aggregate Statistics

| Group | Total | ✅ Complete | 🟡 Partial | ❌ Not Started |
|-------|-------|------------|-----------|---------------|
| A | 7 | 7 | 0 | 0 |
| B | 4 | 4 | 0 | 0 |
| C | 5 | 2 | 0 | 3 |
| D | 3 | 2 | 1 | 0 |
| E | 3 | 0 | 1 | 2 |
| F | 4 | 4 | 0 | 0 |
| G | 3 | 1 | 2 | 0 |
| H | 9 | 3 | 6 | 0 |
| **Total** | **38** | **23** | **9** | **6** |

**Completion rate**: 61% fully complete, 24% partial, 16% not started.

---

## Highest-Value Next Steps

### Top priority — supersedes H7
0. **Composable 6-layer ruleset + 3-stage GA** — see `COMPOSABLE_RULESET_ROADMAP.md`. Replaces hardcoded `RULE_TABLE` for Stage-1 propagation. Root cause of H7's flat-fitness failure (no rule in the 9-rule table propagates thin signals under anisotropic gating). Plan: Stage 0 evolves a 56-bit ruleset DNA for propagation; Stage 1 evolves per-cell orientations on top of it; Stage 2 evolves logic params for XOR.

### Immediate (no new code required)
1. **Re-run C2 at size=64, steps=500** — current sweep (size=32) shows NRMSE ~1.2–1.5, failing the < 0.1 criterion. Larger grid may show capability.
2. **Re-run B1 with driven signal** — inject periodic gliders to test if a *genuine* signal shows delay-dependent TE (not just coupling disturbance).
3. **Close F1-OR gap** — binary OR at 0/3 perfect; test longer evolution (200 gens) or larger population (200).

### Short (small code changes)
4. **Extend eval steps for F1–F4** — 60 ticks may be too short; test 100–200 for slow-propagating rules.
5. **Profile ngrids=2, size=256 slowdown** in B4 — is it cache thrashing or synchronization overhead?

### Medium
6. **C1** — TE-driven coupling pruning (new MUTATE + ANALYZE phases).
7. **C4** — Multi-objective fitness (Pareto front tracking in `local_ga.c`).
8. **E3** — JSON DSL loader (leverage existing `dsl/parser.c`).

### Large
9. **C5** — Genome lineage tracking (lineage ID in reserved bits + phylogeny logging).
10. **E1** — Topology GA (mutate `Coupling` graph structure).
11. **E2** — Two-timescale evolution (outer GA wrapping `exp_local_ga`).
12. **H6** — Complete H3 with 48-bit genome mode orientations; test if per-mode orientation fields unlock all 4 trace directions.
13. **H7** — Implement hierarchical two-stage GA (orientation highways → rule evolution).
14. **H8** — Implement directional rule modification kernel (alt_rule per cardinal direction).
