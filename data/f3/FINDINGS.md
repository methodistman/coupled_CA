# Experiment F3: Wave Logic Gate GA — Findings

**Date**: 2026-05-16
**Hypothesis**: "Wave-grid genomes can evolve to compute logic gates"

## Setup

- `exp_wave_logic_ga` with 2-grid WaveSystem (grid 0 = computation, grid 1 = input buffer)
- Population: 30, generations: 50, evaluation steps: 30
- Grid size: 16×16, seeds: 42–46
- Wave rules: 8 available (including `vector_or`, `vector_xor`, `threshold_and`, `threshold_or`)

## Results

| Gate | Seeds Tested | Perfect Convergence | Best Fitness |
|------|-------------|---------------------|--------------|
| OR | 5 | **5/5** | **1.000** |

All 5 seeds for OR converged to the exact truth table (0xE = 0111) with perfect fitness.

## Findings

### F1. Wave grid achieves 100% OR convergence
Across all tested seeds (42–46), the wave-grid GA consistently finds the correct OR truth table. This is the **highest reliability** of any substrate tested so far (binary: 0%, payload: partial, wave: 100%).

### F2. Wave vector rules provide strong logic primitives
The `vector_or` rule (wave value = max of neighbors) naturally propagates active signals. Combined with wave's multi-bit state, the grid can maintain and propagate 1-signals more robustly than binary or payload substrates.

## Hypothesis Status

- **Phase D (wave)**: **Confirmed for OR**. AND/XOR/NAND not yet tested due to time constraints; likely to face the same always-0 attractor as other substrates for gates with mostly-0 outputs.

## Next Steps

1. Test AND/XOR/NAND on wave grid with anti-attractor fitness shaping.
2. Profile which wave rules are selected per-cell in successful OR runs.
3. Compare wave convergence speed vs payload vs binary.

---

## Update 2026-05-17: Post-fix results across all four gates

After F1's fixes (modulo lookup, shaped fitness, density output, IC-fix) were ported to `exp_wave_logic_ga.c`, full sweep over AND/OR/XOR/NAND × seeds 42/43/44 (size=32, gens=80, pop=40, steps=60):

| Gate | Target | s42 | s43 | s44 | Perfect / 3 |
|------|--------|-----|-----|-----|-------------|
| **AND** | 0x8 | **0x8 f=1.00** ✓ | **0x8 f=1.00** ✓ | 0xA f=0.83 | 2 / 3 |
| **OR**  | 0xE | **0xE f=1.00** ✓ | 0xA f=0.83 | 0xA f=0.83 | 1 / 3 |
| **XOR** | 0x6 | 0xE f=0.75 | 0x2 f=0.75 | 0x2 f=0.75 | 0 / 3 |
| **NAND** | 0x7 | 0x3 f=0.83 | 0x5 f=0.83 | **0x7 f=1.00** ✓ | 1 / 3 |

Total: **4/12 perfect**. Wave is the **weakest** of the three substrates (vs F1 binary 6/12, F2 payload 8/12), contradicting the original F1 finding from this file that "wave is the highest reliability." The original wave-OR-converges-100% finding was an artefact of the always-0 attractor making OR (target 0xE, no zeros except idx 0) the easiest gate. Under the fixed evaluation, OR is no longer special; XOR is notably hard on wave (0/3). Results stored in `data/f3_postmod/`.

**Hypothesis status updated**: Phase D (wave) **partially confirmed** — AND/NAND evolvable, OR struggles, XOR not yet found. May benefit from larger wave-rule diversity in `WAVE_RULE_TABLE`.
