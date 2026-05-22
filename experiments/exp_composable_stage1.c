/*
 * exp_composable_stage1: Stage 1 of the 3-stage hierarchical GA.
 *
 * Loads a frozen Stage-0 ruleset R* (from --load) and evolves a per-cell
 * 4-mode orientation field to maximize min-over-modes propagation
 * (signal-highway fitness).
 *
 * Genome: 48-bit CellGenome per cell. Only orientation bits mutate;
 *   rule_select / coupling_weight / alt_rule / dir_mask are fixed to 0
 *   (the ruleset is global, separate from CellGenome).
 *
 * Output:
 *   stdout CSV trace per generation
 *   --save FILE  saves per-cell orientations as { int32 size; CellGenome[size*size]; }
 *
 * See COMPOSABLE_RULESET_ROADMAP.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ca_core/grid.h"
#include "../ca_core/genome.h"
#include "../ca_core/composable_ruleset.h"
#include "../utils/rng.h"

#define DEF_SIZE   32
#define DEF_POP    200
#define DEF_GENS   300
#define DEF_STEPS  50
#define DEF_SEED   42
#define DEF_ELITE  4
#define DEF_TOUR   3
#define DEF_MUT    0.05
#define NMODES     4

typedef enum { TRACE_HORIZ, TRACE_VERT, TRACE_ADIAG, TRACE_DIAG } TraceType;

static int is_source(int x, int y, int sz, TraceType t) {
    int h = sz / 4;
    switch (t) {
        case TRACE_HORIZ: return x == 0;
        case TRACE_VERT:  return y == 0;
        case TRACE_ADIAG: return (x < h) && (y >= sz - h);
        case TRACE_DIAG:  return (x < h) && (y < h);
    }
    return 0;
}

static int is_sink(int x, int y, int sz, TraceType t) {
    int h = sz / 4;
    switch (t) {
        case TRACE_HORIZ: return x == sz - 1;
        case TRACE_VERT:  return y == sz - 1;
        case TRACE_ADIAG: return (x >= sz - h) && (y < h);
        case TRACE_DIAG:  return (x >= sz - h) && (y >= sz - h);
    }
    return 0;
}

static void inject_source(Grid *g, TraceType t) {
    int sz = g->size;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
            if (is_source(x, y, sz, t)) g->cells[y * sz + x] = 1;
}

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

/* Evaluate one mode given a per-cell orientation field already loaded
 * into g->genomes. */
static double evaluate_mode(const Ruleset *r, Grid *g, TraceType t,
                            int eval_steps, uint8_t *refr_buf) {
    int sz = g->size;
    int ncells = sz * sz;
    memset(g->cells, 0, ncells);
    memset(g->next_cells, 0, ncells);
    if (refr_buf) memset(refr_buf, 0, ncells);
    /* Single-pulse injection (matches Stage 0). */
    inject_source(g, t);
    for (int s = 0; s < eval_steps; s++) {
        ruleset_step_grid(r, g, /*mode=*/(int)t, refr_buf);
    }
    return measure_sink(g, t);
}

/* Fitness: min over modes (signal-highway hard constraint). */
static double evaluate_individual(const Ruleset *r, Grid *g,
                                  const CellGenome *genomes,
                                  int eval_steps, uint8_t *refr_buf,
                                  double per_mode[NMODES]) {
    int ncells = g->size * g->size;
    memcpy(g->genomes, genomes, ncells * sizeof(CellGenome));
    double min_f = 1.0;
    for (int m = 0; m < NMODES; m++) {
        double f = evaluate_mode(r, g, (TraceType)m, eval_steps, refr_buf);
        if (f < min_f) min_f = f;
        if (per_mode) per_mode[m] = f;
    }
    return min_f;
}

typedef struct {
    CellGenome *genomes;
    double fitness;
    double per_mode[NMODES];
} Individual;

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --load R_star.bin [options]\n"
        "  --load FILE      load Stage-0 ruleset (8 bytes packed uint64_t) [REQUIRED]\n"
        "  --size N         grid size (default %d)\n"
        "  --pop N          population (default %d)\n"
        "  --gens N         generations (default %d)\n"
        "  --steps N        eval steps per mode (default %d)\n"
        "  --mut R          per-cell per-orientation mutation rate (default %.2f)\n"
        "  --elite N        elitism (default %d)\n"
        "  --tour K         tournament size (default %d)\n"
        "  --seed N         RNG seed (default %d)\n"
        "  --save FILE      save winning orientation field\n"
        "  --help\n",
        prog, DEF_SIZE, DEF_POP, DEF_GENS, DEF_STEPS, DEF_MUT, DEF_ELITE, DEF_TOUR, DEF_SEED);
}

/* Mutate orientation fields only. */
static CellGenome mutate_orient(CellGenome g, double per_bit_prob, uint64_t (*rng)(void)) {
    /* Mutate each of the 4 mode orientations independently. */
    int o[4];
    o[0] = (int)GENOME_ORIENT_MODE0(g);
    o[1] = (int)GENOME_ORIENT_MODE1(g);
    o[2] = (int)GENOME_ORIENT_MODE2(g);
    o[3] = (int)GENOME_ORIENT_MODE3(g);
    for (int m = 0; m < 4; m++) {
        double u = (double)(rng() & 0xFFFF) / 65536.0;
        if (u < per_bit_prob) {
            o[m] = (int)(rng() & 7);
        }
    }
    return genome_pack_full(0, 0, 0, o[0], o[1], o[2], o[3], 0, 0);
}

/* Uniform crossover: per-cell, take orientations from either parent. */
static CellGenome cx_orient(CellGenome a, CellGenome b, uint64_t (*rng)(void)) {
    return (rng() & 1) ? a : b;
}

int main(int argc, char **argv) {
    int size = DEF_SIZE;
    int pop_sz = DEF_POP;
    int gens = DEF_GENS;
    int steps = DEF_STEPS;
    double mut_rate = DEF_MUT;
    int elite = DEF_ELITE;
    int tour_k = DEF_TOUR;
    uint64_t seed = DEF_SEED;
    const char *load_path = NULL;
    const char *save_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--load") && i+1 < argc) load_path = argv[++i];
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pop") && i+1 < argc) pop_sz = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i+1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mut") && i+1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i+1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i+1 < argc) tour_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--save") && i+1 < argc) save_path = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (!load_path) {
        fprintf(stderr, "Error: --load R_star.bin is required\n");
        usage(argv[0]);
        return 1;
    }

    /* Load ruleset from file. */
    Ruleset R_star;
    {
        FILE *fp = fopen(load_path, "rb");
        if (!fp) { perror(load_path); return 1; }
        uint64_t packed = 0;
        if (fread(&packed, sizeof(packed), 1, fp) != 1) {
            fprintf(stderr, "Error: failed to read 8 bytes from %s\n", load_path);
            fclose(fp); return 1;
        }
        fclose(fp);
        R_star = ruleset_unpack(packed);
    }
    fprintf(stderr, "# Loaded R* from %s: ", load_path);
    ruleset_print(&R_star);

    rng_seed(seed);

    int ncells = size * size;
    Grid *g = grid_create(size);
    g->active = 1;
    g->rng_state = seed ^ 0xC0FFEE1234ULL;
    grid_alloc_genomes(g);

    uint8_t *refr = calloc(ncells, 1);

    Individual *pop = calloc(pop_sz, sizeof(Individual));
    Individual *next = calloc(pop_sz, sizeof(Individual));
    for (int i = 0; i < pop_sz; i++) {
        pop[i].genomes = calloc(ncells, sizeof(CellGenome));
        next[i].genomes = calloc(ncells, sizeof(CellGenome));
        /* Random orientations per cell per mode. */
        for (int c = 0; c < ncells; c++) {
            int o0 = (int)(rng_u64() & 7);
            int o1 = (int)(rng_u64() & 7);
            int o2 = (int)(rng_u64() & 7);
            int o3 = (int)(rng_u64() & 7);
            pop[i].genomes[c] = genome_pack_full(0, 0, 0, o0, o1, o2, o3, 0, 0);
        }
    }

    printf("# exp_composable_stage1: size=%d pop=%d gens=%d steps=%d mut=%.3f seed=%llu\n",
           size, pop_sz, gens, steps, mut_rate, (unsigned long long)seed);
    printf("# R* = B=0x%02X S=0x%02X aniso=%u card=%u refr=%u noise=%u meta=0x%02X\n",
           R_star.p[0], R_star.p[1], R_star.p[2], R_star.p[3],
           R_star.p[4] >> 5, R_star.p[5], R_star.p[6]);
    printf("gen,best_fitness,avg_fitness,reach_h,reach_v,reach_a,reach_d\n");

    for (int gi = 0; gi < gens; gi++) {
        double avg = 0.0, best = -1.0;
        int best_i = 0;
        for (int i = 0; i < pop_sz; i++) {
            pop[i].fitness = evaluate_individual(&R_star, g, pop[i].genomes,
                                                 steps, refr, pop[i].per_mode);
            avg += pop[i].fitness;
            if (pop[i].fitness > best) { best = pop[i].fitness; best_i = i; }
        }
        avg /= pop_sz;
        printf("%d,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f\n",
               gi, best, avg,
               pop[best_i].per_mode[0], pop[best_i].per_mode[1],
               pop[best_i].per_mode[2], pop[best_i].per_mode[3]);
        fflush(stdout);

        if (gi == gens - 1) break;
        if (best >= 0.9999) {
            fprintf(stderr, "# converged at gen %d\n", gi);
            break;
        }

        /* Sort descending for elitism. */
        /* Simple insertion sort on fitness (population small). */
        for (int i = 1; i < pop_sz; i++) {
            Individual key = pop[i];
            int j = i - 1;
            while (j >= 0 && pop[j].fitness < key.fitness) { pop[j+1] = pop[j]; j--; }
            pop[j+1] = key;
        }
        /* Elitism: copy top-elite (the genomes pointer in next is reused). */
        for (int i = 0; i < elite && i < pop_sz; i++) {
            memcpy(next[i].genomes, pop[i].genomes, ncells * sizeof(CellGenome));
            next[i].fitness = pop[i].fitness;
        }
        /* Tournament-select parents, uniform crossover, mutate. */
        for (int i = elite; i < pop_sz; i++) {
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
            for (int c = 0; c < ncells; c++) {
                CellGenome child = cx_orient(pop[pa].genomes[c], pop[pb].genomes[c], rng_u64);
                child = mutate_orient(child, mut_rate, rng_u64);
                next[i].genomes[c] = child;
            }
            next[i].fitness = 0.0;
        }
        /* Swap pop <-> next. */
        Individual *tmp = pop; pop = next; next = tmp;
    }

    /* Final best. */
    int best_i = 0;
    for (int i = 1; i < pop_sz; i++)
        if (pop[i].fitness > pop[best_i].fitness) best_i = i;
    fprintf(stderr, "# Final best fitness: %.4f  per-mode: %.3f %.3f %.3f %.3f\n",
            pop[best_i].fitness,
            pop[best_i].per_mode[0], pop[best_i].per_mode[1],
            pop[best_i].per_mode[2], pop[best_i].per_mode[3]);

    if (save_path) {
        FILE *fp = fopen(save_path, "wb");
        if (fp) {
            int32_t sz32 = size;
            fwrite(&sz32, sizeof(sz32), 1, fp);
            fwrite(pop[best_i].genomes, sizeof(CellGenome), ncells, fp);
            fclose(fp);
            fprintf(stderr, "# Saved orientations to %s\n", save_path);
        } else {
            perror(save_path);
        }
    }

    for (int i = 0; i < pop_sz; i++) { free(pop[i].genomes); free(next[i].genomes); }
    free(pop); free(next); free(refr);
    grid_destroy(g);
    return 0;
}
