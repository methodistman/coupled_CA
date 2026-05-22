#include <stdio.h>
#include <string.h>
#include "../dsl/parser.h"

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name) do { printf("  FAIL: %s at line %d\n", name, __LINE__); errors++; } while(0)

int main(void) {
    int errors = 0;
    printf("test_dsl\n");

    /* Test 1: basic grid + connect */
    {
        DSLTopology topo;
        const char *src = "grid 0 { name=\"g0\" size=32 rule=\"life_payload\" }; connect 0.top -> 1.bottom,0.5; steps 100;";
        if (dsl_parse(src, &topo) == 0) {
            PASS("parse_basic");
            if (topo.num_grids == 1 && topo.grids[0].size == 32 &&
                strcmp(topo.grids[0].rule, "life_payload") == 0 &&
                topo.num_conns == 1 && topo.conns[0].strength == 0.5f)
                PASS("parse_fields");
            else
                FAIL("parse_fields");
        } else {
            FAIL("parse_basic");
        }
    }

    /* Test 2: empty / whitespace */
    {
        DSLTopology topo;
        if (dsl_parse("", &topo) == 0 && topo.num_grids == 0 && topo.num_conns == 0)
            PASS("parse_empty");
        else
            FAIL("parse_empty");
    }

    /* Test 3: multiple grids */
    {
        DSLTopology topo;
        const char *src = "grid 0 { name=\"a\" size=16 rule=\"diffuse\" }; grid 1 { name=\"b\" size=16 rule=\"carry_add\" }; connect 0.bottom -> 1.top;";
        if (dsl_parse(src, &topo) == 0 && topo.num_grids == 2 && topo.num_conns == 1)
            PASS("parse_multi");
        else
            FAIL("parse_multi");
    }

    printf("%d errors\n", errors);
    return errors;
}
