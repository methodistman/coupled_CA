/*
 * test_orientation: verify anisotropic coupling kernel.
 * Setup: 3x3 grid, vertical line alive (cells 1,4,7).
 * All genomes orientation=0 (East), weight=7, rule=Life.
 * Isotropic: center cell 4 sees 2 alive neighbors (1 and 7), survives ~21%.
 * Anisotropic: center cell 4 sees 0 aligned neighbors (1=N and 7=S are orthogonal
 *   to East orientation, so ignored). Survival rate ~0%.
 * We compare isotropic vs anisotropic survival of the center cell.
 */
#include <stdio.h>
#include <string.h>
#include "../ca_core/engine.h"
#include "../ca_core/genome.h"
#include "../ca_core/pipeline.h"
#include "../utils/rng.h"

#define TRIALS 2000

static int run_batch(int use_orientation, uint64_t seed) {
    rng_seed(seed);
    System sys;
    sys_init(&sys, 1, 3);
    sys.grids[0]->active = 1;
    sys.grids[0]->rule_idx = 0; /* Life */
    grid_alloc_genomes(sys.grids[0]);
    for (int i = 0; i < 9; i++) {
        sys.grids[0]->genomes[i] = genome_pack_oriented(0, 7, 0, 0);
    }

    int center_survived = 0;
    for (int t = 0; t < TRIALS; t++) {
        memset(sys.grids[0]->cells, 0, 9);
        sys.grids[0]->cells[1] = 1; /* North */
        sys.grids[0]->cells[4] = 1; /* Center */
        sys.grids[0]->cells[7] = 1; /* South */
        memcpy(sys.grids[0]->next_cells, sys.grids[0]->cells, 9);

        if (use_orientation) {
            sys.pipeline = pipeline_preset_genomic_orientation(
                INTENT_MODE_REPLACE, 0.0f, 0, -1, 0, 0, 1.0, 0);
        } else {
            sys.pipeline = pipeline_preset_genomic(
                INTENT_MODE_REPLACE, 0.0f, 0, -1, 0, 0, 1.0, 0);
        }
        sys_step(&sys);
        pipeline_destroy(sys.pipeline);
        sys.pipeline = NULL;
        center_survived += sys.grids[0]->cells[4];
    }
    sys_destroy(&sys);
    return center_survived;
}

int main(void) {
    int iso = run_batch(0, 12345);
    int aniso = run_batch(1, 12345);

    printf("Vertical line (1,4,7), weight=7, orientation=East, %d trials\n", TRIALS);
    printf("Isotropic center survival:    %d (%.1f%%)\n", iso, 100.0 * iso / TRIALS);
    printf("Anisotropic center survival:    %d (%.1f%%)\n", aniso, 100.0 * aniso / TRIALS);

    if (iso < 50) {
        printf("WARN: isotropic survival unexpectedly low; probabilistic coupling may be broken\n");
    }
    if (aniso > iso / 3) {
        printf("FAIL: anisotropic did not suppress survival enough (got %d, expected < %d)\n",
               aniso, iso / 3);
        return 1;
    }
    printf("PASS: anisotropic coupling suppresses orthogonal neighbors\n");
    return 0;
}
