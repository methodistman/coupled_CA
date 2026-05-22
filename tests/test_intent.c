#include <stdio.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/intent.h"
#include "../utils/rng.h"

static int failures = 0;

static void assert_eq(int a, int b, const char *msg) {
    if (a != b) {
        fprintf(stderr, "FAIL: %s (expected %d, got %d)\n", msg, b, a);
        failures++;
    }
}

static void assert_buf_nonempty(const IntentBuffer *ib, const char *msg) {
    if (!ib || ib->count == 0) {
        fprintf(stderr, "FAIL: %s (intent buffer empty)\n", msg);
        failures++;
    }
}

int main(void) {
    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, 2, 8);

    /* Set both grids to Life */
    sys.grids[0]->rule_idx = 0;
    sys.grids[1]->rule_idx = 0;

    /* Connect grid 0 right edge -> grid 1 left edge */
    coupling_connect(&sys.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);

    /* Seed and randomize */
    rng_seed(42);
    sys_randomize(&sys, rng_u64);

    /* Run a few steps with normal sys_step */
    for (int i = 0; i < 5; i++) {
        sys_step(&sys);
    }

    /* Now run with sys_step_intent (REPLACE mode) */
    for (int i = 0; i < 5; i++) {
        sys_step_intent(&sys, INTENT_MODE_REPLACE, 0.5f);
    }

    /* Verify intent buffer was created and populated */
    assert_buf_nonempty(sys.intent_buf, "intent buffer after sys_step_intent");

    if (sys.intent_buf) {
        assert_eq(sys.intent_buf->count >= 1, 1, "at least one captured intent");

        /* Verify the captured intent has correct source/dest */
        EdgeIntent *ei = &sys.intent_buf->items[0];
        assert_eq(ei->src_grid, 0, "intent src_grid");
        assert_eq(ei->src_edge, EDGE_RIGHT, "intent src_edge");
        assert_eq(ei->dst_grid, 1, "intent dst_grid");
        assert_eq(ei->dst_edge, EDGE_LEFT, "intent dst_edge");
        assert_eq(ei->n_cells, 8, "intent n_cells matches grid size");
    }

    /* Test ADD mode */
    sys_step_intent(&sys, INTENT_MODE_ADD, 0.5f);
    assert_buf_nonempty(sys.intent_buf, "intent buffer after ADD mode");

    /* Test weighted mode */
    sys_step_intent(&sys, INTENT_MODE_WEIGHTED, 0.5f);
    assert_buf_nonempty(sys.intent_buf, "intent buffer after WEIGHTED mode");

    /* Test delayed coupling (delay=2) */
    sys_randomize(&sys, rng_u64);
    for (int i = 0; i < 5; i++) {
        sys_step_intent_delayed(&sys, INTENT_MODE_REPLACE, 0.5f, 2);
    }
    /* Should not crash and should produce reasonable state */
    assert_eq(sys.grids[0]->active, 1, "grid 0 still active after delayed steps");
    assert_eq(sys.grids[1]->active, 1, "grid 1 still active after delayed steps");

    sys_destroy(&sys);

    if (failures == 0) {
        printf("All intent buffer tests passed.\n");
        return 0;
    } else {
        fprintf(stderr, "%d intent buffer test(s) failed.\n", failures);
        return 1;
    }
}
