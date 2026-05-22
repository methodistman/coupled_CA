#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 128

static void ensure_capacity(MetricsHistory *mh) {
    if (!mh || mh->length < mh->capacity) return;
    int new_cap = mh->capacity * 2;
    if (new_cap < INITIAL_CAPACITY) new_cap = INITIAL_CAPACITY;
    int ng = mh->num_grids;
    size_t grid_bytes = new_cap * ng * sizeof(double);
    size_t grid_int_bytes = new_cap * ng * sizeof(int);
    size_t te_bytes = new_cap * ng * ng * sizeof(double);

    double *d = realloc(mh->density, grid_bytes);
    double *e = realloc(mh->entropy, grid_bytes);
    double *a = realloc(mh->activity, grid_bytes);
    int    *g = realloc(mh->gliders, grid_int_bytes);
    int    *o = realloc(mh->oscillators, grid_int_bytes);
    double *t = realloc(mh->te_matrix, te_bytes);

    if (!d || !e || !a || !g || !o || !t) return; /* leave old arrays if realloc fails */

    mh->density = d; mh->entropy = e; mh->activity = a;
    mh->gliders = g; mh->oscillators = o; mh->te_matrix = t;

    /* zero new region */
    memset((char*)mh->density + mh->capacity * ng * sizeof(double), 0, (new_cap - mh->capacity) * ng * sizeof(double));
    memset((char*)mh->entropy + mh->capacity * ng * sizeof(double), 0, (new_cap - mh->capacity) * ng * sizeof(double));
    memset((char*)mh->activity + mh->capacity * ng * sizeof(double), 0, (new_cap - mh->capacity) * ng * sizeof(double));
    memset((char*)mh->gliders + mh->capacity * ng * sizeof(int), 0, (new_cap - mh->capacity) * ng * sizeof(int));
    memset((char*)mh->oscillators + mh->capacity * ng * sizeof(int), 0, (new_cap - mh->capacity) * ng * sizeof(int));
    memset((char*)mh->te_matrix + mh->capacity * ng * ng * sizeof(double), 0, (new_cap - mh->capacity) * ng * ng * sizeof(double));

    mh->capacity = new_cap;
}

MetricsHistory *metrics_history_create(int num_grids, int initial_capacity) {
    if (num_grids < 1) num_grids = 1;
    if (initial_capacity < INITIAL_CAPACITY) initial_capacity = INITIAL_CAPACITY;
    MetricsHistory *mh = calloc(1, sizeof(MetricsHistory));
    if (!mh) return NULL;
    mh->num_grids = num_grids;
    mh->capacity = initial_capacity;
    mh->length = 0;

    mh->density = calloc(initial_capacity * num_grids, sizeof(double));
    mh->entropy = calloc(initial_capacity * num_grids, sizeof(double));
    mh->activity = calloc(initial_capacity * num_grids, sizeof(double));
    mh->gliders = calloc(initial_capacity * num_grids, sizeof(int));
    mh->oscillators = calloc(initial_capacity * num_grids, sizeof(int));
    mh->te_matrix = calloc(initial_capacity * num_grids * num_grids, sizeof(double));

    if (!mh->density || !mh->entropy || !mh->activity || !mh->gliders || !mh->oscillators || !mh->te_matrix) {
        metrics_history_destroy(mh);
        return NULL;
    }
    return mh;
}

void metrics_history_destroy(MetricsHistory *mh) {
    if (!mh) return;
    free(mh->density); free(mh->entropy); free(mh->activity);
    free(mh->gliders); free(mh->oscillators); free(mh->te_matrix);
    free(mh);
}

void metrics_history_reset(MetricsHistory *mh) {
    if (!mh) return;
    mh->length = 0;
}

void metrics_history_append(MetricsHistory *mh, int grid_idx, const GridMetrics *m) {
    if (!mh || grid_idx < 0 || grid_idx >= mh->num_grids) return;
    ensure_capacity(mh);
    int base = mh->length * mh->num_grids + grid_idx;
    mh->density[base] = m->density;
    mh->entropy[base] = m->entropy;
    mh->activity[base] = m->activity;
    mh->gliders[base] = m->gliders;
    mh->oscillators[base] = m->oscillators;
}

void metrics_history_append_te(MetricsHistory *mh, int src_grid, int dst_grid, double te) {
    if (!mh || src_grid < 0 || src_grid >= mh->num_grids) return;
    if (dst_grid < 0 || dst_grid >= mh->num_grids) return;
    ensure_capacity(mh);
    int base = mh->length * mh->num_grids * mh->num_grids
             + src_grid * mh->num_grids + dst_grid;
    mh->te_matrix[base] = te;
}

/* Accessors */
double metrics_history_density(const MetricsHistory *mh, int tick, int grid) {
    if (!mh || tick < 0 || tick >= mh->length || grid < 0 || grid >= mh->num_grids) return 0.0;
    return mh->density[tick * mh->num_grids + grid];
}
double metrics_history_entropy(const MetricsHistory *mh, int tick, int grid) {
    if (!mh || tick < 0 || tick >= mh->length || grid < 0 || grid >= mh->num_grids) return 0.0;
    return mh->entropy[tick * mh->num_grids + grid];
}
double metrics_history_activity(const MetricsHistory *mh, int tick, int grid) {
    if (!mh || tick < 0 || tick >= mh->length || grid < 0 || grid >= mh->num_grids) return 0.0;
    return mh->activity[tick * mh->num_grids + grid];
}
int metrics_history_gliders(const MetricsHistory *mh, int tick, int grid) {
    if (!mh || tick < 0 || tick >= mh->length || grid < 0 || grid >= mh->num_grids) return 0;
    return mh->gliders[tick * mh->num_grids + grid];
}
int metrics_history_oscillators(const MetricsHistory *mh, int tick, int grid) {
    if (!mh || tick < 0 || tick >= mh->length || grid < 0 || grid >= mh->num_grids) return 0;
    return mh->oscillators[tick * mh->num_grids + grid];
}
double metrics_history_te(const MetricsHistory *mh, int tick, int src_grid, int dst_grid) {
    if (!mh || tick < 0 || tick >= mh->length) return 0.0;
    if (src_grid < 0 || src_grid >= mh->num_grids) return 0.0;
    if (dst_grid < 0 || dst_grid >= mh->num_grids) return 0.0;
    return mh->te_matrix[tick * mh->num_grids * mh->num_grids
                        + src_grid * mh->num_grids + dst_grid];
}

int metrics_history_export_csv(const MetricsHistory *mh, const char *path) {
    if (!mh || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int ng = mh->num_grids;
    fprintf(f, "tick");
    for (int g = 0; g < ng; g++) fprintf(f, ",density_g%d,entropy_g%d,activity_g%d,gliders_g%d,oscillators_g%d", g, g, g, g, g);
    for (int s = 0; s < ng; s++)
        for (int d = 0; d < ng; d++)
            fprintf(f, ",te_g%d_to_g%d", s, d);
    fprintf(f, "\n");
    for (int t = 0; t < mh->length; t++) {
        fprintf(f, "%d", t);
        for (int g = 0; g < ng; g++) {
            int base = t * ng + g;
            fprintf(f, ",%.6f,%.6f,%.6f,%d,%d",
                    mh->density[base], mh->entropy[base],
                    mh->activity[base], mh->gliders[base], mh->oscillators[base]);
        }
        for (int s = 0; s < ng; s++) {
            for (int d = 0; d < ng; d++) {
                int base = t * ng * ng + s * ng + d;
                fprintf(f, ",%.6f", mh->te_matrix[base]);
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}
