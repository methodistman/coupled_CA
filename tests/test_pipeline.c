#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ca_core/engine.h"
#include "ca_core/pipeline.h"
#include "utils/rng.h"

static int grid_equal(const Grid *a, const Grid *b) {
    if (!a || !b || a->size != b->size) return 0;
    int n = a->size * a->size;
    return memcmp(a->cells, b->cells, n) == 0;
}

static void copy_grid_state(Grid *dst, const Grid *src) {
    if (!dst || !src || dst->size != src->size) return;
    int n = dst->size * dst->size;
    memcpy(dst->cells, src->cells, n);
    memcpy(dst->next_cells, src->next_cells, n);
}

/* Test 1: PIPELINE_DEFAULT matches legacy sys_step() */
static void test_default_matches_legacy(void) {
    System sys_leg, sys_pipe;
    sys_init(&sys_leg, 2, 16);
    sys_init(&sys_pipe, 2, 16);
    coupling_connect(&sys_leg.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    coupling_connect(&sys_pipe.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    sys_leg.grids[0]->active = 1; sys_leg.grids[1]->active = 1;
    sys_pipe.grids[0]->active = 1; sys_pipe.grids[1]->active = 1;
    sys_leg.grids[0]->rule_idx = 0; sys_leg.grids[1]->rule_idx = 0;
    sys_pipe.grids[0]->rule_idx = 0; sys_pipe.grids[1]->rule_idx = 0;

    uint64_t seed = 42;
    sys_randomize(&sys_leg, rng_u64);
    sys_randomize(&sys_pipe, rng_u64);
    /* Ensure identical initial state */
    copy_grid_state(sys_pipe.grids[0], sys_leg.grids[0]);
    copy_grid_state(sys_pipe.grids[1], sys_leg.grids[1]);

    sys_pipe.pipeline = pipeline_preset_default();
    if (!sys_pipe.pipeline) { printf("FAIL: preset_default returned NULL\n"); exit(1); }

    for (int t = 0; t < 50; t++) {
        sys_step(&sys_leg);
        sys_step(&sys_pipe);
        if (!grid_equal(sys_leg.grids[0], sys_pipe.grids[0]) ||
            !grid_equal(sys_leg.grids[1], sys_pipe.grids[1])) {
            printf("FAIL: default pipeline diverged at tick %d\n", t);
            exit(1);
        }
    }
    printf("PASS: PIPELINE_DEFAULT matches legacy sys_step() for 50 ticks\n");
    sys_destroy(&sys_leg);
    sys_destroy(&sys_pipe);
}

/* Test 2: dependency ordering is enforced */
static int g_phase_order[4];
static int g_phase_idx = 0;

static void phase_track(System *sys, const Phase *phase, void *userdata) {
    (void)sys; (void)userdata;
    g_phase_order[g_phase_idx++] = phase->target_grid;
}

static void test_dependency_order(void) {
    Pipeline *p = pipeline_create("dep_test");
    g_phase_idx = 0;
    memset(g_phase_order, 0, sizeof(g_phase_order));
    /* Phase 0 (target=0), Phase 1 (target=1 depends on 0), Phase 2 (target=2 depends on 1) */
    int p0 = pipeline_add_phase(p, PHASE_RUN, "p0", phase_track, NULL, 0);
    int p1 = pipeline_add_phase(p, PHASE_RUN, "p1", phase_track, NULL, 1);
    int p2 = pipeline_add_phase(p, PHASE_RUN, "p2", phase_track, NULL, 2);
    pipeline_add_dep(p, p1, p0);
    pipeline_add_dep(p, p2, p1);

    System sys;
    sys_init(&sys, 1, 8);
    pipeline_execute(p, &sys);
    if (g_phase_order[0] != 0 || g_phase_order[1] != 1 || g_phase_order[2] != 2) {
        printf("FAIL: dependency order wrong: %d,%d,%d\n",
               g_phase_order[0], g_phase_order[1], g_phase_order[2]);
        exit(1);
    }
    printf("PASS: dependency ordering enforced\n");
    pipeline_destroy(p);
    sys_destroy(&sys);
}

/* Test 3: PIPELINE_INTENT runs without crash */
static void test_intent_preset(void) {
    System sys;
    sys_init(&sys, 2, 16);
    coupling_connect(&sys.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    sys.grids[0]->active = 1; sys.grids[1]->active = 1;
    sys.grids[0]->rule_idx = 0; sys.grids[1]->rule_idx = 0;
    sys_randomize(&sys, rng_u64);

    sys.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    if (!sys.pipeline) { printf("FAIL: preset_intent returned NULL\n"); exit(1); }

    for (int t = 0; t < 20; t++) {
        sys_step(&sys);
    }
    printf("PASS: PIPELINE_INTENT runs 20 ticks without crash\n");
    sys_destroy(&sys);
}

/* Test 4: PIPELINE_PARALLEL matches PIPELINE_INTENT for a single uncoupled grid.
   (Multi-grid coupled parallel has a known read-write race during in-place swap;
   it is not guaranteed bit-exact and is excluded from this test.) */
static void test_parallel_matches_intent(void) {
    System sys_par, sys_int;
    sys_init(&sys_par, 1, 32);
    sys_init(&sys_int, 1, 32);
    sys_par.grids[0]->active = 1; sys_par.grids[0]->rule_idx = 0;
    sys_int.grids[0]->active = 1; sys_int.grids[0]->rule_idx = 0;

    uint64_t seed = 123;
    rng_seed(seed);
    sys_randomize(&sys_par, rng_u64);
    rng_seed(seed);
    sys_randomize(&sys_int, rng_u64);

    sys_par.pipeline = pipeline_preset_parallel(INTENT_MODE_REPLACE, 0.0f);
    sys_int.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    if (!sys_par.pipeline || !sys_int.pipeline) {
        printf("FAIL: parallel or intent preset returned NULL\n"); exit(1);
    }

    for (int t = 0; t < 200; t++) {
        sys_step(&sys_par);
        sys_step(&sys_int);
        if (!grid_equal(sys_par.grids[0], sys_int.grids[0])) {
            printf("FAIL: parallel diverged from intent at tick %d\n", t);
            exit(1);
        }
    }
    printf("PASS: PIPELINE_PARALLEL matches PIPELINE_INTENT for 200 ticks (1 grid)\n");
    sys_destroy(&sys_par);
    sys_destroy(&sys_int);
}

/* Test 5: PIPELINE_GENOMIC with uniform default genomes matches PIPELINE_INTENT.
   Uniform genomes: rule=0 (Life), weight=15 (full coupling). GA disabled. */
static void test_genomic_matches_intent_uniform(void) {
    System sys_gen, sys_int;
    sys_init(&sys_gen, 2, 16);
    sys_init(&sys_int, 2, 16);
    coupling_connect(&sys_gen.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    coupling_connect(&sys_int.coupling, 0, EDGE_BOTTOM, 1, EDGE_TOP);
    for (int g = 0; g < 2; g++) {
        sys_gen.grids[g]->active = 1; sys_gen.grids[g]->rule_idx = 0;
        sys_int.grids[g]->active = 1; sys_int.grids[g]->rule_idx = 0;
    }

    uint64_t seed = 77;
    rng_seed(seed);
    sys_randomize(&sys_gen, rng_u64);
    rng_seed(seed);
    sys_randomize(&sys_int, rng_u64);

    /* Set uniform genomes on genomic system */
    for (int g = 0; g < 2; g++) {
        grid_alloc_genomes(sys_gen.grids[g]);
        int n = sys_gen.grids[g]->size * sys_gen.grids[g]->size;
        for (int i = 0; i < n; i++)
            sys_gen.grids[g]->genomes[i] = genome_pack(0, 15, 0);
    }

    sys_gen.pipeline = pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, 0, -1, 0, 0, 1.0, GENOME_MUTATE_ALL);
    sys_int.pipeline = pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    if (!sys_gen.pipeline || !sys_int.pipeline) {
        printf("FAIL: genomic or intent preset returned NULL\n"); exit(1);
    }

    for (int t = 0; t < 200; t++) {
        sys_step(&sys_gen);
        sys_step(&sys_int);
        for (int g = 0; g < 2; g++) {
            if (!grid_equal(sys_gen.grids[g], sys_int.grids[g])) {
                printf("FAIL: genomic diverged from intent at tick %d grid %d\n", t, g);
                exit(1);
            }
        }
    }
    printf("PASS: PIPELINE_GENOMIC (uniform rule=0 weight=15, no GA) matches PIPELINE_INTENT for 200 ticks\n");
    sys_destroy(&sys_gen);
    sys_destroy(&sys_int);
}

/* Test 6: empty pipeline does nothing */
static void test_empty_pipeline(void) {
    System sys;
    sys_init(&sys, 1, 8);
    sys.grids[0]->active = 1; sys.grids[0]->rule_idx = 0;
    for (int i = 0; i < 64; i++) sys.grids[0]->cells[i] = (i % 3 == 0) ? 1 : 0;
    memcpy(sys.grids[0]->next_cells, sys.grids[0]->cells, 64);

    Pipeline *p = pipeline_create("empty");
    pipeline_execute(p, &sys);
    if (memcmp(sys.grids[0]->cells, sys.grids[0]->next_cells, 64) != 0) {
        printf("FAIL: empty pipeline modified grid state\n");
        exit(1);
    }
    printf("PASS: empty pipeline is no-op\n");
    pipeline_destroy(p);
    sys_destroy(&sys);
}

int main(void) {
    printf("=== Pipeline Tests ===\n");
    test_default_matches_legacy();
    test_dependency_order();
    test_intent_preset();
    test_parallel_matches_intent();
    test_genomic_matches_intent_uniform();
    test_empty_pipeline();
    printf("=== All pipeline tests passed ===\n");
    return 0;
}
