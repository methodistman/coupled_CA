/*
 * exp_compositional_logic: Chain two evolved binary logic gates and test
 * whether the composite system computes the expected truth table.
 *
 * Topology:  input_buf -> gate_A -> gate_B -> output
 *   Grid 0 = input buffer (reset each tick with A,B)
 *   Grid 1 = gate A (loaded genomes from file A)
 *   Grid 2 = gate B (loaded genomes from file B)
 *   Coupling: g0 RIGHT -> g1 LEFT  (gate A receives input)
 *   Coupling: g1 RIGHT -> g2 LEFT  (gate B receives gate A output)
 *
 * Usage:
 *   ./exp_compositional_logic --gate-a and --gate-b xor \
 *       --genome-a data/gate_and.bin --genome-b data/gate_xor.bin \
 *       --size 16 --steps 30 --seed 42
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/coupling.h"
#include "../ca_core/genome.h"
#include "../utils/rng.h"

#define DEF_SIZE  16
#define DEF_STEPS 30
#define DEF_SEED  42

typedef struct {
    char *name;
    int truth[4];
    char *expected[4];
} GateDef;

static GateDef GATES[] = {
    {"and",  {0,0,0,1}, {"0,0,0", "0,0,0", "0,0,0", "0,0,1"}},
    {"or",   {0,1,1,1}, {"0,0,0", "0,0,1", "0,0,1", "0,0,1"}},
    {"xor",  {0,1,1,0}, {"0,0,0", "0,0,1", "0,0,1", "0,0,0"}},
    {"nand", {1,1,1,0}, {"0,0,1", "0,0,1", "0,0,1", "0,0,0"}},
    {NULL,   {0,0,0,0}, {NULL,NULL,NULL,NULL}}
};

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --gate-a  and|or|xor|nand   Gate A truth table (default and)\n");
    fprintf(stderr, "  --gate-b  and|or|xor|nand   Gate B truth table (default xor)\n");
    fprintf(stderr, "  --genome-a FILE            Binary genome file for gate A\n");
    fprintf(stderr, "  --genome-b FILE            Binary genome file for gate B\n");
    fprintf(stderr, "  --size N                   Grid size (default %d)\n", DEF_SIZE);
    fprintf(stderr, "  --steps N                  Eval steps per input (default %d)\n", DEF_STEPS);
    fprintf(stderr, "  --seed N                   RNG seed (default %d)\n", DEF_SEED);
    fprintf(stderr, "  --out-mode strict|density  Readout mode (default density)\n");
    fprintf(stderr, "  --help\n");
}

static GateDef *find_gate(const char *name) {
    for (int i = 0; GATES[i].name; i++)
        if (!strcmp(GATES[i].name, name)) return &GATES[i];
    return NULL;
}

static int read_output(const Grid *g, int mode) {
    int sz = g->size, half = sz / 2;
    int c = 0;
    for (int y = 0; y < half; y++) c += g->cells[y * sz + (sz - 1)];
    if (mode == 0) return (c > half / 2) ? 1 : 0;
    int total = 0, n = sz * sz;
    for (int i = 0; i < n; i++) total += g->cells[i];
    int expected_num = total * half;
    int sampled_num  = c * n;
    return (sampled_num > expected_num) ? 1 : 0;
}

static void set_input(Grid *g, int a, int b) {
    int sz = g->size, half = sz / 2;
    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < half; x++) g->cells[y * sz + x] = (uint8_t)a;
        for (int x = half; x < sz; x++) g->cells[y * sz + x] = (uint8_t)b;
    }
}

static int load_genomes(const char *path, CellGenome **out, int *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
    int size = 0;
    if (fread(&size, sizeof(int), 1, fp) != 1) { fclose(fp); return -1; }
    int n = size * size;
    CellGenome *g = malloc(n * sizeof(CellGenome));
    if (!g || fread(g, sizeof(CellGenome), n, fp) != (size_t)n) {
        free(g); fclose(fp); return -1;
    }
    fclose(fp);
    *out = g;
    *out_size = size;
    return 0;
}

int main(int argc, char **argv) {
    const char *ga_name = "and", *gb_name = "xor";
    const char *genome_a = NULL, *genome_b = NULL;
    int size = DEF_SIZE, steps = DEF_STEPS;
    uint64_t seed = DEF_SEED;
    int out_mode = 1; /* density */

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gate-a") && i+1 < argc) ga_name = argv[++i];
        else if (!strcmp(argv[i], "--gate-b") && i+1 < argc) gb_name = argv[++i];
        else if (!strcmp(argv[i], "--genome-a") && i+1 < argc) genome_a = argv[++i];
        else if (!strcmp(argv[i], "--genome-b") && i+1 < argc) genome_b = argv[++i];
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--out-mode") && i+1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "strict")) out_mode = 0;
            else if (!strcmp(m, "density")) out_mode = 1;
        }
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
    }

    GateDef *ga = find_gate(ga_name);
    GateDef *gb = find_gate(gb_name);
    if (!ga || !gb) { fprintf(stderr, "Unknown gate name\n"); return 1; }

    /* Compute expected composite truth table */
    int composite[4];
    for (int i = 0; i < 4; i++) {
        int a = (i >> 1) & 1;
        int b = i & 1;
        int mid = ga->truth[i];          /* gate A output */
        int idx2 = (mid << 1) | b;        /* gate B input: A=output_A, B=original_B */
        composite[i] = gb->truth[idx2 & 3];
    }

    /* Load genomes */
    CellGenome *genomes_a = NULL, *genomes_b = NULL;
    int size_a = 0, size_b = 0;
    if (genome_a) {
        if (load_genomes(genome_a, &genomes_a, &size_a) < 0) return 1;
        if (size_a != size) {
            fprintf(stderr, "Genome A size mismatch: file=%d vs args=%d\n", size_a, size);
            return 1;
        }
    }
    if (genome_b) {
        if (load_genomes(genome_b, &genomes_b, &size_b) < 0) return 1;
        if (size_b != size) {
            fprintf(stderr, "Genome B size mismatch: file=%d vs args=%d\n", size_b, size);
            return 1;
        }
    }

    rng_seed(seed);

    /* 3-grid system: input_buf, gate_A, gate_B */
    System sys;
    sys_init(&sys, 3, size);
    for (int g = 0; g < 3; g++) {
        sys.grids[g]->active = 1;
        sys.grids[g]->rule_idx = 0; /* Conway's Life */
    }
    /* Couplings: input -> A, A -> B */
    coupling_connect(&sys.coupling, 1, EDGE_LEFT, 0, EDGE_RIGHT);
    coupling_connect(&sys.coupling, 2, EDGE_LEFT, 1, EDGE_RIGHT);

    if (genomes_a) {
        grid_alloc_genomes(sys.grids[1]);
        memcpy(sys.grids[1]->genomes, genomes_a, size*size*sizeof(CellGenome));
    }
    if (genomes_b) {
        grid_alloc_genomes(sys.grids[2]);
        memcpy(sys.grids[2]->genomes, genomes_b, size*size*sizeof(CellGenome));
    }

    sys_randomize(&sys, rng_u64);

    int ncells = size * size;
    uint8_t *bk0 = malloc(ncells);
    uint8_t *bk1 = malloc(ncells);
    uint8_t *bk2 = malloc(ncells);
    memcpy(bk0, sys.grids[0]->cells, ncells);
    memcpy(bk1, sys.grids[1]->cells, ncells);
    memcpy(bk2, sys.grids[2]->cells, ncells);

    printf("# exp_compositional_logic: %s -> %s (size=%d steps=%d seed=%llu)\n",
           ga_name, gb_name, size, steps, (unsigned long long)seed);
    printf("# Gate A truth: ");
    for (int i = 0; i < 4; i++) printf("%d", ga->truth[i]);
    printf("\n# Gate B truth: ");
    for (int i = 0; i < 4; i++) printf("%d", gb->truth[i]);
    printf("\n# Expected composite: ");
    for (int i = 0; i < 4; i++) printf("%d", composite[i]);
    printf("\n# input_A,input_B,output_A,output_B,expected_B,match\n");

    int total_match = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            memcpy(sys.grids[0]->cells, bk0, ncells);
            memcpy(sys.grids[1]->cells, bk1, ncells);
            memcpy(sys.grids[2]->cells, bk2, ncells);

            for (int st = 0; st < steps; st++) {
                sys_step(&sys);
                set_input(sys.grids[0], a, b);
            }
            int out_a = read_output(sys.grids[1], out_mode);
            int out_b = read_output(sys.grids[2], out_mode);
            int idx = (a << 1) | b;
            int expected_b = composite[idx];
            int match = (out_b == expected_b) ? 1 : 0;
            total_match += match;
            printf("%d,%d,%d,%d,%d,%d\n", a, b, out_a, out_b, expected_b, match);
        }
    }
    printf("# Composite accuracy: %d/4\n", total_match);

    free(bk0); free(bk1); free(bk2);
    free(genomes_a); free(genomes_b);
    sys_destroy(&sys);
    return (total_match == 4) ? 0 : 1;
}
