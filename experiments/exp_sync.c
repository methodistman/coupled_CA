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
#define DEFAULT_GRIDS 2

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --grids N  --size N  --rule IDX  --steps N  --seed N\n");
    fprintf(stderr, "  --connect SPEC  (SRC:EDGE->DST:EDGE)\n");
}

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

int main(int argc, char **argv) {
    int num_grids = DEFAULT_GRIDS, size = DEFAULT_SIZE;
    int rule_idx = 0, steps = DEFAULT_STEPS;
    uint64_t seed = 0; int seed_set = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--grids") && i+1 < argc) num_grids = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule") && i+1 < argc) rule_idx = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (num_grids < 1 || num_grids > MAX_GRIDS) return 1;
    if (rule_idx < 0 || rule_idx >= NUM_RULES) return 1;

    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, num_grids, size);
    for (int g = 0; g < num_grids; g++) sys.grids[g]->rule_idx = rule_idx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--connect") && i+1 < argc) {
            int s, d; char se[16], de[16];
            if (sscanf(argv[++i], "%d:%8[^-]->%d:%8s", &s, se, &d, de) == 4) {
                Edge a = parse_edge(se), b = parse_edge(de);
                if (a >= 0 && b >= 0) coupling_connect(&sys.coupling, s, a, d, b);
            }
        }
    }

    if (!seed_set) seed = (uint64_t)time(NULL);
    rng_seed(seed);
    sys_randomize(&sys, rng_u64);

    printf("# seed=%llu rule=%s\n", (unsigned long long)seed, RULE_TABLE[rule_idx].name);
    export_csv_header(stdout, num_grids);

    PeriodRing **rings = calloc(num_grids, sizeof(PeriodRing*));
    Grid **prev = calloc(num_grids, sizeof(Grid*));
    for (int g = 0; g < num_grids; g++) {
        rings[g] = period_ring_create(steps + 1);
        prev[g] = grid_create(size);
        grid_copy(sys.grids[g], prev[g]);
    }

    for (int step = 0; step <= steps; step++) {
        GridMetrics m[MAX_GRIDS];
        for (int g = 0; g < num_grids; g++) {
            metrics_compute(sys.grids[g], prev[g], &m[g]);
            period_ring_push(rings[g], grid_hash(sys.grids[g]));
            m[g].detected_period = period_ring_detect(rings[g]);
        }
        export_csv_row(stdout, step, m, num_grids);

        if (step < steps) {
            for (int g = 0; g < num_grids; g++) grid_copy(sys.grids[g], prev[g]);
            sys_step(&sys);
        }
    }

    for (int g = 0; g < num_grids; g++) {
        period_ring_destroy(rings[g]);
        grid_destroy(prev[g]);
    }
    free(rings); free(prev);
    sys_destroy(&sys);
    return 0;
}
