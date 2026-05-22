#define _GNU_SOURCE
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int parse_kw(const char **pp, const char *kw) {
    const char *p = skip_ws(*pp);
    size_t n = strlen(kw);
    if (strncasecmp(p, kw, n) == 0 && !isalnum((unsigned char)p[n])) {
        *pp = p + n;
        return 1;
    }
    return 0;
}

static int parse_int(const char **pp, int *out) {
    const char *p = skip_ws(*pp);
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;
    *out = (int)v;
    *pp = end;
    return 1;
}

static int parse_float(const char **pp, float *out) {
    const char *p = skip_ws(*pp);
    char *end;
    float v = strtof(p, &end);
    if (end == p) return 0;
    *out = v;
    *pp = end;
    return 1;
}

static int parse_ident(const char **pp, char *out, size_t max) {
    const char *p = skip_ws(*pp);
    size_t n = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-') && n + 1 < max) {
        out[n++] = *p++;
    }
    if (n == 0) return 0;
    out[n] = '\0';
    *pp = p;
    return 1;
}

static int parse_string(const char **pp, char *out, size_t max) {
    const char *p = skip_ws(*pp);
    if (*p != '"') return 0;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < max) {
        out[n++] = *p++;
    }
    if (*p != '"') return 0;
    out[n] = '\0';
    *pp = p + 1;
    return 1;
}

static int expect(const char **pp, char c) {
    const char *p = skip_ws(*pp);
    if (*p == c) { *pp = p + 1; return 1; }
    *pp = p; /* advance past whitespace even on failure */
    return 0;
}

int dsl_parse(const char *text, DSLTopology *out) {
    memset(out, 0, sizeof(*out));
    const char *p = text;
    while (*p) {
        p = skip_ws(p);
        if (*p == '\0') break;
        if (*p == '#') { while (*p && *p != '\n') p++; continue; }
        if (*p == ';') { p++; continue; }

        if (parse_kw(&p, "grid")) {
            int idx;
            if (!parse_int(&p, &idx) || idx < 0 || idx >= DSL_MAX_GRIDS) return -1;
            if (!expect(&p, '{')) return -1;
            while (*p && *p != '}') {
                char key[DSL_NAME_LEN];
                if (!parse_ident(&p, key, sizeof(key))) return -1;
                if (!expect(&p, '=')) return -1;
                if (!strcmp(key, "name")) {
                    if (!parse_string(&p, out->grids[idx].name, DSL_NAME_LEN)) return -1;
                } else if (!strcmp(key, "size")) {
                    if (!parse_int(&p, &out->grids[idx].size)) return -1;
                } else if (!strcmp(key, "rule")) {
                    if (!parse_string(&p, out->grids[idx].rule, DSL_NAME_LEN)) return -1;
                } else if (!strcmp(key, "type")) {
                    char t[4];
                    if (!parse_string(&p, t, sizeof(t))) return -1;
                    out->grids[idx].type = t[0];
                }
                if (expect(&p, ',')) continue;
            }
            if (!expect(&p, '}')) return -1;
            if (idx >= out->num_grids) out->num_grids = idx + 1;
        }
        else if (parse_kw(&p, "xconnect")) {
            DSLXConnSpec *c = &out->xconns[out->num_xconns];
            char src_type_str[4], dst_type_str[4];
            if (!parse_string(&p, src_type_str, sizeof(src_type_str))) return -1;
            c->src_type = src_type_str[0];
            if (!parse_int(&p, &c->src_grid)) return -1;
            if (!expect(&p, '.')) return -1;
            if (!parse_ident(&p, c->src_edge, sizeof(c->src_edge))) return -1;
            if (!parse_kw(&p, "->")) return -1;
            if (!parse_string(&p, dst_type_str, sizeof(dst_type_str))) return -1;
            c->dst_type = dst_type_str[0];
            if (!parse_int(&p, &c->dst_grid)) return -1;
            if (!expect(&p, '.')) return -1;
            if (!parse_ident(&p, c->dst_edge, sizeof(c->dst_edge))) return -1;
            out->num_xconns++;
        }
        else if (parse_kw(&p, "connect")) {
            DSLConnSpec *c = &out->conns[out->num_conns];
            if (!parse_int(&p, &c->src_grid)) return -1;
            if (!expect(&p, '.')) return -1;
            if (!parse_ident(&p, c->src_edge, sizeof(c->src_edge))) return -1;
            if (!parse_kw(&p, "->")) return -1;
            if (!parse_int(&p, &c->dst_grid)) return -1;
            if (!expect(&p, '.')) return -1;
            if (!parse_ident(&p, c->dst_edge, sizeof(c->dst_edge))) return -1;
            if (expect(&p, ',')) {
                if (!parse_float(&p, &c->strength)) return -1;
            } else {
                c->strength = 1.0f;
            }
            out->num_conns++;
        }
        else if (parse_kw(&p, "steps")) {
            if (!parse_int(&p, &out->steps)) return -1;
        }
        else if (parse_kw(&p, "seed")) {
            char buf[32];
            if (!parse_string(&p, buf, sizeof(buf))) return -1;
            out->seed = strtoull(buf, NULL, 10);
        }
        else {
            return -1; /* unknown token */
        }
        expect(&p, ';');
    }
    return 0;
}

int dsl_parse_file(const char *path, DSLTopology *out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    int rc = dsl_parse(buf, out);
    free(buf);
    return rc;
}
