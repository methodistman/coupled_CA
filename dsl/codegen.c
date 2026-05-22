#include "codegen.h"
#include "../ca_core/payload_rules.h"
#include <string.h>

int codegen_emit_verilog_cell(FILE *f, const DSLTopology *topo, int grid_idx) {
    const DSLGridSpec *g = &topo->grids[grid_idx];
    fprintf(f, "// Cell module for grid %d (%s)\n", grid_idx, g->name);
    fprintf(f, "module ca_cell_%d (\n", grid_idx);
    fprintf(f, "    input  wire clk,\n");
    fprintf(f, "    input  wire rst,\n");
    fprintf(f, "    input  wire [7:0] self_alive, self_payload,\n");
    fprintf(f, "    input  wire [8*8-1:0] nb_alive, nb_payload, // 8 neighbors\n");
    fprintf(f, "    output reg  [7:0] out_alive, out_payload\n");
    fprintf(f, ");\n");
    fprintf(f, "    // Rule: %s (stub — expand per rule LUT)\n", g->rule);
    fprintf(f, "    always @(posedge clk or posedge rst) begin\n");
    fprintf(f, "        if (rst) begin out_alive <= 0; out_payload <= 0; end\n");
    fprintf(f, "        else begin\n");
    fprintf(f, "            // TODO: instantiate rule-specific logic here\n");
    fprintf(f, "            out_alive <= self_alive; // placeholder\n");
    fprintf(f, "            out_payload <= self_payload;\n");
    fprintf(f, "        end\n");
    fprintf(f, "    end\n");
    fprintf(f, "endmodule\n\n");
    return 0;
}

int codegen_emit_verilog_top(FILE *f, const DSLTopology *topo) {
    fprintf(f, "// Auto-generated coupled CA top-level\n");
    fprintf(f, "module ca_top (\n");
    fprintf(f, "    input wire clk,\n");
    fprintf(f, "    input wire rst\n");
    fprintf(f, ");\n\n");

    for (int g = 0; g < topo->num_grids; g++) {
        const DSLGridSpec *gs = &topo->grids[g];
        int sz = gs->size;
        fprintf(f, "    // Grid %d: %s (%dx%d)\n", g, gs->name, sz, sz);
        fprintf(f, "    wire [7:0] g%d_alive  [%d:0][%d:0];\n", g, sz-1, sz-1);
        fprintf(f, "    wire [7:0] g%d_payload[%d:0][%d:0];\n\n", g, sz-1, sz-1);
    }

    for (int i = 0; i < topo->num_conns; i++) {
        const DSLConnSpec *c = &topo->conns[i];
        fprintf(f, "    // Connection: %d.%s -> %d.%s (strength=%.2f)\n",
                c->src_grid, c->src_edge, c->dst_grid, c->dst_edge, c->strength);
        fprintf(f, "    // Boundary mux logic here\n\n");
    }

    fprintf(f, "endmodule\n");
    return 0;
}

int codegen_emit_c_runner(FILE *f, const DSLTopology *topo) {
    fprintf(f, "/* Auto-generated C runner from DSL topology */\n");
    fprintf(f, "#include \"../ca_core/payload_engine.h\"\n");
    fprintf(f, "#include \"../ca_core/payload_coupling.h\"\n");
    fprintf(f, "#include \"../ca_core/payload_rules.h\"\n");
    fprintf(f, "#include \"../ca_core/coupling.h\"\n");
    fprintf(f, "#include \"../utils/rng.h\"\n");
    fprintf(f, "#include <stdio.h>\n\n");
    fprintf(f, "static Edge edge_from_string(const char *s) {\n");
    fprintf(f, "    if (!__builtin_strcmp(s, \"top\"))    return EDGE_TOP;\n");
    fprintf(f, "    if (!__builtin_strcmp(s, \"right\"))  return EDGE_RIGHT;\n");
    fprintf(f, "    if (!__builtin_strcmp(s, \"bottom\")) return EDGE_BOTTOM;\n");
    fprintf(f, "    if (!__builtin_strcmp(s, \"left\"))   return EDGE_LEFT;\n");
    fprintf(f, "    return (Edge)-1;\n");
    fprintf(f, "}\n\n");
    fprintf(f, "int main(void) {\n");
    fprintf(f, "    PayloadSystem sys;\n");
    fprintf(f, "    payload_sys_init(&sys, %d, %d);\n",
            topo->num_grids,
            topo->num_grids > 0 ? topo->grids[0].size : 64);
    for (int g = 0; g < topo->num_grids; g++) {
        int ridx = payload_rules_index_by_name(topo->grids[g].rule);
        if (ridx < 0) ridx = 0;
        fprintf(f, "    sys.grids[%d]->rule_idx = %d; /* %s */\n",
                g, ridx, topo->grids[g].rule);
    }
    for (int i = 0; i < topo->num_conns; i++) {
        const DSLConnSpec *c = &topo->conns[i];
        fprintf(f, "    payload_coupling_connect(&sys.coupling, %d, edge_from_string(\"%s\"), %d, edge_from_string(\"%s\"), %.2ff);\n",
                c->src_grid, c->src_edge, c->dst_grid, c->dst_edge, c->strength);
    }
    fprintf(f, "    rng_seed(%lluULL);\n", (unsigned long long)topo->seed);
    fprintf(f, "    payload_sys_randomize(&sys, rng_u64);\n");
    fprintf(f, "    for (int step = 0; step < %d; step++) payload_sys_step(&sys);\n", topo->steps);
    fprintf(f, "    payload_sys_destroy(&sys);\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    return 0;
}
