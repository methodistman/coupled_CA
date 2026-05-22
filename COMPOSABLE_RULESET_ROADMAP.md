# Composable CA Ruleset + 3-Stage Hierarchical GA — Roadmap

**Status**: Stage 0 implementation complete; full Stage-0 run in progress.

## Implementation status

| Component                                              | Status   | Notes                                                                                                       |
|--------------------------------------------------------|----------|-------------------------------------------------------------------------------------------------------------|
| `ca_core/composable_ruleset.h/c`                       | ✅       | 56-bit DNA: birth/survive masks, aniso, card/diag bias, refractory, noise, meta. Pack/mutate/crossover/step. |
| `tests/test_composable_ruleset.c`                      | ✅       | 7/7 pass: Conway blinker, LWD monotone, refractory, anisotropy block, pack roundtrip                        |
| `experiments/exp_composable_stage0.c`                  | ✅       | GA with orientation-sensitivity fitness v2 (`reach_aligned × (1−reach_rand) × (1−reach_adv)`)                 |
| Stage-0 smoke run (size=24, pop=30, gens=20)           | ✅       | fitness 0.27 → 0.83 (v1 formula); `B=0xFE S=0xAF aniso=148` (fill-everything, v1)                           |
| Stage-0 v2 run #1 (size=32, pop=100, gens=100, K=8, seed=42) | ✅ | fitness 0.4657; `B=0x88 S=0xAE aniso=255 card=0 refr=6 noise=76` (non-trivial, orientation-sensitive)     |
| Stage-0 systematic sweep (4 seeds × 3 steps × 2 formulas) | 🟡       | design complete; see §11 Sweep Plan and SWEEP_DESIGN.md                                                     |
| `experiments/exp_composable_stage1.c`                  | ✅       | scaffolding complete; blocked on finding non-trivial R*                                                     |
| `experiments/exp_composable_stage2.c`                  | ❌       | pending Stage-1 results                                                                                     |

---
**Goal**: end-to-end evolution of an XOR-computing 4-mode multimodal logic system, with the *propagation substrate itself* discovered by GA rather than hand-picked from the legacy 9-rule `RULE_TABLE`.

**Motivating failure**: H8 (3333-gen run of `exp_directional_multimodal`) plateaued at 0.875 fitness. Per-mode XOR was discovered 22–48 % of the time but *never simultaneously across all 4 modes*. Root cause: the GA had to discover (a) per-cell rule selection from a Conway-family-only table, (b) anisotropic orientation routing, and (c) directional rule switching, **simultaneously**, with a deceptive fitness landscape. None of the 9 hardcoded rules are well-suited to thin-signal propagation under anisotropic gating, so Stage 1 of the naive hierarchical GA (H7) also fails (flat 0.5 fitness — see `data/h7_run.csv`).

The fix: replace the hardcoded rule table with a **parameterized 6-layer composable ruleset** and decompose the search into **3 stages**.

---

## 1. The 6-layer ruleset

Each layer is an atomic outer-totalistic-or-anisotropic operator producing a contribution in `{-1, 0, +1}` to the next-state decision. The 6 layers + 1 meta byte form a **56-bit ruleset DNA** (`uint64_t`, upper 8 bits reserved).

| # | Bits   | Layer                  | 8-bit param `pᵢ` interpretation                                                              | Why                                                                            |
|---|--------|------------------------|----------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| 1 | 0–7    | **Birth bitmask**      | Bit *k* set ⇒ cell is born when neighbor count = *k* (k ∈ 0..7; 8 collapses to bit 7)        | Subsumes all Conway-family birth rules in one byte (B3 = 0x08, B36 = 0x48 …)   |
| 2 | 8–15   | **Survive bitmask**    | Bit *k* set ⇒ alive cell survives with neighbor count = *k*                                  | Same, for survival (S012345678 = 0xFF = Life-Without-Death; S23 = 0x0C)        |
| 3 | 16–23  | **Aniso strength**     | `p₃ / 255` blends raw count ↔ orientation-gated count                                        | Lets GA tune *how much* anisotropy biases dynamics; 0 = isotropic, 255 = strict |
| 4 | 24–31  | **Cardinal/diag bias** | Cardinal-neighbor weight `p₄`, diagonal weight `255 − p₄` (rescaled into [0,1])              | Many gliders/highways need cardinal-only or diagonal-only coupling             |
| 5 | 32–39  | **Refractory ticks**   | `p₅ >> 5` ∈ [0,7] ticks of post-death birth suppression                                      | Enables wave/glider propagation; classical CAs can't do this                   |
| 6 | 40–47  | **Noise**              | Random bit flip with probability `p₆ / 65535`                                                | Stochastic resonance; escape attractors; some logic regimes need noise         |
| M | 48–55  | **Meta**               | Global activation threshold + layer-active mask (top 4 bits = mask, bottom 4 bits = bias)    | GA can turn off layers or shift overall excitability                           |

**Step function (sequential pipeline variant A)**:

```
1. Read 8 neighbors. Compute:
     raw_count       = Σ neighbor.alive
     weighted_count  = Σ neighbor.alive · alignment_scale[orient][dir(n)] / 16
     gated_count     = lerp(raw, weighted, p₃/255)
     biased_count    = gated_count adjusted by p₄ split between cardinal/diagonal
2. count := round(biased_count)
3. If layer 5 active AND (refractory_age[idx] > 0): next := 0; refractory_age-- ; return
4. born    = (birth_mask  >> count) & 1
   survive = (survive_mask >> count) & 1
5. next := alive ? survive : born
6. If layer 6 active: with prob p₆/65535 flip next
7. If next==0 && alive: refractory_age[idx] := p₅>>5
```

This is **variant A (sequential pipeline)**: each layer is a discrete operator applied in order. Easier to debug than weighted-vote (B) for a first build.

---

## 2. Storage model

- **Homogeneous (per-grid) ruleset**: a single `Ruleset { uint8_t p[7]; }` (56 bits) shared by every cell in the grid. Stored in `Grid` struct, NOT in `CellGenome`.
- **Per-cell orientation field**: stays in the existing 48-bit `CellGenome` (4 × 3-bit orientations).
- **Per-cell logic params**: existing 8-bit `alt_rule_select` + 4-bit `dir_mask` in `CellGenome` — but in this design, `alt_rule_select` indexes into a *second* ruleset for the directional-switching feature. We will store **two** rulesets per grid: `r_primary`, `r_alt`. Per-cell `dir_mask` still selects when the alt fires.

Total spatial-program state per cell stays at 48 bits. New global state per grid: **2 × 56 = 112 bits** for the ruleset pair.

---

## 3. Three-stage GA

### Stage 0 — Ruleset discovery (NEW)

| | |
|--|--|
| **Genome** | 56-bit primary ruleset (homogeneous) |
| **Population** | 100 |
| **Generations** | 100 (budget: small, this is a discovery search) |
| **Fitness v2** | **`fitness = reach_aligned × (1 − reach_random) × (1 − reach_adversarial)`**. For each mode, evaluate three orientation configurations: (a) uniform aligned (cells oriented toward sink), (b) uniform adversarial (perpendicular), (c) K=8 random fields. The v1 formula (`0.5·reach_aligned·spec + 0.5·reach_rand·spec`) discovered trivial fill-everything rulesets (`B=0x7C S=0xFF`, fitness 0.83) because `reach_rand` was rewarded. v2 penalizes high random-field reach, forcing the GA to find rules that propagate *only* under aligned orientations — creating a real gradient for Stage 1. Run #1 (seed=42) found `B=0x88 S=0xAE aniso=255 card=0 refr=6 noise=76` with fitness 0.4657, a genuinely orientation-sensitive ruleset. |
| **Selection** | Tournament-3, elite-2, mutation rate 0.05 per byte (Gaussian Δ around current value, clipped) |
| **Output**    | `data/composable_R_star.bin` — the discovered "good propagation" ruleset |

**Fitness specifics**:
- Use a *fixed* set of 8 random orientation fields (seeded) so Stage 0 ruleset comparisons are deterministic.
- Each individual is evaluated as: `mean over (orient_field, mode) of reachability(R, orient_field, mode)`.
- This makes the fitness a property of the *ruleset alone*, not specialized to any orientation pattern.

**Success criterion**: at least one ruleset achieves mean reachability ≥ 0.50 over the 8 fields. Empirically, Life-Without-Death-style rulesets (`B=0x08, S=0xFF`) get ~0.05 with anisotropic gating; we expect the GA to find configurations exploiting layers 3, 4, and 5 that reach 0.5+.

### Stage 1 — Orientation field evolution

| | |
|--|--|
| **Genome** | Per-cell 4×3-bit orientation field; size² × 12 bits |
| **Population** | 200 |
| **Generations** | 300 |
| **Fitness** | `min over m of reachability(R*, orient_field, m)` — **min** is correct here because Stage 0 guaranteed `R*` *can* propagate |
| **Output** | `data/composable_orientations.bin` |

### Stage 2 — Logic params evolution

| | |
|--|--|
| **Genome** | Per-cell `alt_rule_select` (8 bits) + `dir_mask` (4 bits). Per-grid `R_alt` (56 bits) global. |
| **Population** | 200 |
| **Generations** | 500 |
| **Fitness** | Shaped 4-mode XOR truth-table accuracy (existing H8 fitness function) |
| **Output** | `data/composable_xor_genome.bin` |

**Total budget**: ~100 × 100 + 200 × 300 + 200 × 500 = **170 K evaluations** (vs. H8's 200 K, comparable).

---

## 4. Files to create

```
ca_core/composable_ruleset.h       — Ruleset struct, step function, mutation/crossover
ca_core/composable_ruleset.c       — Implementation of 6 layers + meta + step
tests/test_composable_ruleset.c    — Unit tests: B3/S23 emulation, LWD emulation, refractory wave propagation
experiments/exp_composable_stage0.c — GA over ruleset DNA, fixed random orientations
experiments/exp_composable_stage1.c — GA over orientations, fixed R*
experiments/exp_composable_stage2.c — Full hierarchical with frozen R*, frozen orientations, evolves logic
COMPOSABLE_RULESET_ROADMAP.md      — this file
data/composable_R_star.bin
data/composable_orientations.bin
data/composable_xor_genome.bin
data/composable_FINDINGS.md        — analysis after all 3 stages complete
```

---

## 5. Build & test plan

1. **`composable_ruleset.h/c`** with the 6-layer step function and a refractory-age side buffer in `Grid` (or a new `Grid::refractory[ncells]` field).
2. **Unit tests** verifying:
   - `Ruleset{B=0x08, S=0x0C, aniso=0, ...}` == Conway's Life (compare against existing engine for 50 ticks on a glider).
   - `Ruleset{B=0x08, S=0xFF, ...}` == Life-Without-Death.
   - Refractory layer suppresses immediate rebirth (count alive cells over 10 ticks of a pulse).
   - Anisotropic strength=255 zeroes out perpendicular-neighbor contributions.
3. **Stage 0 experiment** end-to-end on `size=24, K=8 fields, 50 gens` smoke test before full 100-gen run.
4. **Stage 1** with frozen `R*` from step 3, verify `min reachability` improves from generation 0.
5. **Stage 2** with frozen R\* + orientations, target ≥ 1 mode at XOR (`truth=0x6`), goal: all 4 modes XOR.

---

## 6. Success criteria

| Stage | Metric                                              | Target              |
|-------|-----------------------------------------------------|---------------------|
| 0     | Mean reach over 8 fields × 4 modes                  | ≥ 0.50              |
| 1     | `min` reach over 4 modes for best individual        | ≥ 0.80              |
| 2     | Number of modes with XOR truth table (0x6)          | ≥ 2 (stretch: 4)    |
| 2     | Shaped fitness                                       | ≥ 0.90 (stretch: 1.0) |

If Stage 2 achieves all-4-mode XOR, this would be the **first decisive demonstration of the hierarchical-evolution hypothesis** and the **first compositional multimodal logic** in this codebase.

---

## 7. Risk register

| Risk | Mitigation |
|------|------------|
| Stage 0 fitness landscape flat (no ruleset propagates ≥ 0.5) | Add more permissive birth bitmasks; allow B0/B1; increase noise budget |
| Stage 0 ruleset overfits to random fields (Stage 1 stalls) | Include adversarial orientation fields in fitness; periodically re-sample |
| Stage 1 plateaus at `min < 0.5` because some mode is geometrically harder | Switch to weighted-min (e.g., 0.7·min + 0.3·mean); revisit source/sink geometry |
| Stage 2 still can't find XOR despite good substrate | Indicates logic-primitive layer needed (parity layer #6 from full 12-layer brainstorm); add it back |
| Build complexity / time | All layers are O(1) per cell; total step cost is ~2× current `grid_step_kernel` |

---

## 8. Relation to existing work

- **H7 (current `exp_hierarchical.c`)**: superseded. Will be archived once composable Stage-1 runs.
- **H8 (`exp_directional_multimodal.c`)**: kept as the **flat-search baseline** to compare against.
- **48-bit genome**: unchanged for per-cell fields. Ruleset DNA is *new global state*, not a genome extension.
- **Anisotropic kernel (`pipeline.c`)**: replaced by the composable step function inside `grid_step_kernel`; the existing `KERNEL_USE_ORIENTATION` flag becomes equivalent to "Ruleset with layer-3 active and `p₃ = 255`".

---

## 9. Open questions (to revisit before/during implementation)

1. Should Stage 0 evolve `R_alt` too, or fix `R_alt = R*` initially and let Stage 2 evolve it?
2. Should the meta byte's "layer-active mask" be hard-coded to all-on for first run? (Saves search space.)
3. Per-grid ruleset vs. per-region (e.g., 4 quadrants with different rulesets)? Per-region is a Stage 4 extension.
4. Should `alt_rule_select` (8 bits) be interpreted as an *index* into a small palette of Stage-0-discovered rulesets, rather than a free byte? Could give Stage 2 a richer logic-primitive vocabulary.

---

## 10. Decision log

- **2026-05-18**: Decision to start with 6 layers (not 12). Rationale: 6 are demonstrably propagation-relevant; computation-oriented layers (parity, mode-cross, tier-2) deferred until Stage 2 fails without them. See chat for full reasoning.
- **2026-05-18**: Decision: variant A (sequential pipeline) over B (weighted-vote). Easier to debug, more interpretable.
- **2026-05-18**: Decision: homogeneous per-grid ruleset, not per-cell. Per-cell ruleset = 56 KB/genome × 200 pop = 11 MB; intractable.
- **2026-05-18**: Decision: v1 fitness formula rejected. It rewarded `reach_rand` equally, producing fill-everything rulesets (`B=0x7C S=0xFF`) with no orientation gradient for Stage 1.
- **2026-05-18**: Decision: v2 fitness formula (`reach_aligned × (1−reach_rand) × (1−reach_adv)`). Forces orientation-sensitivity by penalizing high random-field reach. Run #1 produced `B=0x88 S=0xAE aniso=255 card=0 refr=6`, a non-trivial ruleset.
- **2026-05-18**: Decision: systematic sweep required. One seed, one step count, one fitness patch is insufficient data in a 56-bit space. A structured sweep will catalogue the search landscape and avoid premature conclusions.

---

## 11. Systematic Sweep Plan (Stage 0)

**Motivation**: A 56-bit ruleset space has ~7.2×10¹⁶ possible configurations. One GA run with one seed and one parameter set is a single sample. To avoid overfitting our conclusions to stochastic outliers, we need a structured sweep that varies seeds, evaluation conditions, and fitness formulations, then catalogues the results.

### 11.1 Variables and levels

| Variable | Symbol | Levels | Rationale |
|----------|--------|--------|-----------|
| **Seed** | `s` | 42, 123, 2025, 9999 | GA stochasticity; best ruleset from one seed may be an outlier |
| **Eval steps** | `T` | 30, 50, 100, 200 | Short steps reward fast waves; long steps test stable highways |
| **K fields** | `K` | 4, 8, 16, 32 | More fields = less noise in fitness, but slower eval |
| **Grid size** | `N` | 24, 32, 48, 64 | Propagation distance scales with size |
| **Mutation rate** | `mu` | 0.05, 0.15, 0.30 | Ruleset DNA has 7 bytes; rate changes search aggressiveness |
| **Fitness formula** | `F` | v1, v2, v3, v4 | The fitness landscape is the primary filter on what the GA discovers |
| **Seed-conway baseline** | `SC` | on, off | Does seeding with Conway/LWD bias toward local optima? |

### 11.2 Fitness formula variants

| Variant | Formula | Expected behavior |
|---------|---------|-------------------|
| **v1** (rejected) | `0.5·reach_aligned·(1−reach_adv) + 0.5·reach_rand·(1−reach_adv)` | Rewards fill-everything; high `reach_rand` is treated positively |
| **v2** (current) | `reach_aligned × (1−reach_rand) × (1−reach_adv)` | Penalizes random-field propagation; forces orientation-sensitivity |
| **v3** (proposed) | `reach_aligned × (1−reach_rand)² × (1−reach_adv)` | Stronger penalty for random-field reach; sharper discrimination |
| **v4** (proposed) | `reach_aligned × (1−reach_adv)` (drop random term entirely) | Only aligned vs adversarial; simplest, but ignores random-field robustness |

### 11.3 Minimal informative sweep

A tractable first sweep: **4 seeds × 3 step counts × 2 fitness variants = 24 runs**.

```
for s in 42 123 2025 9999; do
  for T in 30 50 100; do
    for F in v2 v3; do
      exp_composable_stage0 --size 32 --pop 100 --gens 100 --kfields 8 --steps $T --seed $s --save data/sweep/s_${s}_T_${T}_F_${F}.bin > data/sweep/s_${s}_T_${T}_F_${F}.csv 2> data/sweep/s_${s}_T_${T}_F_${F}.log
    done
  done
done
```

Each run: ~5–10 minutes. Total wall time: ~2–4 hours if sequential, or ~30–60 minutes if parallelized on available cores.

### 11.4 Output structure

```
data/sweep/
  s_42_T_30_F_v2.{csv,log,bin}
  s_42_T_30_F_v3.{csv,log,bin}
  ... (24 files)
  summary.md  — aggregate table of best ruleset per run, fitness, B/S/aniso/card/refr/noise
```

### 11.5 Analysis checklist

For each run, record:
1. Best fitness at generation 99 (or convergence gen)
2. Best ruleset parameters (B, S, aniso, card, refr, noise)
3. Is it fill-everything? (`S=0xFF` and `B` has many bits set)
4. Is it orientation-sensitive? (compare `reach_aligned` vs `reach_rand` in the log)
5. Convergence speed (gen where fitness plateaus)

Aggregate across seeds:
- Which parameters are **conserved** across seeds? (These are robust features of the fitness landscape.)
- Which vary wildly? (These are noise or local optima.)
- Which fitness formula produces the highest-quality (non-trivial, orientation-sensitive) rulesets?

---

## 12. Pulse-and-Reset Evaluation (proposed Stage-0 v4)

### 12.1 Problem with current evaluation

The current `evaluate_one` `@/home/gregory/code/wavebuffer/coupled_ca/experiments/exp_composable_stage0.c:86-107` clears the grid, injects the source once at `t=0`, and runs for `eval_steps` ticks. It rewards **any** mechanism that pushes activity from source to sink — a transient wave, a chaotic expanding blob, or a stable highway all score identically.

This is insufficient for Stage 1. Stage 1 needs **stable highways** — persistent, reliable channels that can be tuned by per-cell orientations. A transient wave that happens to reach the sink once gives Stage 1 no substrate to improve.

### 12.2 Concept: periodic reset

Instead of a single long run, **clear the grid every `R` ticks and re-inject the source**. The ruleset must re-form the highway from scratch each pulse. A transient wave dies on reset (scores 0 for that pulse). A stable highway builder re-forms reliably (scores well every pulse).

```c
static double evaluate_one_periodic(const Ruleset *r, Grid *g,
                                    const uint8_t *orient_field,
                                    TraceType t, int total_steps,
                                    int reset_interval,
                                    uint8_t *refr_buf) {
    int ncells = g->size * g->size;
    double cumulative_reach = 0.0;
    int num_pulses = 0;

    for (int pulse = 0; pulse < total_steps; pulse += reset_interval) {
        memset(g->cells, 0, ncells);
        memset(g->next_cells, 0, ncells);
        if (refr_buf) memset(refr_buf, 0, ncells);
        inject_source(g, t);

        int run_steps = (pulse + reset_interval > total_steps)
                        ? (total_steps - pulse) : reset_interval;
        for (int s = 0; s < run_steps; s++)
            ruleset_step_grid(r, g, /*mode=*/0, refr_buf);

        cumulative_reach += measure_sink(g, t);
        num_pulses++;
    }
    return cumulative_reach / num_pulses;
}
```

### 12.3 Path-coherence bonus

In addition to sink reach, measure **path coherence**: count alive cells along the canonical straight-line path from source to sink. A highway has high coherence; a blob has low coherence.

```c
fitness = 0.7 * mean_sink_reach + 0.3 * mean_path_coherence
```

This explicitly rewards **channel-like** structures.

### 12.4 Why this changes the landscape

| Property | Single long run (v2/v3) | Periodic reset (v4) |
|----------|------------------------|---------------------|
| Transient wave | Scores well | Dies on reset; scores 0 |
| Stable highway | Same score as wave | Survives reset; scores well |
| Chaotic blob | May reach sink | Unlikely to re-form identically |
| Fill-everything | Penalized by v2/v3 | Also penalized; re-forming from seed is harder |
| What Stage 1 sees | Maybe a wave, maybe a highway | Definitely a highway substrate |

### 12.5 Recommended reset interval

| Grid size | `reset_interval` | Rationale |
|-----------|-----------------|-----------|
| 32×32 | 15–25 ticks | ~1 tick per cell for a clean wave; allows slight delay |
| 48×48 | 20–35 ticks | Scales with diagonal distance |
| 64×64 | 30–50 ticks | Longer channels need more time |

Start with **R=20** on 32×32.

### 12.6 Initial sweep results (2026-05-20, 24/32 runs)

A mini-sweep tested R ∈ {10, 15, 20} × 4 seeds × 2 coherence weights.

| R | Mean Best | Peak | Interpretation |
|---|-----------|------|----------------|
| **10** | **0.448** | **0.5038** | Tightest window; easiest to evolve. Rulesets only need 10 ticks of stability. |
| 20 | 0.408 | 0.4707 | Some runs incomplete. Longer highways are harder to build reliably. |
| 15 | 0.370 | 0.4715 | "Uncanny valley" — harder than R=10 without improving peak. |

**Coherence weight c=0.3 lowers mean fitness by ~20%** (0.359 vs 0.448). It successfully penalizes blobs but makes the landscape harder to climb. Recommendation: use c=0.0 for Stage 0 discovery, then re-evaluate top candidates with c=0.3 as a filter.

**Peak fitness ~0.50** — roughly half the old v3-T50 peak (0.9688). This confirms pulse-and-reset is selecting for something genuinely harder than transient waves.

**Refractory values are low (3–5)** across all top rulesets, suggesting rapid cycling is necessary for re-formation after reset.

**Open questions**: Will R=30 close the gap? Do R=10 winners generalize to longer pulses? Are they genuine highways or fast-expanding blobs?

---

## 13. Two-Phase Ruleset (Genome Doubling)

### 13.1 Motivation

A single 7-byte ruleset must simultaneously:
1. **Build** a highway from a cold start (aggressive birth, permissive survival)
2. **Maintain** the highway once formed (conservative dynamics, self-sustaining)

These are conflicting requirements. A build-optimized ruleset may be chaotic; a maintain-optimized ruleset may never bootstrap from an empty grid.

### 13.2 Concept: two-phase ruleset

Double the ruleset DNA: two independent 7-byte phases plus a meta byte that controls switching.

```c
typedef struct {
    uint8_t p[7];   /* Phase 0: BUILD — active for first N ticks after reset */
    uint8_t q[7];   /* Phase 1: MAINTAIN — active thereafter */
    uint8_t meta;   /* switch_timer (4 bits) + activation masks (4 bits) */
} Ruleset2Phase;
```

Total: 15 bytes = 120 bits. Still fits in two `uint64_t`s for serialization.

### 13.3 How it works

After each grid reset + source injection:
- For `switch_timer` ticks, use Phase 0 (`p`) to aggressively build the highway.
- After `switch_timer`, use Phase 1 (`q`) to stabilize and maintain it.

The GA can evolve:
- `p` specialized for **bootstrap** (high birth, low refractory, strong anisotropy)
- `q` specialized for **maintenance** (moderate survival, noise suppression, long-term stability)

### 13.4 GA operators for 2-phase

| Operator | Action |
|----------|--------|
| **Mutate p** | Per-byte mutation on build-phase params |
| **Mutate q** | Per-byte mutation on maintain-phase params |
| **Mutate switch** | Change `switch_timer` ±1 |
| **Crossover** | Two-point: swap p, swap q, or swap both |
| **Phase duplication** | Copy p→q or q→p, then both diverge (gene duplication) |
| **Phase loss** | Set p=q (collapse to single phase, but retain ability to re-diversify) |

### 13.5 Encoding / packing

```c
/* Pack into two uint64_t values. */
static inline void ruleset2phase_pack(const Ruleset2Phase *r, uint64_t *out_hi, uint64_t *out_lo) {
    *out_lo = 0;
    for (int i = 0; i < 7; i++) *out_lo |= ((uint64_t)r->p[i]) << (i * 8);
    *out_hi = r->meta;
    for (int i = 0; i < 7; i++) *out_hi |= ((uint64_t)r->q[i]) << (i * 8 + 8);
}
```

### 13.6 Risk and mitigation

| Risk | Mitigation |
|------|------------|
| 15 bytes = larger search space | Start with fixed `switch_timer=10` to reduce dimensionality; evolve p and q first |
| Phase 0 or 1 becomes degenerate | Enforce minimum Hamming distance between p and q via fitness penalty |
| Packing complexity | Keep two `uint64_t`s; serialize to 16 bytes; straightforward |

### 13.7 Decision: when to implement

**Priority order:**
1. **First**: Implement pulse-and-reset with the existing 7-byte ruleset. If Stage 1 still shows no gradient, proceed to step 2.
2. **Second**: Implement 2-phase ruleset. This is the most likely fix if the problem is "can't build AND maintain with one ruleset."
3. **Third**: Add lifespan layer (§14) as a last resort if the problem is "highways are unstable over long runs."

---

## 14. Maximal Lifespan Layer (proposed Layer 7, deferred)

### 14.1 Problem

Even with v2 fitness, a ruleset can still fill the grid under aligned orientations if it has permissive birth (`B` with many bits) and generous survival. The v2 formula penalizes *random-field* fill, but under aligned orientations, a thick wavefront may still grow uncontrollably. Stage 1 then has no gradient because the sink is reached regardless of orientation tuning.

### 14.2 Concept

Add a **maximal lifespan** cap: each cell tracks `age` (ticks since birth). When `age > L_max`, the cell is forced to die regardless of neighbors. This transforms static fill into a **traveling wavefront** — a pulse that must traverse the grid before its constituent cells die.

### 14.3 Why it helps

| Property | Without lifespan | With lifespan |
|----------|------------------|---------------|
| Static fill | Possible (cells survive forever) | Impossible (cells die at `L_max`) |
| Wavefront shape | Thick, uncontrolled | Thin, velocity-dependent |
| Orientation sensitivity | Weak (thick wave goes everywhere) | Strong (wave follows alignment or dies) |
| Stage 1 gradient | Flat (sink always reached) | Steep (bad orientations → wave dies before sink) |
| Refractory interaction | Independent tuning | Coupled: `L_max` + `refr` together set wave speed |

### 14.4 Implementation

Add `uint8_t lifespan_age[ncells]` parallel to `refractory_buf`. Cost: 1 byte per cell.

```c
/* In ruleset_step_cell: */
if (layer_lifespan_on && alive) {
    if (lifespan_age > L_max) {
        next = 0;           /* forced death */
        *new_lifespan = 0;
    } else {
        (*lifespan_age)++;  /* aging */
    }
}
/* On birth: */
if (!alive && next) {
    *new_lifespan = 0;      /* newborn cells start at age 0 */
}
```

Encoding: `p[4]` currently stores refractory ticks in bits 5–7. We could repurpose bits 0–4 for lifespan (0–31 ticks), or add an 8th byte. Since the ruleset is 56 bits in a `uint64_t` with 8 upper bits free, a clean option is:
- **Option A**: Use upper byte of `uint64_t` (bits 56–63) for lifespan `L_max` (0–255 ticks)
- **Option B**: Split `p[4]` into `refr_ticks (3 bits)` + `lifespan (5 bits)` = 0–31 ticks. Limited range.
- **Option C**: Extend to 64-bit ruleset with `p[7]` = lifespan. This is a **Layer 7** byte.

Recommended: **Option C** — add `p[7]` as the lifespan layer. This makes the ruleset a full **64-bit DNA**, which is still a single `uint64_t` with all bits used. No packing changes needed; we just start using the previously reserved top byte.

### 12.5 Risk: landscape collapse

If `L_max` is too short relative to `grid_size / wave_speed`, no ruleset can reach the sink, and fitness becomes all-zero everywhere. Mitigations:

1. **Adaptive `L_max`**: Set `L_max = grid_size * C` where `C` is a tuneable constant (e.g., 2–4× grid size in ticks). This makes the time budget scale with the propagation challenge.
2. **Layer-deactivatable**: The lifespan layer must have an on/off bit in the meta byte, so a ruleset can evolve with lifespan disabled while it first discovers wave propagation, then later discover lifespan tuning.
3. **Soft lifespan**: Instead of hard death at `L_max`, probability-of-death increases smoothly. More complex but avoids cliff edges.

### 14.6 Experiment plan for lifespan

Before implementing, test the hypothesis manually:

1. Hand-tune `B=0x08 S=0x0C aniso=255 card=0` (Conway with full anisotropy, diagonal-only)
2. Set `L_max = 64` on a 32×32 grid
3. Inject single pulse at left edge, measure sink reach over 100 steps
4. Vary `L_max`: 32, 64, 128, 256
5. Observe: does sink reach drop sharply when `L_max < propagation_time`?

If yes, lifespan is worth implementing. If no, the wave speed is too high or too low for the effect to matter.

**Status**: Deferred. Pulse-and-reset + path-coherence (§12) and 2-phase ruleset (§13) are higher priority because they address the root cause (unstable highways) more directly.

---

## 15. Open questions (revisited)

1. ~~Should Stage 0 evolve `R_alt` too, or fix `R_alt = R*` initially and let Stage 2 evolve it?~~ Defer to Stage 2.
2. ~~Should the meta byte's "layer-active mask" be hard-coded to all-on for first run?~~ Yes, hard-coded for now.
3. ~~Is the v2 fitness sufficient, or does the systematic sweep reveal that v3 or v4 is needed?~~ Sweep confirms **v3 + T=50** is the sweet spot (mean 0.573, peak 0.9688).
4. ~~Does the lifespan layer (Layer 7) improve Stage 0/1 gradient enough to justify implementation?~~ Deferred; pulse-and-reset (§12) addresses the same problem more directly.
5. ~~Should Stage 0 runs be capped at convergence rather than fixed 100 gens, to save budget?~~ 300-gen runs show convergence by ~gen 60; 100 gens is sufficient.
6. ~~Should we save the full population (not just best) from Stage 0, to give Stage 1 a palette of good rulesets?~~ Not yet; first test if the single best R* works.
7. **NEW**: Does pulse-and-reset evaluation (§12) produce rulesets that create stable highways for Stage 1?
8. **NEW**: Does the 2-phase ruleset (§13) outperform the single-phase ruleset for highway building?
9. **NEW**: What is the optimal `switch_timer` for the 2-phase ruleset (build → maintain transition)?

---

## 16. Files to create (updated)

```
ca_core/composable_ruleset.h              — existing
ca_core/composable_ruleset.c              — existing
ca_core/composable_ruleset_2phase.h       — NEW: 2-phase 15-byte ruleset
ca_core/composable_ruleset_2phase.c       — NEW: 2-phase step + GA operators
tests/test_composable_ruleset.c           — existing
experiments/exp_composable_stage0.c       — existing (add --reset-interval, --path-coherence)
experiments/exp_composable_stage1.c       — existing
experiments/exp_composable_stage2.c     — pending
COMPOSABLE_RULESET_ROADMAP.md             — this file
SWEEP_DESIGN.md                           — standalone sweep methodology
PULSE_RESET_DESIGN.md                     — NEW: standalone pulse-reset + 2-phase design
data/composable_R_star.bin                — latest best ruleset (v3, T=50, seed=9999)
data/composable_orientations.bin          — pending non-trivial R*
data/composable_xor_genome.bin            — pending Stage 2
data/sweep/                               — 100-gen systematic sweep results (24 runs)
data/sweep300/                            — 300-gen confirmation runs (24 runs)
data/sweep_new/                           — new combo runs (grid size, K, mutation variants)
data/composable_FINDINGS.md               — analysis after all 3 stages complete
```
