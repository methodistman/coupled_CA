#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/coupling.h"
#include "../utils/rng.h"
#include "../metrics/metrics.h"
#include "../metrics/export.h"

#define DEFAULT_STEPS 500
#define DEFAULT_SIZE  64

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N  --rule IDX  --steps N  --seed N\n");
    fprintf(stderr, "  --loop GRID:EDGE->GRID:EDGE  (repeatable)\n");
}

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE, rule_idx = 0, steps = DEFAULT_STEPS;
    uint64_t seed = 0; int seed_set = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule") && i+1 < argc) rule_idx = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (rule_idx < 0 || rule_idx >= NUM_RULES) return 1;

    /* Memory loop: two grids forming a bidirectional ring */
    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, 2, size);
    for (int g = 0; g < 2; g++) sys.grids[g]->rule_idx = rule_idx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--loop") && i+1 < argc) {
            int a, b; char ae[16], be[16];
            if (sscanf(argv[++i], "%d:%8[^-]->%d:%8s", &a, ae, &b, be) == 4) {
                Edge x = parse_edge(ae), y = parse_edge(be);
                if (x >= 0 && y >= 0) coupling_connect(&sys.coupling, a, x, b, y);
            }
        }
    }

    /* Default loop: 0:bottom->1:top and 1:bottom->0:top */
    if (sys.coupling.conn[0][EDGE_BOTTOM].target_grid < 0)
        coupling_connect(&sys.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    if (sys.coupling.conn[1][EDGE_BOTTOM].target_grid < 0)
        coupling_connect(&sys.coupling, 1, EDGE_BOTTOM, 0, EDGE_TOP);

    if (!seed_set) seed = (uint64_t)time(NULL);
    rng_seed(seed);
    sys_randomize(&sys, rng_u64);

    printf("# seed=%llu rule=%s loop=2grid\n", (unsigned long long)seed, RULE_TABLE[rule_idx].name);
    export_csv_header(stdout, 2);

    Grid **prev = calloc(2, sizeof(Grid*));
    for (int g = 0; g < 2; g++) {
        prev[g] = grid_create(size);
        grid_copy(sys.grids[g], prev[g]);
    }

    for (int step = 0; step <= steps; step++) {
        GridMetrics m[2];
        for (int g = 0; g < 2; g++)
            metrics_compute(sys.grids[g], prev[g], &m[g]);
        export_csv_row(stdout, step, m, 2);

        if (step < steps) {
            for (int g = 0; g < 2; g++) grid_copy(sys.grids[g], prev[g]);
            sys_step(&sys);
        }
    }

    for (int g = 0; g < 2; g++) grid_destroy(prev[g]);
    free(prev);
    sys_destroy(&sys);
    return 0;
}
