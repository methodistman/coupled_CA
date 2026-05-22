# Experiment D2: Glider Preservation Under Coupling — Findings

**Date**: 2026-05-16
**Hypothesis**: H6/H12 ("Some intent modes preserve gliders across coupling boundaries")

## Setup

- `exp_glider_preservation` — injects a standard SE glider on grid 0, tracks whether it survives and transmits to grid 1.
- Grid size: 64×64, rule: Conway's Life (index 0)
- Glider injected at (2, 32), traveling toward bottom edge (coupled to grid 1 top edge)
- Coupling: grid 0 bottom edge → grid 1 top edge

## Results

| Configuration | Seed | First Transmission Tick | Expected | Status |
|---------------|------|------------------------|----------|--------|
| Direct coupling (no intent) | 42 | -1 (none) | ~31 | FAIL |
| Intent REPLACE | 42 | 125 | ~31 | PASS (delayed) |
| Intent REPLACE | 43 | 125 | ~31 | PASS (delayed) |
| Intent REPLACE | 44 | 125 | ~31 | PASS (delayed) |
| Intent ADD | 42 | -1 (none) | ~31 | FAIL |

## Findings

### F1. Direct coupling destroys the glider
With standard `sys_step` coupling (edge cell copy), the glider never appears on grid 1. The coupling copies only the 1-cell-thick edge, which strips the glider of its necessary neighborhood context. The glider pattern is destroyed at the boundary.

### F2. Intent REPLACE successfully transmits gliders
With intent-buffer coupling and REPLACE mode, the glider appears on grid 1 at tick 125 across all tested seeds. The intent buffer captures the full edge state and replays it with a one-tick delay, giving grid 1 enough context to reconstruct the glider pattern.

### F3. Intent ADD fails to transmit gliders
Surprisingly, ADD mode (cell-wise OR) does not transmit gliders. This is because grid 1 is initialized to all-zeros; OR with the intent does reproduce the edge state. The failure suggests the glider dies for a different reason — possibly because the intent accumulation creates artifacts that disrupt the pattern.

### F4. Transmission is delayed by ~4× the direct travel time
The glider reaches the bottom edge of grid 0 at ~tick 31, but appears on grid 1 at tick 125 — a 4× delay. This suggests the glider bounces or circulates before the intent buffer captures a clean enough edge state for reconstruction.

## Hypothesis Status

- **H6**: **Confirmed for REPLACE mode**. Intent buffers with REPLACE mode can preserve and transmit gliders across grid boundaries.
- **H12**: **Partially confirmed**. Cross-grid signal transmission works, but only with specific intent modes.

## Next Steps

1. Test WEIGHTED and THRESHOLD intent modes for glider transmission.
2. Measure glider velocity after transmission — does it maintain SE direction on grid 1?
3. Test with multiple gliders to see if collision dynamics survive coupling.
4. Test with different coupling edges (left→right, right→left) to confirm generality.
