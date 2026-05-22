#ifndef EXPORT_H
#define EXPORT_H

#include <stdio.h>
#include "metrics.h"

void export_csv_header(FILE *f, int ngrids);
void export_csv_row(FILE *f, int step, const GridMetrics *m, int ngrids);

#endif
