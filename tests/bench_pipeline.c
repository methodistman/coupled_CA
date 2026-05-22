#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include "ca_core/engine.h"
#include "ca_core/pipeline.h"
#include "utils/rng.h"

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    const int size = 128;
    const int steps = 500;
    const int num_grids = 2;

    System sys_leg, sys_pipe;
    sys_init(&sys_leg, num_grids, size);
    sys_init(&sys_pipe, num_grids, size);
    coupling_connect(&sys_leg.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    coupling_connect(&sys_pipe.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    for (int g = 0; g < num_grids; g++) {
        sys_leg.grids[g]->active = 1;
        sys_pipe.grids[g]->active = 1;
        sys_leg.grids[g]->rule_idx = 0;
        sys_pipe.grids[g]->rule_idx = 0;
    }
    rng_seed(42);
    sys_randomize(&sys_leg, rng_u64);
    rng_seed(42);
    sys_randomize(&sys_pipe, rng_u64);

    sys_pipe.pipeline = pipeline_preset_default();

    /* Warmup */
    for (int i = 0; i < 50; i++) {
        sys_step(&sys_leg);
        sys_step(&sys_pipe);
    }

    double t0 = now();
    for (int i = 0; i < steps; i++) sys_step(&sys_leg);
    double t1 = now();
    for (int i = 0; i < steps; i++) sys_step(&sys_pipe);
    double t2 = now();

    double leg_ms = (t1 - t0) * 1000.0;
    double pipe_ms = (t2 - t1) * 1000.0;
    double overhead = (pipe_ms / leg_ms - 1.0) * 100.0;

    printf("legacy:   %.3f ms (%d steps)\n", leg_ms, steps);
    printf("pipeline: %.3f ms (%d steps)\n", pipe_ms, steps);
    printf("overhead: %.2f%%\n", overhead);
    printf("%s\n", overhead <= 5.0 ? "PASS: within 5%" : "FAIL: exceeds 5%");

    sys_destroy(&sys_leg);
    sys_destroy(&sys_pipe);
    return (overhead <= 5.0) ? 0 : 1;
}
