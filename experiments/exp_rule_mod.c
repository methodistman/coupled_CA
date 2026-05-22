/*
 * exp_rule_mod: H12 explicit rule modulation experiment.
 *
 * Grid 0's cell state directly selects grid 1's per-cell rule.
 * Compares static baseline vs. modulated to test whether hand-designed
 * rule maps beat (or complement) GA-discovered modulation.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/pipeline.h"
#include "../ca_core/rules.h"
#include "../ca_core/genome.h"
#include "../utils/rng.h"

#define DEFAULT_SIZE      64
#define DEFAULT_STEPS     200
#define DEFAULT_WINDOW    50
#define DEFAULT_SEED      42
#define DEFAULT_TRIALS    10

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N        Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N       Total ticks (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --window N      Score over last N ticks (default %d)\n", DEFAULT_WINDOW);
    fprintf(stderr, "  --seed N        Base RNG seed (default %d)\n", DEFAULT_SEED);
    fprintf(stderr, "  --trials N      Average over N consecutive seeds (default %d)\n", DEFAULT_TRIALS);
    fprintf(stderr, "  --target EDGE   Target edge on grid 1 (top|bottom|left|right, default right)\n");
    fprintf(stderr, "  --modulate-rules A,B  Two rule names for alive,dead (default Conway's Life,HighLife)\n");
    fprintf(stderr, "  --pipeline NAME Pipeline preset: default, intent, parallel (default: intent)\n");
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
    double mean_signal;
    double peak_signal;
} TrialResult;

static TrialResult run_trial(int size, int steps, int window, uint64_t seed,
                              Edge target_edge, int use_modulation,
                              int rule_alive, int rule_dead,
                              const char *pipeline_name) {
    System sys;
    sys_init(&sys, 2, size);
    coupling_connect(&sys.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    for (int g = 0; g < 2; g++) {
        sys.grids[g]->active = 1;
        sys.grids[g]->rule_idx = 0; /* Conway's Life */
    }

    rng_seed(seed);
    sys_randomize(&sys, rng_u64);
    sys.grids[0]->rng_state = seed ^ 0xA5A5A5A5;
    sys.grids[1]->rng_state = seed ^ 0x5A5A5A5A;

    grid_alloc_genomes(sys.grids[0]);
    grid_alloc_genomes(sys.grids[1]);
    int n0 = size * size;
    for (int i = 0; i < n0; i++) {
        sys.grids[0]->genomes[i] = genome_pack(0, 15, 0);
        sys.grids[1]->genomes[i] = genome_pack(0, 15, 0);
    }

    if (use_modulation) {
        Pipeline *p = pipeline_create("modulate");
        if (p) {
            IntentMode *mode_ptr = malloc(sizeof(IntentMode));
            ModulateConfig *mod_cfg = malloc(sizeof(ModulateConfig));
            if (mode_ptr && mod_cfg) {
                *mode_ptr = INTENT_MODE_REPLACE;
                mod_cfg->ctrl_grid = 0;
                mod_cfg->target_grid = 1;
                mod_cfg->rule_if_alive = rule_alive;
                mod_cfg->rule_if_dead = rule_dead;
                int run = pipeline_add_phase(p, PHASE_RUN, "compute_genomic",
                                             phase_grid_step_genomic, NULL, -1);
                int mod = pipeline_add_phase(p, PHASE_MUTATE, "modulate_rule",
                                             phase_modulate_rule, mod_cfg, -1);
                pipeline_add_dep(p, mod, run);
                int ex = pipeline_add_phase(p, PHASE_EXCHANGE, "exchange_intent",
                                            phase_exchange_intent, mode_ptr, -1);
                pipeline_add_dep(p, ex, mod);
                sys.pipeline = p;
            } else {
                free(mode_ptr); free(mod_cfg); pipeline_destroy(p);
            }
        }
    } else {
        if (pipeline_name && !strcmp(pipeline_name, "intent"))
            sys.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
        else if (pipeline_name && !strcmp(pipeline_name, "parallel"))
            sys.pipeline = pipeline_preset_parallel(INTENT_MODE_REPLACE, 0.0f);
        else
            sys.pipeline = pipeline_preset_default();
    }

    inject_glider(sys.grids[0], 0);

    double accum = 0.0, peak = 0.0;
    int window_start = steps - window;
    if (window_start < 0) window_start = 0;
    int counted = 0;
    for (int t = 0; t < steps; t++) {
        sys_step(&sys);
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
    sys_destroy(&sys);
    return r;
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    int window = DEFAULT_WINDOW;
    uint64_t seed = DEFAULT_SEED;
    int trials = DEFAULT_TRIALS;
    Edge target_edge = EDGE_RIGHT;
    const char *rule_alive_name = "Conway's Life";
    const char *rule_dead_name = "HighLife";
    const char *pipeline_name = "intent";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--window") && i+1 < argc) window = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--trials") && i+1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--target") && i+1 < argc) target_edge = parse_edge(argv[++i]);
        else if (!strcmp(argv[i], "--modulate-rules") && i+1 < argc) {
            char buf[128];
            strncpy(buf, argv[++i], sizeof(buf)-1);
            buf[sizeof(buf)-1] = '\0';
            char *comma = strchr(buf, ',');
            if (comma) {
                *comma = '\0';
                rule_alive_name = strdup(buf);
                rule_dead_name = strdup(comma+1);
            } else {
                rule_alive_name = strdup(buf);
            }
        }
        else if (!strcmp(argv[i], "--pipeline") && i+1 < argc) pipeline_name = argv[++i];
    }

    int rule_alive = rules_index_by_name(rule_alive_name);
    int rule_dead = rules_index_by_name(rule_dead_name);
    if (rule_alive < 0) { fprintf(stderr, "Unknown rule: %s\n", rule_alive_name); return 1; }
    if (rule_dead < 0) { fprintf(stderr, "Unknown rule: %s\n", rule_dead_name); return 1; }

    printf("# exp_rule_mod: explicit rule modulation (H12)\n");
    printf("# size=%d steps=%d window=%d base_seed=%llu trials=%d target=%d\n",
           size, steps, window, (unsigned long long)seed, trials, (int)target_edge);
    printf("# rule_alive=%s rule_dead=%s pipeline=%s\n",
           rule_alive_name, rule_dead_name, pipeline_name);
    printf("trial,seed,static_mean,static_peak,mod_mean,mod_peak\n");

    double sum_static = 0, sum_mod = 0;
    int wins = 0;
    for (int t = 0; t < trials; t++) {
        uint64_t s = seed + (uint64_t)t;
        TrialResult base = run_trial(size, steps, window, s, target_edge, 0, 0, 0, pipeline_name);
        TrialResult mod  = run_trial(size, steps, window, s, target_edge, 1, rule_alive, rule_dead, pipeline_name);
        printf("%d,%llu,%.6f,%.6f,%.6f,%.6f\n",
               t, (unsigned long long)s,
               base.mean_signal, base.peak_signal,
               mod.mean_signal, mod.peak_signal);
        sum_static += base.mean_signal;
        sum_mod += mod.mean_signal;
        if (mod.mean_signal > base.mean_signal) wins++;
    }
    double avg_static = sum_static / trials;
    double avg_mod = sum_mod / trials;
    double improvement = avg_mod - avg_static;
    double pct = avg_static > 1e-9 ? (improvement / avg_static) * 100.0 : 0.0;
    printf("# avg_static_mean=%.6f avg_mod_mean=%.6f\n", avg_static, avg_mod);
    printf("# delta_mean=%+.6f improvement=%+.2f%% mod_wins=%d/%d\n",
           improvement, pct, wins, trials);
    return 0;
}
