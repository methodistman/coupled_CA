#include "payload_coupling.h"

void payload_coupling_init(PayloadCoupling *c, int num_grids) {
    c->num_grids = num_grids;
    for (int g = 0; g < MAX_PAYLOAD_GRIDS; g++)
        for (int e = 0; e < 4; e++)
            c->conn[g][e].target_grid = -1;
}

void payload_coupling_connect(PayloadCoupling *c, int src, Edge src_e, int dst, Edge dst_e, float strength) {
    if (src < 0 || src >= c->num_grids) return;
    c->conn[src][src_e].target_grid = dst;
    c->conn[src][src_e].target_edge = dst_e;
    c->conn[src][src_e].strength = strength;
}

void payload_coupling_disconnect(PayloadCoupling *c, int src, Edge src_e) {
    if (src < 0 || src >= c->num_grids) return;
    c->conn[src][src_e].target_grid = -1;
}

Cell payload_coupling_read(const PayloadCoupling *c, PayloadGrid *const *grids, int g, int x, int y, int dx, int dy) {
    const PayloadGrid *grid = grids[g];
    int sz = grid->size;
    int nx = x + dx, ny = y + dy;

    if ((nx < 0 || nx >= sz) && (ny < 0 || ny >= sz)) {
        Cell zero = {0, 0};
        return zero;
    }

    if (nx < 0 || nx >= sz) {
        int edge = (nx < 0) ? EDGE_LEFT : EDGE_RIGHT;
        const PayloadEdgeConn *ec = &c->conn[g][edge];
        if (ec->target_grid < 0) { Cell zero = {0, 0}; return zero; }
        const PayloadGrid *tg = grids[ec->target_grid];
        if (!tg || !tg->active) { Cell zero = {0, 0}; return zero; }
        int ty = (ny % tg->size + tg->size) % tg->size;
        int tx = (ec->target_edge == EDGE_RIGHT) ? tg->size - 1 : 0;
        Cell c = tg->cells[ty * tg->size + tx];
        c.payload = (uint8_t)(c.payload * ec->strength);
        return c;
    }

    if (ny < 0 || ny >= sz) {
        int edge = (ny < 0) ? EDGE_TOP : EDGE_BOTTOM;
        const PayloadEdgeConn *ec = &c->conn[g][edge];
        if (ec->target_grid < 0) { Cell zero = {0, 0}; return zero; }
        const PayloadGrid *tg = grids[ec->target_grid];
        if (!tg || !tg->active) { Cell zero = {0, 0}; return zero; }
        int tx = (nx % tg->size + tg->size) % tg->size;
        int ty = (ec->target_edge == EDGE_BOTTOM) ? tg->size - 1 : 0;
        Cell c = tg->cells[ty * tg->size + tx];
        c.payload = (uint8_t)(c.payload * ec->strength);
        return c;
    }

    return grid->cells[ny * sz + nx];
}
