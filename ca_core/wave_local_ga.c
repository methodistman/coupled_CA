#include "wave_local_ga.h"
#include <stdlib.h>
#include <string.h>

static int moore_neighbor_idx(int sz, int x, int y, int dx, int dy) {
    int nx = (x + dx + sz) % sz;
    int ny = (y + dy + sz) % sz;
    return ny * sz + nx;
}

double wave_genome_penalty(CellGenome cell_g, CellGenome nb_g) {
    int d = 0;
    if (GENOME_RULE_SELECT(cell_g) != GENOME_RULE_SELECT(nb_g)) d++;
    if (GENOME_COUPLING_WEIGHT(cell_g) != GENOME_COUPLING_WEIGHT(nb_g)) d++;
    if (GENOME_MUTATION_RATE(cell_g) != GENOME_MUTATION_RATE(nb_g)) d++;
    return -0.33 * (double)d;
}

double wave_effective_fitness(const WaveGrid *g, int idx) {
    double base = g->cells[idx].fitness;
    return base + wave_genome_penalty(g->cells[idx].genome, g->cells[idx].nb_genome);
}

void wave_local_fitness_accumulate(WaveSystem *sys,
                                    const WaveCell * const *prev_cells,
                                    WaveLocalFitnessMode mode,
                                    int target_grid, Edge target_edge,
                                    double discount) {
    if (!sys) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        WaveGrid *g = sys->grids[gi];
        if (!g || !g->active) continue;
        int sz = g->size;
        int n = sz * sz;
        for (int idx = 0; idx < n; idx++) {
            int x = idx % sz;
            int y = idx / sz;
            double delta = 0.0;
            switch (mode) {
                case WAVE_FITNESS_STABILITY:
                    if (prev_cells && prev_cells[gi]) {
                        delta = (g->cells[idx].alive == prev_cells[gi][idx].alive) ? 0.1 : -0.05;
                    }
                    break;
                case WAVE_FITNESS_ACTIVITY:
                    if (prev_cells && prev_cells[gi]) {
                        delta = (g->cells[idx].alive != prev_cells[gi][idx].alive) ? 0.1 : -0.02;
                    }
                    break;
                case WAVE_FITNESS_DENSITY:
                    delta = g->cells[idx].alive ? 0.05 : 0.0;
                    break;
                case WAVE_FITNESS_EDGE_SIGNAL:
                    if (gi == target_grid) {
                        int on_edge = 0;
                        switch (target_edge) {
                            case EDGE_TOP:    on_edge = (y == 0); break;
                            case EDGE_BOTTOM: on_edge = (y == sz - 1); break;
                            case EDGE_LEFT:   on_edge = (x == 0); break;
                            case EDGE_RIGHT:  on_edge = (x == sz - 1); break;
                        }
                        if (on_edge && g->cells[idx].alive) delta = 0.5;
                    }
                    break;
                case WAVE_FITNESS_WAVE_MAGNITUDE:
                    delta = (wave_popcount(g->cells[idx].wave) / 26.0) * 0.1;
                    break;
                case WAVE_FITNESS_COHERENCE:
                    delta = wave_grid_coherence(g, x, y) * 0.1;
                    break;
            }
            if (discount < 1.0 && discount >= 0.0)
                g->cells[idx].fitness = discount * g->cells[idx].fitness + delta;
            else
                g->cells[idx].fitness += delta;
        }
    }
}

void wave_local_ga_step_grid(WaveGrid *g, CellGenome *next_genomes,
                              int tournament_k, int mutate_mask,
                              uint64_t (*rng)(void)) {
    if (!g || !g->active || !next_genomes || !rng) return;
    int sz = g->size;
    int n = sz * sz;
    for (int idx = 0; idx < n; idx++) {
        int x = idx % sz;
        int y = idx / sz;
        double parent_f = wave_effective_fitness(g, idx);
        int best_idx = -1;
        double best_f = parent_f;
        for (int i = 0; i < tournament_k; i++) {
            int dx, dy;
            do {
                dx = (int)(rng() % 3) - 1;
                dy = (int)(rng() % 3) - 1;
            } while (dx == 0 && dy == 0);
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            double f = wave_effective_fitness(g, ni);
            if (f > best_f) { best_idx = ni; best_f = f; }
        }
        CellGenome parent = g->cells[idx].genome;
        if (best_idx < 0) {
            next_genomes[idx] = parent;
        } else {
            CellGenome winner = g->cells[best_idx].genome;
            CellGenome child = genome_crossover(parent, winner, rng);
            double mut_prob = (double)GENOME_MUTATION_RATE(child) / 15.0;
            child = genome_mutate(child, mut_prob, mutate_mask, rng);
            next_genomes[idx] = child;
        }
    }
}

void wave_local_ga_step_nb(WaveGrid *g, double nb_mutation_rate,
                            int mutate_mask, uint64_t (*rng)(void)) {
    if (!g || !g->active || !rng) return;
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) {
        g->cells[i].nb_genome = genome_mutate(g->cells[i].nb_genome,
                                               nb_mutation_rate,
                                               mutate_mask, rng);
    }
}

void wave_local_ga_step_system(WaveSystem *sys, int tournament_k,
                                int mutate_mask, uint64_t (*rng)(void)) {
    if (!sys || !rng) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        WaveGrid *g = sys->grids[gi];
        if (!g || !g->active) continue;
        int n = g->size * g->size;
        CellGenome *tmp = malloc(n * sizeof(CellGenome));
        if (!tmp) continue;
        wave_local_ga_step_grid(g, tmp, tournament_k, mutate_mask, rng);
        for (int i = 0; i < n; i++) g->cells[i].genome = tmp[i];
        free(tmp);
    }
}
