#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "display_task.h"
#include "display_driver.h"
#include "video_source.h"


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


    while (1)
    {
        uint32_t slot = 0U;
        uint32_t frame_index = 0U;

        const uint8_t * frame =
            video_source_acquire_read_buffer(
                &slot,
                &frame_index
            );

        if (NULL == frame)
        {
            tk_dly_tsk(1);
            continue;
        }


        status =
            display_driver_present_rgb888(
                frame,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT
            );


        video_source_release_read_buffer(
            slot
        );


        if (DISPLAY_DRIVER_OK != status)
        {
            tm_printf(
                (UB *)"ERROR: video display = %d frame=%u\n",
                status,
                frame_index
            );
        }
    }
}