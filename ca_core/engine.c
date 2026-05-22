#include "engine.h"
#include "schedule.h"
#include "pipeline.h"
#include "genome.h"
#include "../metrics/history.h"
#include <string.h>

void sys_init(System *s, int num_grids, int grid_size) {
    s->num_grids = num_grids;
    s->use_bitgrid = 0;
    s->intent_buf = NULL;
    s->intent_ring = NULL;
    s->pipeline = NULL;
    s->metrics_history = NULL;
    coupling_init(&s->coupling, num_grids);
    for (int g = 0; g < num_grids; g++) {
        s->grids[g] = grid_create(grid_size);
        if (!s->grids[g]) {
            /* allocation failed; destroy any already-created grids */
            for (int j = g - 1; j >= 0; j--) grid_destroy(s->grids[j]);
            s->num_grids = 0;
            return;
        }
        if ((grid_size & 63) == 0)
            s->bgrids[g] = bitgrid_create(grid_size);
        else
            s->bgrids[g] = NULL;
    }
    for (int g = num_grids; g < MAX_GRIDS; g++) {
        s->grids[g] = NULL;
        s->bgrids[g] = NULL;
    }
}

void sys_destroy(System *s) {
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            grid_destroy(s->grids[g]);
            s->grids[g] = NULL;
        }
        if (s->bgrids[g]) {
            bitgrid_destroy(s->bgrids[g]);
            s->bgrids[g] = NULL;
        }
    }
    if (s->intent_buf) {
        intent_buffer_destroy(s->intent_buf);
        s->intent_buf = NULL;
    }
    if (s->intent_ring) {
        intent_ring_destroy(s->intent_ring);
        s->intent_ring = NULL;
    }
    if (s->pipeline) {
        pipeline_destroy(s->pipeline);
        s->pipeline = NULL;
    }
    if (s->metrics_history) {
        metrics_history_destroy(s->metrics_history);
        s->metrics_history = NULL;
    }
    s->num_grids = 0;
}

void sys_step(System *s) {
    /* If a pipeline is configured, delegate to it */
    if (s->pipeline) {
        pipeline_execute(s->pipeline, s);
        return;
    }
    /* If any grid has a schedule, force scalar path so schedules tick
       and non-Life rules are applied correctly. */
    int has_schedule = 0;
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g] && s->grids[g]->sched) {
            has_schedule = 1;
            break;
        }
    }

    if (s->use_bitgrid && !has_schedule) {
        /* Fast path: bit-parallel Life per grid (no coupling) */
        for (int g = 0; g < s->num_grids; g++) {
            if (s->bgrids[g])
                bitgrid_step_life(s->bgrids[g]);
        }
        return;
    }

    /* Scalar path with coupling */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->sched) {
            schedule_tick(grid->sched);
            grid->rule_idx = grid->sched->rules[grid->sched->current_idx];
        }
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_RULES) continue;
        const CARule *rule = &RULE_TABLE[grid->rule_idx];
        int sz = grid->size;

        if (rule->is_1d) {
            for (int y = 0; y < sz; y++) {
                for (int x = 0; x < sz; x++) {
                    int l = coupling_read(&s->coupling, s->grids, g, x, y, -1, 0);
                    int c = grid->cells[y * sz + x];
                    int r = coupling_read(&s->coupling, s->grids, g, x, y, 1, 0);
                    int pattern = (l << 2) | (c << 1) | r;
                    grid->next_cells[y * sz + x] = (rule->rule_1d >> pattern) & 1;
                }
            }
        } else {
            for (int y = 0; y < sz; y++) {
                for (int x = 0; x < sz; x++) {
                    int count = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (dx || dy)
                                count += coupling_read(&s->coupling, s->grids, g, x, y, dx, dy);
                    int alive = grid->cells[y * sz + x];
                    grid->next_cells[y * sz + x] = alive ? rule->survive[count] : rule->born[count];
                }
            }
        }
    }

    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(uint8_t));
    }
}

void sys_randomize(System *s, uint64_t (*rng)(void)) {
    for (int g = 0; g < s->num_grids; g++) {
        if (s->grids[g]) {
            grid_init_random(s->grids[g], rng);
            s->grids[g]->rng_state = rng();
        }
        if (s->bgrids[g])
            bitgrid_randomize(s->bgrids[g], rng);
    }
}

/* Step with intent-buffer coupling:
   1. Compute next_cells for all grids using LOCAL neighborhoods only.
   2. Apply previous tick's captured intents to next_cells edges.
   3. Swap cells/next_cells for all grids.
   4. Capture new intents from current cells for next tick.
   This produces a one-tick delay in coupling and records edge history. */
void sys_step_intent(System *s, IntentMode mode, float alpha) {
    if (!s || s->num_grids < 1) return;

    /* Auto-create intent buffer on first use */
    if (!s->intent_buf) {
        int max_edge = 0;
        for (int g = 0; g < s->num_grids; g++) {
            if (s->grids[g] && s->grids[g]->size > max_edge)
                max_edge = s->grids[g]->size;
        }
        s->intent_buf = intent_buffer_create(s->num_grids * 4, max_edge);
    }

    /* 1. Compute next_cells locally (wrap-around edges, no coupling) */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_RULES) continue;
        const CARule *rule = &RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = (x + dx + sz) % sz;
                        int ny = (y + dy + sz) % sz;
                        count += grid->cells[ny * sz + nx];
                    }
                }
                int alive = grid->cells[y * sz + x];
                grid->next_cells[y * sz + x] = alive ? rule->survive[count] : rule->born[count];
            }
        }
    }

    /* 2. Apply captured intents from previous tick to next_cells edges */
    for (int i = 0; i < s->intent_buf->count; i++) {
        EdgeIntent *ei = &s->intent_buf->items[i];
        Grid *dst = s->grids[ei->dst_grid];
        if (dst && dst->active) {
            intent_apply_to_next(ei, dst, mode, alpha);
        }
    }

    /* 3. Swap all grids */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(uint8_t));
    }

    /* 4. Capture new intents from current cells for next tick */
    intent_buffer_capture_system(s, s->intent_buf);
}

void sys_step_intent_delayed(System *s, IntentMode mode, float alpha, int delay) {
    if (!s || s->num_grids < 1) return;
    if (!s->intent_ring) {
        int max_edge = 0;
        for (int g = 0; g < s->num_grids; g++) {
            if (s->grids[g] && s->grids[g]->size > max_edge)
                max_edge = s->grids[g]->size;
        }
        s->intent_ring = intent_ring_create(16, s->num_grids * 4, max_edge);
    }
    /* 1. Compute next_cells locally */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        if (grid->rule_idx < 0 || grid->rule_idx >= NUM_RULES) continue;
        const CARule *rule = &RULE_TABLE[grid->rule_idx];
        int sz = grid->size;
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = (x + dx + sz) % sz;
                        int ny = (y + dy + sz) % sz;
                        count += grid->cells[ny * sz + nx];
                    }
                }
                int alive = grid->cells[y * sz + x];
                grid->next_cells[y * sz + x] = alive ? rule->survive[count] : rule->born[count];
            }
        }
    }
    /* 2. Apply intents from delay ticks ago */
    const IntentBuffer *ib = intent_ring_peek(s->intent_ring, delay);
    if (ib) {
        for (int i = 0; i < ib->count; i++) {
            EdgeIntent *ei = &ib->items[i];
            Grid *dst = s->grids[ei->dst_grid];
            if (dst && dst->active) {
                intent_apply_to_next(ei, dst, mode, alpha);
            }
        }
    }
    /* 3. Swap all grids */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(uint8_t));
    }
    /* 4. Push current state into ring */
    intent_ring_push(s->intent_ring, s);
}

void sys_step_intent_delayed_genomic(System *s, IntentMode mode, float alpha, int delay) {
    if (!s || s->num_grids < 1) return;
    if (!s->intent_ring) {
        int max_edge = 0;
        for (int g = 0; g < s->num_grids; g++) {
            if (s->grids[g] && s->grids[g]->size > max_edge)
                max_edge = s->grids[g]->size;
        }
        s->intent_ring = intent_ring_create(16, s->num_grids * 4, max_edge);
    }
    /* 1. Compute next_cells locally with per-cell genomic rule selection */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        int sz = grid->size;
        int has_genomes = grid->genomes != NULL;
        int default_rule_idx = grid->rule_idx;
        if (default_rule_idx < 0 || default_rule_idx >= NUM_RULES) continue;
        const CARule *default_rule = &RULE_TABLE[default_rule_idx];
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                int idx = y * sz + x;
                const CARule *rule = default_rule;
                if (has_genomes) {
                    int ri = (int)GENOME_RULE_SELECT(grid->genomes[idx]);
                    rule = &RULE_TABLE[((ri % NUM_RULES) + NUM_RULES) % NUM_RULES];
                }
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = (x + dx + sz) % sz;
                        int ny = (y + dy + sz) % sz;
                        count += grid->cells[ny * sz + nx];
                    }
                }
                int alive = grid->cells[idx];
                grid->next_cells[idx] = alive ? rule->survive[count] : rule->born[count];
            }
        }
    }
    /* 2. Apply intents from delay ticks ago */
    const IntentBuffer *ib = intent_ring_peek(s->intent_ring, delay);
    if (ib) {
        for (int i = 0; i < ib->count; i++) {
            EdgeIntent *ei = &ib->items[i];
            Grid *dst = s->grids[ei->dst_grid];
            if (dst && dst->active) {
                intent_apply_to_next(ei, dst, mode, alpha);
            }
        }
    }
    /* 3. Swap all grids */
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        if (!grid || !grid->active) continue;
        memcpy(grid->cells, grid->next_cells, grid->size * grid->size * sizeof(uint8_t));
    }
    /* 4. Push current state into ring */
    intent_ring_push(s->intent_ring, s);
}

void sys_sync_to_bitgrid(System *s) {
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        BitGrid *bg = s->bgrids[g];
        if (!grid || !bg) continue;
        int sz = grid->size;
        for (int y = 0; y < sz; y++)
            for (int x = 0; x < sz; x++)
                bitgrid_set(bg, x, y, grid->cells[y * sz + x]);
    }
}

void sys_sync_from_bitgrid(System *s) {
    for (int g = 0; g < s->num_grids; g++) {
        Grid *grid = s->grids[g];
        BitGrid *bg = s->bgrids[g];
        if (!grid || !bg) continue;
        int sz = grid->size;
        for (int y = 0; y < sz; y++)
            for (int x = 0; x < sz; x++)
                grid->cells[y * sz + x] = (uint8_t)bitgrid_get(bg, x, y);
    }
}
