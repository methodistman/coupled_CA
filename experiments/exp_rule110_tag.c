/*
 * exp_rule110_tag: Validate Rule 110 implementation and simulate a
 * 1D cyclic tag system precursor.  Two modes:
 *   --mode validate   : print row evolution for a simple IC
 *   --mode tag        : run a tiny cyclic tag system (hardcoded)
 *
 * Rule 110 is Turing-complete via Matthew Cook's construction.
 * This experiment is a stepping-stone toward full cyclic-tag encoding.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../utils/rng.h"

#define DEF_SIZE   128
#define DEF_STEPS  200
#define DEF_SEED   42

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --mode validate|tag   (default validate)\n");
    fprintf(stderr, "  --size N            Row width (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --steps N           Steps to run (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N            RNG seed for random IC (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --output FILE       Save raw row states as CSV\n");
    fprintf(stderr, "  --help\n");
}

static void print_row(const Grid *g) {
    int sz = g->size;
    for (int x = 0; x < sz; x++) printf(g->cells[x] ? "#" : ".");
    printf("\n");
}

static void validate_mode(int size, int steps, uint64_t seed, const char *outpath) {
    rng_seed(seed);
    System sys;
    sys_init(&sys, 1, size);
    sys.grids[0]->active = 1;
    sys.grids[0]->rule_idx = rules_index_by_name("Rule 110");
    sys_randomize(&sys, rng_u64);

    FILE *fp = outpath ? fopen(outpath, "w") : NULL;
    if (fp) {
        fprintf(fp, "step,");
        for (int x = 0; x < size; x++) fprintf(fp, "c%d%s", x, x+1<size?",":"");
        fprintf(fp, "\n");
    }

    printf("# Rule 110 validation: size=%d steps=%d seed=%llu\n", size, steps, (unsigned long long)seed);
    printf("# Row-0 (initial condition):\n");
    print_row(sys.grids[0]);

    for (int s = 0; s < steps; s++) {
        sys_step(&sys);
        if (s < 20) { /* only print first 20 steps to stdout */
            printf("# Row-%d:\n", s+1);
            print_row(sys.grids[0]);
        }
        if (fp) {
            fprintf(fp, "%d,", s+1);
            for (int x = 0; x < size; x++)
                fprintf(fp, "%d%s", sys.grids[0]->cells[x], x+1<size?",":"");
            fprintf(fp, "\n");
        }
    }
    if (fp) fclose(fp);
    sys_destroy(&sys);
}

/* Simple cyclic tag system: two productions A->10, B->0, initial word AB.
   We don't encode this into Rule 110 here; we just verify the engine
   can run 1D rules cleanly.  Full encoding requires the Cook construction. */
static void tag_mode(int size, int steps, uint64_t seed) {
    (void)seed;
    printf("# Cyclic tag system mode: not yet implemented.\n");
    printf("# Rule 110 engine validated in --mode validate.\n");
    printf("# Next step: implement Cook's cyclic-tag encoder/decoder.\n");
}

int main(int argc, char **argv) {
    int size = DEF_SIZE, steps = DEF_STEPS;
    uint64_t seed = DEF_SEED;
    const char *mode = "validate";
    const char *outpath = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mode") && i+1 < argc) mode = argv[++i];
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--output") && i+1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    if (!strcmp(mode, "validate")) {
        validate_mode(size, steps, seed, outpath);
    } else if (!strcmp(mode, "tag")) {
        tag_mode(size, steps, seed);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }
    return 0;
}
