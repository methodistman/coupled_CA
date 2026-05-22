/*
 * exp_hierarchical: Two-stage hierarchical GA for multimodal logic.
 *
 * Stage 1: Evolve per-cell orientations to create 4 signal highways.
 *   Fitness = min(reachability across 4 modes).
 *   Genome: only orientations mutate; rule/weight/alt_rule/dir_mask fixed.
 *
 * Stage 2: Freeze winning orientations. Evolve rules/weights/alt_rules/dir_masks.
 *   Fitness = shaped logic accuracy across 4 modes.
 *   Genome: orientations frozen from Stage 1 winner.
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

#define DEF_SIZE      32
#define DEF_S1_GENS   200
#define DEF_S2_GENS   500
#define DEF_S1_POP    200
#define DEF_S2_POP    200
#define DEF_STEPS     50
#define DEF_SEED      42
#define DEF_TOURN_K   3
#define DEF_MUT_RATE  0.05
#define DEF_ELITE     2
#define NMODES        4

#define FIT_RAW       0
#define FIT_SHAPED    1

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
    fprintf(stderr, "  --size N          Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --s1-gens N       Stage 1 generations (default %d)\n", DEF_S1_GENS);
    fprintf(stderr, "  --s2-gens N       Stage 2 generations (default %d)\n", DEF_S2_GENS);
    fprintf(stderr, "  --s1-pop N        Stage 1 population (default %d)\n", DEF_S1_POP);
    fprintf(stderr, "  --s2-pop N        Stage 2 population (default %d)\n", DEF_S2_POP);
    fprintf(stderr, "  --steps N         Eval steps per test (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N          RNG seed (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --mut-rate R      (default %.2f)\n", DEF_MUT_RATE);
    fprintf(stderr, "  --tour K          Tournament size (default %d)\n", DEF_TOURN_K);
    fprintf(stderr, "  --elite N         Elitism (default %d)\n", DEF_ELITE);
    fprintf(stderr, "  --fitness raw|shaped (default shaped, stage2 only)\n");
    fprintf(stderr, "  --stage1-only     Run only stage 1, save orientations\n");
    fprintf(stderr, "  --stage2-only     Run only stage 2 (requires --load-orientations)\n");
    fprintf(stderr, "  --load-orientations FILE\n");
    fprintf(stderr, "  --save-orientations FILE\n");
    fprintf(stderr, "  --save-genomes FILE\n");
    fprintf(stderr, "  --help\n");
}

static int gate_expected[4] = {0, 1, 1, 0}; /* XOR for all modes */

/* ===== Trace types ===== */
typedef enum {
    TRACE_HORIZ,
    TRACE_VERT,
    TRACE_ADIAG,
    TRACE_DIAG
} TraceType;

/* Inject binary inputs A,B along two halves of the source edge/corner */
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
        case TRACE_ADIAG: {
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
        case TRACE_DIAG: {
            int n = 0;
            for (int i = 0; i < sz && n < half; i++) {
                g->cells[i * sz + i] = (n < half/2) ? (uint8_t)a : (uint8_t)b;
                n++;
            }
            break;
        }
    }
}

/* Read output from opposite edge of trace */
static int read_output_trace(const Grid *g, TraceType t) {
    int sz = g->size, half = sz / 2;
    int c = 0, total = 0, ncells = sz * sz;
    switch (t) {
        case TRACE_HORIZ:
            for (int y = 0; y < half; y++) c += g->cells[y * sz + (sz - 1)];
            break;
        case TRACE_VERT:
            for (int x = 0; x < half; x++) c += g->cells[(sz - 1) * sz + x];
            break;
        case TRACE_ADIAG: {
            int k = sz - 1, nread = 0;
            for (int x = 0; x < sz && nread < half; x++) {
                int y = k - x;
                if (y >= 0 && y < sz) { c += g->cells[y * sz + x]; nread++; }
            }
            break;
        }
        case TRACE_DIAG: {
            int nread = 0;
            for (int i = sz - 1; i >= 0 && nread < half; i--) {
                c += g->cells[i * sz + i]; nread++;
            }
            break;
        }
    }
    for (int i = 0; i < ncells; i++) total += g->cells[i];
    int expected_num = total * half;
    int sampled_num  = c * ncells;
    return (sampled_num > expected_num) ? 1 : 0;
}

/* ===== Signal reachability (Stage 1) ===== */

/* Inject uniform signal (all 1s) along full source trace */
static void inject_signal(Grid *g, TraceType t) {
    int sz = g->size;
    switch (t) {
        case TRACE_HORIZ:
            for (int y = 0; y < sz; y++) g->cells[y * sz + 0] = 1;
            break;
        case TRACE_VERT:
            for (int x = 0; x < sz; x++) g->cells[0 * sz + x] = 1;
            break;
        case TRACE_ADIAG: {
            int k = sz - 1;
            for (int x = 0; x < sz; x++) {
                int y = k - x;
                if (y >= 0 && y < sz) g->cells[y * sz + x] = 1;
            }
            break;
        }
        case TRACE_DIAG:
            for (int i = 0; i < sz; i++) g->cells[i * sz + i] = 1;
            break;
    }
}

/* Measure fraction of sink trace cells that are active */
static double measure_reachability(const Grid *g, TraceType t) {
    int sz = g->size, active = 0, total = 0;
    switch (t) {
        case TRACE_HORIZ:
            for (int y = 0; y < sz; y++) { total++; active += g->cells[y * sz + (sz - 1)]; }
            break;
        case TRACE_VERT:
            for (int x = 0; x < sz; x++) { total++; active += g->cells[(sz - 1) * sz + x]; }
            break;
        case TRACE_ADIAG: {
            int k = sz - 1;
            for (int x = 0; x < sz; x++) {
                int y = k - x;
                if (y >= 0 && y < sz) { total++; active += g->cells[y * sz + x]; }
            }
            break;
        }
        case TRACE_DIAG:
            for (int i = 0; i < sz; i++) { total++; active += g->cells[i * sz + i]; }
            break;
    }
    return total > 0 ? (double)active / total : 0.0;
}

static double evaluate_reachability_mode(System *sys, int eval_steps, TraceType t) {
    int sz = sys->grids[0]->size;
    int ncells = sz * sz;
    /* Start from zero, inject source pulse once, then step and measure
       how far the pulse propagates under the anisotropic kernel. */
    memset(sys->grids[0]->cells, 0, ncells);
    inject_signal(sys->grids[0], t);
    for (int st = 0; st < eval_steps; st++) {
        sys_step_genomic_mode(sys, (int)t, 0); /* orientation only, no dir rules */
    }
    return measure_reachability(sys->grids[0], t);
}

static double evaluate_reachability(System *sys, int eval_steps) {
    double sum = 0.0;
    for (int m = 0; m < NMODES; m++) {
        sum += evaluate_reachability_mode(sys, eval_steps, (TraceType)m);
    }
    return sum / NMODES;
}

static void evaluate_reachability_verbose(System *sys, int eval_steps, double out[NMODES]) {
    for (int m = 0; m < NMODES; m++) {
        out[m] = evaluate_reachability_mode(sys, eval_steps, (TraceType)m);
    }
}

/* ===== Logic accuracy (Stage 2) ===== */
static double evaluate_logic_mode(System *sys, const int *expected,
                                  int eval_steps, TraceType t,
                                  const uint8_t *bk_cells, int *out_truth) {
    int sz = sys->grids[0]->size;
    int ncells = sz * sz;
    int truth = 0;
    double shaped = 0.0;
    int n_ones = 0, n_zeros = 0;
    for (int i = 0; i < 4; i++) { if (expected[i]) n_ones++; else n_zeros++; }
    double w_one  = n_ones  > 0 ? 0.5 / n_ones  : 0.0;
    double w_zero = n_zeros > 0 ? 0.5 / n_zeros : 0.0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            memcpy(sys->grids[0]->cells, bk_cells, ncells);
            for (int st = 0; st < eval_steps; st++) {
                sys_step_genomic_mode(sys, (int)t, 1); /* orientation + dir rules */
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
    return shaped;
}

static double evaluate_logic(System *sys, const int *expected,
                             int eval_steps, int fit_mode,
                             const uint8_t *bk_cells, int *truths) {
    int ncells = sys->grids[0]->size * sys->grids[0]->size;
    double total = 0.0;
    for (int m = 0; m < NMODES; m++) {
        memcpy(sys->grids[0]->cells, bk_cells, ncells);
        total += evaluate_logic_mode(sys, expected, eval_steps, (TraceType)m, bk_cells, &truths[m]);
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

/* ===== Genome field fixers ===== */
static CellGenome fix_non_orientation(CellGenome g, int rule, int weight, int alt_rule, int dir_mask) {
    int o0 = (int)GENOME_ORIENT_MODE0(g);
    int o1 = (int)GENOME_ORIENT_MODE1(g);
    int o2 = (int)GENOME_ORIENT_MODE2(g);
    int o3 = (int)GENOME_ORIENT_MODE3(g);
    return genome_pack_full(rule, weight, 0, o0, o1, o2, o3, alt_rule, dir_mask);
}

static CellGenome fix_orientation(CellGenome g, int o0, int o1, int o2, int o3) {
    int rule = (int)GENOME_RULE_SELECT(g);
    int weight = (int)GENOME_COUPLING_WEIGHT(g);
    int alt = (int)GENOME_ALT_RULE_SELECT(g);
    int dmask = (int)GENOME_DIR_MASK(g);
    return genome_pack_full(rule, weight, 0, o0, o1, o2, o3, alt, dmask);
}

/* ===== Population ===== */
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

static int find_best(const Population *p) {
    int best = 0;
    for (int i = 1; i < p->pop_size; i++)
        if (p->pop[i].fitness > p->pop[best].fitness) best = i;
    return best;
}

static void pop_breed(Population *p, int elite, int tourn_k,
                      double mut_rate, int mutate_mask,
                      uint64_t (*rng)(void),
                      CellGenome (*fixer)(CellGenome)) {
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
            if (fixer) child = fixer(child);
            p->next[i].genomes[j] = child;
        }
    }
    free(fit);
    Individual *tmp = p->pop; p->pop = p->next; p->next = tmp;
}

/* ===== Stage 1: Orientation highway evolution ===== */
static void stage1_run(Population *p, System *sys, int gens, int eval_steps,
                       int elite, int tourn_k, double mut_rate,
                       uint64_t (*rng)(void),
                       int fixed_rule, int fixed_weight,
                       int fixed_alt_rule, int fixed_dir_mask) {
    int mutate_mask = GENOME_MUTATE_ORIENT_MODE0 | GENOME_MUTATE_ORIENT_MODE1
                    | GENOME_MUTATE_ORIENT_MODE2 | GENOME_MUTATE_ORIENT_MODE3;
    CellGenome (*fixer)(CellGenome) = NULL;
    /* Use a local closure-like approach: we can't easily pass extra args to fixer.
       Instead, we inline the breed+fix loop for stage 1. */
    for (int g = 0; g < gens; g++) {
        for (int i = 0; i < p->pop_size; i++) {
            memcpy(sys->grids[0]->genomes, p->pop[i].genomes,
                   p->ncells * sizeof(CellGenome));
            p->pop[i].fitness = evaluate_reachability(sys, eval_steps);
        }
        double best_f = 0.0, avg_f = 0.0;
        int best_idx = 0;
        for (int i = 0; i < p->pop_size; i++) {
            avg_f += p->pop[i].fitness;
            if (p->pop[i].fitness > best_f) {
                best_f = p->pop[i].fitness; best_idx = i;
            }
        }
        avg_f /= p->pop_size;
        double per_mode[NMODES];
        memcpy(sys->grids[0]->genomes, p->pop[best_idx].genomes,
               p->ncells * sizeof(CellGenome));
        evaluate_reachability_verbose(sys, eval_steps, per_mode);
        printf("S1,%d,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f\n", g, best_f, avg_f,
               per_mode[0], per_mode[1], per_mode[2], per_mode[3]);
        if (best_f >= 0.9999) break;

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
                child = fix_non_orientation(child, fixed_rule, fixed_weight,
                                            fixed_alt_rule, fixed_dir_mask);
                p->next[i].genomes[j] = child;
            }
        }
        free(fit);
        Individual *tmp = p->pop; p->pop = p->next; p->next = tmp;
    }
}

/* ===== Stage 2: Logic evolution on frozen orientations ===== */
static void stage2_run(Population *p, System *sys, int gens, int eval_steps,
                       int fit_mode, const uint8_t *bk_cells,
                       int elite, int tourn_k, double mut_rate,
                       uint64_t (*rng)(void),
                       CellGenome *winner_orientations) {
    int mutate_mask = GENOME_MUTATE_RULE | GENOME_MUTATE_WEIGHT
                    | GENOME_MUTATE_ALT_RULE | GENOME_MUTATE_DIR_MASK;
    for (int g = 0; g < gens; g++) {
        for (int i = 0; i < p->pop_size; i++) {
            memcpy(sys->grids[0]->genomes, p->pop[i].genomes,
                   p->ncells * sizeof(CellGenome));
            p->pop[i].fitness = evaluate_logic(sys, gate_expected, eval_steps,
                                                 fit_mode, bk_cells, p->pop[i].truth);
        }
        double best_f = 0.0, avg_f = 0.0;
        int best_idx = 0;
        for (int i = 0; i < p->pop_size; i++) {
            avg_f += p->pop[i].fitness;
            if (p->pop[i].fitness > best_f) {
                best_f = p->pop[i].fitness; best_idx = i;
            }
        }
        avg_f /= p->pop_size;
        printf("S2,%d,%.4f,%.4f", g, best_f, avg_f);
        for (int m = 0; m < NMODES; m++) printf(",0x%X", p->pop[best_idx].truth[m]);
        printf("\n");
        if (best_f >= 0.9999) break;

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
                int o0 = (int)GENOME_ORIENT_MODE0(winner_orientations[j]);
                int o1 = (int)GENOME_ORIENT_MODE1(winner_orientations[j]);
                int o2 = (int)GENOME_ORIENT_MODE2(winner_orientations[j]);
                int o3 = (int)GENOME_ORIENT_MODE3(winner_orientations[j]);
                child = fix_orientation(child, o0, o1, o2, o3);
                p->next[i].genomes[j] = child;
            }
        }
        free(fit);
        Individual *tmp = p->pop; p->pop = p->next; p->next = tmp;
    }
}

/* ===== main ===== */
int main(int argc, char **argv) {
    int size = DEF_SIZE;
    int s1_gens = DEF_S1_GENS, s2_gens = DEF_S2_GENS;
    int s1_pop = DEF_S1_POP, s2_pop = DEF_S2_POP;
    int steps = DEF_STEPS, tourn_k = DEF_TOURN_K, elite = DEF_ELITE;
    double mut_rate = DEF_MUT_RATE;
    uint64_t seed = DEF_SEED;
    int fit_mode = FIT_SHAPED;
    int stage1_only = 0, stage2_only = 0;
    const char *load_orientations = NULL;
    const char *save_orientations = NULL;
    const char *save_genomes = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--s1-gens") && i+1 < argc) s1_gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--s2-gens") && i+1 < argc) s2_gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--s1-pop") && i+1 < argc) s1_pop = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--s2-pop") && i+1 < argc) s2_pop = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--mut-rate") && i+1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i+1 < argc) tourn_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i+1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fitness") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "raw")) fit_mode = FIT_RAW;
            else if (!strcmp(m, "shaped")) fit_mode = FIT_SHAPED;
        } else if (!strcmp(argv[i], "--stage1-only")) stage1_only = 1;
        else if (!strcmp(argv[i], "--stage2-only")) stage2_only = 1;
        else if (!strcmp(argv[i], "--load-orientations") && i+1 < argc) load_orientations = argv[++i];
        else if (!strcmp(argv[i], "--save-orientations") && i+1 < argc) save_orientations = argv[++i];
        else if (!strcmp(argv[i], "--save-genomes") && i+1 < argc) save_genomes = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (stage2_only && !load_orientations) {
        fprintf(stderr, "Error: --stage2-only requires --load-orientations\n");
        return 1;
    }

    rng_seed(seed);
    System sys;
    sys_init(&sys, 1, size);
    sys.grids[0]->active = 1;
    sys.grids[0]->rule_idx = 0;
    grid_alloc_genomes(sys.grids[0]);
    sys_randomize(&sys, rng_u64);
    sys.pipeline = NULL;

    int ncells = size * size;
    uint8_t *bk_cells = malloc(ncells);
    memcpy(bk_cells, sys.grids[0]->cells, ncells);

    printf("# exp_hierarchical: size=%d s1_gens=%d s2_gens=%d s1_pop=%d s2_pop=%d steps=%d seed=%llu\n",
           size, s1_gens, s2_gens, s1_pop, s2_pop, steps, (unsigned long long)seed);
    printf("# stage1_only=%d stage2_only=%d\n", stage1_only, stage2_only);

    /* ---- Stage 1: Evolve orientations ---- */
    CellGenome *winner_orientations = NULL;

    if (!stage2_only) {
        printf("# === Stage 1: Orientation Highway Evolution ===\n");
        printf("stage,gen,best_fitness,avg_fitness,reach_h,reach_v,reach_a,reach_d\n");
        Population *p1 = pop_create(size, s1_pop);
        /* Initialize with random orientations, fixed rule/weight/alt/dir.
           Use rule index 6 = "Life Without Death" (B3/S012345678): alive cells
           persist, which makes signal reachability a clean function of how
           orientations route the source pulse under the anisotropic kernel. */
        int fixed_rule = 6;
        /* weight must be < 15 to engage anisotropic orientation modulation in the kernel.
           Use 14: ~95% baseline pass, modulated by ALIGNMENT_SCALE for orientation. */
        int fixed_weight = 14;
        int fixed_alt = 6;
        int fixed_dir = 0;
        for (int i = 0; i < s1_pop; i++)
            for (int j = 0; j < ncells; j++) {
                int o0 = (int)(rng_u64() % 8);
                int o1 = (int)(rng_u64() % 8);
                int o2 = (int)(rng_u64() % 8);
                int o3 = (int)(rng_u64() % 8);
                p1->pop[i].genomes[j] = genome_pack_full(fixed_rule, fixed_weight, 0,
                                                           o0, o1, o2, o3,
                                                           fixed_alt, fixed_dir);
            }
        stage1_run(p1, &sys, s1_gens, steps, elite, tourn_k, mut_rate, rng_u64,
                   fixed_rule, fixed_weight, fixed_alt, fixed_dir);

        int best1 = find_best(p1);
        printf("# Stage 1 best fitness: %.4f\n", p1->pop[best1].fitness);

        /* Extract winning orientations */
        winner_orientations = malloc(ncells * sizeof(CellGenome));
        memcpy(winner_orientations, p1->pop[best1].genomes, ncells * sizeof(CellGenome));

        /* Compute per-cell consensus orientation for Stage 2 fixing */
        /* For now, use the winner directly (each cell keeps its own orientations) */

        if (save_orientations) {
            FILE *fp = fopen(save_orientations, "wb");
            if (fp) {
                fwrite(&size, sizeof(int), 1, fp);
                fwrite(winner_orientations, sizeof(CellGenome), ncells, fp);
                fclose(fp);
                printf("# Saved orientations to %s\n", save_orientations);
            }
        }

        if (stage1_only) {
            pop_destroy(p1);
            free(winner_orientations);
            free(bk_cells);
            sys_destroy(&sys);
            return 0;
        }
        pop_destroy(p1);
    } else {
        /* Load orientations from file */
        FILE *fp = fopen(load_orientations, "rb");
        if (!fp) { perror(load_orientations); return 1; }
        int fsize;
        fread(&fsize, sizeof(int), 1, fp);
        if (fsize != size) {
            fprintf(stderr, "Error: orientation file size %d != grid size %d\n", fsize, size);
            return 1;
        }
        winner_orientations = malloc(ncells * sizeof(CellGenome));
        fread(winner_orientations, sizeof(CellGenome), ncells, fp);
        fclose(fp);
        printf("# Loaded orientations from %s\n", load_orientations);
    }

    /* ---- Stage 2: Evolve rules on fixed orientations ---- */
    printf("# === Stage 2: Logic Evolution ===\n");
    printf("stage,gen,best_fitness,avg_fitness,best_t0,best_t1,best_t2,best_t3\n");
    Population *p2 = pop_create(size, s2_pop);
    /* Initialize with winner orientations, random rules/weights/alt/dir */
    for (int i = 0; i < s2_pop; i++)
        for (int j = 0; j < ncells; j++) {
            int rule = (int)(rng_u64() % NUM_RULES);
            int weight = (int)(rng_u64() % 256);
            int o0 = (int)GENOME_ORIENT_MODE0(winner_orientations[j]);
            int o1 = (int)GENOME_ORIENT_MODE1(winner_orientations[j]);
            int o2 = (int)GENOME_ORIENT_MODE2(winner_orientations[j]);
            int o3 = (int)GENOME_ORIENT_MODE3(winner_orientations[j]);
            int alt_rule = (int)(rng_u64() % NUM_RULES);
            int dir_mask = (int)(rng_u64() % 16);
            p2->pop[i].genomes[j] = genome_pack_full(rule, weight, 0,
                                                       o0, o1, o2, o3,
                                                       alt_rule, dir_mask);
        }

    stage2_run(p2, &sys, s2_gens, steps, fit_mode, bk_cells,
               elite, tourn_k, mut_rate, rng_u64, winner_orientations);

    /* Re-evaluate best for final truths */
    for (int i = 0; i < p2->pop_size; i++) {
        memcpy(sys.grids[0]->genomes, p2->pop[i].genomes, ncells * sizeof(CellGenome));
        p2->pop[i].fitness = evaluate_logic(&sys, gate_expected, steps,
                                            fit_mode, bk_cells, p2->pop[i].truth);
    }
    int best2 = find_best(p2);
    printf("# Final truths: ");
    for (int m = 0; m < NMODES; m++) printf("0x%X ", p2->pop[best2].truth[m]);
    printf("(fit=%.4f)\n", p2->pop[best2].fitness);

    if (save_genomes) {
        FILE *fp = fopen(save_genomes, "wb");
        if (fp) {
            fwrite(&size, sizeof(int), 1, fp);
            fwrite(p2->pop[best2].genomes, sizeof(CellGenome), ncells, fp);
            fclose(fp);
            printf("# Saved genome to %s\n", save_genomes);
        }
    }

    free(winner_orientations);
    free(bk_cells);
    pop_destroy(p2);
    sys_destroy(&sys);
    return 0;
}
