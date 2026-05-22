# Roadmap: Evolving Genomes for Logic Operations (AND / OR / XOR / NAND)

**Status**: Draft — based on codebase analysis as of 2026-05-16
**Scope**: Extend per-cell genome evolution from binary-only to all three grid types, then build logic-gate experiments.
**Estimated effort**: 3–5 sessions (~800–1200 LOC + tests)

---

## 0. Current State Snapshot

| Grid Type | Per-Cell Genomes | Step Kernel Reads Genome | Per-Cell Rule Select | Local GA |
|---|---|---|---|---|
| Binary (`Grid`) | Yes | **Yes** (`grid_step_kernel`) | **Yes** (`GENOME_RULE_SELECT`) | Yes |
| Payload (`PayloadGrid`) | Yes | **No** (`payload_phase_run` ignores them) | **No** | Yes |
| Wave (`WaveGrid`) | Yes (`genomes`, `nb_genome`) | **No** (`wave_sys_step` ignores them) | **No** | Partial |

**Critical blocker**: Only the binary engine's step function checks `g->genomes` for per-cell rule selection. Payload and wave engines always use `grid->rule_idx` globally.

**Ruleset headroom** (4-bit `rule_select` = 0–15):
- Binary: 9 rules (7 slots free)
- Payload: 14 rules (2 slots free)
- Wave: 3 rules (13 slots free)

Any experiment that adds new rules risks exhausting the 4-bit cap.

---

## 1. Phase A — Expand Genome Rule Select to 5 Bits

**Goal**: Support up to 32 rules per table without increasing memory per cell.
**Risk**: Low. Touch-count is small; all callers use macros.

### 1.1 Files to modify

| File | Lines | Change |
|---|---|---|
| `ca_core/genome.h` | 6–28 | Redefine bit layout: `[rule:5][weight:4][mutation:4][reserved:3]` |
| `ca_core/genome.h` | 16–19 | Update `GENOME_RULE_SELECT`, `GENOME_SET_RULE_SELECT` masks from `0x0F` / `~0x0F` to `0x1F` / `~0x1F` |
| `ca_core/genome.h` | 26 | Update `GENOME_MAX_RULE` from 15 to 31 |
| `ca_core/genome.c` | 21 | Mutate wrap mask `& 0x0F` → `& 0x1F` |
| `ca_core/genome.c` | 39 | `genome_random` mask stays `0xFFFF` (no change needed) |
| `ca_core/pipeline.c` | 100 | `grid_step_kernel` `ri` bounds check: `ri < NUM_RULES` already safe, but verify `GENOME_RULE_SELECT` macro |
| `ca_core/payload_pipeline.c` | 102 | Same — future payload genomic step will use expanded macro |
| `ca_core/wave_engine.c` | 64 | Same — future wave genomic step |
| `tests/test_genome.c` | — | Add tests for rule_select = 31, wrap-around, pack/unpack |

### 1.2 Proposed new layout

```c
/* 16-bit per-cell genome, stored as uint16_t.
   Bit layout (from LSB):
     bits 0-4:  rule_select  (0-31, indexes into rule table)
     bits 5-8:  coupling_weight (0-15, scaled to [0,1] by /15.0f)
     bits 9-12: mutation_rate (0-15, scaled to [0,1] by /15.0f)
     bits 13-15: reserved (3 bits)
*/

#define GENOME_RULE_SELECT(g)     ((g) & 0x1F)
#define GENOME_COUPLING_WEIGHT(g) (((g) >> 5) & 0x0F)
#define GENOME_MUTATION_RATE(g)   (((g) >> 9) & 0x0F)
#define GENOME_RESERVED(g)        (((g) >> 13) & 0x07)

#define GENOME_SET_RULE_SELECT(g, v)     ((g) = ((g) & ~0x1F) | ((v) & 0x1F))
#define GENOME_SET_COUPLING_WEIGHT(g, v) ((g) = ((g) & ~0x1E0) | (((v) & 0x0F) << 5))
#define GENOME_SET_MUTATION_RATE(g, v)   ((g) = ((g) & ~0x1E00) | (((v) & 0x0F) << 9))

#define GENOME_DEFAULT 0x0000  /* rule 0, weight 0, mutation 0 */
#define GENOME_MAX_RULE 31
#define GENOME_MAX_WEIGHT 15
#define GENOME_MAX_MUTATION 15
```

### 1.3 Tests

- `test_genome_pack`: verify `genome_pack(31, 15, 15)` sets all active bits.
- `test_genome_mutate_rule`: mutate from rule 0 → should reach rule 31 and wrap 31+1→0.
- `test_genome_crossover`: 1-point crossover at bit 5 should cleanly split rule_select from weight.

---

## 2. Phase B — Add Per-Cell Genomic Rule Selection to Payload Engine

**Goal**: `payload_phase_run` checks each cell's genome and dispatches to the rule indexed by `GENOME_RULE_SELECT`.
**Risk**: Medium. Payload rules expect a `Cell nb[9]` neighborhood. The genomic dispatch must happen per-cell, not per-grid.

### 2.1 Implementation

In `ca_core/payload_pipeline.c`, create a new phase function:

```c
/* Genomic variant: per-cell rule select from genome, with fallback to grid->rule_idx */
void payload_phase_run_genomic(PayloadSystem *sys, const PayloadPhase *phase, void *userdata) {
    (void)userdata;
    if (!sys || !phase) return;
    int do_swap = (phase->target_grid == -2) ? 0 : 1;

    for (int g = 0; g < sys->num_grids; g++) {
        if (phase->target_grid >= 0 && phase->target_grid != g) continue;
        PayloadGrid *grid = sys->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        int default_rule_idx = grid->rule_idx;
        if (default_rule_idx < 0 || default_rule_idx >= NUM_PAYLOAD_RULES) continue;
        const PayloadRule *default_rule = &PAYLOAD_RULE_TABLE[default_rule_idx];
        int has_genomes = (grid->genomes != NULL);
        int sz = grid->size;

        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                Cell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = payload_coupling_read(&sys->coupling, sys->grids, g, x, y, dx, dy);
                    }
                }
                const PayloadRule *rule = default_rule;
                if (has_genomes) {
                    int ri = (int)GENOME_RULE_SELECT(grid->genomes[y * sz + x]);
                    if (ri >= 0 && ri < NUM_PAYLOAD_RULES)
                        rule = &PAYLOAD_RULE_TABLE[ri];
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
        if (do_swap) {
            memcpy(grid->cells, grid->next_cells, sz * sz * sizeof(Cell));
        }
    }
}
```

### 2.2 Files to modify

| File | Change |
|---|---|
| `ca_core/payload_pipeline.h` | Declare `payload_phase_run_genomic` |
| `ca_core/payload_pipeline.c` | Implement `payload_phase_run_genomic` (see above) |
| `ca_core/payload_local_ga.h` | Add `payload_phase_run_genomic` to a new preset: `payload_pipeline_preset_genomic()` |

### 2.3 New rules to add (fills the expanded 5-bit space)

Payload already has `and_payload`, `or_payload`, `xor_payload`. Add:
- `rule_nand_payload` — bitwise NAND on neighborhood payload bytes
- `rule_nor_payload` — bitwise NOR
- `rule_xnor_payload` — bitwise XNOR
- `rule_majority_payload` — payload = majority vote of neighbor payloads
- `rule_parity_payload` — payload = parity (XOR-all) of neighbor payloads
- `rule_multiply_mod` — payload = (a * b) mod 256, where a,b are payload values from opposite neighbors

### 2.4 Tests

- `test_payload_genomic_step`: create a 2×2 payload grid with genomes pointing to `rule_xor_payload`, verify output payload equals XOR of neighbors.
- `test_payload_genomic_preset`: run `payload_pipeline_preset_genomic()` for 10 ticks, ensure no crash.

---

## 3. Phase C — Add Per-Cell Genomic Rule Selection to Wave Engine

**Goal**: `wave_sys_step` checks each cell's genome before dispatching to `WAVE_RULE_TABLE`.
**Risk**: Low-medium. Wave engine is simpler than payload; the pattern is identical.

### 3.1 Implementation

In `ca_core/wave_engine.c`, create `wave_sys_step_genomic`:

```c
void wave_sys_step_genomic(WaveSystem *s) {
    if (!s || s->num_grids < 1) return;
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        int default_rule_idx = grid->rule_idx;
        if (default_rule_idx < 0 || default_rule_idx >= NUM_WAVE_RULES) continue;
        const WaveRule *default_rule = &WAVE_RULE_TABLE[default_rule_idx];
        int has_genomes = (grid->genomes != NULL);
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                WaveCell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = wave_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                const WaveRule *rule = default_rule;
                if (has_genomes) {
                    int ri = (int)GENOME_RULE_SELECT(grid->genomes[y * sz + x]);
                    if (ri >= 0 && ri < NUM_WAVE_RULES)
                        rule = &WAVE_RULE_TABLE[ri];
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }
    /* swap buffers */
    for (int g = 0; g < s->num_grids; g++) {
        WaveGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(WaveCell));
    }
    s->tick_counter++;
    if (s->nb_genome_interval > 0 && s->tick_counter % s->nb_genome_interval == 0) {
        wave_sys_slow_layer(s);
    }
}
```

### 3.2 New wave rules to add

- `wave_rule_vector_and` — `out.wave = (nb[4].wave & neighbor_majority_wave(nb))`
- `wave_rule_vector_or` — `out.wave = (nb[4].wave | neighbor_majority_wave(nb))`
- `wave_rule_vector_xor` — `out.wave = (nb[4].wave ^ neighbor_majority_wave(nb))`
- `wave_rule_threshold_and` — alive if wave value > threshold AND neighbor wave > threshold

### 3.3 Files to modify

| File | Change |
|---|---|
| `ca_core/wave_engine.h` | Declare `wave_sys_step_genomic` |
| `ca_core/wave_engine.c` | Implement `wave_sys_step_genomic` |
| `ca_core/wave_rules.h` | Add new rule function declarations |
| `ca_core/wave_rules.c` | Implement new rules |

---

## 4. Phase D — Experiments

### D1. `exp_binary_logic_ga` — Evolve Binary Spatial Logic Gates

**Prerequisites**: Phase A complete.

**Concept**: Evolve per-cell `rule_select` and `coupling_weight` so that a 2-grid system computes a target truth table.

**Setup**:
- Grid 0 (control/wiring): fixed rule, evolves `coupling_weight` to route signals.
- Grid 1 (computation): evolves `rule_select` per cell. Left half = input A, right half = input B.
- Read output from a target region (e.g., center-right quadrant) after `steps` ticks.

**Fitness function** (for target gate `G`):
```
for each (a,b) in {(0,0),(0,1),(1,0),(1,1)}:
    set left/right halves of grid 1 to a/b
    run pipeline for steps ticks
    observed = majority vote of target region
    error += |observed − G(a,b)|
fitness = 1.0 − error / 4.0
```

**Genome encoding**:
- Grid 0: only `coupling_weight` evolves (rule_select = 0, fixed).
- Grid 1: `rule_select` evolves (0–8 for binary rules), `coupling_weight` evolves.

**Pipeline**: `pipeline_preset_genomic` with `FITNESS_EDGE_SIGNAL` or a new `FITNESS_LOGIC_GATE` mode.

**Output CSV columns**:
`seed,target_gate,best_fitness,avg_fitness,steps,mod_steps,mod_window`

**Success criterion**: Best fitness > 0.75 for at least one gate (AND, OR, XOR) on a 16×16 grid.

---

### D2. `exp_payload_logic_ga` — Evolve Payload Bitwise Logic

**Prerequisites**: Phase A + Phase B complete.

**Concept**: Each cell independently selects a payload rule (AND, OR, XOR, etc.). Evolve the spatial map of which rule goes where.

**Setup**:
- Single payload grid, size 16×16.
- Left edge cells set to payload = 0 or 255 (boolean input A).
- Top edge cells set to payload = 0 or 255 (boolean input B).
- After N steps, read right edge. Average payload > 127 → output 1, else 0.

**Fitness**: Same truth-table error as D1, but payload values are the signal.

**Genome encoding**:
- `rule_select`: 0–13 (payload rules). Key rules:
  - `and_payload` (index TBD)
  - `or_payload`
  - `xor_payload`
  - `identity` (pass-through)
  - `diffuse` (smoothing — useful for wire-like regions)

**Unique feature**: Because payload rules operate on 8-bit values, the "output" is not just 0/1 — it's a graded 0–255 value. Fitness can use the full scalar: `error = |target_payload − observed_payload| / 255.0`.

**Output CSV**: Same schema as D1, plus `avg_payload_output`.

**Success criterion**: Best fitness > 0.80 on a 16×16 grid. Higher than D1 because payload rules are *designed* for logic, not emergent.

---

### D3. `exp_wave_logic_ga` — Evolve Acoustic Vector Logic

**Prerequisites**: Phase A + Phase C complete.

**Concept**: Use wave payload bits as parallel registers. Evolve which wave rule each cell uses.

**Setup**:
- Single wave grid, size 16×16.
- Drive `wave` bits 0–7 (X-coordinate) on left edge with input A (0 or 255).
- Drive `wave` bits 8–15 (Y-coordinate) on top edge with input B.
- After N steps, read `wave` bits 16–23 (Z-coordinate) from center region.

**Genome encoding**:
- `rule_select`: 0–7 (wave rules including new vector AND/OR/XOR).
- `nb_genome`: evolves slowly; acts as a "species boundary" — patches of similar `nb_genome` encourage cells to specialize together.

**Fitness**:
```
for each truth-table row:
    target_z = (a OP b) ? 255 : 0
    observed_z = avg(center_region_wave >> 16) & 0xFF
    error += |target_z − observed_z| / 255.0
fitness = 1.0 − error / 4.0
```

**Output CSV**: Same schema, plus `morans_i` (spatial structure of genomes).

**Success criterion**: Best fitness > 0.70. Lower bar than D2 because wave rules are less mature.

---

### D4. `exp_hybrid_logic` — Binary Control + Payload Computation

**Prerequisites**: Phase A + Phase B complete. Builds on H12 (`phase_modulate_rule`).

**Concept**: Separate control plane (binary) from data plane (payload). Grid 0's spatial state modulates grid 1's payload rule per cell.

**Setup**:
- Grid 0 (binary): evolves per-cell genomes to create stable spatial patterns (e.g., a diagonal band of alive cells).
- Grid 1 (payload): left edge = input A, top edge = input B.
- New phase: `phase_modulate_payload_rule` — for each cell in grid 1:
  ```c
  int ctrl = grid0->cells[idx]; /* 0 or 1 */
  grid1->genomes[idx].rule_select = ctrl ? RULE_AND_PAYLOAD : RULE_XOR_PAYLOAD;
  ```
- The control pattern from grid 0 effectively "programs" which gate each region of grid 1 performs.

**Evolution strategy**:
- Grid 0 genomes evolve via local GA (`FITNESS_STRUCTURE` — reward stable oscillators).
- Grid 1 genomes are NOT evolved; they are overwritten every tick by the modulation phase.
- Fitness is computed on grid 1's output: `error = |target − observed|`.

**Why this is interesting**:
- Grid 0 does not compute directly — it computes **which** computation grid 1 performs.
- This is the CA equivalent of a programmable logic array (PLA): a control matrix selects which operation each cell applies.

**Output CSV**: `seed,target_gate,best_fitness,control_grid_morans_i`

**Success criterion**: Best fitness > 0.70. The challenge is evolving grid 0 to produce spatially structured control patterns that map correctly to the target truth table.

---

## 5. Build & Test Checklist

### Phase A (5-bit genome)
- [ ] Update `genome.h` macros and constants
- [ ] Update `genome.c` mutate logic
- [ ] Run `make test_genome` — verify pack/unpack/mutate for values 0–31
- [ ] Run `make test_local_ga` — verify GA still converges
- [ ] Run `make exp_local_ga` smoke test — byte-exact output with old seed

### Phase B (payload genomic step)
- [ ] Implement `payload_phase_run_genomic`
- [ ] Add new payload rules: `nand`, `nor`, `xnor`, `majority`, `parity`
- [ ] Add `payload_pipeline_preset_genomic`
- [ ] Run `make test_payload` — verify non-genomic path still works
- [ ] Run `make exp_payload` smoke test

### Phase C (wave genomic step)
- [ ] Implement `wave_sys_step_genomic`
- [ ] Add new wave rules: `vector_and`, `vector_or`, `vector_xor`
- [ ] Run `make test_wave` — verify non-genomic path

### Phase D (experiments)
- [ ] Implement `exp_binary_logic_ga.c`
- [ ] Implement `exp_payload_logic_ga.c`
- [ ] Implement `exp_wave_logic_ga.c`
- [ ] Implement `exp_hybrid_logic.c`
- [ ] Add all four to `Makefile` `EXPS`
- [ ] Add smoke tests to `run-tests` target
- [ ] Run a 10-seed sweep for each experiment; record best fitness per gate

---

## 6. Open Questions / Risks

| Risk | Mitigation |
|---|---|
| 5-bit genome breaks existing `.pgm` genome dumps (4-bit nibble maps) | Update `exp_local_ga.c` `--dump-genomes` to use 5-bit encoding or cap at 16 for visualization |
| Payload genomic step is ~1.3× slower (per-cell rule lookup + branch) | Use `__builtin_expect` or flag-based dispatch; benchmark with `exp_bench` before/after |
| Wave engine has no `WavePipeline` yet; `wave_sys_step_genomic` is ad-hoc | Create `wave_pipeline.h/c` to match payload/binary patterns (future session) |
| `exp_hybrid_logic` may be too hard to evolve — control plane is indirect | Fallback: evolve both grids jointly with a combined fitness |
| New payload rules may not compose (e.g., `and_payload` after `diffuse` loses signal) | Test rule chains in isolation before GA integration |

---

## 7. Paper Alignment

| Experiment | Paper |
|---|---|
| `exp_binary_logic_ga` | **Paper 4** (Explicit vs Evolved Control) — compares hand-designed H12 modulation to GA-discovered spatial logic maps |
| `exp_payload_logic_ga` | **Paper 3** (Reservoir Computing) — payload grid as a reservoir; genomic rule map as the readout layer |
| `exp_wave_logic_ga` | **Paper 5** (Self-Organizing Topologies) — dual-time-scale genomes create "species" that specialize on different logic tasks |
| `exp_hybrid_logic` | **Paper 4** — strongest evidence that explicit control (hand-designed phase) + evolved payload (GA) outperforms either alone |

---

## Appendix: Quick Reference — Rule Tables After Expansion

### Binary (`RULE_TABLE`) — target 16+ rules
Current: Conway's Life, HighLife, Day & Night, Seeds, Diamoeba, Anneal, Life Without Death, Morley, Rule 110
Add candidates: B36/S23 (HighLife variant), B2/S (replicators), B3/S012345678 (Vote), B3457/S4568 (Coral), B5678/S45678 (Coral variant)

### Payload (`PAYLOAD_RULE_TABLE`) — target 20+ rules
Current: life_payload, diffuse, carry_add, max_flood, threshold, identity, xor_payload, and_payload, or_payload, add_mod, shift_left, compare_max, noisy_life, prob_diffuse
Add: nand_payload, nor_payload, xnor_payload, majority_payload, parity_payload, multiply_mod

### Wave (`WAVE_RULE_TABLE`) — target 8+ rules
Current: life, diffuse, genomic_life
Add: vector_and, vector_or, vector_xor, threshold_and, threshold_or
