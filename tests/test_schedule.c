#include "../ca_core/schedule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_cycle(void) {
    RuleSchedule *s = schedule_create(3);
    assert(s);
    assert(s->capacity == 8); /* rounded up */
    s->mode = SCHED_MODE_CYCLE;
    s->interval = 2;
    schedule_add_rule(s, 0);
    schedule_add_rule(s, 1);
    schedule_add_rule(s, 2);
    assert(s->current_idx == 0);
    assert(s->rules[s->current_idx] == 0);

    schedule_tick(s); /* counter=1, no switch */
    assert(s->current_idx == 0);
    schedule_tick(s); /* counter=2 -> switch */
    assert(s->current_idx == 1);
    assert(s->rules[s->current_idx] == 1);

    schedule_tick(s); schedule_tick(s);
    assert(s->current_idx == 2);
    schedule_tick(s); schedule_tick(s);
    assert(s->current_idx == 0); /* wrap */

    schedule_destroy(s);
    printf("  PASS: test_cycle\n");
}

static void test_random(void) {
    RuleSchedule *s = schedule_create(8);
    s->mode = SCHED_MODE_RANDOM;
    s->interval = 1;
    s->rng_state = 12345;
    schedule_add_rule(s, 5);
    schedule_add_rule(s, 6);
    schedule_add_rule(s, 7);

    schedule_tick(s);
    assert(s->current_idx >= 0 && s->current_idx < 3);
    int first = s->current_idx;
    schedule_tick(s);
    /* Random mode may or may not change; just verify in bounds */
    assert(s->current_idx >= 0 && s->current_idx < 3);

    schedule_destroy(s);
    printf("  PASS: test_random\n");
}

static void test_shuffle(void) {
    RuleSchedule *s = schedule_create(8);
    s->mode = SCHED_MODE_SHUFFLE;
    s->interval = 1;
    s->rng_state = 99999;
    schedule_add_rule(s, 10);
    schedule_add_rule(s, 11);
    schedule_add_rule(s, 12);

    schedule_tick(s);
    assert(s->current_idx == 1);
    schedule_tick(s);
    assert(s->current_idx == 2);
    schedule_tick(s); /* end of pass -> reshuffle, reset to 0 */
    assert(s->current_idx == 0);

    schedule_destroy(s);
    printf("  PASS: test_shuffle\n");
}

int main(void) {
    printf("test_schedule\n");
    test_cycle();
    test_random();
    test_shuffle();
    return 0;
}
