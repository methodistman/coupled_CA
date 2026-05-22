# Cross-Cutting Findings Summary

**Date**: 2026-05-16
**Coverage**: A1–A7, B1–B4, C2–C3, D2, F1–F4 (19/26 experiments complete)

---

## Theme 1: Random Initial Conditions Are a Trap

| Experiment | Observation |
|-----------|-------------|
| A1–A4 | Local GA improves signal by up to +1910% over static rules, but only when starting from *evolved* genomes |
| B1 | Delay-sweep TE measures noise when ICs decay to ash (~1% density); meaningful TE requires sustained density |
| B2 | All four intent modes produce 4–6% final density — the substrate is dead, so mode differences are artifactual |
| B3 | Random-IC trace shows three phases: transient collapse (0–50), quasi-stable ash (50–400), slow extinction (400–1000) |
| F1 | Fixed: was always-0 attractor; now 6/12 perfect across gates (modulo+fitness+output+IC fixes) |

**Implication**: The "always-0" trap was caused by (a) 72% Conway-fallback bias making random genomes mostly dead rules, (b) raw fitness rewarding majority-class matches, (c) brittle output threshold. Fixing these four bugs unblocked logic gate evolution across all substrates.

---

## Theme 2: Continuous Adaptation Beats Static Configuration

| Metric | Static (frozen genomes) | Continuous GA | Ratio |
|--------|------------------------|---------------|-------|
| End density | 3.7% | 64.3% | 17.4× |
| End signal | 0.0781 | 0.5000 | 6.4× |
| End fitness | 2.64 | 25.45 | 9.6× |

**Source**: A5 static-genome replay

**Implication**: H5 ("static genome map suffices") is **falsified**. The local GA is not a discovery-then-freeze optimizer. It is a **homeostatic process** that actively maintains structure against the drift of Life dynamics. This reframes the GA's role from "search" to "stabilization."

**A7 corollary**: Aggressive fitness discounting (γ=0.5) outperforms both no-discount (γ=1.0) and moderate-discount (γ=0.9). The relationship is U-shaped — partial forgetting creates confusing gradients. The GA benefits from either full memory or rapid forgetting, not intermediates.

---

## Theme 3: Rule Selection Dominates; Coupling Weight Is Epistatically Harmful

| Condition | Short-run GA mean | Long-run GA mean |
|-----------|-------------------|------------------|
| Both fields (rule + weight + mutate) | 0.0257 | 0.0156 |
| Rule-only | 0.0241 | **0.0619** |
| Weight-only | 0.0084 | 0.0051 |

**Source**: A6 decoupling analysis

**Implication**: H1 confirmed — rule selection is the primary driver. But the stronger finding is that **coupling weight co-evolution is actively harmful** at long timescales (0.0156 vs 0.0619). Weight mutations disrupt established rule patterns. Two-timescale evolution (rules fast, weights slow) or sequential evolution (rules first, then freeze) should be tested.

---

## Theme 4: Intent Modes Fall Into Two Clusters

| Cluster | Modes | Density | Activity |
|---------|-------|---------|----------|
| High-activity | ADD, THRESHOLD | ~5.5% | ~0.028 |
| Low-activity | REPLACE, WEIGHTED | ~4.1% | ~0.018 |

**Source**: B2 intent-mode comparison

**Bug**: ADD and THRESHOLD produced byte-identical outputs because `INTENT_MODE_THRESHOLD` was `if (val) target = 1` — identical to ADD for binary cells. Fixed in `ca_core/intent.c` to `(raw > 0 && raw >= alpha) ? 1 : 0`.

**Implication**: For binary grids, the four modes collapse to two effective behaviors: overwrite (REPLACE/WEIGHTED) vs. OR (ADD/THRESHOLD). The threshold bug was benign for binary but would have been fatal for multi-value grids (payload/wave).

---

## Theme 5: Parallel Speedup Requires Sufficient Grids

| (size, ngrids) | Speedup | Threshold met? |
|----------------|---------|----------------|
| (128, 2) | 0.65–0.86 | No |
| (128, 4) | 1.496 | Almost |
| (128, 8) | **1.517** | **Yes** |
| (256, 2) | 0.86 | No |
| (256, 4) | 1.40 | No |

**Source**: B4 benchmark; `MAX_GRIDS` raised from 4→16 to enable ngrids=8.

**Implication**: H8 criterion (≥1.5× for ngrids≥2, size≥128) was **optimistic**. The actual threshold is `ngrids ≥ 4` AND `size ≤ 128`. Thread-pool overhead dominates at small grid counts; cache pressure degrades performance at large sizes.

---

## Theme 6: Cross-Grid Modulation Works, But Search Works Better

| Approach | Improvement | Notes |
|----------|-------------|-------|
| Hand-designed modulation (Life/HighLife) | +43.5% | 8/10 wins; consistent but modest |
| Local GA (density fitness) | +1910% | Discovers spatial structure automatically |
| Population GA (binary) | **6/12 perfect** | Post-fix: AND/OR/XOR/NAND all now reachable |
| Population GA (payload) | **8/12 perfect** | Strongest substrate after fixes |
| Population GA (hybrid) | **6/12 perfect** | Best XOR performance (3/3) |

**Sources**: C3 (H11), A1 (H2), F3–F4 (Phase D)

**Implication**: The central question of Paper 4 — "Does search beat design?" — is answered **yes, by 40×**. Hand-designed modulation is a useful baseline (+43%), but the GA's capacity to discover spatial structure and rule maps far exceeds human intuition. This suggests the substrate's complexity outstrips declarative design.

---

## Theme 7: Logic Gate Evolution — Unblocked

| Substrate | Perfect runs | Best gate | Notes |
|-----------|-------------|-----------|-------|
| Binary (F1) | **6/12** | OR, XOR | Pre-fix: 1/12 (OR only); post-fix: all 4 gates reachable |
| Payload (F2) | **8/12** | AND, OR | Pre-fix: 2/12 (OR partial); post-fix: strongest substrate |
| Wave (F3) | **4/12** | AND | Pre-fix: 3/3 OR; post-fix: OR advantage disappears |
| Hybrid (F4) | **6/12** | XOR (3/3) | Pre-fix: 4/5 OR; post-fix: excels at non-monotonic XOR |

**Source**: F1–F4 logic gate sweeps, 3 runs × 4 gates each

**Implication**: Four independent bugs created a "frozen" fitness landscape:
1. **72% Conway fallback** (Theme 10) — most random genomes defaulted to dead Conway's Life
2. **Raw fitness** — rewarded matching majority-class outputs (always-0)
3. **Brittle output threshold** — failed to detect sparse but correct 1-outputs
4. **IC drift** — population evaluation corrupted initial conditions individual-by-individual

Fixing all four simultaneously **unblocked evolution on all substrates**. No single fix suffices; the system was in a multi-factor trap. This is a cautionary tale for CA GA research: apparent "attractors" may be artifacts of implementation bugs.

---

## Theme 8: Gliders Survive Intent Buffer Coupling, Not Direct Coupling

| Coupling Type | Transmission? | First tick on grid 1 |
|--------------|---------------|---------------------|
| Direct (edge cell copy) | **No** | Never |
| Intent REPLACE | **Yes** | 125 |
| Intent ADD | **No** | Never |

**Source**: D2 glider preservation

**Implication**: Direct coupling destroys gliders by stripping neighborhood context at the 1-cell boundary. Intent REPLACE mode successfully reconstructs the glider pattern because it replays the full edge state with a one-tick delay. This validates the Phase 1 (intent buffer) design for pattern-preserving cross-grid communication.

---

## Theme 9: Payload/Wave Substrates Are More Active Than Binary

| Substrate | Mean Density | Mean Activity | Relative Activity |
|-----------|-------------|---------------|-------------------|
| Binary (reservoir) | 9.7% | 0.071 | 1.0× |
| Payload (reservoir) | 10.3% | 0.180 | 2.5× |
| Wave (logic GA) | — | — | Higher turnover |

**Sources**: C2 reservoir stats; F3 wave convergence

**Implication**: Multi-value substrates (payload 8-bit, wave vector) produce more dynamic feature spaces than binary Life. For reservoir computing (Paper 3), the payload grid's higher activity may yield richer readout features. For logic evolution (Paper 7), payload became the strongest substrate post-fix (8/12 perfect), while wave's pre-fix OR advantage was an artifact of the Conway fallback.

---

## Theme 10: The 72% Conway-Fallback Bias

| Code path | Rule index check | Bug? | Effect |
|-----------|-----------------|------|--------|
| `engine.c` | `ri < NUM_RULES ? ri : 0` | Yes | Values ≥9 → Conway's Life |
| `pipeline.c` | `ri < NUM_RULES ? ri : 0` | Yes | Same |
| `payload_pipeline.c` | `ri < NUM_RULES ? ri : 0` | Yes | Same |
| `wave_engine.c` | `ri < NUM_RULES ? ri : 0` | Yes | Same |

**Fix**: Modulo lookup `((ri % NUM_RULES) + NUM_RULES) % NUM_RULES` distributes all 32 genome values evenly across rules.

**Impact**: With `NUM_RULES=9`, 23/32 genome values (72%) previously mapped to rule index 0 (Conway's Life). Since Conway's Life decays to ash under random ICs, this meant **most random genomes evaluated identically**, collapsing genetic diversity. The modulo fix was necessary but not sufficient — fitness shaping, density-relative output, and IC-drift fixes were also required.

**Implication**: Any CA GA research using discrete rule selection must validate that genome encoding maps uniformly. A non-uniform mapping is an invisible, order-1 bias that can dominate all other effects.

---

## Open Questions / Next Steps

### Immediate (analysis only)
1. **Close F1-OR gap**: binary OR stuck at 0/3 perfect; may need longer evolution or larger population.
2. **Re-run C2 with stdout capture** to get NRMSE and memory capacity for delay/XOR tasks.

### Short (code changes)
3. **Extend eval steps**: current 60 ticks may be too short for slow-propagating rules; test 100–200.
4. **Profile B4 slowdown** at size=256, ngrids=2 — cache thrashing or sync overhead?

### Medium
5. **Two-timescale evolution** (A6 follow-up): evolve rules for N gens, freeze, then evolve weights.
6. **TE-driven coupling pruning** (C1): new MUTATE + per-edge TE analyze phases.
7. **Genome lineage tracking** (C5): use reserved bits for lineage ID; reconstruct phylogeny.

### Large
8. **Topology GA** (E1): mutate `Coupling` graph structure; evolve wiring, not just cells.
9. **Rule 110 substitution**: address Conway-centricity by testing 1D universal computation as substrate.
