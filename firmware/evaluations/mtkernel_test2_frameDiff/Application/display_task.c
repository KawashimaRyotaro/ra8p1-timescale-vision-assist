#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "display_task.h"
#include "display_driver.h"
#include "video_source.h"
#include "frame_diff.h"

// Task configuration
#define DISPLAY_TASK_PRIORITY     (10)
#define DISPLAY_TASK_STACK_SIZE   (4096)
#define DISPLAY_COLOR_RED    (0x00FF0000UL)

static uint8_t s_diff_mask[VIDEO_SOURCE_WIDTH * VIDEO_SOURCE_HEIGHT];

// Module-private state
LOCAL ID s_display_task_id = 0;

// Module-private functions
LOCAL void display_task_entry(INT stacd, void *exinf);

// Task creation information.
LOCAL T_CTSK s_display_task_ctsk ={
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = display_task_entry,
    .itskpri = DISPLAY_TASK_PRIORITY,
    .stksz   = DISPLAY_TASK_STACK_SIZE,
};


// create task
ER display_task_create(void){
    ID task_id;

    task_id = tk_cre_tsk(&s_display_task_ctsk);

    if (task_id < E_OK){
        return (ER) task_id;
    }

    s_display_task_id = task_id;

    return E_OK;
}


// start task
ER display_task_start(void)
{
    if (s_display_task_id <= 0){
        return E_ID;
    }

    return tk_sta_tsk(s_display_task_id, 0);
}


// contents of the task
LOCAL void display_task_entry(INT stacd, void *exinf){
    // a statement of status variable
    display_driver_status_t status;
    
    // statements of image processing
    uint32_t frame_index = 0U;
    const uint8_t * previous_frame = NULL;

    (void) stacd;
    (void) exinf;

    tm_putstring((UB *)"[Display] task started.\n");

    // initialize the display driver
    status = display_driver_init();

    if (DISPLAY_DRIVER_OK != status){
        tm_printf((UB *)"[Display] init failed: %d\n", status);
        tk_ext_tsk();
        return;
    }

    tm_putstring((UB *)"[Display] GLCDC started.\n");

    // display backlight on
    status = display_driver_backlight_on();

    if (DISPLAY_DRIVER_OK != status){
        tm_printf((UB *)"[Display] backlight failed: %d\n", status);
        tk_ext_tsk();
        return;
    }

    tm_putstring((UB *)"[Display] backlight enabled.\n");
    
    // main loop
    while (1)
    {
        // get a frame
        const uint8_t * frame = video_source_get_frame(frame_index);

        // calculate frame difference
        if (NULL != previous_frame){
            // get frane difference metrics
            frame_diff_result_t diff =
                frame_diff_compute(
                    previous_frame,
                    frame,
                    VIDEO_SOURCE_WIDTH,
                    VIDEO_SOURCE_HEIGHT,
                    20U
                );

            // get frane difference image
            frame_diff_build_mask(
                previous_frame,
                frame,
                s_diff_mask,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT,
                20U
            );

            // show metrics
            tm_printf(
                (UB *) "frame=%u motion=%u.%u%% mean_diff=%u\n",
                frame_index,
                diff.changed_permille / 10U,
                diff.changed_permille % 10U,
                diff.mean_abs_diff
            );
        }
        else
        {
            // No previous frame:
            // clear the difference mask.
            for (uint32_t i = 0U;
                i < (VIDEO_SOURCE_WIDTH * VIDEO_SOURCE_HEIGHT);
                i++)
            {
                s_diff_mask[i] = 0U;
            }
        }

        // display
        status =
            display_driver_present_debug(
                frame,
                s_diff_mask,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT
            );

        if (DISPLAY_DRIVER_OK != status){
            tm_printf(
                (UB *) "ERROR: video display = %d\n",
                status
            );
        }

        previous_frame = frame;
        frame_index++;

        if (frame_index >= VIDEO_SOURCE_FRAME_COUNT){
            frame_index = 0U;
            /*
            * Do not compare the last frame with
            * the first frame of the next loop.
            */
            previous_frame = NULL;
        }
        /*
        * 10 fps
        */
        tk_dly_tsk(100);
    }
}
