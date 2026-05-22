# Paper 5: Self-Organizing Coupling Topologies via Transfer Entropy

## Metadata
- **Venue**: IEEE Trans. Evol. Comp. / Artificial Life
- **Preprint**: arXiv, January 2027
- **Hypotheses**: H9, H13
- **Experiments**: C1, E1

## Abstract
We investigate whether coupled CA can self-organize their coupling topology via transfer-entropy-driven edge pruning. Starting with all-to-all edge couplings between N grids, we periodically measure per-edge transfer entropy and disable edges below threshold. Surviving edge count converges to non-trivial sparse topologies that depend on the rule pair — Life-Life differs from Life-HighLife. Evolved topologies outperform random matched-density topologies on information transmission. This demonstrates structural sensitivity in coupled CA: not just how many edges, but which edges matter.

## Structure
1. Introduction — Self-organization of network topology
2. Related Work — TE for network inference; evolution of topology
3. Methods — phase_mutate_coupling; TE threshold sweep; topology metrics
4. Experiments — C1: TE-driven pruning; E1: GA on coupling graph
5. Results — Converged edge counts; rule-dependent topologies; evolved > random
6. Discussion — Structural sensitivity vs. scalar coupling; implications for hardware design
7. Conclusion — Coupled CA exhibit self-organizing topology properties

## Figures
1. Topology evolution diagram
2. Edge count vs. time for different thresholds
3. Surviving topology visualizations per rule pair
4. TE comparison: evolved vs. random topology

## Status
- ❌ Not started

## Next Steps
1. Implement phase_mutate_coupling
2. Implement per-edge TE computation
3. Run pruning experiments (multiple thresholds, rule pairs)
4. Implement topology GA (E1)
5. Draft manuscript
6. arXiv preprint January 2027

---
*Last updated: 2026-05-15*
