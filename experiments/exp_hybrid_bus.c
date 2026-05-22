#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../ca_core/hybrid_engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_rules.h"
#include "../utils/rng.h"

#define DEFAULT_STEPS 200
#define DEFAULT_SIZE  64

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N       Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N      Steps to run (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --seed N       Random seed (default: time)\n");
    fprintf(stderr, "  --brule NAME   Binary rule name (default Conway's Life)\n");
    fprintf(stderr, "  --prule NAME   Payload rule name (default identity)\n");
    fprintf(stderr, "  --xconnect SPEC Cross-type: B0:EDGE->P0:EDGE (binary->payload)\n");
    fprintf(stderr, "                 or P0:EDGE->B0:EDGE (payload->binary)\n");
    fprintf(stderr, "  --help         Show this help\n");
}

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

static void compute_binary_metrics(const Grid *g, double *alive_ratio, double *avg) {
    int n = g->size * g->size;
    if (n == 0) { *alive_ratio = 0; *avg = 0; return; }
    int alive = 0;
    long sum = 0;
    for (int i = 0; i < n; i++) {
        alive += g->cells[i];
        sum += g->cells[i];
    }
    *alive_ratio = (double)alive / n;
    *avg = (double)sum / n;
}

static void compute_payload_metrics(const PayloadGrid *g, double *alive_ratio, double *avg_payload, double *max_payload) {
    int n = g->size * g->size;
    if (n == 0) { *alive_ratio = 0; *avg_payload = 0; *max_payload = 0; return; }
    long sum = 0;
    int alive = 0;
    uint8_t max = 0;
    for (int i = 0; i < n; i++) {
        sum += g->cells[i].payload;
        alive += g->cells[i].alive;
        if (g->cells[i].payload > max) max = g->cells[i].payload;
    }
    *alive_ratio = (double)alive / n;
    *avg_payload = (double)sum / n;
    *max_payload = max;
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    uint64_t seed = 0; int seed_set = 0;
    int brule_idx = 0; /* Conway's Life */
    int prule_idx = payload_rules_index_by_name("identity");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--brule") && i+1 < argc) {
            brule_idx = rules_index_by_name(argv[++i]);
            if (brule_idx < 0) { fprintf(stderr, "Unknown binary rule: %s\n", argv[i]); return 1; }
        }
        else if (!strcmp(argv[i], "--prule") && i+1 < argc) {
            prule_idx = payload_rules_index_by_name(argv[++i]);
            if (prule_idx < 0) { fprintf(stderr, "Unknown payload rule: %s\n", argv[i]); return 1; }
        }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (size < 1 || size > MAX_GRID_SIZE) {
        fprintf(stderr, "Error: size must be 1-%d\n", MAX_GRID_SIZE);
        return 1;
    }

    HybridSystem sys;
    hybrid_sys_init(&sys, 1, 1, 0, size);
    sys.binary.grids[0]->rule_idx = brule_idx;
    sys.payload.grids[0]->rule_idx = prule_idx;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--xconnect") && i+1 < argc) {
            char *spec = argv[++i];
            char src_type[2], dst_type[2];
            int src_grid, dst_grid;
            char src_edge[16], dst_edge[16];
            /* Parse format like B0:bottom->P0:top or P0:left->B0:right */
            if (sscanf(spec, "%1[BP]%d:%8[^-]->%1[BP]%d:%8s",
                       src_type, &src_grid, src_edge, dst_type, &dst_grid, dst_edge) == 6) {
                Edge se = parse_edge(src_edge);
                Edge de = parse_edge(dst_edge);
                HybridGridType st = (src_type[0] == 'B') ? HYBRID_BINARY : HYBRID_PAYLOAD;
                HybridGridType dt = (dst_type[0] == 'B') ? HYBRID_BINARY : HYBRID_PAYLOAD;
                if (se >= 0 && de >= 0 && src_grid < ((st == HYBRID_BINARY) ? sys.binary.num_grids : sys.payload.num_grids)
                    && dst_grid < ((dt == HYBRID_BINARY) ? sys.binary.num_grids : sys.payload.num_grids)) {
                    hybrid_xconn_add(&sys, st, src_grid, se, dt, dst_grid, de);
                } else {
                    fprintf(stderr, "Warning: invalid xconnect spec: %s\n", spec);
                }
            } else {
                fprintf(stderr, "Warning: could not parse xconnect spec: %s\n", spec);
            }
        }
    }

    if (!seed_set) seed = (uint64_t)time(NULL);
    rng_seed(seed);
    hybrid_sys_randomize(&sys, rng_u64);

    printf("# seed=%llu brule=%s prule=%s\n",
           (unsigned long long)seed,
           RULE_TABLE[brule_idx].name,
           PAYLOAD_RULE_TABLE[prule_idx].name);
    printf("step,b_alive,b_avg,p_alive,p_avg_payload,p_max_payload\n");

    for (int step = 0; step <= steps; step++) {
        double b_alive, b_avg, p_alive, p_avg, p_max;
        compute_binary_metrics(sys.binary.grids[0], &b_alive, &b_avg);
        compute_payload_metrics(sys.payload.grids[0], &p_alive, &p_avg, &p_max);
        printf("%d,%.4f,%.2f,%.4f,%.2f,%.0f\n",
               step, b_alive, b_avg, p_alive, p_avg, p_max);
        if (step < steps) hybrid_sys_step(&sys);
    }

    hybrid_sys_destroy(&sys);
    return 0;
}
