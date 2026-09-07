#include <stddef.h>
#include "video_source.h"
#include "video_cache.h"
#include "ospi_cache.h"
#include "assets/video_data.h"


#define VIDEO_WIDTH              (128U)
#define VIDEO_HEIGHT             (72U)
#define VIDEO_BYTES_PER_PIXEL    (2U)
#define VIDEO_FRAME_COUNT        (10U)

#define VIDEO_FRAME_SIZE_BYTES   \
    (VIDEO_WIDTH * VIDEO_HEIGHT * VIDEO_BYTES_PER_PIXEL)

#define VIDEO_TOTAL_SIZE_BYTES   \
    (VIDEO_FRAME_SIZE_BYTES * VIDEO_FRAME_COUNT)

#define VIDEO_OSPI_OFFSET        (0x00010000UL)

static bool s_ospi_video_ready = false;

static const uint8_t * s_video_base = NULL;
static uint32_t s_frame_size = 0U;
static uint32_t s_frame_count = 0U;


const uint8_t * video_source_get_frame(
    uint32_t frame_index)
{
    if ((!s_ospi_video_ready) ||
        (frame_index >= s_frame_count))
    {
        return NULL;
    }

    return s_video_base +
        (frame_index * s_frame_size);
}


bool video_source_bind_ospi(void)
{
    video_cache_header_t header;
    const uint8_t * payload = NULL;

    video_cache_status_t status =
        video_cache_open(
            &header,
            &payload
        );

    if (VIDEO_CACHE_OK != status)
    {
        return false;
    }

    s_video_base  = payload;
    s_frame_size  = header.frame_size;
    s_frame_count = header.frame_count;

    s_ospi_video_ready = true;

    return true;
}