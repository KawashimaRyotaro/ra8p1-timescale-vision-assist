#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <stdint.h>
#include <stdbool.h>

#define VIDEO_SOURCE_WIDTH        (128U)
#define VIDEO_SOURCE_HEIGHT       (72U)
#define VIDEO_SOURCE_FRAME_COUNT  (10U)

const uint8_t * video_source_get_frame(uint32_t frame_index);
bool video_source_prepare_ospi(void);

#endif