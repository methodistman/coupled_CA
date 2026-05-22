/*
 * exp_composable_stage0: Stage 0 of the 3-stage hierarchical GA.
 *
 * Evolves a 56-bit composable Ruleset DNA whose fitness is the average
 * fraction of "sink" cells reached by a source pulse, over K fixed
 * random orientation fields and all 4 modes.
 *
 * Output:
 *   stdout CSV trace of best/avg fitness per generation
 *   --save FILE  writes the winning ruleset as 8 bytes (packed uint64_t)
 *
 * See COMPOSABLE_RULESET_ROADMAP.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ca_core/engine.h"
#include "../ca_core/grid.h"
#include "../ca_core/genome.h"
#include "../ca_core/composable_ruleset.h"
#include "../utils/rng.h"

#define DEF_SIZE   32
#define DEF_POP    100
#define DEF_GENS   100
#define DEF_KFIELDS 8
#define DEF_STEPS  50
#define DEF_SEED   42
#define NMODES     4

typedef enum { TRACE_HORIZ, TRACE_VERT, TRACE_ADIAG, TRACE_DIAG } TraceType;

/* Source/sink geometry: source = "near" edge/corner; sink = "far" edge/corner.
 * Diagonal modes use square corners so all 4 modes test real propagation. */
static int is_source(int x, int y, int sz, TraceType t) {
    int h = sz / 4; /* corner radius for diagonal modes */
    switch (t) {
        case TRACE_HORIZ: return x == 0;
        case TRACE_VERT:  return y == 0;
        case TRACE_ADIAG: return (x < h) && (y >= sz - h); /* bottom-left */
        case TRACE_DIAG:  return (x < h) && (y < h);       /* top-left */
    }
    return 0;
}

static int is_sink(int x, int y, int sz, TraceType t) {
    int h = sz / 4;
    switch (t) {
        case TRACE_HORIZ: return x == sz - 1;
        case TRACE_VERT:  return y == sz - 1;
        case TRACE_ADIAG: return (x >= sz - h) && (y < h);  /* top-right */
        case TRACE_DIAG:  return (x >= sz - h) && (y >= sz - h); /* bottom-right */
    }
    return 0;
}

/* Inject the source set into the grid (set those cells to 1). */
static void inject_source(Grid *g, TraceType t) {
    int sz = g->size;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
            if (is_source(x, y, sz, t)) g->cells[y * sz + x] = 1;
}

/* Fraction of sink cells alive. */
static double measure_sink(const Grid *g, TraceType t) {
    int sz = g->size;
    int alive = 0, total = 0;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            if (is_sink(x, y, sz, t)) {
                total++;
                alive += g->cells[y * sz + x] ? 1 : 0;
            }
        }
    }
    return total > 0 ? (double)alive / total : 0.0;
}

/* Path coherence: fraction of cells along the canonical source->sink line
 * that are alive. A highway should have high coherence; a blob should have low. */
static double measure_path_coherence(const Grid *g, TraceType t) {
    int sz = g->size;
    int alive = 0, total = 0;
    switch (t) {
        case TRACE_HORIZ:
            for (int x = 0; x < sz; x++) {
                int y = sz / 2;
                total++;
                alive += g->cells[y * sz + x] ? 1 : 0;
            }
            break;
        case TRACE_VERT:
            for (int y = 0; y < sz; y++) {
                int x = sz / 2;
                total++;
                alive += g->cells[y * sz + x] ? 1 : 0;
            }
            break;
        case TRACE_ADIAG:
            for (int i = 0; i < sz; i++) {
                total++;
                alive += g->cells[(sz - 1 - i) * sz + i] ? 1 : 0;
            }
            break;
        case TRACE_DIAG:
            for (int i = 0; i < sz; i++) {
                total++;
                alive += g->cells[i * sz + i] ? 1 : 0;
            }
            break;
    }
    return total > 0 ? (double)alive / total : 0.0;
}

/* Load orientation field into genomes (mode-0 slot, steered to active mode). */
static void load_orient_field(Grid *g, const uint8_t *orient_field, int ncells) {
    for (int i = 0; i < ncells; i++) {
        uint8_t o = orient_field[i] & 7;
        g->genomes[i] = genome_pack_full(0, 0, 0, o, o, o, o, 0, 0);
    }
}

/* Evaluate a ruleset against one orientation field for one mode.
 * The orientation field is encoded into g->genomes' mode-0 slot for simplicity;
 * we steer 'mode' so the ruleset reads the same slot.
 *
 * Steps: inject source, simulate eval_steps, measure sink. */
static double evaluate_one(const Ruleset *r, Grid *g, const uint8_t *orient_field,
                           TraceType t, int eval_steps, uint8_t *refr_buf) {
    int sz = g->size;
    int ncells = sz * sz;
    /* Reset grid: zero cells, zero refractory, load orientation field. */
    memset(g->cells, 0, ncells);
    memset(g->next_cells, 0, ncells);
    if (refr_buf) memset(refr_buf, 0, ncells);
    load_orient_field(g, orient_field, ncells);
    /* Single-pulse injection: source seeded at t=0 only.
     * This forces propagation rules to genuinely traverse the grid;
     * sustained injection lets any rule trivially fill via repeated drive. */
    inject_source(g, t);

    for (int s = 0; s < eval_steps; s++) {
        ruleset_step_grid(r, g, /*mode=*/0, refr_buf);
    }
    return measure_sink(g, t);
}

/* Pulse-and-reset evaluation: clear grid every reset_interval ticks,
 * re-inject source, and measure whether the highway re-forms reliably.
 * Returns mean sink reach across all pulses.
 * If coherence_weight > 0, also returns mean path coherence via out param. */
static double evaluate_one_periodic(const Ruleset *r, Grid *g,
                                    const uint8_t *orient_field,
                                    TraceType t, int total_steps,
                                    int reset_interval,
                                    uint8_t *refr_buf,
                                    double *out_coherence) {
    int sz = g->size;
    int ncells = sz * sz;
    double cumulative_reach = 0.0;
    double cumulative_coherence = 0.0;
    int num_pulses = 0;

    load_orient_field(g, orient_field, ncells);

    for (int pulse = 0; pulse < total_steps; pulse += reset_interval) {
        memset(g->cells, 0, ncells);
        memset(g->next_cells, 0, ncells);
        if (refr_buf) memset(refr_buf, 0, ncells);
        inject_source(g, t);

        int run_steps = (pulse + reset_interval > total_steps)
                        ? (total_steps - pulse) : reset_interval;
        for (int s = 0; s < run_steps; s++)
            ruleset_step_grid(r, g, /*mode=*/0, refr_buf);

        cumulative_reach += measure_sink(g, t);
        if (out_coherence)
            cumulative_coherence += measure_path_coherence(g, t);
        num_pulses++;
    }

    if (out_coherence)
        *out_coherence = cumulative_coherence / num_pulses;
    return cumulative_reach / num_pulses;
}

/* Per-mode canonical aligned/adversarial orientation (uniform across the grid).
 * Encoding: 0=E,1=NE,2=N,3=NW,4=W,5=SW,6=S,7=SE.
 *
 *   HORIZ source=left → propagate E:  aligned=0 (E),    adversarial=2 (N)
 *   VERT  source=top  → propagate S:  aligned=6 (S),    adversarial=0 (E)
 *   ADIAG source=BL   → propagate NE: aligned=1 (NE),   adversarial=3 (NW)
 *   DIAG  source=TL   → propagate SE: aligned=7 (SE),   adversarial=5 (SW)
 */
static const uint8_t ALIGNED_ORIENT[4]     = {0, 6, 1, 7};
static const uint8_t ADVERSARIAL_ORIENT[4] = {2, 0, 3, 5};

/* Fill a field with a uniform orientation. */
static void fill_uniform(uint8_t *field, int ncells, uint8_t orient) {
    for (int i = 0; i < ncells; i++) field[i] = orient;
}

/* Generate K random orientation fields, seeded deterministically. */
static uint8_t **make_random_fields(int K, int ncells, uint64_t seed) {
    uint64_t state = seed;
    uint8_t **fields = malloc(K * sizeof(uint8_t *));
    for (int k = 0; k < K; k++) {
        fields[k] = malloc(ncells);
        for (int i = 0; i < ncells; i++) {
            uint64_t z = rng_splitmix64(&state);
            fields[k][i] = (uint8_t)(z & 7);
        }
    }
    return fields;
}

static void free_fields(uint8_t **fields, int K) {
    for (int k = 0; k < K; k++) free(fields[k]);
    free(fields);
}

/* Stage-0 fitness: reward propagation under aligned orientations,
 * penalize propagation under adversarial orientations, and bias toward
 * rules that propagate well on random fields too.
 *
 *   reach_aligned     = mean reach with all cells oriented toward sink (per mode)
 *   reach_adversarial = mean reach with all cells oriented perpendicular (per mode)
 *   reach_random      = mean reach over K random fields × 4 modes
 *
 * fitness = 0.5 · reach_aligned · (1 - reach_adversarial)  +  0.5 · reach_random · (1 - reach_adversarial)
 *
 * Bounded in [0, 1]. Maximum 1.0 requires perfect aligned propagation,
 * perfect adversarial blocking, and good random-field propagation.
 */
static double evaluate_ruleset(const Ruleset *r, Grid *g,
                               uint8_t **fields, int K, int eval_steps,
                               uint8_t *refr_buf, int fitness_version,
                               int reset_interval, double coherence_weight) {
    int sz = g->size;
    int ncells = sz * sz;
    uint8_t *uniform = malloc(ncells);

    double reach_aligned = 0.0, reach_adv = 0.0, reach_rand = 0.0;
    double coh_aligned = 0.0, coh_adv = 0.0, coh_rand = 0.0;
    int use_periodic = (reset_interval > 0 && reset_interval < eval_steps);

    for (int m = 0; m < NMODES; m++) {
        fill_uniform(uniform, ncells, ALIGNED_ORIENT[m]);
        if (use_periodic) {
            double c;
            reach_aligned += evaluate_one_periodic(r, g, uniform, (TraceType)m,
                                                   eval_steps, reset_interval,
                                                   refr_buf, coherence_weight > 0 ? &c : NULL);
            if (coherence_weight > 0) coh_aligned += c;
        } else {
            reach_aligned += evaluate_one(r, g, uniform, (TraceType)m, eval_steps, refr_buf);
        }

        fill_uniform(uniform, ncells, ADVERSARIAL_ORIENT[m]);
        if (use_periodic) {
            double c;
            reach_adv += evaluate_one_periodic(r, g, uniform, (TraceType)m,
                                               eval_steps, reset_interval,
                                               refr_buf, coherence_weight > 0 ? &c : NULL);
            if (coherence_weight > 0) coh_adv += c;
        } else {
            reach_adv += evaluate_one(r, g, uniform, (TraceType)m, eval_steps, refr_buf);
        }

        for (int k = 0; k < K; k++) {
            if (use_periodic) {
                double c;
                reach_rand += evaluate_one_periodic(r, g, fields[k], (TraceType)m,
                                                    eval_steps, reset_interval,
                                                    refr_buf, coherence_weight > 0 ? &c : NULL);
                if (coherence_weight > 0) coh_rand += c;
            } else {
                reach_rand += evaluate_one(r, g, fields[k], (TraceType)m, eval_steps, refr_buf);
            }
        }
    }
    reach_aligned /= NMODES;
    reach_adv     /= NMODES;
    reach_rand    /= (NMODES * K);

    double coherence = 0.0;
    if (coherence_weight > 0) {
        coh_aligned /= NMODES;
        coh_adv     /= NMODES;
        coh_rand    /= (NMODES * K);
        /* Blend coherence the same way as reach, but we only care about aligned coherence. */
        coherence = coh_aligned * (1.0 - coh_rand) * (1.0 - coh_adv);
        if (coherence < 0.0) coherence = 0.0;
    }

    free(uniform);

    double specificity = 1.0 - reach_adv;
    if (specificity < 0.0) specificity = 0.0;
    double random_discrim = 1.0 - reach_rand;
    if (random_discrim < 0.0) random_discrim = 0.0;

    double base;
    if (fitness_version == 3) {
        base = reach_aligned * random_discrim * random_discrim * specificity;
    } else {
        base = reach_aligned * random_discrim * specificity;
    }

    if (coherence_weight > 0) {
        return (1.0 - coherence_weight) * base + coherence_weight * coherence;
    }
    return base;
}

typedef struct {
    Ruleset r;
    double fitness;
} Individual;

static int cmp_fitness_desc(const void *a, const void *b) {
    double fa = ((const Individual *)a)->fitness;
    double fb = ((const Individual *)b)->fitness;
    if (fa > fb) return -1;
    if (fa < fb) return 1;
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --size N         grid size (default %d)\n"
        "  --pop N          population (default %d)\n"
        "  --gens N         generations (default %d)\n"
        "  --kfields N      number of random orientation fields per eval (default %d)\n"
        "  --steps N        eval steps per (field,mode) (default %d)\n"
        "  --mut R          per-byte mutation rate (default 0.15)\n"
        "  --elite N        elitism (default 2)\n"
        "  --tour K         tournament size (default 3)\n"
        "  --seed N         RNG seed (default %d)\n"
        "  --save FILE      save winning ruleset (8 bytes, packed uint64_t)\n"
        "  --seed-conway    seed pop with Conway + LWD + propagation-seed baselines\n"
        "  --reset-interval N  clear grid every N ticks (0=disabled, default 0)\n"
        "  --path-coherence W  weight for path-coherence bonus (0..1, default 0.0)\n"
        "  --help\n",
        prog, DEF_SIZE, DEF_POP, DEF_GENS, DEF_KFIELDS, DEF_STEPS, DEF_SEED);
}

int main(int argc, char **argv) {
    int size = DEF_SIZE;
    int pop_sz = DEF_POP;
    int gens = DEF_GENS;
    int K = DEF_KFIELDS;
    int steps = DEF_STEPS;
    double mut_rate = 0.15;
    int elite = 2;
    int tour_k = 3;
    uint64_t seed = DEF_SEED;
    const char *save_path = NULL;
    int seed_baselines = 0;
    int fitness_version = 2;
    int reset_interval = 0;
    double coherence_weight = 0.0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pop") && i+1 < argc) pop_sz = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i+1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--kfields") && i+1 < argc) K = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mut") && i+1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i+1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i+1 < argc) tour_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--save") && i+1 < argc) save_path = argv[++i];
        else if (!strcmp(argv[i], "--seed-conway")) seed_baselines = 1;
        else if (!strcmp(argv[i], "--fitness") && i+1 < argc) fitness_version = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reset-interval") && i+1 < argc) reset_interval = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--path-coherence") && i+1 < argc) coherence_weight = atof(argv[++i]);
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    rng_seed(seed);

    int ncells = size * size;
    Grid *g = grid_create(size);
    g->active = 1;
    g->rng_state = seed ^ 0xCAFEBABEULL;
    grid_alloc_genomes(g);

    uint8_t *refr = calloc(ncells, 1);

    /* Use seed+1 for field generation so it's reproducible and decoupled from GA RNG. */
    uint8_t **fields = make_random_fields(K, ncells, seed + 1);

    Individual *pop = calloc(pop_sz, sizeof(Individual));
    Individual *next = calloc(pop_sz, sizeof(Individual));

    /* Initialize population. */
    for (int i = 0; i < pop_sz; i++) pop[i].r = ruleset_random(rng_u64);
    if (seed_baselines && pop_sz >= 3) {
        pop[0].r = ruleset_conway();
        pop[1].r = ruleset_life_without_death();
        pop[2].r = ruleset_propagation_seed();
    }

    printf("# exp_composable_stage0: size=%d pop=%d gens=%d K=%d steps=%d mut=%.3f seed=%llu",
           size, pop_sz, gens, K, steps, mut_rate, (unsigned long long)seed);
    if (reset_interval > 0) printf(" reset=%d", reset_interval);
    if (coherence_weight > 0) printf(" coherence=%.2f", coherence_weight);
    printf("\n");
    printf("gen,best_fitness,avg_fitness,best_B,best_S,best_aniso,best_card,best_refr,best_noise,best_meta\n");

    for (int g_idx = 0; g_idx < gens; g_idx++) {
        /* Evaluate. */
        double avg = 0.0, best = -1.0;
        int best_i = 0;
        for (int i = 0; i < pop_sz; i++) {
            pop[i].fitness = evaluate_ruleset(&pop[i].r, g, fields, K, steps, refr,
                                                fitness_version, reset_interval, coherence_weight);
            avg += pop[i].fitness;
            if (pop[i].fitness > best) { best = pop[i].fitness; best_i = i; }
        }
        avg /= pop_sz;

        const Ruleset *br = &pop[best_i].r;
        printf("%d,%.4f,%.4f,0x%02X,0x%02X,%u,%u,%u,%u,0x%02X\n",
               g_idx, best, avg,
               br->p[0], br->p[1], br->p[2], br->p[3],
               br->p[4] >> 5, br->p[5], br->p[6]);
        fflush(stdout);

        if (g_idx == gens - 1) break;

        /* Selection: tournament + elitism. */
        /* Sort by fitness for elitism. */
        qsort(pop, pop_sz, sizeof(Individual), cmp_fitness_desc);
        for (int i = 0; i < elite && i < pop_sz; i++) next[i] = pop[i];

        for (int i = elite; i < pop_sz; i++) {
            /* Tournament select two parents. */
            int pa = (int)(rng_u64() % pop_sz);
            for (int t = 1; t < tour_k; t++) {
                int c = (int)(rng_u64() % pop_sz);
                if (pop[c].fitness > pop[pa].fitness) pa = c;
            }
            int pb = (int)(rng_u64() % pop_sz);
            for (int t = 1; t < tour_k; t++) {
                int c = (int)(rng_u64() % pop_sz);
                if (pop[c].fitness > pop[pb].fitness) pb = c;
            }
            Ruleset child = ruleset_crossover(pop[pa].r, pop[pb].r, rng_u64);
            child = ruleset_mutate(child, mut_rate, rng_u64);
            next[i].r = child;
            next[i].fitness = 0.0;
        }
        Individual *tmp = pop; pop = next; next = tmp;
    }

    /* Final best. */
    int best_i = 0;
    for (int i = 1; i < pop_sz; i++)
        if (pop[i].fitness > pop[best_i].fitness) best_i = i;
    fprintf(stderr, "# Final best fitness: %.4f\n", pop[best_i].fitness);
    fprintf(stderr, "# Final best ruleset: ");
    ruleset_print(&pop[best_i].r);

    if (save_path) {
        FILE *fp = fopen(save_path, "wb");
        if (fp) {
            uint64_t packed = ruleset_pack(&pop[best_i].r);
            fwrite(&packed, sizeof(packed), 1, fp);
            fclose(fp);
            fprintf(stderr, "# Saved ruleset to %s\n", save_path);
        } else {
            perror(save_path);
        }
    }

    free_fields(fields, K);
    free(pop); free(next); free(refr);
    grid_destroy(g);
    return 0;
}
