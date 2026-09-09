#include <tm/tmonitor.h>
#include "display_driver.h"
#include "hal_data.h"

#define DISPLAY_BACKLIGHT_PIN    BSP_IO_PORT_05_PIN_14
#define GLCDC_BUFFER_CHANGE_RETRY_MAX    (20U)


static uint8_t s_initialized = 0U;
static uint32_t s_video_back_buffer = 1U;


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


static uint32_t rgb565_to_rgb888(uint16_t pixel)
{
    uint32_t r5 = (pixel >> 11) & 0x1FU;
    uint32_t g6 = (pixel >> 5)  & 0x3FU;
    uint32_t b5 = pixel & 0x1FU;

    uint32_t r8 = (r5 << 3) | (r5 >> 2);
    uint32_t g8 = (g6 << 2) | (g6 >> 4);
    uint32_t b8 = (b5 << 3) | (b5 >> 2);

    return (r8 << 16) |
           (g8 << 8)  |
           b8;
}

display_driver_status_t display_driver_present_rgb565(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height)
{
    fsp_err_t err;

    if ((NULL == source) ||
        (0U == source_width) ||
        (0U == source_height))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    uint32_t scale_x = DISPLAY_HSIZE_INPUT0 / source_width;
    uint32_t scale_y = DISPLAY_VSIZE_INPUT0 / source_height;

    uint32_t scale =
        (scale_x < scale_y) ? scale_x : scale_y;

    if (0U == scale)
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    uint32_t output_width  = source_width  * scale;
    uint32_t output_height = source_height * scale;

    uint32_t offset_x =
        (DISPLAY_HSIZE_INPUT0 - output_width) / 2U;

    uint32_t offset_y =
        (DISPLAY_VSIZE_INPUT0 - output_height) / 2U;

    uint8_t * framebuffer =
        (uint8_t *) fb_background[s_video_back_buffer];

    /*
     * Black background.
     */
    for (uint32_t y = 0U;
         y < DISPLAY_VSIZE_INPUT0;
         y++)
    {
        uint32_t * row =
            (uint32_t *)
            (framebuffer +
             y * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0);

        for (uint32_t x = 0U;
             x < DISPLAY_HSIZE_INPUT0;
             x++)
        {
            row[x] = 0x00000000U;
        }
    }

    /*
     * RGB565 source -> RGB888 framebuffer.
     * Nearest-neighbor integer scaling.
     */
    for (uint32_t sy = 0U;
         sy < source_height;
         sy++)
    {
        for (uint32_t sx = 0U;
             sx < source_width;
             sx++)
        {
            uint32_t source_index =
                (sy * source_width + sx) * 2U;

            uint16_t pixel565 =
                (uint16_t) source[source_index] |
                ((uint16_t) source[source_index + 1U] << 8);

            uint32_t pixel888 =
                rgb565_to_rgb888(pixel565);

            uint32_t destination_x =
                offset_x + sx * scale;

            uint32_t destination_y =
                offset_y + sy * scale;

            for (uint32_t dy = 0U;
                 dy < scale;
                 dy++)
            {
                uint32_t * row =
                    (uint32_t *)
                    (framebuffer +
                     (destination_y + dy) *
                     DISPLAY_BUFFER_STRIDE_BYTES_INPUT0);

                for (uint32_t dx = 0U;
                     dx < scale;
                     dx++)
                {
                    row[destination_x + dx] =
                        pixel888;
                }
            }
        }
    }

    /*
     * The framebuffer is written by the CPU through the write-back D-cache,
     * while GLCDC reads SDRAM directly.
     * Clean the back buffer before handing it to GLCDC.
     */
    uint32_t frame_bytes =
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
        DISPLAY_VSIZE_INPUT0;

    SCB_CleanDCache_by_Addr(
        (uint32_t *) framebuffer,
        (int32_t) frame_bytes
    );

    /*
     * Request GLCDC to display this framebuffer.
     */
    for (uint32_t retry = 0U;
        retry < GLCDC_BUFFER_CHANGE_RETRY_MAX;
        retry++)
    {
        err = R_GLCDC_BufferChange(
            &g_display0_ctrl,
            framebuffer,
            DISPLAY_FRAME_LAYER_1
        );

        if (FSP_SUCCESS == err)
        {
            break;
        }

        if (FSP_ERR_INVALID_UPDATE_TIMING != err)
        {
            break;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (FSP_SUCCESS != err)
    {
        tm_printf(
            (UB *) "[Display] R_GLCDC_BufferChange failed: fsp_err=%d\n",
            err
        );

        return DISPLAY_DRIVER_ERROR_BUFFER_CHANGE;
    }

    /*
     * The buffer just submitted becomes the front buffer.
     * Prepare the other one next time.
     */
    s_video_back_buffer ^= 1U;

    return DISPLAY_DRIVER_OK;
}