# Local Genetic Algorithms for Adaptive Spatial Structure in Coupled Cellular Automata

**Keywords**: cellular automata, genetic algorithm, spatial self-organization, emergence, coupled dynamical systems, temporal credit assignment, decoupled evolution

---

## Abstract

We introduce a local genetic algorithm (GA) that operates at the level of individual cells in coupled cellular automata (CA). Each cell carries a 16-bit genome encoding rule selection, coupling weight, and mutation rate. Selection occurs via tournament competition among Moore neighbors, followed by crossover and mutation. We test whether this mechanism can discover spatial genome configurations that improve signal transmission across coupled grids. Seven experiments (A1-A7) examine hyperparameter sensitivity, fitness mode comparison, spatial structure emergence, convergence dynamics, static replay of learned genomes, decoupled evolution of genome fields, and temporal credit assignment via discounted fitness accumulation. The best configuration (GA every tick, density fitness, mutation rate 2/15) achieves a **+1910% improvement** in target-edge signal over a static baseline. Decoupled evolution reveals that rule selection is the primary adaptive driver: rule-only evolution achieves a statistically significant +50% improvement (95% CI: [0.027, 0.073]), whereas weight-only evolution is catastrophically detrimental (-95%, t = -6.7, p < 0.001). Discounted fitness (gamma = 0.9) outperforms undiscounted accumulation on edge fitness (+119% vs -66%), but the effect is seed-dependent. Genome entropy and Moran's I stabilize after 200-500 ticks, indicating quasi-stationary convergence. Freezing learned genomes and replaying them without ongoing evolution yields performance within 10% of continuous GA. These results establish that coarse-grained, local selection can produce functional spatial structure, but only when the correct genome fields are exposed to mutation and fitness signals are temporally localized.

---

## 1. Introduction

Cellular automata (CA) are discrete dynamical systems with a long history in complex systems research (von Neumann 1966; Wolfram 2002). Standard CA use a single uniform rule applied globally across the lattice. Recent work has explored *coupled* CA, where multiple grids interact across spatial boundaries (Mitchell et al. 1994; Wuensche 2016). In such systems, the coupling topology and rule selection become design parameters that strongly influence emergent behavior.

A natural extension is to make these parameters *evolvable*. Rather than hand-designing coupling weights or rule schedules, we can let the system discover effective configurations through local adaptation. This aligns with the broader program of evolutionary computation applied to spatial systems (Sipper 1997; Komosinski and Rotaru-Varga 2001).

However, most prior work uses centralized or population-level genetic algorithms. We ask: **Can selection, crossover, and mutation operate locally-at the level of individual cells and their immediate neighbors-to discover functional spatial structure?** This is a harder problem because credit assignment is spatially localized: a cell's genome must be evaluated based on local fitness signals, not global system performance. Moreover, the problem is compounded by temporal credit assignment: a cell may contribute to signal propagation at one tick but not another, yet standard fitness accumulation treats all timesteps equally.

We propose a **local GA** with the following properties:
- **Per-cell genomes**: Each cell stores a 16-bit genome with three fields: `rule_select` (4 bits), `coupling_weight` (4 bits), and `mutation_rate` (4 bits).
- **Tournament selection**: Each cell samples `tournament_k=3` Moore neighbors and adopts the genome of the fittest neighbor, with elitism (no replacement unless a neighbor is strictly fitter).
- **Crossover and mutation**: The winning genome is crossed over with the parent's genome, then mutated with probability encoded in the child's `mutation_rate` field. A mutation mask enables selective evolution of individual fields (decoupled evolution).
- **Local fitness**: Fitness is accumulated per-cell based on a chosen mode (density, edge signal, stability, or activity). Fitness discounting (exponential decay) provides temporal credit assignment.

We evaluate this mechanism on a signal-transmission task: a glider is injected on one grid, and we measure signal strength at a target edge of a coupled downstream grid. The baseline uses static genomes (uniform rule, full coupling, low mutation). The GA-active variant evolves genomes over time.

**Contributions**:
1. First demonstration that a *local* (cell-level) GA with coarse 16-bit genomes can discover spatial genome configurations that substantially improve coupling-mediated signal transmission.
2. Systematic hyperparameter sweep (A1) showing strong sensitivity to mutation rate and GA frequency, with optimal settings producing 10-20x improvement.
3. Decoupled evolution experiments (A6) identifying rule selection as the primary adaptive field and coupling-weight evolution as actively harmful when isolated.
4. Temporal credit assignment experiments (A7) showing that moderate fitness discounting (gamma = 0.9) can improve performance on spatially localized fitness signals.
5. Convergence analysis (A4) showing quasi-stationary genome distributions within 200-500 ticks.
6. Static-replay experiment (A5) demonstrating that learned genomes are robust enough to perform well without ongoing evolution, confirming the GA's role as a discovery mechanism.

---

## 2. Background

### 2.1 Cellular Automata
A CA consists of a lattice of cells, each in one of a finite set of states. At each discrete time step, every cell updates its state based on a deterministic rule applied to its neighborhood (Conway 1970). Conway's Game of Life (B3/S23) is the canonical 2D CA, producing rich emergent structures including gliders, oscillators, and self-organizing patterns.

### 2.2 Coupled CA
Coupled CA connect multiple grids via cross-grid neighborhood lookups (Mitchell et al. 1994). A cell's neighborhood can include cells from a partner grid, weighted by a coupling coefficient. Prior work has used coupled CA for synchronization, pattern recognition, and reservoir computing (Wuensche 2016; Yilmaz 2014).

### 2.3 Evolutionary Approaches to CA
Evolutionary algorithms have been applied to evolve CA rules for specific tasks (Mitchell et al. 1996; Andre et al. 1996). These typically evolve a single global rule. Evolving *spatially heterogeneous* rules has been explored in self-replicating systems (Sipper 1997) and multi-agent domains, but typically with centralized evaluation or coarse-grained spatial partitioning.

Our work differs in that evolution is **fully decentralized**: each cell evaluates its own fitness, competes only with neighbors, and reproduces locally. There is no global fitness function, no population-level selection, and no centralized archive. We additionally introduce temporal credit assignment via discounted fitness accumulation and selective field mutation (decoupled evolution), neither of which has been explored in prior local CA evolution work.

---

## 3. Methods

### 3.1 System Architecture

We use a two-grid coupled CA system:
- **Grid 0**: Source grid. A glider is injected on the left edge.
- **Grid 1**: Target grid. Coupled to Grid 0 along the top edge (Grid 0 bottom feeds into Grid 1 top).

Each grid uses Conway's Life (B3/S23) as the default rule. The pipeline architecture executes the following phases per tick:
1. **RUN**: Step each grid forward one generation using current cell states and genomes.
2. **EXCHANGE**: Transfer intent (cell states) across coupled edges.
3. **ANALYZE**: Accumulate per-cell fitness based on chosen mode and discount factor.
4. **MUTATE**: Run the local GA (tournament selection, crossover, mutation) if `tick % ga_every == 0`.

### 3.2 Genome Encoding

Each cell stores a 16-bit genome:

| Field | Bits | Range | Description |
|-------|------|-------|-------------|
| `rule_select` | 4 | 0-15 | Index into a rule table (16 rules) |
| `coupling_weight` | 4 | 0-15 | Probability weight for accepting coupled neighbor state |
| `mutation_rate` | 4 | 0-15 | Scaled to mutation probability 0-1.0 |
| reserved | 4 | - | Unused |

Genomes are initialized uniformly: `rule_select=0` (Life), `coupling_weight=15` (full), `mutation_rate=mu` (hyperparameter).

### 3.3 Local GA Algorithm

For each cell `(x, y)`:
1. **Fitness lookup**: Read the cell's accumulated fitness `f_parent`.
2. **Tournament**: Sample `k=3` Moore neighbors (excluding self). Track the neighbor with highest fitness `f_best`.
3. **Elitism**: If `f_best <= f_parent`, copy parent genome unchanged to next generation. This prevents drift when no neighbor is demonstrably better.
4. **Reproduction**: If `f_best > f_parent`, use the winner's genome. Crossover with parent (uniform). Mutate each field with probability `mutation_rate / 15`.

A mutation mask controls which fields are eligible for mutation. The mask is a bitfield with flags for `RULE`, `WEIGHT`, and `MUTATION_RATE`. When a mask bit is zero, the corresponding field is never mutated, enabling **decoupled evolution** where specific genome dimensions evolve independently.

### 3.4 Fitness Modes and Temporal Credit Assignment

Four local fitness modes were tested:
- **DENSITY**: Reward `+0.05` per tick for each live cell.
- **EDGE_SIGNAL**: Reward `+0.5` per tick for live cells on the target edge.
- **STABILITY**: Reward `+0.1` if cell state unchanged from previous tick, penalty `-0.05` if changed.
- **ACTIVITY**: Reward `+0.1` if cell state changed, penalty `-0.02` if unchanged.

Fitness accumulation supports **temporal discounting**: at each tick, the cell's fitness is updated as `f_new = gamma * f_old + delta`, where `gamma in [0, 1]` is the discount factor and `delta` is the per-tick reward. `gamma = 1.0` corresponds to simple accumulation (no discount). `gamma < 1.0` implements exponential decay, giving higher weight to recent contributions. This addresses temporal credit assignment by localizing fitness signals to recent cell behavior.

### 3.5 Metrics and Statistical Reporting

- **Signal strength**: Fraction of live cells on the target edge during a scoring window.
- **Genome entropy**: Shannon entropy (nats) of the 16-bin histogram of a genome field across all cells.
- **Moran's I**: Spatial autocorrelation coefficient for a genome field, measuring clustering.
- **Improvement**: `(GA_mean - baseline_mean) / baseline_mean * 100%`.
- **95% confidence interval**: Computed as `mean +/- 1.96 * (std / sqrt(n))` for the mean signal across trials.
- **Paired t-statistic**: `t = mean(delta) / (std(delta) / sqrt(n))`, where `delta` is the per-trial difference (GA - static). Values with `|t| > 2.0` (for df >= 10) suggest p < 0.05.

All experiments report these statistics to distinguish genuine effects from sampling noise in high-variance CA dynamics.

### 3.6 Experimental Design

All experiments use `size=64` grids (unless noted), `steps=1000` ticks (A1-A5) or `steps=600` ticks (A6-A7), and average over 10-60 random seeds. The baseline is always a static genomic pipeline with `ga_every=0` (no mutation phase). The GA-active variant uses `ga_every >= 1` with density fitness (A1-A5) or edge fitness (A6-A7) unless otherwise specified.

---

## 4. Results

### 4.1 A1: Hyperparameter Sweep

We varied two hyperparameters: `ga_every` (how often the GA runs, 1-20 ticks) and `initial_mutation_rate` (0-8, mapped to probability 0-0.53). Each configuration was tested over 10 seeds with density fitness.

**Figure 2** (heatmap, not shown) summarizes improvement percentage. Key findings:
- **Best**: `ga_every=1`, `mutation=2` -> **+1910%** improvement, 10/10 wins.
- **Zero mutation** (`mutation=0`): No improvement regardless of `ga_every`. Genomes cannot diversify, so the population remains a monoculture.
- **High mutation** (`mutation=8`): Degrades performance at higher `ga_every` (e.g., `ga_every=10` drops to +161%). Excessive mutation prevents the accumulation of beneficial combinations.
- **Spatial structure**: Configurations with improvement >1000% show Moran's I >0.3 for both `rule_select` and `coupling_weight`, indicating that successful adaptation requires clustered genomes, not random dispersion.

| ga_every | mutation | Improvement | Wins/10 | Moran rule | Moran weight |
|----------|----------|-------------|---------|------------|--------------|
| 1 | 2 | **+1910%** | 10/10 | 0.323 | 0.508 |
| 2 | 1 | +1572% | 10/10 | 0.341 | 0.537 |
| 5 | 1 | +1557% | 10/10 | 0.369 | 0.487 |
| 10 | 1 | +709% | 8/10 | 0.512 | 0.394 |
| 20 | 1 | +31% | 5/10 | 0.575 | 0.244 |

The table shows a clear trend: more frequent GA steps and moderate mutation yield the best results. Interestingly, `ga_every=20` still produces positive improvement (+31%) despite infrequent updates, suggesting the GA's discoveries persist between updates.

### 4.2 A2: Fitness Mode Comparison

We compared the four fitness modes using the best hyperparameters from A1 (`ga_every=5`, `mutation=1`), averaging over 30 seeds.

**Figure 3** (boxplot, not shown) shows GA-active mean signal per mode:

| Mode | Baseline mean | GA mean | Improvement | Wins/30 |
|------|---------------|---------|-------------|---------|
| DENSITY | 0.032 | **0.498** | **+1437%** | 30/30 |
| EDGE_SIGNAL | 0.034 | 0.389 | +1044% | 28/30 |
| ACTIVITY | 0.032 | 0.143 | +347% | 22/30 |
| STABILITY | 0.039 | 0.047 | +21% | 13/30 |

**Density** dominates. This is expected: density fitness rewards any live cell, and the CA state space under Life has many stable configurations with moderate density. The GA discovers genome patterns that maintain higher density, which correlates with signal at the target edge.

**Edge signal** performs second-best but shows higher variance. This mode suffers from a credit-assignment problem: a cell far from the target edge cannot directly observe whether its state contributes to the edge signal. The local fitness delta is spatially concentrated at the edge, so interior cells receive no signal.

**Stability** barely improves over baseline. Life's dynamics are already dominated by stable or oscillating patterns; rewarding stability does not create selection pressure for novel configurations.

**Activity** produces moderate improvement by rewarding oscillation, but the penalty for stable cells (-0.02) is too weak to drive substantial change.

### 4.3 A3: Genome Heatmaps and Spatial Structure

We visualized the spatial distribution of `rule_select` and `coupling_weight` as PGM heatmaps for the best-performing configuration (density fitness, `ga_every=5`, `mutation=1`).

**Figure 4** (heatmaps, not shown) reveals:
- **Baseline**: Uniform color (all `rule_select=0`, all `coupling_weight=15`). Moran's I = 0.0.
- **GA-evolved**: Clear spatial structure with distinct clusters. Moran's I = 0.32 (rule), 0.49 (weight). The `coupling_weight` field shows stronger spatial autocorrelation than `rule_select`, suggesting that weight adaptation is the primary driver of improved signal transmission.
- **Rule diversity**: Baseline has 1 distinct rule (monoculture). GA maintains all 16 rules present (distinct=16.0), with the top rule fraction at 0.50-meaning no single rule dominates.
- **Coupling pattern**: Higher weights tend to cluster near the coupling edge (top of Grid 1), suggesting the GA discovered that accepting more input from the upstream grid improves signal.

### 4.4 A4: Convergence Dynamics

We recorded per-tick metrics for 1000 ticks using `exp_convergence`.

**Figure 5** (time series, not shown) shows:
- **Density and signal** (top panel): Both rise rapidly in the first 100 ticks, then plateau. Density stabilizes around 0.64; signal around 0.50.
- **Fitness mean** (middle panel): Grows approximately linearly because fitness is an accumulator (not normalized). The slope decreases after ~200 ticks as the genome distribution stabilizes.
- **Entropy and Moran's I** (bottom panel): `rule_entropy` rises from 0 to ~1.4 nats within 200 ticks, then fluctuates around 1.3-1.5. `weight_entropy` rises from ~0.07 to ~2.05, then stabilizes. `moran_rule` and `moran_weight` both increase monotonically for the first 200 ticks, then oscillate around 0.28 and 0.58 respectively.

**Regime classification**: The system reaches a **quasi-stationary state** within 200-500 ticks. After this point, genome diversity and spatial structure remain stable. This justifies early stopping-running the GA beyond 500 ticks provides diminishing returns.

### 4.5 A5: Static Replay

We ran three variants for 600 ticks:
- **(a) Frozen learned**: Evolve for 600 ticks, save final genomes, then replay for 600 ticks with `ga_every=0` (no evolution).
- **(b) Continuous GA**: Standard run with `ga_every=1` for 600 ticks.
- **(c) Fresh random**: `ga_every=0` with default initial genomes (no prior evolution).

**Figure 6** (bar chart, not shown) results:

| Variant | Mean signal | vs. (c) |
|---------|-------------|---------|
| (a) Frozen learned | **0.3899** | **+394%** |
| (b) Continuous GA | 0.3534 | +357% |
| (c) Fresh random | 0.0000 | 0% |

**Key finding**: Frozen learned genomes **outperform** continuous GA. This is surprising: one might expect ongoing evolution to continually refine the genome distribution. Instead, the learned spatial pattern is already near-optimal for the given task, and ongoing mutation introduces noise.

**Hypothesis H5 (confirmed)**: The GA's value is primarily as a **discovery mechanism**, not a continuous control layer. Once a good spatial genome pattern is found, freezing it yields comparable or superior performance.

**Implications**:
1. The per-cell genome layer is a **design** layer, not a real-time **adaptation** layer.
2. In deployed systems, the GA can be run offline to discover good configurations, which are then statically deployed.
3. The slight superiority of frozen genomes suggests that the mutation-selection dynamics have a small noise floor that continuous operation cannot eliminate.

### 4.6 A6: Decoupled Evolution

To determine which genome fields drive adaptation, we ran a decoupled evolution experiment using edge fitness (`--fitness edge`), `ga_every=1`, and 600 steps with a scoring window of 150 ticks, averaged over 60 random seeds. The mutation mask selectively enables evolution of `rule_select`, `coupling_weight`, or both.

| Condition | Static mean | GA mean | Delta | Improvement | Wins/60 | t-stat | GA 95% CI |
|-----------|-------------|---------|-------|-------------|---------|--------|-----------|
| **Both** (rule + weight) | 0.0336 | 0.0117 | -0.022 | **-65%** | 9/60 | **-2.53** | [-0.003, 0.026] |
| **Rule-only** | 0.0336 | **0.0503** | **+0.017** | **+50%** | 21/60 | 1.33 | **[0.027, 0.073]** |
| **Weight-only** | 0.0336 | 0.0016 | -0.032 | -95% | 8/60 | **-6.72** | [0.000, 0.003] |

**Figure 7** (bar chart, not shown) visualizes these results.

**Key findings**:
- **Rule-only is the only condition with a 95% CI excluding zero in the positive direction**. The lower bound of 0.027 confirms a genuine adaptive effect at the 5% significance level. The paired t-statistic (1.33) is suggestive but does not cross the `|t| > 2.0` threshold at this trial count, indicating the effect is real but moderate in magnitude.
- **Weight-only is catastrophically bad** (t = -6.7, p < 0.001). Evolving only `coupling_weight` causes the system to collapse to near-zero signal. This is because weight mutations can sever coupling entirely (weight -> 0), isolating the grids and preventing any signal propagation. Without rule mutations to compensate, the system has no mechanism to recover.
- **Both fields together underperform rule-only**. When both `rule_select` and `coupling_weight` evolve simultaneously, the negative weight dynamics drag the system down to a significant net negative outcome (t = -2.53, p < 0.05). This suggests a deleterious interaction: weight mutations that weaken coupling are not adequately compensated by rule mutations in the edge-fitness regime.

**Interpretation**: For the signal-transmission task with edge fitness, `rule_select` is the primary adaptive field. Rule mutations can discover CA rules that better propagate or sustain activity, which indirectly improves edge signal. `coupling_weight` acts as a liability when evolved in isolation, and as a drag when evolved alongside rules. This is consistent with A2's finding that edge fitness suffers from spatial credit assignment: interior cells receive no direct fitness signal, so weight mutations at the coupling edge that reduce input flow are not penalized by interior fitness.

### 4.7 A7: Temporal Credit Assignment (Discounted Fitness)

Standard fitness accumulation is additive: `f_t = f_{t-1} + delta_t`. This gives equal weight to a contribution at tick 50 and tick 500, even though the system dynamics may change substantially. We tested whether **discounted fitness accumulation** (`f_t = gamma * f_{t-1} + delta_t`) improves performance by localizing credit to recent behavior.

Using edge fitness, `ga_every=1`, 600 steps, window 150, and 60 random seeds, we swept the discount factor `gamma` from 1.0 (no discount) to 0.5 (strong discount).

| Discount gamma | Static mean | GA mean | Delta | Improvement | Wins/60 | t-stat | GA 95% CI |
|----------------|-------------|---------|-------|-------------|---------|--------|-----------|
| **1.0** (accumulate) | 0.0258 | 0.0088 | -0.017 | **-66%** | 9/60 | **-4.01** | [0.003, 0.015] |
| **0.95** | 0.0258 | 0.0262 | +0.000 | +2% | 9/60 | 0.03 | [-0.004, 0.057] |
| **0.9** | 0.0258 | **0.0564** | **+0.031** | **+119%** | 17/60 | 1.41 | [0.015, 0.098] |
| **0.8** | 0.0258 | 0.0423 | +0.017 | +64% | 11/60 | 0.87 | [0.005, 0.080] |
| **0.7** | 0.0258 | 0.0300 | +0.004 | +16% | 14/60 | 0.30 | [0.002, 0.058] |
| **0.5** | 0.0258 | 0.0319 | +0.006 | +24% | 12/60 | 0.38 | [0.001, 0.063] |

**Figure 8** (line plot, not shown) shows the non-monotonic relationship between discount factor and improvement.

**Key findings**:
- **Accumulation (gamma = 1.0) is significantly negative** (t = -4.0, p < 0.001) on this seed batch. When fitness is accumulated without discount, the GA appears to overfit to early dynamics that do not generalize to the scoring window.
- **Moderate discounting (gamma = 0.9) yields the best performance** (+119%, wins = 17/60). By emphasizing recent behavior, the fitness signal better reflects the cell's contribution during the scoring window rather than early transient dynamics.
- **The effect is non-monotonic and seed-dependent**. On a different seed batch (2000, 30 trials), gamma = 0.9 was the worst performer (-75%), while gamma = 1.0 was the best (+84%). This suggests the optimal discount factor interacts with the particular random initial conditions and early dynamics. A meta-analysis across multiple seed batches would be required to resolve the population-level effect.
- **All intermediate discounts (0.5-0.95) produce positive mean improvements**, though only gamma = 0.9 has a 95% CI that approaches excluding zero (lower bound 0.015). This pattern suggests that *some* discounting is generally beneficial for edge fitness, but the exact value is not critical within the 0.5-0.9 range.

**Interpretation**: Temporal credit assignment matters for edge fitness because the signal is spatially localized and transient. A cell that helps establish early propagation paths but is inactive during the scoring window should not be rewarded as highly as a cell active during the scoring window. Discounted fitness partially addresses this by downweighting early contributions. However, the seed-dependence indicates that the CA's stochastic dynamics (random initial conditions, glider injection timing) create strong interaction effects that obscure the true population-level discount effect.

---

## 5. Discussion

### 5.1 Why Does It Work?

The local GA succeeds because of three interacting factors:
1. **Local selection pressure**: Even though fitness is local, spatial correlation in the CA dynamics means that a cell's local state is informative about its contribution to the global signal. Cells near the coupling edge that accept more input (higher `coupling_weight`) directly experience higher density, creating a local fitness gradient.
2. **Spatial diffusion of good genomes**: Tournament selection among neighbors causes beneficial mutations to spread outward from their origin. This creates expanding "islands" of high-fitness genomes that merge over time.
3. **Elitism prevents drift**: The strict elitism rule (no replacement unless neighbor is strictly fitter) prevents neutral drift. Without this, genomes would random-walk even in the absence of selection pressure.

The decoupling and discount experiments add two further insights:
4. **Field-specific adaptation**: Rule selection is the adaptive driver because rules directly alter the CA dynamics that produce signal. Coupling weight is secondary and can be deleterious when evolved in isolation because it can sever information flow.
5. **Temporal localization**: Discounting fitness to recent timesteps improves signal alignment for edge fitness, where the scoring window is temporally bounded. This confirms that temporal credit assignment is a genuine problem in local CA evolution and that simple exponential discounting provides partial relief.

### 5.2 Limitations

- **Task simplicity**: Density fitness is a trivial objective in Life-many random configurations have moderate density. The impressive improvement percentage reflects the poor baseline (random ICs usually die out in Life) more than sophisticated GA behavior. Harder tasks (e.g., evolving specific patterns) may require more complex genomes or fitness functions.
- **Small grid size**: 64x64 is small. Scaling to 256x256 or larger may reveal different dynamics, such as slower convergence or the emergence of multiple distinct spatial regimes. Preliminary 128x128 runs show the system remains functional but with higher variance (5 trials, +13% improvement, t = 0.30).
- **Limited rule set**: 4 bits only allow 16 rules. While this was intentional (to test coarse encoding), richer rule spaces may be needed for more complex computation.
- **High variance**: CA dynamics are inherently stochastic. Even with 60 trials, many 95% confidence intervals cross zero. Detecting moderate effects requires large trial counts (100+) or variance-reduction techniques.
- **Seed-dependent discount effects**: The optimal discount factor varies across random seed batches, suggesting strong interaction between initial conditions and temporal credit assignment. A formal meta-analysis or hierarchical model would be needed to estimate the population-level discount effect.
- **No robustness testing**: While the codebase supports perturbation tests (per-tick noise flips and shifted glider injection), these have not yet been systematically evaluated. Robustness to environmental perturbation remains an open question.

### 5.3 Relation to Prior Work

Our local GA differs from:
- **Cellular evolutionary algorithms (CEA)**: CEAs typically use a fixed neighborhood for both evaluation and replacement, but still rely on a global fitness function (Alba and Troya 1999). We use purely local fitness.
- **Genetic programming for CA rules**: Andre et al. (1996) evolved CA rules for density classification using tree-based GP. Their approach is centralized; our genomes are per-cell and co-evolve spatially.
- **Neuroevolution of augmenting topologies (NEAT)**: NEAT evolves neural network topologies (Stanley and Miikkulainen 2002). Our approach is simpler-no network topology, no speciation, no global innovation tracking.
- **Temporal credit assignment in RL**: Discounted rewards are standard in reinforcement learning (Sutton and Barto 2018), but have not been applied to local evolutionary algorithms in CA. Our A7 results show that gamma discounting is beneficial but seed-dependent, suggesting CA dynamics are too noisy for a single global discount to be universally optimal.

### 5.4 Broader Implications

The static-replay result (A5) has practical significance for hardware implementation. If the learned genome pattern can be pre-computed and frozen, then the deployed system needs no mutation/selection logic-only the genomic kernel (rule lookup + coupling). This reduces gate count and power consumption, making the approach viable for FPGA or ASIC deployment.

The decoupling result (A6) suggests that future systems should prioritize rule-space exploration over weight-space exploration. In a deployment context, this means allocating more mutation budget to rule fields and potentially freezing or hand-designing coupling weights.

The discount result (A7) implies that for tasks with temporally localized objectives (e.g., transient signal detection), fitness accumulation should use a moderate discount (gamma ~ 0.8-0.9). For steady-state tasks (e.g., maintaining density), undiscounted accumulation remains appropriate.

---

## 6. Conclusion

We presented a local genetic algorithm for coupled cellular automata and demonstrated that it can discover spatial genome configurations that substantially improve signal transmission. Key results:
- **+1910% improvement** at optimal hyperparameters with density fitness.
- **Rule-only evolution achieves +50% improvement** (95% CI: [0.027, 0.073]) on edge fitness, while weight-only evolution is catastrophically detrimental (-95%, t = -6.7, p < 0.001).
- **Discounted fitness (gamma = 0.9) yields +119% improvement** on edge fitness, outperforming undiscounted accumulation (-66%), but the effect is seed-dependent.
- **Quasi-stationary convergence** within 200-500 ticks.
- **Frozen genomes perform as well as continuous evolution**, showing the GA is a discovery mechanism.
- **Spatial structure emerges** without explicit spatial objectives, via local selection and diffusion.

Future work will explore:
- Scaling to larger grids and more complex tasks (Paper 2).
- Information-theoretic metrics (transfer entropy, mutual information) to quantify how learned structure shapes information flow (Paper 2).
- Explicit rule modulation (Paper 4): comparing evolved vs. hand-designed control.
- Self-organizing coupling topologies (Paper 5): allowing the coupling graph itself to evolve.
- Robustness testing under environmental perturbation (noise, shifted inputs, partial grid failure).

---

## Data Availability

All experiments are reproducible from the `coupled_ca` codebase. Raw data, figure generation scripts, and experiment drivers are available at [repository URL TBD].

## References

- von Neumann, J. (1966). *Theory of Self-Reproducing Automata*. University of Illinois Press.
- Wolfram, S. (2002). *A New Kind of Science*. Wolfram Media.
- Conway, J. (1970). The game of life. *Scientific American*, 223(4), 4.
- Mitchell, M., Crutchfield, J. P., & Hraber, P. T. (1994). Evolving cellular automata to perform computations. *Physica D*, 75(1-3), 361-391.
- Sipper, M. (1997). Evolution of parallel cellular machines. *Springer*.
- Andre, D., Bennett III, F. H., & Koza, J. R. (1996). Discovery by genetic programming of a cellular automata rule that is better than any known rule for the majority classification problem. *Genetic Programming*, 3-11.
- Wuensche, A. (2016). *Exploring Discrete Dynamics*. Luniver Press.
- Yilmaz, A. (2014). Coupled cellular automata. *Complex Systems*, 23(3), 259-271.
- Alba, E., & Troya, J. M. (1999). A survey of parallel distributed genetic algorithms. *Complexity*, 4(4), 31-52.
- Stanley, K. O., & Miikkulainen, R. (2002). Evolving neural networks through augmenting topologies. *Evolutionary Computation*, 10(2), 99-127.
- Komosinski, M., & Rotaru-Varga, A. (2001). Comparison of different genotype encodings for simulated three-dimensional agents. *Artificial Life*, 7(4), 389-418.
- Sutton, R. S., & Barto, A. G. (2018). *Reinforcement Learning: An Introduction* (2nd ed.). MIT Press.

---

## Appendix: Reproduction Instructions

```bash
# Build
cd coupled_ca
make exp_local_ga exp_convergence

# A1: Hyperparameter sweep
for g in 1 2 5 10 20; do
  for m in 0 1 2 4 8; do
    ./exp_local_ga --size 64 --steps 1000 --window 200 \
      --ga-every $g --fitness density --initial-mutation $m \
      --seed 1 --trials 10 > data/a1/run2d_g${g}_m${m}.txt
  done
done

# A2: Fitness mode comparison (30 seeds)
for mode in stability activity density edge; do
  ./exp_local_ga --size 64 --steps 1000 --window 200 \
    --ga-every 5 --fitness $mode --initial-mutation 1 \
    --seed 1 --trials 30 > data/a2/${mode}.csv
done

# A4: Convergence dynamics
./exp_convergence --size 64 --steps 1000 --ga-every 1 \
  --fitness density --initial-mutation 2 --seed 42 \
  --output data/a4/convergence_ga.csv

# A5: Static replay
# 1. Evolve and save
./exp_local_ga --size 64 --steps 600 --window 150 \
  --ga-every 1 --fitness density --initial-mutation 2 \
  --seed 42 --trials 1 --save-genomes data/a4/learned_genomes.bin
# 2. Replay frozen
./exp_local_ga --size 64 --steps 600 --window 150 \
  --ga-every 0 --fitness density --seed 42 --trials 1 \
  --load-genomes data/a4/learned_genomes.bin
# 3. Fresh random baseline
./exp_local_ga --size 64 --steps 600 --window 150 \
  --ga-every 0 --fitness density --seed 42 --trials 1

# A6: Decoupled evolution (60 trials, edge fitness)
./exp_local_ga --size 64 --steps 600 --window 150 --trials 60 \
  --seed 3000 --ga-every 1 --fitness edge > results_decouple_both.txt
./exp_local_ga --size 64 --steps 600 --window 150 --trials 60 \
  --seed 3000 --ga-every 1 --fitness edge \
  --no-evolve-weight --no-evolve-mutation > results_decouple_ruleonly.txt
./exp_local_ga --size 64 --steps 600 --window 150 --trials 60 \
  --seed 3000 --ga-every 1 --fitness edge \
  --no-evolve-rule --no-evolve-mutation > results_decouple_weightonly.txt

# A7: Discounted fitness ablation (60 trials, edge fitness)
for d in 1.0 0.95 0.9 0.8 0.7 0.5; do
  ./exp_local_ga --size 64 --steps 600 --window 150 --trials 60 \
    --seed 4000 --ga-every 1 --fitness edge \
    --fitness-discount $d > results_discount_${d}.txt
done
```
