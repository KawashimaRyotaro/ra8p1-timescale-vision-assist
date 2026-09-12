#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "hal_data.h"

#include "display_task.h"
#include "video_source.h"
#include "usb_loader.h"
#include "ipc_test.h"

#define APP_ENABLE_DEBUG_DISPLAY    (0U)


void npu_smoke_test(void);
void npu_inference_test(void);
void npu_worker_start(void);
void yolo_fastest_cpu_smoke_test(void);


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

    tm_putstring(
        (UB *)"[YOLO-Fastest] CPU smoke test start.\n"
    );

    yolo_fastest_cpu_smoke_test();

    tm_putstring(
        (UB *)"[YOLO-Fastest] CPU smoke test complete.\n"
    );

    /*
     * Create application tasks.
     */
    #if APP_ENABLE_DEBUG_DISPLAY

    err = display_task_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: display_task_create = %d\n",
            err
        );

        goto error;
    }

    #endif

    err = usb_loader_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: usb_loader_create = %d\n",
            err
        );

        goto error;
    }

    npu_worker_start();

    /*
     * Start application tasks.
     */
    #if APP_ENABLE_DEBUG_DISPLAY

    err = display_task_start();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: display_task_start = %d\n",
            err
        );

        goto error;
    }

    #endif

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