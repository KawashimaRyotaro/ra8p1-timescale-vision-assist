#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <stdint.h>


#define VIDEO_SOURCE_WIDTH              (640U)
#define VIDEO_SOURCE_HEIGHT             (480U)

#define VIDEO_SOURCE_BYTES_PER_PIXEL    (2U)

#define VIDEO_SOURCE_BUFFER_COUNT       (4U)

#define VIDEO_SOURCE_FRAME_BYTES        \
    (VIDEO_SOURCE_WIDTH *               \
     VIDEO_SOURCE_HEIGHT *              \
     VIDEO_SOURCE_BYTES_PER_PIXEL)

#define VIDEO_SOURCE_INVALID_SLOT       (UINT32_MAX)


void video_source_init(void);


uint8_t * video_source_acquire_write_buffer(
    uint32_t * slot
);

void video_source_publish_write_buffer(
    uint32_t slot,
    uint32_t frame_index
);

void video_source_cancel_write_buffer(
    uint32_t slot
);


const uint8_t * video_source_acquire_read_buffer(
    uint32_t * slot,
    uint32_t * frame_index
);

void video_source_release_read_buffer(
    uint32_t slot
);

void video_source_npu_consumer_enable(void);

const uint8_t * video_source_acquire_npu_buffer(
    uint32_t * slot,
    uint32_t * frame_index
);

void video_source_release_npu_buffer(
    uint32_t slot
);


#endif /* VIDEO_SOURCE_H */