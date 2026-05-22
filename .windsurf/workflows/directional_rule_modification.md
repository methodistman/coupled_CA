---
description: Per-direction rule selection kernel for anisotropic CA
---

# Directional Rule Modification

## Core Idea

Instead of one rule per cell, the cell selects a rule based on the **direction from which neighbor signals arrive**.

```
Main rule:     rule_select (bits 0-7 of genome)
Alt rule:      alt_rule_select (bits 36-39)
Dir mask:      dir_mask (bits 40-43, one bit per cardinal direction)

For each neighbor at (dx, dy):
  dir = cardinal_direction(dx, dy)  // N, S, E, W, NE, NW, SE, SW
  if dir_mask has bit for dir:
     use alt_rule_select for that neighbor
  else:
     use rule_select
```

## Genome Layout (48-bit)

```
bits 0-7:   rule_select       (main rule, 0-255)
bits 8-15:  coupling_weight   (0-255)
bits 16-23: mutation_rate     (0-255)
bits 24-35: mode_orientations (3 bits x 4 modes)
bits 36-39: alt_rule_select   (alternate rule, 0-15)
bits 40-43: dir_mask          (N,S,E,W for alt rule)
bits 44-47: reserved
```

## Kernel Changes

In `grid_step_kernel`, when counting neighbors:

```c
int base_rule = GENOME_RULE_SELECT(genome);
int alt_rule  = GENOME_ALT_RULE_SELECT(genome);
int dir_mask  = GENOME_DIR_MASK(genome);

for each neighbor (dx, dy):
    int cardinal = dir_to_cardinal(dx, dy);  // 0=N,1=S,2=E,3=W
    int use_rule = (dir_mask & (1 << cardinal)) ? alt_rule : base_rule;
    // apply use_rule for this neighbor's contribution
```

## Why This Helps Multimodal Logic

A cell can behave as:
- **Life** for horizontal/vertical neighbors (supporting signal highways)
- **Diamoeba** for diagonal neighbors (supporting anti-diagonal propagation)

The same cell participates in multiple directional computations with different rules.

## Key Files to Modify

- `ca_core/genome.h` — add alt_rule and dir_mask macros
- `ca_core/genome.c` — add mutation support for new fields
- `ca_core/pipeline.c` — modify `grid_step_kernel` to check dir_mask per neighbor
- `ca_core/pipeline.h` — add `KERNEL_USE_DIR_RULES` flag

## Experiments

- `exp_directional_multimodal.c` — 4-mode multimodal with dir-modified rules
