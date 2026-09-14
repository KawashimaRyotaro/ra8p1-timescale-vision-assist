#ifndef MOTION_GLOBAL_H
#define MOTION_GLOBAL_H

#include <stdint.h>

#include "motion_block_match.h"


#define MOTION_GLOBAL_SECTOR_COLS       (4U)
#define MOTION_GLOBAL_SECTOR_ROWS       (3U)
#define MOTION_GLOBAL_SECTOR_COUNT      \
    (MOTION_GLOBAL_SECTOR_COLS * MOTION_GLOBAL_SECTOR_ROWS)

/*
 * At least half of the spatial sectors must agree.
 */
#define MOTION_GLOBAL_MIN_CONSENSUS     (6U)

/*
 * Consensus must be spatially distributed.
 */
#define MOTION_GLOBAL_MIN_COL_SPAN      (3U)
#define MOTION_GLOBAL_MIN_ROW_SPAN      (2U)


typedef struct
{
    int8_t dx;
    int8_t dy;

    uint8_t valid;
    uint8_t applied;

    uint8_t consensus_sectors;
    uint8_t agreeing_cols;
    uint8_t agreeing_rows;

} motion_global_result_t;


/*
 * 0 = estimate global motion, but do not subtract it
 * 1 = subtract it only when the estimate passes validation
 *
 * Can be changed directly from debugger.
 */
extern volatile uint32_t
    g_motion_global_compensation_enabled;


void motion_global_compensate(
    const motion_vector_t input[MOTION_VECTOR_COUNT],
    motion_vector_t output[MOTION_VECTOR_COUNT],
    motion_global_result_t * result
);


#endif /* MOTION_GLOBAL_H */