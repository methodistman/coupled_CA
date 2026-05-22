# Paper 6: A Declarative DSL for Reproducible Cellular Automata Experiments

## Metadata
- **Venue**: JOSS / SoftwareX
- **Preprint**: arXiv, March 2027
- **Hypotheses**: H15
- **Experiments**: E3

## Abstract
We present a declarative domain-specific language (DSL) for specifying coupled cellular automata experiments. A JSON/YAML specification maps cleanly to System, Coupling, Pipeline, and genome configurations. We demonstrate that at least three existing experiments (signal transmission, local GA, reservoir readout) can be reproduced byte-for-byte from declarative specs. The DSL enables reproducible CA research by separating experimental design from implementation details, facilitating sharing and review.

## Structure
1. Introduction — Reproducibility crisis in computational research
2. Related Work — JSON/YAML experiment specs in ML (MLflow, W&B, Sacred)
3. Methods — JSON schema: system, coupling, pipeline, genome spec
4. Experiments — E3: reproduce A1, B3, C2 from JSON
5. Results — Byte-equal trajectories; spec validation
6. Discussion — Limitations: what cannot be declared
7. Conclusion — Declarative specs enable reproducible CA science

## Figures
1. JSON schema diagram
2. Spec example: signal transmission experiment
3. Validation pipeline

## Status
- Partial: dsl/ directory exists with basic parser

## Next Steps
1. Extend JSON schema to cover all experiment parameters
2. Implement byte-equal validation
3. Document three full experiment specs
4. Draft manuscript
5. arXiv preprint March 2027

---
*Last updated: 2026-05-15*
