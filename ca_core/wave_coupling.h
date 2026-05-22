#ifndef WAVE_COUPLING_H
#define WAVE_COUPLING_H

#include "wave_grid.h"
#include "coupling.h"

typedef struct {
    int target_grid;
    int target_edge;
    float strength;        /* 0.0–1.0: how much wave/nb_genome transfers */
} WaveEdgeConn;

typedef struct {
    WaveEdgeConn conn[MAX_WAVE_GRIDS][4];
    int num_grids;
} WaveCoupling;

void wave_coupling_init(WaveCoupling *c, int num_grids);
void wave_coupling_connect(WaveCoupling *c, int src, Edge src_e, int dst, Edge dst_e, float strength);
void wave_coupling_disconnect(WaveCoupling *c, int src, Edge src_e);

/* Read neighbor with toroidal wrap and edge coupling. Returns a full WaveCell. */
WaveCell wave_coupling_read(const WaveCoupling *c, WaveGrid *const *grids, int g, int x, int y, int dx, int dy);

#endif
