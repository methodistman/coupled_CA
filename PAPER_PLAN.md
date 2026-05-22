# WaveBuffer Multi-Paper Research Roadmap

## Paper 1: Local GA for Adaptive Spatial Structure in Coupled CA
- **Core**: H1-H5, A1-A7. Per-cell genomes evolved by local tournament selection.
- **Key findings**: +1910% signal with density fitness; spatial structure emerges (Moran 0.3-0.5) but does not predict performance; **H5 falsified** — static-genome replay collapses to ash (signal 6.4× lower than continuous GA); **A6** — rule-only evolution dominates at long timescales (0.0619 vs 0.0156 for full co-evolution), weight-only is actively harmful; **A7** — aggressive discounting gamma=0.5 is optimal (U-shaped: 0.5 > 1.0 > 0.9).
- **Status**: A1-A7 all have FINDINGS.md under `data/a{1,2,3,4,5,6,7}/`.
- **Venue**: Artificial Life / Complex Systems / NetSci. arXiv preprint Aug 2026.

## Paper 2: Information Flow and Coupling Dynamics in Multi-Grid CA
- **Core**: H6-H8, H12, B1-B4. Intent buffers, TE analysis, coupling delays, parallel scaling.
- **Key findings**: Per-tick metrics reveal regimes; intent modes differ statistically; parallel speedup at scale.
- **Status**: B1–B4 complete; all have `FINDINGS.md` under `data/b{1,2,3,4}/`. Key results:
  - B1: H6 **falsified** — delay=0 is global maximum at both random-IC (TE=0.245) and GA-IC (TE=0.056) regimes. TE may measure *coupling disturbance*, not *information flow*. `edge_density_scaled()` + `te_compute_discrete()` fix validated.
  - B2: Two intent-mode clusters; threshold/ADD bug fixed in `ca_core/intent.c`.
  - B3: Three temporal regimes confirmed; GA-IC companion trace produced.
  - B4: Parallel speedup reaches 1.517× at `size=128, ngrids=8` (MAX_GRIDS raised to 16).
- **Venue**: Physical Review E / JSTAT / NetSci.

## Paper 3: Reservoir Computing with Coupled Cellular Automata
- **Core**: H10, C2. CA as reservoir; linear readout; memory capacity; NRMSE benchmarks.
- **Key findings**: H10 precondition confirmed — hybrid reservoir sustains ~10% density across 10 trials. Payload grid is ~2.5× more active than binary (0.18 vs 0.07). **Memory capacity sweep completed** (delays 1–10, size=32, steps=200): test NRMSE ~1.2–1.5, **failing** the < 0.1 success criterion. Substrate is alive but under-resourced at this scale. Need size=64+ or steps=500+ to test true capability.
- **Status**: `data/c2/FINDINGS.md` written. Task perf sweep done; larger grid run pending.
- **Venue**: ICML / NeurIPS (workshop) / IEEE TNNLS.

## Paper 4: Explicit vs. Evolved Control in Spatial CA
- **Core**: H11, C3. Hand-designed rule modulation vs GA-discovered maps.
- **Key question**: Does search beat design for this substrate?
- **Key findings**: H11 confirmed — explicit Life/HighLife modulation improves signal by +43.5% over static HighLife (8/10 wins). However, the local GA achieves +1910% improvement (A1), so **search massively outperforms design** on this substrate.
- **Status**: `data/c3/FINDINGS.md` written. `exp_rule_mod` sweeps completed.
- **Venue**: GECCO / PPSN / Artificial Life.

## Paper 5: Self-Organizing Coupling Topologies
- **Core**: H9, H13, C1+E1. TE-driven edge pruning; topology evolution.
- **Status**: Not started. Needs new mutation primitives.
- **Venue**: IEEE Trans Evol Comp / Artificial Life.

## Paper 6: A Declarative DSL for Reproducible CA Experiments
- **Core**: H15, E3. JSON spec -> byte-equal reproduction.
- **Status**: Partial (dsl/ exists).
- **Venue**: SoftwareX / JOSS.

## Paper 8: Spatial Programmability via Anisotropic Coupling in Cellular Automata
- **Core**: P7, `exp_multimodal_polar_logic_ga.c`. Single-grid multimodal computation using orientation-encoded genomes and anisotropic neighbor coupling.
- **Key question**: Can a single CA substrate self-organize into multiple independent computational channels by spatial differentiation of cell orientation?
- **Concept**: Each cell's genome encodes an orientation angle (3 bits, 8 directions). Coupling to neighbors is weighted by cos²(φ − θ), creating directional highways. A single grid hosts 4 modes (horizontal, vertical, diagonal, anti-diagonal), each computing a different logic gate.
- **Connection to Turing completeness**: If spatial programmability succeeds, it offers an alternative compositional paradigm to grid chaining (G2). Composition via spatial multiplexing may scale better than composition via edge coupling.
- **Status**: Experiment file written (`exp_multimodal_polar_logic_ga.c`). Genome orientation macros and anisotropic kernel not yet implemented. Blocked on `genome.h` macro additions and `pipeline.c` kernel change.
- **Venue**: Artificial Life / Complex Systems / ALIFE.

## Paper 7: Evolving Logic Gates in Multi-Substrate Cellular Automata
- **Core**: Phase D (F1–F4). Population-level GA evolves per-cell genomes on binary, payload, wave, and hybrid grids to compute AND/OR/XOR/NAND.
- **Key findings**: **Breakthrough** — four infrastructure fixes (modulo lookup, shaped fitness, density-relative output, IC-drift fix) unblocked logic gate evolution on all substrates. Perfect runs (3 runs × 4 gates): **binary 6/12**, **payload 8/12**, **wave 4/12**, **hybrid 6/12**. All four gates (AND, OR, XOR, NAND) are now reachable on all substrates. Payload is strongest; hybrid excels at XOR (3/3); wave's pre-fix OR advantage was an artifact of the Conway-fallback bias.
- **Status**: All four experiments implemented, swept, and post-fix re-validated (`data/f{1,2,3,4}/FINDINGS.md`). `CROSS_SUBSTRATE_RESULTS.md` provides top-level comparison. Remaining: close F1-OR gap (0/3 perfect), extend eval steps, longer evolution runs.
- **Venue**: GECCO / PPSN / Artificial Life.

## Cross-Cutting Hypotheses
- **M1**: "Computation requires stable carriers." Underlies Papers 1, 3, 4.

## Recommended Sequencing
1. **Paper 1** (Month 1-2): Strongest data, most complete. arXiv immediately.
2. **Paper 2** (Month 3-4): Leverages pipeline determinism tests.
3. **Paper 4** (Month 4-5): Quick win — explicit modulation implemented; compare to GA results from A1.
4. **Paper 7** (Month 5): Phase D experiments already implemented; needs parameter sweeps and statistical validation.
5. **Paper 3** (Month 6-8): Ridge regression readout done; needs memory capacity benchmark suite.
6. **Paper 5** (Month 8-10): Depends on Papers 1-3 results.
7. **Paper 6** (Month 10+): Documentation/cleanup paper.
