#ifndef PAYLOAD_RULES_H
#define PAYLOAD_RULES_H

#include "cell.h"

/* 3x3 neighborhood flattened: row-major, self at index 4 */
typedef void (*PayloadRuleFn)(const Cell in[9], Cell *out, void *params);

typedef struct {
    const char *name;
    PayloadRuleFn fn;
} PayloadRule;

extern const PayloadRule PAYLOAD_RULE_TABLE[];
extern const int NUM_PAYLOAD_RULES;

const PayloadRule *payload_rules_lookup(const char *name);
int payload_rules_index_by_name(const char *name);

/* Built-in rule implementations */
void rule_life_payload(const Cell in[9], Cell *out, void *params);
void rule_diffuse(const Cell in[9], Cell *out, void *params);
void rule_carry_add(const Cell in[9], Cell *out, void *params);
void rule_max_flood(const Cell in[9], Cell *out, void *params);
void rule_threshold(const Cell in[9], Cell *out, void *params);
void rule_identity(const Cell in[9], Cell *out, void *params);
void rule_xor_payload(const Cell in[9], Cell *out, void *params);
void rule_and_payload(const Cell in[9], Cell *out, void *params);
void rule_or_payload(const Cell in[9], Cell *out, void *params);
void rule_add_mod(const Cell in[9], Cell *out, void *params);
void rule_shift_left(const Cell in[9], Cell *out, void *params);
void rule_compare_max(const Cell in[9], Cell *out, void *params);
void rule_noisy_life_payload(const Cell in[9], Cell *out, void *ctx);
void rule_prob_diffuse(const Cell in[9], Cell *out, void *ctx);
void rule_nand_payload(const Cell in[9], Cell *out, void *params);
void rule_nor_payload(const Cell in[9], Cell *out, void *params);
void rule_xnor_payload(const Cell in[9], Cell *out, void *params);
void rule_majority_payload(const Cell in[9], Cell *out, void *params);
void rule_parity_payload(const Cell in[9], Cell *out, void *params);
void rule_multiply_mod(const Cell in[9], Cell *out, void *params);

#endif
