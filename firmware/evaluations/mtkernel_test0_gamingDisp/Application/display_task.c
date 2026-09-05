#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "display_task.h"
#include "display_driver.h"


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

    (void) stacd;
    (void) exinf;

    ColorCycler my_cycler;
    color_cycler_init(&my_cycler, 0.1f);

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


    status =
        display_driver_fill(DISPLAY_COLOR_RED);

    if (DISPLAY_DRIVER_OK != status)
    {
        tm_printf(
            (UB *)"[Display] fill failed: %d\n",
            status
        );

        tk_ext_tsk();
        return;
    }

    tm_putstring(
        (UB *)"[Display] framebuffer filled.\n"
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
        unsigned long col = color_cycler_update(&my_cycler);

        status =  display_driver_fill(col);

        if (DISPLAY_DRIVER_OK != status)
        {
            tm_printf(
                (UB *)"[Display] fill failed: %d\n",
                status
            );

            tk_ext_tsk();
            return;
        }

        // tm_printf(
        //     (UB *)"[Display] framebuffer filled: 0x%x\n",
        //     col
        // );

        tk_dly_tsk(1);
    }
}
