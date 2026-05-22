# Paper 3: Reservoir Computing with Coupled Cellular Automata

## Metadata
- **Venue**: ICML / NeurIPS (workshop track) / IEEE TNNLS
- **Preprint**: arXiv, November 2026
- **Hypotheses**: H10
- **Experiments**: C2

## Abstract
We evaluate coupled multi-grid cellular automata as substrates for reservoir computing (RC). Unlike traditional RC reservoirs (echo state networks, random Boolean networks), our CA reservoir has spatial structure from local GA evolution and multi-grid coupling. We drive one grid's edge with binary input streams and train ridge regression readouts on another grid's edge states. On delayed-XOR and NARMA-10 benchmarks, density-evolved reservoirs achieve memory capacity > 5 ticks and NRMSE < 0.1, outperforming random-IC baselines. Static-genome reservoirs perform within 10% of continuously-evolved reservoirs, suggesting the value is in the discovered spatial configuration. These results position coupled CA as a novel, interpretable, low-power RC substrate.

## Structure

### 1. Introduction
- Reservoir computing: high-dimensional dynamical systems with trained linear readouts
- Traditional substrates: ESN (Jaeger 2001), RBN (Rodan & Tino 2011)
- Gap: no spatially-structured, evolved CA reservoir evaluated on standard benchmarks
- Contribution: coupled CA with per-cell evolved genomes as RC substrate

### 2. Related Work
- Jaeger (2001): echo state networks
- Rodan & Tino (2011): minimum complexity echo state networks
- Margem & Stern (2017): CA reservoir with global GA
- Nichele & Molund (2017): deep reservoir computing with CA
- Difference: our reservoir has local (not global) evolution and multi-grid coupling

### 3. Methods
- 3.1 Reservoir definition: grid 0 driven, grid N read out
- 3.2 Input encoding: binary pulse injected at driven edge
- 3.3 Readout: ridge regression on N-edge state vector
- 3.4 Tasks: delayed-XOR (memory), NARMA-10 (nonlinear), 5-bit memory
- 3.5 Metrics: memory capacity (Jaeger), NRMSE, information processing capacity (Dambre)

### 4. Experiments
- C2a: Random-IC baseline vs. density-evolved IC
- C2b: Static learned genomes vs. continuous GA
- C2c: Multi-grid scaling: 2, 4, 8 grids
- C2d: Ablation: coupling vs. no coupling

### 5. Results
- Memory capacity > 5 ticks (threshold for H10)
- NRMSE < 0.1 on delayed-XOR
- Density-evolved > random-IC baseline
- Static ≈ continuous (confirms Paper 1 finding)

### 6. Discussion
- Why CA reservoirs? Interpretability, low power, spatial structure
- Limitation: binary states may limit expressive power
- Future: continuous-valued cells, deeper multi-grid hierarchies

### 7. Conclusion
- Coupled CA with local GA is a viable RC substrate
- Spatial evolution discovers configurations that random initialization cannot

## Figures
1. Reservoir architecture diagram (driven grid → coupled grid → readout)
2. C2a: Memory capacity curves (evolved vs. random)
3. C2b: NRMSE vs. delay for different tasks
4. C2c: Scaling: capacity vs. grid count
5. C2d: Ablation: TE with/without coupling

## Status
- exp_hybrid_reservoir exists but needs full task suite
- Ridge regression readout not yet implemented

## Next Steps
1. Implement ridge regression readout (C or numpy bridge)
2. Implement delayed-XOR, NARMA-10, 5-bit memory tasks
3. Run 30-seed evaluation per condition
4. Compare to ESN baseline (implement or cite)
5. Draft manuscript
6. arXiv preprint November 2026
7. Submit to NeurIPS workshop (deadline ~May 2027) or ICML (January 2027)

## Risk Assessment
- **High risk**: If memory capacity < 2 ticks, paper collapses
- **Mitigation**: density-evolved system already maintains live cells reliably (A2 finding); this is a precondition
- **Fallback**: Publish as negative result + analysis of why CA reservoirs fail

---
*Last updated: 2026-05-15*
*Priority: MEDIUM-HIGH — highest venue tier, but most new code required*
