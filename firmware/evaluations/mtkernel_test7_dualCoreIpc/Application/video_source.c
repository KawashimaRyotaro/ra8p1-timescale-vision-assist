#include <stddef.h>
#include <stdint.h>

#include "hal_data.h"
#include "video_source.h"


typedef enum
{
    VIDEO_SOURCE_BUFFER_EMPTY = 0,
    VIDEO_SOURCE_BUFFER_WRITING,
    VIDEO_SOURCE_BUFFER_PUBLISHED

} video_source_buffer_state_t;


/*
 * GLCDC requires framebuffer addresses to be aligned
 * to a 64-byte boundary.
 *
 * VIDEO_SOURCE_FRAME_BYTES = 614400,
 * which is also a multiple of 64 bytes.
 */
static uint8_t s_frame_buffer
    [VIDEO_SOURCE_BUFFER_COUNT]
    [VIDEO_SOURCE_FRAME_BYTES]
    __attribute__((section(".sdram_noinit"), aligned(64)));


static volatile video_source_buffer_state_t
    s_buffer_state[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_display_pending[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_display_acquired[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_npu_pending[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_npu_acquired[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_npu_consumer_enabled = 0U;

static volatile uint8_t
    s_display_consumer_enabled = 0U;

static uint32_t
    s_frame_index[VIDEO_SOURCE_BUFFER_COUNT];


static uint32_t s_next_write_slot = 0U;


void video_source_init(void)
{
    for (uint32_t slot = 0U;
         slot < VIDEO_SOURCE_BUFFER_COUNT;
         slot++)
    {
        s_buffer_state[slot] =
            VIDEO_SOURCE_BUFFER_EMPTY;

        s_display_pending[slot]  = 0U;
        s_display_acquired[slot] = 0U;

        s_npu_pending[slot]      = 0U;
        s_npu_acquired[slot]     = 0U;
    }

    s_next_write_slot = 0U;
    s_npu_consumer_enabled = 0U;

    __DMB();
}


uint8_t * video_source_acquire_write_buffer(
    uint32_t * slot)
{
    if (NULL == slot)
    {
        return NULL;
    }

    /*
     * Search all three buffers.
     *
     * Do not wait only for one predetermined slot:
     * Difference/Display can intentionally keep one buffer
     * in READING state as the previous frame.
     */
    for (uint32_t offset = 0U;
         offset < VIDEO_SOURCE_BUFFER_COUNT;
         offset++)
    {
        uint32_t candidate =
            (s_next_write_slot + offset) %
            VIDEO_SOURCE_BUFFER_COUNT;

        if (VIDEO_SOURCE_BUFFER_EMPTY ==
            s_buffer_state[candidate])
        {
            s_buffer_state[candidate] =
                VIDEO_SOURCE_BUFFER_WRITING;

            __DMB();

            *slot = candidate;

            s_next_write_slot =
                (candidate + 1U) %
                VIDEO_SOURCE_BUFFER_COUNT;

            return s_frame_buffer[candidate];
        }
    }

    return NULL;
}


void video_source_publish_write_buffer(
    uint32_t slot,
    uint32_t frame_index)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (VIDEO_SOURCE_BUFFER_WRITING !=
        s_buffer_state[slot])
    {
        return;
    }

    s_frame_index[slot] = frame_index;

    /* Display always consumes every published frame. */
    s_display_pending[slot] =
        s_display_consumer_enabled;

    s_display_acquired[slot] = 0U;

    /*
    * NPU becomes a second consumer only after
    * video_source_npu_consumer_enable().
    */
    s_npu_pending[slot] =
        s_npu_consumer_enabled;

    s_npu_acquired[slot] = 0U;

    __DMB();

    s_buffer_state[slot] =
        VIDEO_SOURCE_BUFFER_PUBLISHED;

    __DMB();
}


void video_source_cancel_write_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (VIDEO_SOURCE_BUFFER_WRITING ==
        s_buffer_state[slot])
    {
        __DMB();

        s_buffer_state[slot] =
            VIDEO_SOURCE_BUFFER_EMPTY;

        __DMB();
    }
}


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
        VIDEO_SOURCE_INVALID_SLOT;

    uint32_t selected_frame =
        UINT32_MAX;

    for (uint32_t candidate = 0U;
         candidate < VIDEO_SOURCE_BUFFER_COUNT;
         candidate++)
    {
        if ((VIDEO_SOURCE_BUFFER_PUBLISHED ==
             s_buffer_state[candidate]) &&
            (0U != s_display_pending[candidate]) &&
            (0U == s_display_acquired[candidate]))
        {
            if (s_frame_index[candidate] <
                selected_frame)
            {
                selected_frame =
                    s_frame_index[candidate];

                selected_slot =
                    candidate;
            }
        }
    }

    if (VIDEO_SOURCE_INVALID_SLOT ==
        selected_slot)
    {
        return NULL;
    }

    s_display_acquired[selected_slot] = 1U;

    __DMB();

    *slot        = selected_slot;
    *frame_index = selected_frame;

    return s_frame_buffer[selected_slot];
}


void video_source_release_read_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (0U != s_display_acquired[slot])
    {
        s_display_acquired[slot] = 0U;

        __DMB();

        s_display_pending[slot] = 0U;

        __DMB();

        if ((0U == s_display_pending[slot]) &&
            (0U == s_npu_pending[slot]))
        {
            s_buffer_state[slot] =
                VIDEO_SOURCE_BUFFER_EMPTY;

            __DMB();
        }
    }
}


void video_source_npu_consumer_enable(void)
{
    s_npu_consumer_enabled = 1U;

    __DMB();
}


const uint8_t * video_source_acquire_npu_buffer(
    uint32_t * slot,
    uint32_t * frame_index)
{
    if ((NULL == slot) ||
        (NULL == frame_index))
    {
        return NULL;
    }

    uint32_t selected_slot =
        VIDEO_SOURCE_INVALID_SLOT;

    uint32_t selected_frame =
        UINT32_MAX;

    for (uint32_t candidate = 0U;
         candidate < VIDEO_SOURCE_BUFFER_COUNT;
         candidate++)
    {
        if ((VIDEO_SOURCE_BUFFER_PUBLISHED ==
             s_buffer_state[candidate]) &&
            (0U != s_npu_pending[candidate]) &&
            (0U == s_npu_acquired[candidate]))
        {
            if (s_frame_index[candidate] <
                selected_frame)
            {
                selected_frame =
                    s_frame_index[candidate];

                selected_slot =
                    candidate;
            }
        }
    }

    if (VIDEO_SOURCE_INVALID_SLOT ==
        selected_slot)
    {
        return NULL;
    }

    s_npu_acquired[selected_slot] = 1U;

    __DMB();

    *slot        = selected_slot;
    *frame_index = selected_frame;

    return s_frame_buffer[selected_slot];
}


void video_source_release_npu_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (0U != s_npu_acquired[slot])
    {
        s_npu_acquired[slot] = 0U;

        __DMB();

        s_npu_pending[slot] = 0U;

        __DMB();

        if ((0U == s_display_pending[slot]) &&
            (0U == s_npu_pending[slot]))
        {
            s_buffer_state[slot] =
                VIDEO_SOURCE_BUFFER_EMPTY;

            __DMB();
        }
    }
}


void video_source_display_consumer_enable(void)
{
    s_display_consumer_enabled = 1U;
    __DMB();
}


void video_source_display_consumer_disable(void)
{
    s_display_consumer_enabled = 0U;
    __DMB();
}