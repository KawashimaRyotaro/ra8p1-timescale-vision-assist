#include "video_source.h"
#include "assets/video_data.h"

const uint8_t * video_source_get_frame(uint32_t frame_index)
{
    if (frame_index >= VIDEO_SOURCE_FRAME_COUNT)
    {
        frame_index = 0U;
    }

    return &g_video_data[frame_index * VIDEO_FRAME_BYTES];
}