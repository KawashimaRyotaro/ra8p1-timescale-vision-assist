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
 * Camera/VIN-compatible raw frame cache.
 *
 * 4 x (2048-byte stride x 168 lines) = 1,376,256 bytes.
 */
static uint8_t s_frame_buffer
    [VIDEO_SOURCE_BUFFER_COUNT]
    [VIDEO_SOURCE_FRAME_BYTES]
    __attribute__((section(".sdram_noinit"), aligned(64)));


static volatile video_source_buffer_state_t
    s_buffer_state[VIDEO_SOURCE_BUFFER_COUNT];


/*
 * Per-consumer ownership.
 *
 * pending  = this consumer still needs the published frame
 * acquired = this consumer currently owns the frame for reading
 */
static volatile uint8_t
    s_display_pending[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_display_acquired[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_npu_pending[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_npu_acquired[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_motion_pending[VIDEO_SOURCE_BUFFER_COUNT];

static volatile uint8_t
    s_motion_acquired[VIDEO_SOURCE_BUFFER_COUNT];


static volatile uint8_t
    s_display_consumer_enabled = 0U;

static volatile uint8_t
    s_npu_consumer_enabled = 0U;

static volatile uint8_t
    s_motion_consumer_enabled = 0U;


static uint32_t
    s_frame_index[VIDEO_SOURCE_BUFFER_COUNT];

static uint32_t
    s_next_write_slot = 0U;

/*
 * NPU is a latest-frame consumer.
 *
 * While Ethos-U55 is busy, newly published frames replace the
 * previous waiting NPU frame instead of accumulating NPU ownership
 * on every raw frame-cache slot.
 */
static volatile uint32_t
    s_npu_latest_slot = VIDEO_SOURCE_INVALID_SLOT;

volatile uint32_t g_video_npu_superseded_count = 0U;


static void video_source_try_make_empty(
    uint32_t slot)
{
    if ((0U == s_display_pending[slot]) &&
        (0U == s_npu_pending[slot]) &&
        (0U == s_motion_pending[slot]))
    {
        s_buffer_state[slot] =
            VIDEO_SOURCE_BUFFER_EMPTY;

        __DMB();
    }
}


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

        s_motion_pending[slot]   = 0U;
        s_motion_acquired[slot]  = 0U;

        s_frame_index[slot] =
            UINT32_MAX;
    }

    s_next_write_slot = 0U;
    s_npu_latest_slot = VIDEO_SOURCE_INVALID_SLOT;
    g_video_npu_superseded_count = 0U;

    s_display_consumer_enabled = 0U;
    s_npu_consumer_enabled     = 0U;
    s_motion_consumer_enabled  = 0U;

    __DMB();
}


uint8_t * video_source_acquire_write_buffer(
    uint32_t * slot)
{
    if (NULL == slot)
    {
        return NULL;
    }

    for (uint32_t offset = 0U;
         offset < VIDEO_SOURCE_BUFFER_COUNT;
         offset++)
    {
        uint32_t const candidate =
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

    s_frame_index[slot] =
        frame_index;

    s_display_pending[slot] =
        s_display_consumer_enabled;

    s_display_acquired[slot] =
        0U;

    /*
     * NPU policy: keep only the newest waiting frame.
     *
     * If the previous latest frame is still waiting (not currently
     * acquired by the NPU worker), drop only the NPU reference to that
     * old frame. Display/Motion references, if any, remain untouched.
     */
    if (0U != s_npu_consumer_enabled)
    {
        uint32_t const previous_latest =
            s_npu_latest_slot;

        if ((VIDEO_SOURCE_INVALID_SLOT != previous_latest) &&
            (previous_latest != slot) &&
            (0U != s_npu_pending[previous_latest]) &&
            (0U == s_npu_acquired[previous_latest]))
        {
            s_npu_pending[previous_latest] = 0U;

            g_video_npu_superseded_count++;

            __DMB();

            video_source_try_make_empty(
                previous_latest
            );
        }

        s_npu_pending[slot] =
            1U;

        s_npu_acquired[slot] =
            0U;

        s_npu_latest_slot =
            slot;
    }
    else
    {
        s_npu_pending[slot] =
            0U;

        s_npu_acquired[slot] =
            0U;
    }

    s_motion_pending[slot] =
        s_motion_consumer_enabled;

    s_motion_acquired[slot] =
        0U;

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


void video_source_display_consumer_enable(void)
{
    s_display_consumer_enabled =
        1U;

    __DMB();
}


void video_source_display_consumer_disable(void)
{
    s_display_consumer_enabled =
        0U;

    __DMB();
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
            (0U == s_display_acquired[candidate]) &&
            (s_frame_index[candidate] <
             selected_frame))
        {
            selected_frame =
                s_frame_index[candidate];

            selected_slot =
                candidate;
        }
    }

    if (VIDEO_SOURCE_INVALID_SLOT ==
        selected_slot)
    {
        return NULL;
    }

    s_display_acquired[selected_slot] =
        1U;

    __DMB();

    *slot =
        selected_slot;

    *frame_index =
        selected_frame;

    return
        s_frame_buffer[selected_slot];
}


void video_source_release_read_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (0U !=
        s_display_acquired[slot])
    {
        s_display_acquired[slot] =
            0U;

        __DMB();

        s_display_pending[slot] =
            0U;

        __DMB();

        video_source_try_make_empty(
            slot
        );
    }
}


void video_source_npu_consumer_enable(void)
{
    s_npu_consumer_enabled =
        1U;

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

    /*
     * Consume only the latest frame reserved for NPU.
     *
     * Do not scan for the oldest pending frame.  The semantic path is
     * intentionally allowed to skip frames while Ethos-U55 is busy.
     */
    uint32_t const selected_slot =
        s_npu_latest_slot;

    if ((VIDEO_SOURCE_INVALID_SLOT ==
         selected_slot) ||
        (selected_slot >=
         VIDEO_SOURCE_BUFFER_COUNT))
    {
        return NULL;
    }

    if ((VIDEO_SOURCE_BUFFER_PUBLISHED !=
         s_buffer_state[selected_slot]) ||
        (0U == s_npu_pending[selected_slot]) ||
        (0U != s_npu_acquired[selected_slot]))
    {
        return NULL;
    }

    s_npu_acquired[selected_slot] =
        1U;

    __DMB();

    *slot =
        selected_slot;

    *frame_index =
        s_frame_index[selected_slot];

    return
        s_frame_buffer[selected_slot];
}


void video_source_release_npu_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (0U !=
        s_npu_acquired[slot])
    {
        s_npu_acquired[slot] =
            0U;

        __DMB();

        s_npu_pending[slot] =
            0U;

        /*
         * If no newer frame was published while preprocessing this
         * frame, there is currently no waiting NPU frame.
         */
        if (s_npu_latest_slot ==
            slot)
        {
            s_npu_latest_slot =
                VIDEO_SOURCE_INVALID_SLOT;
        }

        __DMB();

        video_source_try_make_empty(
            slot
        );
    }
}


void video_source_motion_consumer_enable(void)
{
    s_motion_consumer_enabled =
        1U;

    __DMB();
}


const uint8_t * video_source_acquire_motion_buffer(
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
            (0U != s_motion_pending[candidate]) &&
            (0U == s_motion_acquired[candidate]) &&
            (s_frame_index[candidate] <
             selected_frame))
        {
            selected_frame =
                s_frame_index[candidate];

            selected_slot =
                candidate;
        }
    }

    if (VIDEO_SOURCE_INVALID_SLOT ==
        selected_slot)
    {
        return NULL;
    }

    s_motion_acquired[selected_slot] =
        1U;

    __DMB();

    *slot =
        selected_slot;

    *frame_index =
        selected_frame;

    return
        s_frame_buffer[selected_slot];
}


void video_source_release_motion_buffer(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return;
    }

    if (0U !=
        s_motion_acquired[slot])
    {
        s_motion_acquired[slot] =
            0U;

        __DMB();

        s_motion_pending[slot] =
            0U;

        __DMB();

        video_source_try_make_empty(
            slot
        );
    }
}
