#ifndef VIDEO_CACHE_H
#define VIDEO_CACHE_H

#define VIDEO_CACHE_PIXEL_RGB565LE       (1U)
#define VIDEO_CACHE_STREAM_CHUNK_BYTES   (4096U)

#include <stdint.h>

typedef enum
{
    VIDEO_CACHE_OK = 0,
    VIDEO_CACHE_ERROR_ARGUMENT,
    VIDEO_CACHE_ERROR_STORAGE,
    VIDEO_CACHE_ERROR_MAGIC,
    VIDEO_CACHE_ERROR_VERSION,
    VIDEO_CACHE_ERROR_FORMAT,
    VIDEO_CACHE_ERROR_SIZE
} video_cache_status_t;


/*
 * 64-byte metadata header stored in OSPI flash.
 */
typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;

    uint32_t payload_offset;
    uint32_t payload_size;

    uint32_t frame_size;
    uint32_t frame_count;

    uint32_t width;
    uint32_t height;

    uint32_t pixel_format;
    uint32_t fps_milli;

    uint32_t reserved[5];
} video_cache_header_t;


typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t frame_size;
    uint32_t pixel_format;
    uint32_t fps_milli;
} video_cache_info_t;


typedef struct
{
    video_cache_info_t info;

    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t bytes_written;

    uint32_t crc32;

    uint8_t active;
} video_cache_stream_t;


video_cache_status_t video_cache_store(
    const uint8_t * payload,
    uint32_t payload_size,
    const video_cache_info_t * info
);


/*
 * Read and validate the persistent cache metadata.
 *
 * On success:
 *   header  = decoded metadata
 *   payload = memory-mapped pointer to video payload
 */
video_cache_status_t video_cache_open(
    video_cache_header_t * header,
    const uint8_t ** payload
);

video_cache_status_t video_cache_stream_begin(
    video_cache_stream_t * stream,
    const video_cache_info_t * info
);

video_cache_status_t video_cache_stream_write(
    video_cache_stream_t * stream,
    const uint8_t * data,
    uint32_t size
);

video_cache_status_t video_cache_stream_commit(
    video_cache_stream_t * stream
);

void video_cache_stream_abort(
    video_cache_stream_t * stream
);

uint32_t video_cache_crc32_update(
    uint32_t crc,
    const uint8_t * data,
    uint32_t size
);

video_cache_status_t video_cache_matches(
    const video_cache_info_t * info,
    uint32_t payload_size,
    uint32_t crc32,
    uint8_t * matches
);

#endif