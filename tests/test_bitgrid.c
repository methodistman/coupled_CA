#include <stdio.h>
#include <string.h>
#include "../ca_core/bitgrid.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

int main(void) {
    int errors = 0;
    printf("test_bitgrid\n");

    /* create + size */
    {
        BitGrid *g = bitgrid_create(64);
        if (g && g->size == 64 && g->words_per_row == 1)
            PASS("create_64");
        else
            FAIL("create_64");
        bitgrid_destroy(g);
    }

    /* get/set */
    {
        BitGrid *g = bitgrid_create(64);
        bitgrid_clear(g);
        bitgrid_set(g, 5, 7, 1);
        if (bitgrid_get(g, 5, 7) == 1 && bitgrid_get(g, 4, 7) == 0)
            PASS("set_get");
        else
            FAIL("set_get");
        bitgrid_destroy(g);
    }

    /* copy */
    {
        BitGrid *a = bitgrid_create(64);
        BitGrid *b = bitgrid_create(64);
        bitgrid_clear(a);
        bitgrid_set(a, 10, 20, 1);
        bitgrid_copy(a, b);
        if (bitgrid_get(b, 10, 20) == 1)
            PASS("copy");
        else
            FAIL("copy");
        bitgrid_destroy(a);
        bitgrid_destroy(b);
    }

    /* hash determinism */
    {
        BitGrid *a = bitgrid_create(64);
        BitGrid *b = bitgrid_create(64);
        bitgrid_clear(a);
        bitgrid_set(a, 1, 2, 1);
        bitgrid_set(a, 3, 4, 1);
        bitgrid_copy(a, b);
        if (bitgrid_hash(a) == bitgrid_hash(b))
            PASS("hash_deterministic");
        else
            FAIL("hash_deterministic");
        bitgrid_destroy(a);
        bitgrid_destroy(b);
    }

    /* Life blinker via LUT */
    {
        BitGrid *g = bitgrid_create(64);
        BitGrid *tmp = bitgrid_create(64);
        bitgrid_clear(g);
        bitgrid_set(g, 10, 9, 1);
        bitgrid_set(g, 10, 10, 1);
        bitgrid_set(g, 10, 11, 1);
        bitgrid_step_lut(g, tmp, BITGRID_RULE_LIFE);
        int h = bitgrid_get(g, 9, 10) && bitgrid_get(g, 10, 10) && bitgrid_get(g, 11, 10);
        bitgrid_step_lut(g, tmp, BITGRID_RULE_LIFE);
        int v = bitgrid_get(g, 10, 9) && bitgrid_get(g, 10, 10) && bitgrid_get(g, 10, 11);
        if (h && v)
            PASS("life_blinker_lut");
        else
            FAIL("life_blinker_lut");
        bitgrid_destroy(g);
        bitgrid_destroy(tmp);
    }

    /* Life blinker via fast bit-parallel path */
    {
        BitGrid *g = bitgrid_create(64);
        bitgrid_clear(g);
        bitgrid_set(g, 10, 9, 1);
        bitgrid_set(g, 10, 10, 1);
        bitgrid_set(g, 10, 11, 1);
        bitgrid_step_life(g);
        int h = bitgrid_get(g, 9, 10) && bitgrid_get(g, 10, 10) && bitgrid_get(g, 11, 10);
        bitgrid_step_life(g);
        int v = bitgrid_get(g, 10, 9) && bitgrid_get(g, 10, 10) && bitgrid_get(g, 10, 11);
        if (h && v)
            PASS("life_blinker_fast");
        else
            FAIL("life_blinker_fast");
        bitgrid_destroy(g);
    }

    /* Block still life — both paths should leave it unchanged */
    {
        BitGrid *g_lut = bitgrid_create(64);
        BitGrid *tmp = bitgrid_create(64);
        BitGrid *g_fast = bitgrid_create(64);
        bitgrid_clear(g_lut);
        bitgrid_clear(g_fast);
        /* 2x2 block at (20,20) */
        bitgrid_set(g_lut, 20, 20, 1);
        bitgrid_set(g_lut, 21, 20, 1);
        bitgrid_set(g_lut, 20, 21, 1);
        bitgrid_set(g_lut, 21, 21, 1);
        bitgrid_set(g_fast, 20, 20, 1);
        bitgrid_set(g_fast, 21, 20, 1);
        bitgrid_set(g_fast, 20, 21, 1);
        bitgrid_set(g_fast, 21, 21, 1);
        bitgrid_step_lut(g_lut, tmp, BITGRID_RULE_LIFE);
        bitgrid_step_life(g_fast);
        int lut_ok = bitgrid_get(g_lut, 20, 20) && bitgrid_get(g_lut, 21, 20) &&
                     bitgrid_get(g_lut, 20, 21) && bitgrid_get(g_lut, 21, 21);
        int fast_ok = bitgrid_get(g_fast, 20, 20) && bitgrid_get(g_fast, 21, 20) &&
                      bitgrid_get(g_fast, 20, 21) && bitgrid_get(g_fast, 21, 21);
        if (lut_ok && fast_ok)
            PASS("life_block_still");
        else
            FAIL("life_block_still");
        bitgrid_destroy(g_lut);
        bitgrid_destroy(tmp);
        bitgrid_destroy(g_fast);
    }

    /* Equivalence: fast path vs LUT path on a 128x128 random grid */
    {
        BitGrid *g_lut = bitgrid_create(128);
        BitGrid *tmp = bitgrid_create(128);
        BitGrid *g_fast = bitgrid_create(128);
        /* deterministic pseudo-random seed */
        for (int i = 0; i < 128 * 2; i++) {
            uint64_t val = ((uint64_t)(i + 1) * 0x9E3779B97F4A7C15ULL);
            g_lut->bits[i] = val;
            g_fast->bits[i] = val;
        }
        bitgrid_step_lut(g_lut, tmp, BITGRID_RULE_LIFE);
        bitgrid_step_life(g_fast);
        int match = 1;
        for (int i = 0; i < 128 * 2; i++) {
            if (g_lut->bits[i] != g_fast->bits[i]) { match = 0; break; }
        }
        if (match)
            PASS("fast_lut_equivalence_128");
        else
            FAIL("fast_lut_equivalence_128");
        bitgrid_destroy(g_lut);
        bitgrid_destroy(tmp);
        bitgrid_destroy(g_fast);
    }

    printf("%d errors\n", errors);
    return errors;
}
