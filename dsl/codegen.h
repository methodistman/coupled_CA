#ifndef DSL_CODEGEN_H
#define DSL_CODEGEN_H

#include "parser.h"
#include <stdio.h>

/* Emit Verilog module for a single grid cell with payload support */
int codegen_emit_verilog_cell(FILE *f, const DSLTopology *topo, int grid_idx);

/* Emit Verilog top-level module interconnecting all grids */
int codegen_emit_verilog_top(FILE *f, const DSLTopology *topo);

/* Emit C simulation equivalent (for verification) */
int codegen_emit_c_runner(FILE *f, const DSLTopology *topo);

#endif
