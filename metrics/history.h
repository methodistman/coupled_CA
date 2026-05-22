#ifndef METRICS_HISTORY_H
#define METRICS_HISTORY_H

#include <stddef.h>
#include "metrics.h"

/* Time-series storage for per-tick metrics across all grids.
   Arrays grow automatically; each tick appends a new sample. */

typedef struct MetricsHistory {
    int num_grids;
    int capacity;   /* allocated ticks */
    int length;     /* number of samples stored */

    /* Per-grid, per-tick arrays [tick * num_grids + grid] */
    double *density;
    double *entropy;
    double *activity;
    int    *gliders;
    int    *oscillators;

    /* Cross-grid: TE src->dst per tick [tick * (num_grids * num_grids) + src * num_grids + dst] */
    double *te_matrix;
} MetricsHistory;

MetricsHistory *metrics_history_create(int num_grids, int initial_capacity);
void metrics_history_destroy(MetricsHistory *mh);
void metrics_history_reset(MetricsHistory *mh);

/* Append a sample for a single grid. Call once per grid per tick. */
void metrics_history_append(MetricsHistory *mh, int grid_idx, const GridMetrics *m);

/* Append a TE value for a src->dst pair at the current tick.
   Must be called AFTER all grid metrics for the tick have been appended. */
void metrics_history_append_te(MetricsHistory *mh, int src_grid, int dst_grid, double te);

/* Accessors (returns 0/0.0 if out of bounds) */
double metrics_history_density(const MetricsHistory *mh, int tick, int grid);
double metrics_history_entropy(const MetricsHistory *mh, int tick, int grid);
double metrics_history_activity(const MetricsHistory *mh, int tick, int grid);
int    metrics_history_gliders(const MetricsHistory *mh, int tick, int grid);
int    metrics_history_oscillators(const MetricsHistory *mh, int tick, int grid);
double metrics_history_te(const MetricsHistory *mh, int tick, int src_grid, int dst_grid);

/* Export full history as CSV to a file. Returns 0 on success, -1 on failure. */
int metrics_history_export_csv(const MetricsHistory *mh, const char *path);

#endif
