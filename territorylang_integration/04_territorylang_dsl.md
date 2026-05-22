---
description: Phase 4 — Embed coupled CA as a TerritoryLang territory with formal layers, kernels, and scheduling
tags: [territorylang, dsl, language, compiler, phase4]
---

# Phase 4: TerritoryLang DSL Integration

## TerritoryLang Concept

From `01-vision-and-scope.md`:
> Design a **low-level, explicitly parallel simulation language** specialized for territorial worlds... Has its **own compiler and build tool**, producing native CPU and GPU code.

From `06-runtime-and-tooling.md`:
> Front-end parses source, checks types and region/effect constraints. Mid-level IR expresses kernels, regions, and tick DAGs. Backends: CPU (LLVM) and GPU (CUDA/HIP/SPIR-V).

This is the **endgame**: not just borrowing ideas, but expressing the coupled CA system as a valid TerritoryLang program.

## Current State vs Target

### Current (C Codebase)

```c
/* Imperative, scattered across files */
System sys;
sys_init(&sys, 2, 64);
sys.grids[0]->rule_idx = rules_index_by_name("Conway's Life");
coupling_add(&sys, 0, EDGE_RIGHT, 1, EDGE_LEFT);
for (int t = 0; t < 1000; t++) {
    sys_step(&sys);
}
```

### Target (TerritoryLang DSL)

```territory
/* Declarative, single file, compiler-verified */
world CoupledCA {
    territory BinaryGrid {
        dimensions: 2D(64, 64);
        topology: wrap;
        layers: {
            state: u1,           /* 1-bit alive/dead */
            rule: u4,            /* rule selector */
            genome: Genome16,    /* 16-bit evolvable params */
        };
    }
    territory PayloadGrid {
        dimensions: 2D(64, 64);
        topology: wrap;
        layers: {
            state: u8,           /* 8-bit payload value */
            rule: u4,
        };
    }
    coupling {
        BinaryGrid.right -> PayloadGrid.left {
            mode: weighted(0.3);
            scale: proportional;
            intent_buffer: true;
            delay: 0;
        }
    }
    tick_pipeline {
        phase exchange_all { exchange edges; }
        phase step_binary   { run life_kernel on BinaryGrid; }
        phase step_payload  { run life_kernel on PayloadGrid; }
        phase census        { analyze census on PayloadGrid; }
        phase local_ga      { mutate genomes on BinaryGrid; }
    }
    /* Experiment parameters */
    experiment SignalTransmission {
        steps: 1000;
        inject: glider at BinaryGrid(10, 32);
        target_edge: PayloadGrid.left;
        fitness: correlation(target_edge, expected_wave);
    }
}
```

## Bridging Architecture

We cannot jump to a full compiler. Instead, build a **C embedding layer** that maps TerritoryLang concepts to the existing codebase.

### Layer 1: C Structs as TerritoryLang Declarations

```c
/* territorylang/c_api.h — map DSL concepts to C structs */

typedef struct TL_Territory {
    const char *name;
    int dim;        /* 1, 2, or 3 */
    int sizes[3];
    TL_Topology topology;
    TL_Layer *layers;
    int n_layers;
} TL_Territory;

typedef struct TL_Coupling {
    const char *src_territory;
    TLEdge src_edge;
    const char *dst_territory;
    TLEdge dst_edge;
    TL_CouplingMode mode;
    float weight;
    int delay;
    int use_intent_buffer;
} TL_Coupling;

typedef struct TL_WorldProgram {
    TL_Territory *territories;
    int n_territories;
    TL_Coupling *couplings;
    int n_couplings;
    Pipeline *pipeline;  /* from Phase 2 */
} TL_WorldProgram;
```

### Layer 2: Parser for Minimal DSL

A small parser (maybe 200 lines with a PEG library, or hand-written) for the subset shown above:

```
world       := "world" IDENT "{" territory+ coupling? tick_pipeline? experiment* "}"
territory   := "territory" IDENT "{" dimensions topology layers "}"
dimensions  := "dimensions:" ("1D" | "2D" | "3D") "(" NUMBER ("," NUMBER)* ")"
topology    := "topology:" ("wrap" | "clamp" | "mirror")
layers      := "layers:" "{" layer_def+ "}"
layer_def   := IDENT ":" TYPE ","
type        := "u1" | "u4" | "u8" | "u16" | "u32" | "u64" | "i8" | ...
coupling    := "coupling" "{" connection+ "}"
connection  := TERRITORY "." EDGE "->" TERRITORY "." EDGE "{" coupling_opts "}"
coupling_opts := mode | scale | intent_buffer | delay
tick_pipeline := "tick_pipeline" "{" phase+ "}"
phase       := "phase" IDENT "{" phase_body "}"
phase_body  := ("run" KERNEL "on" TERRITORY) |
                ("exchange" ("edges" | TERRITORY "." EDGE)) |
                ("analyze" ANALYZER "on" TERRITORY) |
                ("mutate" MUTATOR "on" TERRITORY)
experiment  := "experiment" IDENT "{" experiment_body "}"
```

### Layer 3: Code Generator

The parser produces a `TL_WorldProgram`. A code generator emits C:

```c
/* Generated from a .territory file */
static TL_WorldProgram coupled_ca_world = {
    .n_territories = 2,
    .territories = (TL_Territory[]){
        {"BinaryGrid", 2, {64, 64}, TOPOLOGY_WRAP, ...},
        {"PayloadGrid", 2, {64, 64}, TOPOLOGY_WRAP, ...},
    },
    .n_couplings = 1,
    .couplings = (TL_Coupling[]){
        {"BinaryGrid", EDGE_RIGHT, "PayloadGrid", EDGE_LEFT,
         MODE_WEIGHTED, 0.3f, 0, 1},
    },
    .pipeline = &pipeline_parallel_with_census,  /* Phase 2 preset */
};
```

### Layer 4: Runtime Binding

The generated C initializes the actual `System`:

```c
void tl_world_init(const TL_WorldProgram *prog, System *sys) {
    sys_init(sys, prog->n_territories, prog->territories[0].sizes[0]);
    for (int i = 0; i < prog->n_couplings; i++) {
        TL_Coupling *c = &prog->couplings[i];
        coupling_add(sys,
            territory_index(prog, c->src_territory), c->src_edge,
            territory_index(prog, c->dst_territory), c->dst_edge);
    }
    sys->pipeline = prog->pipeline;
}
```

## Concrete Example: Coupled CA as TerritoryLang

```territory
/* experiments/signal.territory */
world SignalTransmission {
    territory Source {
        dimensions: 2D(64, 64);
        topology: wrap;
        layers: {
            state: u1,
            rule: u4,      /* 0 = Life, 1 = HighLife, ... */
        };
    }
    territory Target {
        dimensions: 2D(64, 64);
        topology: wrap;
        layers: {
            state: u1,
            rule: u4,
        };
    }
    coupling {
        Source.right -> Target.left {
            mode: replace;
            scale: proportional;
            intent_buffer: true;
        }
    }
    tick_pipeline {
        phase exchange { exchange edges; }
        phase step_source { run conway_step on Source; }
        phase step_target { run conway_step on Target; }
        phase measure { analyze edge_correlation on Target.left; }
    }
    experiment main {
        steps: 1000;
        seed: 42;
        inject: glider at Source(10, 32);
        fitness: edge_correlation;
        log: csv("signal.csv");
    }
}
```

Generated C (`experiments/signal_territory.c`):

```c
#include "territorylang/c_api.h"
#include "ca_core/engine.h"

static void run_signal_experiment(void) {
    TL_WorldProgram *prog = tl_parse_file("experiments/signal.territory");
    System sys;
    tl_world_init(prog, &sys);

    inject_glider(&sys, 0);  /* from experiment.inject */

    for (int t = 0; t < prog->experiments[0].steps; t++) {
        pipeline_execute(prog->pipeline, &sys);
    }

    double fitness = metrics_edge_correlation(&sys, 1, EDGE_LEFT);
    printf("fitness = %f\n", fitness);
}
```

## Integration Path

This is a long-term (6+ month) effort. Milestones:

### Milestone 4.1: C API Layer (2-3 weeks)
- Define `TL_Territory`, `TL_Coupling`, `TL_WorldProgram` structs
- Write `tl_world_init()` that maps DSL structs to existing `System`
- Write `tl_parse_json()` so world programs can be defined in JSON (easier than custom parser)
- **Deliverable**: `territorylang/c_api.h/c` + example JSON world definition

### Milestone 4.2: JSON Experiment Definitions (2 weeks)
- Experiments are JSON, not hardcoded C
- `experiments/signal.json` defines territories, couplings, steps, inject, fitness
- C experiment runner: `exp_from_json("experiments/signal.json")`
- **Deliverable**: All existing experiments have JSON equivalents; Makefile can build either

### Milestone 4.3: Custom Parser (4-6 weeks)
- Hand-written recursive descent parser for the `.territory` language
- Parser produces `TL_WorldProgram` in memory
- Error messages with line numbers
- **Deliverable**: `territorylang/parser.h/c` + first `.territory` file that compiles

### Milestone 4.4: Type/Region Checking (4-6 weeks)
- Statically verify:
  - All referenced territories exist
  - Coupling edges are valid for territory dimensions
  - Kernel signatures match layer types (e.g., `conway_step` requires `u1` state layer)
  - Pipeline phases have valid dependencies (no read-after-write races)
- **Deliverable**: Parser rejects invalid programs with useful errors

### Milestone 4.5: Code Generation (8+ weeks)
- Instead of runtime interpretation, generate specialized C code per world program
- Inline kernel loops, unroll small neighborhoods, specialize rule transitions
- **Deliverable**: Generated C is faster than generic `grid_step()` for known world programs

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Writing a compiler is hard | Start with JSON (no parser), then parser, then codegen. Each milestone is independently useful |
| C is not a good host for DSLs | Use JSON as the "user-facing" format; C structs as the runtime representation. No macro magic |
| Existing codebase too coupled to C | Phase 1-3 gradually decouple engine from hardcoded loops; Phase 4 just formalizes the abstraction |
| Nobody wants to learn a new language | JSON experiment definitions require zero learning for users familiar with C |
| Performance of interpreted pipeline | Use Phase 2's pipeline presets (precompiled function pointers), not string dispatch |

## Relation to Other Phases

| Phase | TerritoryLang Mapping |
|-------|----------------------|
| **Phase 1 (Intent Buffers)** | `intent_buffer: true` in coupling block; `exchange edges` in pipeline |
| **Phase 2 (Tick Pipeline)** | `tick_pipeline { phase ... }` is the DSL syntax for the Pipeline struct |
| **Phase 3 (Per-Cell Genomes)** | `layers: { genome: Genome16 }` declares the genome layer; `mutate genomes` is a pipeline phase |
| **This Phase (DSL)** | All of the above in a single declarative file |

## Success Criteria

- [ ] JSON world definition produces identical results to handcoded C experiment
- [ ] Adding a new experiment requires only a new JSON file (no C recompilation)
- [ ] `.territory` parser successfully parses a coupled CA world definition
- [ ] Parser rejects at least 3 common error patterns (missing territory, invalid edge, race condition)
- [ ] Generated/specialized code is within 20% performance of hand-optimized C

## Why This Matters

The current codebase is **experiment-centric**: each experiment is a new C file with hardcoded loops. TerritoryLang makes it **world-centric**: you define the territory, coupling, and pipeline once, then run many experiments against it.

This inversion is the difference between:
- "I wrote a new C program to test signal transmission"
- "I defined a world and ran an experiment on it"

The latter scales to hundreds of experiments, parameter sweeps, and automated hypothesis testing.
