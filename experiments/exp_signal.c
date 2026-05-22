#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../ca_core/engine.h"
#include "../ca_core/rules.h"
#include "../ca_core/pipeline.h"
#include "../metrics/history.h"
#include "../utils/rng.h"

#define DEFAULT_STEPS 1000
#define DEFAULT_SIZE  64
#define DEFAULT_GRIDS 2

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --grids N      Number of grids (1-%d, default %d)\n", MAX_GRIDS, DEFAULT_GRIDS);
    fprintf(stderr, "  --size N       Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --rule IDX     Rule index for all grids (default 0=Life)\n");
    fprintf(stderr, "  --steps N      Steps to run (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --seed N       Random seed (default: use time)\n");
    fprintf(stderr, "  --connect SPEC Add connection (repeatable). Format: SRC:EDGE->DST:EDGE\n");
    fprintf(stderr, "  --intent       Use intent-buffer coupling (one-tick delay, enables TE history)\n");
    fprintf(stderr, "  --intent-mode MODE  Intent consumption mode: replace, add, weighted, threshold\n");
    fprintf(stderr, "  --pipeline NAME Pipeline preset: default, intent, analyze, parallel\n");
    fprintf(stderr, "  --help         Show this help\n");
}

static Edge parse_edge(const char *s) {
    if (strcmp(s, "top") == 0)    return EDGE_TOP;
    if (strcmp(s, "right") == 0)  return EDGE_RIGHT;
    if (strcmp(s, "bottom") == 0) return EDGE_BOTTOM;
    if (strcmp(s, "left") == 0)   return EDGE_LEFT;
    return -1;
}

static void print_csv_header(int ngrids) {
    printf("step");
    for (int g = 0; g < ngrids; g++) {
        printf(",grid%d_density,grid%d_activity", g, g);
    }
    printf("\n");
}

static void compute_metrics(const Grid *curr, const Grid *prev, double *density, double *activity) {
    int n = curr->size * curr->size;
    int alive = 0, changed = 0;
    for (int i = 0; i < n; i++) {
        if (curr->cells[i]) alive++;
        if (prev && curr->cells[i] != prev->cells[i]) changed++;
    }
    *density = (double)alive / n;
    *activity = prev ? (double)changed / n : 0.0;
}

int main(int argc, char **argv) {
    int num_grids = DEFAULT_GRIDS;
    int size = DEFAULT_SIZE;
    int rule_idx = 0;
    int steps = DEFAULT_STEPS;
    uint64_t seed = 0;
    int seed_set = 0;
    int use_intent = 0;
    IntentMode intent_mode = INTENT_MODE_REPLACE;
    float intent_alpha = 0.5f;
    const char *pipeline_name = NULL;

    /* first pass: scalar args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--grids") == 0 && i + 1 < argc) {
            num_grids = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rule") == 0 && i + 1 < argc) {
            rule_idx = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            steps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
            seed_set = 1;
        } else if (strcmp(argv[i], "--intent") == 0) {
            use_intent = 1;
        } else if (strcmp(argv[i], "--intent-mode") == 0 && i + 1 < argc) {
            const char *m = argv[++i];
            if (strcmp(m, "replace") == 0) intent_mode = INTENT_MODE_REPLACE;
            else if (strcmp(m, "add") == 0) intent_mode = INTENT_MODE_ADD;
            else if (strcmp(m, "weighted") == 0) intent_mode = INTENT_MODE_WEIGHTED;
            else if (strcmp(m, "threshold") == 0) intent_mode = INTENT_MODE_THRESHOLD;
        } else if (strcmp(argv[i], "--pipeline") == 0 && i + 1 < argc) {
            pipeline_name = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (num_grids < 1 || num_grids > MAX_GRIDS) {
        fprintf(stderr, "Error: grids must be 1-%d\n", MAX_GRIDS);
        return 1;
    }
    if (rule_idx < 0 || rule_idx >= NUM_RULES) {
        fprintf(stderr, "Error: rule index must be 0-%d\n", NUM_RULES - 1);
        return 1;
    }
    if (size < 1 || size > MAX_GRID_SIZE) {
        fprintf(stderr, "Error: size must be 1-%d\n", MAX_GRID_SIZE);
        return 1;
    }

    System sys;
    memset(&sys, 0, sizeof(sys));
    sys_init(&sys, num_grids, size);
    for (int g = 0; g < num_grids; g++) {
        sys.grids[g]->rule_idx = rule_idx;
    }

    /* second pass: connections */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            char *spec = argv[++i];
            int src, dst;
            char src_edge[16], dst_edge[16];
            if (sscanf(spec, "%d:%8[^-]->%d:%8s", &src, src_edge, &dst, dst_edge) == 4) {
                Edge se = parse_edge(src_edge);
                Edge de = parse_edge(dst_edge);
                if (se >= 0 && de >= 0 && src < num_grids && dst < num_grids) {
                    coupling_connect(&sys.coupling, src, se, dst, de);
                } else {
                    fprintf(stderr, "Warning: ignored invalid connection '%s'\n", spec);
                }
            }
        }
    }

    if (!seed_set) {
        seed = (uint64_t)time(NULL);
    }
    rng_seed(seed);
    sys_randomize(&sys, rng_u64);

    if (pipeline_name) {
        if (strcmp(pipeline_name, "default") == 0)
            sys.pipeline = pipeline_preset_default();
        else if (strcmp(pipeline_name, "intent") == 0)
            sys.pipeline = pipeline_preset_intent(intent_mode, intent_alpha);
        else if (strcmp(pipeline_name, "analyze") == 0)
            sys.pipeline = pipeline_preset_analyze(intent_mode, intent_alpha);
        else if (strcmp(pipeline_name, "parallel") == 0)
            sys.pipeline = pipeline_preset_parallel(intent_mode, intent_alpha);
        else
            fprintf(stderr, "Warning: unknown pipeline '%s', using default\n", pipeline_name);
        if (strcmp(pipeline_name, "analyze") == 0 || strcmp(pipeline_name, "parallel") == 0)
            sys.metrics_history = metrics_history_create(num_grids, steps + 1);
    }

    printf("# seed=%llu\n", (unsigned long long)seed);
    printf("# grids=%d size=%d rule=%s steps=%d\n",
           num_grids, size, RULE_TABLE[rule_idx].name, steps);

    Grid **prev_states = calloc(num_grids, sizeof(Grid*));
    for (int g = 0; g < num_grids; g++) {
        prev_states[g] = grid_create(size);
        grid_copy(sys.grids[g], prev_states[g]);
    }

    print_csv_header(num_grids);

    for (int step = 0; step <= steps; step++) {
        double density[MAX_GRIDS], activity[MAX_GRIDS];
        for (int g = 0; g < num_grids; g++) {
            compute_metrics(sys.grids[g], prev_states[g], &density[g], &activity[g]);
        }

        printf("%d", step);
        for (int g = 0; g < num_grids; g++) {
            printf(",%.6f,%.6f", density[g], activity[g]);
        }
        printf("\n");

        if (step < steps) {
            for (int g = 0; g < num_grids; g++) {
                grid_copy(sys.grids[g], prev_states[g]);
            }
            if (pipeline_name) {
                sys_step(&sys);
            } else if (use_intent) {
                sys_step_intent(&sys, intent_mode, intent_alpha);
            } else {
                sys_step(&sys);
            }
        }
    }

    for (int g = 0; g < num_grids; g++) {
        grid_destroy(prev_states[g]);
    }
    free(prev_states);
    sys_destroy(&sys);

    return 0;
}
