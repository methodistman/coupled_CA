# Brainstorm: Finding Emergent Computational Capacity in Coupled CA

## Core Problem Diagnosis

Our current experiments overwhelmingly produce ash. Random ICs in Life decay to ~3% density within 100–500 steps. Edge coupling between two Life grids injects noise, not structure. We are measuring **thermodynamic** quantities (density, entropy, activity) when we need **information-theoretic** and **structural** quantities.

**The hypothesis**: Computation requires *stable carriers of information* (gliders, oscillators, memory loops) plus *interaction sites* (collisions, gates). Our engine can support both, but our search space and metrics are wrong.

---

## Direction 1: Genetic Algorithm Search for Computational ICs

Instead of random seeds, **evolve initial conditions** that maximize a computational fitness function.

### Fitness Function Ideas

1. **Signal Transmission Fidelity** (`exp_ga_signal.c`)
   - Inject a specific pattern (e.g., glider) at left edge of Grid 0
   - After N steps, measure correlation with expected pattern at right edge of Grid 1
   - Fitness = correlation coefficient
   - Genome = initial condition bits (or payload values) for both grids
   - Use 1-point crossover + bit-flip mutation on the IC

2. **Memory Capacity** (`exp_ga_memory.c`)
   - Set a "write" pulse at t=0 on Grid 0 edge
   - Measure whether Grid 1 can still reproduce that pulse at t=N
   - Fitness = mutual information between initial pulse and later state
   - This finds configurations that *store* information, not just transmit

3. **AND/OR/XOR as Dynamic Patterns** (Phase D: `exp_binary_logic_ga`, `exp_payload_logic_ga`, `exp_wave_logic_ga`, `exp_hybrid_logic`)
   - 2-grid setup: grid 1 = input buffer (left half = A, right half = B); grid 0 = computation grid with per-cell genomes
   - Coupling: grid 1 right edge → grid 0 left edge
   - Fitness = fraction of correct truth-table outputs on grid 0's right edge after N steps
   - Population-level GA with tournament selection, crossover, and mutation on per-cell genomes

### Implementation Sketch

```c
typedef struct {
    uint8_t *bits;   /* flattened grid cells */
    int size;
    int n_grids;
    double fitness;
} Genome;

void genome_evaluate(Genome *g, System *sys) {
    sys_load_from_genome(sys, g);
    double score = 0;
    for (int trial = 0; trial < 4; trial++) {
        inject_pattern(sys, trial);
        for (int s = 0; s < STEPS; s++) sys_step(sys);
        score += correlation(expected[trial], readout(sys));
    }
    g->fitness = score / 4.0;
}
```

**Key insight**: We don't need the rules to change. We need the **initial conditions and coupling topology** to be optimized. Life already contains gliders and gates — we just need to arrange the IC so they collide usefully.

---

## Direction 2: Fractional-Scale (Multi-Resolution) Couplings ✅ Implemented

Current limitation: all coupled grids must be identical size. This forces redundant computation.

### Concept: Zoom Coupling

Grid A (64×64, binary Life) ↔ Grid B (32×32, payload)

When A reads from B's edge at position x, it reads from B's cell `x/2` (with optional interpolation).
When B reads from A's edge at position x, it reads the **average** of A's cells `[2x, 2x+1]`.

This creates a natural hierarchical processing pipeline:
- B acts as a **feature extractor** (sees coarse-grained patterns from A)
- A acts as a **detail processor** (sees fine-grained patterns)
- Cross-talk at edges allows B to modulate A's boundary conditions based on global state

### Geometric Variants

1. **Downsampling coupling**: 2×2 block → 1 cell (mean, max, or threshold)
2. **Upsampling coupling**: 1 cell → 2×2 block (broadcast)
3. **Stripe coupling**: couple every Nth cell (sparse communication)
4. **Fractal coupling**: Grid sizes are powers of 2 apart; recursive coupling

### Code Hook

In `coupling_read` or `hybrid_coupling_read`, add a `scale_factor` to the `EdgeConn` struct:

```c
typedef struct {
    int target_grid;
    int target_edge;
    float scale;     /* 0.5 = half size, 2.0 = double size */
    int mode;        /* 0=nearest, 1=mean, 2=max */
} EdgeConn;
```

---

## Direction 3: Geometric Transformations — Break the Square

### Triangular Lattice (6 neighbors)

- Cell shape: equilateral triangle
- Neighbors: 6 instead of 8
- Natural rules: triangular Life variants exist (e.g., "TriLife" with B3/S3-S5)
- Coupling square↔triangular: at the edge, map 2 triangles to 1 square cell or vice versa

### Hexagonal Lattice (6 neighbors)

- Each cell is a hexagon
- 6 neighbors arranged in a ring
- Rules: hex Life (B2/S2 for interesting behavior)
- Better for isotropic propagation than square grids

### Rotated / Twisted Coupling

Instead of edge-to-edge with aligned coordinates, apply a transformation:

- **90° rotation**: A's bottom edge (x, H) → B's left edge (0, H-x)
- **Reflection**: A's right edge → B's left edge reversed
- **Shear**: A's top edge → B's top edge with offset proportional to x
- **Arbitrary affine**: `x' = ax + by + c` modulo grid size

This breaks translational symmetry and creates **circulatory flows**. A glider leaving A's right edge might re-enter A's bottom edge after traversing B, creating a loop.

### Code Hook

Add a transform matrix to `EdgeConn`:

```c
typedef struct {
    int target_grid;
    int target_edge;
    int rotate;       /* 0, 90, 180, 270 degrees */
    int reflect;      /* 0=none, 1=horizontal, 2=vertical */
    int offset;       /* constant offset along edge */
} EdgeConn;
```

---

## Direction 4: Better Metrics — Detect Structure, Not Noise ✅ Partially Implemented

### Information Flow Metrics

1. **Transfer Entropy** between grid A and grid B
   - Does knowing A's past reduce uncertainty about B's future?
   - High transfer entropy = causal influence = potential computation channel

2. **Integrated Information (Φ)**
   - Minimal information loss when splitting the system
   - High Φ = the system is more than the sum of its parts

3. **Effective Measure Complexity**
   - How much of the past is needed to predict the future?
   - High EMC = memory-rich system

### Structural Metrics

1. **Connected Components**
   - After each step, label connected alive regions
   - Track birth/death/merge/split events
   - Persistent components = memory structures

2. **Glider Detection**
   - Template match for known spaceships (glider, LWSS, etc.)
   - Track trajectories over time
   - Count collisions per unit time

3. **Oscillator Census**
   - Run pattern for 20 steps, check if it returns to initial state
   - Classify by period and population
   - Common oscillators (blinkers, beacons) are noise; rare ones are interesting

4. **Pattern Velocity Field**
   - Optical flow between consecutive frames
   - Regions with coherent velocity = organized information transport

### Logical Metrics

1. **Reversible Logic Test**
   - Run system forward N steps, then backward N steps using inverse rule
   - If state differs, information was lost or computation happened

2. **Functional Dependency**
   - Flip one bit in Grid A IC, observe effect on Grid B after N steps
   - If effect is localized and predictable, there is a functional dependency

---

## Direction 5: Rule Modulation — The Grid as CPU

Instead of fixed rules per grid, let **one grid control another's rule**.

### Binary-Controlled Payload Rule

Grid B (binary) holds a program counter. Its pattern encodes which payload rule Grid P should use:

```c
/* In payload_sys_step, before applying rule: */
int ctrl = coupling_read(&s->coupling, s->grids, g, x, y, 0, -1); /* read from binary above */
PayloadRule *rule = select_rule(ctrl);  /* ctrl=0 -> identity, ctrl=1 -> add, etc. */
rule->fn(nb, &out, &rng);
```

### Payload-Controlled Binary Rule

If payload cell P(x,y) > 128, use Life rule. Otherwise use Seeds rule. The payload grid acts as a **configurable mask** over the binary grid.

### Dynamic Rule Schedules as Programs

Instead of random schedules, encode programs:
- `cycle(identity, add_mod, shift_left)` = a 3-instruction loop
- The payload grid executes a tiny program by cycling through rules
- Binary grid provides control flow (jump when signal arrives)

---

## Direction 6: Embodied / Geometric Computation

### The "Cellular Wire" Experiment

1. Seed a long straight line of alive cells (a wire) in Grid A
2. Couple Grid A's wire endpoint to Grid B's edge
3. Run Life on A. A glider traveling down the wire is a signal.
4. Grid B acts as a processing node — its rule determines what happens when the signal arrives
5. Measure: does the signal arrive at the other end? Is it transformed?

This is the **wire-world** concept but with Life rules and cross-grid coupling.

### The "Logic by Collision" Experiment

1. Seed two gliders on collision course in Grid A
2. Their collision produces debris (or nothing, or a new glider)
3. The debris pattern couples into Grid B
4. Grid B's rule amplifies or suppresses the debris
5. Measure output at B's far edge

This tests whether collisions can be used as logic gates with **external amplification**.

---

## Direction 9: Polar/Anisotropic Coupling & Spatial Programmability (new)

### The Core Idea

Each cell is a circular element with a local polar coordinate system. The "north" faces the cell's interior/center; the "south" faces outward toward neighbors. The grid is a flat Cartesian layout, but each cell has its own radial orientation encoded in its genome.

**Genome encoding** (48-bit `CellGenome`, upgraded from 16-bit):
- bits 0-7:   rule_select (0-255)
- bits 8-15:  coupling_weight (0-255)
- bits 16-23: mutation_rate (0-255)
- bits 24-35: 4 mode orientations (3 bits each: mode0=E/NE/N/NW/W/SW/S/SE, mode1, mode2, mode3)
- bits 36-39: alt_rule_select (alternate rule, 0-15)
- bits 40-43: dir_mask (which cardinal directions trigger alt_rule: N,S,E,W)
- bits 44-47: reserved

**Anisotropic coupling**: weight each neighbor by cos²(φ−θ) where θ is the cell's orientation for the current mode. Precomputed `ALIGNMENT_SCALE[8][8]` lookup avoids trig per cell.

A cell with θ=0° (East) strongly couples to its right neighbor, weakly to its left, and not at all to vertical neighbors. Cells with the same θ self-reinforce into directional "highways."

### Why This Is Different From Existing Work

- **Not grid chaining** (G2). Composition *within* one grid via spatial differentiation.
- **Not rule modulation** (Direction 5). The *same* rule runs everywhere; the coupling geometry changes.
- **Not multi-substrate** (F1–F4). Single substrate with anisotropic topology.

### Spatial Programmability = Multimodal Logic

A single grid can host 4 independent computational modes if cells partition into 4 orientation domains:
- **Mode 0 (horizontal)**: mode0 orientation ≈ E or W
- **Mode 1 (vertical)**: mode1 orientation ≈ N or S
- **Mode 2 (anti-diagonal)**: mode2 orientation ≈ NE or SW
- **Mode 3 (diagonal)**: mode3 orientation ≈ NW or SE

Each mode uses its own orientation field, so a cell can face East for horizontal computation AND North for vertical computation simultaneously.

### Sub-Direction 9a: Hierarchical Evolution (Two-Stage GA)

**Problem**: Evolving 4-mode logic directly fails because the GA optimizes one mode and ignores the others.

**Solution**: Split into two stages:
1. **Stage 1** — Evolve only orientations (ignore logic). Fitness = signal reachability across all 4 traces. Goal: create 4 clean highways.
2. **Stage 2** — Freeze the winning orientation pattern, evolve rules/weights for logic on those highways.

See `.windsurf/workflows/hierarchical_evolution.md`.

### Sub-Direction 9b: Directional Rule Modification

**Problem**: One rule per cell cannot support multiple directional computations.

**Solution**: `dir_mask` + `alt_rule_select` in genome. If a neighbor from a masked cardinal direction (N,S,E,W) is active, the cell switches to its alternate rule for that step. A cell can use "Life" for horizontal signals and "Diamoeba" for vertical signals.

See `.windsurf/workflows/directional_rule_modification.md`.

### Biological Analogy

This is closer to **tissue differentiation** than digital logic:
- Cells know their position (via the flat grid coordinates)
- Cells express a local "type" (via orientation)
- Different types couple preferentially to different neighbors
- The tissue self-organizes into functional regions

### Implementation Status

- ✅ `genome.h/c` — 48-bit macros, mutation, crossover, `genome_pack_full()`
- ✅ `pipeline.c` — anisotropic kernel with `ALIGNMENT_SCALE[8][8]`, mode-specific orientation reading, directional rule switching
- ✅ `test_orientation.c` — statistical verification that orthogonal neighbors are suppressed
- 🟡 `exp_multimodal_polar_logic_ga.c` — runs; DIAG reaches 0.34 fitness, HORIZ/VERT/ADIAG stuck at 0.125
- 🟡 `exp_hierarchical.c` — built. Stage 1 fitness is flat zero: confirmed root cause is the hardcoded `RULE_TABLE` (no rule propagates thin signals under anisotropic gating). See `COMPOSABLE_RULESET_ROADMAP.md`.
- 🟡 `exp_directional_multimodal.c` — 3333 gens completed. Peak 0.875, mean 0.726. XOR in individual modes 22-48% of generations but all-4-XOR never achieved. See `data/h8_FINDINGS.md`.

### Sub-Direction 9c: Composable 6-Layer Ruleset (NEW — top priority)

**Problem**: Both H7 (hierarchical) and H8 (flat search) hit the same wall — the 9 hardcoded Conway-family rules in `RULE_TABLE` cannot propagate thin signals under anisotropic gating, so Stage-1 reachability fitness is flat-zero and Stage-2 logic GA can't find substrates that simultaneously route and compute.

**Approach**: parameterize the cellular automaton itself as a **stack of 6 atomic operators** with one 8-bit parameter each + 1 meta byte = **56 bits of ruleset DNA per grid**. Layers: birth-bitmask, survive-bitmask, anisotropy strength, cardinal/diagonal bias, refractory ticks, noise. Evolved by a new Stage 0 of the hierarchical GA before Stages 1 and 2 run.

Full plan: `COMPOSABLE_RULESET_ROADMAP.md`.

### Open Questions

- Will hierarchical evolution (Stage 0: ruleset, Stage 1: highways, Stage 2: logic) outperform direct 4-mode evolution?
- Does directional rule modification allow a single cell to participate in incompatible directional computations?
- Will the grid fragment into disconnected regions, or will highways percolate across the grid?
- Can the 6-layer composable ruleset discover propagation rules outside the Conway family (e.g., refractory waves) that the hardcoded table cannot express?

---

## Direction 7: Search in Topology Space, Not Just Rule Space

### Evolving Coupling Topologies

Genome encodes:
- Number of grids (2–8)
- Size of each grid
- Rule for each grid
- Connection graph (which edge of which grid connects to which)
- Coupling strength / scale / transform
- Cross-type connections (hybrid)

Fitness = any of the GA fitness functions above.

### The "Topology GA" (`exp_ga_topology.c`)

```c
typedef struct {
    int n_grids;
    int sizes[MAX_GRIDS];
    char types[MAX_GRIDS];        /* 'B' or 'P' */
    int rules[MAX_GRIDS];
    int n_conns;
    struct { int src_g, src_e, dst_g, dst_e; float strength; } conns[MAX_CONNS];
    int n_xconns;
    struct { int src_g, src_e, dst_g, dst_e; } xconns[MAX_XCONNS];
} TopologyGenome;
```

Mutation operators:
- Add/remove a grid
- Change grid size
- Rewire a connection
- Flip a grid type (B↔P)
- Change rule

This searches the **architecture space** of multi-substrate CAs.

---

## Direction 8: Coarse-Graining and Renormalization

### The Renormalization Experiment

1. Run Life on a 256×256 grid for 1000 steps
2. Every 100 steps, **coarse-grain** the grid to 128×128, then 64×64, etc.
3. At each scale, measure whether the coarse-grained pattern is still computationally interesting
4. Look for **scale-invariant structures** (fractal-like patterns that persist across zoom levels)

This connects to physics: if computation is emergent, it should survive coarse-graining like critical phenomena.

---

## Immediate Next Steps (Ranked by Feasibility)

**Implemented:**
- ~~Implement GA for IC evolution~~ (`exp_ga_ic.c`) — Done
- ~~Implement fractional coupling~~ (`coupling.c`) — Done (4 scale modes)
- ~~Implement glider/oscillator census~~ (`patterns.c`) — Done (strict mode: 2-cell dead border eliminates ~99% false positives)
- ~~Add strict census / no-bonus flags~~ — Done (`--strict-census`, `--no-census-bonus`)

**Key Finding:** Census-based fitness created artifactual selection pressure. On 64×64, default census found ~466 gliders in random soup; strict mode found 0. Long-run GA (30 gen) with strict census flatlined at ~0.64 fitness vs 1.13 with loose census. The base signal metric (all-1s correlation + energy) is too weak to drive evolution.

**Remaining (re-prioritized):**
1. **Redesign fitness function** — spatiotemporal pattern match instead of all-1s correlation. **Largely addressed** by `density` fitness (A2: +1437% improvement); still open for *task-specific* (e.g. delayed-XOR) fitness shaping.
2. **~~Add rule modulation experiment~~** (`exp_rule_mod.c`) — closes H11/H12. **Done** — +43.5% improvement over static rule (`data/c3/FINDINGS.md`).
3. ~~**Add transfer entropy metric**~~ — Done. See `metrics/transfer_entropy.{h,c}` and `tests/test_transfer_entropy.c`.
4. **TerritoryLang integration** — Phases 1–3 done; Phase 4 not started. See `territorylang_integration/00_index.md`.
5. **Add rotated coupling** (90° edge mapping) — still open.
6. **Triangular grid support** — larger refactor; still open.

---

## Hypothesis Revisions

| Old Hypothesis | Why It Failed | Revised Hypothesis |
|---------------|---------------|-------------------|
| H1: Binary coupling creates logic gates | Searches random ICs; gates need specific collision setups | H1': GA-evolved ICs produce collision-based logic with cross-grid amplification |
| H3: Payload survives on moving patterns | Glider dies on small grid; needs larger space or boundaries | H3': Payload survives on *self-sustaining* structures (puffers, rakes) in unbounded grids |
| H4: Hybrid creates conditional dataflow | Edge transfer is too weak; needs active amplification | H4': Hybrid dataflow emerges when one substrate *modulates* the other's rule, not just edge state |
| H10: GA-evolved ICs produce persistent structure | Census bonus rewarded accidental subpattern matches, not real structure; base signal metric too weak | H10': Spatiotemporal pattern-match fitness (e.g., expected glider wave at target edge) can drive genuine IC evolution |
| H9: Reservoir distills rule glossary | Readout features are trivial (mean/variance) | H9': Reservoir readout must be *trained* on a task; report task accuracy, not trajectory statistics |

---

## Key Insight

> We are not finding computation because we are looking for **stable structures in random soup**. Computation in CA is not emergent from randomness — it is emergent from **carefully arranged initial conditions** that exploit the rule's native dynamics. The engine is fine. The search strategy needs to be **directed** (GA, gradient descent) rather than **random sampling**.
