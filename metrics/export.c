#include "export.h"

void export_csv_header(FILE *f, int ngrids) {
    fprintf(f, "step");
    for (int g = 0; g < ngrids; g++) {
        fprintf(f, ",grid%d_density,grid%d_entropy,grid%d_activity,grid%d_period", g, g, g, g);
    }
    fprintf(f, "\n");
}

void export_csv_row(FILE *f, int step, const GridMetrics *m, int ngrids) {
    fprintf(f, "%d", step);
    for (int g = 0; g < ngrids; g++) {
        fprintf(f, ",%.6f,%.6f,%.6f,%d",
                m[g].density, m[g].entropy, m[g].activity,
                m[g].detected_period);
    }
    fprintf(f, "\n");
}
