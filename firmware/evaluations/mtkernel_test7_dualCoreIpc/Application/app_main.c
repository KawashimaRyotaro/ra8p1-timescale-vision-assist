#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "hal_data.h"

#include "display_task.h"
#include "video_source.h"
#include "usb_loader.h"
#include "ipc_test.h"


EXPORT INT usermain(void)
{
    ipc_test_start();

    ER err;

    tm_putstring((UB *)"Application start.\n");

    /*
     * Initialize FSP-generated common configuration first.
     */
    g_hal_init();

    tm_putstring((UB *)"FSP HAL initialized.\n");


    video_source_init();

    tm_putstring(
        (UB *)"[Video] SDRAM frame source initialized.\n"
    );

    /*
     * Create application tasks.
     */
    err = display_task_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: display_task_create = %d\n",
            err
        );

        goto error;
    }

    err = usb_loader_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: usb_loader_create = %d\n",
            err
        );

        goto error;
    }

    /*
     * Start application tasks.
     */
    err = display_task_start();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: display_task_start = %d\n",
            err
        );

        goto error;
    }

    err = usb_loader_start();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: usb_loader_start = %d\n",
            err
        );

        goto error;
    }


    tm_putstring((UB *)"All application tasks started.\n");

    tk_slp_tsk(TMO_FEVR);

    return 0;


error:

    tm_putstring(
        (UB *)"Application initialization failed.\n"
    );

    tk_slp_tsk(TMO_FEVR);

    return 0;
}