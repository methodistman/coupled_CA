/*
 * exp_local_ga: Demonstrates Phase 3 per-cell genome evolution.
 *
 * Setup: 2 grids coupled bottom->top. A glider is injected on grid 0's
 * left edge. We measure signal strength (mean live cells) at grid 1's
 * target edge over a window of late-tick steps, comparing:
 *   1. Static-rule baseline   (PIPELINE_INTENT, no genomes)
 *   2. Local-GA evolved       (PIPELINE_GENOMIC with FITNESS_EDGE_SIGNAL)
 *
 * The local GA, given time, should evolve rule_select and coupling_weight
 * patterns that improve signal transmission to the target edge.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/pipeline.h"
#include "../ca_core/rules.h"
#include "../ca_core/genome.h"
#include "../ca_core/local_ga.h"
#include "../utils/rng.h"
#include "../metrics/history.h"

#define DEFAULT_SIZE      32
#define DEFAULT_STEPS     200
#define DEFAULT_WINDOW    50      /* last N ticks used for signal scoring */
#define DEFAULT_GA_EVERY  10
#define DEFAULT_SEED      42
#define DEFAULT_TRIALS    1

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N        Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N       Total ticks (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --window N      Score over last N ticks (default %d)\n", DEFAULT_WINDOW);
    fprintf(stderr, "  --ga-every N    Run local GA every N ticks (default %d)\n", DEFAULT_GA_EVERY);
    fprintf(stderr, "  --seed N        Base RNG seed (default %d)\n", DEFAULT_SEED);
    fprintf(stderr, "  --trials N      Average over N consecutive seeds (default %d)\n", DEFAULT_TRIALS);
    fprintf(stderr, "  --target EDGE   Target edge on grid 1 (top|bottom|left|right, default right)\n");
    fprintf(stderr, "  --fitness MODE  GA fitness mode: stability|activity|density|edge (default edge)\n");
    fprintf(stderr, "  --fitness-discount D  Fitness discount [0,1] (default 1.0 = accumulate)\n");
    fprintf(stderr, "  --evolve-rule       Allow GA to mutate rule_select (default yes)\n");
    fprintf(stderr, "  --evolve-weight     Allow GA to mutate coupling_weight (default yes)\n");
    fprintf(stderr, "  --evolve-mutation   Allow GA to mutate mutation_rate (default yes)\n");
    fprintf(stderr, "  --initial-mutation N  Initial genome mutation_rate field 0..15 (default 1)\n");
    fprintf(stderr, "  --noise-rate R      Per-tick state flip probability (default 0)\n");
    fprintf(stderr, "  --shift-injection N Shift glider injection y by N cells (default 0)\n");
    fprintf(stderr, "  --dump-genomes P  Write PGM heatmaps to P_{base,ga}_g{0,1}_{rule,weight}.pgm\n");
    fprintf(stderr, "  --dump-metrics F  Write per-tick metrics (density, entropy, TE) to CSV\n");
    fprintf(stderr, "  --save-genomes F  Save final genomes to binary file (one grid after another)\n");
    fprintf(stderr, "  --load-genomes F  Load initial genomes from binary file instead of default\n");
    fprintf(stderr, "  --help          Show this help\n");
}

static Edge parse_edge(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    return EDGE_RIGHT;
}

static void inject_glider(Grid *g, int shift) {
    int sz = g->size;
    int y0 = sz / 2 - 1 + shift;
    if (y0 < 1) y0 = 1;
    if (y0 >= sz - 1) y0 = sz - 2;
    grid_set(g, 0, y0, 1);
    grid_set(g, 1, y0+1, 1);
    grid_set(g, 2, y0-1, 1);
    grid_set(g, 2, y0, 1);
    grid_set(g, 2, y0+1, 1);
}

static double measure_target_edge(const Grid *g, Edge edge) {
    int sz = g->size, alive = 0;
    for (int i = 0; i < sz; i++) {
        int idx;
        switch (edge) {
            case EDGE_TOP:    idx = i; break;
            case EDGE_BOTTOM: idx = (sz-1)*sz + i; break;
            case EDGE_LEFT:   idx = i*sz; break;
            case EDGE_RIGHT:  idx = i*sz + (sz-1); break;
            default: idx = i; break;
        }
        if (g->cells[idx]) alive++;
    }
    return (double)alive / sz;
}

typedef struct {
    double mean_signal;     /* avg signal density over window */
    double peak_signal;     /* max signal density in window */
    double moran_rule[2];   /* Moran's I on rule_select per grid */
    double moran_weight[2]; /* Moran's I on coupling_weight per grid */
    double top_rule_frac[2]; /* fraction of cells with most-common rule */
    int distinct_rules[2];   /* how many distinct rule values present */
} TrialResult;

typedef struct {
    double mean;
    double std;
    double se;
    double ci_lo;
    double ci_hi;
} StatSummary;

static StatSummary compute_summary(const double *vals, int n) {
    StatSummary s = {0, 0, 0, 0, 0};
    if (n <= 0) return s;
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += vals[i];
    s.mean = sum / n;
    if (n > 1) {
        double sq = 0.0;
        for (int i = 0; i < n; i++) {
            double d = vals[i] - s.mean;
            sq += d * d;
        }
        s.std = sqrt(sq / (n - 1));
        s.se = s.std / sqrt((double)n);
        /* 95% CI using normal approx (z=1.96); adequate for n>=10.
           For very small n, this is slightly optimistic. */
        double z = 1.96;
        s.ci_lo = s.mean - z * s.se;
        s.ci_hi = s.mean + z * s.se;
    }
    return s;
}

/* Paired t-statistic = mean(diff) / SE(diff). Returns |t| as effect-size proxy.
   For a rough significance check at alpha=0.05 (two-sided), critical |t| ~ 2.0
   when df >= 10. We return t_raw so caller can judge. */
static double paired_t_stat(const double *a, const double *b, int n) {
    if (n <= 1) return 0.0;
    double sum = 0.0, sq = 0.0;
    for (int i = 0; i < n; i++) {
        double d = b[i] - a[i];
        sum += d;
        sq += d * d;
    }
    double mean = sum / n;
    double var = (sq - n * mean * mean) / (n - 1);
    if (var < 0) var = 0;
    double se = sqrt(var / n);
    return se > 1e-12 ? mean / se : 0.0;
}

static void genome_rule_stats(const Grid *g, double *top_frac, int *distinct) {
    int hist[32] = {0};
    int n = g->size * g->size;
    for (int i = 0; i < n; i++) hist[GENOME_RULE_SELECT(g->genomes[i])]++;
    int max = 0, present = 0;
    for (int i = 0; i < 32; i++) {
        if (hist[i] > max) max = hist[i];
        if (hist[i] > 0) present++;
    }
    *top_frac = (double)max / n;
    *distinct = present;
}

/* Write a PGM (P5, 8-bit) image scaled from a genome field.
   nibble_field == 0 dumps GENOME_RULE_SELECT (0..31), 1 dumps GENOME_COUPLING_WEIGHT (0..15). */
static void dump_genome_pgm(const Grid *g, const char *path, int nibble_field) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int sz = g->size;
    fprintf(f, "P5\n%d %d\n255\n", sz, sz);
    for (int i = 0; i < sz * sz; i++) {
        int v = (nibble_field == 0)
                ? (int)GENOME_RULE_SELECT(g->genomes[i])
                : (int)GENOME_COUPLING_WEIGHT(g->genomes[i]);
        unsigned char p = (unsigned char)(nibble_field == 0
                ? (v * 255) / 31   /* 0..31 -> 0..255 */
                : (v * 17));       /* 0..15 -> 0..255 */
        fwrite(&p, 1, 1, f);
    }
    fclose(f);
}

/* Moran's I with rook (von Neumann) neighborhood and toroidal wrap.
   Returns 0 if grid is uniform (std dev == 0). Range: [-1, +1] roughly,
   with 0 = no spatial autocorrelation, > 0 = clustering. */
static double morans_i_nibble(const Grid *g, int nibble_field) {
    int sz = g->size;
    int n = sz * sz;
    double *x = malloc(n * sizeof(double));
    if (!x) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < n; i++) {
        int v = (nibble_field == 0)
                ? (int)GENOME_RULE_SELECT(g->genomes[i])
                : (int)GENOME_COUPLING_WEIGHT(g->genomes[i]);
        x[i] = v;
        mean += v;
    }
    mean /= n;
    double denom = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - mean; denom += d * d; }
    if (denom < 1e-12) { free(x); return 0.0; }
    /* Weights matrix: 4-connected with torus wrap, total W = 4n */
    double numer = 0.0;
    for (int y = 0; y < sz; y++) {
        for (int xc = 0; xc < sz; xc++) {
            int idx = y * sz + xc;
            double di = x[idx] - mean;
            int n_idx[4] = {
                ((y - 1 + sz) % sz) * sz + xc,
                ((y + 1) % sz) * sz + xc,
                y * sz + ((xc - 1 + sz) % sz),
                y * sz + ((xc + 1) % sz),
            };
            for (int k = 0; k < 4; k++) numer += di * (x[n_idx[k]] - mean);
        }
    }
    double W = 4.0 * n;
    double I = (n / W) * (numer / denom);
    free(x);
    return I;
}

static int parse_fitness_mode(const char *s) {
    if (!strcmp(s, "stability")) return FITNESS_STABILITY;
    if (!strcmp(s, "activity"))  return FITNESS_ACTIVITY;
    if (!strcmp(s, "density"))   return FITNESS_DENSITY;
    if (!strcmp(s, "edge"))      return FITNESS_EDGE_SIGNAL;
    return FITNESS_EDGE_SIGNAL;
}

static void apply_noise(System *sys, double rate, uint64_t (*rng)(void)) {
    if (rate <= 0.0) return;
    for (int gi = 0; gi < sys->num_grids; gi++) {
        Grid *g = sys->grids[gi];
        if (!g || !g->active) continue;
        int n = g->size * g->size;
        for (int i = 0; i < n; i++) {
            if ((double)(rng() % 1000000) / 1000000.0 < rate) {
                g->cells[i] = g->cells[i] ? 0 : 1;
            }
        }
    }
}

static TrialResult run_trial(int size, int steps, int window, uint64_t seed,
                              Edge target_edge, int use_local_ga, int ga_every,
                              int fitness_mode, double fitness_discount,
                              int mutate_mask, int initial_mutation,
                              double noise_rate, int shift_inject,
                              const char *dump_prefix, const char *dump_metrics,
                              const char *save_genomes, const char *load_genomes) {
    System sys;
    sys_init(&sys, 2, size);
    coupling_connect(&sys.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    for (int g = 0; g < 2; g++) {
        sys.grids[g]->active = 1;
        sys.grids[g]->rule_idx = 0;  /* default Conway's Life */
    }

    rng_seed(seed);
    sys_randomize(&sys, rng_u64);
    sys.grids[0]->rng_state = seed ^ 0xA5A5A5A5;
    sys.grids[1]->rng_state = seed ^ 0x5A5A5A5A;

    /* Both variants use the same genomic kernel for a fair comparison; only the
       GA-mutation phase differs. Baseline: ga_every = 10^9 (never runs).
       Initial genome: rule=0 (Life), weight=15 (full), mutation=1 (low drift). */
    grid_alloc_genomes(sys.grids[0]);
    grid_alloc_genomes(sys.grids[1]);
    int n0 = size * size;
    int mut = initial_mutation < 0 ? 0 : (initial_mutation > 15 ? 15 : initial_mutation);
    if (load_genomes) {
        FILE *fin = fopen(load_genomes, "rb");
        if (fin) {
            size_t expected = (size_t)n0 * sizeof(CellGenome);
            size_t r0 = fread(sys.grids[0]->genomes, 1, expected, fin);
            size_t r1 = fread(sys.grids[1]->genomes, 1, expected, fin);
            fclose(fin);
            if (r0 != expected || r1 != expected) {
                fprintf(stderr, "Warning: incomplete genome load from %s\n", load_genomes);
            }
        } else {
            fprintf(stderr, "Warning: could not open %s for loading genomes\n", load_genomes);
        }
    } else {
        for (int i = 0; i < n0; i++) {
            sys.grids[0]->genomes[i] = genome_pack(0, 15, mut);
            sys.grids[1]->genomes[i] = genome_pack(0, 15, mut);
        }
    }
    int effective_ga_every = use_local_ga ? ga_every : 0;  /* 0 = no MUTATE phase */
    int fmode = use_local_ga ? fitness_mode : -1;
    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, effective_ga_every,
                                            fmode, 1, (int)target_edge,
                                            fitness_discount, mutate_mask);
    if (dump_metrics) {
        sys.metrics_history = metrics_history_create(2, steps + 16);
        int ex = 1; /* exchange_intent is phase 1 in preset_genomic */
        int census = pipeline_add_phase(sys.pipeline, PHASE_ANALYZE, "census",
                                        phase_analyze_census, NULL, -1);
        pipeline_add_dep(sys.pipeline, census, ex);
        int te = pipeline_add_phase(sys.pipeline, PHASE_ANALYZE, "transfer_entropy",
                                    phase_analyze_te, NULL, -1);
        pipeline_add_dep(sys.pipeline, te, ex);
        int max_edge = size;
        sys.intent_ring = intent_ring_create(16, 2 * 4, max_edge);
    }

    inject_glider(sys.grids[0], shift_inject);

    double accum = 0.0, peak = 0.0;
    int window_start = steps - window;
    if (window_start < 0) window_start = 0;
    int counted = 0;
    for (int t = 0; t < steps; t++) {
        sys_step(&sys);
        apply_noise(&sys, noise_rate, rng_u64);
        if (t >= window_start) {
            double s = measure_target_edge(sys.grids[1], target_edge);
            accum += s;
            if (s > peak) peak = s;
            counted++;
        }
    }
    TrialResult r;
    r.mean_signal = counted ? accum / counted : 0.0;
    r.peak_signal = peak;
    for (int gi = 0; gi < 2; gi++) {
        r.moran_rule[gi]   = morans_i_nibble(sys.grids[gi], 0);
        r.moran_weight[gi] = morans_i_nibble(sys.grids[gi], 1);
        genome_rule_stats(sys.grids[gi], &r.top_rule_frac[gi], &r.distinct_rules[gi]);
    }
    if (dump_prefix) {
        char path[512];
        for (int gi = 0; gi < 2; gi++) {
            snprintf(path, sizeof(path), "%s_g%d_rule.pgm", dump_prefix, gi);
            dump_genome_pgm(sys.grids[gi], path, 0);
            snprintf(path, sizeof(path), "%s_g%d_weight.pgm", dump_prefix, gi);
            dump_genome_pgm(sys.grids[gi], path, 1);
        }
    }
    if (dump_metrics) {
        if (metrics_history_export_csv(sys.metrics_history, dump_metrics) != 0)
            fprintf(stderr, "Warning: failed to write metrics to %s\n", dump_metrics);
    }
    if (save_genomes) {
        FILE *fout = fopen(save_genomes, "wb");
        if (fout) {
            size_t expected = (size_t)n0 * sizeof(CellGenome);
            size_t w0 = fwrite(sys.grids[0]->genomes, 1, expected, fout);
            size_t w1 = fwrite(sys.grids[1]->genomes, 1, expected, fout);
            fclose(fout);
            if (w0 != expected || w1 != expected) {
                fprintf(stderr, "Warning: incomplete genome save to %s\n", save_genomes);
            }
        } else {
            fprintf(stderr, "Warning: could not open %s for saving genomes\n", save_genomes);
        }
    }
    sys_destroy(&sys);
    return r;
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    int window = DEFAULT_WINDOW;
    int ga_every = DEFAULT_GA_EVERY;
    uint64_t seed = DEFAULT_SEED;
    int trials = DEFAULT_TRIALS;
    Edge target_edge = EDGE_RIGHT;
    int fitness_mode = FITNESS_EDGE_SIGNAL;
    double fitness_discount = 1.0;
    int evolve_rule = 1, evolve_weight = 1, evolve_mutation = 1;
    int initial_mutation = 1;
    double noise_rate = 0.0;
    int shift_injection = 0;
    const char *dump_prefix = NULL;
    const char *dump_metrics = NULL;
    const char *save_genomes = NULL;
    const char *load_genomes = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--window") && i+1 < argc) window = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ga-every") && i+1 < argc) ga_every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--trials") && i+1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--target") && i+1 < argc) target_edge = parse_edge(argv[++i]);
        else if (!strcmp(argv[i], "--fitness") && i+1 < argc) fitness_mode = parse_fitness_mode(argv[++i]);
        else if (!strcmp(argv[i], "--fitness-discount") && i+1 < argc) fitness_discount = atof(argv[++i]);
        else if (!strcmp(argv[i], "--evolve-rule")) evolve_rule = 1;
        else if (!strcmp(argv[i], "--no-evolve-rule")) evolve_rule = 0;
        else if (!strcmp(argv[i], "--evolve-weight")) evolve_weight = 1;
        else if (!strcmp(argv[i], "--no-evolve-weight")) evolve_weight = 0;
        else if (!strcmp(argv[i], "--evolve-mutation")) evolve_mutation = 1;
        else if (!strcmp(argv[i], "--no-evolve-mutation")) evolve_mutation = 0;
        else if (!strcmp(argv[i], "--initial-mutation") && i+1 < argc) initial_mutation = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--noise-rate") && i+1 < argc) noise_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--shift-injection") && i+1 < argc) shift_injection = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump-genomes") && i+1 < argc) dump_prefix = argv[++i];
        else if (!strcmp(argv[i], "--dump-metrics") && i+1 < argc) dump_metrics = argv[++i];
        else if (!strcmp(argv[i], "--save-genomes") && i+1 < argc) save_genomes = argv[++i];
        else if (!strcmp(argv[i], "--load-genomes") && i+1 < argc) load_genomes = argv[++i];
    }
    if (trials < 1) trials = 1;

    int mutate_mask = 0;
    if (evolve_rule)     mutate_mask |= GENOME_MUTATE_RULE;
    if (evolve_weight)   mutate_mask |= GENOME_MUTATE_WEIGHT;
    if (evolve_mutation) mutate_mask |= GENOME_MUTATE_MUTATION;
    if (mutate_mask == 0) mutate_mask = GENOME_MUTATE_ALL;

    printf("# exp_local_ga: signal-transmission with vs without per-cell GA\n");
    printf("# size=%d steps=%d window=%d ga_every=%d base_seed=%llu trials=%d target=%d\n",
           size, steps, window, ga_every, (unsigned long long)seed, trials, (int)target_edge);
    printf("# fitness_discount=%.4f mutate_mask=%d noise_rate=%.6f shift=%d\n",
           fitness_discount, mutate_mask, noise_rate, shift_injection);
    if (dump_prefix)
        printf("# dump_genomes prefix=%s (only last trial dumped)\n", dump_prefix);
    printf("trial,seed,static_mean,static_peak,ga_mean,ga_peak,"
           "base_moran_rule_g1,ga_moran_rule_g1,base_moran_w_g1,ga_moran_w_g1\n");

    double *base_means = calloc(trials, sizeof(double));
    double *ga_means   = calloc(trials, sizeof(double));
    double sum_base_mean = 0, sum_ga_mean = 0, sum_base_peak = 0, sum_ga_peak = 0;
    double sum_base_moran_rule = 0, sum_ga_moran_rule = 0;
    double sum_base_moran_w = 0, sum_ga_moran_w = 0;
    double sum_base_top = 0, sum_ga_top = 0;
    double sum_base_distinct = 0, sum_ga_distinct = 0;
    int wins = 0;
    char buf_base[512] = {0}, buf_ga[512] = {0};
    for (int t = 0; t < trials; t++) {
        uint64_t s = seed + (uint64_t)t;
        const char *base_dump = NULL, *ga_dump = NULL;
        if (dump_prefix && t == trials - 1) {
            snprintf(buf_base, sizeof(buf_base), "%s_base", dump_prefix);
            snprintf(buf_ga,   sizeof(buf_ga),   "%s_ga",   dump_prefix);
            base_dump = buf_base; ga_dump = buf_ga;
        }
        const char *m_base = NULL, *m_ga = NULL;
        char mbuf_base[512] = {0}, mbuf_ga[512] = {0};
        if (dump_metrics && t == trials - 1) {
            snprintf(mbuf_base, sizeof(mbuf_base), "%s_base.csv", dump_metrics);
            snprintf(mbuf_ga,   sizeof(mbuf_ga),   "%s_ga.csv",   dump_metrics);
            m_base = mbuf_base; m_ga = mbuf_ga;
        }
        TrialResult base = run_trial(size, steps, window, s, target_edge, 0, 0, fitness_mode, 1.0, GENOME_MUTATE_ALL, initial_mutation, noise_rate, shift_injection, base_dump, m_base, NULL, NULL);
        TrialResult ga   = run_trial(size, steps, window, s, target_edge, 1, ga_every, fitness_mode, fitness_discount, mutate_mask, initial_mutation, noise_rate, shift_injection, ga_dump, m_ga, save_genomes, load_genomes);
        base_means[t] = base.mean_signal;
        ga_means[t]   = ga.mean_signal;
        printf("%d,%llu,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f,%.4f\n", t, (unsigned long long)s,
               base.mean_signal, base.peak_signal, ga.mean_signal, ga.peak_signal,
               base.moran_rule[1], ga.moran_rule[1],
               base.moran_weight[1], ga.moran_weight[1]);
        sum_base_mean += base.mean_signal;
        sum_ga_mean += ga.mean_signal;
        sum_base_peak += base.peak_signal;
        sum_ga_peak += ga.peak_signal;
        sum_base_moran_rule += base.moran_rule[1];
        sum_ga_moran_rule   += ga.moran_rule[1];
        sum_base_moran_w    += base.moran_weight[1];
        sum_ga_moran_w      += ga.moran_weight[1];
        sum_base_top        += base.top_rule_frac[1];
        sum_ga_top          += ga.top_rule_frac[1];
        sum_base_distinct   += base.distinct_rules[1];
        sum_ga_distinct     += ga.distinct_rules[1];
        if (ga.mean_signal > base.mean_signal) wins++;
    }
    double avg_base = sum_base_mean / trials;
    double avg_ga = sum_ga_mean / trials;
    double improvement = avg_ga - avg_base;
    double pct = avg_base > 1e-9 ? (improvement / avg_base) * 100.0 : 0.0;
    StatSummary s_base = compute_summary(base_means, trials);
    StatSummary s_ga   = compute_summary(ga_means, trials);
    double t_stat = paired_t_stat(base_means, ga_means, trials);
    free(base_means); free(ga_means);
    printf("# avg_static_mean=%.6f  avg_ga_mean=%.6f\n", avg_base, avg_ga);
    printf("# avg_static_peak=%.6f  avg_ga_peak=%.6f\n", sum_base_peak / trials, sum_ga_peak / trials);
    printf("# avg_moran_rule_g1: static=%.4f  ga=%.4f\n",
           sum_base_moran_rule / trials, sum_ga_moran_rule / trials);
    printf("# avg_moran_weight_g1: static=%.4f  ga=%.4f\n",
           sum_base_moran_w / trials, sum_ga_moran_w / trials);
    printf("# avg_top_rule_frac_g1: static=%.3f  ga=%.3f  (1.0 = monoculture)\n",
           sum_base_top / trials, sum_ga_top / trials);
    printf("# avg_distinct_rules_g1: static=%.2f  ga=%.2f  (out of 16)\n",
           sum_base_distinct / trials, sum_ga_distinct / trials);
    printf("# delta_mean=%+.6f  improvement=%+.2f%%  ga_wins=%d/%d\n",
           improvement, pct, wins, trials);
    printf("# static_mean_95CI=[%.6f, %.6f]  ga_mean_95CI=[%.6f, %.6f]\n",
           s_base.ci_lo, s_base.ci_hi, s_ga.ci_lo, s_ga.ci_hi);
    printf("# paired_t_stat=%.3f  (|t|>~2.0 suggests p<0.05 for df>=10)\n", t_stat);
    return 0;
}
