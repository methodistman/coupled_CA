# Experiment F1: Binary Logic Gate GA — Findings

**Date**: 2026-05-16
**Hypothesis**: "Binary-grid genomes can evolve to compute AND/OR/XOR/NAND truth tables"

## Setup

- `exp_binary_logic_ga` with 2-grid setup (grid 0 = computation with genomes, grid 1 = input buffer)
- Population: 30, generations: 50, evaluation steps: 30
- Grid size: 16×16, tournament size: 3, elitism: 2
- Gates tested: AND, OR, XOR, NAND across seeds 42–44

## Results

| Gate | Expected | Observed | Fitness | Seeds |
|------|----------|----------|---------|-------|
| AND | 0001 | 0000 | 0.750 | 3/3 |
| OR | 0111 | 0000 | 0.250 | 3/3 |
| XOR | 0110 | 0000 | 0.500 | 3/3 |
| NAND | 1110 | 0000 | 0.250 | 3/3 |

All seeds converge to the **always-0** truth table (0x0).

## Findings

### F1. The always-0 attractor dominates
Every gate and every seed converges to output 0 for all four input combinations. This gives partial fitness:
- AND: 0.75 (3/4 inputs expect 0)
- XOR: 0.50 (2/4 inputs expect 0)
- OR/NAND: 0.25 (1/4 inputs expect 0)

### F2. The fitness landscape has a deceptive gradient
The GA is not "stuck" at a local optimum — it is correctly optimizing toward the trivial solution that maximizes the number of matching outputs. The problem is that the **output-read mechanism** (sampling the right edge of grid 0) does not create a gradient toward the correct 1-output for the active input combination.

### F3. Input-coupling timing may be the issue
The input grid (grid 1) couples its right edge into grid 0's left edge, but the signal may not propagate to grid 0's right edge within the 30-step evaluation window. OR and NAND require the output to be 1 when either input is 1 — if the signal dies before reaching the output edge, the fitness function rewards always-0.

## Hypothesis Status

- **Phase D (binary)**: **Falsified for AND/XOR/OR/NAND** under current parameters. The binary grid cannot evolve logic gates with the current 2-grid population-GA setup.

## Anti-Attractor Fitness Shaping (2026-05-17)

Added `--fitness raw|shaped` flag to `exp_binary_logic_ga`. Default is now `shaped`.

**Shaping formula**:
- Per-class balanced weighting: 1-expected entries weighted `0.5/n_ones`, 0-expected `0.5/n_zeros`. Each class contributes exactly 0.5 → removes the structural bias of `correct/4`.
- Constant-output penalty: if observed truth ∈ {0x0, 0xF}, multiply by 0.25.

**Result (seeds 42–44, default params, 16×16, 30 steps)**: all gates still collapse to 0x0, but fitness comparison:

| Gate | Raw best-fitness | Shaped best-fitness | Penalty effect |
|------|-----------------|---------------------|----------------|
| AND  | 0.7500 | **0.1250** | 6.0× reduction |
| OR   | 0.2500 | **0.1250** | 2.0× reduction |
| XOR  | 0.5000 | **0.1250** | 4.0× reduction |
| NAND | 0.2500 | **0.1250** | 2.0× reduction |

### F4. Shaping is mechanically correct but reveals a deeper blocker
The shaping correctly demotes always-0 from misleading (0.75 for AND) to uniformly penalized (0.125 for all gates). **However, the GA still cannot escape the basin**, because **every random initial individual produces 0x0** — the entire population is on the floor, with zero output variance to drive selection. Even at size=32, steps=100, pop=40 the same plateau persists.

### F5. The output-read threshold is brittle (now fixed)
Original `read_output()` required `count > half/2` alive cells in the top half of the right column (e.g., for size=16, count > 4 of 8 sampled). At the typical decayed-ash density of ~1%, the expected count is ~0.08 — **mathematically impossible to fire**.

**Fix added**: `--out-mode strict|density` flag. New default `density` mode returns 1 iff `count_sampled > expected_from_grid_mean` (scale-invariant comparison: any localized concentration of alive cells at the output edge fires output=1). Implementation in `experiments/exp_binary_logic_ga.c::read_output()`.

### F6. The substrate is bistable, not computational
With both the fitness fix and the output-read fix in place, the population still collapses. A direct diagnostic of 40 random genomes (size=32, 60 steps) reveals:

| Truth table | Count | Frequency |
|-------------|-------|-----------|
| 0x0 (always-0) | 34 / 40 | 85% |
| 0xF (always-1) | 6 / 40 | 15% |
| **Any input-sensitive truth (14 others)** | **0** | **0%** |

**Random binary-Life genomes are bistable**: they collapse to either dead (0x0) or saturated (0xF) regardless of input (a,b). The intent-REPLACE coupling from grid 1's right edge → grid 0's left edge does not perturb the trajectory enough to produce different outputs for different inputs. The constant-output penalty correctly demotes both attractors to 0.125, but with zero input-sensitive individuals in the population, the GA has nothing to select for.

**This falsifies the architectural assumption** that binary-Life + intent-buffer coupling is computationally expressive. Fitness shaping is necessary but not sufficient; the *substrate* needs more expressive coupling.

### F7. The bistability was an artefact of two upstream bugs (resolved)

After writing F6, two upstream bugs were uncovered and fixed; F1 binary logic gate evolution then succeeded.

**Bug 1: Conway-fallback bias (modulo lookup fix)**
The genome `rule_select` is 5 bits (32 values), but `RULE_TABLE` has only 9 entries. The lookup `if (ri >= 0 && ri < NUM_RULES) rule = &RULE_TABLE[ri]; else rule = default_rule;` fell back to Conway's Life for 23 / 32 = **72 % of random genome values**. The "bistability" of F6 was largely an artefact of a population that was 72 % Conway, biased toward decay.

Fix: replaced the lookup with `rule = &RULE_TABLE[((ri % NUM_RULES) + NUM_RULES) % NUM_RULES];` in `engine.c`, `pipeline.c`, `payload_pipeline.c`, `wave_engine.c`. Conway's share dropped from 72 % to 11 %.

Post-fix population diversity (seeds 42–44, size=32, 60 steps):

| Seed | Distinct truth tables | Input-sensitive | Notable |
|------|----------------------|-----------------|---------|
| 42   | 6 / 16 | 4 | NOR (0x1) appears |
| 43   | 9 / 16 | 7 | NAND (0x7) appears twice |
| 44   | 4 / 16 | 2 | NOT-b (0x5) appears |

**Bug 2: IC drift across individuals**
`evaluate()` captured `bk0`/`bk1` (initial-condition backup) *inside* the function from `sys->grids[0/1]->cells`, but the grid had already been modified by the previous individual's evaluation. Each individual was being scored on a different (drifting) IC. After fix (capture IC once before the population loop and pass through `pop_evaluate`), the GA succeeds.

**Final F1 results (size=32, gens=80, pop=40, steps=60, seeds 42–44):**

| Gate | Target | s42 | s43 | s44 | Perfect / 3 |
|------|--------|-----|-----|-----|-------------|
| **AND** | 0x8 | 0xC f=0.83 | **0x8 f=1.00** ✓ | 0xC f=0.83 | 1 / 3 |
| **OR** | 0xE | 0x6 f=0.83 | 0x4 f=0.67 | 0x4 f=0.67 | 0 / 3 |
| **XOR** | 0x6 | **0x6 f=1.00** ✓ | **0x6 f=1.00** ✓ | 0x2 f=0.75 | 2 / 3 |
| **NAND** | 0x7 | **0x7 f=1.00** ✓ | **0x7 f=1.00** ✓ | **0x7 f=1.00** ✓ | **3 / 3** |

**6 / 12 runs achieve perfect fitness.** NAND solved across all seeds; XOR 2/3; AND 1/3 with the other two at 0.83 (one input-bit wrong). OR is the hardest gate on binary (3 ones, 1 zero) and the GA finds 0.67–0.83 partial matches but no perfect 0xE; longer evolution or larger populations may close this gap.

## Hypothesis Status (updated)

- **Phase D (binary)**: **CONFIRMED for AND, XOR, NAND under shaped fitness + density output + modulo lookup + fixed IC**. OR remains partial. The binary grid CAN evolve logic gates from random IC; previous failures were due to (a) Conway fallback bias, (b) IC drift across evaluations, (c) brittle output threshold, (d) misleading raw fitness. All four issues are now fixed.

## Next Steps (post-breakthrough)

1. **Close the OR gap**: longer evolution (gens=200), larger pop (80–100), or anneal mutation rate. OR's 3-of-4-ones target is genuinely harder than NAND's 3-of-4 (NAND's penalty structure is symmetric to OR's but OR has no random-population seed match).
2. **Replicate B1, C2, A-series with the modulo lookup**: the 72 % Conway fallback affected every experiment in the codebase. Headline findings need re-validation.
3. **Audit other experiments for IC-drift bugs**: any experiment that calls `evaluate()`-style functions in a loop without resetting state may have the same issue.
4. **Apply shaping+density to F2/F3/F4** (payload/wave/hybrid logic gates) — should now succeed similarly.
5. **Sweep across substrates**: now that F1 works, compare AND/OR/XOR/NAND fitness curves on binary vs payload vs wave vs hybrid as the headline result of Paper 7.

## Files

- `data/f1_shaped/{and,or,xor,nand}_s{42,43,44}.txt` — pre-breakthrough shaped-mode runs.
- `data/f1_shaped/raw_baseline/{and,or,xor,nand}.txt` — raw-mode baseline.
- `data/f1_density/{and,or,xor,nand}_s{42,43,44}.txt` — shaped+density runs (pre-modulo, still 0x0).
- `data/f1_postmod/{and,or,xor,nand}_s{42,43,44}.txt` — **breakthrough runs**: shaped + density + modulo + IC fix. **6 / 12** hit perfect fitness.
