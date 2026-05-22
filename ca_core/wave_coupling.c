#include "wave_coupling.h"
#include <string.h>

void wave_coupling_init(WaveCoupling *c, int num_grids) {
    memset(c, 0, sizeof(*c));
    c->num_grids = num_grids;
    for (int g = 0; g < num_grids; g++) {
        for (int e = 0; e < 4; e++) {
            c->conn[g][e].target_grid = -1;
            c->conn[g][e].target_edge = -1;
            c->conn[g][e].strength = 0.0f;
        }
    }
}

void wave_coupling_connect(WaveCoupling *c, int src, Edge src_e, int dst, Edge dst_e, float strength) {
    if (src < 0 || src >= c->num_grids) return;
    if (src_e < 0 || src_e >= 4) return;
    c->conn[src][src_e].target_grid = dst;
    c->conn[src][src_e].target_edge = dst_e;
    c->conn[src][src_e].strength = strength;
}

void wave_coupling_disconnect(WaveCoupling *c, int src, Edge src_e) {
    if (src < 0 || src >= c->num_grids) return;
    if (src_e < 0 || src_e >= 4) return;
    c->conn[src][src_e].target_grid = -1;
    c->conn[src][src_e].target_edge = -1;
    c->conn[src][src_e].strength = 0.0f;
}

WaveCell wave_coupling_read(const WaveCoupling *c, WaveGrid *const *grids, int g, int x, int y, int dx, int dy) {
    WaveGrid *grid = grids[g];
    int sz = grid->size;
    int nx = x + dx;
    int ny = y + dy;

    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
        return grid->cells[ny * sz + nx];
    }

    /* Edge crossing */
    Edge edge;
    int idx;
    if (nx < 0)       { edge = EDGE_LEFT;   idx = ny; }
    else if (nx >= sz){ edge = EDGE_RIGHT;  idx = ny; }
    else if (ny < 0)  { edge = EDGE_TOP;    idx = nx; }
    else              { edge = EDGE_BOTTOM; idx = nx; }
    idx = (idx % sz + sz) % sz;

    const WaveEdgeConn *ec = &c->conn[g][edge];
    if (ec->target_grid >= 0) {
        WaveGrid *tg = grids[ec->target_grid];
        int tsz = tg->size;
        int tx = 0, ty = 0;
        switch (ec->target_edge) {
            case EDGE_TOP:    tx = idx; ty = 0;        break;
            case EDGE_BOTTOM: tx = idx; ty = tsz - 1;  break;
            case EDGE_LEFT:   tx = 0;   ty = idx;      break;
            case EDGE_RIGHT:  tx = tsz - 1; ty = idx;   break;
        }
        WaveCell tc = tg->cells[ty * tsz + tx];
        /* Blend alive and wave based on strength */
        float s = ec->strength;
        float t = 1.0f - s;
        WaveCell me = grid->cells[y * sz + x];
        WaveCell out = me;
        out.alive = (t * (me.alive ? 1.0f : 0.0f) + s * (tc.alive ? 1.0f : 0.0f)) >= 0.5f ? 1 : 0;
        out.wave = (uint32_t)(t * (float)me.wave + s * (float)tc.wave) & WAVE_MASK;
        return out;
    }

    /* No connection: wrap toroidally within same grid */
    nx = (nx % sz + sz) % sz;
    ny = (ny % sz + sz) % sz;
    return grid->cells[ny * sz + nx];
}
