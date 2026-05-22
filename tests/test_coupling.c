#include <stdio.h>
#include <string.h>
#include "../ca_core/grid.h"
#include "../ca_core/coupling.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

static void init_pattern(Grid *g, int val) {
    for (int y = 0; y < g->size; y++)
        for (int x = 0; x < g->size; x++)
            g->cells[y * g->size + x] = val;
}

int main(void) {
    int errors = 0;
    printf("test_coupling\n");

    /* Test 1: no coupling → boundary returns 0 */
    {
        Grid *g = grid_create(8);
        init_pattern(g, 1);
        Grid *grids[1] = {g};
        Coupling c;
        coupling_init(&c, 1);
        int v = coupling_read(&c, grids, 0, 0, 0, -1, 0); /* top boundary */
        if (v != 0) FAIL("open_boundary_top"); else PASS("open_boundary_top");
        grid_destroy(g);
    }

    /* Test 2: top->bottom connection */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(4);
        init_pattern(a, 0);
        init_pattern(b, 0);
        /* Set b top edge cells to 1 */
        for (int x = 0; x < 4; x++) grid_set(b, x, 0, 1);
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect(&c, 0, EDGE_TOP, 1, EDGE_TOP);
        /* Read above a[0,0] → should come from b top edge */
        int v = coupling_read(&c, grids, 0, 0, 0, 0, -1);
        if (v != 1) FAIL("top_edge_coupling"); else PASS("top_edge_coupling");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 3: left->right connection */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(4);
        init_pattern(a, 0);
        init_pattern(b, 0);
        /* Set b left edge cells to 1 */
        for (int y = 0; y < 4; y++) grid_set(b, 0, y, 1);
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect(&c, 0, EDGE_LEFT, 1, EDGE_LEFT);
        int v = coupling_read(&c, grids, 0, 0, 0, -1, 0);
        if (v != 1) FAIL("left_edge_coupling"); else PASS("left_edge_coupling");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 4: right->left connection */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(4);
        init_pattern(a, 0);
        init_pattern(b, 0);
        for (int y = 0; y < 4; y++) grid_set(b, 3, y, 1);
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect(&c, 0, EDGE_RIGHT, 1, EDGE_RIGHT);
        int v = coupling_read(&c, grids, 0, 3, 0, 1, 0);
        if (v != 1) FAIL("right_edge_coupling"); else PASS("right_edge_coupling");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 5: bottom->top connection */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(4);
        init_pattern(a, 0);
        init_pattern(b, 0);
        for (int x = 0; x < 4; x++) grid_set(b, x, 3, 1);
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect(&c, 0, EDGE_BOTTOM, 1, EDGE_BOTTOM);
        int v = coupling_read(&c, grids, 0, 0, 3, 0, 1);
        if (v != 1) FAIL("bottom_edge_coupling"); else PASS("bottom_edge_coupling");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 6: internal neighbor (no boundary) */
    {
        Grid *a = grid_create(4);
        grid_clear(a);
        grid_set(a, 2, 2, 1);
        Grid *grids[1] = {a};
        Coupling c;
        coupling_init(&c, 1);
        int v = coupling_read(&c, grids, 0, 1, 1, 1, 1);
        if (v != 1) FAIL("internal_neighbor"); else PASS("internal_neighbor");
        grid_destroy(a);
    }

    /* Test 7: corner case (both out of bounds) returns 0 */
    {
        Grid *a = grid_create(4);
        init_pattern(a, 1);
        Grid *grids[1] = {a};
        Coupling c;
        coupling_init(&c, 1);
        int v = coupling_read(&c, grids, 0, 0, 0, -1, -1);
        if (v != 0) FAIL("corner_oob_returns_0"); else PASS("corner_oob_returns_0");
        grid_destroy(a);
    }

    /* Test 8: proportional coupling 8x8 -> 4x4 (downscale) */
    {
        Grid *a = grid_create(8);
        Grid *b = grid_create(4);
        init_pattern(a, 0);
        init_pattern(b, 0);
        grid_set(b, 0, 0, 1);  /* top-left of 4x4 */
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect_scaled(&c, 0, EDGE_TOP, 1, EDGE_TOP, SCALE_PROPORTIONAL, 0.5f);
        /* a position x=0 maps to b position 0 (0/8*4 = 0) */
        int v = coupling_read(&c, grids, 0, 0, 0, 0, -1);
        if (v != 1) FAIL("proportional_downscale_0"); else PASS("proportional_downscale_0");
        /* a position x=4 maps to b position 2 (4/8*4 = 2) */
        grid_set(b, 2, 0, 1);
        int v2 = coupling_read(&c, grids, 0, 4, 0, 0, -1);
        if (v2 != 1) FAIL("proportional_downscale_4"); else PASS("proportional_downscale_4");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 9: proportional coupling 4x4 -> 8x8 (upscale) */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(8);
        init_pattern(a, 0);
        init_pattern(b, 0);
        grid_set(b, 4, 0, 1);  /* top edge x=4 on 8x8 */
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect_scaled(&c, 0, EDGE_TOP, 1, EDGE_TOP, SCALE_PROPORTIONAL, 2.0f);
        /* a position x=2 maps to b position 4 (2/4*8 = 4) */
        int v = coupling_read(&c, grids, 0, 2, 0, 0, -1);
        if (v != 1) FAIL("proportional_upscale_2"); else PASS("proportional_upscale_2");
        grid_destroy(a); grid_destroy(b);
    }

    /* Test 10: block mean mode (2x2 average) */
    {
        Grid *a = grid_create(4);
        Grid *b = grid_create(8);
        init_pattern(a, 0);
        init_pattern(b, 0);
        /* Set a 2x2 block in b at (4,0)-(5,1) to 1s */
        grid_set(b, 4, 0, 1); grid_set(b, 5, 0, 1);
        grid_set(b, 4, 1, 1); grid_set(b, 5, 1, 1);
        Grid *grids[2] = {a, b};
        Coupling c;
        coupling_init(&c, 2);
        coupling_connect_scaled(&c, 0, EDGE_TOP, 1, EDGE_TOP, SCALE_BLOCK_MEAN, 2.0f);
        /* a position x=2 maps to b block starting at (4,0) */
        int v = coupling_read(&c, grids, 0, 2, 0, 0, -1);
        if (v != 1) FAIL("block_mean_2x2"); else PASS("block_mean_2x2");
        grid_destroy(a); grid_destroy(b);
    }

    printf("%d errors\n", errors);
    return errors ? 1 : 0;
}
