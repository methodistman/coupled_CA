#include "schedule.h"
#include <stdlib.h>
#include <string.h>

static uint64_t sched_rng(RuleSchedule *sched) {
    /* xorshift64* — self-contained, no external dependencies */
    sched->rng_state ^= sched->rng_state >> 12;
    sched->rng_state ^= sched->rng_state << 25;
    sched->rng_state ^= sched->rng_state >> 27;
    return sched->rng_state * 2685821657736338717ULL;
}

RuleSchedule *schedule_create(int capacity_hint) {
    int cap = ((capacity_hint + 7) / 8) * 8;
    if (cap < 8) cap = 8;
    RuleSchedule *s = calloc(1, sizeof(RuleSchedule));
    if (!s) return NULL;
    s->capacity = cap;
    s->rules = calloc(cap, sizeof(int));
    if (!s->rules) {
        free(s);
        return NULL;
    }
    s->interval = 1;
    s->current_idx = 0;
    s->mode = SCHED_MODE_CYCLE;
    return s;
}

void schedule_destroy(RuleSchedule *sched) {
    if (!sched) return;
    free(sched->rules);
    free(sched);
}

void schedule_add_rule(RuleSchedule *sched, int rule_idx) {
    if (!sched || sched->n_rules >= sched->capacity) return;
    sched->rules[sched->n_rules++] = rule_idx;
}

void schedule_tick(RuleSchedule *sched) {
    if (!sched || sched->n_rules == 0) return;
    sched->step_counter++;
    if (sched->step_counter < sched->interval) return;
    sched->step_counter = 0;

    switch (sched->mode) {
    case SCHED_MODE_CYCLE:
        sched->current_idx = (sched->current_idx + 1) % sched->n_rules;
        break;
    case SCHED_MODE_RANDOM:
        sched->current_idx = (int)(sched_rng(sched) % (uint64_t)sched->n_rules);
        break;
    case SCHED_MODE_SHUFFLE:
        if (sched->current_idx + 1 >= sched->n_rules) {
            /* Completed a full pass — Fisher-Yates reshuffle */
            for (int i = sched->n_rules - 1; i > 0; i--) {
                int j = (int)(sched_rng(sched) % (uint64_t)(i + 1));
                int tmp = sched->rules[i];
                sched->rules[i] = sched->rules[j];
                sched->rules[j] = tmp;
            }
            sched->current_idx = 0;
        } else {
            sched->current_idx++;
        }
        break;
    }
}
