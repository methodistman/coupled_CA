#include <stdio.h>
#include <string.h>
#include "../ca_core/grid.h"
#include "../ca_core/patterns.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

int main(void) {
    int errors = 0;
    printf("test_patterns\n");

    /* Test 1: glider pattern has 5 alive cells */
    {
        Grid *g = grid_create(8);
        grid_clear(g);
        pattern_glider(g, 2, 2);
        int alive = 0;
        for (int i = 0; i < g->size * g->size; i++) alive += g->cells[i];
        if (alive != 5) FAIL("glider_alive_count"); else PASS("glider_alive_count");
        grid_destroy(g);
    }

    /* Test 2: block pattern has 4 alive cells */
    {
        Grid *g = grid_create(8);
        grid_clear(g);
        pattern_block(g, 1, 1);
        int alive = 0;
        for (int i = 0; i < g->size * g->size; i++) alive += g->cells[i];
        if (alive != 4) FAIL("block_alive_count"); else PASS("block_alive_count");
        grid_destroy(g);
    }

    /* Test 3: glider gun fits in 48x12 grid */
    {
        Grid *g = grid_create(48);
        grid_clear(g);
        pattern_glider_gun(g, 1, 1);
        int alive = 0;
        for (int i = 0; i < g->size * g->size; i++) alive += g->cells[i];
        if (alive != 36) FAIL("glider_gun_alive_count"); else PASS("glider_gun_alive_count");
        grid_destroy(g);
    }

    /* Test 4: census finds injected glider */
    {
        Grid *g = grid_create(16);
        grid_clear(g);
        pattern_glider(g, 5, 5);
        int n = census_gliders(g);
        if (n != 1) FAIL("census_gliders"); else PASS("census_gliders");
        grid_destroy(g);
    }

    /* Test 5: census finds blinker */
    {
        Grid *g = grid_create(16);
        grid_clear(g);
        pattern_blinker(g, 3, 3);
        int n = census_oscillators(g);
        if (n < 1) FAIL("census_oscillators_blinker"); else PASS("census_oscillators_blinker");
        grid_destroy(g);
    }

    /* Test 6: beacon pattern census */
    {
        Grid *g = grid_create(16);
        grid_clear(g);
        pattern_beacon(g, 2, 2);
        int n = census_oscillators(g);
        if (n < 1) FAIL("census_oscillators_beacon"); else PASS("census_oscillators_beacon");
        grid_destroy(g);
    }

    printf("%d errors\n", errors);
    return errors ? 1 : 0;
}
