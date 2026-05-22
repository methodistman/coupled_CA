/* B4: Parallel scalability benchmark. Compares intent vs parallel pipeline. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/pipeline.h"
#include "../ca_core/intent.h"
#include "../utils/rng.h"

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static double bench_preset(int size, int ngrids, const char *preset) {
    System sys;
    sys_init(&sys, ngrids, size);
    for (int g = 0; g < ngrids; g++) {
        sys.grids[g]->active = 1;
        sys.grids[g]->rule_idx = 0;
    }
    /* Cross-chain coupling */
    for (int g = 0; g < ngrids - 1; g++) {
        coupling_connect(&sys.coupling, g, EDGE_RIGHT, g+1, EDGE_LEFT);
    }
    rng_seed(42);
    sys_randomize(&sys, rng_u64);

    Pipeline *p = NULL;
    if (!strcmp(preset, "intent"))
        p = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    else if (!strcmp(preset, "parallel"))
        p = pipeline_preset_parallel(INTENT_MODE_REPLACE, 0.0f);

    if (p) sys.pipeline = p;

    /* Warmup */
    for (int i = 0; i < 20; i++) sys_step(&sys);

    int steps = 500;
    double t0 = now();
    for (int i = 0; i < steps; i++) sys_step(&sys);
    double t1 = now();

    if (p) { sys.pipeline = NULL; pipeline_destroy(p); }
    sys_destroy(&sys);
    return (t1 - t0) * 1000.0;
}

int main(int argc, char **argv) {
    int sizes[] = {64, 128, 256};
    int nsizes = 3;
    int grids[] = {1, 2, 4};
    int ngrids_arr = 3;
    const char *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--output") && i+1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--size") && i+1 < argc) { sizes[0] = atoi(argv[++i]); nsizes = 1; }
        else if (!strcmp(argv[i], "--ngrids") && i+1 < argc) { grids[0] = atoi(argv[++i]); ngrids_arr = 1; }
    }

    FILE *f = out ? fopen(out, "w") : stdout;
    if (!f) return 1;
    fprintf(f, "size,ngrids,intent_ms,parallel_ms,speedup\n");

    for (int si = 0; si < nsizes; si++) {
        for (int gi = 0; gi < ngrids_arr; gi++) {
            double intent_ms = bench_preset(sizes[si], grids[gi], "intent");
            double parallel_ms = bench_preset(sizes[si], grids[gi], "parallel");
            double speedup = intent_ms / parallel_ms;
            fprintf(f, "%d,%d,%.3f,%.3f,%.3f\n",
                    sizes[si], grids[gi], intent_ms, parallel_ms, speedup);
            fflush(f);
        }
    }

    if (out) fclose(f);
    return 0;
}
