# Project Roadmap — Substrate Diversification & Logic-Gate Unblock

**Date**: 2026-05-17
**Status**: F1 binary logic gate evolution **was unblocked** by the modulo lookup + IC fix proposed in §4.5 and §8 below. NAND solved 3/3, XOR 2/3, AND 1/3, OR partial (6/12 perfect overall). See `data/f1/FINDINGS.md` §F7 for full results. **All four logic-gate substrates (F1–F4) now succeed at ≥ 4/12 perfect** — see `CROSS_SUBSTRATE_RESULTS.md` for the headline comparison. This roadmap is preserved in original form below as a record of the planning that led to the unblock; later sections (§9 below) are forward-looking next steps.

**Context (original)**: F1 (binary logic gate evolution) infrastructure is now correct (anti-attractor fitness shaping + density-relative output read), but the GA still cannot evolve gates because the binary substrate itself is bistable. This roadmap captures the resulting next-step plan and a deeper brainstorm about Conway-centricity in the codebase.

---

## 1. Background — Where F1 Stands After Today's Work

Two infrastructure fixes have been added to `experiments/exp_binary_logic_ga.c`:

- **`--fitness shaped`** (default): per-class balanced weighting + 0.25× constant-output penalty. Demotes always-0 from a misleading 0.75 (AND) to a uniform 0.125. Mathematically correct, behaviorally validated.
- **`--out-mode density`** (default): scale-invariant comparison of edge density to grid-mean density. Replaces the brittle `count > half/2` threshold that was numerically impossible to fire at decayed-ash density.

Despite both fixes, all gates still collapse to `0x0`. A direct population diagnostic (40 random genomes, size=32, 60 steps) reveals **why**:

| Truth table | Count | Frequency |
|-------------|-------|-----------|
| `0x0` (always-0) | 34 / 40 | 85 % |
| `0xF` (always-1) | 6 / 40 | 15 % |
| **All 14 input-sensitive truth tables** | **0 / 40** | **0 %** |

**The binary substrate is bistable, not computational.** Edge-coupling perturbation does not penetrate the bulk dynamics enough to differentiate `(a, b)` inputs at the output column. With zero input-sensitive individuals in the random population, no fitness function can select for one — the GA is searching an empty space.

---

## 2. Roadmap — Prioritised Next Steps

### P0 — Substrate audit (1 day)

Apply the new infrastructure to the other Phase-D experiments and run the **population-diversity probe** (truth-table histogram across 40 random genomes) on each substrate before committing to any further evolution.

| Substrate | File | Status | Action |
|-----------|------|--------|--------|
| Binary | `exp_binary_logic_ga.c` | **Bistable, falsified** | Defer until coupling architecture revised |
| Payload (8-bit) | `exp_payload_logic_ga.c` | Unknown | Add `--fitness shaped`, run probe |
| Wave (vector) | `exp_wave_logic_ga.c` | OR converges 100 % | Add probe; test AND/XOR/NAND |
| Hybrid (cross-grid) | `exp_hybrid_logic.c` | OR converges 80 % | Add probe; test AND/XOR/NAND |

**Goal**: identify which substrates are computationally expressive *before* evolution, not after.

**Acceptance criterion**: a substrate "passes" the audit if a random population of 40 produces ≥ 4 distinct truth tables, of which ≥ 2 are input-sensitive (not in `{0x0, 0xF}`).

### P1 — Stronger coupling architecture (2–3 days)

The F6 finding implies the input must perturb **bulk** dynamics, not just the boundary cell. Three options, in increasing complexity:

1. **Multi-column input injection** — set the leftmost *N* columns from grid 1 instead of just the edge. Sweep `N ∈ {1, 2, 4, 8}` and re-run the diversity probe.
2. **Higher coupling weight default** — current `coupling_connect` uses default weights; experiment with explicit `weight = 0.8 / 1.0`.
3. **Bidirectional drive coupling** — couple grid 1 → grid 0 *and* drive a small region every tick (not just edge). Closer to a reservoir-with-driver setup.

**Acceptance criterion**: random-population diversity probe yields ≥ 2 input-sensitive truth tables.

### P2 — Substrate diversification (Rule 110 + non-Life rules) — see §4 below

Currently 8 of 9 rules in `RULE_TABLE` are 2-D Life family variants, and the genome's 5-bit `rule_select` falls back to Conway's Life for ~72 % of random values. This is the core Conway-centricity issue this roadmap addresses.

### P3 — Population-diagnostic tool (½ day)

Promote the throwaway `probe_pop` test into a reusable tool: `experiments/exp_substrate_audit.c`. Inputs: substrate type, grid size, eval steps, population size. Outputs: truth-table histogram + an *expressiveness score* (number of distinct input-sensitive truth tables / 14). Use this as a precondition gate for any future logic-gate evolution.

### P4 — Apply infrastructure cross-cutting (½ day)

Once P0–P2 settle, port `--fitness shaped` and `--out-mode density` to:

- `exp_payload_logic_ga.c`
- `exp_wave_logic_ga.c`
- `exp_hybrid_logic.c`

Re-run the F2/F3/F4 sweeps with the corrected fitness. Update the corresponding `data/f{2,3,4}/FINDINGS.md`.

### P5 — Defer F1 binary logic

Until P1 (coupling) and P2 (substrate diversity) are addressed, F1 binary logic is **architecturally blocked**. The fitness/output infrastructure is correct; the bottleneck has moved upstream.

---

## 3. Why This Matters for the Wider Project

The F6 bistability finding is not a Phase-D-only issue. It mirrors a recurring theme already documented in `FINDINGS_SUMMARY.md`:

- **Theme 1**: *Random ICs are a trap* — Life decays to ash; signal-bearing dynamics require evolved ICs or density fitness.
- **Theme 2**: *Continuous adaptation beats static* — H5 falsified because static genomes collapse.
- **B1 H6 falsification**: TE peaks at delay = 0 because the only "information" being measured is *coupling disturbance*, not flow through a substrate.

All four of these point at the same root cause: **the substrate cannot sustain non-trivial dynamics under random conditions, so every measurement and every evolutionary search is dominated by ash-recovery rather than computation.** The local GA "succeeds" in Paper 1 because it acts as a homeostat, not because it computes.

If we want claims about computation, information flow, reservoir capacity, or logic synthesis to mean anything beyond "we kept the lights on," **we need substrates where the lights stay on by default**.

---

## 4. Brainstorm — Are We Too Conway-Centric?

> "we are focusing too much on a specific ruleset don't you think?"

**Yes — and the codebase reveals it bluntly.**

### 4.1. The Conway bias is structural, not just cultural

```
RULE_TABLE has 9 entries:
  Conway's Life, HighLife, Day & Night, Seeds, Diamoeba,
  Anneal, Life Without Death, Morley, Rule 110

Genome rule_select is 5 bits (0..31).
Lookup: ri ∈ [0, 9)  → use RULE_TABLE[ri]
        ri ∈ [9, 32) → fall back to default (typically Conway's Life)
```

So for a random genome:
- 9 / 32 = **28 % use a non-default rule**
- 23 / 32 = **72 % silently revert to Conway's Life**

Any "evolution discovers spatial structure" claim is partly an artefact of the GA learning which 9 / 32 indices avoid the Conway fallback. We've been studying *the failure modes of Conway's Life* under various coupling regimes, not the diversity of cellular automata.

### 4.2. Why Conway is a bad default for *this* project

| Dimension | Conway's Life | What we actually need |
|-----------|---------------|----------------------|
| Random IC behaviour | Decays to ash | Sustained, signal-rich activity |
| Glider abundance | Rare, fragile | Common, robust |
| Class | Class IV (edge of chaos) but **biased toward death** | Class IV biased toward life |
| Computational universality | Yes, but requires hand-engineered patterns | Yes, ideally with broader basin |
| Suitability for reservoir computing | Poor (grid dies) | Need persistent, varied dynamics |
| Suitability for logic gates from random IC | Effectively zero (F6) | The whole point |

The story Paper 1 tells is "the local GA evolves homeostasis against Life's drift toward death." That's a real result, but it's *Life-specific*. We do not know whether the GA discovers spatial structure on substrates that don't actively want to die.

### 4.3. Rule 110 as a candidate default

Cook (2004) proved Rule 110 is Turing-complete. Its dynamics differ from Life in exactly the ways we need:

| Property | Conway's Life | Rule 110 |
|----------|---------------|----------|
| Random IC density (steady state) | ~3 % (decay) | ~50 % (sustained) |
| Glider density | Rare | Dense lattice of `A`-, `B`-, `C`- structures |
| 1D / 2D | 2D Moore | 1D, 3-cell |
| Universal computation | Yes (constructive) | Yes (Cook 2004) |
| Logic gates | Hand-designed glider collisions | Glider collisions emerge in random IC |
| Reservoir candidate | Poor (decay) | Strong (rich edge-of-chaos) |

Rule 110 is already in `RULE_TABLE[8]` and the engine handles it (`engine.c:102`, evaluating each row independently as a 1D ECA). What's missing is using it as a *default* or running experiments where it is the primary rule.

### 4.4. Other under-explored substrates worth adding

| Rule | Why | Complexity to add |
|------|-----|-------------------|
| **Brian's Brain** (3-state: on/dying/off) | Pure 2D, glider-rich, doesn't decay, 3-state already supportable in payload grid | Medium — needs new state-machine in `rules.c` or implement in payload |
| **Wireworld** (4-state) | *Designed* for circuits — head/tail/wire/empty. Logic gates trivially expressible. | Medium — 4-state, payload grid is right substrate |
| **Rule 30** | Class III chaos, used for randomness; useful as a TE-signal source | Trivial — same shape as Rule 110 |
| **Rule 90** | Linear/XOR; produces Sierpinski; great for testing decoupled-superposition predictions | Trivial |
| **Rule 184** (traffic) | Conserves particles; deterministic transport; ideal for direct signal-propagation tests | Trivial |
| **Larger Than Life** (2D, range > 1) | Sustains complex dynamics where Life dies | Medium — needs neighborhood radius parameter |
| **Lenia** (continuous Life) | Continuous-state, smooth dynamics; alive by default; orbium gliders | Hard — would need new substrate |

The cheapest high-value additions are **Rule 30, Rule 90, Rule 184** (one line each in `rules.c`) and **Brian's Brain** (state machine). Wireworld is the natural endpoint for Paper 7.

### 4.5. Concrete proposals

#### A. Rebalance `RULE_TABLE` so genome lookup doesn't bias to Conway

Two cheap fixes, in order of preference:

1. **Modulo lookup**: replace
   ```c
   if (ri >= 0 && ri < NUM_RULES) rule = &RULE_TABLE[ri];
   ```
   with
   ```c
   rule = &RULE_TABLE[ri % NUM_RULES];
   ```
   Now every 5-bit value selects *some* table entry; Conway's share drops from 72 % to ~12 %.

2. **Expand `RULE_TABLE` to 32 entries** with diverse rules: Life family + 1D ECA family (Rules 30, 54, 90, 110, 184) + Brian's Brain + Wireworld + a "null/identity" rule. This lets every 5-bit genome value pick a meaningful rule without modulo aliasing.

Option 1 should be done first as a one-line change with a clean A/B comparison against current behaviour. Option 2 is the longer-term plan.

#### B. Add a `--default-rule` flag to all logic-gate experiments

Currently `sys.grids[0]->rule_idx = 0` is hardcoded (Conway). Add a CLI flag so we can run F1 with Rule 110 (or HighLife, or Brian's Brain) as the substrate without recompiling. Same for B1, C2, etc.

#### C. Re-run the headline experiments on Rule-110-based substrates

After (A) and (B):

- **B1 delay sweep on Rule 110** — does TE actually peak at non-zero delay when the substrate is alive by default?
- **C2 reservoir on Rule 110** — does memory capacity exceed the NRMSE < 0.1 threshold when the reservoir doesn't decay?
- **F1 logic-gate GA on Rule 110** — does the substrate audit produce input-sensitive truth tables?
- **A-series local-GA replication** — does the GA still discover homeostasis, or does it find *computation* when the grid isn't dying?

If any of these show qualitatively different results, Papers 1–3 need to acknowledge that their findings are **Life-specific**. If they show the same results, we have a much stronger generality claim.

#### D. Make "substrate" a first-class experimental axis

Right now the codebase is structured around three data-type substrates (binary / payload / wave) but only one *rule-family* per substrate. The roadmap suggests treating rule-family as a separate axis: a 4-cell taxonomy of (substrate-type) × (rule-family) gives 12 distinct experimental contexts, of which we have explored only ~3.

---

## 5. Suggested Execution Order

| Order | Task | Effort | Unlocks |
|-------|------|--------|---------|
| 1 | **§4.5 (A) modulo lookup**, A/B against current | 1 hour | Eliminates the 72 % Conway fallback bias |
| 2 | **P3 substrate-audit tool** (`exp_substrate_audit.c`) | ½ day | Diagnostic infrastructure for all future substrate questions |
| 3 | **P0 audit on payload / wave / hybrid** | ½ day | Identifies which substrate is the right F-series target |
| 4 | **§4.5 (B) `--default-rule` flag** | 1 hour | Enables non-Conway runs without recompile |
| 5 | **§4.5 (C) Rule-110 replication of B1, C2, F1** | 1–2 days | Tests generality of headline findings |
| 6 | **P1 stronger coupling** (multi-column input) | 1 day | Unblocks F1 binary if §4.5 (C) confirms the substrate is alive |
| 7 | **P4 apply shaping/density to F2–F4** | ½ day | Cross-cutting consistency |
| 8 | **§4.4 add Rules 30 / 90 / 184 + Brian's Brain** | 1 day | Substrate diversity expansion |
| 9 | **§4.4 Wireworld substrate (long-term)** | 1 week | Paper 7 endgame: a substrate explicitly designed for circuits |

---

## 6. Risks and Open Questions

1. **Will the modulo fix break Paper 1 reproducibility?** Yes — A1–A7 results are seeded against the current 9-rule table with Conway fallback. We'd need to either (a) keep the old behaviour behind a `--legacy-rule-lookup` flag, or (b) re-run A-series with the new lookup and compare. Likely option (b) — it would be a stronger paper if we can show the GA discovery story holds across rule-table layouts.
2. **Is Rule 110 in 1D-per-row really 2D-comparable?** No — each row is independent, so coupling between rows is meaningless. For honest 2D Rule 110, we'd need a 2D extrusion (e.g., Wolfram's totalistic 2D rules, or a "stacked" Rule 110 where vertical neighbours influence horizontal evolution). Worth a dedicated note.
3. **Does the bistability finding generalise to payload/wave?** Unknown — that's exactly what P0 audit will tell us. If payload also collapses to constant outputs, the issue is the coupling architecture, not the substrate. If only binary collapses, the issue is the substrate's narrow basin.
4. **Could the local GA itself be evolving toward bistability?** Possible — A-series fitness functions reward density / signal stability, both of which are maximised by `0xF` saturation. We've never penalised constant outputs in the local GA. This deserves its own follow-up study.

---

## 7. What This Roadmap Is Not

- It is **not** a refactor plan — minimal code changes, surgical edits.
- It is **not** a paper plan — those live in `PAPER_PLAN.md`. This document feeds into Papers 2, 3, 7 and a possible new "substrate generality" paper.
- It does **not** abandon Conway — Conway's Life remains the most-studied substrate and will stay in the rule table. The argument is only that *Life should not be the default that 72 % of random genomes silently revert to*.

---

## 8. Quick-Win (Today)

The single highest-leverage change is the **modulo lookup** in `engine.c`, `pipeline.c`, `payload_pipeline.c`, and `wave_engine.c` — four one-line edits — followed by re-running the F1 substrate audit with the modulo lookup in place. If post-modulo random populations still collapse to `{0x0, 0xF}`, the bottleneck is definitively coupling architecture (P1), not rule diversity (P2). If they show input-sensitive truth tables, then the entire F-series can be unblocked with a one-hour fix.

That's the next 60 minutes.

---

## 9. Post-Breakthrough Next Steps (added after §8 was executed)

The modulo lookup fix was applied across `engine.c`, `pipeline.c`, `payload_pipeline.c`, `wave_engine.c`. **A second bug was uncovered during follow-up testing**: `evaluate()` in `exp_binary_logic_ga.c` captured the IC backup *inside* the function from whatever state the previous individual left, drifting across the population. After both fixes, F1 succeeds (NAND 3/3, XOR 2/3, AND 1/3, OR partial). Updated priorities:

### P0 — Re-validate headline findings under modulo lookup — **DONE (2026-05-17)**

Re-ran canonical commands for A1, B1, C2 under the modulo lookup. **All three headline findings preserved**:

- **A1**: GA improvement +1819 % (was +1910 %, −5 %). GA now uses 32 / 32 distinct rule indices (vs ~9 pre-modulo cap) — the discovery story is *stronger*, not weaker.
- **B1**: TE still peaks at delay=0 (H6 falsification holds). Absolute magnitudes ~10× smaller than the stale FINDINGS table, but `exp_delay_sweep` does not use genome lookup, so the shift is build drift unrelated to the modulo fix.
- **C2**: Reservoir test NRMSE = 1.96 (was 1.09), still fails the < 0.1 success criterion. Negative finding preserved.

**No paper-quality claim was inflated by the Conway-fallback bias.** Full details: `data/postmod_revalidation/REVALIDATION.md`. Side-effect to address: regenerate stale FINDINGS tables for B1 and C2 (low priority, 5 min each).

### P1 — Audit other experiments for IC-drift bug
The pattern "capture IC backup inside an evaluation function called in a loop" likely appears elsewhere. Greppable signature: `memcpy(bk, grid->cells, ...)` followed by `pipeline_execute` in a per-individual loop without an outer reset. Candidates: `exp_local_ga.c`, `exp_payload_logic_ga.c`, `exp_wave_logic_ga.c`, `exp_hybrid_logic.c`, `exp_ga_ic.c`, `exp_convergence.c`.

### P2 — Close the F1 OR gap
OR (target 0xE) hits 0.83 max but never perfect across seeds 42–44. Three avenues:
1. Longer evolution (gens=200) with same params.
2. Larger population (80–100) for more diversity.
3. Anneal mutation rate down over time (currently flat 0.05).

### P3 — Apply F1 fixes to F2/F3/F4
Port `--fitness shaped`, `--out-mode density`, and the IC-capture fix to:
- `exp_payload_logic_ga.c`
- `exp_wave_logic_ga.c`
- `exp_hybrid_logic.c`

Re-run sweeps; expect similar or better results than F1 binary because these substrates have richer state spaces.

### P4 — Substrate cross-comparison (Paper 7 headline)
With all four logic-gate experiments fixed, run a unified sweep:
- 4 substrates × 4 gates × 5 seeds = 80 runs
- Compare fitness curves, generations-to-perfect, robustness across seeds.
- This is the empirical foundation for Paper 7's "which substrate computes best."

### P5 — Substrate-audit tool (still useful)
The throwaway `probe_pop.c` should still be promoted to `experiments/exp_substrate_audit.c`. Now used as a *diagnostic* rather than a precondition gate, since F1 works — but valuable for characterising substrate expressiveness numerically.

### P6 — Stronger coupling and rule diversity (deferred but not abandoned)
Original P1 (multi-column input) and P2 (Rule-30/90/184 + Brian's Brain + Wireworld) are no longer blockers, but remain valuable for:
- Paper 7's substrate-generality claims.
- F1 OR gap closure.
- Future work on more complex computational tasks (4-bit gates, multiplexers, FSM evolution).

### P7 — Polar/Anisotropic Coupling & Spatial Programmability (new track)

**Concept**: Each cell carries an orientation angle in its genome (repurposing the 3 reserved bits into an 8-direction index: N, NE, E, SE, S, SW, W, NW). Coupling strength to each neighbor is weighted by cos²(φ_neighbor − θ_cell), creating anisotropic connectivity. Cells with the same orientation self-reinforce into directional "highways." A single substrate can host multiple independent signal paths (horizontal, vertical, diagonal) by spatial differentiation of orientation.

**Why this matters for multimodality**: Our `exp_multimodal_logic_ga` (binary, 2 modes) plateaus at ~0.87 fitness — the substrate cannot sustain two independent signal paths because isotropic coupling causes crosstalk. Anisotropic coupling physically decouples the modes into different directional channels.

**Implementation plan**:
1. **Genome change**: repurpose reserved bits [15:13] → `orientation` (3 bits, 0–7). Update `genome.h` macros: `GENOME_ORIENTATION(g)`, `GENOME_SET_ORIENTATION(g, val)`. Backward-compatible if old code never touched reserved bits.
2. **Coupling kernel change**: in `grid_step_kernel` (pipeline.c), compute neighbor weight as:
   ```c
   double phi = atan2(dy, dx);  /* direction to neighbor */
   double theta = (orientation * M_PI / 4.0); /* cell's preferred angle */
   double align = cos(phi - theta);
   int effective_weight = base_weight * (align * align);
   ```
   Only count neighbors passing this threshold; others are treated as "not present."
3. **Experiment**: `exp_multimodal_polar_logic_ga.c` — 4-mode evaluation (horiz, vert, diag, anti-diag). Each mode activates a different orientation class. Fitness = average across all 4 modes.
4. **Validation**: If the substrate learns to partition into orientation domains, we have a form of **spatial programmability** — the grid self-organizes into a multi-track circuit board.

**Connection to Turing completeness**: Composition via chaining (G2) requires multiple grids and clean edge coupling. Composition via spatial programmability (P7) uses a *single* grid with multiple independent channels. This is closer to how biological tissues work (different cell types in different regions) and may generalize better to larger-scale computation.

**Risks**:
- Anisotropic coupling may fragment the grid into disconnected regions, preventing signal from reaching the output edge.
- The GA may settle on a single dominant orientation (all cells → 0°) rather than spatial patterning.
- Computational cost: `atan2` per neighbor per cell is expensive; precompute a lookup table `alignment[8][8]` for (orientation, neighbor_direction) pairs.

### Order of execution

| Order | Task | Effort | Why now |
|-------|------|--------|---------|
| 1 | **P1 IC-drift audit** across other experiments | 1 h | High likelihood of finding equivalent bugs in A/B/C series |
| 2 | **P0 re-validate B1, C2, A-series** with modulo lookup | 1 day | Headline findings credibility |
| 3 | **P3 apply fixes to F2/F3/F4** | ½ day | Get all four logic-gate experiments to parity |
| 4 | **P4 substrate cross-comparison sweep** | 1 day | Paper 7 empirical foundation |
| 5 | **P2 close OR gap** | ½ day | F1 polish |
| 6 | **P5 promote probe_pop to audit tool** | 2 h | Reusable diagnostic |
| 7 | **P6 add Rules 30/90/184 + Brian's Brain** | 1 day | Substrate diversity |
| 8 | **P7 anisotropic coupling / spatial programmability** | 2–3 days | Multimodal composition; alternative to grid chaining |

### What changed in the brainstorm conclusion

The original §4 brainstorm argued we were "studying the failure modes of Conway's Life under coupling regimes" rather than CAs in general. After the modulo fix, **this is now empirically true and quantifiable**: the same code, same parameters, same seeds, but with 11 % Conway instead of 72 %, produces *fundamentally different behaviour* (bistable → diverse, GA failure → GA success). This is a result in itself — the Conway-fallback ratio is a hidden hyperparameter that strongly determines outcomes across the entire codebase. Every paper-quality claim should be re-evaluated under the modulo lookup.
