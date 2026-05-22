#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "metrics/transfer_entropy.h"
#include "ca_core/intent.h"
#include "ca_core/engine.h"

/* Helper: test that actual is within epsilon of expected */
static void assert_near(double actual, double expected, double epsilon, const char *msg) {
    if (fabs(actual - expected) > epsilon) {
        fprintf(stderr, "FAIL: %s | expected %.4f, got %.4f\n", msg, expected, actual);
        exit(1);
    }
}

/* Test 1: independent binary series should have TE ~= 0 */
static void test_independent(void) {
    int n = 1000;
    uint8_t *x = malloc(n);
    uint8_t *y = malloc(n);
    for (int i = 0; i < n; i++) {
        x[i] = (i % 3 == 0) ? 1 : 0;  /* periodic pattern */
        y[i] = (i % 5 == 0) ? 1 : 0;  /* different periodic pattern */
    }
    double te = te_compute_binary(y, x, n);
    assert_near(te, 0.0, 0.05, "independent series TE");
    free(x); free(y);
    printf("PASS: independent series TE ~= 0\n");
}

/* Test 2: y is a one-tick delayed copy of x -> TE ~= 1 bit */
static void test_delayed_copy(void) {
    int n = 1000;
    uint8_t *x = malloc(n);
    uint8_t *y = malloc(n);
    /* x is pseudo-random-ish binary sequence */
    unsigned int state = 12345;
    x[0] = 0;
    for (int i = 1; i < n; i++) {
        state = state * 1103515245 + 12345;
        x[i] = (state >> 16) & 1;
    }
    /* y is x delayed by 1: y[t] = x[t-1] */
    y[0] = 0;
    for (int i = 1; i < n; i++) y[i] = x[i - 1];

    double te = te_compute_binary(y, x, n);
    /* Perfect delayed copy should give exactly 1 bit of TE */
    assert_near(te, 1.0, 0.05, "delayed copy TE");
    free(x); free(y);
    printf("PASS: delayed copy TE ~= 1 bit\n");
}

/* Test 3: y is an exact copy of x -> TE should be 0 (Y_{t-1} already has the info) */
static void test_exact_copy(void) {
    int n = 1000;
    uint8_t *x = malloc(n);
    uint8_t *y = malloc(n);
    unsigned int state = 54321;
    for (int i = 0; i < n; i++) {
        state = state * 1103515245 + 12345;
        x[i] = (state >> 16) & 1;
        y[i] = x[i];
    }
    double te = te_compute_binary(y, x, n);
    assert_near(te, 0.0, 0.05, "exact copy TE");
    free(x); free(y);
    printf("PASS: exact copy TE ~= 0\n");
}

/* Test 4: constant series -> TE = 0 (no information to transfer) */
static void test_constant(void) {
    int n = 100;
    uint8_t *x = calloc(n, sizeof(uint8_t));
    uint8_t *y = calloc(n, sizeof(uint8_t));
    double te = te_compute_binary(y, x, n);
    assert_near(te, 0.0, 0.01, "constant series TE");
    free(x); free(y);
    printf("PASS: constant series TE = 0\n");
}

/* Test 5: TE from intent ring on a simple coupled system */
static void test_ring_te(void) {
    System sys;
    sys_init(&sys, 2, 16);
    /* Bidirectional coupling so both edges appear in intent ring captures */
    coupling_connect(&sys.coupling, 0, EDGE_RIGHT, 1, EDGE_LEFT);
    coupling_connect(&sys.coupling, 1, EDGE_LEFT, 0, EDGE_RIGHT);
    sys.grids[0]->active = 1;
    sys.grids[1]->active = 1;
    sys.grids[0]->rule_idx = 0;
    sys.grids[1]->rule_idx = 0;

    /* Seed grid 0 right edge with glider-like pattern */
    for (int y = 0; y < 16; y++) {
        sys.grids[0]->cells[y * 16 + 15] = (y % 3 == 0) ? 1 : 0;
    }

    /* Run 20 ticks with delayed intent stepping to populate ring */
    for (int t = 0; t < 20; t++) {
        sys_step_intent_delayed(&sys, INTENT_MODE_REPLACE, 0.0f, 0);
    }

    /* Compute TE from grid 0 right edge to grid 1 left edge */
    double te = te_compute_from_ring(sys.intent_ring, 0, EDGE_RIGHT, 1, EDGE_LEFT, 0, 10);
    if (te < 0) {
        fprintf(stderr, "FAIL: ring TE returned %.2f (not enough data)\n", te);
        exit(1);
    }
    /* With periodic seed and Life rule, the actual value varies;
       just assert it's non-negative. */
    if (te < 0.0) {
        fprintf(stderr, "FAIL: ring TE negative: %.4f\n", te);
        exit(1);
    }
    printf("PASS: ring TE = %.4f bits (non-negative)\n", te);

    sys_destroy(&sys);
}

int main(void) {
    printf("=== Transfer Entropy Tests ===\n");
    test_independent();
    test_delayed_copy();
    test_exact_copy();
    test_constant();
    test_ring_te();
    printf("=== All transfer entropy tests passed ===\n");
    return 0;
}
