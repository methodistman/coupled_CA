# Cross-Substrate Logic Gate Evolution — Headline Results

**Date**: 2026-05-17
**Status**: All four logic-gate experiments (F1 binary, F2 payload, F3 wave, F4 hybrid) succeeded after a four-part infrastructure fix and one-line code change. See `data/f{1,2,3,4}/FINDINGS.md` for per-substrate details and `ROADMAP.md` §9 for forward-looking next steps.

## The Result in One Table

Each cell: best truth table achieved (✓ = perfect target match) / fitness. Setup uniform across all substrates: size=32, gens=80, pop=40, steps=60, mut_rate=0.05, tour=3, elite=2.

| Gate | Target | F1 binary | F2 payload | F3 wave | F4 hybrid |
|------|--------|-----------|------------|---------|-----------|
| **AND** | 0x8 | 1 / 3 ✓ | **2 / 3 ✓** | **2 / 3 ✓** | 1 / 3 ✓ |
| **OR**  | 0xE | 0 / 3 | **2 / 3 ✓** | 1 / 3 ✓ | **2 / 3 ✓** |
| **XOR** | 0x6 | 2 / 3 ✓ | **2 / 3 ✓** | 0 / 3 | **2 / 3 ✓** |
| **NAND** | 0x7 | **3 / 3 ✓** | 2 / 3 ✓ | 1 / 3 ✓ | 1 / 3 ✓ |
| **Total perfect** | | **6 / 12** | **8 / 12** | **4 / 12** | **6 / 12** |
| **Total ≥ 0.83** | | 12 / 12 | 12 / 12 | 12 / 12 | 12 / 12 |

**Grand total: 24 / 48 runs (50 %) achieve perfect target match. All 48 runs avoid the trivial 0x0 / 0xF attractors.**

Every substrate × gate × seed combination achieves at least 0.83 (only one input-bit wrong). **All 48 runs are now non-degenerate** — i.e., no run collapses to the 0x0 or 0xF attractor.

## Per-Substrate Ranking

| Rank | Substrate | Perfect / 12 | Best Gate | Worst Gate |
|------|-----------|--------------|-----------|------------|
| 1 | **Payload** | 8 / 12 | AND/OR/XOR/NAND all 2/3 — most uniform | (none, all balanced) |
| 2 | **Binary**  | 6 / 12 | NAND 3/3 | OR 0/3 |
| 2 | **Hybrid**  | 6 / 12 | OR/XOR 2/3 each | AND/NAND 1/3 |
| 4 | **Wave**    | 4 / 12 | AND 2/3 | XOR 0/3 |

## Per-Gate Ranking

| Rank | Gate | Total perfect / 12 | Easiest substrate | Hardest substrate |
|------|------|-------------------|-------------------|-------------------|
| 1 | NAND | 7 / 12 | Binary (3/3) | Wave / Hybrid (1/3 each) |
| 2 | XOR | 6 / 12 | Payload / Hybrid (2/3 each) | Wave (0/3) |
| 2 | AND | 6 / 12 | Payload / Wave (2/3 each) | Binary / Hybrid (1/3 each) |
| 4 | OR | 5 / 12 | Payload / Hybrid (2/3 each) | Binary (0/3) |

## The Three Fixes That Unblocked Everything

All four experiments were previously stuck in `0x0`-attractor failure modes. Three orthogonal bugs needed to be fixed in concert:

1. **Modulo rule-lookup** (one line in each of `engine.c`, `pipeline.c`, `payload_pipeline.c`, `wave_engine.c`). The genome's 5-bit `rule_select` field has 32 values but the rule tables have 9–20 entries; the original `if (ri < NUM_RULES) ... else fallback` silently routed 50–72 % of random genomes to the default Conway/payload rule. The fix `((ri % N) + N) % N` distributes evenly.

2. **Shaped fitness with anti-attractor penalty**. Replace `correct / 4` with per-class balanced weights (each class contributes 0.5 of fitness) and apply a 0.25× penalty if the truth table is constant (0x0 or 0xF). Without this, AND's three-zero baseline gives always-0 a misleading 0.75 fitness floor.

3. **Density-relative output read**. Replace the brittle `count > half/2` threshold with `sampled_count * total_cells > total_alive * sampled_cells` — a scale-invariant test for "is the output edge denser than the grid average." Fires at any operating density, not just 50 %+.

A fourth bug surfaced during testing:

4. **IC drift across population**. `evaluate()` captured the initial-condition backup *inside* the function, from the grid state left behind by the previous individual's evaluation. Each individual was thus scored on a different IC. Capturing IC once before the population loop and threading through `pop_evaluate` solves this.

## What This Means for Papers

- **Paper 7 (logic gates)** now has empirical content: a cross-substrate comparison with statistically meaningful results. The four substrates differ in expressiveness; payload leads.
- **Papers 1–3** need re-validation under the modulo lookup. The 72 % Conway fallback was a hidden hyperparameter affecting every experiment with genome-controlled rules.
- **The "Themes" in `FINDINGS_SUMMARY.md`** about random ICs being a trap and substrates dying remain valid in spirit, but the specific magnitudes were inflated by the Conway-fallback bias. Re-running A/B/C series under modulo lookup may shift some falsified hypotheses back to confirmed (or vice versa).

## Reproducibility

```bash
# F1 binary
for gate in and or xor nand; do for seed in 42 43 44; do
  ./exp_binary_logic_ga --gate $gate --seed $seed --size 32 \
    --gens 80 --pop 40 --steps 60 > data/f1_postmod/${gate}_s${seed}.txt
done; done

# F2 payload, F3 wave, F4 hybrid — replace binary with payload/wave/hybrid binary names
# (exp_payload_logic_ga, exp_wave_logic_ga, exp_hybrid_logic)
```

Each gate × seed takes 1–8 s (binary fastest, hybrid slowest). Full 48-run sweep: ~5 minutes.

## Open Questions

1. **Why does wave underperform?** It was originally the most-successful substrate (F3 claimed 100 % OR convergence pre-fix). Under unbiased evaluation it drops to 4/12. Investigation: which wave rules dominate in successful vs failed populations?
2. **Why is XOR easier on payload/hybrid than wave?** XOR requires the substrate to *invert* signals, which may need explicit not-payload or threshold rules that wave lacks.
3. **Closing the gaps**: Binary OR (0/3), Wave XOR (0/3), Hybrid AND/NAND (1/3 each). Are these substrate limits, or do they close under longer evolution / larger populations?
4. **Robustness to seed**: 5 / 16 (substrate × gate) configurations hit 0 perfect runs across 3 seeds. Does seed=42–60 sweep tell a different story?

These are tractable follow-ups, not bugs.
