#ifndef FRAME_DIFFERENCE_H
#define FRAME_DIFFERENCE_H

#include <stdint.h>


typedef struct
{
    uint32_t sample_count;

    /*
     * Mean absolute luminance difference x 1000.
     *
     * Example:
     *   12345 -> mean difference = 12.345 / 255
     */
    uint32_t mean_abs_luma_x1000;

} frame_difference_result_t;


uint8_t frame_difference_compute_rgb565(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint32_t width,
    uint32_t height,
    frame_difference_result_t * result
);


#endif /* FRAME_DIFFERENCE_H */