#include "metrics.h"
#include "../ca_core/patterns.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv1a(const uint8_t *data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void metrics_compute(const Grid *curr, const Grid *prev, GridMetrics *m) {
    int n = curr->size * curr->size;
    int alive = 0, changed = 0;
    for (int i = 0; i < n; i++) {
        if (curr->cells[i]) alive++;
        if (prev && curr->cells[i] != prev->cells[i]) changed++;
    }
    m->density = (double)alive / n;
    m->activity = prev ? (double)changed / n : 0.0;

    /* entropy: p0 and p1 */
    if (alive == 0 || alive == n) {
        m->entropy = 0.0;
    } else {
        double p1 = (double)alive / n;
        double p0 = 1.0 - p1;
        m->entropy = -(p0 * log(p0) + p1 * log(p1)) / log(2.0);
    }

    m->detected_period = -1; /* caller sets via period detector */
}

PeriodRing *period_ring_create(int capacity) {
    PeriodRing *pr = calloc(1, sizeof(PeriodRing));
    pr->hashes = calloc(capacity, sizeof(uint64_t));
    pr->capacity = capacity;
    return pr;
}

void period_ring_destroy(PeriodRing *pr) {
    free(pr->hashes);
    free(pr);
}

void period_ring_push(PeriodRing *pr, uint64_t hash) {
    pr->hashes[pr->pos] = hash;
    pr->pos = (pr->pos + 1) % pr->capacity;
    if (pr->count < pr->capacity) pr->count++;
}

int period_ring_detect(const PeriodRing *pr) {
    if (pr->count < 2) return -1;
    uint64_t target = pr->hashes[(pr->pos + pr->capacity - 1) % pr->capacity];
    /* search backwards from newest-1 to oldest */
    for (int i = 2; i <= pr->count; i++) {
        int idx = (pr->pos + pr->capacity - i) % pr->capacity;
        if (pr->hashes[idx] == target) return i;
    }
    return -1;
}

void metrics_census(const Grid *g, GridMetrics *m) {
    m->gliders = census_gliders(g);
    m->oscillators = census_oscillators(g);
}

uint64_t grid_hash(const Grid *g) {
    return fnv1a(g->cells, g->size * g->size * sizeof(uint8_t));
}
