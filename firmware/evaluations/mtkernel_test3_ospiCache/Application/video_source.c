#include <stddef.h>
#include <tm/tmonitor.h>
#include "video_source.h"
#include "assets/video_data.h"
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


const uint8_t * video_source_get_frame(uint32_t frame_index)
{
    if ((!s_ospi_video_ready) ||
        (frame_index >= VIDEO_FRAME_COUNT))
    {
        return NULL;
    }

    const uint8_t * video_base =
        ospi_cache_mapped_ptr(VIDEO_OSPI_OFFSET);

    if (NULL == video_base)
    {
        return NULL;
    }

    return video_base +
        (frame_index * VIDEO_FRAME_SIZE_BYTES);
}


bool video_source_prepare_ospi(void)
{
    ospi_cache_status_t status;

    status = ospi_cache_store(
        VIDEO_OSPI_OFFSET,
        g_video_data,
        VIDEO_TOTAL_SIZE_BYTES
    );

    tm_printf(
        (UB *)"[OSPI] store status=%d\n",
        status
    );

    if (OSPI_CACHE_OK != status)
    {
        return false;
    }

    s_ospi_video_ready = true;

    return true;
}