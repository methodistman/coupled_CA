#include <stdio.h>
#include <string.h>
#include "../ca_core/grid.h"
#include "../metrics/metrics.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

int main(void) {
    int errors = 0;
    printf("test_grid\n");

    /* create + size */
    Grid *g = grid_create(64);
    if (!g || g->size != 64) FAIL("grid_create"); else PASS("grid_create");

    /* get/set */
    grid_set(g, 10, 20, 1);
    if (grid_get(g, 10, 20) != 1) FAIL("grid_set_get"); else PASS("grid_set_get");
    grid_set(g, 10, 20, 0);
    if (grid_get(g, 10, 20) != 0) FAIL("grid_clear_cell"); else PASS("grid_clear_cell");

    /* clear */
    grid_set(g, 5, 5, 1);
    grid_clear(g);
    if (grid_get(g, 5, 5) != 0) FAIL("grid_clear"); else PASS("grid_clear");

    /* copy */
    Grid *h = grid_create(64);
    grid_set(g, 3, 4, 1);
    grid_copy(g, h);
    if (grid_get(h, 3, 4) != 1) FAIL("grid_copy"); else PASS("grid_copy");
    grid_set(h, 3, 4, 0);
    if (grid_get(g, 3, 4) != 1) FAIL("grid_copy_independent"); else PASS("grid_copy_independent");

    /* hash determinism */
    uint64_t ha = grid_hash(g);
    uint64_t hb = grid_hash(g);
    if (ha != hb) FAIL("grid_hash_deterministic"); else PASS("grid_hash_deterministic");

    /* hash sensitivity */
    grid_set(g, 3, 4, 0);
    uint64_t hc = grid_hash(g);
    if (ha == hc) FAIL("grid_hash_sensitive"); else PASS("grid_hash_sensitive");

    grid_destroy(g);
    grid_destroy(h);

    printf("%d errors\n", errors);
    return errors ? 1 : 0;
}
