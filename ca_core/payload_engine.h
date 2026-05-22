#ifndef PAYLOAD_ENGINE_H
#define PAYLOAD_ENGINE_H

#include "payload_grid.h"
#include "payload_coupling.h"
#include "payload_rules.h"
#include "payload_intent.h"
#include "intent.h"

struct PayloadMetricsHistory;
struct PayloadPipeline;

typedef struct PayloadSystem {
    PayloadGrid *grids[MAX_PAYLOAD_GRIDS];
    int num_grids;
    PayloadCoupling coupling;
    PayloadIntentBuffer *intent_buf;      /* captured edge intents for TE / delayed coupling */
    PayloadIntentRing *intent_ring;       /* temporal ring for delayed coupling history */
    struct PayloadMetricsHistory *metrics_history; /* per-tick time-series for ANALYZE phases */
    struct PayloadPipeline *pipeline;     /* optional tick pipeline (NULL = legacy payload_sys_step) */
} PayloadSystem;

void payload_sys_init(PayloadSystem *s, int num_grids, int grid_size);
void payload_sys_destroy(PayloadSystem *s);
void payload_sys_step(PayloadSystem *s);
void payload_sys_step_intent(PayloadSystem *s, IntentMode mode, float alpha);
void payload_sys_randomize(PayloadSystem *s, uint64_t (*rng)(void));

#endif
