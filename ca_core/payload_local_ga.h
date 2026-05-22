#ifndef PAYLOAD_LOCAL_GA_H
#define PAYLOAD_LOCAL_GA_H

#include "payload_grid.h"
#include "payload_engine.h"

/* Local fitness modes for payload grids.
   The binary modes (STABILITY, ACTIVITY, DENSITY, EDGE_SIGNAL) use cell.alive.
   PAYLOAD_MAGNITUDE rewards high payload values. */
typedef enum {
    PAYLOAD_FITNESS_STABILITY,
    PAYLOAD_FITNESS_ACTIVITY,
    PAYLOAD_FITNESS_DENSITY,
    PAYLOAD_FITNESS_EDGE_SIGNAL,
    PAYLOAD_FITNESS_PAYLOAD_MAGNITUDE,
} PayloadLocalFitnessMode;

/* Accumulate local fitness for all cells in all payload grids.
   If prev_cells is NULL, only density/edge/payload modes work.
   discount in [0,1]: 1.0 = simple accumulation (default), <1.0 = exponential decay. */
void payload_local_fitness_accumulate(PayloadSystem *sys, const Cell * const *prev_cells,
                                      PayloadLocalFitnessMode mode, int target_grid, Edge target_edge,
                                      double discount);

/* Run one local GA step on a single PayloadGrid: tournament selection among
   Moore neighbors, crossover, mutation, elitist replacement into next_genomes. */
void payload_local_ga_step_grid(PayloadGrid *g, CellGenome *next_genomes,
                                int tournament_k, int mutate_mask, uint64_t (*rng)(void));

/* Convenience: run local GA on all grids in a PayloadSystem. */
void payload_local_ga_step_system(PayloadSystem *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void));

/* Allocate scratch buffers for all grids and run one GA step. */
void payload_local_ga_step_system_alloc(PayloadSystem *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void));

#endif
