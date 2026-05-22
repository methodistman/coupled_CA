#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/hybrid_engine.h"
#include "../ca_core/coupling.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_rules.h"
#include "../utils/rng.h"

#define DEFAULT_SIZE 64
#define DEFAULT_STEPS 100

static Edge edge_from_string(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

static void seed_glider_binary(Grid *g, int x, int y) {
    static const int pat[5][2] = {{1,0},{2,1},{0,2},{1,2},{2,2}};
    for (int i = 0; i < 5; i++) {
        int cx = x + pat[i][0];
        int cy = y + pat[i][1];
        if (cx >= 0 && cx < g->size && cy >= 0 && cy < g->size)
            grid_set(g, cx, cy, 1);
    }
}

static void seed_glider_payload(PayloadGrid *g, int x, int y, uint8_t payload_val) {
    static const int pat[5][2] = {{1,0},{2,1},{0,2},{1,2},{2,2}};
    for (int i = 0; i < 5; i++) {
        int cx = x + pat[i][0];
        int cy = y + pat[i][1];
        if (cx >= 0 && cx < g->size && cy >= 0 && cy < g->size)
            g->cells[cy * g->size + cx] = (Cell){1, payload_val};
    }
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N       Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N      Steps to run (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --seed N       Random seed (default: time)\n");
    fprintf(stderr, "  --payload V    Payload value for glider cells (default 200)\n");
    fprintf(stderr, "  --xconnect SPEC Cross-type edge coupling (default none)\n");
    fprintf(stderr, "  --help         Show this help\n");
}

typedef struct {
    double binary_mass;
    double payload_mass;
    double payload_alive;
    double overlap;   /* cells where both binary and payload are alive */
} GliderMetrics;

static void compute_glider_metrics(const HybridSystem *hs, int cx, int cy, int window, GliderMetrics *m) {
    int sz = hs->binary.grids[0]->size;
    int half = window / 2;
    m->binary_mass = 0;
    m->payload_mass = 0;
    m->payload_alive = 0;
    m->overlap = 0;
    int count = 0;
    for (int dy = -half; dy <= half; dy++) {
        for (int dx = -half; dx <= half; dx++) {
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= sz || y < 0 || y >= sz) continue;
            int b = hs->binary.grids[0]->cells[y * sz + x];
            Cell p = hs->payload.grids[0]->cells[y * sz + x];
            m->binary_mass += b;
            m->payload_mass += p.payload;
            m->payload_alive += p.alive;
            if (b && p.alive) m->overlap++;
            count++;
        }
    }
    if (count > 0) {
        m->binary_mass /= count;
        m->payload_mass /= count;
        m->payload_alive /= count;
        m->overlap /= count;
    }
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    uint64_t seed = (uint64_t)time(NULL);
    int payload_val = 200;
    int has_xconn = 0;
    HybridGridType xconn_st = HYBRID_BINARY, xconn_dt = HYBRID_BINARY;
    int xconn_sg = 0, xconn_dg = 0;
    Edge xconn_se = EDGE_TOP, xconn_de = EDGE_TOP;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--payload") && i + 1 < argc) payload_val = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--xconnect") && i + 1 < argc) {
            const char *spec = argv[++i];
            char src_t, dst_t;
            char src_e[16], dst_e[16];
            if (sscanf(spec, "%c%d:%15[^-]->%c%d:%15s",
                       &src_t, &xconn_sg, src_e, &dst_t, &xconn_dg, dst_e) == 6) {
                xconn_st = (src_t == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
                xconn_dt = (dst_t == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
                xconn_se = edge_from_string(src_e);
                xconn_de = edge_from_string(dst_e);
                has_xconn = 1;
            }
        }
    }

    rng_seed(seed);
    HybridSystem hs;
    hybrid_sys_init(&hs, 1, 1, 0, size);

    hs.binary.grids[0]->rule_idx = rules_index_by_name("Conway's Life");
    hs.payload.grids[0]->rule_idx = payload_rules_index_by_name("life_payload");

    int cx = size / 2;
    int cy = size / 2;
    seed_glider_binary(hs.binary.grids[0], cx, cy);
    seed_glider_payload(hs.payload.grids[0], cx, cy, (uint8_t)payload_val);

    if (has_xconn)
        hybrid_xconn_add(&hs, xconn_st, xconn_sg, xconn_se, xconn_dt, xconn_dg, xconn_de);

    printf("# H3: Payload survival on moving glider\n");
    printf("# size=%d steps=%d seed=%lu payload_val=%d\n", size, steps, (unsigned long)seed, payload_val);
    printf("step,binary_mass,payload_mass,payload_alive,overlap\n");

    for (int step = 0; step <= steps; step++) {
        GliderMetrics m;
        compute_glider_metrics(&hs, cx, cy, 12, &m);
        printf("%d,%.4f,%.4f,%.4f,%.4f\n", step,
               m.binary_mass, m.payload_mass, m.payload_alive, m.overlap);
        if (step < steps)
            hybrid_sys_step(&hs);
    }

    hybrid_sys_destroy(&hs);
    return 0;
}
