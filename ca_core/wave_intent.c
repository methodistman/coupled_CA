#include "wave_intent.h"
#include "wave_engine.h"
#include <stdlib.h>
#include <string.h>

WaveIntentBuffer *wave_intent_buffer_create(int max_edges, int max_edge_size) {
    WaveIntentBuffer *ib = calloc(1, sizeof(WaveIntentBuffer));
    if (!ib) return NULL;
    ib->items = calloc(max_edges, sizeof(WaveEdgeIntent));
    if (!ib->items) { free(ib); return NULL; }
    for (int i = 0; i < max_edges; i++) {
        ib->items[i].cells = calloc(max_edge_size, sizeof(WaveCell));
        if (!ib->items[i].cells) {
            for (int j = 0; j < i; j++) free(ib->items[j].cells);
            free(ib->items);
            free(ib);
            return NULL;
        }
        ib->items[i].n_cells = max_edge_size;
    }
    ib->capacity = max_edges;
    ib->max_edge_size = max_edge_size;
    ib->count = 0;
    return ib;
}

void wave_intent_buffer_destroy(WaveIntentBuffer *ib) {
    if (!ib) return;
    for (int i = 0; i < ib->capacity; i++) {
        free(ib->items[i].cells);
    }
    free(ib->items);
    free(ib);
}

void wave_intent_buffer_clear(WaveIntentBuffer *ib) {
    if (ib) ib->count = 0;
}

int wave_intent_buffer_capture_edge(WaveIntentBuffer *ib,
                                     const WaveGrid *g,
                                     int src_grid, Edge src_edge,
                                     int dst_grid, Edge dst_edge) {
    if (!ib || !g || ib->count >= ib->capacity) return -1;
    int sz = g->size;
    int n_cells = sz;
    if (n_cells > ib->max_edge_size) n_cells = ib->max_edge_size;
    WaveEdgeIntent *ei = &ib->items[ib->count];
    ei->src_grid = src_grid;
    ei->src_edge = src_edge;
    ei->dst_grid = dst_grid;
    ei->dst_edge = dst_edge;
    ei->n_cells = n_cells;

    int x = 0, y = 0;
    for (int i = 0; i < n_cells; i++) {
        switch (src_edge) {
            case EDGE_TOP:    x = i; y = 0;        break;
            case EDGE_BOTTOM: x = i; y = sz - 1;  break;
            case EDGE_LEFT:   x = 0; y = i;        break;
            case EDGE_RIGHT:  x = sz - 1; y = i;   break;
        }
        ei->cells[i] = g->cells[y * sz + x];
    }
    ib->count++;
    return 0;
}

void wave_intent_buffer_capture_system(const struct WaveSystem *sys, WaveIntentBuffer *ib) {
    if (!sys || !ib) return;
    wave_intent_buffer_clear(ib);
    for (int g = 0; g < sys->num_grids; g++) {
        for (int e = 0; e < 4; e++) {
            const WaveEdgeConn *ec = &sys->coupling.conn[g][e];
            if (ec->target_grid >= 0) {
                wave_intent_buffer_capture_edge(ib, sys->grids[g],
                                                 g, (Edge)e,
                                                 ec->target_grid, (Edge)ec->target_edge);
            }
        }
    }
}

static void wave_apply_edge(const WaveEdgeIntent *intent, WaveCell *target, int sz, IntentMode mode, float alpha) {
    if (!intent || !target) return;
    int n = (intent->n_cells < sz) ? intent->n_cells : sz;
    for (int i = 0; i < n; i++) {
        int x = 0, y = 0;
        switch (intent->dst_edge) {
            case EDGE_TOP:    x = i; y = 0;        break;
            case EDGE_BOTTOM: x = i; y = sz - 1;  break;
            case EDGE_LEFT:   x = 0; y = i;        break;
            case EDGE_RIGHT:  x = sz - 1; y = i;   break;
        }
        int idx = y * sz + x;
        WaveCell val = intent->cells[i];
        switch (mode) {
            case INTENT_MODE_REPLACE:
                target[idx] = val;
                break;
            case INTENT_MODE_ADD:
                target[idx].alive = target[idx].alive || val.alive;
                if (val.wave > target[idx].wave)
                    target[idx].wave = val.wave;
                break;
            case INTENT_MODE_WEIGHTED: {
                float cur_alive = target[idx].alive ? 1.0f : 0.0f;
                float val_alive = val.alive ? 1.0f : 0.0f;
                target[idx].alive = (cur_alive * (1.0f - alpha) + val_alive * alpha) >= 0.5f ? 1 : 0;
                target[idx].wave = (uint32_t)(target[idx].wave * (1.0f - alpha) + val.wave * alpha) & WAVE_MASK;
                break;
            }
            case INTENT_MODE_THRESHOLD:
                if (val.alive) target[idx] = val;
                break;
        }
    }
}

void wave_intent_apply_to_next(const WaveEdgeIntent *intent, WaveGrid *g, IntentMode mode, float alpha) {
    if (!g) return;
    wave_apply_edge(intent, g->next_cells, g->size, mode, alpha);
}

void wave_intent_apply_to_cells(const WaveEdgeIntent *intent, WaveGrid *g, IntentMode mode, float alpha) {
    if (!g) return;
    wave_apply_edge(intent, g->cells, g->size, mode, alpha);
}

/* ---- Wave Intent Ring ---- */

WaveIntentRing *wave_intent_ring_create(int depth, int max_edges, int max_edge_size) {
    if (depth < 1) depth = 1;
    WaveIntentRing *ir = calloc(1, sizeof(WaveIntentRing));
    if (!ir) return NULL;
    ir->buffers = calloc(depth, sizeof(WaveIntentBuffer));
    if (!ir->buffers) { free(ir); return NULL; }
    ir->size = depth;
    ir->head = 0;
    for (int i = 0; i < depth; i++) {
        ir->buffers[i].items = calloc(max_edges, sizeof(WaveEdgeIntent));
        if (!ir->buffers[i].items) {
            for (int j = 0; j < i; j++) {
                for (int k = 0; k < max_edges; k++) free(ir->buffers[j].items[k].cells);
                free(ir->buffers[j].items);
            }
            free(ir->buffers);
            free(ir);
            return NULL;
        }
        for (int e = 0; e < max_edges; e++) {
            ir->buffers[i].items[e].cells = calloc(max_edge_size, sizeof(WaveCell));
            if (!ir->buffers[i].items[e].cells) {
                for (int ej = 0; ej < e; ej++) free(ir->buffers[i].items[ej].cells);
                for (int j = 0; j < i; j++) {
                    for (int k = 0; k < max_edges; k++) free(ir->buffers[j].items[k].cells);
                    free(ir->buffers[j].items);
                }
                free(ir->buffers[i].items);
                free(ir->buffers);
                free(ir);
                return NULL;
            }
            ir->buffers[i].items[e].n_cells = max_edge_size;
        }
        ir->buffers[i].capacity = max_edges;
        ir->buffers[i].max_edge_size = max_edge_size;
        ir->buffers[i].count = 0;
    }
    return ir;
}

void wave_intent_ring_destroy(WaveIntentRing *ir) {
    if (!ir) return;
    for (int i = 0; i < ir->size; i++) {
        for (int e = 0; e < ir->buffers[i].capacity; e++) {
            free(ir->buffers[i].items[e].cells);
        }
        free(ir->buffers[i].items);
    }
    free(ir->buffers);
    free(ir);
}

void wave_intent_ring_push(WaveIntentRing *ir, const struct WaveSystem *sys) {
    if (!ir || !sys) return;
    wave_intent_buffer_capture_system(sys, &ir->buffers[ir->head]);
    ir->head = (ir->head + 1) % ir->size;
}

const WaveIntentBuffer *wave_intent_ring_peek(const WaveIntentRing *ir, int delay) {
    if (!ir || delay < 0 || delay >= ir->size) return NULL;
    int idx = (ir->head - 1 - delay + ir->size) % ir->size;
    return &ir->buffers[idx];
}
