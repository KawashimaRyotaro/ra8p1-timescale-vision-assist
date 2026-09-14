#include <stddef.h>
#include <stdint.h>

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "hal_data.h"
#include "display_task.h"
#include "display_driver.h"
#include "video_source.h"
#include "fps_control.h"
#include "npu_worker.h"
#include "motion_task.h"
#include "motion_global.h"


#define DISPLAY_TASK_PRIORITY     (10)
#define DISPLAY_TASK_STACK_SIZE   (4096)


/*
 * RAW is always the base image.
 *
 * BBOX / VECTOR / ACTIVITY can be independently enabled
 * and combined by bit mask.
 */
volatile uint32_t g_display_overlay_mask =
    DISPLAY_OVERLAY_ALL;


LOCAL ID s_display_task_id = 0;

/*
 * Snapshot of the newest completed YOLOX result.
 *
 * This is independent of the current 30-FPS raw frame.  The display
 * never waits for YOLOX; it simply reuses the last completed result
 * until a newer one becomes available.
 */
static Detection_t s_display_latest_detections[MAX_DETECTIONS];

/*
 * Exact-frame Motion snapshot for debug drawing.
 */
/*
 * RAW exact-frame vectors from Motion task.
 */
static motion_vector_t
    s_display_motion_vectors_raw[MOTION_VECTOR_COUNT];

/*
 * Vectors actually used by VECTOR / ACTIVITY overlays.
 *
 * Compensation ON:
 *   RAW - estimated global translation
 *
 * Compensation OFF:
 *   RAW
 */
static motion_vector_t
    s_display_motion_vectors[MOTION_VECTOR_COUNT];


static motion_global_result_t
    s_display_global_result;

volatile uint32_t g_display_latest_yolo_frame_index = UINT32_MAX;
volatile int32_t  g_display_latest_yolo_detection_count = -1;
volatile int32_t  g_display_latest_yolo_age_frames = -1;

LOCAL void display_task_entry(INT stacd, void *exinf);


LOCAL T_CTSK s_display_task_ctsk =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = display_task_entry,
    .itskpri = DISPLAY_TASK_PRIORITY,
    .stksz   = DISPLAY_TASK_STACK_SIZE,
};


ER display_task_create(void)
{
    ID task_id;

    task_id =
        tk_cre_tsk(
            &s_display_task_ctsk
        );

    if (task_id < E_OK)
    {
        return (ER) task_id;
    }

    s_display_task_id =
        task_id;

    return E_OK;
}


ER display_task_start(void)
{
    if (s_display_task_id <= 0)
    {
        return E_ID;
    }

    return
        tk_sta_tsk(
            s_display_task_id,
            0
        );
}


void display_task_set_overlay_mask(
    uint32_t overlay_mask)
{
    g_display_overlay_mask =
        overlay_mask &
        DISPLAY_OVERLAY_ALL;
}


/*
 * Motion activity:
 *
 * A = |dx| + |dy|
 *
 * dx,dy are each in [-4,+4], therefore:
 *
 * A = 0 ... 8
 *
 * Activity is visualized only as a blue block outline.
 * It is NOT a risk score.
 */
LOCAL uint32_t display_task_motion_activity(
    motion_vector_t const * vector)
{
    int32_t const dx =
        vector->dx;

    int32_t const dy =
        vector->dy;

    uint32_t const abs_dx =
        (uint32_t)
        (
            (dx < 0) ?
            -dx :
            dx
        );

    uint32_t const abs_dy =
        (uint32_t)
        (
            (dy < 0) ?
            -dy :
            dy
        );

    return
        abs_dx + abs_dy;
}


/*
 * Encode activity 1...8 as blue intensity in RGB565.
 *
 * 0 is handled separately and is not drawn.
 */
LOCAL uint16_t display_task_activity_color(
    uint32_t activity)
{
    if (activity > 8U)
    {
        activity =
            8U;
    }

    uint32_t blue =
        3U +
        ((activity * 28U) / 8U);

    if (blue > 31U)
    {
        blue =
            31U;
    }

    return
        (uint16_t) blue;
}


LOCAL void display_task_entry(INT stacd, void *exinf)
{
    display_driver_status_t status;

    (void) stacd;
    (void) exinf;


    tm_putstring(
        (UB *)"[Display] task started.\n"
    );


    status =
        display_driver_init();

    if (DISPLAY_DRIVER_OK != status)
    {
        tm_printf(
            (UB *)"[Display] init failed: %d\n",
            status
        );

        tk_ext_tsk();
        return;
    }


    tm_putstring(
        (UB *)"[Display] GLCDC started.\n"
    );


    status =
        display_driver_fill(
            0x0000U
        );

    if (DISPLAY_DRIVER_OK != status)
    {
        tm_printf(
            (UB *)"[Display] fill failed: %d\n",
            status
        );

        tk_ext_tsk();
        return;
    }


    status =
        display_driver_backlight_on();

    if (DISPLAY_DRIVER_OK != status)
    {
        tm_printf(
            (UB *)"[Display] backlight failed: %d\n",
            status
        );

        tk_ext_tsk();
        return;
    }


    tm_putstring(
        (UB *)"[Display] backlight enabled.\n"
    );


    fps_control_init(
        FPS_CONTROL_UNLIMITED
    );


    video_source_display_consumer_enable();


    while (1)
    {
        fps_control_wait_before_frame();


        uint32_t current_slot =
            VIDEO_SOURCE_INVALID_SLOT;

        uint32_t frame_index =
            0U;


        const uint8_t * current_frame =
            video_source_acquire_read_buffer(
                &current_slot,
                &frame_index
            );


        if (NULL == current_frame)
        {
            tk_dly_tsk(1);
            continue;
        }


    
        uint32_t const overlay_mask =
            g_display_overlay_mask;


        /*
         * Only RAW_YOLO uses NPU results.
         * The result is the newest completed YOLOX result; Display never
         * waits for an exact-frame semantic result.
         */
        const Detection_t * detections =
            NULL;

        int32_t detection_count =
            0;


        if (0U !=
            (overlay_mask &
            DISPLAY_OVERLAY_BBOX))
        {
            /*
             * Non-blocking latest-result overlay.
             *
             * The displayed RAW frame remains 30-FPS.  YOLOX is slower,
             * so the newest completed detection result is copied here
             * and reused until the next YOLOX result is published.
             *
             * This keeps the semantic path asynchronous and prevents
             * Display from holding a raw Frame Cache slot for ~150 ms.
             */
            uint32_t yolo_frame_index =
                UINT32_MAX;

            int32_t const latest_count =
                npu_worker_copy_latest_detections(
                    s_display_latest_detections,
                    MAX_DETECTIONS,
                    &yolo_frame_index
                );

            if (latest_count >= 0)
            {
                detections =
                    s_display_latest_detections;

                detection_count =
                    latest_count;

                g_display_latest_yolo_frame_index =
                    yolo_frame_index;

                g_display_latest_yolo_detection_count =
                    latest_count;

                if (frame_index >=
                    yolo_frame_index)
                {
                    g_display_latest_yolo_age_frames =
                        (int32_t)
                        (
                            frame_index -
                            yolo_frame_index
                        );
                }
                else
                {
                    g_display_latest_yolo_age_frames =
                        -1;
                }
            }
        }


        fps_control_display_begin(
            frame_index
        );


        /*
         * Current checkpoint:
         * all views use the RAW 224x168 source.
         *
         * The driver enlarges it exactly 2x in X and Y:
         * 224x168 -> 448x336.
         */
        status =
            display_driver_compose_debug_view_rgb565(
                current_frame,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT,
                VIDEO_SOURCE_STRIDE_BYTES
            );


        /*
         * RAW + latest completed YOLOX result.
         *
         * This remains intentionally asynchronous:
         * Display never waits for YOLOX.
         */
        if ((DISPLAY_DRIVER_OK == status) &&
            (0U !=
            (overlay_mask &
            DISPLAY_OVERLAY_BBOX)) &&
            (NULL != detections))
        {
            for (int32_t i = 0;
                 i < detection_count;
                 i++)
            {
                (void)
                display_driver_overlay_rect_rgb565(
                    VIDEO_SOURCE_WIDTH,
                    VIDEO_SOURCE_HEIGHT,
                    (int32_t) detections[i].x1,
                    (int32_t) detections[i].y1,
                    (int32_t) detections[i].x2,
                    (int32_t) detections[i].y2,
                    0x07E0U,
                    4U
                );
            }
        }


        /*
         * Obtain the motion-vector field belonging to exactly the
         * RAW frame that Display is currently composing.
         */
        uint8_t motion_vectors_available =
            0U;


        if ((DISPLAY_DRIVER_OK == status) &&
            (0U !=
            (overlay_mask &
            (DISPLAY_OVERLAY_VECTOR |
            DISPLAY_OVERLAY_ACTIVITY))))
        {
            motion_vectors_available =
                motion_task_copy_vectors_for_frame(
                    frame_index,
                    s_display_motion_vectors_raw
                );


            if (0U != motion_vectors_available)
            {
                motion_global_compensate(
                    s_display_motion_vectors_raw,
                    s_display_motion_vectors,
                    &s_display_global_result
                );


                if ((1U == frame_index) ||
                    (100U == frame_index) ||
                    (200U == frame_index) ||
                    (299U == frame_index))
                {
                    tm_printf(
                        (UB *)"[Motion][Global] "
                            "frame=%u enable=%u "
                            "global=(%d,%d) "
                            "valid=%u applied=%u "
                            "consensus=%u/12 "
                            "cols=%u/4 rows=%u/3\n",
                        frame_index,
                        g_motion_global_compensation_enabled,
                        s_display_global_result.dx,
                        s_display_global_result.dy,
                        s_display_global_result.valid,
                        s_display_global_result.applied,
                        s_display_global_result.consensus_sectors,
                        s_display_global_result.agreeing_cols,
                        s_display_global_result.agreeing_rows
                    );
                }
            }
        }


        /*
        * Motion activity overlay.
        *
        * Blue block outline intensity represents:
        *
        *     A = |dx| + |dy|
        *
        * The top and bottom block rows are hidden from the debug
        * visualization, consistently with the vector overlay.
        */
        if ((DISPLAY_DRIVER_OK == status) &&
            (0U !=
            (overlay_mask &
            DISPLAY_OVERLAY_ACTIVITY)) &&
            (0U != motion_vectors_available))
        {
            for (uint32_t block_y = 1U;
                block_y < (MOTION_GRID_ROWS - 1U);
                block_y++)
            {
                for (uint32_t block_x = 0U;
                    block_x < MOTION_GRID_COLS;
                    block_x++)
                {
                    uint32_t const vector_index =
                        block_y *
                        MOTION_GRID_COLS +
                        block_x;

                    motion_vector_t const * const vector =
                        &s_display_motion_vectors[
                            vector_index
                        ];

                    uint32_t const activity =
                        display_task_motion_activity(
                            vector
                        );

                    if (0U == activity)
                    {
                        continue;
                    }


                    /*
                    * One grayscale block = 7x7.
                    * One RAW block       = 14x14.
                    */
                    int32_t const x1 =
                        (int32_t)
                        (
                            block_x *
                            MOTION_BLOCK_WIDTH *
                            2U
                        );

                    int32_t const y1 =
                        (int32_t)
                        (
                            block_y *
                            MOTION_BLOCK_HEIGHT *
                            2U
                        );

                    int32_t const x2 =
                        x1 +
                        (int32_t)
                        (
                            MOTION_BLOCK_WIDTH *
                            2U -
                            1U
                        );

                    int32_t const y2 =
                        y1 +
                        (int32_t)
                        (
                            MOTION_BLOCK_HEIGHT *
                            2U -
                            1U
                        );

                    uint16_t const activity_color =
                        display_task_activity_color(
                            activity
                        );


                    (void)
                    display_driver_overlay_rect_rgb565(
                        VIDEO_SOURCE_WIDTH,
                        VIDEO_SOURCE_HEIGHT,
                        x1,
                        y1,
                        x2,
                        y2,
                        activity_color,
                        2U
                    );
                }
            }
        }


        /*
        * CP2 exact-frame motion-vector visualization.
        */
        if ((DISPLAY_DRIVER_OK == status) &&
            (0U !=
            (overlay_mask &
            DISPLAY_OVERLAY_VECTOR)) &&
            (0U != motion_vectors_available))
        {
            for (uint32_t block_y = 1U;
                block_y < (MOTION_GRID_ROWS - 1U);
                block_y++)
            {
                for (uint32_t block_x = 0U;
                     block_x < MOTION_GRID_COLS;
                     block_x++)
                {
                    uint32_t const vector_index =
                        block_y *
                        MOTION_GRID_COLS +
                        block_x;

                    motion_vector_t const * const vector =
                        &s_display_motion_vectors[vector_index];


                    /*
                     * Center of corresponding 14x14 RAW region.
                     *
                     * grayscale 7x7 block
                     *       -> RAW 14x14 region.
                     */
                    int32_t const origin_x =
                        (int32_t)
                        (
                            block_x *
                            MOTION_BLOCK_WIDTH *
                            2U +
                            MOTION_BLOCK_WIDTH
                        );

                    int32_t const origin_y =
                        (int32_t)
                        (
                            block_y *
                            MOTION_BLOCK_HEIGHT *
                            2U +
                            MOTION_BLOCK_HEIGHT
                        );


                    /*
                     * Motion vector is measured in 112x84 coordinates.
                     * Convert it to 224x168 RAW coordinates.
                     */
                    int32_t const vector_dx =
                        (int32_t) vector->dx *
                        2;

                    int32_t const vector_dy =
                        (int32_t) vector->dy *
                        2;


                    (void)
                    display_driver_overlay_vector_rgb565(
                        VIDEO_SOURCE_WIDTH,
                        VIDEO_SOURCE_HEIGHT,
                        origin_x,
                        origin_y,
                        vector_dx,
                        vector_dy,
                        0xF800U,   /* red origin */
                        0xFFE0U    /* yellow vector */
                    );
                }
            }
        }


        if (DISPLAY_DRIVER_OK == status)
        {
            status =
                display_driver_present_composed();
        }


        fps_control_display_end();


        /*
         * GLCDC scans only its own 448x336 display framebuffer.
         * The raw Camera/USB slot is no longer needed after composition.
         */
        video_source_release_read_buffer(
            current_slot
        );


        if (DISPLAY_DRIVER_OK == status)
        {
            fps_control_frame_presented(
                frame_index
            );
        }
        else
        {
            tm_printf(
                (UB *)"[Display] present failed: %d\n",
                status
            );
        }
    }
}
