#include "transfer_entropy.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

double te_compute_binary(const uint8_t *y, const uint8_t *x, int n) {
    if (n < 3 || !y || !x) return 0.0;

    /* Count transitions: y_t, y_{t-1}, x_{t-1} */
    int joint[2][2][2] = {{{0}}};
    int yt_ytm1[2][2] = {{0}};
    int ytm1[2] = {0};

    for (int t = 1; t < n; t++) {
        int yt  = y[t] & 1;
        int ytm = y[t - 1] & 1;
        int xtm = x[t - 1] & 1;
        joint[yt][ytm][xtm]++;
        yt_ytm1[yt][ytm]++;
        ytm1[ytm]++;
    }

    int total = n - 1;
    double te = 0.0;

    for (int yt = 0; yt < 2; yt++) {
        for (int ytm = 0; ytm < 2; ytm++) {
            int denom_marg = ytm1[ytm];
            if (denom_marg == 0) continue;
            double p_cond_marg = (double)yt_ytm1[yt][ytm] / denom_marg;
            if (p_cond_marg <= 0.0) continue;

            for (int xtm = 0; xtm < 2; xtm++) {
                int c_joint = joint[yt][ytm][xtm];
                if (c_joint == 0) continue;

                int denom_joint = joint[0][ytm][xtm] + joint[1][ytm][xtm];
                if (denom_joint == 0) continue;

                double p_joint = (double)c_joint / total;
                double p_cond_joint = (double)c_joint / denom_joint;

                if (p_cond_joint > 0.0) {
                    te += p_joint * log2(p_cond_joint / p_cond_marg);
                }
            }
        }
    }

    return te;
}

double te_compute_discrete(const uint8_t *y, const uint8_t *x, int n, int states) {
    if (n < 3 || !y || !x || states < 2 || states > 16) return 0.0;

    int joint[16][16][16] = {{{0}}};
    int yt_ytm1[16][16] = {{0}};
    int ytm1[16] = {0};

    for (int t = 1; t < n; t++) {
        int yt  = y[t] % states;
        int ytm = y[t - 1] % states;
        int xtm = x[t - 1] % states;
        joint[yt][ytm][xtm]++;
        yt_ytm1[yt][ytm]++;
        ytm1[ytm]++;
    }

    int total = n - 1;
    double te = 0.0;

    for (int yt = 0; yt < states; yt++) {
        for (int ytm = 0; ytm < states; ytm++) {
            int denom_marg = ytm1[ytm];
            if (denom_marg == 0) continue;
            double p_cond_marg = (double)yt_ytm1[yt][ytm] / denom_marg;
            if (p_cond_marg <= 0.0) continue;

            for (int xtm = 0; xtm < states; xtm++) {
                int c_joint = joint[yt][ytm][xtm];
                if (c_joint == 0) continue;

                int denom_joint = 0;
                for (int s = 0; s < states; s++) denom_joint += joint[s][ytm][xtm];
                if (denom_joint == 0) continue;

                double p_joint = (double)c_joint / total;
                double p_cond_joint = (double)c_joint / denom_joint;

                if (p_cond_joint > 0.0) {
                    te += p_joint * log2(p_cond_joint / p_cond_marg);
                }
            }
        }
    }

    return te;
}

/* Aggregate an edge intent into a single binary value by majority vote */
static int edge_intent_to_binary(const EdgeIntent *ei) {
    if (!ei || ei->n_cells <= 0) return 0;
    int sum = 0;
    for (int i = 0; i < ei->n_cells; i++)
        sum += ei->cells[i] ? 1 : 0;
    return (sum * 2 >= ei->n_cells) ? 1 : 0;
}

static const EdgeIntent *find_edge_in_buffer(const IntentBuffer *ib,
                                              int grid, Edge edge) {
    if (!ib) return NULL;
    for (int i = 0; i < ib->count; i++) {
        const EdgeIntent *ei = &ib->items[i];
        if (ei->src_grid == grid && ei->src_edge == edge)
            return ei;
    }
    return NULL;
}

double te_compute_from_ring(const IntentRing *ring,
                              int src_grid, Edge src_edge,
                              int dst_grid, Edge dst_edge,
                              int delay, int history) {
    if (!ring || history < 3 || delay < 0) return -1.0;

    int max_history = ring->size;
    if (history > max_history) history = max_history;
    if (history < 3) return -1.0;

    uint8_t *x = calloc(history, sizeof(uint8_t));
    uint8_t *y = calloc(history, sizeof(uint8_t));
    if (!x || !y) { free(x); free(y); return -1.0; }

    int collected = 0;
    /* Walk backwards from most recent tick */
    for (int d = 0; d < history; d++) {
        const IntentBuffer *ib = intent_ring_peek(ring, d);
        if (!ib) break;

        const EdgeIntent *dst_ei = find_edge_in_buffer(ib, dst_grid, dst_edge);
        if (!dst_ei) break;

        /* Source is delayed by 'delay' ticks relative to destination */
        const IntentBuffer *ib_src = intent_ring_peek(ring, d + delay);
        if (!ib_src) break;
        const EdgeIntent *src_ei = find_edge_in_buffer(ib_src, src_grid, src_edge);
        if (!src_ei) break;

        x[collected] = edge_intent_to_binary(src_ei);
        y[collected] = edge_intent_to_binary(dst_ei);
        collected++;
    }

    double te = 0.0;
    if (collected >= 3)
        te = te_compute_binary(y, x, collected);
    else
        te = -1.0;

    free(x);
    free(y);
    return te;
}
