#include <stddef.h>
#include <stdint.h>

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "display_task.h"
#include "display_driver.h"
#include "video_source.h"
#include "frame_difference.h"
#include "fps_control.h"
#include "npu_worker.h"


/*
 * Frame Difference switch.
 *
 * Uncomment:
 *     Difference ON
 *
 * Comment out:
 *     Difference OFF
 */
// #define DISPLAY_TASK_ENABLE_FRAME_DIFFERENCE


/*
 * Task configuration
 */
#define DISPLAY_TASK_PRIORITY     (10)
#define DISPLAY_TASK_STACK_SIZE   (4096)


/*
 * Module-private state
 */
LOCAL ID s_display_task_id = 0;


/*
 * Module-private functions
 */
LOCAL void display_task_entry(INT stacd, void *exinf);


/*
 * Task creation information.
 */
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

    task_id = tk_cre_tsk(&s_display_task_ctsk);

    if (task_id < E_OK)
    {
        return (ER) task_id;
    }

    s_display_task_id = task_id;

    return E_OK;
}


ER display_task_start(void)
{
    if (s_display_task_id <= 0)
    {
        return E_ID;
    }

    return tk_sta_tsk(s_display_task_id, 0);
}


LOCAL void display_task_entry(INT stacd, void *exinf)
{
    display_driver_status_t status;

    (void) stacd;
    (void) exinf;

    tm_putstring(
        (UB *)"[Display] task started.\n"
    );


    /*
     * Initialize and start GLCDC.
     */
    status = display_driver_init();

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


    /*
     * Clear both GLCDC framebuffers once.
     */
    status =
        display_driver_fill(
            0x00000000UL
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


    /*
     * Enable LCD backlight.
     */
    status = display_driver_backlight_on();

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


    /*
    * First measurement:
    *
    * 0 = unlimited.
    *
    * Do not limit the current USB -> SDRAM -> Display path.
    */
    fps_control_init(
        FPS_CONTROL_UNLIMITED
    );

    /*
     * Keep the previous frame until the next frame arrives.
     *
     * These variables must be outside the while loop because
     * Difference compares frame[n - 1] with frame[n].
     */
    uint32_t previous_slot =
        VIDEO_SOURCE_INVALID_SLOT;


#ifdef DISPLAY_TASK_ENABLE_FRAME_DIFFERENCE

    uint32_t previous_frame_index =
        UINT32_MAX;

    const uint8_t * previous_frame =
    NULL;

#endif

    video_source_display_consumer_enable();

    while (1)
    {
        fps_control_wait_before_frame();


        uint32_t current_slot =
            VIDEO_SOURCE_INVALID_SLOT;

        uint32_t frame_index = 0U;


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


        /*
        * Difference is valid only for consecutive frames
        * belonging to the same video stream.
        *
        * frame_index == 0 indicates a new stream.
        */
        #ifdef DISPLAY_TASK_ENABLE_FRAME_DIFFERENCE

        uint8_t difference_valid = 0U;

        frame_difference_result_t
            difference_result =
            {
                .sample_count = 0U,
                .mean_abs_luma_x1000 = 0U
            };


        if ((NULL != previous_frame) &&
            (0U != frame_index) &&
            ((previous_frame_index + 1U) ==
            frame_index))
        {
            difference_valid =
                frame_difference_compute_rgb565(
                    previous_frame,
                    current_frame,
                    VIDEO_SOURCE_WIDTH,
                    VIDEO_SOURCE_HEIGHT,
                    &difference_result
                );
        }


        /*
        * Minimal debug output.
        */
        if ((0U != difference_valid) &&
            ((1U == frame_index) ||
            (0U == (frame_index % 100U)) ||
            (299U == frame_index)))
        {
            tm_printf(
                (UB *)"[Diff] "
                    "frame=%u "
                    "samples=%u "
                    "mad=%u.%03u\n",
                frame_index,
                difference_result.sample_count,
                difference_result.mean_abs_luma_x1000 /
                    1000U,
                difference_result.mean_abs_luma_x1000 %
                    1000U
            );
        }

        /*
        * Minimal debug output.
        *
        * Do not print every frame because serial logging
        * itself would disturb FPS measurement.
        */
        if ((0U != difference_valid) &&
            ((1U == frame_index) ||
            (0U == (frame_index % 100U)) ||
            (299U == frame_index)))
        {
            tm_printf(
                (UB *)"[Diff] "
                    "frame=%u "
                    "samples=%u "
                    "mad=%u.%03u\n",
                frame_index,
                difference_result.sample_count,
                difference_result.mean_abs_luma_x1000 /
                    1000U,
                difference_result.mean_abs_luma_x1000 %
                    1000U
            );
        }

        #endif


        while (0U !=
            display_driver_release_pending())
        {
            tk_dly_tsk(1);
        }


        fps_control_display_begin(
            frame_index
        );


        /*
        * Wait for the NPU result corresponding to
        * exactly this framebuffer.
        *
        * YOLOX currently takes about 320 ms,
        * so allow up to 1000 ms.
        */
        const Detection_t * detections = NULL;
        int32_t detection_count = 0;

        for (uint32_t wait_ms = 0U;
            wait_ms < 1000U;
            wait_ms++)
        {
            detections =
                npu_worker_get_detections(
                    current_slot,
                    frame_index,
                    &detection_count
                );

            if (NULL != detections)
            {
                break;
            }

            tk_dly_tsk(1);
        }


        /*
        * NPU has finished reading this frame before
        * publishing its result, so it is now safe for
        * the display consumer to annotate the framebuffer.
        */
        if (NULL != detections)
        {
            uint8_t * annotated_frame =
                (uint8_t *) (uintptr_t) current_frame;


            for (int32_t i = 0;
                i < detection_count;
                i++)
            {
                /*
                * Green RGB565 bounding box.
                */
                (void)
                display_driver_draw_rect_rgb565(
                    annotated_frame,
                    VIDEO_SOURCE_WIDTH,
                    VIDEO_SOURCE_HEIGHT,
                    (int32_t) detections[i].x1,
                    (int32_t) detections[i].y1,
                    (int32_t) detections[i].x2,
                    (int32_t) detections[i].y2,
                    0x07E0U,
                    2U
                );
            }
        }


        status =
            display_driver_present_rgb565(
                current_frame,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT
            );


        fps_control_display_end();


        if (DISPLAY_DRIVER_OK ==
            status)
        {
            /*
            * Do NOT release previous_slot here.
            *
            * GLCDC may still be scanning it.
            *
            * Arm it for release by the next
            * LINE_DETECTION interrupt instead.
            */
            if (VIDEO_SOURCE_INVALID_SLOT !=
                previous_slot)
            {
                if (0U ==
                    display_driver_arm_release(
                        previous_slot
                    ))
                {
                    tm_putstring(
                        (UB *)"[Display] "
                            "failed to arm buffer release.\n"
                    );

                    video_source_release_read_buffer(
                        current_slot
                    );

                    tk_ext_tsk();
                    return;
                }
            }


            /*
            * Current frame becomes:
            *
            * 1. GLCDC framebuffer
            * 2. previous frame for next Difference
            */
            previous_slot =
                current_slot;


            #ifdef DISPLAY_TASK_ENABLE_FRAME_DIFFERENCE

            previous_frame =
                current_frame;

            previous_frame_index =
                frame_index;

            #endif


            fps_control_frame_presented(
                frame_index
            );
        }
        else
        {
            video_source_release_read_buffer(
                current_slot
            );


            tm_printf(
                (UB *)"[Display] present failed: %d\n",
                status
            );
        }
    }
}