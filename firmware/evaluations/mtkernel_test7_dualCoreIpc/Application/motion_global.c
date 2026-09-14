#include <stddef.h>
#include <stdint.h>

#include "motion_global.h"


volatile uint32_t
    g_motion_global_compensation_enabled =
        1U;


/*
 * One sector contains at most 4x4 blocks.
 */
#define MOTION_GLOBAL_MAX_SECTOR_VECTORS    (16U)


static uint32_t motion_global_abs_i32(
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


static int8_t motion_global_median_i8(
    int8_t values[],
    uint32_t count)
{
    /*
     * Very small arrays only: <= 16 values.
     * Simple insertion sort is sufficient.
     */
    for (uint32_t i = 1U;
         i < count;
         i++)
    {
        int8_t const key =
            values[i];

        uint32_t j =
            i;

        while ((j > 0U) &&
               (values[j - 1U] > key))
        {
            values[j] =
                values[j - 1U];

            j--;
        }

        values[j] =
            key;
    }


    /*
     * Lower median for even count.
     */
    return
        values[(count - 1U) / 2U];
}


static void motion_global_sector_vector(
    const motion_vector_t input[MOTION_VECTOR_COUNT],
    uint32_t sector_x,
    uint32_t sector_y,
    int8_t * result_dx,
    int8_t * result_dy)
{
    int8_t dx_values[
        MOTION_GLOBAL_MAX_SECTOR_VECTORS];

    int8_t dy_values[
        MOTION_GLOBAL_MAX_SECTOR_VECTORS];

    uint32_t count =
        0U;


    uint32_t const block_x_begin =
        sector_x * 4U;

    uint32_t const block_x_end =
        block_x_begin + 4U;

    uint32_t const block_y_begin =
        sector_y * 4U;

    uint32_t const block_y_end =
        block_y_begin + 4U;


    for (uint32_t block_y = block_y_begin;
         block_y < block_y_end;
         block_y++)
    {
        /*
         * Ignore outermost block row.
         */
        if ((0U == block_y) ||
            ((MOTION_GRID_ROWS - 1U) == block_y))
        {
            continue;
        }


        for (uint32_t block_x = block_x_begin;
             block_x < block_x_end;
             block_x++)
        {
            /*
             * Ignore outermost block column.
             */
            if ((0U == block_x) ||
                ((MOTION_GRID_COLS - 1U) == block_x))
            {
                continue;
            }


            uint32_t const index =
                block_y *
                MOTION_GRID_COLS +
                block_x;


            dx_values[count] =
                input[index].dx;

            dy_values[count] =
                input[index].dy;

            count++;
        }
    }


    if (0U == count)
    {
        *result_dx = 0;
        *result_dy = 0;

        return;
    }


    *result_dx =
        (int8_t)
        motion_global_median_i8(
            dx_values,
            count
        );

    *result_dy =
        (int8_t)
        motion_global_median_i8(
            dy_values,
            count
        );
}


void motion_global_compensate(
    const motion_vector_t input[MOTION_VECTOR_COUNT],
    motion_vector_t output[MOTION_VECTOR_COUNT],
    motion_global_result_t * result)
{
    if ((NULL == input) ||
        (NULL == output) ||
        (NULL == result))
    {
        return;
    }


    int8_t sector_dx[
        MOTION_GLOBAL_SECTOR_COUNT];

    int8_t sector_dy[
        MOTION_GLOBAL_SECTOR_COUNT];


    /*
     * Stage 1:
     * One robust representative vector per spatial sector.
     */
    for (uint32_t sector_y = 0U;
         sector_y < MOTION_GLOBAL_SECTOR_ROWS;
         sector_y++)
    {
        for (uint32_t sector_x = 0U;
             sector_x < MOTION_GLOBAL_SECTOR_COLS;
             sector_x++)
        {
            uint32_t const sector_index =
                sector_y *
                MOTION_GLOBAL_SECTOR_COLS +
                sector_x;


            motion_global_sector_vector(
                input,
                sector_x,
                sector_y,
                &sector_dx[sector_index],
                &sector_dy[sector_index]
            );
        }
    }


    /*
     * Stage 2:
     *
     * Search all possible integer translation vectors.
     * Candidate space is only 9x9 = 81.
     *
     * Primary criterion:
     *   maximum number of agreeing sectors.
     *
     * Secondary:
     *   minimum total L1 error.
     *
     * Final tie:
     *   smaller vector magnitude.
     */
    int32_t best_dx =
        0;

    int32_t best_dy =
        0;

    uint32_t best_consensus =
        0U;

    uint32_t best_total_error =
        UINT32_MAX;

    uint32_t best_magnitude =
        UINT32_MAX;


    for (int32_t candidate_dy = -MOTION_SEARCH_RADIUS;
         candidate_dy <= MOTION_SEARCH_RADIUS;
         candidate_dy++)
    {
        for (int32_t candidate_dx = -MOTION_SEARCH_RADIUS;
             candidate_dx <= MOTION_SEARCH_RADIUS;
             candidate_dx++)
        {
            uint32_t consensus =
                0U;

            uint32_t total_error =
                0U;


            for (uint32_t sector = 0U;
                 sector < MOTION_GLOBAL_SECTOR_COUNT;
                 sector++)
            {
                uint32_t const error =
                    motion_global_abs_i32(
                        (int32_t) sector_dx[sector] -
                        candidate_dx
                    ) +
                    motion_global_abs_i32(
                        (int32_t) sector_dy[sector] -
                        candidate_dy
                    );


                total_error +=
                    error;


                /*
                 * +/-1 Manhattan-distance agreement.
                 */
                if (error <= 1U)
                {
                    consensus++;
                }
            }


            uint32_t const magnitude =
                motion_global_abs_i32(candidate_dx) +
                motion_global_abs_i32(candidate_dy);


            if ((consensus > best_consensus) ||
                ((consensus == best_consensus) &&
                 (total_error < best_total_error)) ||
                ((consensus == best_consensus) &&
                 (total_error == best_total_error) &&
                 (magnitude < best_magnitude)))
            {
                best_consensus =
                    consensus;

                best_total_error =
                    total_error;

                best_magnitude =
                    magnitude;

                best_dx =
                    candidate_dx;

                best_dy =
                    candidate_dy;
            }
        }
    }


    /*
     * Stage 3:
     * Check whether the agreeing sectors are spatially spread.
     */
    uint32_t agreeing_col_mask =
        0U;

    uint32_t agreeing_row_mask =
        0U;


    for (uint32_t sector_y = 0U;
         sector_y < MOTION_GLOBAL_SECTOR_ROWS;
         sector_y++)
    {
        for (uint32_t sector_x = 0U;
             sector_x < MOTION_GLOBAL_SECTOR_COLS;
             sector_x++)
        {
            uint32_t const sector_index =
                sector_y *
                MOTION_GLOBAL_SECTOR_COLS +
                sector_x;


            uint32_t const error =
                motion_global_abs_i32(
                    (int32_t) sector_dx[sector_index] -
                    best_dx
                ) +
                motion_global_abs_i32(
                    (int32_t) sector_dy[sector_index] -
                    best_dy
                );


            if (error <= 1U)
            {
                agreeing_col_mask |=
                    (1UL << sector_x);

                agreeing_row_mask |=
                    (1UL << sector_y);
            }
        }
    }


    uint32_t agreeing_cols =
        0U;

    uint32_t agreeing_rows =
        0U;


    for (uint32_t x = 0U;
         x < MOTION_GLOBAL_SECTOR_COLS;
         x++)
    {
        if (0U !=
            (agreeing_col_mask &
             (1UL << x)))
        {
            agreeing_cols++;
        }
    }


    for (uint32_t y = 0U;
         y < MOTION_GLOBAL_SECTOR_ROWS;
         y++)
    {
        if (0U !=
            (agreeing_row_mask &
             (1UL << y)))
        {
            agreeing_rows++;
        }
    }


    uint8_t valid =
        1U;


    if (best_consensus <
        MOTION_GLOBAL_MIN_CONSENSUS)
    {
        valid =
            0U;
    }


    if (agreeing_cols <
        MOTION_GLOBAL_MIN_COL_SPAN)
    {
        valid =
            0U;
    }


    if (agreeing_rows <
        MOTION_GLOBAL_MIN_ROW_SPAN)
    {
        valid =
            0U;
    }


    /*
     * +/-4 is the search boundary.
     *
     * A result at the boundary may represent saturation rather than
     * the true camera translation. Do not compensate such a frame.
     */
    if ((motion_global_abs_i32(best_dx) >=
         MOTION_SEARCH_RADIUS) ||
        (motion_global_abs_i32(best_dy) >=
         MOTION_SEARCH_RADIUS))
    {
        valid =
            0U;
    }


    result->dx =
        (int8_t) best_dx;

    result->dy =
        (int8_t) best_dy;

    result->valid =
        valid;

    result->consensus_sectors =
        (uint8_t) best_consensus;

    result->agreeing_cols =
        (uint8_t) agreeing_cols;

    result->agreeing_rows =
        (uint8_t) agreeing_rows;


    uint8_t const apply =
        (0U !=
         g_motion_global_compensation_enabled) &&
        (0U != valid);


    result->applied =
        apply;


    /*
     * Stage 4:
     * Keep RAW vectors when compensation is disabled or unreliable.
     */
    for (uint32_t i = 0U;
         i < MOTION_VECTOR_COUNT;
         i++)
    {
        output[i] =
            input[i];


        if (0U != apply)
        {
            output[i].dx =
                (int8_t)
                (
                    (int32_t) input[i].dx -
                    best_dx
                );

            output[i].dy =
                (int8_t)
                (
                    (int32_t) input[i].dy -
                    best_dy
                );
        }
    }
}