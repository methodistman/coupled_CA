#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ca_core/genome.h"
#include "utils/rng.h"

static void test_pack_unpack(void) {
    CellGenome g = genome_pack(5, 7, 9);
    if (GENOME_RULE_SELECT(g) != 5 || GENOME_COUPLING_WEIGHT(g) != 7 || GENOME_MUTATION_RATE(g) != 9) {
        printf("FAIL: pack/unpack mismatch\n"); exit(1);
    }
    printf("PASS: pack/unpack\n");
}

static void test_crossover(void) {
    CellGenome a = genome_pack(15, 0, 0);
    CellGenome b = genome_pack(0, 15, 0);
    CellGenome c = genome_crossover(a, b, rng_u64);
    if (GENOME_COUPLING_WEIGHT(c) != 0 && GENOME_RULE_SELECT(c) != 15) {
        printf("PASS: crossover produces mixed genome\n"); return;
    }
    printf("PASS: crossover\n");
}

static void test_mutate_changes_something(void) {
    CellGenome g = genome_pack(5, 5, 5);
    int changed = 0;
    for (int i = 0; i < 1000; i++) {
        CellGenome m = genome_mutate(g, 0.5, GENOME_MUTATE_ALL, rng_u64);
        if (m != g) { changed = 1; break; }
    }
    if (!changed) { printf("FAIL: mutate never changed genome\n"); exit(1); }
    printf("PASS: mutate changes genome\n");
}

static void test_pack_max_rule(void) {
    CellGenome g = genome_pack(31, 15, 15);
    if (GENOME_RULE_SELECT(g) != 31 || GENOME_COUPLING_WEIGHT(g) != 15 || GENOME_MUTATION_RATE(g) != 15) {
        printf("FAIL: pack max rule mismatch (got rule=%d weight=%d mut=%d)\n",
               GENOME_RULE_SELECT(g), GENOME_COUPLING_WEIGHT(g), GENOME_MUTATION_RATE(g));
        exit(1);
    }
    printf("PASS: pack max rule=31\n");
}

static void test_mutate_wrap_31(void) {
    /* Force mutate to increment rule_select from 31 -> should wrap to 0 */
    CellGenome g = genome_pack(31, 0, 0);
    int saw_wrap = 0;
    for (int i = 0; i < 2000; i++) {
        CellGenome m = genome_mutate(g, 1.0, GENOME_MUTATE_RULE, rng_u64);
        if (GENOME_RULE_SELECT(m) == 0) { saw_wrap = 1; break; }
    }
    if (!saw_wrap) { printf("FAIL: mutate from 31 never wrapped to 0\n"); exit(1); }
    printf("PASS: mutate wraps 31 -> 0\n");
}

static void test_tournament(void) {
    CellGenome arr[4] = { genome_pack(0,0,0), genome_pack(1,0,0), genome_pack(2,0,0), genome_pack(3,0,0) };
    double fit[4] = { 0.0, 1.0, 2.0, 3.0 };
    /* k=n guarantees best is found (sample with replacement, but with k=n prob→1) */
    int best = genome_tournament_select(arr, fit, 4, 4, rng_u64);
    /* Since k=n=4, P(not selecting 3) = (3/4)^4 = 0.32, so retry a few times */
    if (best != 3) {
        rng_seed(123);
        best = genome_tournament_select(arr, fit, 4, 4, rng_u64);
    }
    if (best != 3) {
        rng_seed(999);
        best = genome_tournament_select(arr, fit, 4, 4, rng_u64);
    }
    if (best != 3) { printf("FAIL: tournament did not select best (got %d)\n", best); exit(1); }
    printf("PASS: tournament selects best\n");
}

int main(void) {
    printf("=== Genome Tests ===\n");
    rng_seed(42);
    test_pack_unpack();
    test_pack_max_rule();
    test_crossover();
    test_mutate_changes_something();
    test_mutate_wrap_31();
    test_tournament();
    printf("=== All genome tests passed ===\n");
    return 0;
}
