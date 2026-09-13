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


#define DISPLAY_TASK_PRIORITY     (10)
#define DISPLAY_TASK_STACK_SIZE   (4096)


/*
 * One debug video is shown at a time.
 *
 * Current implemented sources:
 *   RAW_YOLO : raw video + YOLO bbox
 *   RAW      : raw video only
 *
 * DIFF / MOTION / RISK are reserved and currently fall back to RAW.
 */
volatile uint32_t g_display_debug_view =
    (uint32_t) DISPLAY_DEBUG_VIEW_RAW_YOLO;


LOCAL ID s_display_task_id = 0;

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


void display_task_set_debug_view(
    display_debug_view_t view)
{
    if ((uint32_t) view >=
        (uint32_t) DISPLAY_DEBUG_VIEW_COUNT)
    {
        return;
    }

    g_display_debug_view =
        (uint32_t) view;

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


    
        display_debug_view_t const selected_view =
            (display_debug_view_t)
            g_display_debug_view;


        /*
         * Only RAW_YOLO requires the exact-frame NPU result.
         * RAW / DIFF / MOTION / RISK do not wait for YOLO.
         */
        const Detection_t * detections =
            NULL;

        int32_t detection_count =
            0;


        if (DISPLAY_DEBUG_VIEW_RAW_YOLO ==
            selected_view)
        {
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


        if ((DISPLAY_DRIVER_OK == status) &&
            (DISPLAY_DEBUG_VIEW_RAW_YOLO == selected_view) &&
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
