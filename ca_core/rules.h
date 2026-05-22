#ifndef RULES_H
#define RULES_H

#include <stdint.h>

typedef struct {
    const char *name;
    int born[9];
    int survive[9];
    int is_1d;
    uint8_t rule_1d;
} CARule;

extern const CARule RULE_TABLE[];
extern const int NUM_RULES;

const CARule *rules_lookup(const char *name);
int rules_index_by_name(const char *name);

#endif
