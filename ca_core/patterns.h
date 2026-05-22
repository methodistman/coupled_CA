#ifndef PATTERNS_H
#define PATTERNS_H

#include "grid.h"

/*
 * Pattern library for Conway's Life.
 * Each function stamps a pattern at (x0, y0) on a grid.
 * Patterns are given as minimal bounding-box coordinates.
 */

/* Spaceships */
void pattern_glider(Grid *g, int x0, int y0);
void pattern_glider_rev(Grid *g, int x0, int y0); /* reflected */
void pattern_lwss(Grid *g, int x0, int y0);

/* Oscillators */
void pattern_blinker(Grid *g, int x0, int y0);
void pattern_blinker_v(Grid *g, int x0, int y0); /* vertical */
void pattern_beacon(Grid *g, int x0, int y0);
void pattern_toad(Grid *g, int x0, int y0);
void pattern_pulsar(Grid *g, int x0, int y0);

/* Guns & breeders */
void pattern_glider_gun(Grid *g, int x0, int y0);

/* Still lifes */
void pattern_block(Grid *g, int x0, int y0);
void pattern_beehive(Grid *g, int x0, int y0);

/* Eaters & reflectors */
void pattern_eater(Grid *g, int x0, int y0);

/* Utility: rotate a pattern 90 degrees */
void pattern_rotate_90(int *x, int *y, int w, int h);

/* Glider census: count known patterns in a grid */
void patterns_set_strict(int strict);
int census_gliders(const Grid *g);
int census_oscillators(const Grid *g);

#endif
