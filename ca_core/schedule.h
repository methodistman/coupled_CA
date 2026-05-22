#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdint.h>

typedef enum {
    SCHED_MODE_CYCLE,
    SCHED_MODE_RANDOM,
    SCHED_MODE_SHUFFLE
} ScheduleMode;

typedef struct RuleSchedule_ {
    int *rules;          /* dynamically allocated rule indices */
    int n_rules;
    int capacity;        /* allocated size (rounded to multiple of 8) */
    int interval;        /* steps between rule switches */
    int step_counter;    /* counts 0 .. interval-1 */
    int current_idx;     /* active slot in rules[] */
    ScheduleMode mode;
    uint64_t rng_state;  /* self-contained RNG for random/shuffle */
} RuleSchedule;

/* Create a schedule. capacity_hint is rounded up to the nearest multiple of 8. */
RuleSchedule *schedule_create(int capacity_hint);
void schedule_destroy(RuleSchedule *sched);

/* Append a rule index. No-op if at capacity. */
void schedule_add_rule(RuleSchedule *sched, int rule_idx);

/* Advance the schedule by one step. Updates current_idx if interval elapsed. */
void schedule_tick(RuleSchedule *sched);

#endif
