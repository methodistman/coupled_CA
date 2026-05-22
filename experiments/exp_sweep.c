#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/coupling.h"
#include "../utils/rng.h"
#include "../metrics/metrics.h"
#include "../metrics/glossary.h"

#define DEF_STEPS  200
#define DEF_SIZE   32
#define DEF_TRIALS 5

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N      Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --steps N     Steps per trial (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --trials N    Trials per config (default %d)\n", DEF_TRIALS);
    fprintf(stderr, "  --seed N      Base random seed (default: time)\n");
    fprintf(stderr, "  --rules RANGE Rule indices to sweep, e.g. 0-3 or 0,2,4 (default all)\n");
    fprintf(stderr, "  --topos LIST  Connection topologies: none,uni,bi,ring (default all)\n");
}

typedef enum { TOPO_NONE, TOPO_UNI, TOPO_BI, TOPO_RING, TOPO_COUNT } Topo;

static const char *topo_name(Topo t) {
    const char *names[] = {"none","uni","bi","ring"};
    return (t < TOPO_COUNT) ? names[t] : "?";
}

static void apply_topo(System *s, Topo t) {
    coupling_init(&s->coupling, s->num_grids);
    switch (t) {
    case TOPO_UNI:
        coupling_connect(&s->coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
        break;
    case TOPO_BI:
        coupling_connect(&s->coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
        coupling_connect(&s->coupling, 1, EDGE_BOTTOM, 0, EDGE_TOP);
        break;
    case TOPO_RING:
        if (s->num_grids >= 3) {
            for (int g = 0; g < s->num_grids; g++)
                coupling_connect(&s->coupling, g, EDGE_RIGHT, (g+1)%s->num_grids, EDGE_LEFT);
        }
        break;
    default: break;
    }
}

static int parse_rules(const char *s, int *out, int max) {
    int n = 0;
    if (strchr(s, '-')) {
        int a, b;
        if (sscanf(s, "%d-%d", &a, &b) == 2) {
            for (int i = a; i <= b && n < max; i++) out[n++] = i;
        }
    } else {
        const char *p = s;
        while (*p && n < max) {
            int r = atoi(p);
            if (r >= 0 && r < NUM_RULES) out[n++] = r;
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        }
    }
    return n;
}

static int parse_topos(const char *s, Topo *out, int max) {
    int n = 0;
    const char *p = s;
    while (*p && n < max) {
        if (!strncmp(p, "none", 4)) out[n++] = TOPO_NONE;
        else if (!strncmp(p, "uni", 3)) out[n++] = TOPO_UNI;
        else if (!strncmp(p, "bi", 2)) out[n++] = TOPO_BI;
        else if (!strncmp(p, "ring", 4)) out[n++] = TOPO_RING;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return n;
}

int main(int argc, char **argv) {
    int size = DEF_SIZE, steps = DEF_STEPS, trials = DEF_TRIALS;
    uint64_t seed = 0; int seed_set = 0;
    int rule_list[16], nrules = 0;
    Topo topo_list[4];
    int ntopos = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trials") && i+1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = strtoull(argv[++i], NULL, 10); seed_set = 1; }
        else if (!strcmp(argv[i], "--rules") && i+1 < argc) nrules = parse_rules(argv[++i], rule_list, 16);
        else if (!strcmp(argv[i], "--topos") && i+1 < argc) ntopos = parse_topos(argv[++i], topo_list, 4);
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (!nrules) { for (int i = 0; i < NUM_RULES && i < 16; i++) rule_list[nrules++] = i; }
    if (!ntopos) { topo_list[ntopos++] = TOPO_NONE; topo_list[ntopos++] = TOPO_UNI;
                   topo_list[ntopos++] = TOPO_BI; topo_list[ntopos++] = TOPO_RING; }
    if (!seed_set) seed = (uint64_t)time(NULL);

    GlossaryDB *gloss = glossary_create(64);

    printf("# sweep seed=%llu size=%d steps=%d trials=%d\n",
           (unsigned long long)seed, size, steps, trials);
    printf("rule0,rule1,size,topology,trials,mean_density0,std_density0,mean_activity0,std_activity0,period_detected_rate0,mean_density1,std_density1,mean_activity1,std_activity1,period_detected_rate1\n");

    for (int ri = 0; ri < nrules; ri++) {
        for (int rj = 0; rj < nrules; rj++) {
            int r0 = rule_list[ri];
            int r1 = rule_list[rj];
            for (int ti = 0; ti < ntopos; ti++) {
                Topo topo = topo_list[ti];
                int ngrids = (topo == TOPO_RING) ? 3 : 2;

                double sum_d[3] = {0}, sum_a[3] = {0}, sum_e[3] = {0};
                double sum_d2[3] = {0}, sum_a2[3] = {0};
                int periods[3] = {0};

                for (int t = 0; t < trials; t++) {
                    uint64_t trial_seed = seed + (uint64_t)(ri * nrules * ntopos * trials +
                                                           rj * ntopos * trials +
                                                           ti * trials + t);
                    rng_seed(trial_seed);

                    System sys;
                    memset(&sys, 0, sizeof(sys));
                    sys_init(&sys, ngrids, size);
                    for (int g = 0; g < ngrids; g++)
                        sys.grids[g]->rule_idx = (g == 0) ? r0 : r1;
                    apply_topo(&sys, topo);
                    sys_randomize(&sys, rng_u64);

                    PeriodRing *pr[3] = {0};
                    Grid *prev[3] = {0};
                    for (int g = 0; g < ngrids; g++) {
                        pr[g] = period_ring_create(steps + 1);
                        prev[g] = grid_create(size);
                        grid_copy(sys.grids[g], prev[g]);
                    }

                    for (int step = 0; step < steps; step++) {
                        for (int g = 0; g < ngrids; g++) {
                            GridMetrics m;
                            metrics_compute(sys.grids[g], prev[g], &m);
                            period_ring_push(pr[g], grid_hash(sys.grids[g]));
                        }
                        for (int g = 0; g < ngrids; g++)
                            grid_copy(sys.grids[g], prev[g]);
                        sys_step(&sys);
                    }

                    for (int g = 0; g < ngrids; g++) {
                        GridMetrics m;
                        metrics_compute(sys.grids[g], prev[g], &m);
                        sum_d[g] += m.density;
                        sum_d2[g] += m.density * m.density;
                        sum_a[g] += m.activity;
                        sum_a2[g] += m.activity * m.activity;
                        sum_e[g] += m.entropy;
                        if (period_ring_detect(pr[g]) > 0) periods[g]++;
                        period_ring_destroy(pr[g]);
                        grid_destroy(prev[g]);
                    }
                    sys_destroy(&sys);
                }

                printf("%s,%s,%d,%s,%d",
                       RULE_TABLE[r0].name, RULE_TABLE[r1].name,
                       size, topo_name(topo), trials);
                for (int g = 0; g < 2; g++) {
                    double mean_d = sum_d[g] / trials;
                    double std_d = sqrt(sum_d2[g]/trials - mean_d*mean_d);
                    double mean_a = sum_a[g] / trials;
                    double std_a = sqrt(sum_a2[g]/trials - mean_a*mean_a);
                    double prate = (double)periods[g] / trials;
                    printf(",%.6f,%.6f,%.6f,%.6f,%.2f", mean_d, std_d, mean_a, std_a, prate);
                }
                printf("\n");

                /* Populate glossary entry for this (rule0, rule1, topology) config */
                GlossaryEntry ge;
                memset(&ge, 0, sizeof(ge));
                snprintf(ge.name, GLOSSARY_NAME_LEN, "%s+%s_%s",
                         RULE_TABLE[r0].name, RULE_TABLE[r1].name, topo_name(topo));
                snprintf(ge.desc, GLOSSARY_DESC_LEN,
                         "Sweep: %s + %s, topology=%s, size=%d",
                         RULE_TABLE[r0].name, RULE_TABLE[r1].name, topo_name(topo), size);
                ge.n_rules = 2;
                snprintf(ge.rule_names[0], GLOSSARY_RULE_LEN, "%s", RULE_TABLE[r0].name);
                snprintf(ge.rule_names[1], GLOSSARY_RULE_LEN, "%s", RULE_TABLE[r1].name);
                snprintf(ge.mode, GLOSSARY_RULE_LEN, "sweep");
                ge.interval = 0;
                ge.sample_steps = steps;
                ge.mean_density  = sum_d[0] / trials;
                ge.mean_activity = sum_a[0] / trials;
                ge.mean_entropy  = sum_e[0] / trials;
                double prate = (double)periods[0] / trials;
                ge.emergence_score = (1.0 - prate) * ge.mean_activity;
                glossary_add(gloss, &ge);
            }
        }
    }

    glossary_export_md(gloss, "glossary_sweep.md");
    fprintf(stderr, "# Glossary exported to glossary_sweep.md (%d entries)\n", gloss->n_entries);
    glossary_destroy(gloss);
    return 0;
}
