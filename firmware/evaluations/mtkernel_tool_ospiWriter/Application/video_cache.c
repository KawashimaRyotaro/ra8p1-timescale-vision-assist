#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "video_cache.h"
#include "ospi_cache.h"


#define VIDEO_CACHE_MAGIC               (0x56494430UL) /* "VID0" */
#define VIDEO_CACHE_VERSION             (1U)

#define VIDEO_CACHE_METADATA_OFFSET     (0x0000F000UL)
#define VIDEO_CACHE_PAYLOAD_OFFSET      (0x00010000UL)

#define VIDEO_CACHE_PIXEL_RGB565LE      (1U)

#define VIDEO_CACHE_TEST_WIDTH          (128U)
#define VIDEO_CACHE_TEST_HEIGHT         (72U)
#define VIDEO_CACHE_TEST_FRAME_COUNT    (10U)

#define VIDEO_CACHE_TEST_FRAME_SIZE     \
    (VIDEO_CACHE_TEST_WIDTH * VIDEO_CACHE_TEST_HEIGHT * 2U)

#define VIDEO_CACHE_TEST_PAYLOAD_SIZE   \
    (VIDEO_CACHE_TEST_FRAME_SIZE * VIDEO_CACHE_TEST_FRAME_COUNT)

#define VIDEO_CACHE_TEST_FPS_MILLI      (10000U)


video_cache_status_t video_cache_store(
    const uint8_t * payload,
    uint32_t payload_size,
    const video_cache_info_t * info)
{
    if ((NULL == payload) ||
        (NULL == info) ||
        (0U == payload_size))
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if ((0U == info->width) ||
        (0U == info->height) ||
        (0U == info->frame_count) ||
        (0U == info->frame_size))
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if (payload_size !=
        (info->frame_size * info->frame_count))
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    if (info->frame_size !=
        (info->width * info->height * 2U))
    {
        return VIDEO_CACHE_ERROR_FORMAT;
    }

    if (VIDEO_CACHE_PIXEL_RGB565LE !=
        info->pixel_format)
    {
        return VIDEO_CACHE_ERROR_FORMAT;
    }


    /*
     * Step 1:
     * Invalidate the old cache first.
     *
     * Erasing the metadata sector destroys the magic value.
     * Therefore an interrupted payload update cannot be
     * mistaken for a valid video cache after reboot.
     */
    ospi_cache_status_t storage_status =
        ospi_cache_erase(
            VIDEO_CACHE_METADATA_OFFSET,
            sizeof(video_cache_header_t)
        );

    if (OSPI_CACHE_OK != storage_status)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }


    /*
     * Step 2:
     * Store and verify the actual video payload.
     */
    storage_status =
        ospi_cache_store(
            VIDEO_CACHE_PAYLOAD_OFFSET,
            payload,
            payload_size
        );

    if (OSPI_CACHE_OK != storage_status)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }


    /*
     * Step 3:
     * Construct metadata only after the payload has been
     * stored successfully.
     */
    const video_cache_header_t header =
    {
        .magic          = VIDEO_CACHE_MAGIC,
        .version        = VIDEO_CACHE_VERSION,
        .header_size    = sizeof(video_cache_header_t),

        .payload_offset = VIDEO_CACHE_PAYLOAD_OFFSET,
        .payload_size   = payload_size,

        .frame_size     = info->frame_size,
        .frame_count    = info->frame_count,

        .width          = info->width,
        .height         = info->height,

        .pixel_format   = info->pixel_format,
        .fps_milli      = info->fps_milli,

        .reserved       = {0U}
    };


    /*
     * Step 4:
     * Write metadata last.
     *
     * ospi_cache_store() performs:
     * erase -> program -> verify.
     *
     * When this succeeds, the cache becomes valid.
     */
    storage_status =
        ospi_cache_store(
            VIDEO_CACHE_METADATA_OFFSET,
            (const uint8_t *) &header,
            sizeof(header)
        );

    if (OSPI_CACHE_OK != storage_status)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }

    return VIDEO_CACHE_OK;
}


video_cache_status_t video_cache_open(
    video_cache_header_t * header,
    const uint8_t ** payload)
{
    if ((NULL == header) || (NULL == payload))
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    /*
     * Get memory-mapped address of metadata.
     */
    const uint8_t * metadata =
        ospi_cache_mapped_ptr(
            VIDEO_CACHE_METADATA_OFFSET
        );

    if (NULL == metadata)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }

    /*
     * This memcpy causes the actual OSPI memory-mapped read.
     */
    memcpy(
        header,
        metadata,
        sizeof(video_cache_header_t)
    );


    /*
     * Validate metadata.
     */
    if (VIDEO_CACHE_MAGIC != header->magic)
    {
        return VIDEO_CACHE_ERROR_MAGIC;
    }

    if (VIDEO_CACHE_VERSION != header->version)
    {
        return VIDEO_CACHE_ERROR_VERSION;
    }

    if (sizeof(video_cache_header_t) != header->header_size)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    if ((0U == header->payload_size) ||
        (0U == header->frame_size) ||
        (0U == header->frame_count))
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    if (header->payload_size !=
        (header->frame_size * header->frame_count))
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    if (VIDEO_CACHE_PIXEL_RGB565LE !=
        header->pixel_format)
    {
        return VIDEO_CACHE_ERROR_FORMAT;
    }

    if (header->frame_size !=
        (header->width * header->height * 2U))
    {
        return VIDEO_CACHE_ERROR_FORMAT;
    }


    /*
     * Obtain memory-mapped video payload address.
     */
    const uint8_t * video =
        ospi_cache_mapped_ptr(
            header->payload_offset
        );

    if (NULL == video)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }

    /*
     * Also verify that the last payload byte is inside
     * the OSPI address range.
     */
    if (NULL ==
        ospi_cache_mapped_ptr(
            header->payload_offset +
            header->payload_size - 1U))
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    *payload = video;

    return VIDEO_CACHE_OK;
}