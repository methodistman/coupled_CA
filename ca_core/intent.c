#include "intent.h"
#include "engine.h"
#include <stdlib.h>
#include <string.h>

IntentBuffer *intent_buffer_create(int max_edges, int max_edge_size) {
    IntentBuffer *ib = calloc(1, sizeof(IntentBuffer));
    if (!ib) return NULL;
    ib->items = calloc(max_edges, sizeof(EdgeIntent));
    if (!ib->items) { free(ib); return NULL; }
    for (int i = 0; i < max_edges; i++) {
        ib->items[i].cells = calloc(max_edge_size, sizeof(uint8_t));
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

void intent_buffer_destroy(IntentBuffer *ib) {
    if (!ib) return;
    for (int i = 0; i < ib->capacity; i++) {
        free(ib->items[i].cells);
    }
    free(ib->items);
    free(ib);
}

void intent_buffer_clear(IntentBuffer *ib) {
    if (ib) ib->count = 0;
}

int intent_buffer_capture_edge(IntentBuffer *ib,
                                const Grid *g,
                                int src_grid, Edge src_edge,
                                int dst_grid, Edge dst_edge) {
    if (!ib || !g || ib->count >= ib->capacity) return -1;
    int sz = g->size;
    int n_cells = sz;
    if (n_cells > ib->max_edge_size) n_cells = ib->max_edge_size;
    EdgeIntent *ei = &ib->items[ib->count];
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

void intent_buffer_capture_system(const struct System *sys, IntentBuffer *ib) {
    if (!sys || !ib) return;
    intent_buffer_clear(ib);
    for (int g = 0; g < sys->num_grids; g++) {
        for (int e = 0; e < 4; e++) {
            const EdgeConn *ec = &sys->coupling.conn[g][e];
            if (ec->target_grid >= 0) {
                intent_buffer_capture_edge(ib, sys->grids[g],
                                           g, (Edge)e,
                                           ec->target_grid, (Edge)ec->target_edge);
            }
        }
    }
}

static void apply_edge(const EdgeIntent *intent, uint8_t *target, int sz, IntentMode mode, float alpha) {
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
        uint8_t raw = intent->cells[i];
        uint8_t val = raw ? 1 : 0;
        switch (mode) {
            case INTENT_MODE_REPLACE:
                target[idx] = val;
                break;
            case INTENT_MODE_ADD:
                target[idx] = (target[idx] || val) ? 1 : 0;
                break;
            case INTENT_MODE_WEIGHTED: {
                float cur = target[idx] ? 1.0f : 0.0f;
                float nv = cur * (1.0f - alpha) + val * alpha;
                target[idx] = (nv >= 0.5f) ? 1 : 0;
                break;
            }
            case INTENT_MODE_THRESHOLD:
                target[idx] = (raw > 0 && (float)raw >= alpha) ? 1 : 0;
                break;
        }
    }
}

void intent_apply_to_next(const EdgeIntent *intent, Grid *g, IntentMode mode, float alpha) {
    if (!g) return;
    apply_edge(intent, g->next_cells, g->size, mode, alpha);
}

void intent_apply_to_cells(const EdgeIntent *intent, Grid *g, IntentMode mode, float alpha) {
    if (!g) return;
    apply_edge(intent, g->cells, g->size, mode, alpha);
}

/* ---- Intent Ring ---- */

IntentRing *intent_ring_create(int depth, int max_edges, int max_edge_size) {
    if (depth < 1) depth = 1;
    IntentRing *ir = calloc(1, sizeof(IntentRing));
    if (!ir) return NULL;
    ir->buffers = calloc(depth, sizeof(IntentBuffer));
    if (!ir->buffers) { free(ir); return NULL; }
    ir->size = depth;
    ir->head = 0;
    for (int i = 0; i < depth; i++) {
        ir->buffers[i].items = calloc(max_edges, sizeof(EdgeIntent));
        if (!ir->buffers[i].items) {
            for (int j = 0; j < i; j++) free(ir->buffers[j].items);
            free(ir->buffers);
            free(ir);
            return NULL;
        }
        for (int e = 0; e < max_edges; e++) {
            ir->buffers[i].items[e].cells = calloc(max_edge_size, sizeof(uint8_t));
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

void intent_ring_destroy(IntentRing *ir) {
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

void intent_ring_push(IntentRing *ir, const struct System *sys) {
    if (!ir || !sys) return;
    intent_buffer_capture_system(sys, &ir->buffers[ir->head]);
    ir->head = (ir->head + 1) % ir->size;
}

const IntentBuffer *intent_ring_peek(const IntentRing *ir, int delay) {
    if (!ir || delay < 0 || delay >= ir->size) return NULL;
    int idx = (ir->head - 1 - delay + ir->size) % ir->size;
    return &ir->buffers[idx];
}
