#include "payload_local_ga.h"
#include "payload_coupling.h"
#include <stdlib.h>
#include <string.h>

static int moore_neighbor_idx(int sz, int x, int y, int dx, int dy) {
    int nx = (x + dx + sz) % sz;
    int ny = (y + dy + sz) % sz;
    return ny * sz + nx;
}

static double payload_grid_cell_fitness(const PayloadGrid *g, int idx) {
    return g->fitness ? g->fitness[idx] : 0.0;
}

void payload_local_fitness_accumulate(PayloadSystem *sys, const Cell * const *prev_cells,
                                      PayloadLocalFitnessMode mode, int target_grid, Edge target_edge,
                                      double discount) {
    if (!sys) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        PayloadGrid *g = sys->grids[gi];
        if (!g || !g->active || !g->genomes || !g->fitness) continue;
        int sz = g->size;
        int n = sz * sz;
        for (int idx = 0; idx < n; idx++) {
            int x = idx % sz;
            int y = idx / sz;
            double delta = 0.0;
            switch (mode) {
                case PAYLOAD_FITNESS_STABILITY:
                    if (prev_cells && prev_cells[gi]) {
                        delta = (g->cells[idx].alive == prev_cells[gi][idx].alive) ? 0.1 : -0.05;
                    }
                    break;
                case PAYLOAD_FITNESS_ACTIVITY:
                    if (prev_cells && prev_cells[gi]) {
                        delta = (g->cells[idx].alive != prev_cells[gi][idx].alive) ? 0.1 : -0.02;
                    }
                    break;
                case PAYLOAD_FITNESS_DENSITY:
                    delta = g->cells[idx].alive ? 0.05 : 0.0;
                    break;
                case PAYLOAD_FITNESS_EDGE_SIGNAL:
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
                case PAYLOAD_FITNESS_PAYLOAD_MAGNITUDE:
                    delta = (g->cells[idx].payload / 255.0) * 0.1;
                    break;
            }
            if (discount < 1.0 && discount >= 0.0)
                g->fitness[idx] = discount * g->fitness[idx] + delta;
            else
                g->fitness[idx] += delta;
        }
    }
}

void payload_local_ga_step_grid(PayloadGrid *g, CellGenome *next_genomes,
                                int tournament_k, int mutate_mask, uint64_t (*rng)(void)) {
    if (!g || !g->active || !g->genomes || !next_genomes || !rng) return;
    int sz = g->size;
    int n = sz * sz;
    for (int idx = 0; idx < n; idx++) {
        int x = idx % sz;
        int y = idx / sz;
        double parent_f = payload_grid_cell_fitness(g, idx);
        int best_idx = -1;
        double best_f = parent_f;
        for (int i = 0; i < tournament_k; i++) {
            int dx, dy;
            do {
                dx = (int)(rng() % 3) - 1;
                dy = (int)(rng() % 3) - 1;
            } while (dx == 0 && dy == 0);
            int ni = moore_neighbor_idx(sz, x, y, dx, dy);
            double f = payload_grid_cell_fitness(g, ni);
            if (f > best_f) { best_idx = ni; best_f = f; }
        }
        CellGenome parent = g->genomes[idx];
        if (best_idx < 0) {
            next_genomes[idx] = parent;
        } else {
            CellGenome winner = g->genomes[best_idx];
            CellGenome child = genome_crossover(parent, winner, rng);
            double mut_prob = (double)GENOME_MUTATION_RATE(child) / 15.0;
            child = genome_mutate(child, mut_prob, mutate_mask, rng);
            next_genomes[idx] = child;
        }
    }
}

void payload_local_ga_step_system(PayloadSystem *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void)) {
    if (!sys || !rng) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        PayloadGrid *g = sys->grids[gi];
        if (!g || !g->active || !g->genomes) continue;
        int n = g->size * g->size;
        CellGenome *tmp = malloc(n * sizeof(CellGenome));
        if (!tmp) continue;
        payload_local_ga_step_grid(g, tmp, tournament_k, mutate_mask, rng);
        memcpy(g->genomes, tmp, n * sizeof(CellGenome));
        free(tmp);
    }
}

void payload_local_ga_step_system_alloc(PayloadSystem *sys, int tournament_k, int mutate_mask, uint64_t (*rng)(void)) {
    if (!sys || !rng) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        PayloadGrid *g = sys->grids[gi];
        if (!g || !g->active) continue;
        payload_grid_alloc_genomes(g);
    }
    payload_local_ga_step_system(sys, tournament_k, mutate_mask, rng);
}
