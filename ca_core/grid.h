#ifndef GRID_H
#define GRID_H

#include <stdint.h>
#include "genome.h"

#define MAX_GRIDS 16
#define MAX_GRID_SIZE 128

typedef struct RuleSchedule_ RuleSchedule;

typedef struct {
    int active;
    int size;
    int rule_idx;
    uint64_t rng_state;     /* for stochastic rules */
    RuleSchedule *sched;    /* optional rule rotation schedule */
    uint8_t *cells;
    uint8_t *next_cells;
    CellGenome *genomes;    /* per-cell genome (2 bytes/cell), NULL = not used */
    double *fitness;        /* per-cell fitness accumulator, NULL = not used */
} Grid;

Grid *grid_create(int size);
void grid_destroy(Grid *g);
void grid_clear(Grid *g);
void grid_init_random(Grid *g, uint64_t (*rng)(void));
void grid_copy(const Grid *src, Grid *dst);
void grid_alloc_genomes(Grid *g);
void grid_clear_fitness(Grid *g);

static inline int grid_get(const Grid *g, int x, int y) {
    return g->cells[y * g->size + x];
}

static inline void grid_set(Grid *g, int x, int y, int val) {
    g->cells[y * g->size + x] = val ? 1 : 0;
}

#endif
