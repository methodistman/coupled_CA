/* D2: Pattern preservation under coupling. Inject a glider on grid 0 and
   track whether it survives and transmits across the coupling boundary. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/intent.h"
#include "../ca_core/patterns.h"
#include "../utils/rng.h"

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top")) return EDGE_TOP;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left")) return EDGE_LEFT;
    if (!strcmp(s, "right")) return EDGE_RIGHT;
    return -1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N       Grid size (default 64)\n");
    fprintf(stderr, "  --steps N      Ticks to run (default 200)\n");
    fprintf(stderr, "  --seed N       RNG seed (default 42)\n");
    fprintf(stderr, "  --rule IDX     Rule index (default 0 = Conway's Life)\n");
    fprintf(stderr, "  --connect SPEC Edge coupling (default 0:bottom->1:top)\n");
    fprintf(stderr, "  --intent       Use intent-buffer coupling\n");
    fprintf(stderr, "  --intent-mode MODE  replace, add, weighted, threshold\n");
    fprintf(stderr, "  --glider-x X   Glider start x (default 2)\n");
    fprintf(stderr, "  --glider-y Y   Glider start y (default size/2)\n");
    fprintf(stderr, "  --output FILE  CSV output (default stdout)\n");
    fprintf(stderr, "  --help\n");
}

int main(int argc, char **argv) {
    int size = 64, steps = 200, rule_idx = 0;
    int glider_x = 2, glider_y = -1;
    uint64_t seed = 42;
    int use_intent = 0;
    IntentMode intent_mode = INTENT_MODE_REPLACE;
    const char *out = NULL;
    const char *conn_spec = "0:bottom->1:top";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule") && i+1 < argc) rule_idx = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--connect") && i+1 < argc) conn_spec = argv[++i];
        else if (!strcmp(argv[i], "--intent")) use_intent = 1;
        else if (!strcmp(argv[i], "--intent-mode") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "replace")) intent_mode = INTENT_MODE_REPLACE;
            else if (!strcmp(m, "add")) intent_mode = INTENT_MODE_ADD;
            else if (!strcmp(m, "weighted")) intent_mode = INTENT_MODE_WEIGHTED;
            else if (!strcmp(m, "threshold")) intent_mode = INTENT_MODE_THRESHOLD;
        }
        else if (!strcmp(argv[i], "--glider-x") && i+1 < argc) glider_x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--glider-y") && i+1 < argc) glider_y = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--output") && i+1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }
    if (glider_y < 0) glider_y = size / 2;

    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, 2, size);
    for (int g = 0; g < 2; g++) {
        sys.grids[g]->active = 1;
        sys.grids[g]->rule_idx = rule_idx;
    }

    /* Parse connection spec */
    int src, dst;
    char src_edge[16], dst_edge[16];
    if (sscanf(conn_spec, "%d:%8[^-]->%d:%8s", &src, src_edge, &dst, dst_edge) == 4) {
        Edge se = parse_edge(src_edge);
        Edge de = parse_edge(dst_edge);
        if (se >= 0 && de >= 0) {
            coupling_connect(&sys.coupling, src, se, dst, de);
        }
    }

    rng_seed(seed);
    sys_randomize(&sys, rng_u64);

    /* Clear both grids and inject glider on grid 0 */
    for (int g = 0; g < 2; g++) {
        memset(sys.grids[g]->cells, 0, size * size * sizeof(uint8_t));
        memset(sys.grids[g]->next_cells, 0, size * size * sizeof(uint8_t));
    }
    pattern_glider(sys.grids[0], glider_x, glider_y);

    FILE *f = out ? fopen(out, "w") : stdout;
    if (!f) return 1;
    fprintf(f, "tick,g0_gliders,g1_gliders,g0_density,g1_density\n");

    int first_transmission = -1;
    for (int t = 0; t < steps; t++) {
        int g0 = census_gliders(sys.grids[0]);
        int g1 = census_gliders(sys.grids[1]);
        double d0 = 0, d1 = 0;
        int n = size * size;
        for (int i = 0; i < n; i++) { d0 += sys.grids[0]->cells[i]; d1 += sys.grids[1]->cells[i]; }
        d0 /= n; d1 /= n;
        fprintf(f, "%d,%d,%d,%.6f,%.6f\n", t, g0, g1, d0, d1);
        if (first_transmission < 0 && g1 > 0) first_transmission = t;
        if (use_intent)
            sys_step_intent(&sys, intent_mode, 0.0f);
        else
            sys_step(&sys);
    }

    if (out) fclose(f);
    sys_destroy(&sys);

    printf("# first_transmission_tick=%d (expected ~%d for SE glider)\n",
           first_transmission, (size - glider_x) / 2);
    if (first_transmission > 0)
        printf("# PASS: glider transmitted to grid 1 at tick %d\n", first_transmission);
    else
        printf("# FAIL: no glider detected on grid 1 within %d ticks\n", steps);
    return 0;
}
