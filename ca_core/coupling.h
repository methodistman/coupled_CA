#ifndef COUPLING_H
#define COUPLING_H

#include "grid.h"

typedef enum {
    EDGE_TOP = 0,
    EDGE_RIGHT = 1,
    EDGE_BOTTOM = 2,
    EDGE_LEFT = 3
} Edge;

typedef enum {
    SCALE_WRAP = 0,      /* modulo wrap (original behavior) */
    SCALE_PROPORTIONAL,  /* map edge index proportionally */
    SCALE_BLOCK_MEAN,    /* average of 2x2 block (for 2:1 up) */
    SCALE_BLOCK_MAX      /* max of 2x2 block (for 2:1 up) */
} ScaleMode;

typedef struct {
    int target_grid;
    int target_edge;
    ScaleMode scale_mode;
    float scale;         /* target_size / source_size, or 0 for auto */
} EdgeConn;

typedef struct {
    EdgeConn conn[MAX_GRIDS][4];
    int num_grids;
} Coupling;

void coupling_init(Coupling *c, int num_grids);
void coupling_connect(Coupling *c, int src, Edge src_e, int dst, Edge dst_e);
void coupling_connect_scaled(Coupling *c, int src, Edge src_e, int dst, Edge dst_e, ScaleMode mode, float scale);
void coupling_disconnect(Coupling *c, int src, Edge src_e);
int coupling_read(const Coupling *c, Grid *const *grids, int g, int x, int y, int dx, int dy);

#endif
