#ifndef HYBRID_ENGINE_H
#define HYBRID_ENGINE_H

#include "engine.h"
#include "payload_engine.h"
#include "wave_engine.h"
#include "coupling.h"
#include "payload_coupling.h"
#include "wave_coupling.h"

#define HYBRID_MAX_XCONNS 32

typedef enum { HYBRID_BINARY, HYBRID_PAYLOAD, HYBRID_WAVE } HybridGridType;

typedef struct {
    HybridGridType src_type;
    int src_grid;
    Edge src_edge;
    HybridGridType dst_type;
    int dst_grid;
    Edge dst_edge;
} HybridXConn;

typedef struct {
    System binary;
    PayloadSystem payload;
    WaveSystem wave;
    HybridXConn xconns[HYBRID_MAX_XCONNS];
    int n_xconns;
} HybridSystem;

void hybrid_sys_init(HybridSystem *s, int n_binary, int n_payload, int n_wave, int grid_size);
void hybrid_sys_destroy(HybridSystem *s);
void hybrid_sys_step(HybridSystem *s);
void hybrid_sys_randomize(HybridSystem *s, uint64_t (*rng)(void));

/* Cross-type coupling */
void hybrid_xconn_add(HybridSystem *s,
                      HybridGridType st, int sg, Edge se,
                      HybridGridType dt, int dg, Edge de);

/* Direct edge transfers */
void transfer_binary_to_payload(const Grid *src, PayloadGrid *dst,
                                Edge src_e, Edge dst_e);
void transfer_payload_to_binary(const PayloadGrid *src, Grid *dst,
                                Edge src_e, Edge dst_e);
void transfer_binary_to_wave(const Grid *src, WaveGrid *dst,
                              Edge src_e, Edge dst_e);
void transfer_wave_to_binary(const WaveGrid *src, Grid *dst,
                              Edge src_e, Edge dst_e);
void transfer_payload_to_wave(const PayloadGrid *src, WaveGrid *dst,
                               Edge src_e, Edge dst_e);
void transfer_wave_to_payload(const WaveGrid *src, PayloadGrid *dst,
                               Edge src_e, Edge dst_e);

/* Read a cell from any grid type, used for cross-type boundary reads */
Cell hybrid_coupling_read(const HybridSystem *s,
                          HybridGridType type, int grid_idx,
                          int x, int y, int dx, int dy);

#endif
