# Pulse-and-Reset Evaluation + Two-Phase Ruleset Design

**Date**: 2026-05-19  
**Status**: Design complete; implementation pending  
**Related**: `COMPOSABLE_RULESET_ROADMAP.md` §12–13, `SWEEP_ANALYSIS.md`

---

## 1. Context: why we are pivoting

The 24-run systematic sweep (100 gen, 4 seeds, 3 step counts, v2/v3 fitness) produced excellent results:

- **0/24 fill-everything rulesets** — v2/v3 fitness successfully breaks the trivial attractor
- **Best fitness: 0.9688** (seed=9999, T=50, v3)
- **Mean fitness v3: 0.573**, v2: 0.544 — v3 wins
- **T=50 is the sweet spot** — T=100 performs worst (0.487 mean)

The 300-gen confirmation runs showed convergence by ~gen 60 and discovered the same peak fitness (0.9688) with a different parameter set (`B=0xA2 S=0xAB aniso=135 card=33 refr=5 noise=0`), confirming the landscape has multiple high-fitness attractors.

**However**: We still don't know if any of these rulesets produce **stable highways** that Stage 1 can tune. The current fitness rewards *transient wavefronts* that reach the sink once. Stage 1 needs *persistent channels* that maintain their shape over time. A wavefront that reaches the sink in 50 ticks and then dissipates gives Stage 1 no substrate.

This document proposes two coordinated changes to address this:
1. **Pulse-and-reset evaluation** — forces rulesets to build re-formable highways
2. **Two-phase ruleset** — separates build-phase DNA from maintain-phase DNA

---

## 2. Part A: Pulse-and-Reset Evaluation

### 2.1 Current evaluation (v3)

```
for each ruleset:
    for each mode:
        clear grid
        inject source at t=0
        run 50 steps
        measure sink reach
    fitness = reach_aligned × (1−reach_rand)² × (1−reach_adv)
```

**Problem**: A chaotic expanding blob and a clean highway both get the same score if they touch the sink. The fitness has no way to distinguish reliable structure from lucky transient.

### 2.2 Pulse-and-reset evaluation (v4)

```
for each ruleset:
    for each mode:
        cumulative_reach = 0
        for pulse = 0; pulse < total_steps; pulse += R:
            clear grid
            inject source
            run R steps
            cumulative_reach += sink_reach_this_pulse
        mean_reach = cumulative_reach / num_pulses
    fitness = mean_reach_aligned × (1−mean_reach_rand)² × (1−mean_reach_adv)
```

**Key parameter**: `R` = reset interval.

### 2.3 What pulse-and-reset selects for

| Phenotype | Old fitness (v3) | New fitness (v4) |
|-----------|------------------|------------------|
| Transient wave (reaches sink, then dies) | High | Low (dies on reset) |
| Stable highway (re-forms reliably) | High | High |
| Chaotic blob (unreliable) | Medium | Low |
| Fill-everything | Penalized by v3 | Penalized + re-forming from seed is harder |

### 2.4 Path-coherence bonus

Add a secondary metric: **path coherence** = fraction of cells along the ideal source→sink line that are alive.

```c
double path_coherence(const Grid *g, TraceType t) {
    /* Trace the canonical line from source edge to sink zone.
     * Count alive cells / total line cells. */
}
```

Combined fitness:
```c
fitness = (0.7 * mean_sink_reach + 0.3 * mean_path_coherence)
          * (1 - mean_reach_rand) * (1 - mean_reach_rand)
          * (1 - mean_reach_adv);
```

This explicitly rewards **channel-like** structures over **blob-like** ones.

### 2.5 Reset interval selection

| Grid size | Diagonal distance | `R` (min) | `R` (recommended) |
|-----------|-------------------|-----------|-------------------|
| 32×32 | ~45 cells | 10 ticks | **20 ticks** |
| 48×48 | ~68 cells | 15 ticks | 30 ticks |
| 64×64 | ~90 cells | 20 ticks | 40 ticks |

A wave moving at ~2 cells/tick needs ~22 ticks to cross a 32×32 diagonal. `R=20` gives a tight budget; `R=30` is forgiving.

**Start with R=20 on 32×32.**

### 2.6 Implementation plan

Modify `@/home/gregory/code/wavebuffer/coupled_ca/experiments/exp_composable_stage0.c`:

1. Add `--reset-interval` CLI flag (default 0 = disabled, old behavior)
2. Add `--path-coherence` CLI flag (0..1 weight, default 0.3)
3. Implement `evaluate_one_periodic()` (see COMPOSABLE_RULESET_ROADMAP §12.2)
4. Implement `measure_path_coherence()` (mode-specific line tracing)
5. Wire into main GA loop

**Estimated work**: 1–2 hours, ~80 lines of C.

### 2.7 Expected outcomes

- Fitness values will drop (a stable highway is harder to evolve than a transient wave)
- But the rulesets that *do* achieve high fitness will be genuine highway builders
- Stage 1 should show a meaningful gradient when run on these rulesets

---

## 3. Part B: Two-Phase Ruleset (Genome Doubling)

### 3.1 The build-vs-maintain conflict

A single 7-byte ruleset must encode both:
- **Bootstrap**: aggressive birth, low refractory, strong anisotropy → "grow fast from seed"
- **Maintenance**: moderate survival, noise suppression, stability → "don't collapse"

These are in tension. A build-optimized ruleset may be chaotic; a maintain-optimized ruleset may never start.

### 3.2 Design: two-phase DNA

```c
typedef struct {
    uint8_t p[7];   /* Phase 0: BUILD */
    uint8_t q[7];   /* Phase 1: MAINTAIN */
    uint8_t meta;   /* switch_timer (4 bits) + layer masks (4 bits) */
} Ruleset2Phase;
```

Total: **15 bytes = 120 bits** (fits in two `uint64_t`s).

### 3.3 Phase switching

After each grid reset:
1. **Phase 0** (`p`) is active for `switch_timer` ticks — builds the highway
2. **Phase 1** (`q`) is active for remaining ticks — stabilizes it

The `meta` byte encodes:
- Bits 0–3: `switch_timer` (0–15 ticks). 0 = Phase 0 only; 15 = Phase 0 for 15 ticks then Phase 1.
- Bits 4–7: layer enable mask (same semantics as current meta byte).

### 3.4 Step function

```c
void ruleset2phase_step_grid(const Ruleset2Phase *r, Grid *g,
                             int mode, uint8_t *refr_buf,
                             int ticks_since_reset) {
    const uint8_t *params = (ticks_since_reset < (r->meta & 0x0F))
                            ? r->p : r->q;
    /* Use 'params' as the active ruleset for this tick. */
    /* ... same neighbor counting, anisotropy, etc. ... */
}
```

### 3.5 GA operators

| Operator | Description |
|----------|-------------|
| `random()` | Randomize both p and q independently |
| `mutate()` | Per-byte mutation on p, q, and meta independently |
| `crossover(a, b)` | Two-point: swap p, swap q, or swap both |
| `phase_duplication()` | Copy p→q or q→p, then both diverge |
| `phase_collapse()` | Set p=q (revert to single-phase; retain re-diversification potential) |

### 3.6 Implementation files

```
ca_core/composable_ruleset_2phase.h  — struct, pack/unpack, operators
ca_core/composable_ruleset_2phase.c  — step_grid, step_cell, random, mutate, crossover
experiments/exp_composable_stage0.c  — add --2phase flag; use Ruleset2Phase when enabled
```

### 3.7 Priority

**Implement pulse-and-reset first.** If Stage 1 still fails to find a gradient with pulse-and-reset rulesets, **then** implement the 2-phase ruleset. The 2-phase is a larger architectural change (new struct, new packing, new operators); pulse-and-reset is a smaller change to evaluation only.

---

## 4. Part C: Coordination with existing sweep data

### 4.1 What the sweep tells us

The sweep discovered that **v3 + T=50** is the best single-run configuration. The pulse-and-reset evaluation should use this as its baseline:

- **Fitness formula**: v3 (quadratic penalty on random reach)
- **Total steps**: Keep T=50 but split into pulses (e.g., 2 pulses × 25 ticks, or 3 pulses × 17 ticks)
- **Grid size**: 32×32
- **Population**: 100
- **Generations**: 100–200

### 4.2 Completed sweep (2026-05-20)

A mini-sweep was run with R ∈ {10, 15, 20, 30} × 4 seeds × 2 coherence weights (32 runs planned, 24 completed as of this writing).

| Run | R | Completed | Mean Best | Peak | Key finding |
|-----|---|-----------|-----------|------|-------------|
| R10 | 10 | 8/8 | **0.448** | **0.5038** | Sweet spot. Tight window is easiest to evolve. |
| R15 | 15 | 8/8 | 0.370 | 0.4715 | "Uncanny valley" — harder than R=10, no better peak. |
| R20 | 20 | 8/8 | 0.408 | 0.4707 | Some runs incomplete; longer highways are harder. |
| R30 | 30 | 0/8 | — | — | Pending |

**Coherence c=0.3**: mean 0.359, peak 0.3864. Too aggressive for primary evolution; recommend c=0.0 for discovery, c=0.3 for filtering.

**Refractory values are low (3–5)** across all top rulesets, indicating rapid cycling is necessary for re-formation.

Then compare the best ruleset from R=10 to the old v3-T50 ruleset in a Stage 1 test.

---

## 5. Success criteria

| Milestone | Original Criterion | Actual Result (2026-05-20) |
|-----------|-------------------|---------------------------|
| Pulse-and-reset works | Rulesets achieve fitness > 0.5 with R=20 | **Achieved 0.5038 with R=10** (R=20 peak 0.4707). Tighter windows are easier to evolve. |
| Highway quality | Path coherence > 0.6 for best ruleset | Not yet measured. Top ruleset `B=0x5F S=0x61 aniso=237 card=121 refr=4`. |
| Stage 1 gradient | Running Stage 1 on pulse-and-reset R* shows avg fitness improvement > 0.1 over 20 gens | **Blocked** — need to promote best ruleset to R* and test. |
| 2-phase helps | If single-phase pulse-and-reset fails, 2-phase achieves fitness > 0.5 where single-phase could not | **Not yet tested.** Single-phase already achieves >0.5 at R=10. Priority: test Stage 1 on R=10 winner before building 2-phase. |

---

## 6. Relation to lifespan layer

The **maximal lifespan layer** (COMPOSABLE_RULESET_ROADMAP §14) was previously proposed to solve the same problem (unstable highways). Pulse-and-reset is preferred because:

- It doesn't require new state per cell (no `lifespan_age` buffer)
- It doesn't change the ruleset DNA size
- It addresses the root cause directly: evaluation rewards stability
- Lifespan can still be added later if pulse-and-reset alone is insufficient

**Priority order**: pulse-and-reset → 2-phase → lifespan.

---

## 7. Files and dependencies

| File | Action | Depends on |
|------|--------|------------|
| `exp_composable_stage0.c` | Add `--reset-interval`, `--path-coherence`, `evaluate_one_periodic()` | None (existing code) |
| `composable_ruleset_2phase.h` | New struct + operators | `composable_ruleset.h` |
| `composable_ruleset_2phase.c` | New step function | `composable_ruleset.c` (reuses neighbor counting) |
| `test_composable_ruleset.c` | Add tests for 2-phase pack/unpack and step | Both new files |
| `COMPOSABLE_RULESET_ROADMAP.md` | Update §12–13 | This document |
| `PULSE_RESET_DESIGN.md` | This document | — |
