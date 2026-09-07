#ifndef VIDEO_DATA_H
#define VIDEO_DATA_H

#include <stdint.h>

#define VIDEO_WIDTH        (128U)
#define VIDEO_HEIGHT       (72U)
#define VIDEO_FRAME_COUNT  (10U)
#define VIDEO_FRAME_BYTES  (VIDEO_WIDTH * VIDEO_HEIGHT * 2U)
#define VIDEO_DATA_SIZE    (VIDEO_FRAME_BYTES * VIDEO_FRAME_COUNT)

extern const uint8_t g_video_data[VIDEO_DATA_SIZE];

#endif