#include <stdint.h>
#include <limits.h>

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "fps_control.h"


#define FPS_CONTROL_REPORT_FRAME_COUNT    (300U)


static uint32_t s_target_fps = FPS_CONTROL_UNLIMITED;

static uint32_t s_period_base_ms = 0U;
static uint32_t s_period_remainder = 0U;
static uint32_t s_period_remainder_accumulator = 0U;

static uint64_t s_next_deadline_ms = 0U;

static uint8_t s_schedule_started = 0U;


/*
 * Measurement state.
 */
static uint8_t s_measurement_started = 0U;

static uint64_t s_measurement_start_ms = 0U;
static uint64_t s_previous_frame_ms = 0U;

static uint32_t s_measurement_frame_count = 0U;

static uint32_t s_min_interval_ms = UINT32_MAX;
static uint32_t s_max_interval_ms = 0U;

static uint32_t s_deadline_miss_count = 0U;

/*
 * USB read profiling.
 */
static uint64_t s_usb_read_start_ms = 0U;
static uint64_t s_usb_read_total_ms = 0U;

static uint32_t s_usb_read_count = 0U;
static uint32_t s_usb_read_min_ms = UINT32_MAX;
static uint32_t s_usb_read_max_ms = 0U;


/*
 * Display processing profiling.
 */
static uint64_t s_display_start_ms = 0U;
static uint64_t s_display_total_ms = 0U;

static uint32_t s_display_count = 0U;
static uint32_t s_display_min_ms = UINT32_MAX;
static uint32_t s_display_max_ms = 0U;


/*
 * Get monotonically increasing system operating time.
 */
static uint8_t fps_control_get_time_ms(
    uint64_t * time_ms)
{
    if (NULL == time_ms)
    {
        return 0U;
    }

    SYSTIM system_time;

    ER err =
        tk_get_otm(
            &system_time
        );

    if (err < E_OK)
    {
        return 0U;
    }

    *time_ms =
        ((uint64_t) (uint32_t) system_time.hi << 32) |
        (uint64_t) system_time.lo;

    return 1U;
}


/*
 * Configure frame-rate controller.
 */
void fps_control_init(
    uint32_t target_fps)
{
    s_schedule_started = 0U;

    s_measurement_started = 0U;
    s_measurement_frame_count = 0U;

    s_deadline_miss_count = 0U;

    fps_control_set_target_fps(
        target_fps
    );
}


/*
 * Change target FPS.
 *
 * 0 means unlimited.
 */
void fps_control_set_target_fps(
    uint32_t target_fps)
{
    s_target_fps = target_fps;

    s_schedule_started = 0U;

    s_period_remainder_accumulator = 0U;


    if (FPS_CONTROL_UNLIMITED ==
        target_fps)
    {
        s_period_base_ms = 0U;
        s_period_remainder = 0U;

        return;
    }


    /*
    * Frame period in milliseconds.
    *
    * Example:
    *
    * 30 fps:
    *   1000 / 30 = 33 ms, remainder 10
    *
    * Therefore the controller automatically produces
    * 33 ms / 34 ms periods so that the long-term
    * average becomes exactly 30 fps.
    *
    * 60 fps similarly becomes 16 ms / 17 ms.
    */
    s_period_base_ms =
        1000U /
        target_fps;

    s_period_remainder =
        1000U %
        target_fps;
}


/*
 * Wait until the scheduled start of the next frame.
 *
 * Unlimited mode:
 *     immediately return.
 *
 * Controlled mode:
 *     use an absolute deadline instead of
 *     "processing time + fixed delay".
 */
void fps_control_wait_before_frame(void)
{
    if (FPS_CONTROL_UNLIMITED ==
        s_target_fps)
    {
        return;
    }


    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }


    /*
     * First frame establishes the schedule origin.
     */
    if (0U == s_schedule_started)
    {
        s_next_deadline_ms = now_ms;

        s_schedule_started = 1U;

        return;
    }


    uint32_t period_ms =
        s_period_base_ms;


    /*
     * Fractional-period correction.
     */
    s_period_remainder_accumulator +=
        s_period_remainder;

    if (s_period_remainder_accumulator >=
        s_target_fps)
    {
        period_ms++;

        s_period_remainder_accumulator -=
            s_target_fps;
    }


    /*
     * IMPORTANT:
     *
     * Advance from the previous deadline,
     * not from the current time.
     *
     * Therefore processing time does not accumulate
     * as long-term frame-rate drift.
     */
    s_next_deadline_ms +=
        period_ms;


    if (now_ms <
        s_next_deadline_ms)
    {
        uint64_t wait_ms =
            s_next_deadline_ms -
            now_ms;

        (void) tk_dly_tsk(
            (RELTIM) wait_ms
        );
    }
    else
    {
        /*
         * Processing was slower than the requested FPS.
         */
        s_deadline_miss_count++;
    }
}


void fps_control_usb_read_begin(
    uint32_t frame_index)
{
    if (0U == frame_index)
    {
        s_usb_read_total_ms = 0U;
        s_usb_read_count = 0U;
        s_usb_read_min_ms = UINT32_MAX;
        s_usb_read_max_ms = 0U;
    }


    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }

    s_usb_read_start_ms = now_ms;
}


void fps_control_usb_read_end(void)
{
    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }


    uint32_t elapsed_ms =
        (uint32_t)
        (now_ms -
         s_usb_read_start_ms);


    s_usb_read_total_ms +=
        elapsed_ms;

    s_usb_read_count++;


    if (elapsed_ms <
        s_usb_read_min_ms)
    {
        s_usb_read_min_ms =
            elapsed_ms;
    }

    if (elapsed_ms >
        s_usb_read_max_ms)
    {
        s_usb_read_max_ms =
            elapsed_ms;
    }
}


void fps_control_display_begin(
    uint32_t frame_index)
{
    if (0U == frame_index)
    {
        s_display_total_ms = 0U;
        s_display_count = 0U;
        s_display_min_ms = UINT32_MAX;
        s_display_max_ms = 0U;
    }


    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }

    s_display_start_ms = now_ms;
}


void fps_control_display_end(void)
{
    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }


    uint32_t elapsed_ms =
        (uint32_t)
        (now_ms -
         s_display_start_ms);


    s_display_total_ms +=
        elapsed_ms;

    s_display_count++;


    if (elapsed_ms <
        s_display_min_ms)
    {
        s_display_min_ms =
            elapsed_ms;
    }

    if (elapsed_ms >
        s_display_max_ms)
    {
        s_display_max_ms =
            elapsed_ms;
    }
}


/*
 * Record the actual time at which one frame
 * has completed display processing.
 */
void fps_control_frame_presented(
    uint32_t frame_index)
{
    uint64_t now_ms = 0U;

    if (0U ==
        fps_control_get_time_ms(
            &now_ms))
    {
        return;
    }


    /*
     * frame_index == 0 marks the beginning
     * of a new RAW video.
     */
    if ((0U == frame_index) ||
        (0U == s_measurement_started))
    {
        s_measurement_started = 1U;

        s_measurement_start_ms =
            now_ms;

        s_previous_frame_ms =
            now_ms;

        s_measurement_frame_count = 1U;

        s_min_interval_ms =
            UINT32_MAX;

        s_max_interval_ms = 0U;

        s_deadline_miss_count = 0U;

        return;
    }


    uint32_t interval_ms =
        (uint32_t)
        (now_ms -
        s_previous_frame_ms);

    s_previous_frame_ms =
        now_ms;


    if (interval_ms <
        s_min_interval_ms)
    {
        s_min_interval_ms =
            interval_ms;
    }

    if (interval_ms >
        s_max_interval_ms)
    {
        s_max_interval_ms =
            interval_ms;
    }


    s_measurement_frame_count++;


    /*
     * Current RAW test videos contain 300 frames.
     *
     * Measure frame 0 -> frame 299:
     * 299 frame intervals.
     */
    if (FPS_CONTROL_REPORT_FRAME_COUNT ==
        s_measurement_frame_count)
    {
        uint32_t elapsed_ms =
            (uint32_t)
            (now_ms -
            s_measurement_start_ms);

        uint32_t interval_count =
            FPS_CONTROL_REPORT_FRAME_COUNT -
            1U;


        uint32_t average_interval_ms =
            elapsed_ms /
            interval_count;


        if (0U == elapsed_ms)
        {
            s_measurement_started = 0U;
            return;
        }


        /*
        * fps_x1000:
        *
        * actual_fps =
        *     intervals * 1000 / elapsed_ms
        *
        * Therefore:
        *
        * actual_fps x 1000 =
        *     intervals * 1,000,000 / elapsed_ms
        */
        uint32_t fps_x1000 =
            (uint32_t)
            (
                ((uint64_t) interval_count *
                1000000ULL) /
                elapsed_ms
            );


        tm_printf(
            (UB *)"[FPS] target=%u "
                "frames=%u "
                "elapsed_ms=%u "
                "avg_interval_ms=%u "
                "min_interval_ms=%u "
                "max_interval_ms=%u "
                "fps=%u.%03u "
                "deadline_miss=%u\n",
            s_target_fps,
            FPS_CONTROL_REPORT_FRAME_COUNT,
            elapsed_ms,
            average_interval_ms,
            s_min_interval_ms,
            s_max_interval_ms,
            fps_x1000 / 1000U,
            fps_x1000 % 1000U,
            s_deadline_miss_count
        );

        if (s_usb_read_count > 0U)
        {
            tm_printf(
                (UB *)"[Profile][USB] "
                    "count=%u "
                    "avg_ms=%u "
                    "min_ms=%u "
                    "max_ms=%u "
                    "total_ms=%u\n",
                s_usb_read_count,
                (uint32_t)
                (s_usb_read_total_ms /
                s_usb_read_count),
                s_usb_read_min_ms,
                s_usb_read_max_ms,
                (uint32_t) s_usb_read_total_ms
            );
        }


        if (s_display_count > 0U)
        {
            tm_printf(
                (UB *)"[Profile][Display] "
                    "count=%u "
                    "avg_ms=%u "
                    "min_ms=%u "
                    "max_ms=%u "
                    "total_ms=%u\n",
                s_display_count,
                (uint32_t)
                (s_display_total_ms /
                s_display_count),
                s_display_min_ms,
                s_display_max_ms,
                (uint32_t) s_display_total_ms
            );
        }


        s_measurement_started = 0U;
    }
}