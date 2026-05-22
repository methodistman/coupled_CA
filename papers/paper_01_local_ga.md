# Paper 1: Local GA for Adaptive Spatial Structure in Coupled CA

## Metadata
- **Venue**: Artificial Life / Complex Systems / NetSci 2027
- **Preprint**: arXiv, August 2026
- **Hypotheses**: H1–H7
- **Experiments**: A1–A7

## Abstract
We introduce a decentralized GA evolving 16-bit per-cell genomes in coupled multi-grid CA. Each genome encodes rule selection, coupling weight, and mutation rate. Evolution uses only local tournament selection (k=3) among Moore neighbors — no global fitness. On a signal-transmission task, density fitness achieves +1910% improvement over static baselines (30/30 wins, p ≈ 10⁻⁹). Decoupled evolution reveals rule selection is the primary adaptive driver: rule-only achieves +50% (95% CI: [0.027, 0.073]), while weight-only is catastrophically detrimental (-95%, t = -6.7). Discounted fitness (gamma = 0.9) outperforms undiscounted accumulation on edge fitness (+119% vs -66%), but the effect is seed-dependent. Genome heatmaps show emergent spatial structure (Moran's I up to 0.57), but critically, spatial autocorrelation does not predict performance: intermediate clustering (Moran ≈ 0.3) outperforms strong clustering (Moran > 0.5). Static-genome replay matches continuous GA within 10%, showing the GA's primary value is discovering effective configurations, not continuous adaptation.

## Structure

### 1. Introduction (1000 words)
- Motivation: credit assignment in spatially-distributed systems
- Gap: CA evolution uses global fitness; local fitness underexplored
- Temporal credit assignment: local fitness lacks temporal localization
- Contribution: per-cell genomes with purely local selection, decoupled evolution, discounted fitness

### 2. Related Work (700 words)
- Packard & Mitchell (1992): global fitness CA evolution
- Giacobini et al. (2003): cellular GA with neighborhood selection
- Sutton & Barto (2018): temporal credit assignment in RL
- Key difference: our fitness is accumulated locally with discounting; our genomes support selective field mutation

### 3. Methods (1500 words)
- 3.1 Architecture: multi-grid, toroidal, directed coupling
- 3.2 Genome: 16-bit (rule_select 4b, coupling_weight 4b, mutation_rate 4b, reserved 4b)
- 3.3 Local GA: tournament k=3, uniform crossover, elitist replacement
- 3.4 Fitness modes: DENSITY, EDGE_SIGNAL, STABILITY, ACTIVITY
- 3.5 Temporal credit assignment: discounted fitness accumulation (gamma in [0, 1])
- 3.6 Decoupled evolution: mutation mask for selective field evolution
- 3.7 Metrics: Moran's I, transfer entropy, genome diversity, 95% CI, paired t-tests

### 4. Experiments (1500 words)
- A1: Hyperparameter sweep (ga_every × mutation_rate). Best: ga_every=1, mut=2, +1910%
- A2: Fitness-mode shootout. Density dominates; edge fitness fails credit assignment
- A3: Genome heatmaps. Visual clustering confirmed; Moran's I > 0.3 threshold reached
- A4: Convergence dynamics. Quasi-stationary after 200–500 ticks
- A5: Static replay. Frozen learned genomes ≈ continuous GA
- A6: Decoupled evolution. Rule-only +50%; weight-only -95% (t = -6.7, p < 0.001)
- A7: Discounted fitness ablation. Gamma = 0.9 best (+119%), but seed-dependent

### 5. Results (1000 words)
- Tables: sweep results, fitness comparison, Moran's I values, decoupling results, discount ablation
- Figures: parameter heatmap, fitness bar chart, genome heatmaps, time-series, decoupling bar chart, discount line plot

### 6. Discussion (800 words)
- Credit assignment: density fitness solves local-global mismatch; discounting helps edge fitness
- Decoupling: rule selection is the adaptive driver; weight evolution is a liability
- Structure-performance decoupling: intermediate clustering is optimal
- Static vs. dynamic: GA is a design tool, not a control layer
- Limitations: binary states only, 2D Moore neighborhood, high variance, seed-dependent discount effects

### 7. Conclusion (300 words)
- Local selection produces adaptive spatial structure without global coordination
- Fitness function design and mutation mask configuration matter more than GA mechanism
- Temporal credit assignment is beneficial but requires task-specific tuning
- Framework applicable to sensor networks, tissue modeling, edge ML

## Figures
1. System architecture diagram
2. A1: Signal improvement heatmap (ga_every × mutation_rate)
3. A2: Fitness mode comparison (boxplot)
4. A3: Genome heatmaps (rule_select, coupling_weight)
5. A4: Convergence time-series (fitness, entropy, Moran's I)
6. A5: Static replay comparison (bar chart)
7. A6: Decoupled evolution comparison (bar chart with 95% CI)
8. A7: Discounted fitness ablation (line plot with 95% CI)

## Data Availability
All experiments reproducible from commit hash [TBD] with `make exp_local_ga`.

## Status
- A1: ✅ Complete — 10 seeds, density fitness
- A2: ✅ Complete — 30 seeds, all 4 modes
- A3: ✅ Complete — heatmaps generated
- A4: ✅ Complete — quasi-stationary convergence after 200–500 ticks
- A5: ✅ Complete — frozen learned genomes outperform continuous GA (+394% vs +357%)
- A6: ✅ Complete — 60 trials, edge fitness, rule-only +50% (CI: [0.027, 0.073]), weight-only -95% (t = -6.7)
- A7: ✅ Complete — 60 trials, edge fitness, gamma sweep 1.0–0.5, best: 0.9 (+119%)

## Next Steps
1. Generate Figures 7–8 from collected data
2. Polish manuscript draft (target 5000 words)
3. Internal review and revision
4. arXiv preprint August 2026
5. Submit to NetSci/ALIFE 2027

## Anticipated Reviewer Concerns
- "Is +1910% just because density fitness keeps cells alive trivially?"
  - Response: Yes, and that's the point — the paper shows the *mechanism* of credit assignment, not claiming sophisticated computation. C2/C3 experiments test harder tasks.
- "Why 16-bit genomes?"
  - Response: Coarse encoding is intentional — tests whether simple local search can work without massive parameter spaces. Future work can expand.
- "How does this scale to larger grids?"
  - Response: Preliminary 128x128 runs show the system remains functional but with higher variance. Full scaling analysis in Paper 2.
- "Why is weight-only evolution harmful?"
  - Response: Weight mutations can sever coupling (weight -> 0), isolating grids. Without rule mutations to compensate, signal propagation collapses.
- "Is the discount effect real or a seed artifact?"
  - Response: All intermediate discounts (0.5-0.95) produce positive mean improvements, suggesting a genuine population-level trend. However, the exact optimal gamma is seed-dependent, indicating high interaction with initial conditions.

## Broader Impact
- Open-source engine enables reproducible CA research
- Local fitness principle applicable to decentralized ML (federated learning, edge inference)
- Decoupled evolution and discounted fitness are general techniques applicable beyond CA
- No dual-use concerns; purely computational research

## References (key)
- Packard, N. & Mitchell, M. (1992). Evolution of emergent computation. SFI Working Paper.
- Crutchfield, J. & Mitchell, M. (1995). The evolution of emergent computation. PNAS.
- Giacobini, M., Alba, E., & Tomassini, M. (2003). Cellular GA: evaluating the influence of ratio. Springer.
- Wolfram, S. (2002). A New Kind of Science. Wolfram Media.
- Moran, P.A.P. (1950). Notes on continuous stochastic phenomena. Biometrika.
- Schreiber, T. (2000). Measuring information transfer. PRL.
- Sutton, R. S., & Barto, A. G. (2018). Reinforcement Learning: An Introduction (2nd ed.). MIT Press.

---
*Last updated: 2026-05-16*
*Priority: HIGH — strongest data, fastest path to publication*
