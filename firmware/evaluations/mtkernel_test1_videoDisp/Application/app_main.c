#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "hal_data.h"

#include "display_task.h"
#include "dummy_tasks.h"


EXPORT INT usermain(void)
{
    ER err;

    tm_putstring((UB *)"Application start.\n");

    /*
     * Initialize FSP-generated common configuration.
     */
    g_hal_init();

    tm_putstring((UB *)"FSP HAL initialized.\n");


    err = display_task_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: display_task_create = %d\n",
            err
        );

        goto error;
    }


    err = dummy_tasks_create();

    if (err < E_OK)
    {
        tm_printf((UB *)"ERROR: dummy_tasks_create = %d\n", err);
        goto error;
    }


    /*
     * Start application tasks.
     */
    err = display_task_start();

    if (err < E_OK)
    {
        tm_printf((UB *)"ERROR: display_task_start = %d\n", err);
        goto error;
    }


    err = dummy_tasks_start();

    if (err < E_OK)
    {
        tm_printf((UB *)"ERROR: dummy_tasks_start = %d\n", err);
        goto error;
    }


    tm_putstring((UB *)"All application tasks started.\n");


    /*
     * usermain() must not return during normal operation.
     *
     * The initial task is no longer required after
     * application tasks have been started.
     */
    tk_slp_tsk(TMO_FEVR);

    return 0;


error:

    tm_putstring((UB *)"Application initialization failed.\n");

    tk_slp_tsk(TMO_FEVR);

    return 0;
}