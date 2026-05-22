# Experiment F2: Payload Logic Gate GA — Findings

**Date**: 2026-05-16
**Hypothesis**: "Payload-grid genomes can evolve to compute logic gates"

## Setup

- `exp_payload_logic_ga` with 2-grid PayloadSystem (grid 0 = computation, grid 1 = input buffer)
- Population: 30, generations: 50, evaluation steps: 30
- Grid size: 16×16, seeds: 42–44
- Payload rules: 20 available (including `and_payload`, `or_payload`, `xor_payload`)

## Results

| Gate | Best Fitness | Perfect? | Notes |
|------|-------------|----------|-------|
| AND | 0.750 | No | Always-0 attractor |
| OR | **1.000** | **Yes** | Converged to 0xE (0111) across multiple seeds |
| XOR | 0.500 | No | Always-0 attractor |
| NAND | 0.250 | No | Always-0 attractor |

## Findings

### F1. OR is evolvable; AND/XOR/NAND are not
The payload grid successfully evolved OR (truth table 0111) with perfect fitness. This demonstrates that:
1. The 2-grid population-GA architecture is sound.
2. The payload rule set contains sufficient logic primitives.
3. The fitness evaluation pipeline works correctly for at least one gate.

### F2. AND and XOR hit the same always-0 attractor as binary
For gates where the majority of outputs are 0 (AND: 3/4, XOR: 2/4), the GA converges to always-0. This suggests the problem is **not substrate-specific** (binary vs payload) but rather a fundamental issue with the fitness evaluation or signal propagation.

### F3. OR succeeds because it is "easiest"
OR outputs 1 for 3/4 input combinations, making always-0 a poor strategy (only 25% fitness). The GA is forced to find a mechanism to produce 1-outputs, and the payload rules provide a path to do so.

## Hypothesis Status

- **Phase D (payload)**: **Partially confirmed**. OR gate evolves perfectly; AND/XOR/NAND fail due to always-0 attractor.

## Next Steps

1. **Run larger sweep** (seeds 42–60) to confirm OR robustness.
2. **Test XOR with modified fitness** — penalize always-0 solutions to break the attractor.
3. **Compare rule usage** — does OR success use `or_payload` rule or emergent signal propagation?

---

## Update 2026-05-17: Post-fix breakthrough

After F1's three fixes (modulo lookup, shaped fitness with anti-attractor penalty, density-relative output read, IC-fix) were ported to `exp_payload_logic_ga.c`, the payload substrate now achieves **8/12 perfect runs** across AND/OR/XOR/NAND × seeds 42/43/44 (size=32, gens=80, pop=40, steps=60):

| Gate | Target | s42 | s43 | s44 | Perfect / 3 |
|------|--------|-----|-----|-----|-------------|
| **AND** | 0x8 | **0x8 f=1.00** ✓ | 0xE f=0.67 | **0x8 f=1.00** ✓ | 2 / 3 |
| **OR**  | 0xE | **0xE f=1.00** ✓ | **0xE f=1.00** ✓ | 0x6 f=0.83 | 2 / 3 |
| **XOR** | 0x6 | **0x6 f=1.00** ✓ | 0x7 f=0.75 | **0x6 f=1.00** ✓ | 2 / 3 |
| **NAND** | 0x7 | 0x5 f=0.83 | **0x7 f=1.00** ✓ | **0x7 f=1.00** ✓ | 2 / 3 |

**Payload is the strongest substrate** for logic gate evolution (8/12 vs F1 binary 6/12 vs F3 wave 4/12). Results stored in `data/f2_postmod/`. Phase D (payload) hypothesis fully confirmed across all four gates.
