# Further Hypotheses

Companion to `further_experiments.md` and `BRAINSTORM.md`. Lists the
falsifiable claims that the next round of experiments is designed to
test. Each entry is structured for honest science: a hypothesis, the
prediction it makes, and what observation would falsify it.

The goal is **not** to confirm beliefs; it is to identify which of the
following claims survive contact with data and which need revision.

---

## H1. Per-cell GA can outperform static-rule baselines on signal tasks

**Claim**: There exists a configuration of `pipeline_preset_genomic` such
that, on a coupled-grid signal-transmission task, mean target-edge signal
is reliably higher than `pipeline_preset_intent` with `rule=0` everywhere.

**Prediction**: experiment **A1** finds at least one
`(mutation_rate, ga_every, tournament_k, initial_weight, fitness_mode)`
cell with mean improvement ≥ 50% over 30 seeds, with `ga_wins ≥ 20/30`.

**Falsified if**: across all 144 grid cells × 30 seeds, no configuration
achieves both bars. Implication: the per-cell genome encoding
(4 bits rule + 4 bits weight + 4 bits mutation) is too coarse, or
tournament selection on local Moore neighbours is the wrong selection
scheme.

**Stakes**: If H1 fails, Phase 3's central design assumption is wrong
and the per-cell-genome direction needs rethinking before C/E experiments
proceed.

---

## H2. Fitness signal, not GA mechanism, is the bottleneck

**Claim**: The current GA mechanism (uniform crossover, tournament k=3,
Moore neighbourhood) is adequate; the dominant reason `exp_local_ga`
sometimes loses to baseline is that `FITNESS_EDGE_SIGNAL` is too sparse
to provide useful gradient.

**Prediction**: experiment **A2** finds that switching to
`FITNESS_DENSITY` or `FITNESS_ACTIVITY` improves median GA performance
by ≥ 30% at the *same* GA hyperparameters.

**Falsified if**: all fitness modes perform within 10% of each other.
Implication: the GA machinery itself (selection, crossover, replacement
rule) is the limit, not the fitness function.

**Stakes**: distinguishes "shape the reward" from "rebuild the optimizer"
work next.

---

## H3. Evolved genomes contain visible spatial structure

**Claim**: After local-GA evolution, `rule_select` and/or
`coupling_weight` fields exhibit spatial autocorrelation distinct from
random initialization — the GA produces *patterns*, not noise.

**Prediction**: experiment **A3** finds Moran's I > 0.3 on at least one
genome field after evolution, vs ≈ 0 for unevolved random genomes.

**Falsified if**: evolved Moran's I indistinguishable from initial.
Implication: the GA is either failing to converge or is producing
solutions that look statistically identical to random — both suggest
the selection signal isn't propagating spatially as designed.

**Stakes**: cheap visual confirmation that the GA is doing *something*
spatially coherent.

---

## H4. The local GA converges (rather than collapses or oscillates)

**Claim**: Given enough ticks, fitness mean rises and variance falls;
the GA reaches a quasi-stationary distribution over genomes.

**Prediction**: experiment **A4** classifies the dominant regime as
`converged` for at least 50% of seeds at default parameters.

**Falsified if**:
- `extinct` dominates → fitness pressure is killing the population.
- `oscillating` dominates → no fixed point, possibly intransitive
  fitness or rule arms-race.
- `chaotic` (high variance throughout) → GA isn't selecting.

**Stakes**: tells us whether to add elitism / diversity preservation
(if extinct) or to interpret runs differently (if oscillating).

---

## H5. Most of the GA's gain comes from a *static* learned configuration

**Claim**: Once the GA has run, the resulting genome pattern is the
artifact of value; ongoing GA activity contributes little.

**Prediction**: experiment **A5** finds variant (a) "frozen learned
genomes + no GA" within 10% of variant (b) "GA continues", and both
substantially above (c) "fresh random + no GA".

**Falsified if**: (b) ≫ (a). Implication: the GA's value is in continuous
*adaptation* to the current dynamic state, not in finding a fixed map.
That would make Phase 3 a *control* layer, not a *design* layer.

**Stakes**: changes the framing of what local GA is *for*.

---

## H6. Inter-grid coupling has a non-trivial optimal delay

**Claim**: For information transmission, the best inter-grid coupling
delay is not zero. Some level of "memory" in the intent ring is needed
to coordinate transmission with the receiver's local dynamics.

**Prediction**: experiment **B1** finds a single peak in TE-vs-delay at
`delay > 0`.

**Falsified if**: TE monotonically decreases with delay (zero is always
best). Implication: the intent-ring infrastructure (Phase 1) provides no
advantage beyond simple buffering.

**Stakes**: validates or undermines a core Phase 1 design choice.

---

## H7. Coupled-CA dynamics partition into temporally distinct regimes

**Claim**: A long run on random ICs passes through identifiable phases
(e.g., chaotic decay → quiescence with sparse oscillators → equilibrium)
that can be detected automatically from per-tick metrics.

**Prediction**: experiment **B3** identifies ≥ 3 distinct regimes via
change-point analysis on density + entropy + activity time series.

**Falsified if**: metrics decay monotonically and no regimes are
detectable. Implication: the system is too simple; coupling adds no
qualitative richness.

**Stakes**: motivates whether per-tick metrics are scientifically
informative or merely instrumentation.

---

## H8. Parallel pipeline speeds up at realistic problem sizes

**Claim**: The thread pool produces meaningful speedup
(≥ 1.5× over `PIPELINE_INTENT`) once `num_grids ≥ 2` and
`grid_size ≥ 128`.

**Prediction**: experiment **B4** measures speedup ≥ 1.5× in that regime.

**Falsified if**: parallel preset is within ±10% of intent everywhere,
or *slower* for small grids. Implication: thread-pool overhead dominates
the workload at these sizes; the parallel path is only useful for
larger problems than the codebase currently supports.

**Stakes**: practical — should we keep the parallel preset or document
it as research-only?

---

## H9. Coupled-CA can self-organize a sparse coupling topology

**Claim**: Starting with a dense coupling graph and pruning edges by TE
threshold leads to a stable, sparse, non-trivial topology that depends
on the rule.

**Prediction**: experiment **C1** observes that surviving edge count
plateaus above zero and below max; surviving topologies differ
qualitatively between rule pairs (Life-Life vs Life-HighLife vs
HighLife-HighLife).

**Falsified if**: prune-to-zero or prune-to-full are the only outcomes
(no intermediate equilibrium), or the surviving topology is rule-
independent. Implication: TE-based pruning is too crude a control law.

**Stakes**: tests whether the system supports *self-organized*
architecture, a precursor to TerritoryLang's "specialized regions"
vision.

---

## H10. Coupled-CA functions as a usable reservoir

**Claim**: A 64×64×2 coupled-grid system has memory capacity > 5 ticks
and can solve linear delay tasks at NRMSE < 0.1.

**Prediction**: experiment **C2** measures both.

**Falsified if**: memory capacity ≤ 1 tick or NRMSE > 0.3 on the
simplest task. Implication: this CA stack is interesting as a *model*
but not as a *computer*.

**Stakes**: determines whether to pursue the framework for external
applications or treat it as a pure research toy.

---

## H11. Explicit rule modulation (H12) outperforms evolved modulation

**Claim**: Hand-designed rule maps — e.g. "if coupled grid 0 cell is
alive, use HighLife on grid 1 else use Life" — outperform GA-discovered
maps at the same signal-transmission task, *with the same compute budget
that would otherwise be spent on the GA*.

**Prediction**: experiment **C3** shows a clear (Life/HighLife) modulation
configuration that yields higher mean signal than the best A1 GA
configuration after equal wall time.

**Falsified if**: GA-discovered maps consistently match or beat
hand-designed maps. Implication: search is better than design for this
parameter; investing further in GA pays.

**Stakes**: this is the core TerritoryLang H12 claim. If H11 is true,
the per-cell genome is a *user-controlled* layer, not an *evolved*
layer; the framework's selling point shifts toward declarative control.

---

## H12. Intent modes meaningfully change emergent dynamics

**Claim**: The four `IntentMode` values produce statistically different
glider/TE trajectories under fixed IC + rule + topology.

**Prediction**: experiment **B2** finds pairwise mean differences
significant at p < 0.01 in glider count or TE.

**Falsified if**: modes are statistically indistinguishable.
Implication: `IntentMode` is bookkeeping with no behavioural
consequence; simplify the API and document only one mode as canonical.

**Stakes**: API surface decision.

---

## H13. Evolved coupling topology beats random matched-density topology

**Claim**: Topology evolution (E1) finds non-random structure that
outperforms a topology with the same edge density but random placement.

**Prediction**: evolved-vs-random comparison shows higher TE for evolved.

**Falsified if**: evolved is within noise of random. Implication: at
these scales, *which* edges exist doesn't matter — only *how many*.

**Stakes**: distinguishes a system with structural sensitivity from
one where coupling is a scalar parameter.

---

## H14. Two-timescale evolution > either timescale alone

**Claim**: Combining slow IC GA with fast local GA outperforms each
individually.

**Prediction**: experiment **E2** shows two-timescale fitness ≥ 1.5×
either alone at matched compute budget.

**Falsified if**: addition of inner local GA does not improve outer GA
fitness, or hurts it (inner GA destabilizes evaluations). Implication:
the two layers are not composable; redesign.

**Stakes**: validates a hierarchical-evolution thesis that originates in
the TerritoryLang docs.

---

## H15. The current C API is declaratively expressible

**Claim**: All current experiments can be described as a JSON/YAML
specification mapping cleanly to `(System, Coupling, Pipeline, genomes)`.

**Prediction**: experiment **E3** reproduces ≥ 3 existing experiments
byte-for-byte from JSON specs.

**Falsified if**: more than one experiment requires bespoke C glue or a
configuration field with no clear declarative form. Implication: the
Phase 4 DSL needs a richer surface than just a config loader; partial
imperative escape hatches must be designed in.

**Stakes**: gates the design of Phase 4. If H15 is true, Phase 4 is a
parser. If false, Phase 4 must include a small embedded language.

---

## Cross-cutting meta-hypothesis

**M1. "Computation requires stable carriers"** (from `BRAINSTORM.md`)
remains the operating belief. Most of A–C above are stress-tests:
H1/H3/H5 ask whether per-cell evolution *finds* such carriers; H6/H12
ask whether the coupling layer *preserves* them; H10/H11 ask whether
they *can be put to work*. If A1, A3, A4, A5, C2 all fail, M1 itself
may need revision: perhaps computation in this substrate is mediated by
*ensemble statistics* rather than localized carriers.

---

## What we won't test (yet)

- Anything requiring grids > 128² — the current `MAX_GRID_SIZE` blocks
  this and lifting it is a separate task.
- GPU acceleration — out of scope for the current codebase.
- Continuous (non-boolean) cell states beyond the existing
  `PayloadGrid` — a third layer would require its own Phase.
- Biological/territorial interpretation — these are computational
  hypotheses about the CA substrate, not claims about ecology or
  geography.
