#include "coupling.h"

void coupling_init(Coupling *c, int num_grids) {
    c->num_grids = num_grids;
    for (int g = 0; g < MAX_GRIDS; g++)
        for (int e = 0; e < 4; e++) {
            c->conn[g][e].target_grid = -1;
            c->conn[g][e].scale_mode = SCALE_WRAP;
            c->conn[g][e].scale = 1.0f;
        }
}

void coupling_connect(Coupling *c, int src, Edge src_e, int dst, Edge dst_e) {
    if (src < 0 || src >= c->num_grids) return;
    c->conn[src][src_e].target_grid = dst;
    c->conn[src][src_e].target_edge = dst_e;
    c->conn[src][src_e].scale_mode = SCALE_WRAP;
    c->conn[src][src_e].scale = 1.0f;
}

void coupling_connect_scaled(Coupling *c, int src, Edge src_e, int dst, Edge dst_e, ScaleMode mode, float scale) {
    if (src < 0 || src >= c->num_grids) return;
    c->conn[src][src_e].target_grid = dst;
    c->conn[src][src_e].target_edge = dst_e;
    c->conn[src][src_e].scale_mode = mode;
    c->conn[src][src_e].scale = scale;
}

void coupling_disconnect(Coupling *c, int src, Edge src_e) {
    if (src < 0 || src >= c->num_grids) return;
    c->conn[src][src_e].target_grid = -1;
}

/* Map source edge index to target edge index based on scale_mode */
static int map_idx(int idx, int src_sz, int tgt_sz, ScaleMode mode) {
    if (mode == SCALE_WRAP || src_sz == tgt_sz)
        return (idx % tgt_sz + tgt_sz) % tgt_sz;
    if (mode == SCALE_PROPORTIONAL)
        return (int)((double)idx / src_sz * tgt_sz);
    if (mode == SCALE_BLOCK_MEAN || mode == SCALE_BLOCK_MAX) {
        /* For upsampling: each target cell covers (tgt_sz/src_sz) source cells */
        /* Simplified: assume power-of-2 ratio */
        int ratio = tgt_sz / src_sz;
        if (ratio <= 0) ratio = 1;
        return idx * ratio;
    }
    return (idx % tgt_sz + tgt_sz) % tgt_sz;
}

static int read_edge_cell(const Grid *tg, int edge, int idx, ScaleMode mode) {
    int tx = 0, ty = 0;
    int sz = tg->size;
    switch (edge) {
    case EDGE_TOP:    tx = idx; ty = 0;        break;
    case EDGE_BOTTOM: tx = idx; ty = sz - 1;  break;
    case EDGE_LEFT:   tx = 0;   ty = idx;      break;
    case EDGE_RIGHT:  tx = sz - 1; ty = idx;   break;
    }
    if (mode == SCALE_BLOCK_MEAN) {
        /* Average 2x2 block for upsampling (assumes 2:1 or similar) */
        int sum = 0, cnt = 0;
        for (int dy = 0; dy < 2 && ty + dy < sz; dy++)
            for (int dx = 0; dx < 2 && tx + dx < sz; dx++) {
                sum += tg->cells[(ty + dy) * sz + (tx + dx)];
                cnt++;
            }
        return (cnt > 0) ? (sum * 2 / cnt / 2) : 0; /* round to 0/1 */
    }
    if (mode == SCALE_BLOCK_MAX) {
        int max = 0;
        for (int dy = 0; dy < 2 && ty + dy < sz; dy++)
            for (int dx = 0; dx < 2 && tx + dx < sz; dx++)
                if (tg->cells[(ty + dy) * sz + (tx + dx)] > max)
                    max = tg->cells[(ty + dy) * sz + (tx + dx)];
        return (max > 0) ? 1 : 0;
    }
    return tg->cells[ty * sz + tx];
}

int coupling_read(const Coupling *c, Grid *const *grids, int g, int x, int y, int dx, int dy) {
    const Grid *grid = grids[g];
    int sz = grid->size;
    int nx = x + dx, ny = y + dy;

    if ((nx < 0 || nx >= sz) && (ny < 0 || ny >= sz)) return 0;

    if (nx < 0 || nx >= sz) {
        int edge = (nx < 0) ? EDGE_LEFT : EDGE_RIGHT;
        const EdgeConn *ec = &c->conn[g][edge];
        if (ec->target_grid < 0) return 0;
        const Grid *tg = grids[ec->target_grid];
        if (!tg || !tg->active) return 0;
        int idx = (ny % sz + sz) % sz;
        int tidx = map_idx(idx, sz, tg->size, ec->scale_mode);
        return read_edge_cell(tg, ec->target_edge, tidx, ec->scale_mode);
    }

    if (ny < 0 || ny >= sz) {
        int edge = (ny < 0) ? EDGE_TOP : EDGE_BOTTOM;
        const EdgeConn *ec = &c->conn[g][edge];
        if (ec->target_grid < 0) return 0;
        const Grid *tg = grids[ec->target_grid];
        if (!tg || !tg->active) return 0;
        int idx = (nx % sz + sz) % sz;
        int tidx = map_idx(idx, sz, tg->size, ec->scale_mode);
        return read_edge_cell(tg, ec->target_edge, tidx, ec->scale_mode);
    }

    return grid->cells[ny * sz + nx];
}
