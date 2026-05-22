#ifndef GENOME_H
#define GENOME_H

#include <stdint.h>

/* 48-bit per-cell genome, stored as uint64_t (upper 16 bits unused).
   Bit layout (from LSB):
     bits 0-7:   rule_select       (0-255, indexes into RULE_TABLE)
     bits 8-15:  coupling_weight   (0-255)
     bits 16-23: mutation_rate     (0-255)
     bits 24-35: mode_orientations (3 bits x 4 modes)
       bits 24-26: orient_mode0
       bits 27-29: orient_mode1
       bits 30-32: orient_mode2
       bits 33-35: orient_mode3
     bits 36-39: alt_rule_select   (4 bits, 0-15)
     bits 40-43: dir_mask          (4 bits: N,S,E,W)
     bits 44-47: reserved
*/

typedef uint64_t CellGenome;

#define GENOME_RULE_SELECT(g)        ((g) & 0xFF)
#define GENOME_COUPLING_WEIGHT(g)    (((g) >> 8)  & 0xFF)
#define GENOME_MUTATION_RATE(g)      (((g) >> 16) & 0xFF)
#define GENOME_ORIENT_MODE0(g)       (((g) >> 24) & 0x07)
#define GENOME_ORIENT_MODE1(g)       (((g) >> 27) & 0x07)
#define GENOME_ORIENT_MODE2(g)       (((g) >> 30) & 0x07)
#define GENOME_ORIENT_MODE3(g)       (((g) >> 33) & 0x07)
#define GENOME_ALT_RULE_SELECT(g)    (((g) >> 36) & 0x0F)
#define GENOME_DIR_MASK(g)           (((g) >> 40) & 0x0F)

#define GENOME_SET_RULE_SELECT(g, v)        ((g) = ((g) & ~0x00000000000000FFULL) | (((uint64_t)(v)) & 0xFF))
#define GENOME_SET_COUPLING_WEIGHT(g, v)  ((g) = ((g) & ~0x000000000000FF00ULL) | ((((uint64_t)(v)) & 0xFF) << 8))
#define GENOME_SET_MUTATION_RATE(g, v)      ((g) = ((g) & ~0x0000000000FF0000ULL) | ((((uint64_t)(v)) & 0xFF) << 16))
#define GENOME_SET_ORIENT_MODE0(g, v)       ((g) = ((g) & ~0x0000000007000000ULL) | ((((uint64_t)(v)) & 0x07) << 24))
#define GENOME_SET_ORIENT_MODE1(g, v)       ((g) = ((g) & ~0x0000000038000000ULL) | ((((uint64_t)(v)) & 0x07) << 27))
#define GENOME_SET_ORIENT_MODE2(g, v)       ((g) = ((g) & ~0x00000001C0000000ULL) | ((((uint64_t)(v)) & 0x07) << 30))
#define GENOME_SET_ORIENT_MODE3(g, v)       ((g) = ((g) & ~0x0000000E00000000ULL) | ((((uint64_t)(v)) & 0x07) << 33))
#define GENOME_SET_ALT_RULE_SELECT(g, v)    ((g) = ((g) & ~0x000000F000000000ULL) | ((((uint64_t)(v)) & 0x0F) << 36))
#define GENOME_SET_DIR_MASK(g, v)           ((g) = ((g) & ~0x00000F0000000000ULL) | ((((uint64_t)(v)) & 0x0F) << 40))

/* Legacy orientation = mode0 orientation */
#define GENOME_ORIENTATION(g)         GENOME_ORIENT_MODE0(g)
#define GENOME_SET_ORIENTATION(g, v)  GENOME_SET_ORIENT_MODE0(g, v)

#define GENOME_DEFAULT 0x0000000000000000ULL
#define GENOME_MAX_RULE 255
#define GENOME_MAX_WEIGHT 255
#define GENOME_MAX_MUTATION 255
#define GENOME_MAX_ORIENTATION 7

/* Mutation mask bits for genome_mutate */
#define GENOME_MUTATE_RULE          (1u << 0)
#define GENOME_MUTATE_WEIGHT        (1u << 1)
#define GENOME_MUTATE_MUTATION      (1u << 2)
#define GENOME_MUTATE_ORIENT_MODE0  (1u << 3)
#define GENOME_MUTATE_ORIENT_MODE1  (1u << 4)
#define GENOME_MUTATE_ORIENT_MODE2  (1u << 5)
#define GENOME_MUTATE_ORIENT_MODE3  (1u << 6)
#define GENOME_MUTATE_ALT_RULE      (1u << 7)
#define GENOME_MUTATE_DIR_MASK      (1u << 8)
#define GENOME_MUTATE_ORIENTATION   GENOME_MUTATE_ORIENT_MODE0  /* legacy alias */
#define GENOME_MUTATE_ALL           (GENOME_MUTATE_RULE | GENOME_MUTATE_WEIGHT | GENOME_MUTATE_MUTATION | \
                                       GENOME_MUTATE_ORIENT_MODE0 | GENOME_MUTATE_ORIENT_MODE1 | \
                                       GENOME_MUTATE_ORIENT_MODE2 | GENOME_MUTATE_ORIENT_MODE3 | \
                                       GENOME_MUTATE_ALT_RULE | GENOME_MUTATE_DIR_MASK)

/* Random genome with all fields independently uniform */
CellGenome genome_random(uint64_t (*rng)(void));

/* Mutate one field chosen at random from mask, with given global probability.
   mask is a bitmask of GENOME_MUTATE_*; 0 defaults to GENOME_MUTATE_ALL. */
CellGenome genome_mutate(CellGenome g, double prob, int mask, uint64_t (*rng)(void));

/* Uniform crossover: each bit independently from parent a or b */
CellGenome genome_crossover(CellGenome a, CellGenome b, uint64_t (*rng)(void));

/* 1-point crossover on the 48 bits */
CellGenome genome_crossover_1pt(CellGenome a, CellGenome b, uint64_t (*rng)(void));

/* Tournament selection: return the best of n candidates drawn from arr */
int genome_tournament_select(const CellGenome *arr, const double *fitness, int n, int k, uint64_t (*rng)(void));

/* Build genome with explicit field values */
CellGenome genome_pack(int rule, int weight, int mutation);
CellGenome genome_pack_oriented(int rule, int weight, int mutation, int orientation);
CellGenome genome_pack_full(int rule, int weight, int mutation,
                            int o0, int o1, int o2, int o3,
                            int alt_rule, int dir_mask);

#endif
