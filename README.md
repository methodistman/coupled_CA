# Coupled Cellular Automata Computation Engine

A modular C11 simulation engine for experimenting with multiple coupled cellular automata grids. Supports both binary (Conway's Life, elementary rules) and 8-bit payload (arithmetic, logic) substrates.

## Quick Start

```bash
make                    # Build all experiments and tests
make test               # Run unit tests + smoke tests
make clean              # Remove binaries and object files
```

## Experiments

Each experiment is a standalone CLI binary writing CSV to stdout:

```bash
# Phase 2: Signal transmission time-series
./exp_signal --grids 2 --size 64 --steps 100 --seed 42 --connect 0:bottom->1:top

# Phase 3: Phase-locking and period detection
./exp_sync --grids 2 --size 64 --steps 200 --seed 42

# Phase 4: Cyclic memory / feedback loops
./exp_memory --size 64 --steps 200 --seed 42

# Phase 5: Search for emergent logic gates
./exp_gates --rule0 0 --rule1 1 --size 32 --steps 200 --trials 10

# Phase 5 (batch): Sweep rules × topologies
./exp_sweep --size 16 --steps 50 --trials 10 --seed 42 --rules 0-8 --topos none,uni,bi

# Phase 9: Payload-aware metrics
./exp_payload --grids 2 --size 64 --steps 100 --seed 42 --connect 0:bottom->1:top,1.0

# Phase 10: Hybrid binary+payload bus
./exp_hybrid_bus --size 16 --steps 100 --seed 42 --xconnect 'B0:bottom->P0:top'

# Phase 3: Per-cell genome evolution (local GA)
./exp_local_ga --size 64 --steps 200 --window 50 --ga-every 1 --fitness density --trials 10 --seed 42

# Phase 3: Load evolved genomes for downstream experiments
./exp_local_ga --size 64 --steps 200 --window 50 --ga-every 1 --fitness density --trials 1 --seed 42 --save-genomes evolved.bin
./exp_delay_sweep --size 64 --steps 500 --seeds 5 --load-genomes evolved.bin

# B1: Coupling-delay sweep with transfer entropy
./exp_delay_sweep --size 64 --steps 500 --seeds 10 --max-delay 16 --output delay_sweep.csv

# B4: Parallel scalability benchmark
./exp_bench --output bench.csv

# D2: Glider preservation under coupling
./exp_glider_preservation --size 64 --steps 200 --intent --intent-mode replace --output glider.csv

# H3: Payload survival on moving glider
./exp_hybrid_glider --size 32 --steps 100 --seed 42 --payload 200

# H9: Reservoir + ridge regression readout
./exp_hybrid_reservoir --size 16 --steps 200 --trials 4 --seed 42 --task delay --delay 1

# H12: Explicit rule modulation (hand-designed vs GA)
./exp_rule_mod --size 16 --steps 50 --window 20 --trials 10 --seed 42 --modulate-rules "Conway's Life,HighLife"

# Phase D: Evolve logic gates via per-cell genome GA (binary grid)
# Use --fitness shaped and --out-mode density for best results (post-fix)
./exp_binary_logic_ga --gate and --size 16 --pop 30 --gens 50 --steps 30 --seed 42 --fitness shaped --out-mode density

# Phase D: Evolve logic gates via per-cell genome GA (payload grid)
./exp_payload_logic_ga --gate xor --size 16 --pop 30 --gens 50 --steps 30 --seed 42 --fitness shaped --out-mode density

# Phase D: Evolve logic gates via per-cell genome GA (wave grid)
./exp_wave_logic_ga --gate or --size 16 --pop 30 --gens 50 --steps 30 --seed 42 --fitness shaped --out-mode density

# Phase D: Cross-grid modulation — binary control + payload computation
./exp_hybrid_logic --gate nand --size 16 --pop 30 --gens 50 --steps 30 --seed 42 --fitness shaped --out-mode density
```

## Architecture

### Binary Engine (ca_core/)

| File | Purpose |
|------|---------|
| `grid.h/c` | Scalar `uint8_t` grid with inline get/set |
| `bitgrid.h/c` | Bit-packed `uint64_t` grid, bit-parallel Life kernel |
| `rules.h/c` | 9 binary rules: Conway's Life, HighLife, Day & Night, Seeds, Diamoeba, Anneal, Life Without Death, Morley, Rule 110 |
| `coupling.h/c` | Edge connection graph, cross-grid neighbor resolution |
| `engine.h/c` | `System` struct with scalar + BitGrid fast paths; `sys_step_intent_delayed_genomic` for delayed coupling with per-cell genomic rules |
| `schedule.h/c` | `RuleSchedule` with cycle, random, and shuffle modes |

**Note**: `MAX_GRIDS` is 16 (up from 4). Systems with up to 16 coupled grids are supported.

### Hybrid Engine (ca_core/)

| File | Purpose |
|------|---------|
| `hybrid_engine.h/c` | `HybridSystem` combining binary + payload grids; cross-type edge coupling |

### Payload Engine (ca_core/)

| File | Purpose |
|------|---------|
| `cell.h` | `Cell { uint8_t alive; uint8_t payload; }` |
| `payload_grid.h/c` | `PayloadGrid` allocation, get/set, copy |
| `payload_rules.h/c` | 20 payload rules: life_payload, diffuse, carry_add, max_flood, threshold, identity, xor_payload, and_payload, or_payload, add_mod, shift_left, compare_max, noisy_life, prob_diffuse, nand, nor, xnor, majority, parity, multiply_mod |
| `payload_coupling.h/c` | Cross-edge payload transfer with `float strength` |
| `payload_engine.h/c` | `PayloadSystem` init, step, destroy |

### Wave Engine (ca_core/)

| File | Purpose |
|------|---------|
| `wave_grid.h/c` | `WaveGrid` with `WaveCell { alive, wave, genome, nb_genome, fitness }` |
| `wave_rules.h/c` | 8 wave rules: life, diffuse, genomic_life, vector_and, vector_or, vector_xor, threshold_and, threshold_or |
| `wave_coupling.h/c` | Cross-edge wave transfer |
| `wave_engine.h/c` | `WaveSystem` init, step, `wave_sys_step_genomic` for per-cell rule selection |
| `wave_local_ga.h/c` | Neighborhood consensus genome recomputation |

### Metrics (metrics/)

| File | Purpose |
|------|---------|
| `metrics.h/c` | Density, entropy, activity, period detection via hash ring buffer |
| `export.h/c` | CSV header/row serialization |
| `glossary.h/c` | `GlossaryDB` runtime database with Markdown export |

### DSL (dsl/)

| File | Purpose |
|------|---------|
| `parser.h/c` | Parse grid/connect/steps/seed topology scripts |
| `bind.h/c` | DSL → `System` / `PayloadSystem` runtime binding |
| `codegen.h/c` | Verilog and C code generation stubs |


## BitGrid Fast Path

When grid size is a multiple of 64 and `use_bitgrid` is set, the engine uses `bitgrid_step_life()` — a bit-parallel adder-tree kernel processing 64 cells per `uint64_t` word.

**Limitation**: The BitGrid fast path does **not** apply cross-grid coupling. If coupling is active, `use_bitgrid` must remain `0`.

## Tests

```bash
./tests/test_grid       # Grid API: create, get, set, hash
./tests/test_coupling   # Boundary resolution correctness
./tests/test_dsl        # DSL parser unit tests
./tests/test_bitgrid    # BitGrid + bit-parallel Life equivalence
./tests/test_bind       # DSL runtime binding (binary + payload)
./tests/test_schedule   # Rule schedule cycle, random, shuffle modes
./tests/test_glossary   # GlossaryDB create, add, resize, export
./tests/test_hybrid     # HybridSystem init, step, cross-type edge transfer
```

## Dependencies

- C11 compiler (GCC or Clang)
- Standard C library (`math.h` for entropy)
- No external dependencies, no Python, no GUI

## Directory Layout

```
coupled_ca/
├── ca_core/           # Simulation kernel
├── dsl/               # Topology DSL parser and codegen
├── metrics/           # Measurement and CSV export
├── experiments/       # Standalone CLI tools
├── tests/             # Unit tests
├── utils/             # RNG (xoshiro256**)
├── data/              # Outputs (gitignored)
├── Makefile
└── README.md
```

## Roadmap

- Phases 0–6: Binary CA experiments (Complete)
- Phase 7: Publication (In progress)
- Phase 8: Language hypothesis (Partial)
- Phase 9: Payload engine (Complete)
- Phase 10: Hybrid binary+payload engine (Partial)
- Phase C: Expanded rulesets + per-cell genomic rule selection for payload and wave engines (Complete)
- Phase D: Logic-gate evolution experiments across all grid types via population-level GA (Complete — **breakthrough**: modulo lookup + shaped fitness + density-relative output + IC-drift fix unblocked all gates)
- Phase 11: DSL codegen prototype (Partial)
- Phase 12: Hardware lowering (Future)
