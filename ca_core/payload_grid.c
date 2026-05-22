#include "payload_grid.h"
#include "schedule.h"
#include <stdlib.h>
#include <string.h>

PayloadGrid *payload_grid_create(int size) {
    PayloadGrid *g = calloc(1, sizeof(PayloadGrid));
    if (!g) return NULL;
    g->active = 1;
    g->size = size;
    int n = size * size;
    g->cells = calloc(n, sizeof(Cell));
    g->next_cells = calloc(n, sizeof(Cell));
    if (!g->cells || !g->next_cells) {
        payload_grid_destroy(g);
        return NULL;
    }
    return g;
}

void payload_grid_destroy(PayloadGrid *g) {
    if (!g) return;
    schedule_destroy(g->sched);
    g->sched = NULL;
    free(g->cells);
    free(g->next_cells);
    free(g->genomes);
    free(g->fitness);
    free(g);
}

void payload_grid_clear(PayloadGrid *g) {
    if (!g || !g->active) return;
    memset(g->cells, 0, g->size * g->size * sizeof(Cell));
}

void payload_grid_init_random(PayloadGrid *g, uint64_t (*rng)(void)) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) {
        g->cells[i].alive = (rng() % 5 == 0) ? 1 : 0;
        g->cells[i].payload = (uint8_t)(rng() % 256);
    }
}

void payload_grid_copy(const PayloadGrid *src, PayloadGrid *dst) {
    if (!src || !dst || !src->active || !dst->active) return;
    int n = src->size * src->size;
    memcpy(dst->cells, src->cells, n * sizeof(Cell));
}

void payload_grid_alloc_genomes(PayloadGrid *g) {
    if (!g || !g->active) return;
    int n = g->size * g->size;
    if (!g->genomes) g->genomes = calloc(n, sizeof(CellGenome));
    if (!g->fitness) g->fitness = calloc(n, sizeof(double));
}

void payload_grid_clear_fitness(PayloadGrid *g) {
    if (!g || !g->fitness) return;
    memset(g->fitness, 0, g->size * g->size * sizeof(double));
}
