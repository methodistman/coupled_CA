#ifndef WAVE_ENGINE_H
#define WAVE_ENGINE_H

#include "wave_grid.h"
#include "wave_coupling.h"
#include "wave_rules.h"
#include "wave_intent.h"
#include "intent.h"

struct WaveMetricsHistory;

typedef struct WaveSystem {
    WaveGrid *grids[MAX_WAVE_GRIDS];
    int num_grids;
    WaveCoupling coupling;
    WaveIntentBuffer *intent_buf;
    WaveIntentRing *intent_ring;
    struct WaveMetricsHistory *metrics_history;
    int nb_genome_interval;   /* recompute nb_genomes every N ticks */
    int tick_counter;           /* increments every step */

    /* Slow-layer (nb_genome) GA configuration, applied after each
       wave_grid_recompute_nb_genomes call. If rng_fn is NULL the slow-layer
       mutation step is skipped and the engine only recomputes the consensus. */
    int    consensus_mode;       /* 0=vote, 1=avg, 2=fittest (see wave_grid_recompute_nb_genomes) */
    double nb_mutation_rate;     /* per-field probability passed to genome_mutate */
    int    mutate_mask;          /* GENOME_MUTATE_* bitmask; 0 = no mutation */
    uint64_t (*rng_fn)(void);    /* RNG used for slow-layer mutation; NULL disables */
} WaveSystem;

void wave_sys_init(WaveSystem *s, int num_grids, int grid_size);
void wave_sys_destroy(WaveSystem *s);
void wave_sys_step(WaveSystem *s);
void wave_sys_step_genomic(WaveSystem *s);
void wave_sys_step_intent(WaveSystem *s, IntentMode mode, float alpha);
void wave_sys_randomize(WaveSystem *s, uint64_t (*rng)(void));
void wave_sys_recompute_nb(WaveSystem *s, int mode);

#endif
