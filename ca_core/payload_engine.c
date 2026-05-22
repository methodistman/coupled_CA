#include "payload_engine.h"
#include "payload_intent.h"
#include "payload_pipeline.h"
#include "schedule.h"
#include <string.h>

void payload_sys_init(PayloadSystem *s, int num_grids, int grid_size) {
    memset(s, 0, sizeof(*s));
    s->num_grids = num_grids;
    payload_coupling_init(&s->coupling, num_grids);
    for (int g = 0; g < num_grids; g++) {
        s->grids[g] = payload_grid_create(grid_size);
        if (!s->grids[g]) {
            for (int j = g - 1; j >= 0; j--) payload_grid_destroy(s->grids[j]);
            s->num_grids = 0;
            return;
        }
    }
    for (int g = num_grids; g < MAX_PAYLOAD_GRIDS; g++)
        s->grids[g] = NULL;
}

void payload_sys_destroy(PayloadSystem *s) {
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            payload_grid_destroy(s->grids[g]);
            s->grids[g] = NULL;
        }
    }
    if (s->intent_buf) {
        payload_intent_buffer_destroy(s->intent_buf);
        s->intent_buf = NULL;
    }
    if (s->intent_ring) {
        payload_intent_ring_destroy(s->intent_ring);
        s->intent_ring = NULL;
    }
    s->num_grids = 0;
}

void payload_sys_step(PayloadSystem *s) {
    if (s && s->pipeline) {
        payload_pipeline_execute(s->pipeline, s);
        return;
    }
    for (int g = 0; g < s->num_grids; g++) {
        PayloadGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_PAYLOAD_RULES) continue;
        const PayloadRule *rule = &PAYLOAD_RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                Cell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = payload_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }
    for (int g = 0; g < s->num_grids; g++) {
        PayloadGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(Cell));
    }
}

void payload_sys_step_intent(PayloadSystem *s, IntentMode mode, float alpha) {
    if (!s || s->num_grids < 1) return;

    /* Auto-create intent buffer on first use */
    if (!s->intent_buf) {
        int max_edge = 0;
        for (int g = 0; g < s->num_grids; g++) {
            if (s->grids[g] && s->grids[g]->size > max_edge)
                max_edge = s->grids[g]->size;
        }
        s->intent_buf = payload_intent_buffer_create(s->num_grids * 4, max_edge);
    }

    /* 1. Compute next_cells locally (wrap-around edges, no coupling) */
    for (int g = 0; g < s->num_grids; g++) {
        PayloadGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_PAYLOAD_RULES) continue;
        const PayloadRule *rule = &PAYLOAD_RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                Cell nb[9];
                int idx = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        nb[idx++] = payload_coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    }
                }
                rule->fn(nb, &grid->next_cells[y * sz + x], &grid->rng_state);
            }
        }
    }

    /* 2. Apply captured intents from previous tick to next_cells edges */
    if (s->intent_buf) {
        for (int i = 0; i < s->intent_buf->count; i++) {
            PayloadEdgeIntent *ei = &s->intent_buf->items[i];
            PayloadGrid *dst = s->grids[ei->dst_grid];
            if (dst && dst->active) {
                payload_intent_apply_to_next(ei, dst, mode, alpha);
            }
        }
    }

    /* 3. Swap all grids */
    for (int g = 0; g < s->num_grids; g++) {
        PayloadGrid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(Cell));
    }

    /* 4. Capture new intents from current cells for next tick */
    if (s->intent_buf) {
        payload_intent_buffer_capture_system(s, s->intent_buf);
    }
}

void payload_sys_randomize(PayloadSystem *s, uint64_t (*rng)(void)) {
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            payload_grid_init_random(s->grids[g], rng);
            s->grids[g]->rng_state = rng();
        }
    }
}
