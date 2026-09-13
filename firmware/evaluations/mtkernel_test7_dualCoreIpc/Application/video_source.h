#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <stdint.h>


#define VIDEO_SOURCE_WIDTH              (224U)
#define VIDEO_SOURCE_HEIGHT             (168U)

#define VIDEO_SOURCE_BYTES_PER_PIXEL    (2U)

/*
 * Camera/VIN Frame Cache layout.
 */
#define VIDEO_SOURCE_STRIDE_BYTES       (2048U)

/*
 * Actual valid RGB565 image bytes in one line.
 * 224 pixels x 2 bytes = 448 bytes
 */
#define VIDEO_SOURCE_ACTIVE_LINE_BYTES  \
    (VIDEO_SOURCE_WIDTH * VIDEO_SOURCE_BYTES_PER_PIXEL)

/*
 * Size of one tightly-packed frame stored in .Y56.
 * 448 x 168 = 75264 bytes
 */
#define VIDEO_SOURCE_FILE_FRAME_BYTES   \
    (VIDEO_SOURCE_ACTIVE_LINE_BYTES * VIDEO_SOURCE_HEIGHT)

/*
 * Size occupied by one VIN-compatible frame in SDRAM.
 * 2048 x 168 = 344064 bytes
 */
#define VIDEO_SOURCE_FRAME_BYTES        \
    (VIDEO_SOURCE_STRIDE_BYTES * VIDEO_SOURCE_HEIGHT)


#define VIDEO_SOURCE_BUFFER_COUNT       (4U)
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


/*
 * Debug-display consumer.
 */
void video_source_display_consumer_enable(void);
void video_source_display_consumer_disable(void);

const uint8_t * video_source_acquire_read_buffer(
    uint32_t * slot,
    uint32_t * frame_index
);

void video_source_release_read_buffer(
    uint32_t slot
);


/*
 * YOLOX/NPU consumer.
 */
void video_source_npu_consumer_enable(void);

/* Number of waiting NPU frames replaced by newer frames. */
extern volatile uint32_t g_video_npu_superseded_count;

const uint8_t * video_source_acquire_npu_buffer(
    uint32_t * slot,
    uint32_t * frame_index
);

void video_source_release_npu_buffer(
    uint32_t slot
);


/*
 * Emergency temporal-motion consumer.
 *
 * CP1 uses this path to copy each raw RGB565 frame into a compact
 * 112x84 grayscale buffer and then immediately releases the raw slot.
 */
void video_source_motion_consumer_enable(void);

const uint8_t * video_source_acquire_motion_buffer(
    uint32_t * slot,
    uint32_t * frame_index
);

void video_source_release_motion_buffer(
    uint32_t slot
);


#endif /* VIDEO_SOURCE_H */
