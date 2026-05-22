/*
 * exp_te_evolution: Evolve composable rulesets to maximize transfer entropy
 * between two coupled binary CA grids.
 *
 * Concepts from exp_composable_stage0 reused:
 *   - 56-bit composable Ruleset DNA (birth, survive, aniso, card, refr, noise, meta)
 *   - Population GA with tournament selection, crossover, mutation, elitism
 *   - Fitness = transfer entropy across edge-coupled grids + activity penalty
 *
 * Two grids are coupled edge-to-edge via intent buffers (one-tick delay).
 * TE is computed directly from edge-state time-series using te_compute_binary.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ca_core/engine.h"
#include "../ca_core/grid.h"
#include "../ca_core/coupling.h"
#include "../ca_core/intent.h"
#include "../ca_core/genome.h"
#include "../ca_core/composable_ruleset.h"
#include "../metrics/transfer_entropy.h"
#include "../utils/rng.h"

#define DEF_SIZE     32
#define DEF_POP      100
#define DEF_GENS     100
#define DEF_STEPS    50
#define DEF_KTRIALS  4
#define DEF_SEED     42

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

/* Evaluate a ruleset on a 2-grid system with edge coupling.
 * Fitness = mean TE over K random initial-condition pairs. */
static double evaluate_te(const Ruleset *r, System *sys,
                          int eval_steps, uint8_t **refr_bufs,
                          IntentMode mode, float alpha,
                          int te_direction, int K_trials) {
    int sz = sys->grids[0]->size;
    int ncells = sz * sz;
    double total_te = 0.0;

    uint8_t *src_hist = malloc(eval_steps * sizeof(uint8_t));
    uint8_t *dst_hist = malloc(eval_steps * sizeof(uint8_t));
    if (!src_hist || !dst_hist) { free(src_hist); free(dst_hist); return 0.0; }

    for (int trial = 0; trial < K_trials; trial++) {
        /* Independent randomization for each grid */
        for (int g = 0; g < sys->num_grids; g++) {
            Grid *grid = sys->grids[g];
            grid_init_random(grid, rng_u64);
            grid->rng_state = rng_u64();
            if (grid->genomes) {
                for (int i = 0; i < ncells; i++) {
                    uint8_t o = (uint8_t)(rng_u64() & 7);
                    grid->genomes[i] = genome_pack_full(0, 0, 0, o, o, o, o, 0, 0);
                }
            }
            if (refr_bufs[g]) memset(refr_bufs[g], 0, ncells);
        }

        /* Run simulation */
        for (int s = 0; s < eval_steps; s++) {
            /* 1. Local evolution: each grid steps independently */
            for (int g = 0; g < sys->num_grids; g++) {
                ruleset_step_grid(r, sys->grids[g], 0, refr_bufs[g]);
            }

            /* 2. Apply previous tick's captured intents to current cells edges */
            if (sys->intent_buf && sys->intent_buf->count > 0) {
                for (int i = 0; i < sys->intent_buf->count; i++) {
                    EdgeIntent *ei = &sys->intent_buf->items[i];
                    Grid *dst = sys->grids[ei->dst_grid];
                    if (dst && dst->active) intent_apply_to_cells(ei, dst, mode, alpha);
                }
            }

            /* 3. Capture new intents from current cells for next tick */
            intent_buffer_capture_system(sys, sys->intent_buf);

            /* 4. Record edge states for TE analysis */
            int sum0 = 0, sum1 = 0;
            for (int y = 0; y < sz; y++) {
                sum0 += sys->grids[0]->cells[y * sz + (sz - 1)]; /* grid 0 right edge */
                sum1 += sys->grids[1]->cells[y * sz + 0];       /* grid 1 left edge  */
            }
            src_hist[s] = (sum0 * 2 >= sz) ? 1 : 0;
            dst_hist[s] = (sum1 * 2 >= sz) ? 1 : 0;
        }

        /* Compute TE from edge time-series */
        double te = 0.0;
        if (eval_steps >= 3) {
            if (te_direction == 0 || te_direction == 2) {
                /* TE from grid 0 right -> grid 1 left */
                double t = te_compute_binary(dst_hist, src_hist, eval_steps);
                if (t > 0.0) te += t;
            }
            if (te_direction == 1 || te_direction == 2) {
                /* TE from grid 1 left -> grid 0 right */
                double t = te_compute_binary(src_hist, dst_hist, eval_steps);
                if (t > 0.0) te += t;
            }
        }

        /* Activity sanity: penalize dead or fully-saturated grids */
        for (int g = 0; g < sys->num_grids; g++) {
            Grid *grid = sys->grids[g];
            if (!grid) continue;
            int alive = 0;
            for (int i = 0; i < ncells; i++) alive += grid->cells[i];
            double density = (double)alive / ncells;
            if (density < 0.01 || density > 0.99) te -= 0.5;
        }

        total_te += te;
    }

    free(src_hist);
    free(dst_hist);
    return total_te / K_trials;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --size N         grid size (default %d)\n"
        "  --pop N          population (default %d)\n"
        "  --gens N         generations (default %d)\n"
        "  --steps N        eval steps per trial (default %d)\n"
        "  --ktrials N      random IC trials per ruleset (default %d)\n"
        "  --mut R          per-byte mutation rate (default 0.15)\n"
        "  --elite N        elitism (default 2)\n"
        "  --tour K         tournament size (default 3)\n"
        "  --seed N         RNG seed (default %d)\n"
        "  --direction D    0=0->1, 1=1->0, 2=both (default 2)\n"
        "  --mode MODE      intent mode: replace,add,weighted,threshold (default replace)\n"
        "  --alpha F        blending alpha for weighted mode (default 0.5)\n"
        "  --save FILE      save winning ruleset (8 bytes)\n"
        "  --help\n",
        prog, DEF_SIZE, DEF_POP, DEF_GENS, DEF_STEPS, DEF_KTRIALS, DEF_SEED);
}

int main(int argc, char **argv) {
    int size = DEF_SIZE;
    int pop_sz = DEF_POP;
    int gens = DEF_GENS;
    int steps = DEF_STEPS;
    int K_trials = DEF_KTRIALS;
    double mut_rate = 0.15;
    int elite = 2;
    int tour_k = 3;
    uint64_t seed = DEF_SEED;
    int te_direction = 2;
    IntentMode mode = INTENT_MODE_REPLACE;
    float alpha = 0.5f;
    const char *save_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pop") && i + 1 < argc) pop_sz = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i + 1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ktrials") && i + 1 < argc) K_trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mut") && i + 1 < argc) mut_rate = atof(argv[++i]);
        else if (!strcmp(argv[i], "--elite") && i + 1 < argc) elite = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tour") && i + 1 < argc) tour_k = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--direction") && i + 1 < argc) te_direction = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "replace")) mode = INTENT_MODE_REPLACE;
            else if (!strcmp(m, "add")) mode = INTENT_MODE_ADD;
            else if (!strcmp(m, "weighted")) mode = INTENT_MODE_WEIGHTED;
            else if (!strcmp(m, "threshold")) mode = INTENT_MODE_THRESHOLD;
        }
        else if (!strcmp(argv[i], "--alpha") && i + 1 < argc) alpha = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save_path = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    rng_seed(seed);

    /* Setup 2-grid system with right<->left coupling */
    System sys;
    sys_init(&sys, 2, size);
    coupling_connect(&sys.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    if (te_direction == 1 || te_direction == 2)
        coupling_connect(&sys.coupling, 1, EDGE_LEFT, 0, EDGE_RIGHT);

    /* Auto-create intent buffer */
    int max_edge = size;
    sys.intent_buf = intent_buffer_create(2 * 4, max_edge);

    /* Allocate per-cell genomes and refractory buffers */
    int ncells = size * size;
    uint8_t *refr_bufs[2] = { calloc(ncells, 1), calloc(ncells, 1) };
    for (int g = 0; g < 2; g++) {
        grid_alloc_genomes(sys.grids[g]);
    }

    Individual *pop = calloc(pop_sz, sizeof(Individual));
    Individual *next = calloc(pop_sz, sizeof(Individual));

    for (int i = 0; i < pop_sz; i++) pop[i].r = ruleset_random(rng_u64);

    printf("# exp_te_evolution: size=%d pop=%d gens=%d steps=%d ktrials=%d mut=%.3f seed=%llu",
           size, pop_sz, gens, steps, K_trials, mut_rate, (unsigned long long)seed);
    printf(" direction=%d mode=%d alpha=%.2f\n", te_direction, (int)mode, alpha);
    printf("gen,best_fitness,avg_fitness,best_B,best_S,best_aniso,best_card,best_refr,best_noise,best_meta\n");

    for (int g_idx = 0; g_idx < gens; g_idx++) {
        double avg = 0.0, best = -1.0;
        int best_i = 0;
        for (int i = 0; i < pop_sz; i++) {
            pop[i].fitness = evaluate_te(&pop[i].r, &sys, steps, refr_bufs,
                                          mode, alpha, te_direction, K_trials);
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

        qsort(pop, pop_sz, sizeof(Individual), cmp_fitness_desc);
        for (int i = 0; i < elite && i < pop_sz; i++) next[i] = pop[i];

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
            Ruleset child = ruleset_crossover(pop[pa].r, pop[pb].r, rng_u64);
            child = ruleset_mutate(child, mut_rate, rng_u64);
            next[i].r = child;
            next[i].fitness = 0.0;
        }
        Individual *tmp = pop; pop = next; next = tmp;
    }

    int best_i = 0;
    for (int i = 1; i < pop_sz; i++)
        if (pop[i].fitness > pop[best_i].fitness) best_i = i;
    fprintf(stderr, "# Final best fitness (TE): %.4f\n", pop[best_i].fitness);
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

    free(pop); free(next);
    free(refr_bufs[0]); free(refr_bufs[1]);
    sys_destroy(&sys);
    return 0;
}
