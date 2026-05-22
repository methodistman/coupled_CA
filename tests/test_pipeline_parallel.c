#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ca_core/engine.h"
#include "ca_core/pipeline.h"
#include "metrics/history.h"
#include "utils/rng.h"

static int grid_equal(const Grid *a, const Grid *b) {
    if (!a || !b || a->size != b->size) return 0;
    return memcmp(a->cells, b->cells, a->size * a->size) == 0;
}

static void copy_grid_state(Grid *dst, const Grid *src) {
    if (!dst || !src || dst->size != src->size) return;
    memcpy(dst->cells, src->cells, dst->size * dst->size);
    memcpy(dst->next_cells, src->next_cells, dst->size * dst->size);
}

/* PIPELINE_PARALLEL must match PIPELINE_INTENT (sequential) when there are no schedules */
static void test_parallel_matches_intent(void) {
    System sys_seq, sys_par;
    sys_init(&sys_seq, 2, 16); sys_init(&sys_par, 2, 16);
    coupling_connect(&sys_seq.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    coupling_connect(&sys_par.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    sys_seq.grids[0]->active = 1; sys_seq.grids[1]->active = 1;
    sys_par.grids[0]->active = 1; sys_par.grids[1]->active = 1;
    sys_seq.grids[0]->rule_idx = 0; sys_seq.grids[1]->rule_idx = 0;
    sys_par.grids[0]->rule_idx = 0; sys_par.grids[1]->rule_idx = 0;

    sys_randomize(&sys_seq, rng_u64);
    sys_randomize(&sys_par, rng_u64);
    copy_grid_state(sys_par.grids[0], sys_seq.grids[0]);
    copy_grid_state(sys_par.grids[1], sys_seq.grids[1]);

    sys_seq.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    sys_par.pipeline = pipeline_preset_parallel(INTENT_MODE_REPLACE, 0.0f);
    if (!sys_seq.pipeline || !sys_par.pipeline) { printf("FAIL: preset NULL\n"); exit(1); }

    for (int t = 0; t < 50; t++) {
        sys_step(&sys_seq);
        sys_step(&sys_par);
        if (!grid_equal(sys_seq.grids[0], sys_par.grids[0]) ||
            !grid_equal(sys_seq.grids[1], sys_par.grids[1])) {
            printf("FAIL: parallel diverged at tick %d\n", t); exit(1);
        }
    }
    printf("PASS: PIPELINE_PARALLEL matches PIPELINE_INTENT for 50 ticks\n");
    sys_destroy(&sys_seq); sys_destroy(&sys_par);
}

/* PIPELINE_ANALYZE with metrics_history accumulates data */
static void test_analyze_history(void) {
    System sys;
    sys_init(&sys, 2, 16);
    coupling_connect(&sys.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    coupling_connect(&sys.coupling, 1, EDGE_LEFT, 0, EDGE_RIGHT);
    sys.grids[0]->active = 1; sys.grids[1]->active = 1;
    sys.grids[0]->rule_idx = 0; sys.grids[1]->rule_idx = 0;
    sys_randomize(&sys, rng_u64);
    sys.pipeline = pipeline_preset_analyze(INTENT_MODE_REPLACE, 0.0f);
    sys.metrics_history = metrics_history_create(2, 64);
    if (!sys.pipeline || !sys.metrics_history) { printf("FAIL: init failed\n"); exit(1); }

    for (int t = 0; t < 20; t++) sys_step(&sys);
    if (sys.metrics_history->length != 20) {
        printf("FAIL: history length %d != 20\n", sys.metrics_history->length); exit(1);
    }
    double d = metrics_history_density(sys.metrics_history, 10, 0);
    if (d < 0.0 || d > 1.0) {
        printf("FAIL: density out of range at tick 10\n"); exit(1);
    }
    printf("PASS: PIPELINE_ANALYZE accumulates %d ticks of history\n", sys.metrics_history->length);
    sys_destroy(&sys);
}

int main(void) {
    printf("=== Pipeline Parallel Tests ===\n");
    test_parallel_matches_intent();
    test_analyze_history();
    printf("=== All parallel pipeline tests passed ===\n");
    return 0;
}
