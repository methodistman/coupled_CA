#include "bind.h"
#include "../ca_core/coupling.h"
#include "../ca_core/payload_coupling.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_rules.h"
#include "../ca_core/schedule.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Edge edge_from_string(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return (Edge)-1;
}

/*
 * Parse a rule string.  Supports shorthand schedules:
 *   cycle(rule1,rule2),period=N
 *   random(rule1,rule2),period=N
 *   shuffle(rule1,rule2),period=N
 * Returns:
 *   0  -> plain rule, *plain_ridx is set
 *   1  -> schedule,   *sched_out is set
 *  -1  -> error
 */
static int parse_rule_or_schedule(const char *str, int *plain_ridx,
                                   RuleSchedule **sched_out,
                                   int (*lookup)(const char *)) {
    const char *p = str;
    ScheduleMode mode;

    if (!strncmp(p, "cycle(", 6))       { mode = SCHED_MODE_CYCLE;  p += 6; }
    else if (!strncmp(p, "random(", 7)) { mode = SCHED_MODE_RANDOM; p += 7; }
    else if (!strncmp(p, "shuffle(", 8)){ mode = SCHED_MODE_SHUFFLE;p += 8; }
    else {
        *plain_ridx = lookup(str);
        return (*plain_ridx < 0) ? -1 : 0;
    }

    const char *close = strchr(p, ')');
    if (!close) return -1;

    *sched_out = schedule_create(8);
    if (!*sched_out) return -1;
    (*sched_out)->mode = mode;

    const char *start = p;
    for (const char *q = p; q <= close; q++) {
        if (*q == ',' || q == close) {
            int len = (int)(q - start);
            while (len > 0 && start[len - 1] == ' ') len--;
            if (len > 0 && len < 64) {
                char name[64];
                memcpy(name, start, len);
                name[len] = '\0';
                int ridx = lookup(name);
                if (ridx < 0) {
                    schedule_destroy(*sched_out);
                    *sched_out = NULL;
                    return -1;
                }
                schedule_add_rule(*sched_out, ridx);
            }
            start = q + 1;
            while (*start == ' ') start++;
        }
    }

    if ((*sched_out)->n_rules == 0) {
        schedule_destroy(*sched_out);
        *sched_out = NULL;
        return -1;
    }

    const char *period = strstr(close, ",period=");
    if (period) {
        (*sched_out)->interval = atoi(period + 8);
        if ((*sched_out)->interval < 1) (*sched_out)->interval = 1;
    }

    return 1;
}

int dsl_bind_system(const DSLTopology *topo, System *sys) {
    if (topo->num_grids == 0) return -1;
    int sz = topo->grids[0].size;
    for (int g = 1; g < topo->num_grids; g++)
        if (topo->grids[g].size != sz) return -1; /* mismatched sizes */

    sys_init(sys, topo->num_grids, sz);

    for (int g = 0; g < topo->num_grids; g++) {
        int plain_ridx;
        RuleSchedule *sched = NULL;
        int rc = parse_rule_or_schedule(topo->grids[g].rule, &plain_ridx, &sched, rules_index_by_name);
        if (rc < 0) {
            sys_destroy(sys);
            return -1;
        }
        if (sched) {
            sys->grids[g]->sched = sched;
            sys->grids[g]->rule_idx = sched->rules[sched->current_idx];
        } else {
            sys->grids[g]->rule_idx = plain_ridx;
        }
    }

    for (int i = 0; i < topo->num_conns; i++) {
        const DSLConnSpec *c = &topo->conns[i];
        Edge se = edge_from_string(c->src_edge);
        Edge de = edge_from_string(c->dst_edge);
        if ((int)se < 0 || (int)de < 0) return -1;
        coupling_connect(&sys->coupling, c->src_grid, se, c->dst_grid, de);
    }

    return 0;
}

int dsl_bind_payload_system(const DSLTopology *topo, PayloadSystem *sys) {
    if (topo->num_grids == 0) return -1;
    int sz = topo->grids[0].size;
    for (int g = 1; g < topo->num_grids; g++)
        if (topo->grids[g].size != sz) return -1;

    payload_sys_init(sys, topo->num_grids, sz);

    for (int g = 0; g < topo->num_grids; g++) {
        int plain_ridx;
        RuleSchedule *sched = NULL;
        int rc = parse_rule_or_schedule(topo->grids[g].rule, &plain_ridx, &sched, payload_rules_index_by_name);
        if (rc < 0) {
            payload_sys_destroy(sys);
            return -1;
        }
        if (sched) {
            sys->grids[g]->sched = sched;
            sys->grids[g]->rule_idx = sched->rules[sched->current_idx];
        } else {
            sys->grids[g]->rule_idx = plain_ridx;
        }
    }

    for (int i = 0; i < topo->num_conns; i++) {
        const DSLConnSpec *c = &topo->conns[i];
        Edge se = edge_from_string(c->src_edge);
        Edge de = edge_from_string(c->dst_edge);
        if ((int)se < 0 || (int)de < 0) return -1;
        payload_coupling_connect(&sys->coupling, c->src_grid, se, c->dst_grid, de, c->strength);
    }

    return 0;
}

int dsl_bind_hybrid_system(const DSLTopology *topo, HybridSystem *sys) {
    if (topo->num_grids == 0) return -1;
    int sz = topo->grids[0].size;
    for (int g = 1; g < topo->num_grids; g++)
        if (topo->grids[g].size != sz) return -1;

    int n_binary = 0, n_payload = 0;
    int bmap[DSL_MAX_GRIDS];
    int pmap[DSL_MAX_GRIDS];
    for (int g = 0; g < topo->num_grids; g++) {
        if (topo->grids[g].type == 'P') {
            pmap[g] = n_payload++;
            bmap[g] = -1;
        } else {
            bmap[g] = n_binary++;
            pmap[g] = -1;
        }
    }

    hybrid_sys_init(sys, n_binary, n_payload, 0, sz);

    /* Set rules */
    for (int g = 0; g < topo->num_grids; g++) {
        int plain_ridx;
        RuleSchedule *sched = NULL;
        if (bmap[g] >= 0) {
            int rc = parse_rule_or_schedule(topo->grids[g].rule, &plain_ridx, &sched, rules_index_by_name);
            if (rc < 0) { hybrid_sys_destroy(sys); return -1; }
            if (sched) {
                sys->binary.grids[bmap[g]]->sched = sched;
                sys->binary.grids[bmap[g]]->rule_idx = sched->rules[sched->current_idx];
            } else {
                sys->binary.grids[bmap[g]]->rule_idx = plain_ridx;
            }
        } else {
            int rc = parse_rule_or_schedule(topo->grids[g].rule, &plain_ridx, &sched, payload_rules_index_by_name);
            if (rc < 0) { hybrid_sys_destroy(sys); return -1; }
            if (sched) {
                sys->payload.grids[pmap[g]]->sched = sched;
                sys->payload.grids[pmap[g]]->rule_idx = sched->rules[sched->current_idx];
            } else {
                sys->payload.grids[pmap[g]]->rule_idx = plain_ridx;
            }
        }
    }

    /* Intra-type connections */
    for (int i = 0; i < topo->num_conns; i++) {
        const DSLConnSpec *c = &topo->conns[i];
        Edge se = edge_from_string(c->src_edge);
        Edge de = edge_from_string(c->dst_edge);
        if ((int)se < 0 || (int)de < 0) return -1;
        if (bmap[c->src_grid] >= 0 && bmap[c->dst_grid] >= 0) {
            coupling_connect(&sys->binary.coupling, bmap[c->src_grid], se, bmap[c->dst_grid], de);
        } else if (pmap[c->src_grid] >= 0 && pmap[c->dst_grid] >= 0) {
            payload_coupling_connect(&sys->payload.coupling, pmap[c->src_grid], se, pmap[c->dst_grid], de, c->strength);
        }
        /* mixed-type conns should use xconnect instead */
    }

    /* Cross-type xconnects (grid indices are subsystem-local) */
    for (int i = 0; i < topo->num_xconns; i++) {
        const DSLXConnSpec *c = &topo->xconns[i];
        Edge se = edge_from_string(c->src_edge);
        Edge de = edge_from_string(c->dst_edge);
        if ((int)se < 0 || (int)de < 0) return -1;
        HybridGridType st = (c->src_type == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
        HybridGridType dt = (c->dst_type == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
        int sg = c->src_grid;
        int dg = c->dst_grid;
        int max_sg = (st == HYBRID_BINARY) ? sys->binary.num_grids : sys->payload.num_grids;
        int max_dg = (dt == HYBRID_BINARY) ? sys->binary.num_grids : sys->payload.num_grids;
        if (sg < 0 || sg >= max_sg || dg < 0 || dg >= max_dg) return -1;
        hybrid_xconn_add(sys, st, sg, se, dt, dg, de);
    }

    return 0;
}
