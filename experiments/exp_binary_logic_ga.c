/*
 * exp_binary_logic_ga: Evolve per-cell genomes on a binary grid to compute
 * a target logic gate (AND / OR / XOR / NAND).
 *
 * Approach: 2-grid population-level GA.
 *   Grid 0 = computation grid with per-cell genomes.
 *   Grid 1 = input buffer (left half = A, right half = B), reset each tick.
 *   Coupling: grid 1 right edge -> grid 0 left edge.
 *   Fitness = fraction of correct truth-table outputs on grid 0's right edge.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/coupling.h"
#include "../ca_core/genome.h"
#include "../ca_core/pipeline.h"
#include "../utils/rng.h"

#define DEF_SIZE       16
#define DEF_POP        30
#define DEF_GENS       50
#define DEF_STEPS      30
#define DEF_SEED       42
#define DEF_TOURN_K    3
#define DEF_MUT_RATE   0.05
#define DEF_ELITE      2

#define FIT_RAW     0
#define FIT_SHAPED  1

typedef struct {
    CellGenome *genomes; /* size*size */
    double fitness;
    int truth;           /* 4-bit truth table observed */
} Individual;

typedef struct {
    int size;
    int ncells;
    int pop_size;
    Individual *pop;
    Individual *next;
} Population;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --gate and|or|xor|nand   Target gate (default and)\n");
    fprintf(stderr, "  --size N                 Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --pop N                  Population size (default %d)\n", DEF_POP);
    fprintf(stderr, "  --gens N                 Generations (default %d)\n", DEF_GENS);
    fprintf(stderr, "  --steps N                Evaluation steps per truth-table test (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N                 RNG seed (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --mut-rate R             Per-field mutation probability (default %.2f)\n", DEF_MUT_RATE);
    fprintf(stderr, "  --tour K                 Tournament size (default %d)\n", DEF_TOURN_K);
    fprintf(stderr, "  --elite N                Elitism count (default %d)\n", DEF_ELITE);
    fprintf(stderr, "  --fitness raw|shaped     Fitness mode (default shaped: per-class balanced + constant-output penalty)\n");
    fprintf(stderr, "  --out-mode strict|density Output-read mode (default density: count > expected from grid mean)\n");
    fprintf(stderr, "  --save-genomes FILE        Save best genome to binary file\n");
    fprintf(stderr, "  --help\n");
}

/* Set left half of grid as input A, right half as input B */
static void set_input(Grid *g, int a, int b) {
    int sz = g->size, half = sz / 2;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < half; x++) g->cells[y * sz + x] = (uint8_t)a;
        for (int x = half; x < sz; x++) g->cells[y * sz + x] = (uint8_t)b;
    }
}

#define OUT_STRICT  0
#define OUT_DENSITY 1

/* Read output from top half of rightmost column of grid 0.
   OUT_STRICT  = legacy hard threshold (count > half/2).
   OUT_DENSITY = relative: count_top > expected_from_grid_mean. Scale-invariant;
                 fires whenever the output edge has above-average activity. */
static int read_output(const Grid *g, int mode) {
    int sz = g->size, half = sz / 2;
    int c = 0;
    for (int y = 0; y < half; y++) c += g->cells[y * sz + (sz - 1)];
    if (mode == OUT_STRICT) return (c > half / 2) ? 1 : 0;

    int total = 0;
    int n = sz * sz;
    for (int i = 0; i < n; i++) total += g->cells[i];
    /* Expected count if alive cells were uniformly distributed across the
       half-column slot we just sampled: total * half / n. Compare strictly
       so a fully-dead or fully-alive grid both return 0. */
    int expected_num = total * half;
    int sampled_num  = c * n;
    return (sampled_num > expected_num) ? 1 : 0;
}

/* Evaluate one individual (genome set already copied into sys->grids[0]).
   Returns scalar fitness in [0,1]. Writes observed 4-bit truth table into *out_truth.

   Fitness modes:
     FIT_RAW    = correct/4 (legacy)
     FIT_SHAPED = per-class balanced (each class contributes 0.5)
                  + 0.25x penalty if observed truth is constant (0x0 or 0xF).
                  This removes the structural bias of the raw fraction (AND's
                  3-zero advantage) and breaks the always-0 attractor basin. */
static double evaluate(System *sys, const int *expected, int eval_steps,
                       int fit_mode, int out_mode,
                       const uint8_t *bk0, const uint8_t *bk1,
                       int *out_truth) {
    int sz = sys->grids[0]->size;
    int ncells = sz * sz;

    int correct = 0;
    int truth = 0;
    int n_ones = 0, n_zeros = 0;
    for (int i = 0; i < 4; i++) { if (expected[i]) n_ones++; else n_zeros++; }
    double w_one  = n_ones  > 0 ? 0.5 / n_ones  : 0.0;
    double w_zero = n_zeros > 0 ? 0.5 / n_zeros : 0.0;
    double shaped = 0.0;

    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            /* Restore initial random state */
            memcpy(sys->grids[0]->cells, bk0, ncells);
            memcpy(sys->grids[1]->cells, bk1, ncells);
            for (int st = 0; st < eval_steps; st++) {
                pipeline_execute(sys->pipeline, sys);
                set_input(sys->grids[1], a, b);
            }
            int o = read_output(sys->grids[0], out_mode);
            int idx = (a << 1) | b;
            truth |= (o << idx);
            if (o == expected[idx]) {
                correct++;
                shaped += expected[idx] ? w_one : w_zero;
            }
        }
    }
    if (out_truth) *out_truth = truth;

    if (fit_mode == FIT_RAW) return (double)correct / 4.0;

    /* Anti-attractor penalty: constant outputs are heavily discounted. */
    if (truth == 0x0 || truth == 0xF) shaped *= 0.25;
    return shaped;
}

static Population *pop_create(int size, int pop_size) {
    Population *p = calloc(1, sizeof(Population));
    if (!p) return NULL;
    p->size = size;
    p->ncells = size * size;
    p->pop_size = pop_size;
    p->pop = calloc(pop_size, sizeof(Individual));
    p->next = calloc(pop_size, sizeof(Individual));
    if (!p->pop || !p->next) { free(p); return NULL; }
    for (int i = 0; i < pop_size; i++) {
        p->pop[i].genomes = calloc(p->ncells, sizeof(CellGenome));
        p->next[i].genomes = calloc(p->ncells, sizeof(CellGenome));
        if (!p->pop[i].genomes || !p->next[i].genomes) return NULL;
    }
    return p;
}

static void pop_destroy(Population *p) {
    if (!p) return;
    for (int i = 0; i < p->pop_size; i++) {
        free(p->pop[i].genomes);
        free(p->next[i].genomes);
    }
    free(p->pop);
    free(p->next);
    free(p);
}

static void pop_randomize(Population *p, uint64_t (*rng)(void)) {
    for (int i = 0; i < p->pop_size; i++)
        for (int j = 0; j < p->ncells; j++)
            p->pop[i].genomes[j] = genome_random(rng);
}

static void pop_evaluate(Population *p, System *sys,
                         const int *expected, int eval_steps,
                         int fit_mode, int out_mode,
                         const uint8_t *bk0, const uint8_t *bk1) {
    for (int i = 0; i < p->pop_size; i++) {
        memcpy(sys->grids[0]->genomes, p->pop[i].genomes,
               p->ncells * sizeof(CellGenome));
        p->pop[i].fitness = evaluate(sys, expected, eval_steps, fit_mode,
                                     out_mode, bk0, bk1, &p->pop[i].truth);
    }
}

static int find_best(const Population *p) {
    int best = 0;
    for (int i = 1; i < p->pop_size; i++)
        if (p->pop[i].fitness > p->pop[best].fitness) best = i;
    return best;
}

static void pop_breed(Population *p, int elite, int tourn_k,
                      double mut_rate, int mutate_mask,
                      uint64_t (*rng)(void)) {
    int best = find_best(p);
    for (int i = 0; i < elite && i < p->pop_size; i++)
        memcpy(p->next[i].genomes, p->pop[best].genomes,
               p->ncells * sizeof(CellGenome));

    double *fit = malloc(p->pop_size * sizeof(double));
    for (int i = 0; i < p->pop_size; i++) fit[i] = p->pop[i].fitness;

    for (int i = elite; i < p->pop_size; i++) {
        int pa = genome_tournament_select(p->pop[0].genomes, fit,
                                           p->pop_size, tourn_k, rng);
        int pb = genome_tournament_select(p->pop[0].genomes, fit,
                                           p->pop_size, tourn_k, rng);
        for (int j = 0; j < p->ncells; j++) {
            CellGenome child = genome_crossover(p->pop[pa].genomes[j],
                                                 p->pop[pb].genomes[j], rng);
            child = genome_mutate(child, mut_rate, mutate_mask, rng);
            p->next[i].genomes[j] = child;
        }
    }
    free(fit);
    Individual *tmp = p->pop;
    p->pop = p->next;
    p->next = tmp;
}

static int parse_gate(const char *s, int *expected) {
    if (!strcmp(s, "and")) {
        expected[0]=0; expected[1]=0; expected[2]=0; expected[3]=1; return 0;
    }
    if (!strcmp(s, "or")) {
        expected[0]=0; expected[1]=1; expected[2]=1; expected[3]=1; return 0;
    }
    if (!strcmp(s, "xor")) {
        expected[0]=0; expected[1]=1; expected[2]=1; expected[3]=0; return 0;
    }
    if (!strcmp(s, "nand")) {
        expected[0]=1; expected[1]=1; expected[2]=1; expected[3]=0; return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    int size = DEF_SIZE, pop = DEF_POP, gens = DEF_GENS;
    int steps = DEF_STEPS, tourn_k = DEF_TOURN_K, elite = DEF_ELITE;
    double mut_rate = DEF_MUT_RATE;
    uint64_t seed = DEF_SEED;
    int expected[4] = {0, 0, 0, 1};
    int mutate_mask = GENOME_MUTATE_RULE | GENOME_MUTATE_WEIGHT;
    int fit_mode = FIT_SHAPED;
    int out_mode = OUT_DENSITY;
    const char *save_genomes = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gate") && i+1 < argc) {
            if (parse_gate(argv[++i], expected) < 0) {
                fprintf(stderr, "Unknown gate: %s\n", argv[i]); return 1;
            }
        } else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pop") && i+1 < argc) pop = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i+1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--mut-rate") && i+1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i+1 < argc) tourn_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i+1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--evolve-mutation")) mutate_mask |= GENOME_MUTATE_MUTATION;
        else if (!strcmp(argv[i], "--fitness") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "raw")) fit_mode = FIT_RAW;
            else if (!strcmp(m, "shaped")) fit_mode = FIT_SHAPED;
            else { fprintf(stderr, "Unknown fitness mode: %s\n", m); return 1; }
        }
        else if (!strcmp(argv[i], "--out-mode") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "strict")) out_mode = OUT_STRICT;
            else if (!strcmp(m, "density")) out_mode = OUT_DENSITY;
            else { fprintf(stderr, "Unknown out-mode: %s\n", m); return 1; }
        }
        else if (!strcmp(argv[i], "--save-genomes") && i+1 < argc) {
            save_genomes = argv[++i];
        }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    rng_seed(seed);

    /* 2-grid system: grid 0 = computation, grid 1 = input buffer */
    System sys;
    sys_init(&sys, 2, size);
    sys.grids[0]->active = 1;
    sys.grids[0]->rule_idx = 0;  /* Conway's Life default */
    sys.grids[1]->active = 1;
    sys.grids[1]->rule_idx = 0;

    /* Coupling: grid 1 right edge -> grid 0 left edge */
    coupling_connect(&sys.coupling, 0, EDGE_LEFT, 1, EDGE_RIGHT);

    grid_alloc_genomes(sys.grids[0]);
    sys_randomize(&sys, rng_u64);

    /* Pipeline: genomic step + exchange, no local GA */
    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f,
                                           0, -1, 0, 0, 1.0,
                                           GENOME_MUTATE_ALL);

    Population *popu = pop_create(size, pop);
    if (!popu) { fprintf(stderr, "Failed to create population\n"); return 1; }
    pop_randomize(popu, rng_u64);

    /* Capture IC once for all individuals to avoid drift across evaluations. */
    int ncells_main = size * size;
    uint8_t *bk0_main = malloc(ncells_main);
    uint8_t *bk1_main = malloc(ncells_main);
    if (!bk0_main || !bk1_main) { fprintf(stderr, "OOM\n"); return 1; }
    memcpy(bk0_main, sys.grids[0]->cells, ncells_main);
    memcpy(bk1_main, sys.grids[1]->cells, ncells_main);

    printf("# exp_binary_logic_ga: evolve binary grid genomes for logic gate\n");
    printf("# size=%d pop=%d gens=%d steps=%d seed=%llu mut_rate=%.3f tour=%d elite=%d fitness=%s out=%s\n",
           size, pop, gens, steps, (unsigned long long)seed, mut_rate, tourn_k, elite,
           fit_mode == FIT_RAW ? "raw" : "shaped",
           out_mode == OUT_STRICT ? "strict" : "density");
    printf("gen,best_fitness,avg_fitness,best_truth\n");

    for (int g = 0; g < gens; g++) {
        pop_evaluate(popu, &sys, expected, steps, fit_mode, out_mode, bk0_main, bk1_main);
        double best_f = 0.0, avg_f = 0.0;
        int best_idx = 0;
        for (int i = 0; i < popu->pop_size; i++) {
            avg_f += popu->pop[i].fitness;
            if (popu->pop[i].fitness > best_f) {
                best_f = popu->pop[i].fitness; best_idx = i;
            }
        }
        avg_f /= popu->pop_size;
        printf("%d,%.4f,%.4f,0x%X\n", g, best_f, avg_f, popu->pop[best_idx].truth);
        if (best_f >= 0.9999) break;
        pop_breed(popu, elite, tourn_k, mut_rate, mutate_mask, rng_u64);
    }

    pop_evaluate(popu, &sys, expected, steps, fit_mode, out_mode, bk0_main, bk1_main);
    int best = find_best(popu);
    printf("# Final best truth table: 0x%X (fitness=%.4f)\n",
           popu->pop[best].truth, popu->pop[best].fitness);
    printf("# Expected: ");
    for (int i = 0; i < 4; i++) printf("%d", expected[i]);
    printf("\n# Observed: ");
    for (int i = 0; i < 4; i++) printf("%d", (popu->pop[best].truth >> i) & 1);
    printf("\n");

    if (save_genomes) {
        FILE *fp = fopen(save_genomes, "wb");
        if (fp) {
            fwrite(&size, sizeof(int), 1, fp);
            fwrite(popu->pop[best].genomes, sizeof(CellGenome), ncells_main, fp);
            fclose(fp);
            printf("# Saved best genome to %s (%d cells)\n", save_genomes, ncells_main);
        } else {
            fprintf(stderr, "Warning: could not write %s\n", save_genomes);
        }
    }

    free(bk0_main);
    free(bk1_main);
    pop_destroy(popu);
    sys_destroy(&sys);
    return 0;
}
