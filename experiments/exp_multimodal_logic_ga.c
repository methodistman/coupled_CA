/*
 * exp_multimodal_logic_ga: Evolve per-cell genomes on a single grid to
 * compute *two* distinct logic gates depending on which input pair is
 * injected.  This tests whether a single substrate can host multiple
 * computational modes — a stepping-stone to programmable matter and a
 * different compositional paradigm (spatial differentiation, not grid
 * chaining).
 *
 * Topology: single grid, per-cell genomes.
 *   Mode 0: input A injected at left edge, output read from right edge.
 *   Mode 1: input A injected at top edge,    output read from bottom edge.
 *
 * Fitness = average accuracy across both modes.
 * If the grid evolves to compute AND on the left→right axis and XOR on
 * the top→bottom axis, it demonstrates spatial multimodality.
 *
 * Usage:
 *   ./exp_multimodal_logic_ga --gate0 and --gate1 xor --size 24 ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/genome.h"
#include "../ca_core/pipeline.h"
#include "../utils/rng.h"

#define DEF_SIZE    24
#define DEF_POP     40
#define DEF_GENS    60
#define DEF_STEPS   40
#define DEF_SEED    42
#define DEF_TOURN_K 3
#define DEF_MUT_RATE 0.05
#define DEF_ELITE   2

#define FIT_RAW     0
#define FIT_SHAPED  1
#define OUT_DENSITY 1

typedef struct {
    CellGenome *genomes;
    double fitness;
    int truth[2];   /* truth[0]=mode0, truth[1]=mode1 */
} Individual;

typedef struct {
    int size, ncells, pop_size;
    Individual *pop, *next;
} Population;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --gate0 and|or|xor|nand   First gate (default and)\n");
    fprintf(stderr, "  --gate1 and|or|xor|nand   Second gate (default xor)\n");
    fprintf(stderr, "  --size N                 Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --pop N                  Population size (default %d)\n", DEF_POP);
    fprintf(stderr, "  --gens N                 Generations (default %d)\n", DEF_GENS);
    fprintf(stderr, "  --steps N                Eval steps per test (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N                 RNG seed (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --mut-rate R             (default %.2f)\n", DEF_MUT_RATE);
    fprintf(stderr, "  --tour K                 Tournament size (default %d)\n", DEF_TOURN_K);
    fprintf(stderr, "  --elite N                Elitism (default %d)\n", DEF_ELITE);
    fprintf(stderr, "  --fitness raw|shaped     (default shaped)\n");
    fprintf(stderr, "  --save-genomes FILE      Save best genome\n");
    fprintf(stderr, "  --help\n");
}

static int parse_gate(const char *s, int *exp) {
    if (!strcmp(s, "and"))  { exp[0]=0; exp[1]=0; exp[2]=0; exp[3]=1; return 0; }
    if (!strcmp(s, "or"))   { exp[0]=0; exp[1]=1; exp[2]=1; exp[3]=1; return 0; }
    if (!strcmp(s, "xor"))  { exp[0]=0; exp[1]=1; exp[2]=1; exp[3]=0; return 0; }
    if (!strcmp(s, "nand")) { exp[0]=1; exp[1]=1; exp[2]=1; exp[3]=0; return 0; }
    return -1;
}

/* Inject input pair (a,b) along an edge, read output from opposite edge */
static void set_input_left(Grid *g, int a, int b) {
    int sz = g->size, half = sz / 2;
    for (int y = 0; y < sz; y++)
        g->cells[y * sz + 0] = (y < half) ? (uint8_t)a : (uint8_t)b;
}

static void set_input_top(Grid *g, int a, int b) {
    int sz = g->size, half = sz / 2;
    for (int x = 0; x < sz; x++)
        g->cells[0 * sz + x] = (x < half) ? (uint8_t)a : (uint8_t)b;
}

static int read_output_right(const Grid *g) {
    int sz = g->size, half = sz / 2;
    int c = 0, total = 0, n = sz * sz;
    for (int y = 0; y < half; y++) c += g->cells[y * sz + (sz - 1)];
    for (int i = 0; i < n; i++) total += g->cells[i];
    int expected_num = total * half;
    int sampled_num  = c * n;
    return (sampled_num > expected_num) ? 1 : 0;
}

static int read_output_bottom(const Grid *g) {
    int sz = g->size, half = sz / 2;
    int c = 0, total = 0, n = sz * sz;
    for (int x = 0; x < half; x++) c += g->cells[(sz - 1) * sz + x];
    for (int i = 0; i < n; i++) total += g->cells[i];
    int expected_num = total * half;
    int sampled_num  = c * n;
    return (sampled_num > expected_num) ? 1 : 0;
}

static double evaluate_mode(System *sys, const int *expected,
                          int eval_steps,
                          void (*set_in)(Grid*,int,int),
                          int (*read_out)(const Grid*),
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
                pipeline_execute(sys->pipeline, sys);
                set_in(sys->grids[0], a, b);
            }
            int o = read_out(sys->grids[0]);
            int idx = (a << 1) | b;
            truth |= (o << idx);
            if (o == expected[idx])
                shaped += expected[idx] ? w_one : w_zero;
        }
    }
    if (out_truth) *out_truth = truth;
    if (truth == 0x0 || truth == 0xF) shaped *= 0.25;
    free(bk);
    return shaped;
}

static double evaluate_individual(System *sys, const int *exp0, const int *exp1,
                                  int eval_steps, int fit_mode,
                                  const uint8_t *bk_cells,
                                  int *truth0, int *truth1) {
    memcpy(sys->grids[0]->cells, bk_cells, sys->grids[0]->size * sys->grids[0]->size);
    double f0 = evaluate_mode(sys, exp0, eval_steps, set_input_left,
                              read_output_right, truth0);
    memcpy(sys->grids[0]->cells, bk_cells, sys->grids[0]->size * sys->grids[0]->size);
    double f1 = evaluate_mode(sys, exp1, eval_steps, set_input_top,
                              read_output_bottom, truth1);
    if (fit_mode == FIT_RAW) {
        int c0 = 0, c1 = 0;
        for (int i = 0; i < 4; i++) {
            c0 += (((*truth0) >> i) & 1) == exp0[i];
            c1 += (((*truth1) >> i) & 1) == exp1[i];
        }
        return ((double)c0 + (double)c1) / 8.0;
    }
    return (f0 + f1) / 2.0;
}

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
        for (int j = 0; j < p->ncells; j++)
            p->pop[i].genomes[j] = genome_random(rng);
}

static void pop_evaluate(Population *p, System *sys,
                         const int *exp0, const int *exp1,
                         int eval_steps, int fit_mode,
                         const uint8_t *bk_cells) {
    for (int i = 0; i < p->pop_size; i++) {
        memcpy(sys->grids[0]->genomes, p->pop[i].genomes,
               p->ncells * sizeof(CellGenome));
        p->pop[i].fitness = evaluate_individual(sys, exp0, exp1, eval_steps,
                                                fit_mode, bk_cells,
                                                &p->pop[i].truth[0],
                                                &p->pop[i].truth[1]);
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

int main(int argc, char **argv) {
    int size = DEF_SIZE, pop = DEF_POP, gens = DEF_GENS;
    int steps = DEF_STEPS, tourn_k = DEF_TOURN_K, elite = DEF_ELITE;
    double mut_rate = DEF_MUT_RATE;
    uint64_t seed = DEF_SEED;
    int exp0[4] = {0,0,0,1}, exp1[4] = {0,1,1,0};
    int mutate_mask = GENOME_MUTATE_RULE | GENOME_MUTATE_WEIGHT;
    int fit_mode = FIT_SHAPED;
    const char *save_genomes = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gate0") && i+1 < argc) {
            if (parse_gate(argv[++i], exp0) < 0) { fprintf(stderr, "bad gate0\n"); return 1; }
        } else if (!strcmp(argv[i], "--gate1") && i+1 < argc) {
            if (parse_gate(argv[++i], exp1) < 0) { fprintf(stderr, "bad gate1\n"); return 1; }
        } else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
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

    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f,
                                           0, -1, 0, 0, 1.0,
                                           GENOME_MUTATE_ALL);

    Population *popu = pop_create(size, pop);
    pop_randomize(popu, rng_u64);

    int ncells = size * size;
    uint8_t *bk_cells = malloc(ncells);
    memcpy(bk_cells, sys.grids[0]->cells, ncells);

    printf("# exp_multimodal_logic_ga: gate0=%s gate1=%s size=%d pop=%d gens=%d steps=%d seed=%llu\n",
           (exp0[3]?"and":""), (exp1[1]?"xor":""), size, pop, gens, steps, (unsigned long long)seed);
    printf("# mode0=left->right  mode1=top->bottom\n");
    printf("gen,best_fitness,avg_fitness,best_truth0,best_truth1\n");

    for (int g = 0; g < gens; g++) {
        pop_evaluate(popu, &sys, exp0, exp1, steps, fit_mode, bk_cells);
        double best_f = 0.0, avg_f = 0.0;
        int best_idx = 0;
        for (int i = 0; i < popu->pop_size; i++) {
            avg_f += popu->pop[i].fitness;
            if (popu->pop[i].fitness > best_f) {
                best_f = popu->pop[i].fitness; best_idx = i;
            }
        }
        avg_f /= popu->pop_size;
        printf("%d,%.4f,%.4f,0x%X,0x%X\n", g, best_f, avg_f,
               popu->pop[best_idx].truth[0], popu->pop[best_idx].truth[1]);
        if (best_f >= 0.9999) break;
        pop_breed(popu, elite, tourn_k, mut_rate, mutate_mask, rng_u64);
    }

    pop_evaluate(popu, &sys, exp0, exp1, steps, fit_mode, bk_cells);
    int best = find_best(popu);
    printf("# Final: truth0=0x%X (fit=%.4f) truth1=0x%X (fit=%.4f)\n",
           popu->pop[best].truth[0], popu->pop[best].fitness,
           popu->pop[best].truth[1], popu->pop[best].fitness);

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
