#include <stddef.h>
#include <stdint.h>

#include "motion_preprocess.h"


static uint8_t motion_rgb565_to_luma(
    uint16_t pixel)
{
    uint32_t const r5 =
        (pixel >> 11U) & 0x1FU;

    uint32_t const g6 =
        (pixel >> 5U) & 0x3FU;

    uint32_t const b5 =
        pixel & 0x1FU;

    /*
     * Expand RGB565 components to 8-bit.
     */
    uint32_t const r8 =
        (r5 << 3U) | (r5 >> 2U);

    uint32_t const g8 =
        (g6 << 2U) | (g6 >> 4U);

    uint32_t const b8 =
        (b5 << 3U) | (b5 >> 2U);

    /*
     * Integer approximation of:
     * Y = 0.299 R + 0.587 G + 0.114 B
     *
     * 77 + 150 + 29 = 256.
     */
    return
        (uint8_t)
        (
            (
                77U  * r8 +
                150U * g8 +
                29U  * b8 +
                128U
            ) >> 8U
        );
}


void motion_preprocess_rgb565_to_gray_half(
    const uint8_t * source,
    uint32_t source_stride_bytes,
    uint8_t destination[MOTION_GRAY_PIXELS])
{
    if ((NULL == source) ||
        (NULL == destination))
    {
        return;
    }

    for (uint32_t gy = 0U;
         gy < MOTION_GRAY_HEIGHT;
         gy++)
    {
        uint32_t const source_y =
            gy * 2U;

        const uint16_t * const source_row =
            (const uint16_t *)
            (
                source +
                source_y *
                source_stride_bytes
            );

        uint8_t * const destination_row =
            destination +
            gy * MOTION_GRAY_WIDTH;

        for (uint32_t gx = 0U;
             gx < MOTION_GRAY_WIDTH;
             gx++)
        {
            uint32_t const source_x =
                gx * 2U;

            destination_row[gx] =
                motion_rgb565_to_luma(
                    source_row[source_x]
                );
        }
    }
}
