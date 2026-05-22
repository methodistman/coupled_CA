---
description: TerritoryLang Integration Roadmap — phased adoption of concepts from territorylang into coupled_ca
tags: [territorylang, roadmap, integration, index]
---

# TerritoryLang Integration for Coupled CA

## Background

[territorylang](https://github.com/methodistman/territorylang) is an earlier project by the same author: a prototype simulation language for large territorial grids with cellular automata, sparse agents, explicit CPU/GPU scheduling, and in-place updates with static safety guarantees.

This directory contains a phased plan for integrating TerritoryLang's most relevant concepts into the current `coupled_ca` codebase. Each phase is designed to be independently valuable and build upon the previous.

## Summary of Phases

| Phase | Concept | Effort | Impact | Status |
|-------|---------|--------|--------|--------|
| **1** | [Intent Buffers](01_intent_buffers.md) — Decouple cross-grid coupling via append-only edge intents | Low | High | **Done** |
| **2** | [Tick Pipeline](02_tick_pipeline.md) — Replace hardcoded `sys_step()` with a composable DAG of phases | Medium | High | **Done** |
| **3** | [Per-Cell Genomes](03_per_cell_genomes.md) — Evolve per-cell parameters (rules, coupling weights) not just ICs | Medium | Very High | **Done** |
| **4** | [TerritoryLang DSL](04_territorylang_dsl.md) — Embed coupled CA as a declarative TerritoryLang world program | High | Architectural | Not started |

## Dependency Graph

```
Phase 1 (Intent Buffers)
    |
    v
Phase 2 (Tick Pipeline)  <--- Phase 1's intents are exchanged in pipeline phases
    |
    v
Phase 3 (Per-Cell Genomes)  <--- Phase 2's MUTATE phase runs local GA
    |
    v
Phase 4 (TerritoryLang DSL)  <--- Phases 1-3 become DSL constructs
```

## Quick Reference: TerritoryLang Concepts vs. Current Codebase

| TerritoryLang Concept | Current Implementation | Gap |
|-----------------------|----------------------|-----|
| Multi-layer grids | Binary grid + payload grid (2 hardcoded) | Generalize to N layers with typed fields |
| In-place updates with phase discipline | Full grid swap every step | Partial in-place for edge cells only |
| Intent buffers | `sys_step_intent()` with `IntentBuffer` and `IntentRing` | TE computation module added; ring ownership fixed per-System |
| Tick pipeline DAG | `sys_step()` delegates to `pipeline_execute()` when `sys->pipeline` is set; `ca_core/pipeline.h/c` with PHASE_RUN/EXCHANGE/ANALYZE/MUTATE, topological executor, presets (default/intent/analyze/parallel), thread pool for parallel compute | None significant |
| Per-cell genomes | Global GA on initial conditions only | No adaptation during simulation |
| Deterministic per-cell RNG | Global LCG (`rng_u64()`) | Reproducibility requires same global state |
| Structured access modes | Raw pointer access in kernels | No static aliasing guarantees |
| Explicit device placement | CPU-only | No GPU offload path |

## Recommended Starting Point

**Start with Phase 1 (Intent Buffers)**. It requires:
- No changes to grid data structures
- No new syntax or parser
- ~200 lines of new C code
- Immediate payoff: enables transfer entropy, delayed coupling, and history-based analysis

Phase 1 also unblocks Phase 2's `EXCHANGE` phase type, making the pipeline transition smoother.

## Files in This Directory

- `00_index.md` — This file
- `01_intent_buffers.md` — Edge intent buffers, decoupled coupling, temporal queues
- `02_tick_pipeline.md` — Phase DAG, per-tick analysis, parallel grid updates
- `03_per_cell_genomes.md` — Local GA, rule modulation, evolvable coupling
- `04_territorylang_dsl.md` — Declarative world definitions, JSON/C parser, code generation

## Integration with Main Roadmap

These phases should be tracked in the main project roadmaps:

- Add to `roadmap/coupled_ca/15_computational_search.md` under "Next Steps"
- Reference from `roadmap/coupled_ca/BRAINSTORM.md` Immediate Next Steps
- Link from `README.md` under "Future Directions"
