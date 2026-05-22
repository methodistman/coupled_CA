/*
 * exp_ruleset_profile: Profile every rule in every ruleset for
 * computational suitability.  Measures density maintenance, signal
 * propagation, and pattern richness across a standardized sweep.
 *
 * Three tracks in one binary:
 *   --ruleset binary|payload|wave|all
 *   --size N    (default 64)
 *   --steps N   (default 200)
 *   --trials N  (default 5)
 *   --seed N    (default 42)
 *   --output FILE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_engine.h"
#include "../ca_core/payload_rules.h"
#include "../ca_core/wave_engine.h"
#include "../ca_core/wave_rules.h"
#include "../ca_core/patterns.h"
#include "../metrics/metrics.h"
#include "../utils/rng.h"

#define DEF_SIZE   64
#define DEF_STEPS  200
#define DEF_TRIALS 5
#define DEF_SEED   42

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --ruleset binary|payload|wave|all  (default all)\n");
    fprintf(stderr, "  --size N                            (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --steps N                           (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --trials N                          (default %d)\n", DEF_TRIALS);
    fprintf(stderr, "  --seed N                            (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --output FILE                       (default stdout)\n");
}

/* ---------- binary profiling ---------- */
static void profile_binary(int size, int steps, int trials, uint64_t base_seed,
                           FILE *fp) {
    for (int r = 0; r < NUM_RULES; r++) {
        const char *name = RULE_TABLE[r].name;
        for (int t = 0; t < trials; t++) {
            uint64_t seed = base_seed + (uint64_t)t * 1000u + (uint64_t)r;
            rng_seed(seed);
            System sys;
            sys_init(&sys, 1, size);
            sys.grids[0]->active = 1;
            sys.grids[0]->rule_idx = r;
            sys_randomize(&sys, rng_u64);

            Grid *g = sys.grids[0];
            GridMetrics m;
            double prev_density = -1;

            for (int s = 0; s < steps; s++) {
                sys_step(&sys);
                metrics_compute(g, NULL, &m);
                if (prev_density < 0) metrics_census(g, &m);
                prev_density = m.density;

                int right_edge_count = 0;
                for (int y = 0; y < size; y++)
                    right_edge_count += g->cells[y * size + (size - 1)];

                fprintf(fp, "binary,%s,%d,%llu,%d,%.5f,%.5f,%.5f,%d,%d,%d\n",
                        name, t, (unsigned long long)seed, s,
                        m.density, m.activity, m.entropy,
                        m.gliders, m.oscillators, right_edge_count);
            }
            sys_destroy(&sys);
        }
    }
}

/* ---------- payload profiling ---------- */
static void profile_payload(int size, int steps, int trials, uint64_t base_seed,
                              FILE *fp) {
    for (int r = 0; r < NUM_PAYLOAD_RULES; r++) {
        const char *name = PAYLOAD_RULE_TABLE[r].name;
        for (int t = 0; t < trials; t++) {
            uint64_t seed = base_seed + (uint64_t)t * 1000u + (uint64_t)r;
            rng_seed(seed);
            PayloadSystem ps;
            payload_sys_init(&ps, 1, size);
            ps.grids[0]->rule_idx = r;
            payload_sys_randomize(&ps, rng_u64);

            PayloadGrid *g = ps.grids[0];
            double prev_density = -1;

            for (int s = 0; s < steps; s++) {
                payload_sys_step(&ps);
                int total = 0, alive = 0;
                for (int i = 0; i < size * size; i++) {
                    total += g->cells[i].payload;
                    alive += g->cells[i].alive;
                }
                double density = (double)alive / (size * size);
                double avg_payload = (double)total / (size * size);

                int right_edge_alive = 0;
                for (int y = 0; y < size; y++)
                    right_edge_alive += g->cells[y * size + (size - 1)].alive;

                /* No glider/oscillator census for payload — patterns.c is binary-only */
                fprintf(fp, "payload,%s,%d,%llu,%d,%.5f,%.5f,%.5f,NA,NA,%d\n",
                        name, t, (unsigned long long)seed, s,
                        density, avg_payload / 255.0, /* activity proxy */ 0.0,
                        right_edge_alive);
                prev_density = density;
            }
            payload_sys_destroy(&ps);
        }
    }
}

/* ---------- wave profiling ---------- */
static void profile_wave(int size, int steps, int trials, uint64_t base_seed,
                         FILE *fp) {
    for (int r = 0; r < NUM_WAVE_RULES; r++) {
        const char *name = WAVE_RULE_TABLE[r].name;
        for (int t = 0; t < trials; t++) {
            uint64_t seed = base_seed + (uint64_t)t * 1000u + (uint64_t)r;
            rng_seed(seed);
            WaveSystem ws;
            wave_sys_init(&ws, 1, size);
            ws.grids[0]->rule_idx = r;
            wave_sys_randomize(&ws, rng_u64);

            WaveGrid *g = ws.grids[0];

            for (int s = 0; s < steps; s++) {
                wave_sys_step(&ws);
                int alive_count = 0;
                uint64_t wave_sum = 0;
                for (int i = 0; i < size * size; i++) {
                    alive_count += g->cells[i].alive;
                    wave_sum += g->cells[i].wave;
                }
                double density = (double)alive_count / (size * size);
                double avg_wave = (double)wave_sum / (size * size);

                int right_edge_alive = 0;
                for (int y = 0; y < size; y++)
                    right_edge_alive += g->cells[y * size + (size - 1)].alive;

                fprintf(fp, "wave,%s,%d,%llu,%d,%.5f,%.5f,%.5f,NA,NA,%d\n",
                        name, t, (unsigned long long)seed, s,
                        density, avg_wave / (double)WAVE_MASK, 0.0,
                        right_edge_alive);
            }
            wave_sys_destroy(&ws);
        }
    }
}

int main(int argc, char **argv) {
    int size = DEF_SIZE, steps = DEF_STEPS, trials = DEF_TRIALS;
    uint64_t seed = DEF_SEED;
    const char *ruleset = "all";
    const char *outpath = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ruleset") && i + 1 < argc) ruleset = argv[++i];
        else if (!strcmp(argv[i], "--size")   && i + 1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps")  && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trials") && i + 1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed")   && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--output")  && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    FILE *fp = outpath ? fopen(outpath, "w") : stdout;
    if (!fp) { fprintf(stderr, "Cannot open %s\n", outpath); return 1; }

    fprintf(fp, "ruleset,rule_name,trial,seed,step,density,activity,entropy,gliders,oscillators,right_edge_alive\n");

    int do_binary  = !strcmp(ruleset, "all") || !strcmp(ruleset, "binary");
    int do_payload = !strcmp(ruleset, "all") || !strcmp(ruleset, "payload");
    int do_wave    = !strcmp(ruleset, "all") || !strcmp(ruleset, "wave");

    if (do_binary)  profile_binary(size, steps, trials, seed, fp);
    if (do_payload) profile_payload(size, steps, trials, seed, fp);
    if (do_wave)    profile_wave(size, steps, trials, seed, fp);

    if (outpath) fclose(fp);
    return 0;
}
