#ifndef METRICS_H
#define METRICS_H

#include "../ca_core/grid.h"

typedef struct {
    double density;
    double entropy;
    double activity;
    int detected_period;
    int gliders;
    int oscillators;
} GridMetrics;

void metrics_compute(const Grid *curr, const Grid *prev, GridMetrics *m);
void metrics_census(const Grid *g, GridMetrics *m);
uint64_t grid_hash(const Grid *g);

/* Period detector: ring buffer of grid hashes */
typedef struct {
    uint64_t *hashes;
    int capacity;
    int count;
    int pos;
} PeriodRing;

PeriodRing *period_ring_create(int capacity);
void period_ring_destroy(PeriodRing *pr);
void period_ring_push(PeriodRing *pr, uint64_t hash);
int period_ring_detect(const PeriodRing *pr);

#endif
