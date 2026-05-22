#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../ca_core/hybrid_engine.h"
#include "../ca_core/coupling.h"
#include "../ca_core/rules.h"
#include "../ca_core/payload_rules.h"
#include "../ca_core/payload_grid.h"
#include "../utils/rng.h"
#include "../metrics/metrics.h"
#include "../metrics/glossary.h"

#define DEFAULT_SIZE 32
#define DEFAULT_STEPS 200
#define DEFAULT_TRIALS 4
#define DEFAULT_DELAY 1
#define DEFAULT_RIDGE_LAMBDA 0.01
#define MAX_FEATURES 256

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  --size N       Grid size (default %d)\n", DEFAULT_SIZE);
    fprintf(stderr, "  --steps N      Steps per trial (default %d)\n", DEFAULT_STEPS);
    fprintf(stderr, "  --trials N     Number of seeds (default %d)\n", DEFAULT_TRIALS);
    fprintf(stderr, "  --seed N       Base random seed (default: time)\n");
    fprintf(stderr, "  --brule NAME   Binary rule (default Conway's Life)\n");
    fprintf(stderr, "  --prule NAME   Payload rule (default life_payload)\n");
    fprintf(stderr, "  --xconnect SPEC  Cross-type edge coupling (default none)\n");
    fprintf(stderr, "  --delay N      Target delay for memory task (default %d)\n", DEFAULT_DELAY);
    fprintf(stderr, "  --ridge L      Ridge regression lambda (default %.2f)\n", DEFAULT_RIDGE_LAMBDA);
    fprintf(stderr, "  --features N   Reservoir features to sample (default 64)\n");
    fprintf(stderr, "  --task MODE    Task: delay|xor (default delay)\n");
    fprintf(stderr, "  --output-perf FILE  Write task performance CSV (default stdout only)\n");
    fprintf(stderr, "  --help         Show this help\n");
}

static Edge edge_from_string(const char *s) {
    if (!strcmp(s, "top"))    return EDGE_TOP;
    if (!strcmp(s, "right"))  return EDGE_RIGHT;
    if (!strcmp(s, "bottom")) return EDGE_BOTTOM;
    if (!strcmp(s, "left"))   return EDGE_LEFT;
    return -1;
}

/* --------------------------------------------------------------------------
 * Ridge regression solver (Gaussian elimination on normal equations)
 * Solves (A + lambda*I) w = b  where A = X'X, b = X'y
 * n = dimension (n_features), A is n×n symmetric positive-definite
 * -------------------------------------------------------------------------- */
static int solve_ridge(const double *A_in, const double *b_in, int n, double lambda, double *w) {
    double A[256 * 256]; /* MAX_FEATURES^2 */
    double b[256];
    if (n > 256) return -1;
    memcpy(A, A_in, n * n * sizeof(double));
    memcpy(b, b_in, n * sizeof(double));
    for (int i = 0; i < n; i++) A[i * n + i] += lambda;

    /* Gaussian elimination with partial pivoting */
    for (int col = 0; col < n; col++) {
        int pivot = col;
        double max = fabs(A[col * n + col]);
        for (int row = col + 1; row < n; row++) {
            double v = fabs(A[row * n + col]);
            if (v > max) { max = v; pivot = row; }
        }
        if (max < 1e-12) return -1; /* singular */
        if (pivot != col) {
            for (int k = col; k < n; k++) {
                double t = A[col * n + k]; A[col * n + k] = A[pivot * n + k]; A[pivot * n + k] = t;
            }
            double t = b[col]; b[col] = b[pivot]; b[pivot] = t;
        }
        double div = A[col * n + col];
        for (int k = col; k < n; k++) A[col * n + k] /= div;
        b[col] /= div;
        for (int row = 0; row < n; row++) {
            if (row == col) continue;
            double factor = A[row * n + col];
            if (fabs(factor) < 1e-15) continue;
            for (int k = col; k < n; k++) A[row * n + k] -= factor * A[col * n + k];
            b[row] -= factor * b[col];
        }
    }
    memcpy(w, b, n * sizeof(double));
    return 0;
}

/* Sample reservoir features from binary alive cells and payload alive cells.
   Features are taken from a deterministic stride across the flattened grids. */
static void sample_features(const HybridSystem *hs, double *feat, int n_feat, int n_cells) {
    int stride = n_cells / n_feat;
    if (stride < 1) stride = 1;
    int f = 0;
    for (int i = 0; i < n_cells && f < n_feat; i += stride) {
        feat[f++] = hs->binary.grids[0]->cells[i] ? 1.0 : 0.0;
    }
    if (f < n_feat) {
        int n_payload = n_cells;
        int stride_p = n_payload / (n_feat - f);
        if (stride_p < 1) stride_p = 1;
        for (int i = 0; i < n_payload && f < n_feat; i += stride_p) {
            feat[f++] = hs->payload.grids[0]->cells[i].alive ? 1.0 : 0.0;
        }
    }
    while (f < n_feat) feat[f++] = 0.0;
}

/* Drive a binary input into the left edge of the binary grid */
static void drive_input(HybridSystem *hs, int input_bit) {
    int sz = hs->binary.grids[0]->size;
    for (int y = 0; y < sz; y++) {
        int val = input_bit ? 1 : 0;
        /* Overwrite left edge with input bit */
        hs->binary.grids[0]->cells[y * sz] = val;
    }
}

static double nrmse(const double *y_true, const double *y_pred, int n) {
    double mean = 0, mse = 0;
    for (int i = 0; i < n; i++) mean += y_true[i];
    mean /= n;
    double var = 0;
    for (int i = 0; i < n; i++) { double d = y_true[i] - mean; var += d * d; }
    for (int i = 0; i < n; i++) { double d = y_true[i] - y_pred[i]; mse += d * d; }
    return (var < 1e-12) ? 0.0 : sqrt(mse / n) / sqrt(var / n);
}

int main(int argc, char **argv) {
    int size = DEFAULT_SIZE;
    int steps = DEFAULT_STEPS;
    int trials = DEFAULT_TRIALS;
    uint64_t base_seed = (uint64_t)time(NULL);
    const char *brule_name = "Conway's Life";
    const char *prule_name = "life_payload";
    int has_xconn = 0;
    HybridGridType xconn_st = HYBRID_BINARY, xconn_dt = HYBRID_BINARY;
    int xconn_sg = 0, xconn_dg = 0;
    Edge xconn_se = EDGE_TOP, xconn_de = EDGE_TOP;
    int delay = DEFAULT_DELAY;
    double ridge_lambda = DEFAULT_RIDGE_LAMBDA;
    int n_features = 64;
    const char *task_name = "delay";
    const char *perf_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--trials") && i + 1 < argc) trials = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) base_seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--brule") && i + 1 < argc) brule_name = argv[++i];
        else if (!strcmp(argv[i], "--prule") && i + 1 < argc) prule_name = argv[++i];
        else if (!strcmp(argv[i], "--delay") && i + 1 < argc) delay = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ridge") && i + 1 < argc) ridge_lambda = atof(argv[++i]);
        else if (!strcmp(argv[i], "--features") && i + 1 < argc) n_features = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--task") && i + 1 < argc) task_name = argv[++i];
        else if (!strcmp(argv[i], "--output-perf") && i + 1 < argc) perf_file = argv[++i];
        else if (!strcmp(argv[i], "--xconnect") && i + 1 < argc) {
            const char *spec = argv[++i];
            char src_t, dst_t;
            char src_e[16], dst_e[16];
            if (sscanf(spec, "%c%d:%15[^-]->%c%d:%15s",
                       &src_t, &xconn_sg, src_e, &dst_t, &xconn_dg, dst_e) == 6) {
                xconn_st = (src_t == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
                xconn_dt = (dst_t == 'P') ? HYBRID_PAYLOAD : HYBRID_BINARY;
                xconn_se = edge_from_string(src_e);
                xconn_de = edge_from_string(dst_e);
                has_xconn = 1;
            }
        }
    }

    int brule_idx = rules_index_by_name(brule_name);
    int prule_idx = payload_rules_index_by_name(prule_name);
    if (brule_idx < 0) { fprintf(stderr, "Unknown binary rule: %s\n", brule_name); return 1; }
    if (prule_idx < 0) { fprintf(stderr, "Unknown payload rule: %s\n", prule_name); return 1; }
    if (n_features < 2) n_features = 2;
    if (n_features > MAX_FEATURES) n_features = MAX_FEATURES;

    int n_cells = size * size;
    int washout = steps / 4;
    if (washout < 10) washout = 10;
    int n_train = (steps - washout) / 2;
    int n_test = steps - washout - n_train;
    if (n_train < 10) { fprintf(stderr, "Need more steps for train/test split\n"); return 1; }

    printf("# H9: Reservoir Computing with Ridge Regression Readout\n");
    printf("# size=%d steps=%d trials=%d brule=%s prule=%s\n",
           size, steps, trials, brule_name, prule_name);
    printf("# task=%s delay=%d ridge_lambda=%.4f features=%d\n",
           task_name, delay, ridge_lambda, n_features);
    printf("# washout=%d n_train=%d n_test=%d\n", washout, n_train, n_test);
    printf("trial,seed,train_nrmse,test_nrmse\n");

    FILE *perf_fp = NULL;
    if (perf_file) {
        perf_fp = fopen(perf_file, "w");
        if (!perf_fp) { fprintf(stderr, "Cannot open %s\n", perf_file); return 1; }
        fprintf(perf_fp, "# H9: Reservoir Computing with Ridge Regression Readout\n");
        fprintf(perf_fp, "# size=%d steps=%d trials=%d brule=%s prule=%s\n",
                size, steps, trials, brule_name, prule_name);
        fprintf(perf_fp, "# task=%s delay=%d ridge_lambda=%.4f features=%d\n",
                task_name, delay, ridge_lambda, n_features);
        fprintf(perf_fp, "# washout=%d n_train=%d n_test=%d\n", washout, n_train, n_test);
        fprintf(perf_fp, "trial,seed,train_nrmse,test_nrmse\n");
    }

    double *X_train = malloc((size_t)n_train * n_features * sizeof(double));
    double *y_train = malloc((size_t)n_train * sizeof(double));
    double *X_test  = malloc((size_t)n_test  * n_features * sizeof(double));
    double *y_test  = malloc((size_t)n_test  * sizeof(double));
    double *y_pred  = malloc((size_t)n_test  * sizeof(double));
    if (!X_train || !y_train || !X_test || !y_test || !y_pred) {
        fprintf(stderr, "Allocation failed\n"); return 1;
    }

    int task_xor = !strcmp(task_name, "xor");

    for (int t = 0; t < trials; t++) {
        uint64_t seed = base_seed + (uint64_t)t;
        rng_seed(seed);

        HybridSystem hs;
        hybrid_sys_init(&hs, 1, 1, 0, size);
        hs.binary.grids[0]->rule_idx = brule_idx;
        hs.payload.grids[0]->rule_idx = prule_idx;
        hybrid_sys_randomize(&hs, rng_u64);
        if (has_xconn)
            hybrid_xconn_add(&hs, xconn_st, xconn_sg, xconn_se, xconn_dt, xconn_dg, xconn_de);

        /* Generate input sequence and compute target */
        uint8_t *inputs = malloc((size_t)steps * sizeof(uint8_t));
        double *targets = malloc((size_t)steps * sizeof(double));
        for (int s = 0; s < steps; s++) {
            inputs[s] = (rng_u64() & 1u) ? 1 : 0;
            if (task_xor) {
                int d1 = (s >= 1) ? inputs[s-1] : 0;
                int d2 = (s >= delay) ? inputs[s-delay] : 0;
                targets[s] = (double)(d1 ^ d2);
            } else {
                targets[s] = (s >= delay) ? (double)inputs[s-delay] : 0.0;
            }
        }

        /* Run reservoir, sampling states */
        int sample_idx = 0;
        for (int s = 0; s < steps; s++) {
            drive_input(&hs, inputs[s]);
            hybrid_sys_step(&hs);
            if (s >= washout) {
                double *X = (sample_idx < n_train) ? &X_train[sample_idx * n_features]
                                                    : &X_test[(sample_idx - n_train) * n_features];
                double *y = (sample_idx < n_train) ? &y_train[sample_idx]
                                                    : &y_test[sample_idx - n_train];
                sample_features(&hs, X, n_features, n_cells);
                *y = targets[s];
                sample_idx++;
            }
        }

        /* Ridge regression: solve (X'X + lambda*I) w = X'y */
        double A[256 * 256] = {0};
        double b[256] = {0};
        for (int i = 0; i < n_features; i++) {
            for (int j = 0; j < n_features; j++) {
                double sum = 0;
                for (int k = 0; k < n_train; k++) sum += X_train[k * n_features + i] * X_train[k * n_features + j];
                A[i * n_features + j] = sum;
            }
            double sum = 0;
            for (int k = 0; k < n_train; k++) sum += X_train[k * n_features + i] * y_train[k];
            b[i] = sum;
        }
        double w[256];
        if (solve_ridge(A, b, n_features, ridge_lambda, w) != 0) {
            fprintf(stderr, "Ridge solve failed for trial %d\n", t);
            printf("%d,%llu,NA,NA\n", t, (unsigned long long)seed);
            free(inputs); free(targets); hybrid_sys_destroy(&hs);
            continue;
        }

        /* Evaluate on train and test */
        double train_mse = 0, test_mse = 0;
        for (int k = 0; k < n_train; k++) {
            double pred = 0;
            for (int i = 0; i < n_features; i++) pred += X_train[k * n_features + i] * w[i];
            double d = pred - y_train[k]; train_mse += d * d;
        }
        for (int k = 0; k < n_test; k++) {
            double pred = 0;
            for (int i = 0; i < n_features; i++) pred += X_test[k * n_features + i] * w[i];
            y_pred[k] = pred;
            double d = pred - y_test[k]; test_mse += d * d;
        }
        /* Recompute NRMSE properly: need predictions for train too */
        double *y_train_pred = malloc((size_t)n_train * sizeof(double));
        for (int k = 0; k < n_train; k++) {
            double pred = 0;
            for (int i = 0; i < n_features; i++) pred += X_train[k * n_features + i] * w[i];
            y_train_pred[k] = pred;
        }
        double train_err = nrmse(y_train, y_train_pred, n_train);
        double test_err = nrmse(y_test, y_pred, n_test);
        free(y_train_pred);

        printf("%d,%llu,%.6f,%.6f\n", t, (unsigned long long)seed, train_err, test_err);
        if (perf_fp)
            fprintf(perf_fp, "%d,%llu,%.6f,%.6f\n", t, (unsigned long long)seed, train_err, test_err);

        free(inputs); free(targets);
        hybrid_sys_destroy(&hs);
    }

    if (perf_fp) fclose(perf_fp);
    free(X_train); free(y_train); free(X_test); free(y_test); free(y_pred);
    return 0;
}
