#include "hybrid_engine.h"
#include <string.h>

void hybrid_sys_init(HybridSystem *s, int n_binary, int n_payload, int n_wave, int grid_size) {
    memset(s, 0, sizeof(*s));
    sys_init(&s->binary, n_binary, grid_size);
    payload_sys_init(&s->payload, n_payload, grid_size);
    wave_sys_init(&s->wave, n_wave, grid_size);
}

void hybrid_sys_destroy(HybridSystem *s) {
    sys_destroy(&s->binary);
    payload_sys_destroy(&s->payload);
    wave_sys_destroy(&s->wave);
    s->n_xconns = 0;
}

void transfer_binary_to_payload(const Grid *src, PayloadGrid *dst,
                                Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        int val = 0;
        switch (src_e) {
        case EDGE_TOP:    val = src->cells[i]; break;
        case EDGE_BOTTOM: val = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   val = src->cells[i*sz]; break;
        case EDGE_RIGHT:  val = src->cells[i*sz + (sz-1)]; break;
        }
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i].alive = (uint8_t)val; dst->cells[i].payload = val ? 255 : 0; break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i].alive = (uint8_t)val; dst->cells[(sz-1)*sz + i].payload = val ? 255 : 0; break;
        case EDGE_LEFT:   dst->cells[i*sz].alive = (uint8_t)val; dst->cells[i*sz].payload = val ? 255 : 0; break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)].alive = (uint8_t)val; dst->cells[i*sz + (sz-1)].payload = val ? 255 : 0; break;
        }
    }
}

void transfer_payload_to_binary(const PayloadGrid *src, Grid *dst,
                                Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        Cell c = {0, 0};
        switch (src_e) {
        case EDGE_TOP:    c = src->cells[i]; break;
        case EDGE_BOTTOM: c = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   c = src->cells[i*sz]; break;
        case EDGE_RIGHT:  c = src->cells[i*sz + (sz-1)]; break;
        }
        int val = (c.payload > 127) ? 1 : 0;
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i] = (uint8_t)val; break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i] = (uint8_t)val; break;
        case EDGE_LEFT:   dst->cells[i*sz] = (uint8_t)val; break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)] = (uint8_t)val; break;
        }
    }
}

void transfer_binary_to_wave(const Grid *src, WaveGrid *dst,
                              Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        int val = 0;
        switch (src_e) {
        case EDGE_TOP:    val = src->cells[i]; break;
        case EDGE_BOTTOM: val = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   val = src->cells[i*sz]; break;
        case EDGE_RIGHT:  val = src->cells[i*sz + (sz-1)]; break;
        }
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i].alive = (uint8_t)val; dst->cells[i].wave = val ? WAVE_MASK : 0; break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i].alive = (uint8_t)val; dst->cells[(sz-1)*sz + i].wave = val ? WAVE_MASK : 0; break;
        case EDGE_LEFT:   dst->cells[i*sz].alive = (uint8_t)val; dst->cells[i*sz].wave = val ? WAVE_MASK : 0; break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)].alive = (uint8_t)val; dst->cells[i*sz + (sz-1)].wave = val ? WAVE_MASK : 0; break;
        }
    }
}

void transfer_wave_to_binary(const WaveGrid *src, Grid *dst,
                              Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        WaveCell c = {0, 0, 0, 0, 0.0};
        switch (src_e) {
        case EDGE_TOP:    c = src->cells[i]; break;
        case EDGE_BOTTOM: c = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   c = src->cells[i*sz]; break;
        case EDGE_RIGHT:  c = src->cells[i*sz + (sz-1)]; break;
        }
        int val = c.alive;
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i] = (uint8_t)val; break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i] = (uint8_t)val; break;
        case EDGE_LEFT:   dst->cells[i*sz] = (uint8_t)val; break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)] = (uint8_t)val; break;
        }
    }
}

void transfer_payload_to_wave(const PayloadGrid *src, WaveGrid *dst,
                               Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        Cell c = {0, 0};
        switch (src_e) {
        case EDGE_TOP:    c = src->cells[i]; break;
        case EDGE_BOTTOM: c = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   c = src->cells[i*sz]; break;
        case EDGE_RIGHT:  c = src->cells[i*sz + (sz-1)]; break;
        }
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i].alive = c.alive; dst->cells[i].wave = (uint32_t)c.payload; break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i].alive = c.alive; dst->cells[(sz-1)*sz + i].wave = (uint32_t)c.payload; break;
        case EDGE_LEFT:   dst->cells[i*sz].alive = c.alive; dst->cells[i*sz].wave = (uint32_t)c.payload; break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)].alive = c.alive; dst->cells[i*sz + (sz-1)].wave = (uint32_t)c.payload; break;
        }
    }
}

void transfer_wave_to_payload(const WaveGrid *src, PayloadGrid *dst,
                               Edge src_e, Edge dst_e) {
    int sz = src->size;
    for (int i = 0; i < sz; i++) {
        WaveCell c = {0, 0, 0, 0, 0.0};
        switch (src_e) {
        case EDGE_TOP:    c = src->cells[i]; break;
        case EDGE_BOTTOM: c = src->cells[(sz-1)*sz + i]; break;
        case EDGE_LEFT:   c = src->cells[i*sz]; break;
        case EDGE_RIGHT:  c = src->cells[i*sz + (sz-1)]; break;
        }
        switch (dst_e) {
        case EDGE_TOP:    dst->cells[i].alive = c.alive; dst->cells[i].payload = (uint8_t)(c.wave & 0xFF); break;
        case EDGE_BOTTOM: dst->cells[(sz-1)*sz + i].alive = c.alive; dst->cells[(sz-1)*sz + i].payload = (uint8_t)(c.wave & 0xFF); break;
        case EDGE_LEFT:   dst->cells[i*sz].alive = c.alive; dst->cells[i*sz].payload = (uint8_t)(c.wave & 0xFF); break;
        case EDGE_RIGHT:  dst->cells[i*sz + (sz-1)].alive = c.alive; dst->cells[i*sz + (sz-1)].payload = (uint8_t)(c.wave & 0xFF); break;
        }
    }
}

void hybrid_sys_step(HybridSystem *s) {
    for (int i = 0; i < s->n_xconns; i++) {
        const HybridXConn *xc = &s->xconns[i];
        if (xc->src_type == HYBRID_BINARY && xc->dst_type == HYBRID_PAYLOAD) {
            transfer_binary_to_payload(s->binary.grids[xc->src_grid],
                                       s->payload.grids[xc->dst_grid],
                                       xc->src_edge, xc->dst_edge);
        } else if (xc->src_type == HYBRID_PAYLOAD && xc->dst_type == HYBRID_BINARY) {
            transfer_payload_to_binary(s->payload.grids[xc->src_grid],
                                       s->binary.grids[xc->dst_grid],
                                       xc->src_edge, xc->dst_edge);
        } else if (xc->src_type == HYBRID_BINARY && xc->dst_type == HYBRID_WAVE) {
            transfer_binary_to_wave(s->binary.grids[xc->src_grid],
                                    s->wave.grids[xc->dst_grid],
                                    xc->src_edge, xc->dst_edge);
        } else if (xc->src_type == HYBRID_WAVE && xc->dst_type == HYBRID_BINARY) {
            transfer_wave_to_binary(s->wave.grids[xc->src_grid],
                                    s->binary.grids[xc->dst_grid],
                                    xc->src_edge, xc->dst_edge);
        } else if (xc->src_type == HYBRID_PAYLOAD && xc->dst_type == HYBRID_WAVE) {
            transfer_payload_to_wave(s->payload.grids[xc->src_grid],
                                     s->wave.grids[xc->dst_grid],
                                     xc->src_edge, xc->dst_edge);
        } else if (xc->src_type == HYBRID_WAVE && xc->dst_type == HYBRID_PAYLOAD) {
            transfer_wave_to_payload(s->wave.grids[xc->src_grid],
                                     s->payload.grids[xc->dst_grid],
                                     xc->src_edge, xc->dst_edge);
        }
    }
    sys_step(&s->binary);
    payload_sys_step(&s->payload);
    wave_sys_step(&s->wave);
}

void hybrid_sys_randomize(HybridSystem *s, uint64_t (*rng)(void)) {
    sys_randomize(&s->binary, rng);
    payload_sys_randomize(&s->payload, rng);
    wave_sys_randomize(&s->wave, rng);
}

void hybrid_xconn_add(HybridSystem *s,
                      HybridGridType st, int sg, Edge se,
                      HybridGridType dt, int dg, Edge de) {
    if (s->n_xconns >= HYBRID_MAX_XCONNS) return;
    HybridXConn *xc = &s->xconns[s->n_xconns++];
    xc->src_type = st; xc->src_grid = sg; xc->src_edge = se;
    xc->dst_type = dt; xc->dst_grid = dg; xc->dst_edge = de;
}

Cell hybrid_coupling_read(const HybridSystem *s,
                          HybridGridType type, int grid_idx,
                          int x, int y, int dx, int dy) {
    int nx = x + dx, ny = y + dy;

    if (type == HYBRID_BINARY) {
        const Grid *grid = s->binary.grids[grid_idx];
        int sz = grid->size;
        if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
            int v = grid->cells[ny * sz + nx];
            return (Cell){(uint8_t)v, v ? 255 : 0};
        }
        /* Edge crossing — find matching xconn */
        Edge edge;
        int idx;
        if (nx < 0)       { edge = EDGE_LEFT;   idx = ny; }
        else if (nx >= sz){ edge = EDGE_RIGHT;  idx = ny; }
        else if (ny < 0)  { edge = EDGE_TOP;    idx = nx; }
        else              { edge = EDGE_BOTTOM; idx = nx; }
        idx = (idx % sz + sz) % sz;
        for (int i = 0; i < s->n_xconns; i++) {
            const HybridXConn *xc = &s->xconns[i];
            if (xc->src_type == HYBRID_BINARY && xc->src_grid == grid_idx && xc->src_edge == edge) {
                const PayloadGrid *pg = s->payload.grids[xc->dst_grid];
                int psz = pg->size;
                int tx = 0, ty = 0;
                switch (xc->dst_edge) {
                case EDGE_TOP:    tx = idx; ty = 0;        break;
                case EDGE_BOTTOM: tx = idx; ty = psz - 1;  break;
                case EDGE_LEFT:   tx = 0;   ty = idx;      break;
                case EDGE_RIGHT:  tx = psz - 1; ty = idx;   break;
                }
                return pg->cells[ty * psz + tx];
            }
        }
    } else if (type == HYBRID_PAYLOAD) {
        const PayloadGrid *grid = s->payload.grids[grid_idx];
        int sz = grid->size;
        if (nx >= 0 && nx < sz && ny >= 0 && ny < sz)
            return grid->cells[ny * sz + nx];
        Edge edge;
        int idx;
        if (nx < 0)       { edge = EDGE_LEFT;   idx = ny; }
        else if (nx >= sz){ edge = EDGE_RIGHT;  idx = ny; }
        else if (ny < 0)  { edge = EDGE_TOP;    idx = nx; }
        else              { edge = EDGE_BOTTOM; idx = nx; }
        idx = (idx % sz + sz) % sz;
        for (int i = 0; i < s->n_xconns; i++) {
            const HybridXConn *xc = &s->xconns[i];
            if (xc->src_type == HYBRID_PAYLOAD && xc->src_grid == grid_idx && xc->src_edge == edge) {
                const Grid *bg = s->binary.grids[xc->dst_grid];
                int bsz = bg->size;
                int tx = 0, ty = 0;
                switch (xc->dst_edge) {
                case EDGE_TOP:    tx = idx; ty = 0;        break;
                case EDGE_BOTTOM: tx = idx; ty = bsz - 1;  break;
                case EDGE_LEFT:   tx = 0;   ty = idx;      break;
                case EDGE_RIGHT:  tx = bsz - 1; ty = idx;   break;
                }
                int v = bg->cells[ty * bsz + tx];
                return (Cell){(uint8_t)v, v ? 255 : 0};
            }
        }
    } else if (type == HYBRID_WAVE) {
        /* Wave-source neighbor reads. WaveCells are projected down to Cell
           via {alive, low 8 bits of wave} — the same semantics as
           transfer_wave_to_payload. Cross-edges to binary become
           {alive, alive ? 255 : 0}; cross-edges to payload return the
           target payload cell directly; cross-edges to wave project the
           target's WaveCell. */
        const WaveGrid *grid = s->wave.grids[grid_idx];
        int sz = grid->size;
        if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
            WaveCell wc = grid->cells[ny * sz + nx];
            return (Cell){wc.alive, (uint8_t)(wc.wave & 0xFFu)};
        }
        Edge edge;
        int idx;
        if (nx < 0)       { edge = EDGE_LEFT;   idx = ny; }
        else if (nx >= sz){ edge = EDGE_RIGHT;  idx = ny; }
        else if (ny < 0)  { edge = EDGE_TOP;    idx = nx; }
        else              { edge = EDGE_BOTTOM; idx = nx; }
        idx = (idx % sz + sz) % sz;
        for (int i = 0; i < s->n_xconns; i++) {
            const HybridXConn *xc = &s->xconns[i];
            if (xc->src_type != HYBRID_WAVE || xc->src_grid != grid_idx || xc->src_edge != edge) continue;
            int tx = 0, ty = 0;
            int tsz;
            if (xc->dst_type == HYBRID_BINARY) {
                const Grid *bg = s->binary.grids[xc->dst_grid];
                tsz = bg->size;
                switch (xc->dst_edge) {
                case EDGE_TOP:    tx = idx; ty = 0;        break;
                case EDGE_BOTTOM: tx = idx; ty = tsz - 1;  break;
                case EDGE_LEFT:   tx = 0;   ty = idx;      break;
                case EDGE_RIGHT:  tx = tsz - 1; ty = idx;   break;
                }
                int v = bg->cells[ty * tsz + tx];
                return (Cell){(uint8_t)v, v ? 255 : 0};
            } else if (xc->dst_type == HYBRID_PAYLOAD) {
                const PayloadGrid *pg = s->payload.grids[xc->dst_grid];
                tsz = pg->size;
                switch (xc->dst_edge) {
                case EDGE_TOP:    tx = idx; ty = 0;        break;
                case EDGE_BOTTOM: tx = idx; ty = tsz - 1;  break;
                case EDGE_LEFT:   tx = 0;   ty = idx;      break;
                case EDGE_RIGHT:  tx = tsz - 1; ty = idx;   break;
                }
                return pg->cells[ty * tsz + tx];
            } else if (xc->dst_type == HYBRID_WAVE) {
                const WaveGrid *wg = s->wave.grids[xc->dst_grid];
                tsz = wg->size;
                switch (xc->dst_edge) {
                case EDGE_TOP:    tx = idx; ty = 0;        break;
                case EDGE_BOTTOM: tx = idx; ty = tsz - 1;  break;
                case EDGE_LEFT:   tx = 0;   ty = idx;      break;
                case EDGE_RIGHT:  tx = tsz - 1; ty = idx;   break;
                }
                WaveCell wc = wg->cells[ty * tsz + tx];
                return (Cell){wc.alive, (uint8_t)(wc.wave & 0xFFu)};
            }
        }
    }
    return (Cell){0, 0};
}
