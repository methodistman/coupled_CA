#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/coupling.h"
#include "../utils/rng.h"

#define DEF_STEPS  200
#define DEF_SIZE   32
#define DEF_TRIALS 10

typedef struct { int t; const char *n; } Gate;
static const Gate GATES[] = {
    {0x0,"FALSE"},{0x1,"AND"},{0x2,"A_AND_NOT_B"},{0x3,"A"},
    {0x4,"NOT_A_AND_B"},{0x5,"B"},{0x6,"XOR"},{0x7,"OR"},
    {0x8,"NOR"},{0x9,"XNOR"},{0xA,"NOT_B"},{0xB,"A_OR_NOT_B"},
    {0xC,"NOT_A"},{0xD,"NOT_A_OR_B"},{0xE,"NAND"},{0xF,"TRUE"},
};
static const char *gname(int t) {
    for (size_t i = 0; i < sizeof(GATES)/sizeof(GATES[0]); i++)
        if (GATES[i].t == t) return GATES[i].n;
    return "?";
}

static Edge parse(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

/* Set left half of grid as input A, right half as input B */
static void set_in(Grid *g, int a, int b) {
    int sz = g->size, half = sz / 2;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < half; x++) grid_set(g, x, y, a);
        for (int x = half; x < sz; x++) grid_set(g, x, y, b);
    }
}

/* Read output from rightmost column of grid 0 */
static int read_out(const Grid *g) {
    int sz = g->size, half = sz / 2;
    int c = 0;
    for (int y = 0; y < half; y++) c += grid_get(g, sz - 1, y);
    return (c > half / 2) ? 1 : 0;
}

/* One truth table trial: returns 4-bit truth table */
static int trial(System *s, int steps, Grid **bk) {
    int truth = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            grid_copy(bk[0], s->grids[0]);
            grid_copy(bk[1], s->grids[1]);
            for (int st = 0; st < steps; st++) {
                sys_step(s);
                set_in(s->grids[1], a, b);
            }
            int o = read_out(s->grids[0]);
            truth |= (o << ((a << 1) | b));
        }
    }
    return truth;
}

int main(int argc, char **argv) {
    int r0 = 0, r1 = 0, sz = DEF_SIZE, steps = DEF_STEPS, trials = DEF_TRIALS;
    uint64_t seed = 0; int seed_set = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rule0") && i+1 < argc) r0 = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule1") && i+1 < argc) r1 = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i+1 < argc) sz = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trials") && i+1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--help")) {
            printf("Usage: %s [--rule0 N] [--rule1 N] [--size N] [--steps N] [--trials N] [--seed N] [--connect SPEC]\n", argv[0]);
            return 0;
        }
    }

    if (r0 < 0 || r0 >= NUM_RULES || r1 < 0 || r1 >= NUM_RULES) {
        fprintf(stderr, "bad rule index\n"); return 1;
    }

    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, 2, sz);
    sys.grids[0]->rule_idx = r0;
    sys.grids[1]->rule_idx = r1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--connect") && i+1 < argc) {
            int s, d; char se[16], de[16];
            if (sscanf(argv[++i], "%d:%8[^-]->%d:%8s", &s, se, &d, de) == 4) {
                Edge a = parse(se), b = parse(de);
                if (a >= 0 && b >= 0) coupling_connect(&sys.coupling, s, a, d, b);
            }
        }
    }

    if (!seed_set) seed = (uint64_t)time(NULL);
    rng_seed(seed);

    int votes[16] = {0};
    Grid *bk[2] = { grid_create(sz), grid_create(sz) };

    for (int t = 0; t < trials; t++) {
        sys_randomize(&sys, rng_u64);
        grid_copy(sys.grids[0], bk[0]);
        grid_copy(sys.grids[1], bk[1]);
        int truth = trial(&sys, steps, bk);
        if (truth >= 0 && truth < 16) votes[truth]++;
    }

    int best = 0, bv = 0;
    for (int i = 0; i < 16; i++)
        if (votes[i] > bv) { bv = votes[i]; best = i; }

    /* Build connection summary string */
    char conn_str[256] = "none";
    int cp = 0;
    for (int g = 0; g < 2; g++) {
        for (int e = 0; e < 4; e++) {
            if (sys.coupling.conn[g][e].target_grid >= 0) {
                if (cp > 0) cp += snprintf(conn_str + cp, sizeof(conn_str) - cp, ";");
                const char *en[] = {"top","right","bottom","left"};
                cp += snprintf(conn_str + cp, sizeof(conn_str) - cp, "%d:%s->%d:%s",
                    g, en[e], sys.coupling.conn[g][e].target_grid,
                    en[sys.coupling.conn[g][e].target_edge]);
            }
        }
    }

    printf("rule0,rule1,size,steps,trials,seed,truth_table,gate_name,confidence,connections\n");
    printf("%d,%d,%d,%d,%d,%llu,0x%X,%s,%.2f,%s\n",
           r0, r1, sz, steps, trials,
           (unsigned long long)seed,
           best, gname(best),
           (double)bv / trials,
           conn_str);

    grid_destroy(bk[0]);
    grid_destroy(bk[1]);
    sys_destroy(&sys);
    return 0;
}
