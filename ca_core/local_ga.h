#ifndef LOCAL_GA_H
#define LOCAL_GA_H

#include "grid.h"
#include "engine.h"

/* Local fitness modes */
typedef enum {
    FITNESS_STABILITY,    /* reward cells that don't change state */
    FITNESS_ACTIVITY,     /* reward oscillation (changed from prev) */
    FITNESS_DENSITY,      /* reward being alive */
    FITNESS_EDGE_SIGNAL,  /* reward cells on target edge being alive */
} LocalFitnessMode;

/* Accumulate local fitness for all cells in all grids.
   prev_cells should hold the cell state from the previous tick.
   If prev_cells is NULL, only density/edge modes work.
   discount in [0,1]: 1.0 = simple accumulation (default), <1.0 = exponential decay. */
void local_fitness_accumulate(System *sys, const uint8_t * const *prev_cells,
                                LocalFitnessMode mode, int target_grid, Edge target_edge,
                                double discount);

/* Run one local GA step: tournament selection among Moore neighbors,
   crossover, mutation, elitist replacement into next_genomes.
   next_genomes must be a pre-allocated scratch buffer (same size as g->genomes).
   tournament_k is the number of neighbors to sample (typically 3).
   mutate_mask is a bitmask of GENOME_MUTATE_*; 0 defaults to all. */
void local_ga_step_grid(Grid *g, CellGenome *next_genomes,
                        int tournament_k, int mutate_mask, uint64_t (*rng)(void));

/* Convenience: run local GA on all grids in a System. */
void local_ga_step_system(System *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void));

/* Allocate scratch buffers for all grids and run one GA step. */
void local_ga_step_system_alloc(System *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void));

#endif
