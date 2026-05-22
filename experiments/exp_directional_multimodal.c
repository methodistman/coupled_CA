/*
 * exp_directional_multimodal: Multimodal logic with directional rule
 * modification. Each cell can switch to an alternate rule when neighbors
 * from masked cardinal directions (N,S,E,W) are active. Combined with
 * mode-specific orientations (48-bit genome), this lets a single cell
 * participate in multiple directional computations with different rules.
 *
 * Modes:
 *   0 = horizontal   (left edge  -> right edge)
 *   1 = vertical     (top edge   -> bottom edge)
 *   2 = anti-diagonal (top-right -> bottom-left)
 *   3 = diagonal     (top-left   -> bottom-right)
 *
 * Fitness = average accuracy across all 4 modes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/genome.h"
#include "../ca_core/pipeline.h"
#include "../utils/rng.h"

#define DEF_SIZE    32
#define DEF_POP     60
#define DEF_GENS    100
#define DEF_STEPS   50
#define DEF_SEED    42
#define DEF_TOURN_K 3
#define DEF_MUT_RATE 0.05
#define DEF_ELITE    2
#define NMODES       4

#define FIT_RAW     0
#define FIT_SHAPED  1

typedef struct {
    CellGenome *genomes;
    double fitness;
    int truth[NMODES];
} Individual;

typedef struct {
    int size, ncells, pop_size;
    Individual *pop, *next;
} Population;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N    Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --pop N     Population size (default %d)\n", DEF_POP);
    fprintf(stderr, "  --gens N    Generations (default %d)\n", DEF_GENS);
    fprintf(stderr, "  --steps N   Eval steps per test (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N    RNG seed (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --mut-rate R (default %.2f)\n", DEF_MUT_RATE);
    fprintf(stderr, "  --tour K    Tournament size (default %d)\n", DEF_TOURN_K);
    fprintf(stderr, "  --elite N   Elitism (default %d)\n", DEF_ELITE);
    fprintf(stderr, "  --fitness raw|shaped (default shaped)\n");
    fprintf(stderr, "  --save-genomes FILE\n");
    fprintf(stderr, "  --help\n");
}

static int gate_expected[4] = {0, 1, 1, 0}; /* default XOR for all modes */

/* ===== Trace definitions ===== */
typedef enum {
    TRACE_HORIZ,      /* left -> right */
    TRACE_VERT,       /* top -> bottom */
    TRACE_ADIAG,      /* anti-diagonal \  (x+y = const) */
    TRACE_DIAG        /* diagonal     /  (x-y = const) */
} TraceType;

/* Inject binary inputs A,B along two adjacent traces near the source corner */
static void set_input_trace(Grid *g, TraceType t, int a, int b) {
    int sz = g->size;
    int half = sz / 2;
    switch (t) {
        case TRACE_HORIZ:
            for (int y = 0; y < sz; y++)
                g->cells[y * sz + 0] = (y < half) ? (uint8_t)a : (uint8_t)b;
            break;
        case TRACE_VERT:
            for (int x = 0; x < sz; x++)
                g->cells[0 * sz + x] = (x < half) ? (uint8_t)a : (uint8_t)b;
            break;
        case TRACE_ADIAG: { /* top-right corner as source: x+y = sz-1 */
            int k = sz - 1;
            int n = 0;
            for (int x = sz - 1; x >= 0 && n < half; x--) {
                int y = k - x;
                if (y >= 0 && y < sz) {
                    g->cells[y * sz + x] = (n < half/2) ? (uint8_t)a : (uint8_t)b;
                    n++;
                }
            }
            break;
        }
        case TRACE_DIAG: { /* top-left corner as source: x = y */
            int n = 0;
            for (int i = 0; i < sz && n < half; i++) {
                g->cells[i * sz + i] = (n < half/2) ? (uint8_t)a : (uint8_t)b;
                n++;
            }
            break;
        }
    }
}

/* Read output from the opposite edge of the trace */
static int read_output_trace(const Grid *g, TraceType t) {
    int sz = g->size, half = sz / 2;
    int c = 0, total = 0, n = sz * sz;
    switch (t) {
        case TRACE_HORIZ:
            for (int y = 0; y < half; y++) c += g->cells[y * sz + (sz - 1)];
            break;
        case TRACE_VERT:
            for (int x = 0; x < half; x++) c += g->cells[(sz - 1) * sz + x];
            break;
        case TRACE_ADIAG: { /* bottom-left: x+y = sz-1, read from bottom half */
            int k = sz - 1;
            int nread = 0;
            for (int x = 0; x < sz && nread < half; x++) {
                int y = k - x;
                if (y >= 0 && y < sz) {
                    c += g->cells[y * sz + x];
                    nread++;
                }
            }
            break;
        }
        case TRACE_DIAG: { /* bottom-right: x = y, read from bottom half */
            int nread = 0;
            for (int i = sz - 1; i >= 0 && nread < half; i--) {
                c += g->cells[i * sz + i];
                nread++;
            }
            break;
        }
    }
    for (int i = 0; i < n; i++) total += g->cells[i];
    int expected_num = total * half;
    int sampled_num  = c * n;
    return (sampled_num > expected_num) ? 1 : 0;
}

/* ===== Evaluation ===== */
static double evaluate_mode(System *sys, const int *expected,
                            int eval_steps, TraceType t,
                            int *out_truth) {
    int sz = sys->grids[0]->size;
    int ncells = sz * sz;
    uint8_t *bk = malloc(ncells);
    memcpy(bk, sys->grids[0]->cells, ncells);

    int truth = 0;
    double shaped = 0.0;
    int n_ones = 0, n_zeros = 0;
    for (int i = 0; i < 4; i++) { if (expected[i]) n_ones++; else n_zeros++; }
    double w_one  = n_ones  > 0 ? 0.5 / n_ones  : 0.0;
    double w_zero = n_zeros > 0 ? 0.5 / n_zeros : 0.0;

    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            memcpy(sys->grids[0]->cells, bk, ncells);
            for (int st = 0; st < eval_steps; st++) {
                /* Use mode-specific orientation + directional rule switching */
                sys_step_genomic_mode(sys, (int)t, 1);
                set_input_trace(sys->grids[0], t, a, b);
            }
            int o = read_output_trace(sys->grids[0], t);
            int idx = (a << 1) | b;
            truth |= (o << idx);
            if (o == expected[idx]) shaped += expected[idx] ? w_one : w_zero;
        }
    }
    if (out_truth) *out_truth = truth;
    if (truth == 0x0 || truth == 0xF) shaped *= 0.25;
    free(bk);
    return shaped;
}

static double evaluate_individual(System *sys, const int *expected,
                                  int eval_steps, int fit_mode,
                                  const uint8_t *bk_cells,
                                  int *truths) {
    int ncells = sys->grids[0]->size * sys->grids[0]->size;
    double total = 0.0;
    for (int m = 0; m < NMODES; m++) {
        memcpy(sys->grids[0]->cells, bk_cells, ncells);
        total += evaluate_mode(sys, expected, eval_steps, (TraceType)m, &truths[m]);
    }
    if (fit_mode == FIT_RAW) {
        int correct = 0;
        for (int m = 0; m < NMODES; m++)
            for (int i = 0; i < 4; i++)
                correct += (((truths[m] >> i) & 1) == expected[i]);
        return (double)correct / (NMODES * 4.0);
    }
    return total / NMODES;
}

/* ===== GA Population ===== */
static Population *pop_create(int size, int pop_size) {
    Population *p = calloc(1, sizeof(Population));
    p->size = size; p->ncells = size*size; p->pop_size = pop_size;
    p->pop = calloc(pop_size, sizeof(Individual));
    p->next = calloc(pop_size, sizeof(Individual));
    for (int i = 0; i < pop_size; i++) {
        p->pop[i].genomes = calloc(p->ncells, sizeof(CellGenome));
        p->next[i].genomes = calloc(p->ncells, sizeof(CellGenome));
    }
    return p;
}

static void pop_destroy(Population *p) {
    for (int i = 0; i < p->pop_size; i++) {
        free(p->pop[i].genomes); free(p->next[i].genomes);
    }
    free(p->pop); free(p->next); free(p);
}

static void pop_randomize(Population *p, uint64_t (*rng)(void)) {
    for (int i = 0; i < p->pop_size; i++)
        for (int j = 0; j < p->ncells; j++) {
            int rule      = (int)(rng() % NUM_RULES);
            int weight    = (int)(rng() % 256);
            int o0        = (int)(rng() % 8);
            int o1        = (int)(rng() % 8);
            int o2        = (int)(rng() % 8);
            int o3        = (int)(rng() % 8);
            int alt_rule  = (int)(rng() % NUM_RULES);
            int dir_mask  = (int)(rng() % 16);
            p->pop[i].genomes[j] = genome_pack_full(rule, weight, 0,
                                                     o0, o1, o2, o3,
                                                     alt_rule, dir_mask);
        }
}

static void pop_evaluate(Population *p, System *sys, const int *expected,
                         int eval_steps, int fit_mode,
                         const uint8_t *bk_cells) {
    for (int i = 0; i < p->pop_size; i++) {
        memcpy(sys->grids[0]->genomes, p->pop[i].genomes,
               p->ncells * sizeof(CellGenome));
        p->pop[i].fitness = evaluate_individual(sys, expected, eval_steps,
                                                fit_mode, bk_cells,
                                                p->pop[i].truth);
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
    Individual *tmp = p->pop; p->pop = p->next; p->next = tmp;
}

/* ===== main ===== */
int main(int argc, char **argv) {
    int size = DEF_SIZE, pop = DEF_POP, gens = DEF_GENS;
    int steps = DEF_STEPS, tourn_k = DEF_TOURN_K, elite = DEF_ELITE;
    double mut_rate = DEF_MUT_RATE;
    uint64_t seed = DEF_SEED;
    int mutate_mask = GENOME_MUTATE_ALL;
    int fit_mode = FIT_SHAPED;
    const char *save_genomes = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pop") && i+1 < argc) pop = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i+1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--mut-rate") && i+1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i+1 < argc) tourn_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i+1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fitness") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "raw")) fit_mode = FIT_RAW;
            else if (!strcmp(m, "shaped")) fit_mode = FIT_SHAPED;
        } else if (!strcmp(argv[i], "--save-genomes") && i+1 < argc) save_genomes = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    rng_seed(seed);
    System sys;
    sys_init(&sys, 1, size);
    sys.grids[0]->active = 1;
    sys.grids[0]->rule_idx = 0;
    grid_alloc_genomes(sys.grids[0]);
    sys_randomize(&sys, rng_u64);

    /* No pipeline needed; sys_step_genomic_mode is called directly */
    sys.pipeline = NULL;

    Population *popu = pop_create(size, pop);
    pop_randomize(popu, rng_u64);

    int ncells = size * size;
    uint8_t *bk_cells = malloc(ncells);
    memcpy(bk_cells, sys.grids[0]->cells, ncells);

    printf("# exp_directional_multimodal: size=%d pop=%d gens=%d steps=%d seed=%llu\n",
           size, pop, gens, steps, (unsigned long long)seed);
    printf("# modes: 0=horiz 1=vert 2=anti-diag 3=diag (all target XOR=0110)\n");
    printf("# dir_rules: enabled (alt_rule + dir_mask per cell)\n");
    printf("gen,best_fitness,avg_fitness,best_t0,best_t1,best_t2,best_t3\n");

    for (int g = 0; g < gens; g++) {
        pop_evaluate(popu, &sys, gate_expected, steps, fit_mode, bk_cells);
        double best_f = 0.0, avg_f = 0.0;
        int best_idx = 0;
        for (int i = 0; i < popu->pop_size; i++) {
            avg_f += popu->pop[i].fitness;
            if (popu->pop[i].fitness > best_f) {
                best_f = popu->pop[i].fitness; best_idx = i;
            }
        }
        avg_f /= popu->pop_size;
        printf("%d,%.4f,%.4f", g, best_f, avg_f);
        for (int m = 0; m < NMODES; m++) printf(",0x%X", popu->pop[best_idx].truth[m]);
        printf("\n");
        if (best_f >= 0.9999) break;
        pop_breed(popu, elite, tourn_k, mut_rate, mutate_mask, rng_u64);
    }

    pop_evaluate(popu, &sys, gate_expected, steps, fit_mode, bk_cells);
    int best = find_best(popu);
    printf("# Final truths: ");
    for (int m = 0; m < NMODES; m++) printf("0x%X ", popu->pop[best].truth[m]);
    printf("(fit=%.4f)\n", popu->pop[best].fitness);

    if (save_genomes) {
        FILE *fp = fopen(save_genomes, "wb");
        if (fp) {
            fwrite(&size, sizeof(int), 1, fp);
            fwrite(popu->pop[best].genomes, sizeof(CellGenome), ncells, fp);
            fclose(fp);
            printf("# Saved genome to %s\n", save_genomes);
        }
    }

    free(bk_cells);
    pop_destroy(popu);
    sys_destroy(&sys);
    return 0;
}
