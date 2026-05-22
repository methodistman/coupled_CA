#ifndef INTENT_H
#define INTENT_H

#include <stdint.h>
#include "grid.h"
#include "coupling.h"

struct System;  /* forward declaration from engine.h */

typedef enum {
    INTENT_MODE_REPLACE = 0,     /* overwrite destination edge */
    INTENT_MODE_ADD = 1,         /* cell-wise OR */
    INTENT_MODE_WEIGHTED = 2,    /* weighted blend (requires alpha) */
    INTENT_MODE_THRESHOLD = 3,   /* gate: apply only if intent > threshold */
} IntentMode;

typedef struct {
    int src_grid;
    Edge src_edge;
    int dst_grid;
    Edge dst_edge;
    uint8_t *cells;     /* flattened edge cells */
    int n_cells;
} EdgeIntent;

typedef struct {
    EdgeIntent *items;
    int count;
    int capacity;
    int max_edge_size;
} IntentBuffer;

/* Create intent buffer for up to max_edges, each storing max_edge_size cells */
IntentBuffer *intent_buffer_create(int max_edges, int max_edge_size);
void intent_buffer_destroy(IntentBuffer *ib);
void intent_buffer_clear(IntentBuffer *ib);

/* Capture one edge from a grid into the buffer. Returns 0 on success, -1 if full. */
int intent_buffer_capture_edge(IntentBuffer *ib,
                                const Grid *g,
                                int src_grid, Edge src_edge,
                                int dst_grid, Edge dst_edge);

/* Capture all connected edges from a system into the buffer. */
void intent_buffer_capture_system(const struct System *sys, IntentBuffer *ib);

/* Apply a captured intent to a grid's next_cells edge. */
void intent_apply_to_next(const EdgeIntent *intent, Grid *g, IntentMode mode, float alpha);

/* Apply a captured intent to a grid's cells edge (for delayed coupling). */
void intent_apply_to_cells(const EdgeIntent *intent, Grid *g, IntentMode mode, float alpha);

/* ---- Intent Ring (history for delayed coupling / TE) ---- */

typedef struct {
    IntentBuffer *buffers;   /* one IntentBuffer per tick slot */
    int size;                /* ring depth */
    int head;                /* next write position */
} IntentRing;

IntentRing *intent_ring_create(int depth, int max_edges, int max_edge_size);
void intent_ring_destroy(IntentRing *ir);

/* Capture current system state into ring[head], then advance head. */
void intent_ring_push(IntentRing *ir, const struct System *sys);

/* Return a read-only view of the buffer at delay ticks ago (0 = current). */
const IntentBuffer *intent_ring_peek(const IntentRing *ir, int delay);

#endif
