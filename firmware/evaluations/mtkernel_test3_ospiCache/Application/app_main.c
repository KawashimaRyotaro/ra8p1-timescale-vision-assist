#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "hal_data.h"

#include "display_task.h"
#include "dummy_tasks.h"
#include "ospi_cache.h"
#include "video_source.h"
#include "video_cache.h"
#include "assets/video_data.h"


EXPORT INT usermain(void)
{
    ER err;

    tm_putstring((UB *)"Application start.\n");

    /*
     * Initialize FSP-generated common configuration first.
     */
    g_hal_init();

    tm_putstring((UB *)"FSP HAL initialized.\n");


    /*
     * Initialize OSPI flash.
     */
    uint32_t ospi_device_id = 0U;

    ospi_cache_status_t ospi_status =
        ospi_cache_init(&ospi_device_id);

    tm_printf(
        (UB *)"[OSPI] init=%d device_id=0x%08X\n",
        ospi_status,
        ospi_device_id
    );

    if (OSPI_CACHE_OK != ospi_status)
    {
        tm_putstring((UB *)"[OSPI] ERROR: initialization failed\n");
        goto error;
    }

    // Please remove comentout when you run this project for the first time
    // const video_cache_info_t video_info =
    // {
    //     .width        = 128U,
    //     .height       = 72U,
    //     .frame_count  = 10U,
    //     .frame_size   = 128U * 72U * 2U,
    //     .pixel_format = 1U,
    //     .fps_milli    = 10000U
    // };

    // video_cache_status_t cache_status =
    //     video_cache_store(
    //         g_video_data,
    //         sizeof(g_video_data),
    //         &video_info
    //     );

    // tm_printf(
    //     (UB *)"[VideoCache] store=%d\n",
    //     cache_status
    // );

    // if (VIDEO_CACHE_OK != cache_status)
    // {
    //     tm_putstring(
    //         (UB *)"[VideoCache] ERROR: store failed\n"
    //     );

    //     goto error;
    // }

    /*
     * Prepare video data in OSPI flash.
     */
    tm_putstring((UB *)"[Video] binding OSPI source...\n");

    if (!video_source_bind_ospi())
    {
        tm_putstring((UB *)"[Video] ERROR: OSPI source bind failed\n");
        goto error;
    }

    tm_putstring((UB *)"[Video] OSPI source ready\n");

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

    err = dummy_tasks_create();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: dummy_tasks_create = %d\n",
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

    err = dummy_tasks_start();

    if (err < E_OK)
    {
        tm_printf(
            (UB *)"ERROR: dummy_tasks_start = %d\n",
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