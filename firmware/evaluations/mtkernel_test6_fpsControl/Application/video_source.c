#include <stddef.h>
#include <stdint.h>

#include "hal_data.h"
#include "video_source.h"


typedef enum
{
    VIDEO_SOURCE_BUFFER_EMPTY = 0,
    VIDEO_SOURCE_BUFFER_WRITING,
    VIDEO_SOURCE_BUFFER_READY,
    VIDEO_SOURCE_BUFFER_READING

} video_source_buffer_state_t;


typedef struct
{
    volatile video_source_buffer_state_t state;
    uint32_t frame_index;

} video_source_buffer_meta_t;


/*
 * Input video frame buffers.
 *
 * Each buffer:
 *   640 x 480 x 3 bytes
 *   = 921600 bytes
 *
 * Two buffers:
 *   1843200 bytes
 *
 * These buffers are separate from the GLCDC framebuffers.
 */
static uint8_t s_frame_buffer
    [VIDEO_SOURCE_BUFFER_COUNT]
    [VIDEO_SOURCE_FRAME_BYTES]
    __attribute__((section(".sdram_noinit"), aligned(32)));


/*
 * Buffer state and frame number.
 *
 * The metadata itself is small, so it remains in normal internal RAM.
 */
static video_source_buffer_meta_t
    s_buffer_meta[VIDEO_SOURCE_BUFFER_COUNT];


/*
 * Slot selected for the next USB write.
 *
 * 0 -> 1 -> 0 -> 1 ...
 */
static uint32_t s_next_write_slot = 0U;


/*
 * Initialize both SDRAM frame-buffer states.
 */
void video_source_init(void)
{
    for (uint32_t i = 0U;
         i < VIDEO_SOURCE_BUFFER_COUNT;
         i++)
    {
        s_buffer_meta[i].state =
            VIDEO_SOURCE_BUFFER_EMPTY;

        s_buffer_meta[i].frame_index = 0U;
    }

    s_next_write_slot = 0U;

    __DMB();
}


/*
 * USB producer:
 *
 * Obtain the next empty SDRAM frame buffer.
 *
 * Return:
 *   buffer pointer : writable buffer available
 *   NULL           : next buffer is still in use
 */
uint8_t * video_source_acquire_write_buffer(
    uint32_t * slot)
{
    if (NULL == slot)
    {
        return NULL;
    }

    uint32_t candidate =
        s_next_write_slot;

    if (VIDEO_SOURCE_BUFFER_EMPTY !=
        s_buffer_meta[candidate].state)
    {
        return NULL;
    }

    s_buffer_meta[candidate].state =
        VIDEO_SOURCE_BUFFER_WRITING;

    __DMB();

    *slot = candidate;

    return s_frame_buffer[candidate];
}


/*
 * USB producer:
 *
 * Publish a completely written frame to the display task.
 */
void video_source_publish_write_buffer(
    uint32_t slot,
    uint32_t frame_index)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (VIDEO_SOURCE_BUFFER_WRITING !=
        s_buffer_meta[slot].state)
    {
        return;
    }

    s_buffer_meta[slot].frame_index =
        frame_index;

    /*
     * Complete all frame writes before making the frame READY.
     */
    __DMB();

    s_buffer_meta[slot].state =
        VIDEO_SOURCE_BUFFER_READY;

    __DMB();

    s_next_write_slot =
        (slot + 1U) %
        VIDEO_SOURCE_BUFFER_COUNT;
}


/*
 * USB producer:
 *
 * Return a buffer to EMPTY if ff_fread() failed before publishing it.
 */
void video_source_cancel_write_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (VIDEO_SOURCE_BUFFER_WRITING ==
        s_buffer_meta[slot].state)
    {
        s_buffer_meta[slot].state =
            VIDEO_SOURCE_BUFFER_EMPTY;

        __DMB();
    }
}


/*
 * Display consumer:
 *
 * Obtain the oldest READY frame.
 *
 * Return:
 *   buffer pointer : READY frame available
 *   NULL           : no complete frame available
 */
const uint8_t * video_source_acquire_read_buffer(
    uint32_t * slot,
    uint32_t * frame_index)
{
    if ((NULL == slot) ||
        (NULL == frame_index))
    {
        return NULL;
    }

    uint32_t selected_slot =
        VIDEO_SOURCE_BUFFER_COUNT;

    uint32_t selected_frame =
        UINT32_MAX;

    /*
     * Usually only one frame is READY.
     *
     * If both are READY, select the older one.
     */
    for (uint32_t i = 0U;
         i < VIDEO_SOURCE_BUFFER_COUNT;
         i++)
    {
        if ((VIDEO_SOURCE_BUFFER_READY ==
             s_buffer_meta[i].state) &&
            (s_buffer_meta[i].frame_index <
             selected_frame))
        {
            selected_slot = i;

            selected_frame =
                s_buffer_meta[i].frame_index;
        }
    }

    if (selected_slot >=
        VIDEO_SOURCE_BUFFER_COUNT)
    {
        return NULL;
    }

    s_buffer_meta[selected_slot].state =
        VIDEO_SOURCE_BUFFER_READING;

    __DMB();

    *slot = selected_slot;
    *frame_index = selected_frame;

    return s_frame_buffer[selected_slot];
}


/*
 * Display consumer:
 *
 * Release a frame after its pixels have been copied to a GLCDC buffer.
 */
void video_source_release_read_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (VIDEO_SOURCE_BUFFER_READING ==
        s_buffer_meta[slot].state)
    {
        __DMB();

        s_buffer_meta[slot].state =
            VIDEO_SOURCE_BUFFER_EMPTY;

        __DMB();
    }
}