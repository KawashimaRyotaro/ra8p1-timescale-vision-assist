#include <stddef.h>
#include <stdint.h>

#include "frame_difference.h"


/*
 * 640x480
 *   ↓ sample every 4 pixels
 * 160x120 = 19200 samples
 *
 * The full previous/current frames are retained,
 * but Difference does not need to inspect all 307200 pixels.
 */
#define FRAME_DIFFERENCE_SAMPLE_STEP    (4U)


static uint8_t frame_difference_rgb565_luma(
    uint16_t pixel)
{
    uint32_t r5 =
        (pixel >> 11) & 0x1FU;

    uint32_t g6 =
        (pixel >> 5) & 0x3FU;

    uint32_t b5 =
        pixel & 0x1FU;


    uint32_t r8 =
        (r5 << 3) |
        (r5 >> 2);

    uint32_t g8 =
        (g6 << 2) |
        (g6 >> 4);

    uint32_t b8 =
        (b5 << 3) |
        (b5 >> 2);


    /*
     * Integer approximation of:
     *
     * Y = 0.299 R + 0.587 G + 0.114 B
     */
    uint32_t y =
        (77U  * r8 +
         150U * g8 +
         29U  * b8) >> 8;

    return (uint8_t) y;
}


uint8_t frame_difference_compute_rgb565(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint32_t width,
    uint32_t height,
    frame_difference_result_t * result)
{
    if ((NULL == previous_frame) ||
        (NULL == current_frame) ||
        (NULL == result) ||
        (0U == width) ||
        (0U == height))
    {
        return 0U;
    }


    const uint16_t * previous =
        (const uint16_t *) previous_frame;

    const uint16_t * current =
        (const uint16_t *) current_frame;


    uint64_t difference_sum = 0U;
    uint32_t sample_count = 0U;


    for (uint32_t y = 0U;
         y < height;
         y += FRAME_DIFFERENCE_SAMPLE_STEP)
    {
        uint32_t row =
            y * width;

        for (uint32_t x = 0U;
             x < width;
             x += FRAME_DIFFERENCE_SAMPLE_STEP)
        {
            uint32_t index =
                row + x;


            uint8_t previous_luma =
                frame_difference_rgb565_luma(
                    previous[index]
                );

            uint8_t current_luma =
                frame_difference_rgb565_luma(
                    current[index]
                );


            uint32_t difference;

            if (current_luma >=
                previous_luma)
            {
                difference =
                    current_luma -
                    previous_luma;
            }
            else
            {
                difference =
                    previous_luma -
                    current_luma;
            }


            difference_sum +=
                difference;

            sample_count++;
        }
    }


    if (0U == sample_count)
    {
        return 0U;
    }


    result->sample_count =
        sample_count;

    result->mean_abs_luma_x1000 =
        (uint32_t)
        (
            (difference_sum * 1000ULL) /
            sample_count
        );


    return 1U;
}