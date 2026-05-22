#ifndef TRANSFER_ENTROPY_H
#define TRANSFER_ENTROPY_H

#include <stdint.h>
#include "../ca_core/intent.h"

/* Compute transfer entropy TE(X -> Y) for two binary time series.
   y[i] and x[i] must be 0 or 1.  Returns bits (log base 2).
   n must be >= 3 (need at least one transition count). */
double te_compute_binary(const uint8_t *y, const uint8_t *x, int n);

/* Compute transfer entropy TE(X -> Y) for two discrete time series.
   Values in y and x are assumed to be in [0, states-1].
   states <= 16 (hard limit for stack-allocated histograms).
   Returns bits (log base 2). */
double te_compute_discrete(const uint8_t *y, const uint8_t *x, int n, int states);

/* Compute TE from intent ring history between two edges.
   Looks back up to 'history' ticks from the ring.
   delay = how many ticks ago the source edge is shifted (0 = no shift).
   Returns -1.0 if not enough data. */
double te_compute_from_ring(const IntentRing *ring,
                              int src_grid, Edge src_edge,
                              int dst_grid, Edge dst_edge,
                              int delay, int history);

#endif
