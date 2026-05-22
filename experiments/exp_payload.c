#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../ca_core/payload_engine.h"
#include "../ca_core/payload_rules.h"
#include "../ca_core/payload_coupling.h"
#include "../utils/rng.h"

#define DEFAULT_STEPS 200
#define DEFAULT_SIZE  64
#define DEFAULT_GRIDS 2

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --grids N      Number of grids (1-%d, default %d)\n", MAX_PAYLOAD_GRIDS, DEFAULT_GRIDS);
    fprintf(stderr, "  --size N       Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --rule NAME    Rule name for all grids (default life_payload)\n");
    fprintf(stderr, "  --steps N      Steps to run (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --seed N       Random seed (default: time)\n");
    fprintf(stderr, "  --connect SPEC Add connection: SRC:EDGE->DST:EDGE,STRENGTH\n");
    fprintf(stderr, "  --help         Show this help\n");
}

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

static void compute_payload_metrics(const PayloadGrid *g, double *avg_payload, double *max_payload, double *alive_ratio) {
    int n = g->size * g->size;
    if (n == 0) { *avg_payload = 0; *max_payload = 0; *alive_ratio = 0; return; }
    long sum = 0;
    int alive = 0;
    uint8_t max = 0;
    for (int i = 0; i < n; i++) {
        sum += g->cells[i].payload;
        alive += g->cells[i].alive;
        if (g->cells[i].payload > max) max = g->cells[i].payload;
    }
    *avg_payload = (double)sum / n;
    *max_payload = max;
    *alive_ratio = (double)alive / n;
}

int main(int argc, char **argv) {
    int num_grids = DEFAULT_GRIDS;
    int size = DEFAULT_SIZE;
    int rule_idx = 0; /* life_payload */
    int steps = DEFAULT_STEPS;
    uint64_t seed = 0; int seed_set = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--grids") && i+1 < argc) num_grids = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule") && i+1 < argc) {
            rule_idx = payload_rules_index_by_name(argv[++i]);
            if (rule_idx < 0) { fprintf(stderr, "Unknown rule: %s\n", argv[i]); return 1; }
        }
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (num_grids < 1 || num_grids > MAX_PAYLOAD_GRIDS) {
        fprintf(stderr, "Error: grids must be 1-%d\n", MAX_PAYLOAD_GRIDS);
        return 1;
    }
    if (size < 1 || size > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Error: size must be 1-%d\n", MAX_PAYLOAD_SIZE);
        return 1;
    }

    PayloadSystem sys;
    memset(&sys, 0, sizeof(sys));
    payload_sys_init(&sys, num_grids, size);
    for (int g = 0; g < num_grids; g++) sys.grids[g]->rule_idx = rule_idx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--connect") && i+1 < argc) {
            char *spec = argv[++i];
            int src, dst; float strength = 1.0f;
            char src_edge[16], dst_edge[16];
            if (sscanf(spec, "%d:%8[^-]->%d:%8[^,],%f", &src, src_edge, &dst, dst_edge, &strength) >= 4) {
                Edge se = parse_edge(src_edge);
                Edge de = parse_edge(dst_edge);
                if (se >= 0 && de >= 0 && src < num_grids && dst < num_grids)
                    payload_coupling_connect(&sys.coupling, src, se, dst, de, strength);
            }
        }
    }

    if (!seed_set) seed = (uint64_t)time(NULL);
    rng_seed(seed);
    payload_sys_randomize(&sys, rng_u64);

    printf("# seed=%llu rule=%s\n", (unsigned long long)seed, PAYLOAD_RULE_TABLE[rule_idx].name);
    printf("step");
    for (int g = 0; g < num_grids; g++) {
        printf(",grid%d_alive,grid%d_avg_payload,grid%d_max_payload", g, g, g);
    }
    printf("\n");

    for (int step = 0; step <= steps; step++) {
        printf("%d", step);
        for (int g = 0; g < num_grids; g++) {
            double avg, max, alive;
            compute_payload_metrics(sys.grids[g], &avg, &max, &alive);
            printf(",%.4f,%.2f,%.0f", alive, avg, max);
        }
        printf("\n");
        if (step < steps) payload_sys_step(&sys);
    }

    payload_sys_destroy(&sys);
    return 0;
}
