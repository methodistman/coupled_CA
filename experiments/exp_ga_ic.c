#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/coupling.h"
#include "../ca_core/rules.h"
#include "../ca_core/patterns.h"
#include "../ca_core/pipeline.h"
#include "../metrics/metrics.h"
#include "../metrics/history.h"
#include "../utils/rng.h"

#define DEFAULT_POP 50
#define DEFAULT_GEN 100
#define DEFAULT_SIZE 32
#define DEFAULT_STEPS 50
#define DEFAULT_MUTATE 0.05
#define TOURNAMENT_SIZE 3
#define ELITE_COUNT 2

static int census_bonus = 1;

typedef struct {
    uint8_t *bits;   /* flattened grid cells, 1 bit per cell */
    int nbits;
    double fitness;
} Genome;

typedef struct {
    Genome *individuals;
    int pop_size;
    int nbits;
    int n_grids;
    int *grid_sizes;
} Population;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --pop N       Population size (default %d)\n", DEFAULT_POP);
    fprintf(stderr, "  --gen N       Generations (default %d)\n", DEFAULT_GEN);
    fprintf(stderr, "  --size N      Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N     Steps per evaluation (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --mutate F    Mutation rate (default %.2f)\n", DEFAULT_MUTATE);
    fprintf(stderr, "  --seed N      Base seed (default: time)\n");
    fprintf(stderr, "  --grids N     Number of grids (default 2)\n");
    fprintf(stderr, "  --connect SPEC  e.g. 0:bottom->1:top (default none)\n");
    fprintf(stderr, "  --inject PAT  Pattern to inject: glider,blinker,block (default glider)\n");
    fprintf(stderr, "  --target EDGE Readout edge: top,bottom,left,right (default right)\n");
    fprintf(stderr, "  --scaled      Use proportional scaling for different-size grids\n");
    fprintf(stderr, "  --seed-pattern PAT  Seed every IC with a pattern: gun,block,beehive,eater (default none)\n");
    fprintf(stderr, "  --census      Output glider/oscillator counts per generation\n");
    fprintf(stderr, "  --pipeline NAME Pipeline preset: default, intent, analyze, parallel\n");
    fprintf(stderr, "  --per-tick-census  Output per-tick census for best genome (needs --pipeline analyze)\n");
    fprintf(stderr, "  --help        Show this help\n");
}

static Edge edge_from_string(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

/* Inject a glider pattern at the left edge of grid 0, centered vertically */
static void inject_glider(System *sys, int grid_idx) {
    Grid *g = sys->grids[grid_idx];
    int sz = g->size;
    int y0 = sz / 2 - 1;
    if (y0 < 1) y0 = 1;
    grid_set(g, 0, y0, 1);
    grid_set(g, 1, y0+1, 1);
    grid_set(g, 2, y0-1, 1);
    grid_set(g, 2, y0, 1);
    grid_set(g, 2, y0+1, 1);
}

/* Inject a blinker (period-2 oscillator) */
static void inject_blinker(System *sys, int grid_idx) {
    Grid *g = sys->grids[grid_idx];
    int sz = g->size;
    int y0 = sz / 2;
    grid_set(g, 0, y0-1, 1);
    grid_set(g, 0, y0, 1);
    grid_set(g, 0, y0+1, 1);
}

/* Inject a 2x2 block (still life) */
static void inject_block(System *sys, int grid_idx) {
    Grid *g = sys->grids[grid_idx];
    int sz = g->size;
    int y0 = sz / 2;
    grid_set(g, 0, y0, 1);
    grid_set(g, 1, y0, 1);
    grid_set(g, 0, y0+1, 1);
    grid_set(g, 1, y0+1, 1);
}

static void stamp_pattern_into_genome(Genome *g, const char *pat, int gi, int sz) {
    Grid *t = grid_create(sz); grid_clear(t);
    if (!strcmp(pat, "gun")) pattern_glider_gun(t, 1, 1);
    else if (!strcmp(pat, "block")) pattern_block(t, sz/2, sz/2);
    else if (!strcmp(pat, "beehive")) pattern_beehive(t, sz/2, sz/2);
    else if (!strcmp(pat, "eater")) pattern_eater(t, sz/2, sz/2);
    int off = gi * sz * sz;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
            g->bits[off + y*sz + x] = t->cells[y*sz+x] ? 1 : 0;
    grid_destroy(t);
}

static void genome_to_system(const Genome *g, System *sys, const int *sizes, int n_grids) {
    int pos = 0;
    for (int gi = 0; gi < n_grids; gi++) {
        Grid *grid = sys->grids[gi];
        int sz = sizes[gi];
        grid->size = sz;
        for (int y = 0; y < sz; y++)
            for (int x = 0; x < sz; x++)
                grid->cells[y * sz + x] = g->bits[pos++] ? 1 : 0;
    }
}

/* Compute fitness: correlation between injected signal and target edge readout */
static double evaluate_fitness(const Genome *g, System *sys, const int *sizes, int n_grids,
                                int steps, void (*inject_fn)(System*, int),
                                Edge target_edge, int inject_grid) {
    genome_to_system(g, sys, sizes, n_grids);
    /* Inject the signal */
    inject_fn(sys, inject_grid);
    /* Run */
    for (int s = 0; s < steps; s++)
        sys_step(sys);
    /* Read target edge from target grid (last grid) */
    int tgt_grid = n_grids - 1;
    Grid *tg = sys->grids[tgt_grid];
    int tsz = tg->size;
    /* Build expected pattern: all 1s at target edge (simplest signal detection) */
    double sum = 0, sum_sq = 0, expected_sum = 0, expected_sq = 0, cross = 0;
    int n = tsz;
    for (int i = 0; i < n; i++) {
        int val = 0;
        switch (target_edge) {
        case EDGE_TOP:    val = tg->cells[i]; break;
        case EDGE_BOTTOM: val = tg->cells[(tsz-1)*tsz + i]; break;
        case EDGE_LEFT:   val = tg->cells[i*tsz]; break;
        case EDGE_RIGHT:  val = tg->cells[i*tsz + (tsz-1)]; break;
        }
        int expected = 1; /* simple: we want signal to arrive */
        sum += val;
        sum_sq += val * val;
        expected_sum += expected;
        expected_sq += expected * expected;
        cross += val * expected;
    }
    double denom = sqrt(sum_sq * expected_sq);
    if (denom < 1e-9) return 0.0;
    double corr = cross / denom;
    /* Also reward total signal energy */
    double energy = sum / n;
    double signal = corr * 0.6 + energy * 0.4;
    /* Census-based structure bonus on target grid */
    int gliders = census_gliders(tg);
    int oscillators = census_oscillators(tg);
    if (census_bonus)
        return signal + 0.05 * gliders + 0.02 * oscillators;
    else
        return signal;
}

static void population_create(Population *pop, int pop_size, int n_grids, const int *sizes) {
    pop->pop_size = pop_size;
    pop->n_grids = n_grids;
    pop->grid_sizes = malloc(n_grids * sizeof(int));
    memcpy(pop->grid_sizes, sizes, n_grids * sizeof(int));
    int nbits = 0;
    for (int i = 0; i < n_grids; i++) nbits += sizes[i] * sizes[i];
    pop->nbits = nbits;
    pop->individuals = malloc(pop_size * sizeof(Genome));
    for (int i = 0; i < pop_size; i++) {
        pop->individuals[i].bits = malloc(nbits * sizeof(uint8_t));
        pop->individuals[i].nbits = nbits;
        pop->individuals[i].fitness = 0.0;
    }
}

static void population_randomize(Population *pop) {
    for (int i = 0; i < pop->pop_size; i++) {
        for (int b = 0; b < pop->nbits; b++)
            pop->individuals[i].bits[b] = (rng_u64() & 1) ? 1 : 0;
    }
}

static void population_destroy(Population *pop) {
    for (int i = 0; i < pop->pop_size; i++)
        free(pop->individuals[i].bits);
    free(pop->individuals);
    free(pop->grid_sizes);
}

/* Tournament selection: return index of winner */
static int tournament_select(const Population *pop) {
    int best = rng_u64() % pop->pop_size;
    double best_f = pop->individuals[best].fitness;
    for (int i = 1; i < TOURNAMENT_SIZE; i++) {
        int idx = rng_u64() % pop->pop_size;
        if (pop->individuals[idx].fitness > best_f) {
            best = idx;
            best_f = pop->individuals[idx].fitness;
        }
    }
    return best;
}

/* 1-point crossover between two parents, store in child */
static void crossover(const Genome *p1, const Genome *p2, Genome *child) {
    int point = (int)(rng_u64() % (child->nbits + 1));
    memcpy(child->bits, p1->bits, point * sizeof(uint8_t));
    memcpy(child->bits + point, p2->bits + point, (child->nbits - point) * sizeof(uint8_t));
}

static void mutate(Genome *g, double rate) {
    for (int i = 0; i < g->nbits; i++) {
        if ((double)(rng_u64() % 1000000) / 1000000.0 < rate)
            g->bits[i] = 1 - g->bits[i];
    }
}

int main(int argc, char **argv) {
    int pop_size = DEFAULT_POP;
    int n_gen = DEFAULT_GEN;
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    double mutate_rate = DEFAULT_MUTATE;
    uint64_t seed = (uint64_t)time(NULL);
    int n_grids = 2;
    int has_conn = 0;
    Edge conn_se = EDGE_TOP, conn_de = EDGE_TOP;
    int conn_src = 0, conn_dst = 0;
    ScaleMode scale_mode = SCALE_WRAP;
    void (*inject_fn)(System*, int) = inject_glider;
    Edge target_edge = EDGE_RIGHT;
    const char *seed_pattern = NULL;
    int do_census = 0;
    int strict_census = 0;
    const char *pipeline_name = NULL;
    int per_tick_census = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--pop") && i+1 < argc) pop_size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gen") && i+1 < argc) n_gen = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mutate") && i+1 < argc) mutate_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--grids") && i+1 < argc) n_grids = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--connect") && i+1 < argc) {
            const char *spec = argv[++i];
            char se[16], de[16];
            if (sscanf(spec, "%d:%15[^-]->%d:%15s", &conn_src, se, &conn_dst, de) == 4) {
                conn_se = edge_from_string(se);
                conn_de = edge_from_string(de);
                has_conn = 1;
            }
        }
        else if (!strcmp(argv[i], "--scaled")) scale_mode = SCALE_PROPORTIONAL;
        else if (!strcmp(argv[i], "--inject") && i+1 < argc) {
            const char *p = argv[++i];
            if (!strcmp(p, "glider")) inject_fn = inject_glider;
            else if (!strcmp(p, "blinker")) inject_fn = inject_blinker;
            else if (!strcmp(p, "block")) inject_fn = inject_block;
        }
        else if (!strcmp(argv[i], "--target") && i+1 < argc) {
            target_edge = edge_from_string(argv[++i]);
        }
        else if (!strcmp(argv[i], "--seed-pattern") && i+1 < argc) {
            seed_pattern = argv[++i];
        }
        else if (!strcmp(argv[i], "--census")) {
            do_census = 1;
        }
        else if (!strcmp(argv[i], "--strict-census")) {
            strict_census = 1;
            do_census = 1;
        }
        else if (!strcmp(argv[i], "--no-census-bonus")) {
            census_bonus = 0;
        }
        else if (!strcmp(argv[i], "--pipeline") && i+1 < argc) {
            pipeline_name = argv[++i];
        }
        else if (!strcmp(argv[i], "--per-tick-census")) {
            per_tick_census = 1;
        }
    }

    rng_seed(seed);
    patterns_set_strict(strict_census);

    int *sizes = malloc(n_grids * sizeof(int));
    for (int i = 0; i < n_grids; i++) sizes[i] = size;

    System sys;
    sys_init(&sys, n_grids, size);
    for (int g = 0; g < n_grids; g++)
        sys.grids[g]->rule_idx = rules_index_by_name("Conway's Life");
    if (pipeline_name) {
        if (!strcmp(pipeline_name, "default"))
            sys.pipeline = pipeline_preset_default();
        else if (!strcmp(pipeline_name, "intent"))
            sys.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
        else if (!strcmp(pipeline_name, "analyze"))
            sys.pipeline = pipeline_preset_analyze(INTENT_MODE_REPLACE, 0.0f);
        else if (!strcmp(pipeline_name, "parallel"))
            sys.pipeline = pipeline_preset_parallel(INTENT_MODE_REPLACE, 0.0f);
    }
    if (has_conn) {
        if (scale_mode == SCALE_PROPORTIONAL)
            coupling_connect_scaled(&sys.coupling, conn_src, conn_se, conn_dst, conn_de, scale_mode, 1.0f);
        else
            coupling_connect(&sys.coupling, conn_src, conn_se, conn_dst, conn_de);
    }

    Population pop;
    population_create(&pop, pop_size, n_grids, sizes);
    population_randomize(&pop);
    if (seed_pattern) {
        for (int i = 0; i < pop_size; i++)
            for (int gi = 0; gi < n_grids; gi++)
                stamp_pattern_into_genome(&pop.individuals[i], seed_pattern, gi, sizes[gi]);
    }

    printf("# GA-IC: Evolve initial conditions for signal transmission\n");
    printf("# pop=%d gen=%d size=%d steps=%d mutate=%.3f seed=%llu\n",
           pop_size, n_gen, size, steps, mutate_rate, (unsigned long long)seed);
    if (do_census)
        printf("gen,best_fitness,mean_fitness,gliders,oscillators\n");
    else
        printf("gen,best_fitness,mean_fitness\n");

    Genome *next_gen = malloc(pop_size * sizeof(Genome));
    for (int i = 0; i < pop_size; i++) {
        next_gen[i].bits = malloc(pop.nbits * sizeof(uint8_t));
        next_gen[i].nbits = pop.nbits;
    }

    for (int gen = 0; gen < n_gen; gen++) {
        double sum_f = 0;
        double best_f = -1;
        int tot_gliders = 0, tot_osc = 0;
        for (int i = 0; i < pop_size; i++) {
            pop.individuals[i].fitness = evaluate_fitness(&pop.individuals[i], &sys, sizes, n_grids,
                                                           steps, inject_fn, target_edge, 0);
            sum_f += pop.individuals[i].fitness;
            if (pop.individuals[i].fitness > best_f) best_f = pop.individuals[i].fitness;
            if (do_census) {
                genome_to_system(&pop.individuals[i], &sys, sizes, n_grids);
                for (int gi = 0; gi < n_grids; gi++) {
                    tot_gliders += census_gliders(sys.grids[gi]);
                    tot_osc += census_oscillators(sys.grids[gi]);
                }
            }
        }
        if (do_census)
            printf("%d,%.6f,%.6f,%d,%d\n", gen, best_f, sum_f / pop_size, tot_gliders, tot_osc);
        else
            printf("%d,%.6f,%.6f\n", gen, best_f, sum_f / pop_size);

        /* Elitism */
        for (int e = 0; e < ELITE_COUNT; e++) {
            int best_idx = 0;
            for (int i = 1; i < pop_size; i++)
                if (pop.individuals[i].fitness > pop.individuals[best_idx].fitness)
                    best_idx = i;
            memcpy(next_gen[e].bits, pop.individuals[best_idx].bits, pop.nbits * sizeof(uint8_t));
            next_gen[e].fitness = pop.individuals[best_idx].fitness;
            pop.individuals[best_idx].fitness = -1; /* mark as used */
        }

        /* Tournament selection + crossover + mutation */
        for (int i = ELITE_COUNT; i < pop_size; i++) {
            int p1 = tournament_select(&pop);
            int p2 = tournament_select(&pop);
            crossover(&pop.individuals[p1], &pop.individuals[p2], &next_gen[i]);
            mutate(&next_gen[i], mutate_rate);
        }

        /* Swap generations */
        for (int i = 0; i < pop_size; i++) {
            memcpy(pop.individuals[i].bits, next_gen[i].bits, pop.nbits * sizeof(uint8_t));
            pop.individuals[i].fitness = next_gen[i].fitness;
        }
    }

    /* Final evaluation: report best genome details */
    int best_idx = 0;
    for (int i = 1; i < pop_size; i++)
        if (pop.individuals[i].fitness > pop.individuals[best_idx].fitness)
            best_idx = i;

    printf("\n# Best genome fitness: %.6f\n", pop.individuals[best_idx].fitness);
    printf("# Best grid states after injection:\n");
    genome_to_system(&pop.individuals[best_idx], &sys, sizes, n_grids);
    inject_fn(&sys, 0);

    if (per_tick_census && pipeline_name && !strcmp(pipeline_name, "analyze")) {
        sys.pipeline = pipeline_preset_analyze(INTENT_MODE_REPLACE, 0.0f);
        sys.metrics_history = metrics_history_create(n_grids, steps + 1);
    }

    for (int s = 0; s < steps; s++) sys_step(&sys);

    if (per_tick_census && sys.metrics_history && sys.metrics_history->length > 0) {
        printf("\n# Per-tick census for best genome\n");
        printf("tick,grid,density,entropy,activity,gliders,oscillators\n");
        for (int t = 0; t < sys.metrics_history->length; t++) {
            for (int g = 0; g < n_grids; g++) {
                printf("%d,%d,%.4f,%.4f,%.4f,%d,%d\n", t, g,
                       metrics_history_density(sys.metrics_history, t, g),
                       metrics_history_entropy(sys.metrics_history, t, g),
                       metrics_history_activity(sys.metrics_history, t, g),
                       metrics_history_gliders(sys.metrics_history, t, g),
                       metrics_history_oscillators(sys.metrics_history, t, g));
            }
        }
    }

    for (int g = 0; g < n_grids; g++) {
        int gz = sys.grids[g]->size;
        printf("# Grid %d (%dx%d):\n", g, gz, gz);
        for (int y = 0; y < gz; y++) {
            printf("# ");
            for (int x = 0; x < gz; x++)
                printf("%c", sys.grids[g]->cells[y*gz+x] ? 'O' : '.');
            printf("\n");
        }
    }

    /* Cleanup */
    for (int i = 0; i < pop_size; i++) {
        free(next_gen[i].bits);
    }
    free(next_gen);
    population_destroy(&pop);
    sys_destroy(&sys);
    free(sizes);
    return 0;
}
