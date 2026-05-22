#include "payload_rules.h"
#include <string.h>

void rule_life_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int count = 0;
    int payload_sum = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4) {
            count += in[i].alive;
            payload_sum += in[i].payload;
        }
    }
    int alive = in[4].alive;
    out->alive = alive ? ((count == 2 || count == 3) ? 1 : 0) : (count == 3 ? 1 : 0);
    out->payload = (uint8_t)(count ? (payload_sum / count) : 0);
}

void rule_diffuse(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int sum = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4) sum += in[i].payload;
    out->alive = 1;
    out->payload = (uint8_t)(sum / 9);
}

void rule_carry_add(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int sum = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4) sum += in[i].payload;
    out->alive = (sum > 255) ? 1 : 0;
    out->payload = (uint8_t)(sum & 0xFF);
}

void rule_max_flood(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t max = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4 && in[i].payload > max) max = in[i].payload;
    out->alive = 1;
    out->payload = max;
}

void rule_threshold(const Cell in[9], Cell *out, void *params) {
    int threshold = params ? *(int*)params : 128;
    out->alive = (in[4].payload > threshold) ? 1 : 0;
    out->payload = out->alive ? in[4].payload : 0;
}

void rule_identity(const Cell in[9], Cell *out, void *params) {
    (void)params;
    *out = in[4];
}

void rule_xor_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val ^= in[i].payload;
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_and_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val &= in[i].payload;
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_or_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val |= in[i].payload;
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_add_mod(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int sum = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4) sum += in[i].payload;
    out->alive = 1;
    out->payload = (uint8_t)(sum & 0xFF);
}

void rule_shift_left(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint16_t val = (uint16_t)(in[4].payload << 1);
    out->alive = (val > 255) ? 1 : 0;
    out->payload = (uint8_t)(val & 0xFF);
}

void rule_compare_max(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t max = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4 && in[i].payload > max) max = in[i].payload;
    out->alive = (max != in[4].payload) ? 1 : 0;
    out->payload = max;
}

static uint64_t stoch_rng(uint64_t *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return *state * 2685821657736338717ULL;
}

void rule_noisy_life_payload(const Cell in[9], Cell *out, void *ctx) {
    uint64_t *st = (uint64_t *)ctx;
    int count = 0, payload_sum = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4) { count += in[i].alive; payload_sum += in[i].payload; }
    }
    int alive = in[4].alive;
    int next_alive = alive ? ((count == 2 || count == 3) ? 1 : 0) : (count == 3 ? 1 : 0);
    if (st) {
        uint64_t r = stoch_rng(st);
        if ((r & 0xFF) < 13) next_alive = !next_alive; /* ~5% noise */
    }
    out->alive = next_alive;
    out->payload = (uint8_t)(count ? (payload_sum / count) : 0);
}

void rule_nand_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val = ~(val & in[i].payload);
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_nor_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val = ~(val | in[i].payload);
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_xnor_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val = ~(val ^ in[i].payload);
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_majority_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int hist[256] = {0};
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            hist[in[i].payload]++;
            alive_neighbors++;
        }
    }
    uint8_t maj = in[4].payload;
    int best = hist[maj];
    for (int v = 0; v < 256; v++) {
        if (hist[v] > best) { best = hist[v]; maj = (uint8_t)v; }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = maj;
}

void rule_parity_payload(const Cell in[9], Cell *out, void *params) {
    (void)params;
    uint8_t val = 0;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            val ^= in[i].payload;
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = val;
}

void rule_multiply_mod(const Cell in[9], Cell *out, void *params) {
    (void)params;
    int prod = in[4].payload;
    int alive_neighbors = 0;
    for (int i = 0; i < 9; i++) {
        if (i != 4 && in[i].alive) {
            prod = (prod * in[i].payload) & 0xFF;
            alive_neighbors++;
        }
    }
    out->alive = (alive_neighbors > 0) ? 1 : 0;
    out->payload = (uint8_t)prod;
}

void rule_prob_diffuse(const Cell in[9], Cell *out, void *ctx) {
    uint64_t *st = (uint64_t *)ctx;
    if (st) {
        uint64_t r = stoch_rng(st);
        if ((r & 0xFF) >= 204) { /* ~20% chance to freeze */
            out->alive = in[4].alive;
            out->payload = in[4].payload;
            return;
        }
    }
    int sum = in[4].payload;
    for (int i = 0; i < 9; i++) if (i != 4) sum += in[i].payload;
    out->alive = 1;
    out->payload = (uint8_t)(sum / 9);
}

const PayloadRule PAYLOAD_RULE_TABLE[] = {
    {"life_payload", rule_life_payload},
    {"diffuse",      rule_diffuse},
    {"carry_add",    rule_carry_add},
    {"max_flood",    rule_max_flood},
    {"threshold",    rule_threshold},
    {"identity",     rule_identity},
    {"xor_payload",  rule_xor_payload},
    {"and_payload",  rule_and_payload},
    {"or_payload",   rule_or_payload},
    {"add_mod",      rule_add_mod},
    {"shift_left",   rule_shift_left},
    {"compare_max",  rule_compare_max},
    {"noisy_life",   rule_noisy_life_payload},
    {"prob_diffuse", rule_prob_diffuse},
    {"nand_payload", rule_nand_payload},
    {"nor_payload",  rule_nor_payload},
    {"xnor_payload", rule_xnor_payload},
    {"majority",     rule_majority_payload},
    {"parity",       rule_parity_payload},
    {"multiply_mod", rule_multiply_mod},
};

const int NUM_PAYLOAD_RULES = (int)(sizeof(PAYLOAD_RULE_TABLE) / sizeof(PAYLOAD_RULE_TABLE[0]));

const PayloadRule *payload_rules_lookup(const char *name) {
    for (int i = 0; i < NUM_PAYLOAD_RULES; i++)
        if (strcmp(PAYLOAD_RULE_TABLE[i].name, name) == 0)
            return &PAYLOAD_RULE_TABLE[i];
    return NULL;
}

int payload_rules_index_by_name(const char *name) {
    for (int i = 0; i < NUM_PAYLOAD_RULES; i++)
        if (strcmp(PAYLOAD_RULE_TABLE[i].name, name) == 0)
            return i;
    return -1;
}
