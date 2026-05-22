#ifndef WAVE_LOCAL_GA_H
#define WAVE_LOCAL_GA_H

#include "wave_grid.h"
#include "wave_engine.h"

typedef enum {
    WAVE_FITNESS_STABILITY,
    WAVE_FITNESS_ACTIVITY,
    WAVE_FITNESS_DENSITY,
    WAVE_FITNESS_EDGE_SIGNAL,
    WAVE_FITNESS_WAVE_MAGNITUDE,
    WAVE_FITNESS_COHERENCE,
} WaveLocalFitnessMode;

/* Genome-vs-neighborhood penalty: -0.33 * (number of differing fields).
   Returns value in [-1.0, 0.0]. */
double wave_genome_penalty(CellGenome cell_g, CellGenome nb_g);

/* Effective fitness used for selection = fitness + genome_penalty. */
double wave_effective_fitness(const WaveGrid *g, int idx);

/* Accumulate per-cell fitness across all grids. */
void wave_local_fitness_accumulate(WaveSystem *sys,
                                    const WaveCell * const *prev_cells,
                                    WaveLocalFitnessMode mode,
                                    int target_grid, Edge target_edge,
                                    double discount);

/* Fast layer: evolve cell genomes via tournament selection + crossover + mutation.
   Writes results into next_genomes (size n = sz*sz). */
void wave_local_ga_step_grid(WaveGrid *g, CellGenome *next_genomes,
                              int tournament_k, int mutate_mask,
                              uint64_t (*rng)(void));

/* Slow layer: in-place mutation of nb_genome (no selection, just drift). */
void wave_local_ga_step_nb(WaveGrid *g, double nb_mutation_rate,
                            int mutate_mask, uint64_t (*rng)(void));

/* Run fast GA on all grids in a system. */
void wave_local_ga_step_system(WaveSystem *sys, int tournament_k,
                                int mutate_mask, uint64_t (*rng)(void));

#endif
