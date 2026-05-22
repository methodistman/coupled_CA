#include "grid.h"
#include "schedule.h"
#include <stdlib.h>
#include <string.h>

Grid *grid_create(int size) {
    Grid *g = calloc(1, sizeof(Grid));
    if (!g) return NULL;
    g->active = 1;
    g->size = size;
    int n = size * size;
    g->cells = calloc(n, sizeof(uint8_t));
    g->next_cells = calloc(n, sizeof(uint8_t));
    if (!g->cells || !g->next_cells) {
        grid_destroy(g);
        return NULL;
    }
    return g;
}

void grid_destroy(Grid *g) {
    if (!g) return;
    schedule_destroy(g->sched);
    g->sched = NULL;
    free(g->cells);
    free(g->next_cells);
    free(g->genomes);
    free(g->fitness);
    free(g);
}

void grid_clear(Grid *g) {
    if (!g || !g->active) return;
    memset(g->cells, 0, g->size * g->size * sizeof(uint8_t));
}

void grid_init_random(Grid *g, uint64_t (*rng)(void)) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    for (int i = 0; i < n; i++)
        g->cells[i] = (rng() % 5 == 0) ? 1 : 0;
}

void grid_copy(const Grid *src, Grid *dst) {
    if (!src || !dst || !src->active || !dst->active) return;
    int n = src->size * src->size;
    memcpy(dst->cells, src->cells, n * sizeof(uint8_t));
}

/* Allocate and zero-initialize genomes and fitness arrays */
void grid_alloc_genomes(Grid *g) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    if (!g->genomes) g->genomes = calloc(n, sizeof(CellGenome));
    if (!g->fitness) g->fitness = calloc(n, sizeof(double));
}

void grid_clear_fitness(Grid *g) {
    if (!g || !g->fitness) return;
    memset(g->fitness, 0, g->size * g->size * sizeof(double));
}
