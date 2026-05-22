#include <stdio.h>
#include <string.h>
#include "../ca_core/payload_engine.h"
#include "../ca_core/payload_pipeline.h"
#include "../utils/rng.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); failures++; } \
    else fprintf(stderr, "  PASS: %s\n", msg); \
} while (0)

int main(void) {
    rng_seed(42);

    /* Test 1: default preset matches legacy payload_sys_step */
    PayloadSystem a, b;
    payload_sys_init(&a, 2, 16);
    payload_sys_init(&b, 2, 16);
    a.grids[0]->rule_idx = 0;
    a.grids[1]->rule_idx = 0;
    b.grids[0]->rule_idx = 0;
    b.grids[1]->rule_idx = 0;
    rng_seed(7);
    payload_sys_randomize(&a, rng_u64);
    rng_seed(7);
    payload_sys_randomize(&b, rng_u64);

    PayloadPipeline *p = payload_pipeline_preset_default();
    CHECK(p != NULL, "preset_default created");
    a.pipeline = p;
    for (int t = 0; t < 5; t++) {
        payload_sys_step(&a);
        payload_sys_step(&b);
    }
    int match = memcmp(a.grids[0]->cells, b.grids[0]->cells, 16*16*sizeof(Cell)) == 0
             && memcmp(a.grids[1]->cells, b.grids[1]->cells, 16*16*sizeof(Cell)) == 0;
    CHECK(match, "default preset matches legacy step bit-for-bit");
    a.pipeline = NULL;
    payload_pipeline_destroy(p);
    payload_sys_destroy(&a);
    payload_sys_destroy(&b);

    /* Test 2: intent preset runs without crash */
    PayloadSystem s;
    payload_sys_init(&s, 2, 16);
    s.grids[0]->rule_idx = 0;
    s.grids[1]->rule_idx = 0;
    payload_sys_randomize(&s, rng_u64);
    p = payload_pipeline_preset_intent(INTENT_MODE_REPLACE, 0.0f);
    CHECK(p != NULL, "preset_intent created");
    s.pipeline = p;
    for (int t = 0; t < 5; t++) payload_sys_step(&s);
    CHECK(p->tick_count == 5, "intent pipeline tick_count == 5");
    s.pipeline = NULL;
    payload_pipeline_destroy(p);
    payload_sys_destroy(&s);

    /* Test 3: genomic preset allocates genomes and runs */
    payload_sys_init(&s, 1, 16);
    s.grids[0]->rule_idx = 0;
    payload_sys_randomize(&s, rng_u64);
    p = payload_pipeline_preset_genomic(INTENT_MODE_REPLACE, 0.0f, /*ga_interval*/2,
                                          /*fitness_mode*/PAYLOAD_FITNESS_DENSITY,
                                          /*target_grid*/0, /*target_edge*/EDGE_TOP,
                                          /*discount*/1.0, GENOME_MUTATE_ALL);
    CHECK(p != NULL, "preset_genomic created");
    s.pipeline = p;
    for (int t = 0; t < 6; t++) payload_sys_step(&s);
    CHECK(s.grids[0]->genomes != NULL, "genomic preset allocates genomes");
    CHECK(s.grids[0]->fitness != NULL, "genomic preset allocates fitness");
    s.pipeline = NULL;
    payload_pipeline_destroy(p);
    payload_sys_destroy(&s);

    fprintf(stderr, "%d errors\n", failures);
    return failures ? 1 : 0;
}
