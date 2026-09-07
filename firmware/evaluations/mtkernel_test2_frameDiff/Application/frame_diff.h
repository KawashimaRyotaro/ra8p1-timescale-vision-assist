#ifndef FRAME_DIFF_H
#define FRAME_DIFF_H

#include <stdint.h>

typedef struct
{
    uint32_t changed_pixels;
    uint32_t total_pixels;
    uint32_t changed_permille;
    uint32_t mean_abs_diff;

} frame_diff_result_t;


frame_diff_result_t frame_diff_compute(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint32_t width,
    uint32_t height,
    uint8_t threshold
);


void frame_diff_build_mask(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint8_t * mask,
    uint32_t width,
    uint32_t height,
    uint8_t threshold
);

#endif