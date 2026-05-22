#ifndef WAVE_INTENT_H
#define WAVE_INTENT_H

#include <stdint.h>
#include "wave_grid.h"
#include "coupling.h"
#include "intent.h"

struct WaveSystem;

typedef struct {
    int src_grid;
    Edge src_edge;
    int dst_grid;
    Edge dst_edge;
    WaveCell *cells;
    int n_cells;
} WaveEdgeIntent;

typedef struct {
    WaveEdgeIntent *items;
    int count;
    int capacity;
    int max_edge_size;
} WaveIntentBuffer;

WaveIntentBuffer *wave_intent_buffer_create(int max_edges, int max_edge_size);
void wave_intent_buffer_destroy(WaveIntentBuffer *ib);
void wave_intent_buffer_clear(WaveIntentBuffer *ib);

int wave_intent_buffer_capture_edge(WaveIntentBuffer *ib,
                                     const WaveGrid *g,
                                     int src_grid, Edge src_edge,
                                     int dst_grid, Edge dst_edge);

void wave_intent_buffer_capture_system(const struct WaveSystem *sys, WaveIntentBuffer *ib);

void wave_intent_apply_to_next(const WaveEdgeIntent *intent, WaveGrid *g, IntentMode mode, float alpha);
void wave_intent_apply_to_cells(const WaveEdgeIntent *intent, WaveGrid *g, IntentMode mode, float alpha);

typedef struct {
    WaveIntentBuffer *buffers;
    int size;
    int head;
} WaveIntentRing;

WaveIntentRing *wave_intent_ring_create(int depth, int max_edges, int max_edge_size);
void wave_intent_ring_destroy(WaveIntentRing *ir);

void wave_intent_ring_push(WaveIntentRing *ir, const struct WaveSystem *sys);
const WaveIntentBuffer *wave_intent_ring_peek(const WaveIntentRing *ir, int delay);

#endif
