
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "motion_block_match.h"


#if MOTION_BLOCK_MATCH_MVE_ENABLED

#include <arm_mve.h>

#endif


static uint32_t motion_abs_i32(
    int32_t value)
{
    return
        (uint32_t)
        (
            (value < 0) ?
            -value :
            value
        );
}


static uint32_t motion_block_sad(
    const uint8_t * previous,
    const uint8_t * current,
    uint32_t previous_x,
    uint32_t previous_y,
    uint32_t current_x,
    uint32_t current_y)
{
    uint32_t sad =
        0U;


#if MOTION_BLOCK_MATCH_MVE_ENABLED

    /*
     * One block row contains only 7 valid pixels.
     *
     * vctp8q(7) enables lanes 0..6 only.
     * vld1q_z_u8() therefore never reads beyond
     * the 7-pixel block row.
     */
    mve_pred16_t const predicate =
        vctp8q(
            MOTION_BLOCK_WIDTH
        );


    for (uint32_t y = 0U;
         y < MOTION_BLOCK_HEIGHT;
         y++)
    {
        const uint8_t * const previous_row =
            previous +
            (previous_y + y) *
            MOTION_GRAY_WIDTH +
            previous_x;

        const uint8_t * const current_row =
            current +
            (current_y + y) *
            MOTION_GRAY_WIDTH +
            current_x;


        uint8x16_t const previous_vector =
            vld1q_z_u8(
                previous_row,
                predicate
            );

        uint8x16_t const current_vector =
            vld1q_z_u8(
                current_row,
                predicate
            );


        /*
         * 7 absolute differences are calculated in parallel,
         * reduced, and accumulated directly into scalar SAD.
         */
        sad =
            vabavq_u8(
                sad,
                previous_vector,
                current_vector
            );
    }


#else

    /*
     * Scalar fallback.
     */
    for (uint32_t y = 0U;
         y < MOTION_BLOCK_HEIGHT;
         y++)
    {
        const uint8_t * const previous_row =
            previous +
            (previous_y + y) *
            MOTION_GRAY_WIDTH +
            previous_x;

        const uint8_t * const current_row =
            current +
            (current_y + y) *
            MOTION_GRAY_WIDTH +
            current_x;


        for (uint32_t x = 0U;
             x < MOTION_BLOCK_WIDTH;
             x++)
        {
            int32_t const difference =
                (int32_t) previous_row[x] -
                (int32_t) current_row[x];

            sad +=
                motion_abs_i32(
                    difference
                );
        }
    }

#endif


    return sad;
}


#if MOTION_BLOCK_MATCH_MVE_ENABLED

static uint32_t motion_block_sad_mve_preloaded(
    const uint8x16_t previous_rows[MOTION_BLOCK_HEIGHT],
    const uint8_t * current,
    uint32_t current_x,
    uint32_t current_y,
    mve_pred16_t predicate)
{
    uint32_t sad =
        0U;


    for (uint32_t y = 0U;
         y < MOTION_BLOCK_HEIGHT;
         y++)
    {
        const uint8_t * const current_row =
            current +
            (current_y + y) *
            MOTION_GRAY_WIDTH +
            current_x;


        uint8x16_t const current_vector =
            vld1q_z_u8(
                current_row,
                predicate
            );


        sad =
            vabavq_u8(
                sad,
                previous_rows[y],
                current_vector
            );
    }


    return sad;
}

#endif


void motion_block_match(
    const uint8_t previous[MOTION_GRAY_PIXELS],
    const uint8_t current[MOTION_GRAY_PIXELS],
    motion_vector_t vectors[MOTION_VECTOR_COUNT])
{
    if ((NULL == previous) ||
        (NULL == current) ||
        (NULL == vectors))
    {
        return;
    }


    for (uint32_t block_y = 0U;
         block_y < MOTION_GRID_ROWS;
         block_y++)
    {
        uint32_t const previous_y =
            block_y *
            MOTION_BLOCK_HEIGHT;

        for (uint32_t block_x = 0U;
             block_x < MOTION_GRID_COLS;
             block_x++)
        {
            uint32_t const previous_x =
                block_x *
                MOTION_BLOCK_WIDTH;


            #if MOTION_BLOCK_MATCH_MVE_ENABLED

            uint8x16_t previous_rows[MOTION_BLOCK_HEIGHT];

            mve_pred16_t const predicate =
                vctp8q(
                    MOTION_BLOCK_WIDTH
                );


            for (uint32_t y = 0U;
                y < MOTION_BLOCK_HEIGHT;
                y++)
            {
                const uint8_t * const previous_row =
                    previous +
                    (previous_y + y) *
                    MOTION_GRAY_WIDTH +
                    previous_x;


                previous_rows[y] =
                    vld1q_z_u8(
                        previous_row,
                        predicate
                    );
            }

            #endif


            uint32_t best_sad =
                UINT32_MAX;

            uint32_t best_distance =
                UINT32_MAX;

            int32_t best_dx =
                0;

            int32_t best_dy =
                0;


            for (int32_t dy =
                     -MOTION_SEARCH_RADIUS;
                 dy <=
                     MOTION_SEARCH_RADIUS;
                 dy++)
            {
                int32_t const candidate_y =
                    (int32_t) previous_y +
                    dy;

                if ((candidate_y < 0) ||
                    ((candidate_y +
                      (int32_t) MOTION_BLOCK_HEIGHT) >
                     (int32_t) MOTION_GRAY_HEIGHT))
                {
                    continue;
                }


                for (int32_t dx =
                         -MOTION_SEARCH_RADIUS;
                     dx <=
                         MOTION_SEARCH_RADIUS;
                     dx++)
                {
                    int32_t const candidate_x =
                        (int32_t) previous_x +
                        dx;

                    if ((candidate_x < 0) ||
                        ((candidate_x +
                          (int32_t) MOTION_BLOCK_WIDTH) >
                         (int32_t) MOTION_GRAY_WIDTH))
                    {
                        continue;
                    }


                    #if MOTION_BLOCK_MATCH_MVE_ENABLED

                    uint32_t const sad =
                        motion_block_sad_mve_preloaded(
                            previous_rows,
                            current,
                            (uint32_t) candidate_x,
                            (uint32_t) candidate_y,
                            predicate
                        );

                    #else

                    uint32_t const sad =
                        motion_block_sad(
                            previous,
                            current,
                            previous_x,
                            previous_y,
                            (uint32_t) candidate_x,
                            (uint32_t) candidate_y
                        );

                    #endif

                    uint32_t const distance =
                        motion_abs_i32(dx) +
                        motion_abs_i32(dy);


                    if ((sad < best_sad) ||
                        ((sad == best_sad) &&
                         (distance <
                          best_distance)))
                    {
                        best_sad =
                            sad;

                        best_distance =
                            distance;

                        best_dx =
                            dx;

                        best_dy =
                            dy;
                    }
                }
            }


            uint32_t const vector_index =
                block_y *
                MOTION_GRID_COLS +
                block_x;

            vectors[vector_index].dx =
                (int8_t) best_dx;

            vectors[vector_index].dy =
                (int8_t) best_dy;

            vectors[vector_index].sad =
                (uint16_t) best_sad;
        }
    }
}
