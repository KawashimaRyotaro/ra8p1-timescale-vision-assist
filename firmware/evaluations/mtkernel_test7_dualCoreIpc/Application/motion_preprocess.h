#ifndef MOTION_PREPROCESS_H
#define MOTION_PREPROCESS_H

#include <stdint.h>


#define MOTION_GRAY_WIDTH       (112U)
#define MOTION_GRAY_HEIGHT      (84U)
#define MOTION_GRAY_PIXELS      \
    (MOTION_GRAY_WIDTH * MOTION_GRAY_HEIGHT)


/*
 * CP1 conversion:
 *
 *   224x168 RGB565, source stride = 2048 bytes
 *                 ↓
 *   nearest 1/2 downsample: source pixel (2x, 2y)
 *                 ↓
 *   BT.601-like integer luma
 *                 ↓
 *   112x84 uint8 grayscale
 */
void motion_preprocess_rgb565_to_gray_half(
    const uint8_t * source,
    uint32_t source_stride_bytes,
    uint8_t destination[MOTION_GRAY_PIXELS]
);


#endif /* MOTION_PREPROCESS_H */
