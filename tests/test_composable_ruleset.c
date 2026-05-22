/* test_composable_ruleset.c — sanity tests for the 6-layer composable CA. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_core/composable_ruleset.h"
#include "../ca_core/grid.h"
#include "../ca_core/genome.h"
#include "../utils/rng.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { fprintf(stderr, "ok:   %s\n", msg); } \
} while (0)

/* Build a small grid with optional center-region seed. */
static Grid *make_grid(int sz) {
    Grid *g = grid_create(sz);
    g->active = 1;
    g->rng_state = 0xDEADBEEFCAFEBABEULL;
    grid_alloc_genomes(g);
    /* All cells get orientation 0 (East) for all modes, no logic params. */
    for (int i = 0; i < sz * sz; i++) {
        g->genomes[i] = genome_pack_full(0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    return g;
}

static int count_alive(Grid *g) {
    int n = 0;
    for (int i = 0; i < g->size * g->size; i++) n += g->cells[i] ? 1 : 0;
    return n;
}

/* Test 1: a Conway ruleset on a single 3-cell vertical bar at center
 * should oscillate to a horizontal bar (the "blinker") after 1 step. */
static void test_conway_blinker(void) {
    Grid *g = make_grid(8);
    grid_clear(g);
    /* Vertical bar at (4, 3), (4, 4), (4, 5). */
    g->cells[3 * 8 + 4] = 1;
    g->cells[4 * 8 + 4] = 1;
    g->cells[5 * 8 + 4] = 1;

    Ruleset r = ruleset_conway();
    /* For Conway we want isotropic count: disable aniso layer via meta bit 4. */
    r.p[6] &= (uint8_t)~0x10;
    /* Also disable cardinal/diag bias to compare exactly to plain Conway. */
    r.p[6] &= (uint8_t)~0x20;

    ruleset_step_grid(&r, g, /*mode=*/0, /*refr=*/NULL);

    /* After 1 step: horizontal bar at (3,4),(4,4),(5,4). */
    int ok = g->cells[4 * 8 + 3] == 1
          && g->cells[4 * 8 + 4] == 1
          && g->cells[4 * 8 + 5] == 1
          && count_alive(g) == 3;
    CHECK(ok, "Conway blinker oscillates vertical->horizontal");
    grid_destroy(g);
}

/* Test 2: Life-Without-Death never reduces alive count. */
static void test_lwd_monotone(void) {
    Grid *g = make_grid(16);
    grid_clear(g);
    /* Seed a small random cluster in the middle. */
    g->cells[7 * 16 + 7] = 1;
    g->cells[7 * 16 + 8] = 1;
    g->cells[8 * 16 + 7] = 1;
    g->cells[8 * 16 + 8] = 1;
    g->cells[8 * 16 + 9] = 1;

    Ruleset r = ruleset_life_without_death();
    r.p[6] &= (uint8_t)~0x10; /* isotropic */
    r.p[6] &= (uint8_t)~0x20; /* no bias */

    int prev = count_alive(g);
    int ok = 1;
    for (int step = 0; step < 10; step++) {
        ruleset_step_grid(&r, g, 0, NULL);
        int cur = count_alive(g);
        if (cur < prev) { ok = 0; break; }
        prev = cur;
    }
    CHECK(ok, "Life-Without-Death: alive count never decreases");
    grid_destroy(g);
}

/* Test 3: refractory layer prevents immediate rebirth. */
static void test_refractory(void) {
    Grid *g = make_grid(8);
    grid_clear(g);
    /* Single cell with 0 neighbors: dies next step under any normal rule. */
    g->cells[4 * 8 + 4] = 1;

    Ruleset r = ruleset_conway();
    /* Enable refractory with 3 ticks. */
    r.p[4] = (uint8_t)(3 << 5);
    r.p[6] |= 0x40;
    r.p[6] &= (uint8_t)~0x10;
    r.p[6] &= (uint8_t)~0x20;

    uint8_t *refr = calloc(64, 1);

    /* Step 1: cell dies (0 neighbors -> Conway death). */
    ruleset_step_grid(&r, g, 0, refr);
    CHECK(g->cells[4 * 8 + 4] == 0, "Refractory step1: cell dies");
    CHECK(refr[4 * 8 + 4] == 3,     "Refractory step1: refr_age set to 3");

    /* Surround the dead cell with 3 alive neighbors so Conway would re-birth.
     * Refractory should suppress birth for the next 3 ticks. */
    g->cells[3 * 8 + 4] = 1;
    g->cells[5 * 8 + 4] = 1;
    g->cells[4 * 8 + 3] = 1;

    for (int t = 0; t < 3; t++) {
        ruleset_step_grid(&r, g, 0, refr);
        if (g->cells[4 * 8 + 4] != 0) {
            fprintf(stderr, "FAIL: refractory broke at tick %d\n", t);
            failures++;
            free(refr); grid_destroy(g);
            return;
        }
    }
    fprintf(stderr, "ok:   Refractory suppresses birth for 3 ticks\n");

    free(refr);
    grid_destroy(g);
}

/* Test 4: anisotropy strength = 255 should make perpendicular neighbors
 * contribute zero to the count. Demonstrate that a "wall" of alive cells
 * along x=0, with all cells oriented North (orient=2), does NOT propagate
 * East (E neighbors invisible). */
static void test_anisotropy(void) {
    int sz = 16;
    Grid *g = make_grid(sz);
    grid_clear(g);
    /* Set every cell's mode-0 orientation to NORTH (2). With max anisotropy,
     * each cell only "sees" N and S neighbors (and 45° NE/NW/SE/SW at half). */
    for (int i = 0; i < sz * sz; i++) {
        g->genomes[i] = genome_pack_full(0, 0, 0, /*o0=*/2, 2, 2, 2, 0, 0);
    }
    /* Fill column x=0 entirely alive. */
    for (int y = 0; y < sz; y++) g->cells[y * sz + 0] = 1;

    Ruleset r = ruleset_life_without_death();
    r.p[2] = 255;          /* max anisotropy */
    r.p[6] |= 0x10;        /* enable aniso layer */
    r.p[6] &= (uint8_t)~0x20; /* disable bias for clean test */

    /* Step 5 times; column x=1 should remain mostly dead because for a cell
     * at (1,y) oriented N, the E-W direction (towards x=0) is perpendicular. */
    for (int t = 0; t < 5; t++) ruleset_step_grid(&r, g, 0, NULL);

    int col1_alive = 0;
    for (int y = 0; y < sz; y++) col1_alive += g->cells[y * sz + 1];
    /* With LWD any spurious birth persists. Allow some bleed via diagonal half-scale,
     * but ensure column 1 is not fully filled. */
    CHECK(col1_alive < sz, "Anisotropy 255 + N orientation: blocks E-W propagation");
    grid_destroy(g);
}

/* Test 5: pack/unpack roundtrip. */
static void test_pack_roundtrip(void) {
    rng_seed(12345);
    int ok = 1;
    for (int trial = 0; trial < 32; trial++) {
        Ruleset a = ruleset_random(rng_u64);
        uint64_t v = ruleset_pack(&a);
        Ruleset b = ruleset_unpack(v);
        for (int i = 0; i < 7; i++) {
            if (a.p[i] != b.p[i]) { ok = 0; break; }
        }
    }
    CHECK(ok, "pack/unpack roundtrip preserves all 7 bytes");
}

int main(void) {
    test_conway_blinker();
    test_lwd_monotone();
    test_refractory();
    test_anisotropy();
    test_pack_roundtrip();
    if (failures) {
        fprintf(stderr, "\n%d failure(s)\n", failures);
        return 1;
    }
    fprintf(stderr, "\nAll composable_ruleset tests passed.\n");
    return 0;
}
