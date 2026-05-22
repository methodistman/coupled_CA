#ifndef CELL_H
#define CELL_H

#include <stdint.h>

typedef struct {
    uint8_t alive;
    uint8_t payload;
} Cell;

#endif
