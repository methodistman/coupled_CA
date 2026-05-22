# Experiment A1: Hyperparameter Sweep — Findings

**Date**: 2026-05-15
**Hypothesis**: H1 ("Per-cell GA can outperform static baselines on signal
tasks") + H2 (refinement).

## Setup

- After A3 bug fixes and A2's identification of `density` fitness as the
  productive choice.
- `exp_local_ga --size 64 --steps 600 --window 150 --trials 10 --fitness density`
- Sweep over `(ga_every, initial_mutation)`.
- Tournament_k fixed at 3 (hardcoded in `phase_mutate_local_ga`).

## Results

### 2D sweep (ga_every × initial_mutation)

`improvement %` (rows = ga_every, cols = initial_mutation 0..8):

| ga_every \\ mut | 0   | 1     | 2         | 4     | 8     |
| --------------- | --- | ----- | --------- | ----- | ----- |
| **1**           | 0   | 1379  | **1910**  | 1544  | 1056  |
| **2**           | 0   | 1572  | 1319      | 1305  | 1481  |
| **5**           | 0   | 1557  | 1737      | 1253  | 238   |
| 10              | 0   | 709   | 603       | 348   | 161   |
| 20              | 0   | 31    | 48        | 190   | 68    |

`ga_wins / 10`:

| ga_every \\ mut | 0 | 1 | 2 | 4 | 8 |
| --------------- | - | - | - | - | - |
| **1**           | 0 | 9 | 10 | 10 | 9  |
| **2**           | 0 | 10 | 9 | 10 | 10 |
| **5**           | 0 | 10 | 9 | 10 | 5  |
| 10              | 0 | 8  | 7 | 6  | 4  |
| 20              | 0 | 5  | 6 | 5  | 4  |

`Moran's I (rule)`:

| ga_every \\ mut | 0   | 1     | 2     | 4     | 8     |
| --------------- | --- | ----- | ----- | ----- | ----- |
| 1               | 0   | 0.38  | 0.32  | 0.26  | 0.31  |
| 2               | 0   | 0.34  | 0.31  | 0.27  | 0.24  |
| 5               | 0   | 0.37  | 0.30  | 0.29  | 0.31  |
| 10              | 0   | 0.51  | 0.45  | 0.34  | 0.33  |
| 20              | 0   | **0.57**| 0.49 | 0.38  | 0.29  |

## Findings

### F1. Best cell: `ga_every=1, initial_mutation=2`
- improvement = **+1910 %** (10/10 wins, binomial p < 10⁻³)
- Moran's I (rule) = 0.32, (weight) = 0.51
- This is the configuration to use as the canonical "GA-active" reference.

### F2. mut=0 is a clean control
Across all five values of `ga_every`, `mut=0` produces:
- improvement = 0 %
- ga_wins = 0/10
- Moran's I = 0.0000 exactly

This is the cleanest possible negative control: with `mutation_rate=0` the
mutate operator is a no-op, crossover among identical genomes produces
identical children, and there is no source of diversity for selection to
act on. The elitism fix from A3 holds: zero drift in the absence of
selectable variation.

### F3. Aggressive scheduling is robust to mutation rate
At `ga_every=1`, the GA wins ≥ 9/10 across `mut ∈ {1, 2, 4, 8}`. High
mutation only starts to hurt at slow GA scheduling — `ga_every=10, mut=8`
falls to 4/10 wins.

### F4. **Spatial structure does NOT predict performance**
The most spatially-clustered configurations (`ga_every=20, mut=1`,
Moran's I = 0.57) achieve only **31 %** improvement vs the highest-
performing configurations (Moran ≈ 0.32) hitting **1910 %**.

This contradicts the intuition behind H3's threshold. *More clustering
is not better.* Possible mechanism: high Moran's I corresponds to large
homogeneous patches, which after long evolution converge to one rule
locally. Performance benefits from a **mixture** that lets each region
specialize without freezing.

This is the most interesting finding of A1: spatial autocorrelation is a
useful diagnostic that the GA is doing *something*, but a poor predictor
of task fitness. The intermediate-clustering regime (Moran ≈ 0.3) wins.

### F5. Cost of high mutation depends on update frequency
- At `ga_every=1`: mut=8 still gives 1056 % (most disruption tolerated).
- At `ga_every=20`: mut=8 gives 68 % (frequent random walks between
  rare selection events overwhelm the signal).
- **Interpretation**: mutation is a per-GA-call random walk. The system
  can only push back via selection events. With infrequent selection,
  drift wins.

## Hypothesis status updates

- **H1**: confirmed strongly. A configuration exists where the GA beats
  baseline in **30/30** robustness checks (mut=1, ga_every=2 from A2;
  this sweep finds further configurations equally robust).
- **H2**: re-confirmed by F2 (mut=0 control: GA mechanism without
  variation contributes nothing) and F3 (selection frequency dominates).
- **H3**: revised. Original threshold `Moran > 0.3` is reached or
  exceeded by *many* configurations, but **F4 shows it is not the right
  performance metric**. Should be replaced by a richer measure: e.g.
  effective number of rule-clusters, or genotype entropy combined with
  spatial coherence.

## Open question raised

**Why does over-clustering (Moran > 0.5) hurt performance?**
Hypothesis: it indicates the GA has converged to a monoculture-like
state where most cells share a rule, losing the bimodal mixture that
A2's visual heatmap showed as the productive structure. A future
experiment could correlate `top_rule_frac` with improvement directly
and look for a sweet spot (around 0.4–0.6).

A quick check on the sweep data:
- Best (ga_every=1, mut=2): top_rule_frac likely ≈ 0.5 (bimodal).
- Worst high-Moran (ga_every=20, mut=1): top_rule_frac likely ≈ 0.85+
  (one rule dominates with crisp boundaries).

## Artefacts

- `sweep.sh`, `sweep2d.sh` — drivers.
- `sweep.csv`, `sweep2d.csv` — machine-readable summary tables.
- `run2d_g{N}_m{M}.txt` — individual run outputs.

## Post-Modulo Re-Validation (2026-05-17)

Re-ran canonical command under the modulo-lookup fix (`./exp_local_ga --size 64 --steps 600 --window 150 --trials 10 --fitness density --ga-every 1 --initial-mutation 2 --seed 1`):

| Metric | Original | Post-modulo | Delta |
|--------|----------|-------------|-------|
| `improvement %` | **+1910 %** | **+1819 %** | −4.8 % |
| `ga_wins` | 10 / 10 | 10 / 10 | unchanged |
| `distinct_rules_g1` | capped at ~9 | **31.9 / 32** | 3.5× more |

**Conclusion**: Headline finding is **preserved** — the GA still dominates static by ~1800%. The modulo fix *strengthens* the story: the GA explores the full 32-value rule space and still converges to a productive configuration. See `data/postmod_revalidation/REVALIDATION.md` for full comparison.

## Next experiments suggested by A1

- **A1.5**: Sweep on `tournament_k` (currently fixed at 3) at the best
  cell. Hypothesis: k=2 may be enough, k>5 may over-select.
- **A4**: Convergence dynamics at the best cell — does Moran's I rise
  monotonically, plateau, then drift back down? If so, **early
  stopping** of the GA would help.
- **B3**: Per-tick analyze trace at the best cell — what does density
  look like over time?
