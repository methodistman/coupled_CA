/* exp_convergence: Per-tick convergence dynamics of the local GA. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ca_core/engine.h"
#include "../ca_core/pipeline.h"
#include "../ca_core/genome.h"
#include "../ca_core/local_ga.h"
#include "../utils/rng.h"

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top")) return EDGE_TOP;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left")) return EDGE_LEFT;
    if (!strcmp(s, "right")) return EDGE_RIGHT;
    return EDGE_RIGHT;
}
static int parse_fitness(const char *s) {
    if (!strcmp(s, "stability")) return FITNESS_STABILITY;
    if (!strcmp(s, "activity")) return FITNESS_ACTIVITY;
    if (!strcmp(s, "density")) return FITNESS_DENSITY;
    if (!strcmp(s, "edge")) return FITNESS_EDGE_SIGNAL;
    return FITNESS_DENSITY;
}
static double target_edge_signal(const Grid *g, Edge e) {
    int sz = g->size, alive = 0;
    for (int i = 0; i < sz; i++) {
        int idx;
        switch (e) {
            case EDGE_TOP: idx = i; break;
            case EDGE_BOTTOM: idx = (sz - 1) * sz + i; break;
            case EDGE_LEFT: idx = i * sz; break;
            case EDGE_RIGHT: idx = i * sz + (sz - 1); break;
            default: idx = i; break;
        }
        if (g->cells[idx]) alive++;
    }
    return (double)alive / sz;
}
static double entropy16(const int h[16], int n) {
    if (n <= 0) return 0.0;
    double s = 0.0;
    for (int i = 0; i < 16; i++) {
        if (h[i] > 0) {
            double p = (double)h[i] / n;
            s -= p * log(p);
        }
    }
    return s;
}
static double moran_nibble(const Grid *g, int nibble_field) {
    int sz = g->size, n = sz * sz;
    double *x = malloc(n * sizeof(double));
    if (!x) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < n; i++) {
        x[i] = nibble_field ? (int)GENOME_COUPLING_WEIGHT(g->genomes[i])
                            : (int)GENOME_RULE_SELECT(g->genomes[i]);
        mean += x[i];
    }
    mean /= n;
    double denom = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - mean; denom += d * d; }
    if (denom < 1e-12) { free(x); return 0.0; }
    double numer = 0.0;
    for (int y = 0; y < sz; y++) {
        for (int xc = 0; xc < sz; xc++) {
            int idx = y * sz + xc;
            int nidx[4] = {
                ((y - 1 + sz) % sz) * sz + xc,
                ((y + 1) % sz) * sz + xc,
                y * sz + ((xc - 1 + sz) % sz),
                y * sz + ((xc + 1) % sz),
            };
            for (int k = 0; k < 4; k++) numer += (x[idx] - mean) * (x[nidx[k]] - mean);
        }
    }
    double W = 4.0 * n;
    double I = (n / W) * (numer / denom);
    free(x);
    return I;
}
static void inject_glider(Grid *g) {
    int sz = g->size, y0 = sz / 2 - 1;
    if (y0 < 1) y0 = 1;
    grid_set(g, 0, y0, 1); grid_set(g, 1, y0 + 1, 1); grid_set(g, 2, y0 - 1, 1);
    grid_set(g, 2, y0, 1); grid_set(g, 2, y0 + 1, 1);
}

int main(int argc, char **argv) {
    int size = 64, steps = 1000, ga_every = 1, mut = 2;
    int mode = FITNESS_DENSITY;
    double fitness_discount = 1.0;
    Edge edge = EDGE_RIGHT;
    uint64_t seed = 42;
    const char *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--ga-every") && i + 1 < argc) ga_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fitness") && i + 1 < argc) mode = parse_fitness(argv[++i]);
        else if (!strcmp(argv[i], "--fitness-discount") && i + 1 < argc) fitness_discount = atof(argv[++i]);
        else if (!strcmp(argv[i], "--initial-mutation") && i + 1 < argc) mut = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--target") && i + 1 < argc) edge = parse_edge(argv[++i]);
        else if (!strcmp(argv[i], "--output") && i + 1 < argc) out = argv[++i];
    }
    FILE *f = out ? fopen(out, "w") : stdout;
    if (!f) return 1;

    System sys;
    sys_init(&sys, 2, size);
    coupling_connect(&sys.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    for (int g = 0; g < 2; g++) { sys.grids[g]->active = 1; sys.grids[g]->rule_idx = 0; }
    rng_seed(seed);
    sys_randomize(&sys, rng_u64);
    sys.grids[0]->rng_state = seed ^ 0xA5A5A5A5;
    sys.grids[1]->rng_state = seed ^ 0x5A5A5A5A;
    grid_alloc_genomes(sys.grids[0]);
    grid_alloc_genomes(sys.grids[1]);
    int n0 = size * size;
    for (int i = 0; i < n0; i++) {
        sys.grids[0]->genomes[i] = genome_pack(0, 15, mut);
        sys.grids[1]->genomes[i] = genome_pack(0, 15, mut);
    }
    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f,
                                           ga_every, mode, 1, (int)edge,
                                           fitness_discount, GENOME_MUTATE_ALL);
    inject_glider(sys.grids[0]);

    fprintf(f, "tick,density_g1,signal_g1,fit_mean,fit_var,fit_max,");
    fprintf(f, "rule_entropy,weight_entropy,moran_rule,moran_weight\n");

    for (int t = 0; t < steps; t++) {
        sys_step(&sys);
        Grid *g1 = sys.grids[1];
        int n = n0;

        /* density */
        int alive = 0;
        for (int i = 0; i < n; i++) if (g1->cells[i]) alive++;
        double density = (double)alive / n;

        /* signal at target edge */
        double signal = target_edge_signal(g1, edge);

        /* fitness stats */
        double fsum = 0, fmax = g1->fitness ? g1->fitness[0] : 0;
        for (int i = 0; i < n; i++) {
            double fi = g1->fitness ? g1->fitness[i] : 0;
            fsum += fi;
            if (fi > fmax) fmax = fi;
        }
        double fmean = fsum / n;
        double fvar = 0;
        for (int i = 0; i < n; i++) {
            double d = (g1->fitness ? g1->fitness[i] : 0) - fmean;
            fvar += d * d;
        }
        fvar /= n;

        /* genome entropies */
        int histr[16] = {0}, histw[16] = {0};
        for (int i = 0; i < n; i++) {
            histr[GENOME_RULE_SELECT(g1->genomes[i])]++;
            histw[GENOME_COUPLING_WEIGHT(g1->genomes[i])]++;
        }
        double rentropy = entropy16(histr, n);
        double wentropy = entropy16(histw, n);

        /* Moran's I */
        double mrule = moran_nibble(g1, 0);
        double mweight = moran_nibble(g1, 1);

        fprintf(f, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                t, density, signal, fmean, fvar, fmax,
                rentropy, wentropy, mrule, mweight);
    }
    if (out) fclose(f);
    sys_destroy(&sys);
    return 0;
}
