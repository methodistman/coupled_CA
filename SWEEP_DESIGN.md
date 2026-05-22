# Systematic Sweep Design for Composable Ruleset Stage-0

**Date**: 2026-05-18  
**Scope**: Discover which combinations of seeds, evaluation conditions, and fitness formulations produce non-trivial, orientation-sensitive propagation rulesets in a 56-bit (or 64-bit with lifespan) composable CA ruleset space.

---

## 1. Why a sweep?

A 56-bit ruleset space contains ~7.2×10¹⁶ possible configurations. A single GA run with one seed, one step count, and one fitness formula is a **single sample** from a high-dimensional stochastic process. We cannot draw conclusions about the fitness landscape from one data point.

**Historical evidence**: The first run with fitness v1 (seed=42, steps=50, K=8) discovered a trivial fill-everything ruleset (`B=0x7C S=0xFF`, fitness 0.83). The second run with fitness v2 on the same seed discovered a genuinely different ruleset (`B=0x88 S=0xAE aniso=255 card=0 refr=6`, fitness 0.4657). This proves the fitness formula matters, but we don't yet know:
- Is the v2 result reproducible across seeds?
- Is it an outlier or a robust attractor?
- Would a different step count or grid size produce better rulesets?

**Goal of the sweep**: Build a catalogue of "what works" — seed/condition combinations that reliably produce non-trivial, orientation-sensitive rulesets suitable as `R*` for Stage 1.

---

## 2. Experimental design

### 2.1 Response variable

**Quality score** of a discovered ruleset, computed from the final log:

```
quality = orientation_sensitivity × non_triviality × reach

where:
  orientation_sensitivity = (reach_aligned - reach_random) / reach_aligned
  non_triviality = 1.0 if S != 0xFF or birth_mask has < 4 bits set, else 0.2
  reach = reach_aligned (raw aligned reach, 0..1)
```

A perfect ruleset has `quality ≈ 1.0` (high aligned reach, low random reach, non-trivial survival). A fill-everything ruleset has `quality ≈ 0.2` (high reach but `non_triviality = 0.2`).

### 2.2 Factors and levels

| Factor | Symbol | Levels | Type | Rationale |
|--------|--------|--------|------|-----------|
| **Seed** | `s` | 42, 123, 2025, 9999 | Categorical | GA stochasticity; best ruleset from one seed may be an outlier |
| **Eval steps** | `T` | 30, 50, 100 | Continuous-ish | Short steps reward fast waves; long steps test stable highways |
| **Fitness formula** | `F` | v2, v3 | Categorical | v2 penalizes random reach; v3 penalizes it quadratically |
| **Grid size** | `N` | 32 (fixed for sweep) | Held constant | First sweep fixes N=32; second sweep varies N if warranted |
| **K fields** | `K` | 8 (fixed for sweep) | Held constant | 8 fields gives reasonable noise reduction without excessive cost |
| **Population** | `P` | 100 (fixed) | Held constant | Budget constraint; 100 × 100 gens = 10K evaluations per run |
| **Mutation rate** | `mu` | 0.15 (fixed) | Held constant | Per-byte rate; 7 bytes × 0.15 ≈ 1 byte mutated per offspring |

### 2.3 Design: full factorial

**4 seeds × 3 step counts × 2 formulas = 24 runs**

This is small enough to run in a few hours, large enough to detect main effects of seed, steps, and formula.

### 2.4 Hypotheses

1. **H1**: v3 produces higher-quality rulesets than v2. (Stronger penalty on random reach forces sharper orientation discrimination.)
2. **H2**: T=100 produces higher-quality rulesets than T=30 or T=50. (Longer evaluation gives slow-but-stable waves time to reach the sink; short eval may reward fast chaotic waves.)
3. **H3**: Some seeds consistently produce higher-quality rulesets than others. (If true, those seeds have favorable initialization basins.)
4. **H4**: The best rulesets share conserved parameters across seeds and conditions. (If true, those parameters are robust features of the fitness landscape.)

---

## 3. Execution

### 3.1 Shell script

```bash
#!/bin/bash
# sweep_stage0.sh — run the 24-condition sweep
set -e
mkdir -p data/sweep

for s in 42 123 2025 9999; do
  for T in 30 50 100; do
    for F in v2 v3; do
      tag="s_${s}_T_${T}_F_${F}"
      echo "=== Starting $tag ==="
      ./exp_composable_stage0 \
        --size 32 --pop 100 --gens 100 \
        --kfields 8 --steps $T --seed $s \
        --save "data/sweep/${tag}.bin" \
        > "data/sweep/${tag}.csv" \
        2> "data/sweep/${tag}.log"
      echo "=== Finished $tag ==="
    done
  done
done
```

### 3.2 Parallel execution

Since each run is independent, they can run in parallel:

```bash
# Run all 24 in background, limit to 4 concurrent
mkdir -p data/sweep
for s in 42 123 2025 9999; do
  for T in 30 50 100; do
    for F in v2 v3; do
      tag="s_${s}_T_${T}_F_${F}"
      (
        ./exp_composable_stage0 --size 32 --pop 100 --gens 100 \
          --kfields 8 --steps $T --seed $s \
          --save "data/sweep/${tag}.bin" \
          > "data/sweep/${tag}.csv" \
          2> "data/sweep/${tag}.log"
      ) &
      # Limit concurrent jobs
      if (( $(jobs -r | wc -l) >= 4 )); then
        wait -n
      fi
    done
  done
done
wait
```

### 3.3 Time estimate

- Single run: ~5–10 minutes (100 pop × 100 gens × 32 modes × (2 + K) fields × 32×32 cells × 50–100 steps)
- 24 runs sequential: ~2–4 hours
- 24 runs parallel (4 cores): ~30–60 minutes

---

## 4. Data extraction

### 4.1 Per-run metrics

From each `.csv` and `.log` file, extract:

| Metric | Source | Description |
|--------|--------|-------------|
| `best_fitness` | CSV last line | Final best fitness |
| `avg_fitness` | CSV last line | Final population average |
| `B`, `S`, `aniso`, `card`, `refr`, `noise`, `meta` | CSV last line | Best ruleset params |
| `reach_aligned` | Log (needs adding to code) | Mean aligned reach |
| `reach_random` | Log (needs adding to code) | Mean random-field reach |
| `reach_adversarial` | Log (needs adding to code) | Mean adversarial reach |
| `convergence_gen` | CSV | First gen where `best_fitness` changes < 0.001 for 10 gens |

### 4.2 Quality computation

```python
# quality.py — compute quality score from log
def compute_quality(reach_aligned, reach_random, survive_mask):
    orient_sens = (reach_aligned - reach_random) / max(reach_aligned, 1e-6)
    non_trivial = 1.0 if survive_mask != 0xFF and bin(survive_mask).count('1') < 6 else 0.2
    return orient_sens * non_trivial * reach_aligned
```

### 4.3 Aggregate table

| seed | steps | formula | best_fit | B | S | aniso | card | refr | quality | fill? |
|------|-------|---------|----------|---|---|-------|------|------|---------|-------|
| 42 | 30 | v2 | 0.47 | 0x88 | 0xAE | 255 | 0 | 6 | 0.42 | No |
| 42 | 30 | v3 | ... | ... | ... | ... | ... | ... | ... | ... |
| ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... |

---

## 5. Decision criteria

After the sweep, decide:

### 5.1 Which fitness formula?

Compare mean quality across all (seed, steps) combinations:
- If `mean_quality(v3) > mean_quality(v2) + 0.05`, adopt v3.
- If `mean_quality(v2) ≈ mean_quality(v3)`, keep v2 (simpler).
- If both are poor (< 0.3), consider v4 or adding the lifespan layer.

### 5.2 Which step count?

Compare mean quality across all (seed, formula) combinations:
- If quality increases monotonically with `T`, use `T=100` (or higher).
- If `T=50` is the peak, use `T=50`.
- If `T=30` and `T=100` are both poor but `T=50` is good, there's a sweet spot.

### 5.3 Which ruleset to promote to R*?

Select the **highest-quality** ruleset from the sweep. If multiple have similar quality, prefer:
1. Higher `reach_aligned` (better propagation)
2. Lower `noise` (more deterministic, easier for Stage 1)
3. Lower `refr` (simpler dynamics)

If no ruleset has `quality > 0.5`, the lifespan layer (§12 of roadmap) is the next experiment.

---

## 6. Required code changes before sweep

1. **Add per-reach logging to `exp_composable_stage0.c`**:
   - Print `reach_aligned`, `reach_random`, `reach_adversarial` to stderr at the end of `evaluate_ruleset` for the best individual each generation, OR
   - Print them once at the end for the best individual.

2. **Add `--fitness-v3` flag** (or compile-time switch):
   - Implement v3 formula: `reach_aligned * (1 - reach_rand) * (1 - reach_rand) * (1 - reach_adv)`
   - Or pass formula variant as a command-line argument.

3. **Add convergence detection**:
   - Track best fitness over last 10 generations; if delta < 0.001, log "converged" and optionally early-exit.

---

## 7. Post-sweep actions

| Outcome | Action |
|---------|--------|
| Best quality ≥ 0.5, formula clear winner | Fix that formula, run Stage 1 with the best R* |
| Best quality 0.3–0.5, formula unclear | Run larger sweep (add seeds, add v4) |
| Best quality < 0.3 regardless of conditions | Implement lifespan layer (Layer 7) and re-sweep |
| Conserved parameters emerge | Document them as "robust propagation motifs" |
| No conserved parameters | The landscape is too noisy; consider larger pop or more gens |

---

## 8. Relation to broader project

This sweep is a **methodological requirement** before declaring Stage 0 "done." The hierarchical GA hypothesis depends on Stage 0 producing a substrate that Stage 1 can actually improve. If we promote a ruleset to R* based on one run, and Stage 1 fails, we won't know whether the problem is (a) the ruleset, (b) Stage 1's fitness, or (c) the whole approach. A sweep gives us confidence that the ruleset is representative of what the fitness landscape can produce.

**Files produced by this sweep** become part of the permanent data record:
- `data/sweep/*.csv` — raw GA traces
- `data/sweep/*.log` — per-reachability breakdowns
- `data/sweep/*.bin` — saved rulesets for manual inspection
- `data/sweep/summary.md` — aggregate analysis
- `data/sweep/quality_plot.png` — visualization (if tooling available)
