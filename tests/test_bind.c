#include <stdio.h>
#include <string.h>
#include "../dsl/parser.h"
#include "../dsl/bind.h"
#include "../ca_core/engine.h"
#include "../ca_core/payload_engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_rules.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

int main(void) {
    int errors = 0;
    printf("test_bind\n");

    /* Test 1: bind binary System from DSL */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"g0\" size=32 rule=\"Conway's Life\" };"
            "grid 1 { name=\"g1\" size=32 rule=\"Rule 110\" };"
            "connect 0.top -> 1.bottom;"
            "steps 100;";
        if (dsl_parse(src, &topo) == 0) {
            System sys;
            if (dsl_bind_system(&topo, &sys) == 0) {
                PASS("bind_system_ok");
                if (sys.num_grids == 2 &&
                    sys.grids[0]->size == 32 &&
                    sys.grids[1]->size == 32)
                    PASS("bind_system_sizes");
                else
                    FAIL("bind_system_sizes");

                if (strcmp(RULE_TABLE[sys.grids[0]->rule_idx].name, "Conway's Life") == 0 &&
                    strcmp(RULE_TABLE[sys.grids[1]->rule_idx].name, "Rule 110") == 0)
                    PASS("bind_system_rules");
                else
                    FAIL("bind_system_rules");

                /* check coupling was set: grid 0 top -> grid 1 bottom */
                if (sys.coupling.conn[0][EDGE_TOP].target_grid == 1 &&
                    sys.coupling.conn[0][EDGE_TOP].target_edge == EDGE_BOTTOM)
                    PASS("bind_system_coupling");
                else
                    FAIL("bind_system_coupling");

                sys_destroy(&sys);
            } else {
                FAIL("bind_system_ok");
            }
        } else {
            FAIL("parse_for_bind");
        }
    }

    /* Test 2: bind PayloadSystem from DSL */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"p0\" size=16 rule=\"diffuse\" };"
            "grid 1 { name=\"p1\" size=16 rule=\"carry_add\" };"
            "connect 0.left -> 1.right, 0.5;"
            "steps 50;";
        if (dsl_parse(src, &topo) == 0) {
            PayloadSystem psys;
            if (dsl_bind_payload_system(&topo, &psys) == 0) {
                PASS("bind_payload_ok");
                if (psys.num_grids == 2 &&
                    psys.grids[0]->size == 16 &&
                    psys.grids[1]->size == 16)
                    PASS("bind_payload_sizes");
                else
                    FAIL("bind_payload_sizes");

                if (strcmp(PAYLOAD_RULE_TABLE[psys.grids[0]->rule_idx].name, "diffuse") == 0 &&
                    strcmp(PAYLOAD_RULE_TABLE[psys.grids[1]->rule_idx].name, "carry_add") == 0)
                    PASS("bind_payload_rules");
                else
                    FAIL("bind_payload_rules");

                /* check coupling with strength */
                if (psys.coupling.conn[0][EDGE_LEFT].target_grid == 1 &&
                    psys.coupling.conn[0][EDGE_LEFT].target_edge == EDGE_RIGHT &&
                    psys.coupling.conn[0][EDGE_LEFT].strength == 0.5f)
                    PASS("bind_payload_coupling_strength");
                else
                    FAIL("bind_payload_coupling_strength");

                payload_sys_destroy(&psys);
            } else {
                FAIL("bind_payload_ok");
            }
        } else {
            FAIL("parse_for_payload_bind");
        }
    }

    /* Test 3: parser comment handling (# as line comment) */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"a\" size=16 rule=\"life\" }; # this is a comment\n"
            "connect 0.top -> 1.bottom;";
        if (dsl_parse(src, &topo) == 0 && topo.num_grids == 1 && topo.num_conns == 1) {
            PASS("parse_comment_skip");
        } else {
            FAIL("parse_comment_skip");
        }
    }

    /* Test 4: mismatched sizes should fail */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"a\" size=16 rule=\"life\" };"
            "grid 1 { name=\"b\" size=32 rule=\"life\" };";
        if (dsl_parse(src, &topo) == 0) {
            System sys;
            if (dsl_bind_system(&topo, &sys) == -1) {
                PASS("bind_mismatch_size_rejected");
            } else {
                sys_destroy(&sys);
                FAIL("bind_mismatch_size_rejected");
            }
        } else {
            FAIL("parse_mismatch");
        }
    }

    /* Test 5: bind HybridSystem from DSL with xconnect */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"b0\" size=16 rule=\"Conway's Life\" type=\"B\" };"
            "grid 1 { name=\"p0\" size=16 rule=\"identity\" type=\"P\" };"
            "xconnect \"B\"0.bottom -> \"P\"0.top;"
            "steps 50;";
        if (dsl_parse(src, &topo) == 0) {
            HybridSystem hsys;
            if (dsl_bind_hybrid_system(&topo, &hsys) == 0) {
                PASS("bind_hybrid_ok");
                if (hsys.binary.num_grids == 1 && hsys.payload.num_grids == 1)
                    PASS("bind_hybrid_counts");
                else
                    FAIL("bind_hybrid_counts");

                if (hsys.binary.grids[0]->size == 16 && hsys.payload.grids[0]->size == 16)
                    PASS("bind_hybrid_sizes");
                else
                    FAIL("bind_hybrid_sizes");

                if (strcmp(RULE_TABLE[hsys.binary.grids[0]->rule_idx].name, "Conway's Life") == 0 &&
                    strcmp(PAYLOAD_RULE_TABLE[hsys.payload.grids[0]->rule_idx].name, "identity") == 0)
                    PASS("bind_hybrid_rules");
                else
                    FAIL("bind_hybrid_rules");

                if (hsys.n_xconns == 1)
                    PASS("bind_hybrid_xconn");
                else
                    FAIL("bind_hybrid_xconn");

                hybrid_sys_destroy(&hsys);
            } else {
                FAIL("bind_hybrid_ok");
            }
        } else {
            FAIL("parse_for_hybrid_bind");
        }
    }

    /* Test 6: hybrid with intra-type connections */
    {
        DSLTopology topo;
        const char *src =
            "grid 0 { name=\"b0\" size=8 rule=\"Conway's Life\" type=\"B\" };"
            "grid 1 { name=\"b1\" size=8 rule=\"Conway's Life\" type=\"B\" };"
            "grid 2 { name=\"p0\" size=8 rule=\"diffuse\" type=\"P\" };"
            "connect 0.top -> 1.bottom;"
            "xconnect \"B\"0.bottom -> \"P\"0.top;"
            "steps 20;";
        if (dsl_parse(src, &topo) == 0) {
            HybridSystem hsys;
            if (dsl_bind_hybrid_system(&topo, &hsys) == 0) {
                PASS("bind_hybrid_multi_ok");
                if (hsys.binary.num_grids == 2 && hsys.payload.num_grids == 1)
                    PASS("bind_hybrid_multi_counts");
                else
                    FAIL("bind_hybrid_multi_counts");

                /* intra-type binary coupling: b0 top -> b1 bottom */
                if (hsys.binary.coupling.conn[0][EDGE_TOP].target_grid == 1)
                    PASS("bind_hybrid_multi_bconn");
                else
                    FAIL("bind_hybrid_multi_bconn");

                hybrid_sys_destroy(&hsys);
            } else {
                FAIL("bind_hybrid_multi_ok");
            }
        } else {
            FAIL("parse_for_hybrid_multi_bind");
        }
    }

    printf("%d errors\n", errors);
    return errors;
}
