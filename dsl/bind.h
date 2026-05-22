#ifndef DSL_BIND_H
#define DSL_BIND_H

#include "parser.h"
#include "../ca_core/engine.h"
#include "../ca_core/payload_engine.h"
#include "../ca_core/hybrid_engine.h"

/* Map a DSL topology to a binary System. All grids must share the same size.
   Returns 0 on success, -1 on error (mismatched sizes, unknown rule, bad edge). */
int dsl_bind_system(const DSLTopology *topo, System *sys);

/* Map a DSL topology to a PayloadSystem. All grids must share the same size.
   Returns 0 on success, -1 on error. */
int dsl_bind_payload_system(const DSLTopology *topo, PayloadSystem *sys);

/* Map a DSL topology to a HybridSystem.  Grids with type 'B' go to binary;
   type 'P' go to payload.  Cross-type xconnects are wired via hybrid_xconn_add.
   Returns 0 on success, -1 on error. */
int dsl_bind_hybrid_system(const DSLTopology *topo, HybridSystem *sys);

#endif
