# Paper 4: Search vs. Design: Explicit Rule Modulation in Coupled CA

## Metadata
- **Venue**: GECCO 2027 / PPSN XVI / Artificial Life
- **Preprint**: arXiv, October 2026
- **Hypotheses**: H11
- **Experiments**: C3

## Abstract
We compare hand-designed explicit rule modulation against GA-evolved rule selection in coupled CA. In explicit modulation, one grid's state selects another grid's rule per-cell (e.g., "if grid 0 alive, use HighLife on grid 1, else use Life"). We test whether hand-designed maps outperform GA-discovered maps on signal transmission, given equal compute budget. Certain explicit modulations (Life/HighLife) achieve comparable signal to best GA configs, but GA discovers non-obvious combinations missed by hand design. The trade-off: explicit is interpretable and instant; evolved is adaptive but requires search.

## Structure
1. Introduction — Search vs. design in spatial systems
2. Related Work — Mitchell et al. (1996), Wolfram (2002), Sipper (1997)
3. Methods — New phase_modulate_rule; 9 rule pairs; 30 seeds
4. Experiments — C3: explicit vs. evolved vs. static baseline
5. Results — Explicit wins on some pairs; GA discovers surprising pairs
6. Discussion — When to use each approach; implications for DSL design
7. Conclusion — Both approaches valid; choice depends on interpretability needs

## Figures
1. Explicit modulation diagram
2. Signal comparison: explicit vs. evolved vs. static
3. Rule-pair matrix heatmap

## Status
- ❌ Not started

## Next Steps
1. Implement phase_modulate_rule
2. Implement exp_rule_mod.c driver
3. Run 30-seed evaluation per rule pair
4. Draft manuscript
5. arXiv preprint October 2026

---
*Last updated: 2026-05-15*
