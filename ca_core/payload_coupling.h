#ifndef PAYLOAD_COUPLING_H
#define PAYLOAD_COUPLING_H

#include "payload_grid.h"
#include "coupling.h"

typedef struct {
    int target_grid;
    int target_edge;
    float strength; /* 0.0–1.0: how much payload transfers */
} PayloadEdgeConn;

typedef struct {
    PayloadEdgeConn conn[MAX_PAYLOAD_GRIDS][4];
    int num_grids;
} PayloadCoupling;

void payload_coupling_init(PayloadCoupling *c, int num_grids);
void payload_coupling_connect(PayloadCoupling *c, int src, Edge src_e, int dst, Edge dst_e, float strength);
void payload_coupling_disconnect(PayloadCoupling *c, int src, Edge src_e);
Cell payload_coupling_read(const PayloadCoupling *c, PayloadGrid *const *grids, int g, int x, int y, int dx, int dy);

#endif
