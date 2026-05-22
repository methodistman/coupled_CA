#include "patterns.h"
#include <string.h>

/* Helper: stamp a row of bits */
static void stamp_row(Grid *g, int x0, int y0, const char *row) {
    for (int x = 0; row[x]; x++)
        if (row[x] == 'O')
            grid_set(g, x0 + x, y0, 1);
}

/* --- Spaceships --- */

/* Standard phase-0 glider (SE direction) */
void pattern_glider(Grid *g, int x0, int y0) {
    grid_set(g, x0+1, y0+0, 1);
    grid_set(g, x0+2, y0+1, 1);
    grid_set(g, x0+0, y0+2, 1);
    grid_set(g, x0+1, y0+2, 1);
    grid_set(g, x0+2, y0+2, 1);
}

void pattern_glider_rev(Grid *g, int x0, int y0) {
    grid_set(g, x0+1, y0+0, 1);
    grid_set(g, x0+0, y0+1, 1);
    grid_set(g, x0+2, y0+2, 1);
    grid_set(g, x0+1, y0+2, 1);
    grid_set(g, x0+0, y0+2, 1);
}

void pattern_lwss(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        "..O...",
        "O...O.",
        ".....O",
        "O....O",
        ".OOOOO",
    };
    for (int y = 0; y < 5; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

/* --- Oscillators --- */

void pattern_blinker(Grid *g, int x0, int y0) {
    grid_set(g, x0+0, y0, 1);
    grid_set(g, x0+1, y0, 1);
    grid_set(g, x0+2, y0, 1);
}

void pattern_blinker_v(Grid *g, int x0, int y0) {
    grid_set(g, x0, y0+0, 1);
    grid_set(g, x0, y0+1, 1);
    grid_set(g, x0, y0+2, 1);
}

void pattern_beacon(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        "OO..",
        "O...",
        "...O",
        "..OO",
    };
    for (int y = 0; y < 4; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

void pattern_toad(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        ".OOO",
        "OOO.",
    };
    for (int y = 0; y < 2; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

void pattern_pulsar(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        "..OOO...OOO..",
        ".............",
        "O....O.O....O",
        "O....O.O....O",
        "O....O.O....O",
        "..OOO...OOO..",
        ".............",
        "..OOO...OOO..",
        "O....O.O....O",
        "O....O.O....O",
        "O....O.O....O",
        ".............",
        "..OOO...OOO..",
    };
    for (int y = 0; y < 13; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

/* --- Still lifes --- */

void pattern_block(Grid *g, int x0, int y0) {
    grid_set(g, x0,   y0,   1);
    grid_set(g, x0+1, y0,   1);
    grid_set(g, x0,   y0+1, 1);
    grid_set(g, x0+1, y0+1, 1);
}

void pattern_beehive(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        ".OO.",
        "O..O",
        "O..O",
        ".OO.",
    };
    for (int y = 0; y < 4; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

/* --- Eater --- */

void pattern_eater(Grid *g, int x0, int y0) {
    static const char *rows[] = {
        "OO",
        "O.",
        ".O",
        ".OO",
    };
    for (int y = 0; y < 4; y++)
        stamp_row(g, x0, y0 + y, rows[y]);
}

/* --- Glider gun (Gosper) --- */

void pattern_glider_gun(Grid *g, int x0, int y0) {
    /* Left block */
    grid_set(g, x0+0, y0+4, 1); grid_set(g, x0+0, y0+5, 1);
    grid_set(g, x0+1, y0+4, 1); grid_set(g, x0+1, y0+5, 1);
    /* Left shuttle */
    grid_set(g, x0+10, y0+4, 1); grid_set(g, x0+10, y0+5, 1); grid_set(g, x0+10, y0+6, 1);
    grid_set(g, x0+11, y0+3, 1); grid_set(g, x0+11, y0+7, 1);
    grid_set(g, x0+12, y0+2, 1); grid_set(g, x0+12, y0+8, 1);
    grid_set(g, x0+13, y0+2, 1); grid_set(g, x0+13, y0+8, 1);
    grid_set(g, x0+14, y0+5, 1);
    grid_set(g, x0+15, y0+3, 1); grid_set(g, x0+15, y0+7, 1);
    grid_set(g, x0+16, y0+4, 1); grid_set(g, x0+16, y0+5, 1); grid_set(g, x0+16, y0+6, 1);
    grid_set(g, x0+17, y0+5, 1);
    /* Right shuttle */
    grid_set(g, x0+20, y0+2, 1); grid_set(g, x0+20, y0+3, 1); grid_set(g, x0+20, y0+4, 1);
    grid_set(g, x0+21, y0+2, 1); grid_set(g, x0+21, y0+3, 1); grid_set(g, x0+21, y0+4, 1);
    grid_set(g, x0+22, y0+1, 1); grid_set(g, x0+22, y0+5, 1);
    grid_set(g, x0+24, y0+0, 1); grid_set(g, x0+24, y0+1, 1);
    grid_set(g, x0+24, y0+5, 1); grid_set(g, x0+24, y0+6, 1);
    /* Right block */
    grid_set(g, x0+34, y0+2, 1); grid_set(g, x0+34, y0+3, 1);
    grid_set(g, x0+35, y0+2, 1); grid_set(g, x0+35, y0+3, 1);
}

/* --- Census --- */

static int strict_mode = 0;
void patterns_set_strict(int strict) { strict_mode = strict; }

static int match_3x3(const Grid *g, int x0, int y0, const int pat[3][3]) {
    int sz = g->size;
    for (int dy = 0; dy < 3; dy++)
        for (int dx = 0; dx < 3; dx++) {
            int x = (x0 + dx) % sz;
            int y = (y0 + dy) % sz;
            if (grid_get(g, x, y) != pat[dy][dx])
                return 0;
        }
    return 1;
}

static int match_4x4(const Grid *g, int x0, int y0, const int pat[4][4]) {
    int sz = g->size;
    for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 4; dx++) {
            int x = (x0 + dx) % sz;
            int y = (y0 + dy) % sz;
            if (grid_get(g, x, y) != pat[dy][dx])
                return 0;
        }
    return 1;
}

static int count_glider_at(const Grid *g, int x0, int y0) {
    /* Phase 0: standard SE glider */
    static const int glider0[3][3] = {
        {0,1,0},
        {0,0,1},
        {1,1,1},
    };
    /* Phase 1 */
    static const int glider1[3][3] = {
        {0,0,0},
        {1,0,1},
        {0,1,1},
    };
    /* Phase 2 */
    static const int glider2[3][3] = {
        {0,0,0},
        {0,1,1},
        {1,1,0},
    };
    /* Phase 3 */
    static const int glider3[3][3] = {
        {0,0,0},
        {1,0,1},
        {0,1,1},
    };
    int matched = 0;
    if (match_3x3(g, x0, y0, glider0)) matched = 1;
    else if (match_3x3(g, x0, y0, glider1)) matched = 1;
    else if (match_3x3(g, x0, y0, glider2)) matched = 1;
    else if (match_3x3(g, x0, y0, glider3)) matched = 1;
    if (!matched) return 0;
    if (strict_mode) {
        int sz = g->size;
        for (int dy = -2; dy <= 4; dy++)
            for (int dx = -2; dx <= 4; dx++) {
                if (dy >= 0 && dy < 3 && dx >= 0 && dx < 3) continue;
                if (grid_get(g, (x0+dx+sz)%sz, (y0+dy+sz)%sz) != 0) return 0;
            }
    }
    return 1;
}

static int count_blinker_h_at(const Grid *g, int x0, int y0) {
    static const int blinker[1][3] = {{1,1,1}};
    int sz = g->size;
    for (int dx = 0; dx < 3; dx++)
        if (grid_get(g, (x0+dx)%sz, y0%sz) != blinker[0][dx])
            return 0;
    /* Isolation: cells above and below must be dead across the span */
    for (int dx = 0; dx < 3; dx++) {
        if (grid_get(g, (x0+dx)%sz, (y0-1+sz)%sz) != 0) return 0;
        if (grid_get(g, (x0+dx)%sz, (y0+1)%sz)     != 0) return 0;
    }
    if (strict_mode) {
        for (int dx = 0; dx < 3; dx++) {
            if (grid_get(g, (x0+dx)%sz, (y0-2+sz)%sz) != 0) return 0;
            if (grid_get(g, (x0+dx)%sz, (y0+2)%sz)     != 0) return 0;
        }
    }
    return 1;
}

static int count_blinker_v_at(const Grid *g, int x0, int y0) {
    int sz = g->size;
    for (int dy = 0; dy < 3; dy++)
        if (grid_get(g, x0%sz, (y0+dy)%sz) != 1)
            return 0;
    /* Isolation: horizontal neighbors dead across full span */
    for (int dy = 0; dy < 3; dy++) {
        if (grid_get(g, (x0-1+sz)%sz, (y0+dy)%sz) == 1) return 0;
        if (grid_get(g, (x0+1)%sz,     (y0+dy)%sz) == 1) return 0;
    }
    /* Isolation: cells above top and below bottom must be dead */
    if (grid_get(g, x0%sz, (y0-1+sz)%sz) != 0) return 0;
    if (grid_get(g, x0%sz, (y0+3)%sz)     != 0) return 0;
    if (strict_mode) {
        for (int dy = 0; dy < 3; dy++) {
            if (grid_get(g, (x0-2+sz)%sz, (y0+dy)%sz) != 0) return 0;
            if (grid_get(g, (x0+2)%sz,     (y0+dy)%sz) != 0) return 0;
        }
        if (grid_get(g, x0%sz, (y0-2+sz)%sz) != 0) return 0;
        if (grid_get(g, x0%sz, (y0+4)%sz)     != 0) return 0;
    }
    return 1;
}

static int count_block_at(const Grid *g, int x0, int y0) {
    static const int block[2][2] = {{1,1},{1,1}};
    int sz = g->size;
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++)
            if (grid_get(g, (x0+dx)%sz, (y0+dy)%sz) != block[dy][dx])
                return 0;
    return 1;
}

static int count_beacon_at(const Grid *g, int x0, int y0) {
    static const int beacon[4][4] = {
        {1,1,0,0},
        {1,0,0,0},
        {0,0,0,1},
        {0,0,1,1},
    };
    if (!match_4x4(g, x0, y0, beacon)) return 0;
    /* Isolation: surrounding cells must be dead */
    int sz = g->size;
    int border = strict_mode ? 2 : 1;
    for (int dy = -border; dy <= 3+border; dy++)
        for (int dx = -border; dx <= 3+border; dx++) {
            if (dy >= 0 && dy < 4 && dx >= 0 && dx < 4) continue; /* skip pattern interior */
            if (grid_get(g, (x0+dx+sz)%sz, (y0+dy+sz)%sz) != 0) return 0;
        }
    return 1;
}

int census_gliders(const Grid *g) {
    int count = 0;
    int sz = g->size;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++)
            count += count_glider_at(g, x, y);
    return count;
}

int census_oscillators(const Grid *g) {
    int count = 0;
    int sz = g->size;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            count += count_blinker_h_at(g, x, y);
            count += count_blinker_v_at(g, x, y);
            count += count_beacon_at(g, x, y);
        }
    return count;
}
