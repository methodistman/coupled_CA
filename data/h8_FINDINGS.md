# H8: Directional Rule Modification — 3333 Generation Run Findings

**Experiment**: `exp_directional_multimodal.c`
**Parameters**: size=32, pop=60, gens=3333, steps=50, seed=42
**Genome**: 48-bit (rule_select, weight, mutation_rate, 4 mode_orientations, alt_rule, dir_mask)
**Kernel**: mode-specific orientation + directional rule switching (alt_rule when dir_mask neighbors active)
**Date**: 2026-05-18

---

## 1. Aggregate Results

| Metric | Value |
|--------|-------|
| Generations evaluated | 3333 |
| Peak fitness | **0.8750** at generation 87 |
| Peak truths | 0x4 (HORIZ), 0x7 (VERT), 0x6 (ADIAG), 0x6 (DIAG) |
| Final fitness | 0.7500 |
| Final truths | 0x7 (HORIZ), 0x7 (VERT), 0x6 (ADIAG), 0x3 (DIAG) |
| Mean fitness | 0.726 |
| Std dev | 0.062 |
| Gens with fitness ≥ 0.75 | 1876 / 3333 = **56%** |
| Gens with fitness ≥ 0.50 | 3327 / 3333 = **99%** |
| Convergence (≥ 0.9999) | **0** (never) |

---

## 2. Per-Mode Truth Table Evolution

Target for all modes: **XOR = 0x6**

### Mode 0 (Horizontal: left → right)

| Truth | Count | % |
|-------|-------|---|
| 0x7 | 2567 | **77.0%** |
| 0x3 | 205 | 6.2% |
| 0x2 | 157 | 4.7% |
| 0x4 | 175 | 5.2% |
| 0x5 | 94 | 2.8% |
| 0x0 | 60 | 1.8% |
| **0x6 (XOR)** | **59** | **1.8%** |

**Observation**: Horizontal mode is dominated by 0x7 (AND-like: output=1 when both inputs are 1). The anisotropic coupling strongly favors East/West alignment, making it easy to propagate a "1" signal but hard to implement the XOR cancellation (output 0 when both inputs are 1).

### Mode 1 (Vertical: top → bottom)

| Truth | Count | % |
|-------|-------|---|
| 0x7 | 2018 | **60.5%** |
| **0x6 (XOR)** | **318** | **9.5%** |
| 0x4 | 175 | 5.2% |
| 0x2 | 157 | 4.7% |
| 0x5 | 214 | 6.4% |
| 0x0 | 60 | 1.8% |

**Observation**: Vertical mode shows ~5x more XOR discoveries than horizontal (9.5% vs 1.8%). Still AND-dominated but with meaningful exploration of XOR.

### Mode 2 (Anti-diagonal: \)

| Truth | Count | % |
|-------|-------|---|
| **0x2** | 754 | **22.6%** |
| **0x6 (XOR)** | **754** | **22.6%** |
| 0x4 | 319 | 9.6% |
| 0xE | 315 | 9.4% |
| 0x7 | 597 | 17.9% |
| 0x3 | 141 | 4.2% |
| 0xA | 150 | 4.5% |

**Observation**: Anti-diagonal is the most balanced mode. XOR appears in 22.6% of generations, tied with 0x2 (A AND NOT B). This is a dramatic improvement over the polar anisotropic experiment (H3) where anti-diagonal never exceeded 0.125 fitness.

### Mode 3 (Diagonal: /)

| Truth | Count | % |
|-------|-------|---|
| 0x7 | 790 | **23.7%** |
| **0x2** | 716 | **21.5%** |
| **0x6 (XOR)** | **716** | **21.5%** |
| 0x4 | 318 | 9.5% |
| 0xE | 212 | 6.4% |
| 0x3 | 160 | 4.8% |

**Observation**: Diagonal also achieves 21.5% XOR rate. Combined with mode 2, the two diagonal traces together represent the strongest directional rule success.

---

## 3. Cross-Mode XOR Co-occurrence

| Condition | Count | % |
|-----------|-------|---|
| Any single mode = XOR | 1619 | 48.6% |
| All 4 modes = XOR simultaneously | **0** | **0%** |
| All 4 modes = XOR AND fitness ≥ 0.75 | **0** | **0%** |

**Critical finding**: While individual modes discover XOR frequently (especially diagonal modes), **no generation achieved XOR in all 4 modes simultaneously**. The fitness function averages across modes, so a generation with truths [0x7, 0x7, 0x6, 0x6] scores 0.75 — good enough to survive selection pressure but not good enough to drive toward the global optimum of [0x6, 0x6, 0x6, 0x6].

---

## 4. Fitness Trajectory

```
Early (gens 0-100):   Rapid rise from ~0.3 → 0.875 (peak)
Mid (gens 100-500):   Oscillation 0.65-0.80, no sustained improvement
Late (gens 500-3333): Plateau at ~0.72 ± 0.06, no convergence
```

The population **explores** the truth-table space actively but **does not converge** on the global optimum. The best individual at gen 87 (0x4,0x7,0x6,0x6) was never recovered. This suggests:
- The search space is too large (48 bits × 1024 cells = 49,152 bits)
- Mutation+crossover destroy good combinations faster than they discover them
- The fitness landscape has many local optima at ~0.75

---

## 5. Comparison to Baseline Experiments

| Experiment | Peak Fitness | XOR in Any Mode | All 4 Modes XOR | Notes |
|------------|------------|-----------------|-----------------|-------|
| H1: 2-mode binary | 0.87 | N/A (2-mode) | N/A | Plateaued |
| H3: Polar anisotropic | 0.34 | 1 mode (DIAG only) | No | 3 modes stuck at 0.125 |
| **H8: Directional rules** | **0.875** | **48.6% of gens** | **No** | **2.5x better peak than H3** |

**Directional rule modification delivers a 2.5× improvement in peak fitness over the polar anisotropic baseline (H3) and unlocks XOR in 2/4 modes (22% of generations).**

---

## 6. Root Cause Analysis: Why All-4-XOR Fails

### Hypothesis A: Search Space Too Large
48-bit genome × 1024 cells = 49,152 bits. A population of 60 covers only 0.12% of one-bit-flip neighbors. **Verdict: Likely contributor.**

### Hypothesis B: Fitness Landscape Deceptive
The fitness function `avg(shaped_accuracy)` rewards partial solutions. An individual with [0x7, 0x7, 0x6, 0x6] scores 0.75 — high enough to survive but not high enough to create strong selection pressure for the missing XOR modes. **Verdict: Confirmed.** The population gets trapped in local optima where 2 modes are correct and 2 are wrong.

### Hypothesis C: Horizontal/Vertical Modes Intrinsically Harder
The histograms show HORIZ (mode0) achieves XOR only 1.8% of the time vs DIAG (mode3) at 21.5%. The anisotropic kernel's 8-direction quantization may not provide clean enough highway separation for cardinal directions. East/West and North/South have 4 aligned neighbors each (including diagonals at 0.5 weight), while diagonal directions have only 2 fully aligned neighbors. **Verdict: Confirmed by data.**

### Hypothesis D: Single-Grid Interference
All 4 modes share the same grid and cells. When mode3 (DIAG) is being evaluated, the genome's mode3 orientation and dir_mask affect the grid state. But the same cells' mode0 orientation may create conflicting coupling. **Verdict: Unconfirmed but plausible.**

---

## 7. Recommendations

### Immediate: Hierarchical Evolution (H7)
The data strongly supports the hierarchical approach:
1. **Stage 1**: Evolve only orientations. Fitness = does a signal reach the output edge in each mode? Ignore logic correctness. Goal: ensure all 4 traces have viable highways.
2. **Stage 2**: Freeze the winning orientation pattern. Evolve only rules/weights/alt_rules for logic.

This decouples the hard problem (orientation + logic) into two easier problems.

### Medium: Larger Population / Parallel Populations
- Increase population to 200-500
- Run 4 parallel populations, each seeding the others with migrants
- Or use CMA-ES or other continuous-space optimizer instead of GA

### Alternative: Change Target Gate
XOR may be too hard for this substrate. Test AND (0x8) or OR (0xE) as target:
- AND and OR are monotonic — easier to propagate through anisotropic coupling
- If AND/OR succeed, it validates the directional rule approach
- Then attempt XOR as a harder follow-up

---

## 8. Conclusion

**Directional rule modification is a valid and powerful mechanism.** It unlocks per-mode computation in a single grid, achieving 2.5× better fitness than the anisotropic-only approach and discovering XOR in individual modes 22% of the time.

**However, the full 4-mode XOR target remains unsolved.** The search space is too large and the fitness landscape too deceptive for a simple GA with population 60. Hierarchical evolution (Stage 1: highways, Stage 2: logic) is the recommended next step.
