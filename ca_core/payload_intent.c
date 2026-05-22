#include "payload_intent.h"
#include "payload_engine.h"
#include <stdlib.h>
#include <string.h>

PayloadIntentBuffer *payload_intent_buffer_create(int max_edges, int max_edge_size) {
    PayloadIntentBuffer *ib = calloc(1, sizeof(PayloadIntentBuffer));
    if (!ib) return NULL;
    ib->items = calloc(max_edges, sizeof(PayloadEdgeIntent));
    if (!ib->items) { free(ib); return NULL; }
    for (int i = 0; i < max_edges; i++) {
        ib->items[i].cells = calloc(max_edge_size, sizeof(Cell));
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

void payload_intent_buffer_destroy(PayloadIntentBuffer *ib) {
    if (!ib) return;
    for (int i = 0; i < ib->capacity; i++) {
        free(ib->items[i].cells);
    }
    free(ib->items);
    free(ib);
}

void payload_intent_buffer_clear(PayloadIntentBuffer *ib) {
    if (ib) ib->count = 0;
}

int payload_intent_buffer_capture_edge(PayloadIntentBuffer *ib,
                                        const PayloadGrid *g,
                                        int src_grid, Edge src_edge,
                                        int dst_grid, Edge dst_edge) {
    if (!ib || !g || ib->count >= ib->capacity) return -1;
    int sz = g->size;
    int n_cells = sz;
    if (n_cells > ib->max_edge_size) n_cells = ib->max_edge_size;
    PayloadEdgeIntent *ei = &ib->items[ib->count];
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

void payload_intent_buffer_capture_system(const struct PayloadSystem *sys, PayloadIntentBuffer *ib) {
    if (!sys || !ib) return;
    payload_intent_buffer_clear(ib);
    for (int g = 0; g < sys->num_grids; g++) {
        for (int e = 0; e < 4; e++) {
            const PayloadEdgeConn *ec = &sys->coupling.conn[g][e];
            if (ec->target_grid >= 0) {
                payload_intent_buffer_capture_edge(ib, sys->grids[g],
                                                   g, (Edge)e,
                                                   ec->target_grid, (Edge)ec->target_edge);
            }
        }
    }
}

static void payload_apply_edge(const PayloadEdgeIntent *intent, Cell *target, int sz, IntentMode mode, float alpha) {
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
        Cell val = intent->cells[i];
        switch (mode) {
            case INTENT_MODE_REPLACE:
                target[idx] = val;
                break;
            case INTENT_MODE_ADD:
                target[idx].alive = target[idx].alive || val.alive;
                if (val.payload > target[idx].payload)
                    target[idx].payload = val.payload;
                break;
            case INTENT_MODE_WEIGHTED: {
                float cur_alive = target[idx].alive ? 1.0f : 0.0f;
                float val_alive = val.alive ? 1.0f : 0.0f;
                target[idx].alive = (cur_alive * (1.0f - alpha) + val_alive * alpha) >= 0.5f ? 1 : 0;
                target[idx].payload = (uint8_t)(target[idx].payload * (1.0f - alpha) + val.payload * alpha);
                break;
            }
            case INTENT_MODE_THRESHOLD:
                if (val.alive) target[idx] = val;
                break;
        }
    }
}

void payload_intent_apply_to_next(const PayloadEdgeIntent *intent, PayloadGrid *g, IntentMode mode, float alpha) {
    if (!g) return;
    payload_apply_edge(intent, g->next_cells, g->size, mode, alpha);
}

void payload_intent_apply_to_cells(const PayloadEdgeIntent *intent, PayloadGrid *g, IntentMode mode, float alpha) {
    if (!g) return;
    payload_apply_edge(intent, g->cells, g->size, mode, alpha);
}

/* ---- Payload Intent Ring ---- */

PayloadIntentRing *payload_intent_ring_create(int depth, int max_edges, int max_edge_size) {
    if (depth < 1) depth = 1;
    PayloadIntentRing *ir = calloc(1, sizeof(PayloadIntentRing));
    if (!ir) return NULL;
    ir->buffers = calloc(depth, sizeof(PayloadIntentBuffer));
    if (!ir->buffers) { free(ir); return NULL; }
    ir->size = depth;
    ir->head = 0;
    for (int i = 0; i < depth; i++) {
        ir->buffers[i].items = calloc(max_edges, sizeof(PayloadEdgeIntent));
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
            ir->buffers[i].items[e].cells = calloc(max_edge_size, sizeof(Cell));
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

void payload_intent_ring_destroy(PayloadIntentRing *ir) {
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

void payload_intent_ring_push(PayloadIntentRing *ir, const struct PayloadSystem *sys) {
    if (!ir || !sys) return;
    payload_intent_buffer_capture_system(sys, &ir->buffers[ir->head]);
    ir->head = (ir->head + 1) % ir->size;
}

const PayloadIntentBuffer *payload_intent_ring_peek(const PayloadIntentRing *ir, int delay) {
    if (!ir || delay < 0 || delay >= ir->size) return NULL;
    int idx = (ir->head - 1 - delay + ir->size) % ir->size;
    return &ir->buffers[idx];
}
