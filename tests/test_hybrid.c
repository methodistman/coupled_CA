#include <stdio.h>
#include <string.h>
#include "../ca_core/hybrid_engine.h"
#include "../utils/rng.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", msg); failures++; } \
    else fprintf(stderr, "  PASS: %s\n", msg); \
} while (0)

int main(void) {
    /* Test 1: Basic create/destroy */
    HybridSystem hs;
    hybrid_sys_init(&hs, 1, 1, 0, 16);
    CHECK(hs.binary.num_grids == 1, "binary.num_grids == 1");
    CHECK(hs.payload.num_grids == 1, "payload.num_grids == 1");
    CHECK(hs.n_xconns == 0, "n_xconns == 0");
    hybrid_sys_destroy(&hs);
    CHECK(1, "hybrid_sys_destroy ok");

    /* Test 2: Randomize both subsystems */
    hybrid_sys_init(&hs, 1, 1, 0, 16);
    rng_seed(42);
    hybrid_sys_randomize(&hs, rng_u64);
    int b_has_data = 0, p_has_data = 0;
    for (int i = 0; i < 16*16; i++) {
        if (hs.binary.grids[0]->cells[i]) b_has_data = 1;
        if (hs.payload.grids[0]->cells[i].alive) p_has_data = 1;
    }
    CHECK(b_has_data, "binary grid has data after randomize");
    CHECK(p_has_data, "payload grid has data after randomize");
    hybrid_sys_destroy(&hs);

    /* Test 3: Cross-connection binary->payload edge transfer */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    /* Use identity rule so transferred values survive payload step */
    hs.payload.grids[0]->rule_idx = payload_rules_index_by_name("identity");
    /* Set binary bottom edge to all 1s */
    for (int i = 0; i < 8; i++)
        hs.binary.grids[0]->cells[7*8 + i] = 1;

    hybrid_xconn_add(&hs, HYBRID_BINARY, 0, EDGE_BOTTOM,
                     HYBRID_PAYLOAD, 0, EDGE_TOP);
    CHECK(hs.n_xconns == 1, "xconn added");

    hybrid_sys_step(&hs);
    /* After step, payload top edge should have received binary bottom edge */
    int all_255 = 1;
    for (int i = 0; i < 8; i++) {
        Cell c = hs.payload.grids[0]->cells[i];
        if (c.alive != 1 || c.payload != 255) all_255 = 0;
    }
    CHECK(all_255, "payload top edge received binary bottom (alive=1,payload=255)");
    hybrid_sys_destroy(&hs);

    /* Test 4: Direct payload->binary edge transfer */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    for (int i = 0; i < 8; i++)
        hs.payload.grids[0]->cells[i*8] = (Cell){1, 200};

    transfer_payload_to_binary(hs.payload.grids[0], hs.binary.grids[0],
                               EDGE_LEFT, EDGE_RIGHT);
    int all_1 = 1;
    for (int i = 0; i < 8; i++) {
        if (hs.binary.grids[0]->cells[i*8 + 7] != 1) all_1 = 0;
    }
    CHECK(all_1, "binary right edge received payload left (payload>127 => 1)");
    hybrid_sys_destroy(&hs);

    /* Test 5: Direct payload->binary with low payload (should map to 0) */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    for (int i = 0; i < 8; i++)
        hs.payload.grids[0]->cells[i*8] = (Cell){1, 50}; /* payload < 127 */

    transfer_payload_to_binary(hs.payload.grids[0], hs.binary.grids[0],
                               EDGE_LEFT, EDGE_RIGHT);
    int all_0 = 1;
    for (int i = 0; i < 8; i++) {
        if (hs.binary.grids[0]->cells[i*8 + 7] != 0) all_0 = 0;
    }
    CHECK(all_0, "binary right edge received payload left (payload<127 => 0)");
    hybrid_sys_destroy(&hs);

    /* Test 6: Multiple cross-connections */
    hybrid_sys_init(&hs, 2, 2, 0, 8);
    hybrid_xconn_add(&hs, HYBRID_BINARY, 0, EDGE_BOTTOM, HYBRID_PAYLOAD, 0, EDGE_TOP);
    hybrid_xconn_add(&hs, HYBRID_BINARY, 1, EDGE_TOP, HYBRID_PAYLOAD, 1, EDGE_BOTTOM);
    CHECK(hs.n_xconns == 2, "two xconns added");
    hybrid_sys_destroy(&hs);

    /* Test 7: hybrid_coupling_read internal cell */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    hs.binary.grids[0]->cells[4*8 + 3] = 1;
    hs.payload.grids[0]->cells[2*8 + 5] = (Cell){1, 42};
    Cell c1 = hybrid_coupling_read(&hs, HYBRID_BINARY, 0, 3, 4, 0, 0);
    Cell c2 = hybrid_coupling_read(&hs, HYBRID_PAYLOAD, 0, 5, 2, 0, 0);
    CHECK(c1.alive == 1 && c1.payload == 255, "read binary internal (alive=1,payload=255)");
    CHECK(c2.alive == 1 && c2.payload == 42, "read payload internal (alive=1,payload=42)");
    hybrid_sys_destroy(&hs);

    /* Test 8: hybrid_coupling_read across xconn boundary (binary->payload) */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    hybrid_xconn_add(&hs, HYBRID_BINARY, 0, EDGE_BOTTOM, HYBRID_PAYLOAD, 0, EDGE_TOP);
    /* Set payload top row so cross-read has something to return */
    for (int i = 0; i < 8; i++)
        hs.payload.grids[0]->cells[i] = (Cell){1, 77};
    /* Read "below" the bottom edge of binary -> should see payload top via xconn */
    Cell c3 = hybrid_coupling_read(&hs, HYBRID_BINARY, 0, 4, 7, 0, 1);
    CHECK(c3.alive == 1 && c3.payload == 77, "cross-read binary->payload via bottom edge");
    hybrid_sys_destroy(&hs);

    /* Test 9: hybrid_coupling_read payload->binary across xconn */
    hybrid_sys_init(&hs, 1, 1, 0, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    hybrid_xconn_add(&hs, HYBRID_PAYLOAD, 0, EDGE_LEFT, HYBRID_BINARY, 0, EDGE_RIGHT);
    for (int i = 0; i < 8; i++)
        hs.binary.grids[0]->cells[i*8 + 7] = 1;
    Cell c4 = hybrid_coupling_read(&hs, HYBRID_PAYLOAD, 0, 0, 3, -1, 0);
    CHECK(c4.alive == 1 && c4.payload == 255, "cross-read payload->binary via left edge");
    hybrid_sys_destroy(&hs);

    /* Test 10: hybrid_coupling_read with HYBRID_WAVE source — internal cell.
       Wave projects to Cell as {alive, low 8 bits of wave}. */
    hybrid_sys_init(&hs, 0, 0, 1, 8);
    memset(hs.wave.grids[0]->cells, 0, 8*8*sizeof(WaveCell));
    hs.wave.grids[0]->cells[3*8 + 4].alive = 1;
    hs.wave.grids[0]->cells[3*8 + 4].wave = 0xABu;  /* low byte = 0xAB */
    Cell c5 = hybrid_coupling_read(&hs, HYBRID_WAVE, 0, 4, 3, 0, 0);
    CHECK(c5.alive == 1 && c5.payload == 0xAB, "wave internal read projects to {alive, low8(wave)}");
    hybrid_sys_destroy(&hs);

    /* Test 11: hybrid_coupling_read across wave->binary xconn. */
    hybrid_sys_init(&hs, 1, 0, 1, 8);
    memset(hs.binary.grids[0]->cells, 0, 8*8);
    memset(hs.wave.grids[0]->cells, 0, 8*8*sizeof(WaveCell));
    hybrid_xconn_add(&hs, HYBRID_WAVE, 0, EDGE_BOTTOM, HYBRID_BINARY, 0, EDGE_TOP);
    for (int i = 0; i < 8; i++) hs.binary.grids[0]->cells[i] = 1;
    /* Read "below" wave bottom edge -> should see binary top row via xconn */
    Cell c6 = hybrid_coupling_read(&hs, HYBRID_WAVE, 0, 4, 7, 0, 1);
    CHECK(c6.alive == 1 && c6.payload == 255, "cross-read wave->binary via bottom edge");
    hybrid_sys_destroy(&hs);

    /* Test 12: hybrid_coupling_read across wave->payload xconn. */
    hybrid_sys_init(&hs, 0, 1, 1, 8);
    memset(hs.payload.grids[0]->cells, 0, 8*8*sizeof(Cell));
    memset(hs.wave.grids[0]->cells, 0, 8*8*sizeof(WaveCell));
    hybrid_xconn_add(&hs, HYBRID_WAVE, 0, EDGE_LEFT, HYBRID_PAYLOAD, 0, EDGE_RIGHT);
    for (int i = 0; i < 8; i++) hs.payload.grids[0]->cells[i*8 + 7] = (Cell){1, 99};
    Cell c7 = hybrid_coupling_read(&hs, HYBRID_WAVE, 0, 0, 3, -1, 0);
    CHECK(c7.alive == 1 && c7.payload == 99, "cross-read wave->payload via left edge");
    hybrid_sys_destroy(&hs);

    fprintf(stderr, "%d errors\n", failures);
    return failures ? 1 : 0;
}
