#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ca_core/engine.h"
#include "ca_core/pipeline.h"
#include "ca_core/local_ga.h"
#include "utils/rng.h"

static int grid_equal(const Grid *a, const Grid *b) {
    if (!a || !b || a->size != b->size) return 0;
    return memcmp(a->cells, b->cells, a->size * a->size) == 0;
}

/* Genomic step with default genomes should run without crash and produce live cells */
static void test_genomic_fallback(void) {
    System sys;
    sys_init(&sys, 1, 16);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    rng_seed(42); sys_randomize(&sys, rng_u64);
    grid_alloc_genomes(sys.grids[0]);
    int n = sys.grids[0]->size * sys.grids[0]->size;
    for (int i = 0; i < n; i++) sys.grids[0]->genomes[i] = genome_pack(0, 15, 0);

    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 10000, -1, 0, 0, 1.0, GENOME_MUTATE_ALL);
    if (!sys.pipeline) { printf("FAIL: preset_genomic NULL\n"); exit(1); }
    for (int t = 0; t < 20; t++) sys_step(&sys);
    int alive = 0;
    for (int i = 0; i < n; i++) if (sys.grids[0]->cells[i]) alive++;
    if (alive == 0) { printf("FAIL: genomic step killed everything\n"); exit(1); }
    printf("PASS: genomic step runs 20 ticks (%d alive cells)\n", alive);
    sys_destroy(&sys);
}

/* Bit-exact: with all-zero genomes (rule 0, weight 0), it should still run.
   weight=0 means coupling fully suppressed; cells decouple. */
static void test_genomic_zero_weight(void) {
    System sys;
    sys_init(&sys, 1, 16);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    rng_seed(42); sys_randomize(&sys, rng_u64);
    grid_alloc_genomes(sys.grids[0]);
    /* All zeros: rule 0, weight 0 */
    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 10000, -1, 0, 0, 1.0, GENOME_MUTATE_ALL);
    for (int t = 0; t < 10; t++) sys_step(&sys);
    printf("PASS: genomic step with weight=0 runs without crash\n");
    sys_destroy(&sys);
}

/* HighLife (rule 1) genomes on edge should change behavior vs all Life */
static void test_genomic_rule_switch(void) {
    System sys;
    sys_init(&sys, 1, 16);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    rng_seed(42); sys_randomize(&sys, rng_u64);
    grid_alloc_genomes(sys.grids[0]);
    /* Set right half of grid to use HighLife (rule 1) */
    int sz = sys.grids[0]->size;
    for (int y = 0; y < sz; y++) {
        for (int x = sz/2; x < sz; x++) {
            sys.grids[0]->genomes[y*sz + x] = genome_pack(1, 0, 0);
        }
    }

    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 10, -1, 0, 0, 1.0, GENOME_MUTATE_ALL);
    if (!sys.pipeline) { printf("FAIL: preset NULL\n"); exit(1); }

    for (int t = 0; t < 30; t++) sys_step(&sys);
    printf("PASS: genomic rule switch runs 30 ticks without crash\n");
    sys_destroy(&sys);
}

/* Genomic preset with fitness phase: fitness should accumulate non-trivially */
static void test_genomic_with_fitness(void) {
    System sys;
    sys_init(&sys, 1, 16);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    rng_seed(42); sys_randomize(&sys, rng_u64);
    grid_alloc_genomes(sys.grids[0]);
    int n = sys.grids[0]->size * sys.grids[0]->size;
    for (int i = 0; i < n; i++) sys.grids[0]->genomes[i] = genome_pack(0, 15, 0);

    /* FITNESS_DENSITY=2, no edge target needed */
    sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 5,
                                           FITNESS_DENSITY, 0, EDGE_RIGHT, 1.0, GENOME_MUTATE_ALL);
    if (!sys.pipeline) { printf("FAIL: fitness preset NULL\n"); exit(1); }
    for (int t = 0; t < 30; t++) sys_step(&sys);

    double fmin = 1e9, fmax = -1e9;
    for (int i = 0; i < n; i++) {
        if (sys.grids[0]->fitness[i] < fmin) fmin = sys.grids[0]->fitness[i];
        if (sys.grids[0]->fitness[i] > fmax) fmax = sys.grids[0]->fitness[i];
    }
    if (fmax <= fmin) { printf("FAIL: fitness uniform after 30 ticks (min=%.4f max=%.4f)\n", fmin, fmax); exit(1); }
    printf("PASS: fitness accumulated with spread (min=%.4f max=%.4f)\n", fmin, fmax);
    sys_destroy(&sys);
}

/* Determinism audit: same seed must produce identical cell and genome state */
static void test_genomic_determinism(void) {
    uint64_t seed = 99;
    uint8_t *cells_a = NULL, *cells_b = NULL;
    CellGenome *genomes_a = NULL, *genomes_b = NULL;
    int n = 0;

    for (int run = 0; run < 2; run++) {
        System sys;
        sys_init(&sys, 1, 16);
        sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
        rng_seed(seed); sys_randomize(&sys, rng_u64);
        grid_alloc_genomes(sys.grids[0]);
        n = sys.grids[0]->size * sys.grids[0]->size;
        for (int i = 0; i < n; i++) sys.grids[0]->genomes[i] = genome_pack(0, 15, 1);
        sys.grids[0]->rng_state = seed ^ 0xA5A5A5A5;
        sys.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 5,
                                               FITNESS_DENSITY, 0, EDGE_RIGHT, 1.0, GENOME_MUTATE_ALL);
        for (int t = 0; t < 50; t++) sys_step(&sys);
        if (run == 0) {
            cells_a = malloc(n); genomes_a = malloc(n * sizeof(CellGenome));
            memcpy(cells_a, sys.grids[0]->cells, n);
            memcpy(genomes_a, sys.grids[0]->genomes, n * sizeof(CellGenome));
        } else {
            cells_b = malloc(n); genomes_b = malloc(n * sizeof(CellGenome));
            memcpy(cells_b, sys.grids[0]->cells, n);
            memcpy(genomes_b, sys.grids[0]->genomes, n * sizeof(CellGenome));
        }
        sys_destroy(&sys);
    }
    if (memcmp(cells_a, cells_b, n) != 0 || memcmp(genomes_a, genomes_b, n * sizeof(CellGenome)) != 0) {
        printf("FAIL: genomic determinism broken (same seed produced different state)\n");
        free(cells_a); free(cells_b); free(genomes_a); free(genomes_b);
        exit(1);
    }
    printf("PASS: genomic pipeline is deterministic across two runs with seed %llu\n", (unsigned long long)seed);
    free(cells_a); free(cells_b); free(genomes_a); free(genomes_b);
}

/* Local GA runs without crash */
static void test_local_ga_runs(void) {
    System sys;
    sys_init(&sys, 1, 8);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    rng_seed(42); sys_randomize(&sys, rng_u64);
    grid_alloc_genomes(sys.grids[0]);
    grid_clear_fitness(sys.grids[0]);

    /* Run a few GA steps */
    for (int i = 0; i < 5; i++) {
        local_ga_step_system_alloc(&sys, 3, GENOME_MUTATE_ALL, rng_u64);
    }
    printf("PASS: local GA runs 5 steps without crash\n");
    sys_destroy(&sys);
}

int main(void) {
    printf("=== Local GA Tests ===\n");
    test_genomic_fallback();
    test_genomic_zero_weight();
    test_genomic_rule_switch();
    test_genomic_with_fitness();
    test_genomic_determinism();
    test_local_ga_runs();
    printf("=== All local GA tests passed ===\n");
    return 0;
}
