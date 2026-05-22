/* B1: Coupling-delay sweep. Records TE vs delay for delayed intent coupling. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/intent.h"
#include "../ca_core/genome.h"
#include "../ca_core/grid.h"
#include "../metrics/transfer_entropy.h"
#include "../utils/rng.h"

/* Returns edge density scaled to [0, 255] so TE histogram bins capture
   fractional variation instead of binary occupancy.  This enables
   meaningful TE computation with high-density GA-ICs where the edge
   is almost always occupied. */
static int edge_density_scaled(const Grid *g, Edge e) {
    int sz = g->size;
    int alive = 0;
    for (int i = 0; i < sz; i++) {
        int idx;
        switch (e) {
            case EDGE_TOP:    idx = i; break;
            case EDGE_BOTTOM: idx = (sz-1)*sz + i; break;
            case EDGE_LEFT:   idx = i*sz; break;
            case EDGE_RIGHT:  idx = i*sz + (sz-1); break;
            default: idx = i; break;
        }
        alive += g->cells[idx] ? 1 : 0;
    }
    return (alive * 255) / sz;   /* [0, 255] */
}

int main(int argc, char **argv) {
    int size = 64, steps = 1000, nseeds = 10, max_delay = 16, rule_idx = 2;
    const char *out = NULL;
    const char *load_genomes = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seeds") && i+1 < argc) nseeds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-delay") && i+1 < argc) max_delay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rule") && i+1 < argc) rule_idx = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--output") && i+1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--load-genomes") && i+1 < argc) load_genomes = argv[++i];
    }

    FILE *f = out ? fopen(out, "w") : stdout;
    if (!f) return 1;
    fprintf(f, "delay,te_mean,te_var,density_g0,density_g1\n");

    for (int delay = 0; delay <= max_delay; delay++) {
        double te_sum = 0, te_sq = 0, d0_sum = 0, d1_sum = 0;
        int valid = 0;

        for (int seed = 0; seed < nseeds; seed++) {
            System sys;
            sys_init(&sys, 2, size);
            coupling_connect(&sys.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
            for (int g = 0; g < 2; g++) {
                sys.grids[g]->active = 1;
                sys.grids[g]->rule_idx = rule_idx;
                grid_alloc_genomes(sys.grids[g]);
            }
            rng_seed(42 + seed);
            sys_randomize(&sys, rng_u64);
            if (load_genomes) {
                FILE *fin = fopen(load_genomes, "rb");
                if (fin) {
                    size_t expected = (size_t)size * size * sizeof(CellGenome);
                    size_t r0 = fread(sys.grids[0]->genomes, 1, expected, fin);
                    size_t r1 = fread(sys.grids[1]->genomes, 1, expected, fin);
                    fclose(fin);
                    if (r0 != expected || r1 != expected) {
                        fprintf(stderr, "Warning: incomplete genome load from %s\n", load_genomes);
                    }
                } else {
                    fprintf(stderr, "Warning: could not open %s for loading genomes\n", load_genomes);
                }
            }

            uint8_t *src_hist = calloc(steps, sizeof(uint8_t));
            uint8_t *dst_hist = calloc(steps, sizeof(uint8_t));
            double d0_acc = 0, d1_acc = 0;

            for (int t = 0; t < steps; t++) {
                if (load_genomes)
                    sys_step_intent_delayed_genomic(&sys, INTENT_MODE_REPLACE, 0.0f, delay);
                else
                    sys_step_intent_delayed(&sys, INTENT_MODE_REPLACE, 0.0f, delay);
                src_hist[t] = edge_density_scaled(sys.grids[0], EDGE_BOTTOM) / 32;   /* 8 discrete bins */
                dst_hist[t] = edge_density_scaled(sys.grids[1], EDGE_TOP) / 32;

                int n = size * size;
                int a0 = 0, a1 = 0;
                for (int i = 0; i < n; i++) { if (sys.grids[0]->cells[i]) a0++; if (sys.grids[1]->cells[i]) a1++; }
                d0_acc += (double)a0 / n;
                d1_acc += (double)a1 / n;
            }

            double te = te_compute_discrete(dst_hist, src_hist, steps, 8);
            if (te >= 0.0) {
                te_sum += te;
                te_sq += te * te;
                valid++;
            }
            d0_sum += d0_acc / steps;
            d1_sum += d1_acc / steps;

            free(src_hist); free(dst_hist);
            sys_destroy(&sys);
        }

        double te_mean = valid > 0 ? te_sum / valid : 0;
        double te_var = valid > 0 ? (te_sq / valid - te_mean * te_mean) : 0;
        fprintf(f, "%d,%.6f,%.6f,%.6f,%.6f\n",
                delay, te_mean, te_var, d0_sum / nseeds, d1_sum / nseeds);
    }

    if (out) fclose(f);
    return 0;
}
