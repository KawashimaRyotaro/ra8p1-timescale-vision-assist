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
#define DISPLAY_COLOR_RED    (0x00FF0000UL)


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
    uint32_t frame_index = 0U;

    (void) stacd;
    (void) exinf;

    tm_putstring((UB *)"[Display] task started.\n");

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
        const uint8_t * frame =
            video_source_get_frame(frame_index);

        status =
            display_driver_present_rgb565(
                frame,
                VIDEO_SOURCE_WIDTH,
                VIDEO_SOURCE_HEIGHT
            );

        if (DISPLAY_DRIVER_OK != status)
        {
            tm_printf(
                (UB *) "ERROR: video display = %d\n",
                status
            );
        }

        frame_index++;

        if (frame_index >=
            VIDEO_SOURCE_FRAME_COUNT)
        {
            frame_index = 0U;
        }

        /*
         * 10 fps
         */
        tk_dly_tsk(100);
    }
}
