#ifndef PAYLOAD_INTENT_H
#define PAYLOAD_INTENT_H

#include <stdint.h>
#include "payload_grid.h"
#include "coupling.h"
#include "intent.h"   /* for IntentMode */

struct PayloadSystem;  /* forward declaration from payload_engine.h */

typedef struct {
    int src_grid;
    Edge src_edge;
    int dst_grid;
    Edge dst_edge;
    Cell *cells;     /* flattened edge Cells */
    int n_cells;
} PayloadEdgeIntent;

typedef struct {
    PayloadEdgeIntent *items;
    int count;
    int capacity;
    int max_edge_size;
} PayloadIntentBuffer;

/* Create intent buffer for up to max_edges, each storing max_edge_size Cells */
PayloadIntentBuffer *payload_intent_buffer_create(int max_edges, int max_edge_size);
void payload_intent_buffer_destroy(PayloadIntentBuffer *ib);
void payload_intent_buffer_clear(PayloadIntentBuffer *ib);

/* Capture one edge from a payload grid into the buffer. Returns 0 on success, -1 if full. */
int payload_intent_buffer_capture_edge(PayloadIntentBuffer *ib,
                                         const PayloadGrid *g,
                                         int src_grid, Edge src_edge,
                                         int dst_grid, Edge dst_edge);

/* Capture all connected edges from a payload system into the buffer. */
void payload_intent_buffer_capture_system(const struct PayloadSystem *sys, PayloadIntentBuffer *ib);

/* Apply a captured intent to a payload grid's next_cells edge. */
void payload_intent_apply_to_next(const PayloadEdgeIntent *intent, PayloadGrid *g, IntentMode mode, float alpha);

/* Apply a captured intent to a payload grid's cells edge (for delayed coupling). */
void payload_intent_apply_to_cells(const PayloadEdgeIntent *intent, PayloadGrid *g, IntentMode mode, float alpha);

/* ---- Payload Intent Ring (history for delayed coupling / TE) ---- */

typedef struct {
    PayloadIntentBuffer *buffers;   /* one PayloadIntentBuffer per tick slot */
    int size;                       /* ring depth */
    int head;                       /* next write position */
} PayloadIntentRing;

PayloadIntentRing *payload_intent_ring_create(int depth, int max_edges, int max_edge_size);
void payload_intent_ring_destroy(PayloadIntentRing *ir);

/* Capture current payload system state into ring[head], then advance head. */
void payload_intent_ring_push(PayloadIntentRing *ir, const struct PayloadSystem *sys);

/* Return a read-only view of the buffer at delay ticks ago (0 = current). */
const PayloadIntentBuffer *payload_intent_ring_peek(const PayloadIntentRing *ir, int delay);

#endif
