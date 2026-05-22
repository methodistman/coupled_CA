# Grid Update 01: WaveGrid Core Implementation

## Implementation Status: COMPLETE

All API in this document is implemented in `ca_core/wave_grid.h` and `ca_core/wave_grid.c`.
Unit coverage lives in `tests/test_wave.c`. `wave_grid.o` is included in `CORE` in the Makefile.

## Objective
Implement the concrete C source file `ca_core/wave_grid.c` that instantiates the `WaveGrid` data model defined in `wave_grid.h`. This includes lifecycle management (create / destroy / clear / randomize / copy), neighborhood genome consensus kernels, and spatial coherence computation.

## Files
- **Create**: `ca_core/wave_grid.c`
- **Modify**: `Makefile` (add `ca_core/wave_grid.o` to `CORE` or a new `WAVE` variable)
- **Reference**: `ca_core/wave_grid.h`, `ca_core/genome.h`, `utils/rng.h`

## Data Model Recap

```c
typedef struct {
    uint8_t  alive;
    uint32_t wave;
    CellGenome genome;
    NeighborhoodGenome nb_genome;
    double   fitness;
} WaveCell;

typedef struct {
    int active;
    int size;
    int rule_idx;
    uint64_t rng_state;
    WaveCell *cells;
    WaveCell *next_cells;
} WaveGrid;
```

Per-cell memory: `1 + 4 + 2 + 2 + 8 = 17 bytes`. With `next_cells` copy: **34 bytes/cell**.
For a 128x128 grid: ~557 KB per `WaveGrid`. Four grids: ~2.2 MB. Well within research bounds.

## API Signatures to Implement

```c
WaveGrid *wave_grid_create(int size);
void wave_grid_destroy(WaveGrid *g);
void wave_grid_clear(WaveGrid *g);
void wave_grid_init_random(WaveGrid *g, uint64_t (*rng)(void));
void wave_grid_copy(const WaveGrid *src, WaveGrid *dst);
void wave_grid_recompute_nb_genomes(WaveGrid *g, int mode);
double wave_grid_coherence(const WaveGrid *g, int x, int y);
```

## Implementation Details

### 1. `wave_grid_create(int size)`
- `calloc(1, sizeof(WaveGrid))`
- Set `active = 1`, `size = size`, `rule_idx = 0`
- `n = size * size`
- `cells = calloc(n, sizeof(WaveCell))`
- `next_cells = calloc(n, sizeof(WaveCell))`
- On any allocation failure, call `wave_grid_destroy(g)` and return `NULL`
- Initialize `rng_state` with a deterministic splitmix64 of a global counter or caller-provided seed. For now, leave `rng_state = 0` and let `wave_grid_init_random` set it.

### 2. `wave_grid_destroy(WaveGrid *g)`
- Guard `if (!g) return;`
- `free(g->cells)`
- `free(g->next_cells)`
- `free(g)`

### 3. `wave_grid_clear(WaveGrid *g)`
- Guard `if (!g || !g->active) return;`
- `memset(g->cells, 0, n * sizeof(WaveCell))`

### 4. `wave_grid_init_random(WaveGrid *g, uint64_t (*rng)(void))`
- Guard `if (!g || !g->active) return;`
- `n = g->size * g->size`
- For each cell `i`:
  - `g->cells[i].alive = (rng() % 5 == 0) ? 1 : 0;`  /* 20% density */
  - `g->cells[i].wave = rng() & WAVE_MASK;`         /* 26 random bits */
  - `g->cells[i].genome = genome_random(rng);`      /* from genome.h */
  - `g->cells[i].nb_genome = g->cells[i].genome;`  /* initial consensus = self */
  - `g->cells[i].fitness = 0.0;`
- Set `g->rng_state = rng();`

### 5. `wave_grid_copy(const WaveGrid *src, WaveGrid *dst)`
- Guard `if (!src || !dst || !src->active || !dst->active) return;`
- Assert `src->size == dst->size` (or silently fail).
- `n = src->size * src->size`
- `memcpy(dst->cells, src->cells, n * sizeof(WaveCell));`

### 6. `wave_grid_recompute_nb_genomes(WaveGrid *g, int mode)`

This is the **territorial consensus kernel**. It computes a `nb_genome` for each cell by aggregating the `genome` values of its 8 Moore neighbors (not including self).

#### Mode 0: Majority Vote per Field
For each of the three fields (`rule_select`, `coupling_weight`, `mutation_rate`), collect the 8 neighbor values and take the mode (most frequent). Break ties by choosing the lower value.

```c
static int vote_field(const uint8_t *vals, int n) {
    int freq[16] = {0};  /* all fields are 4-bit */
    for (int i = 0; i < n; i++) freq[vals[i]]++;
    int best = 0, best_count = freq[0];
    for (int v = 1; v < 16; v++) {
        if (freq[v] > best_count) { best = v; best_count = freq[v]; }
    }
    return best;
}
```

Then pack the three voted fields into `nb_genome` using `GENOME_SET_*` macros.

#### Mode 1: Average per Field (rounded)
```c
uint8_t avg_rule = (uint8_t)((sum_rule_select + 4) / 8);
uint8_t avg_wt   = (uint8_t)((sum_coupling_weight + 4) / 8);
uint8_t avg_mut  = (uint8_t)((sum_mutation_rate + 4) / 8);
```

#### Mode 2: Copy from Fittest Neighbor
Among the 8 neighbors, find the one with highest `fitness`. If no neighbor has higher fitness than the cell itself, copy the cell's own `genome`.

**Performance note**: This is O(n) per call. Call it every `N` ticks (e.g., `N = 10`), not every tick. The calling engine tracks the tick counter.

### 7. `wave_grid_coherence(const WaveGrid *g, int x, int y)`

Returns a `double` in `[0.0, 1.0]` indicating how similar this cell's genome is to its neighbors' genomes.

**Definition**: fraction of Moore neighbors (8) whose `genome` matches this cell's `genome` exactly.

Alternative (more nuanced): average of per-field Hamming distance normalized to `[0,1]`, then `coherence = 1.0 - normalized_distance`.

```c
static inline int genome_distance(CellGenome a, CellGenome b) {
    int d = 0;
    if (GENOME_RULE_SELECT(a) != GENOME_RULE_SELECT(b)) d++;
    if (GENOME_COUPLING_WEIGHT(a) != GENOME_COUPLING_WEIGHT(b)) d++;
    if (GENOME_MUTATION_RATE(a) != GENOME_MUTATION_RATE(b)) d++;
    return d;
}

double wave_grid_coherence(const WaveGrid *g, int x, int y) {
    int sz = g->size;
    int idx = y * sz + x;
    CellGenome me = g->cells[idx].genome;
    int total_dist = 0;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + sz) % sz;
            int ny = (y + dy + sz) % sz;
            int ni = ny * sz + nx;
            total_dist += genome_distance(me, g->cells[ni].genome);
            count++;
        }
    }
    /* max distance per neighbor = 3, so max total = 24 */
    return 1.0 - ((double)total_dist / 24.0);
}
```

This coherence value can be used by the engine to modulate how strongly the cell genome is expressed versus the neighborhood genome.

## Testing Strategy

Create `tests/test_wave_grid.c`:
1. Create a 16x16 grid, verify `cells` and `next_cells` are non-NULL.
2. `init_random` → verify ~20% alive, all waves within `WAVE_MASK`.
3. Set all neighbors to `genome = 0x1234`, call `recompute_nb_genomes(g, 0)` → verify `nb_genome` matches.
4. Set a checkerboard of two genomes, call `recompute_nb_genomes(g, 1)` → verify averages.
5. Set one neighbor with `fitness = 100.0`, rest at 0.0, call mode 2 → verify `nb_genome` copies the fit neighbor.
6. `coherence` test: identical neighborhood → `1.0`; maximally different (all three fields differ) → `0.0`.

## Makefile Update
```makefile
WAVE    = ca_core/wave_grid.o
CORE    = ca_core/grid.o ... ca_core/local_ga.o $(WAVE)
```

## Estimated Effort
- Implementation: 1 hour
- Tests: 30 min
- Debug / build fix: 15 min
