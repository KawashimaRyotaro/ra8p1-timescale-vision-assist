#include "display_driver.h"
#include "hal_data.h"

#define DISPLAY_BACKLIGHT_PIN    BSP_IO_PORT_05_PIN_14


static uint8_t s_initialized = 0U;


display_driver_status_t display_driver_init(void)
{
    fsp_err_t err;

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


display_driver_status_t display_driver_fill(uint32_t color)
{
    uint8_t * buffer0;
    uint8_t * buffer1;
    uint32_t  frame_bytes;

    if (0U == s_initialized)
    {
        return DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }

    /*
     * Use the actual framebuffer base address
     * generated in the GLCDC configuration.
     */
    buffer0 =
        (uint8_t *) g_display0_cfg.input[0].p_base;

    buffer1 =
        buffer0 +
        (DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
         DISPLAY_VSIZE_INPUT0);

    frame_bytes =
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
        DISPLAY_VSIZE_INPUT0;

    /*
     * Fill BOTH framebuffers.
     */
    for (uint32_t y = 0U;
         y < DISPLAY_VSIZE_INPUT0;
         y++)
    {
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

    /*
     * Cortex-M85 uses a write-back data cache.  The GLCDC reads SDRAM
     * directly, so make the CPU-written pixels visible to the GLCDC before
     * returning ownership of the framebuffers to the display hardware.
     */
    SCB_CleanDCache_by_Addr(
        (uint32_t *) buffer0,
        (int32_t) (frame_bytes * 2U)
    );

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
