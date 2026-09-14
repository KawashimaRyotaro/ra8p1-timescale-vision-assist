#ifndef MOTION_BLOCK_MATCH_H
#define MOTION_BLOCK_MATCH_H

#include <stdint.h>

#include "motion_preprocess.h"


#if defined(__ARM_FEATURE_MVE) && \
    ((__ARM_FEATURE_MVE & 1) != 0)

#define MOTION_BLOCK_MATCH_MVE_ENABLED    (1U)

#else

#define MOTION_BLOCK_MATCH_MVE_ENABLED    (0U)

#endif


#define MOTION_GRID_COLS          (16U)
#define MOTION_GRID_ROWS          (12U)

#define MOTION_BLOCK_WIDTH        (7U)
#define MOTION_BLOCK_HEIGHT       (7U)

#define MOTION_SEARCH_RADIUS      (4)

#define MOTION_VECTOR_COUNT       \
    (MOTION_GRID_COLS * MOTION_GRID_ROWS)


typedef struct
{
    int8_t   dx;
    int8_t   dy;
    uint16_t sad;

} motion_vector_t;


/*
 * Match each 7x7 block in the previous 112x84 grayscale frame
 * against the current frame within +/-4 pixels.
 *
 * Vector convention:
 *
 *   previous block position -> current best-match position
 *
 * Therefore:
 *   dx > 0 : content moved right
 *   dx < 0 : content moved left
 *   dy > 0 : content moved down
 *   dy < 0 : content moved up
 *
 * Tie breaking:
 *   1. smaller SAD
 *   2. smaller |dx| + |dy|
 *
 * This makes a flat/static region prefer (0,0) instead of an
 * arbitrary edge of the search window.
 */
void motion_block_match(
    const uint8_t previous[MOTION_GRAY_PIXELS],
    const uint8_t current[MOTION_GRAY_PIXELS],
    motion_vector_t vectors[MOTION_VECTOR_COUNT]
);


#endif /* MOTION_BLOCK_MATCH_H */
