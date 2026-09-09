#include <tm/tmonitor.h>
#include <tk/tkernel.h>

#include "display_driver.h"
#include "hal_data.h"
#include "video_source.h"

#define DISPLAY_BACKLIGHT_PIN    BSP_IO_PORT_05_PIN_14
#define GLCDC_BUFFER_CHANGE_RETRY_MAX    (20U)


static uint8_t s_initialized = 0U;


/*
 * Buffer which may be returned to the USB producer
 * after GLCDC has completed the active scan.
 */
static volatile uint32_t
    s_pending_release_slot =
        VIDEO_SOURCE_INVALID_SLOT;

static volatile uint8_t
    s_pending_release_armed = 0U;


void display_driver_callback(
    display_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }


    if (DISPLAY_EVENT_LINE_DETECTION !=
        p_args->event)
    {
        return;
    }


    /*
     * GLCDC has completed the active image scan.
     *
     * The framebuffer used by the previous scan
     * can now safely be returned to the USB producer.
     *
     * video_source_release_read_buffer() does not call
     * any μT-Kernel service; it only changes the
     * buffer ownership state, so it can be used here.
     */
    if (0U != s_pending_release_armed)
    {
        uint32_t slot =
            s_pending_release_slot;


        video_source_release_read_buffer(
            slot
        );


        __DMB();

        s_pending_release_slot =
            VIDEO_SOURCE_INVALID_SLOT;

        s_pending_release_armed = 0U;

        __DMB();
    }
}


display_driver_status_t display_driver_init(void)
{
    fsp_err_t err;

    tm_printf(
        (UB *)"[DisplayCfg] "
               "hsize=%u vsize=%u stride=%u format=%u\n",
        DISPLAY_HSIZE_INPUT0,
        DISPLAY_VSIZE_INPUT0,
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0,
        (uint32_t) g_display0_cfg.input[0].format
    );

    err = R_GLCDC_Open(
        &g_display0_ctrl,
        &g_display0_cfg
    );

    if (FSP_SUCCESS != err)
    {
        return DISPLAY_DRIVER_ERROR_OPEN;
    }

    err = R_GLCDC_Start(
        &g_display0_ctrl
    );

    if (FSP_SUCCESS != err)
    {
        return DISPLAY_DRIVER_ERROR_START;
    }

    s_initialized = 1U;

    return DISPLAY_DRIVER_OK;
}


display_driver_status_t
display_driver_fill(uint32_t color)
{
    if (0U == s_initialized)
    {
        return
            DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }


    uint8_t * buffer0 =
        (uint8_t *)
        g_display0_cfg.input[0].p_base;


    uint32_t frame_bytes =
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
        DISPLAY_VSIZE_INPUT0;


    uint8_t * buffer1 =
        buffer0 + frame_bytes;


    uint16_t pixel =
        (uint16_t) color;


    for (uint32_t y = 0U;
         y < DISPLAY_VSIZE_INPUT0;
         y++)
    {
        uint16_t * row0 =
            (uint16_t *)
            (
                buffer0 +
                y *
                DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );


        uint16_t * row1 =
            (uint16_t *)
            (
                buffer1 +
                y *
                DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );


        for (uint32_t x = 0U;
             x < DISPLAY_HSIZE_INPUT0;
             x++)
        {
            row0[x] = pixel;
            row1[x] = pixel;
        }
    }


#if BSP_CFG_DCACHE_ENABLED

    SCB_CleanDCache_by_Addr(
        (volatile void *) buffer0,
        (int32_t) (frame_bytes * 2U)
    );

#endif


    return DISPLAY_DRIVER_OK;
}


display_driver_status_t display_driver_backlight_on(void)
{
    fsp_err_t err;

    if (0U == s_initialized)
    {
        return DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }

    R_BSP_PinAccessEnable();

    err = R_IOPORT_PinWrite(
        &g_ioport_ctrl,
        DISPLAY_BACKLIGHT_PIN,
        BSP_IO_LEVEL_HIGH
    );

    R_BSP_PinAccessDisable();

    if (FSP_SUCCESS != err)
    {
        return DISPLAY_DRIVER_ERROR_BACKLIGHT;
    }

    return DISPLAY_DRIVER_OK;
}


display_driver_status_t display_driver_test_pattern(void)
{
    uint8_t * buffer0;
    uint8_t * buffer1;

    if (0U == s_initialized)
    {
        return DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }

    buffer0 =
        (uint8_t *) g_display0_cfg.input[0].p_base;

    buffer1 =
        buffer0 +
        (DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
         DISPLAY_VSIZE_INPUT0);

    for (uint32_t y = 0U;
         y < DISPLAY_VSIZE_INPUT0;
         y++)
    {
        uint32_t color;

        if (y < (DISPLAY_VSIZE_INPUT0 / 2U))
        {
            color = 0x00FF0000UL;     /* RED */
        }
        else
        {
            color = 0x0000FF00UL;     /* GREEN */
        }

        uint32_t * row0 =
            (uint32_t *)
            (buffer0 +
             y * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0);

        uint32_t * row1 =
            (uint32_t *)
            (buffer1 +
             y * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0);

        for (uint32_t x = 0U;
             x < DISPLAY_HSIZE_INPUT0;
             x++)
        {
            row0[x] = color;
            row1[x] = color;
        }
    }

    return DISPLAY_DRIVER_OK;
}


display_driver_status_t display_driver_present_rgb565(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height)
{
    fsp_err_t err =
        FSP_ERR_INVALID_ARGUMENT;


    if ((NULL == source) ||
        (VIDEO_SOURCE_WIDTH != source_width) ||
        (VIDEO_SOURCE_HEIGHT != source_height))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }


    /*
     * R_GLCDC_BufferChange() requires 64-byte alignment.
     */
    if (0U !=
        ((uintptr_t) source & 0x3FU))
    {
        tm_printf(
            (UB *)"[Display] framebuffer not 64-byte aligned: 0x%08X\n",
            (uint32_t) (uintptr_t) source
        );

        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }


    /*
     * No pixel conversion.
     * No CPU framebuffer copy.
     *
     * The RGB565 SDRAM frame received from USB DMA
     * becomes the GLCDC input framebuffer itself.
     */
    for (uint32_t retry = 0U;
         retry < GLCDC_BUFFER_CHANGE_RETRY_MAX;
         retry++)
    {
        err =
            R_GLCDC_BufferChange(
                &g_display0_ctrl,
                (uint8_t *) source,
                DISPLAY_FRAME_LAYER_1
            );


        if (FSP_SUCCESS == err)
        {
            break;
        }


        if (FSP_ERR_INVALID_UPDATE_TIMING !=
            err)
        {
            break;
        }


        /*
        * Yield CPU while waiting for the next
        * GLCDC update window.
        *
        * This allows the lower-priority USB task
        * to continue filling the free SDRAM buffer.
        */
        tk_dly_tsk(1);
    }


    if (FSP_SUCCESS != err)
    {
        tm_printf(
            (UB *)"[Display] R_GLCDC_BufferChange failed: fsp_err=%d\n",
            err
        );

        return
            DISPLAY_DRIVER_ERROR_BUFFER_CHANGE;
    }


    return DISPLAY_DRIVER_OK;
}


uint8_t display_driver_release_pending(void)
{
    __DMB();

    return s_pending_release_armed;
}


uint8_t display_driver_arm_release(
    uint32_t slot)
{
    if (slot >= VIDEO_SOURCE_BUFFER_COUNT)
    {
        return 0U;
    }


    /*
     * There must never be two unreleased GLCDC
     * framebuffers at the same time.
     */
    if (0U != s_pending_release_armed)
    {
        return 0U;
    }


    s_pending_release_slot =
        slot;

    __DMB();

    s_pending_release_armed = 1U;

    __DMB();


    return 1U;
}