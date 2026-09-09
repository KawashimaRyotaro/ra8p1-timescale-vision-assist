#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "video_cache.h"
#include "ospi_cache.h"


#define VIDEO_CACHE_MAGIC               (0x56494430UL) /* "VID0" */
#define VIDEO_CACHE_VERSION             (1U)

#define VIDEO_CACHE_METADATA_OFFSET     (0x0000F000UL)
#define VIDEO_CACHE_PAYLOAD_OFFSET      (0x00010000UL)

#define VIDEO_CACHE_SLOT_A_OFFSET       (0x00010000UL)
#define VIDEO_CACHE_SLOT_B_OFFSET       (0x02000000UL)

#define VIDEO_CACHE_SLOT_A_CAPACITY     \
    (VIDEO_CACHE_SLOT_B_OFFSET - VIDEO_CACHE_SLOT_A_OFFSET)

#define VIDEO_CACHE_SLOT_B_CAPACITY     \
    (0x04000000UL - VIDEO_CACHE_SLOT_B_OFFSET)

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


video_cache_status_t video_cache_stream_begin(
    video_cache_stream_t * stream,
    const video_cache_info_t * info)
{
    if ((NULL == stream) ||
        (NULL == info))
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

    uint32_t payload_size =
        info->frame_size * info->frame_count;

    if (0U == payload_size)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }


    /*
     * Select the slot which is NOT currently active.
     *
     * This allows the display to keep reading the old
     * payload while a new video is written.
     */
    uint32_t next_payload_offset =
        VIDEO_CACHE_SLOT_A_OFFSET;

    uint32_t next_capacity =
        VIDEO_CACHE_SLOT_A_CAPACITY;

    video_cache_header_t old_header;
    const uint8_t * old_payload = NULL;

    video_cache_status_t old_status =
        video_cache_open(
            &old_header,
            &old_payload
        );

    if (VIDEO_CACHE_OK == old_status)
    {
        if (VIDEO_CACHE_SLOT_A_OFFSET ==
            old_header.payload_offset)
        {
            next_payload_offset =
                VIDEO_CACHE_SLOT_B_OFFSET;

            next_capacity =
                VIDEO_CACHE_SLOT_B_CAPACITY;
        }
        else
        {
            next_payload_offset =
                VIDEO_CACHE_SLOT_A_OFFSET;

            next_capacity =
                VIDEO_CACHE_SLOT_A_CAPACITY;
        }
    }

    if (payload_size > next_capacity)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }


    /*
     * Invalidate metadata before modifying a payload slot.
     *
     * If power is lost during the following writes,
     * video_cache_open() will reject the incomplete cache.
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


    stream->info = *info;

    stream->payload_offset =
        next_payload_offset;

    stream->payload_size =
        payload_size;

    stream->bytes_written = 0U;

    /*
    * CRC-32 initial value.
    */
    stream->crc32 = 0xFFFFFFFFUL;

    stream->active = 1U;

    return VIDEO_CACHE_OK;
}


video_cache_status_t video_cache_stream_write(
    video_cache_stream_t * stream,
    const uint8_t * data,
    uint32_t size)
{
    if ((NULL == stream) ||
        (NULL == data) ||
        (0U == size))
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if (0U == stream->active)
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if (stream->bytes_written >
        stream->payload_size)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    uint32_t remaining =
        stream->payload_size -
        stream->bytes_written;

    if (size > remaining)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }


    /*
     * ospi_cache_store() erases before programming.
     *
     * Therefore each non-final streaming write must start
     * on a fresh 4-KiB erase sector.
     */
    if ((stream->bytes_written %
         VIDEO_CACHE_STREAM_CHUNK_BYTES) != 0U)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    /*
     * Every non-final transfer must be exactly one
     * erase sector.
     *
     * Only the last transfer may be shorter.
     */
    if ((size < remaining) &&
        (VIDEO_CACHE_STREAM_CHUNK_BYTES != size))
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    if (size > VIDEO_CACHE_STREAM_CHUNK_BYTES)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }


    ospi_cache_status_t storage_status =
        ospi_cache_store(
            stream->payload_offset +
            stream->bytes_written,
            data,
            size
        );

    if (OSPI_CACHE_OK != storage_status)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }

    stream->crc32 =
        video_cache_crc32_update(
            stream->crc32,
            data,
            size
        );

    stream->bytes_written += size;

    return VIDEO_CACHE_OK;
}


video_cache_status_t video_cache_stream_commit(
    video_cache_stream_t * stream)
{
    if (NULL == stream)
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if (0U == stream->active)
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    if (stream->bytes_written !=
        stream->payload_size)
    {
        return VIDEO_CACHE_ERROR_SIZE;
    }

    uint32_t final_crc =
        stream->crc32 ^
        0xFFFFFFFFUL;

    const video_cache_header_t header =
    {
        .magic          = VIDEO_CACHE_MAGIC,
        .version        = VIDEO_CACHE_VERSION,
        .header_size    = sizeof(video_cache_header_t),

        .payload_offset = stream->payload_offset,
        .payload_size   = stream->payload_size,

        .frame_size     = stream->info.frame_size,
        .frame_count    = stream->info.frame_count,

        .width          = stream->info.width,
        .height         = stream->info.height,

        .pixel_format   = stream->info.pixel_format,
        .fps_milli      = stream->info.fps_milli,

        .reserved =
        {
            final_crc,
            0U,
            0U,
            0U,
            0U
        }
    };


    /*
     * Metadata is written only after every payload
     * sector has been successfully stored.
     */
    ospi_cache_status_t storage_status =
        ospi_cache_store(
            VIDEO_CACHE_METADATA_OFFSET,
            (const uint8_t *) &header,
            sizeof(header)
        );

    if (OSPI_CACHE_OK != storage_status)
    {
        return VIDEO_CACHE_ERROR_STORAGE;
    }

    stream->active = 0U;

    return VIDEO_CACHE_OK;
}


void video_cache_stream_abort(
    video_cache_stream_t * stream)
{
    if (NULL != stream)
    {
        /*
         * Metadata was already invalidated by begin().
         * Therefore no incomplete payload can become valid.
         */
        stream->active = 0U;
    }
}


uint32_t video_cache_crc32_update(
    uint32_t crc,
    const uint8_t * data,
    uint32_t size)
{
    if (NULL == data)
    {
        return crc;
    }

    for (uint32_t i = 0U; i < size; i++)
    {
        crc ^= data[i];

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            if (0U != (crc & 1U))
            {
                crc =
                    (crc >> 1U) ^
                    0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}


video_cache_status_t video_cache_matches(
    const video_cache_info_t * info,
    uint32_t payload_size,
    uint32_t crc32,
    uint8_t * matches)
{
    if ((NULL == info) ||
        (NULL == matches))
    {
        return VIDEO_CACHE_ERROR_ARGUMENT;
    }

    *matches = 0U;

    video_cache_header_t header;
    const uint8_t * payload = NULL;

    video_cache_status_t status =
        video_cache_open(
            &header,
            &payload
        );

    if (VIDEO_CACHE_OK != status)
    {
        /*
         * No valid old cache means simply "not identical".
         */
        return VIDEO_CACHE_OK;
    }

    if ((header.payload_size == payload_size) &&
        (header.width == info->width) &&
        (header.height == info->height) &&
        (header.frame_count == info->frame_count) &&
        (header.frame_size == info->frame_size) &&
        (header.pixel_format == info->pixel_format) &&
        (header.fps_milli == info->fps_milli) &&
        (header.reserved[0] == crc32) &&
        (0U != header.reserved[0]))
    {
        *matches = 1U;
    }

    return VIDEO_CACHE_OK;
}


