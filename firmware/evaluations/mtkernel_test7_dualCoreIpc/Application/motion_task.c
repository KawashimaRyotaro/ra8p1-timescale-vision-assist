#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "motion_task.h"
#include "video_source.h"


/*
 * Emergency temporal path must run before display/USB/NPU tasks,
 * but CP1 only performs a short 112x84 conversion before yielding.
 *
 * Existing priorities:
 *   Display = 10
 *   USB     = 11
 *   NPU     = 12
 */
#define MOTION_TASK_PRIORITY       (9)
#define MOTION_TASK_STACK_SIZE     (2048)


uint8_t g_motion_gray_buffers
    [MOTION_GRAY_BUFFER_COUNT]
    [MOTION_GRAY_PIXELS];

volatile uint32_t g_motion_gray_frame_count =
    0U;

volatile uint32_t g_motion_gray_last_frame_index =
    UINT32_MAX;

volatile uint32_t g_motion_gray_last_buffer =
    0U;

volatile uint32_t g_motion_gray_consecutive_count =
    0U;

volatile uint32_t g_motion_gray_gap_count =
    0U;

volatile uint32_t g_motion_gray_last_checksum =
    0U;


static ID s_motion_task_id =
    0;

static uint8_t s_write_buffer =
    0U;


static void motion_task_entry(
    INT stacd,
    void * exinf);


static T_CTSK s_motion_task_ctsk =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = motion_task_entry,
    .itskpri = MOTION_TASK_PRIORITY,
    .stksz   = MOTION_TASK_STACK_SIZE,
};


static uint32_t motion_gray_checksum(
    const uint8_t * gray)
{
    /*
     * FNV-1a: used only for sparse CP1 debug output.
     */
    uint32_t hash =
        2166136261UL;

    for (uint32_t i = 0U;
         i < MOTION_GRAY_PIXELS;
         i++)
    {
        hash ^=
            gray[i];

        hash *=
            16777619UL;
    }

    return hash;
}


static void motion_gray_print_checkpoint(
    uint32_t frame_index,
    uint32_t buffer_index)
{
    uint8_t const * const gray =
        g_motion_gray_buffers[buffer_index];

    uint32_t sum =
        0U;

    uint32_t min_value =
        255U;

    uint32_t max_value =
        0U;

    for (uint32_t i = 0U;
         i < MOTION_GRAY_PIXELS;
         i++)
    {
        uint32_t const value =
            gray[i];

        sum +=
            value;

        if (value < min_value)
        {
            min_value =
                value;
        }

        if (value > max_value)
        {
            max_value =
                value;
        }
    }

    uint32_t const checksum =
        motion_gray_checksum(
            gray
        );

    g_motion_gray_last_checksum =
        checksum;

    tm_printf(
        (UB *)"[Motion][CP1] "
              "frame=%u buf=%u gray=%ux%u "
              "min=%u max=%u mean=%u "
              "checksum=0x%08X "
              "count=%u consecutive=%u gaps=%u\n",
        frame_index,
        buffer_index,
        MOTION_GRAY_WIDTH,
        MOTION_GRAY_HEIGHT,
        min_value,
        max_value,
        sum / MOTION_GRAY_PIXELS,
        checksum,
        g_motion_gray_frame_count,
        g_motion_gray_consecutive_count,
        g_motion_gray_gap_count
    );
}


ER motion_task_create(void)
{
    ID const task_id =
        tk_cre_tsk(
            &s_motion_task_ctsk
        );

    if (task_id < E_OK)
    {
        return
            (ER) task_id;
    }

    s_motion_task_id =
        task_id;

    return
        E_OK;
}


ER motion_task_start(void)
{
    if (s_motion_task_id <= 0)
    {
        return
            E_ID;
    }

    return
        tk_sta_tsk(
            s_motion_task_id,
            0
        );
}


static void motion_task_entry(
    INT stacd,
    void * exinf)
{
    (void) stacd;
    (void) exinf;

    tm_putstring(
        (UB *)"[Motion] task started.\n"
    );

    /*
     * From this point every newly published raw frame also has a
     * motion-consumer reference.
     */
    video_source_motion_consumer_enable();

    while (1)
    {
        uint32_t slot =
            VIDEO_SOURCE_INVALID_SLOT;

        uint32_t frame_index =
            0U;

        const uint8_t * const frame =
            video_source_acquire_motion_buffer(
                &slot,
                &frame_index
            );

        if (NULL == frame)
        {
            tk_dly_tsk(1);
            continue;
        }


        if (0U == frame_index)
        {
            g_motion_gray_frame_count =
                0U;

            g_motion_gray_last_frame_index =
                UINT32_MAX;

            g_motion_gray_last_buffer =
                0U;

            g_motion_gray_consecutive_count =
                0U;

            g_motion_gray_gap_count =
                0U;

            g_motion_gray_last_checksum =
                0U;

            s_write_buffer =
                0U;
        }


        uint32_t const buffer_index =
            s_write_buffer;


        /*
         * CP1:
         * 224x168 RGB565 / 2048-byte stride
         *     -> 112x84 uint8 grayscale.
         */
        motion_preprocess_rgb565_to_gray_half(
            frame,
            VIDEO_SOURCE_STRIDE_BYTES,
            g_motion_gray_buffers[buffer_index]
        );


        /*
         * Raw frame ownership ends immediately after grayscale copy.
         * Later block matching operates only on these compact buffers.
         */
        video_source_release_motion_buffer(
            slot
        );


        if (UINT32_MAX !=
            g_motion_gray_last_frame_index)
        {
            if (frame_index ==
                (g_motion_gray_last_frame_index + 1U))
            {
                g_motion_gray_consecutive_count++;
            }
            else
            {
                g_motion_gray_gap_count++;
            }
        }


        g_motion_gray_last_buffer =
            buffer_index;

        g_motion_gray_last_frame_index =
            frame_index;

        g_motion_gray_frame_count++;


        /*
         * Sparse logging only.
         */
        if ((0U == frame_index) ||
            (1U == frame_index) ||
            (0U == (frame_index % 100U)) ||
            (299U == frame_index))
        {
            motion_gray_print_checkpoint(
                frame_index,
                buffer_index
            );
        }


        s_write_buffer ^=
            1U;
    }
}
