#ifndef PAYLOAD_GRID_H
#define PAYLOAD_GRID_H

#include "cell.h"
#include "genome.h"
#include <stdint.h>

#define MAX_PAYLOAD_GRIDS 4
#define MAX_PAYLOAD_SIZE  128

typedef struct RuleSchedule_ RuleSchedule;

typedef struct {
    int active;
    int size;
    int rule_idx;
    uint64_t rng_state;     /* for stochastic rules */
    RuleSchedule *sched;    /* optional rule rotation schedule */
    Cell *cells;
    Cell *next_cells;
    CellGenome *genomes;    /* per-cell genome (2 bytes/cell), NULL = not used */
    double *fitness;        /* per-cell fitness accumulator, NULL = not used */
} PayloadGrid;

PayloadGrid *payload_grid_create(int size);
void payload_grid_destroy(PayloadGrid *g);
void payload_grid_clear(PayloadGrid *g);
void payload_grid_init_random(PayloadGrid *g, uint64_t (*rng)(void));
void payload_grid_copy(const PayloadGrid *src, PayloadGrid *dst);
void payload_grid_alloc_genomes(PayloadGrid *g);
void payload_grid_clear_fitness(PayloadGrid *g);

static inline Cell payload_grid_get(const PayloadGrid *g, int x, int y) {
    return g->cells[y * g->size + x];
}

static inline void payload_grid_set(PayloadGrid *g, int x, int y, Cell c) {
    g->cells[y * g->size + x] = c;
}

#endif
