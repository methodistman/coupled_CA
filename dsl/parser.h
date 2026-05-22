#ifndef DSL_PARSER_H
#define DSL_PARSER_H

#include "../ca_core/cell.h"

#define DSL_MAX_GRIDS 8
#define DSL_MAX_CONNS 16
#define DSL_MAX_XCONNS 16
#define DSL_NAME_LEN  32

typedef struct {
    char name[DSL_NAME_LEN];
    int size;
    char rule[DSL_NAME_LEN];
    char type; /* 'B' = binary (default), 'P' = payload */
} DSLGridSpec;

typedef struct {
    int src_grid;
    char src_edge[DSL_NAME_LEN];
    int dst_grid;
    char dst_edge[DSL_NAME_LEN];
    float strength;
} DSLConnSpec;

typedef struct {
    char src_type; /* 'B' or 'P' */
    int src_grid;
    char src_edge[DSL_NAME_LEN];
    char dst_type; /* 'B' or 'P' */
    int dst_grid;
    char dst_edge[DSL_NAME_LEN];
} DSLXConnSpec;

typedef struct {
    DSLGridSpec grids[DSL_MAX_GRIDS];
    int num_grids;
    DSLConnSpec conns[DSL_MAX_CONNS];
    int num_conns;
    DSLXConnSpec xconns[DSL_MAX_XCONNS];
    int num_xconns;
    int steps;
    uint64_t seed;
} DSLTopology;

/* Parse a simple topology script. Returns 0 on success, -1 on error. */
int dsl_parse(const char *text, DSLTopology *out);

/* Convenience: parse from file */
int dsl_parse_file(const char *path, DSLTopology *out);

#endif
