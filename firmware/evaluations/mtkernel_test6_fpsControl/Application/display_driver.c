#include <tm/tmonitor.h>

#include "display_driver.h"
#include "hal_data.h"
#include "video_source.h"

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


display_driver_status_t display_driver_present_rgb888(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height)
{
    fsp_err_t err;

    /*
     * GLCDC must already be initialized.
     */
    if (0U == s_initialized)
    {
        return DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }

    /*
     * Validate source image.
     */
    if ((NULL == source) ||
        (0U == source_width) ||
        (0U == source_height))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    /*
     * This function performs 1:1 display without scaling.
     * Therefore the source must fit inside the GLCDC framebuffer.
     */
    if ((source_width > DISPLAY_HSIZE_INPUT0) ||
        (source_height > DISPLAY_VSIZE_INPUT0))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }


    /*
     * Center the source image in the display framebuffer.
     *
     * Current configuration:
     *
     *   Display : 1024 x 600
     *   Source  :  640 x 480
     *
     * Therefore:
     *
     *   offset_x = 192
     *   offset_y = 60
     */
    uint32_t offset_x =
        (DISPLAY_HSIZE_INPUT0 -
         source_width) / 2U;

    uint32_t offset_y =
        (DISPLAY_VSIZE_INPUT0 -
         source_height) / 2U;


    /*
     * Select the GLCDC back buffer.
     */
    uint8_t * framebuffer =
        (uint8_t *)
        fb_background[s_video_back_buffer];


    /*
     * Copy packed 24-bit RGB input into the
     * 32-bit RGB888 GLCDC framebuffer.
     *
     * Source:
     *
     *   R G B | R G B | R G B ...
     *
     * Destination:
     *
     *   0x00RRGGBB
     *
     * No scaling is performed.
     */
    for (uint32_t sy = 0U;
         sy < source_height;
         sy++)
    {
        const uint8_t * source_row =
            source +
            (sy *
             source_width *
             VIDEO_SOURCE_BYTES_PER_PIXEL);

        uint32_t * destination_row =
            (uint32_t *)
            (framebuffer +
             ((offset_y + sy) *
              DISPLAY_BUFFER_STRIDE_BYTES_INPUT0));

        destination_row += offset_x;


        for (uint32_t sx = 0U;
             sx < source_width;
             sx++)
        {
            uint32_t source_index =
                sx *
                VIDEO_SOURCE_BYTES_PER_PIXEL;

            uint32_t red =
                source_row[source_index + 0U];

            uint32_t green =
                source_row[source_index + 1U];

            uint32_t blue =
                source_row[source_index + 2U];

            destination_row[sx] =
                (red << 16) |
                (green << 8) |
                blue;
        }


        /*
         * Cortex-M85 uses write-back D-cache.
         *
         * GLCDC reads SDRAM directly, so clean only
         * the part of this row that was modified.
         */
        SCB_CleanDCache_by_Addr(
            destination_row,
            (int32_t)
            (source_width *
             sizeof(uint32_t))
        );
    }


    /*
     * Ensure cache maintenance has completed before
     * transferring ownership to GLCDC.
     */
    __DSB();


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

        R_BSP_SoftwareDelay(
            1U,
            BSP_DELAY_UNITS_MILLISECONDS
        );
    }


    if (FSP_SUCCESS != err)
    {
        tm_printf(
            (UB *)"[Display] R_GLCDC_BufferChange failed: "
                   "fsp_err=%d\n",
            err
        );

        return DISPLAY_DRIVER_ERROR_BUFFER_CHANGE;
    }


    /*
     * The submitted buffer becomes the front buffer.
     * Use the other GLCDC buffer for the next frame.
     */
    s_video_back_buffer ^= 1U;

    return DISPLAY_DRIVER_OK;
}