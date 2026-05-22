#ifndef ENGINE_H
#define ENGINE_H

#include "grid.h"
#include "bitgrid.h"
#include "coupling.h"
#include "rules.h"
#include "intent.h"

struct Pipeline; /* forward declaration from pipeline.h */
struct MetricsHistory; /* forward declaration from metrics/history.h */

typedef struct System {
    Grid *grids[MAX_GRIDS];
    BitGrid *bgrids[MAX_GRIDS]; /* optional fast path (size multiple of 64) */
    int use_bitgrid;            /* set to 1 to use bitgrid fast path */
    int num_grids;
    Coupling coupling;
    IntentBuffer *intent_buf;   /* captured edge intents for TE / delayed coupling */
    IntentRing *intent_ring;  /* temporal ring for delayed coupling history */
    struct Pipeline *pipeline;  /* optional tick pipeline (NULL = legacy sys_step) */
    struct MetricsHistory *metrics_history; /* per-tick time-series for ANALYZE phases */
} System;

void sys_init(System *s, int num_grids, int grid_size);
void sys_destroy(System *s);
void sys_step(System *s);
void sys_step_intent(System *s, IntentMode mode, float alpha);
void sys_step_intent_delayed(System *s, IntentMode mode, float alpha, int delay);
void sys_step_intent_delayed_genomic(System *s, IntentMode mode, float alpha, int delay);
void sys_randomize(System *s, uint64_t (*rng)(void));

/* Sync grid cells to/from bitgrid (for init / metrics) */
void sys_sync_to_bitgrid(System *s);
void sys_sync_from_bitgrid(System *s);

#endif
