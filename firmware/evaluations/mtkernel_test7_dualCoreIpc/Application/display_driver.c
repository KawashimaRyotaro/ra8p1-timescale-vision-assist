#include <tm/tmonitor.h>
#include <tk/tkernel.h>
#include <stdint.h>

#include "display_driver.h"
#include "hal_data.h"

#define DISPLAY_BACKLIGHT_PIN             BSP_IO_PORT_05_PIN_14
#define GLCDC_BUFFER_CHANGE_RETRY_MAX     (20U)
#define GLCDC_VSYNC_WAIT_TIMEOUT_MS       (100U)

/*
 * One debug video is shown at a time.
 *
 * Source:  224 x 168 RGB565
 * Display: 448 x 336 RGB565
 *
 * This is exactly 2x in X and 2x in Y, i.e. 4x pixel area.
 */
#define DEBUG_SOURCE_WIDTH                (224U)
#define DEBUG_SOURCE_HEIGHT               (168U)
#define DEBUG_DISPLAY_WIDTH               (448U)
#define DEBUG_DISPLAY_HEIGHT              (336U)
#define DEBUG_DISPLAY_STRIDE_BYTES        (896U)

static uint8_t s_initialized = 0U;
static uint8_t s_video_back_buffer = 1U;
static volatile uint32_t s_vsync_count = 0U;


void display_driver_callback(display_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    if (DISPLAY_EVENT_LINE_DETECTION == p_args->event)
    {
        s_vsync_count++;
        __DMB();
    }
}


static uint32_t display_driver_frame_bytes(void)
{
    return
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 *
        DISPLAY_VSIZE_INPUT0;
}


static uint8_t * display_driver_get_buffer(uint8_t index)
{
    uint8_t * const base =
        (uint8_t *) g_display0_cfg.input[0].p_base;

    return
        base +
        ((uint32_t) index * display_driver_frame_bytes());
}


static uint8_t * display_driver_get_back_buffer(void)
{
    return display_driver_get_buffer(s_video_back_buffer);
}


display_driver_status_t display_driver_init(void)
{
    fsp_err_t err;

    tm_printf(
        (UB *)"[DisplayCfg] hsize=%u vsize=%u stride=%u format=%u\n",
        DISPLAY_HSIZE_INPUT0,
        DISPLAY_VSIZE_INPUT0,
        DISPLAY_BUFFER_STRIDE_BYTES_INPUT0,
        (uint32_t) g_display0_cfg.input[0].format
    );

    tm_printf(
        (UB *)"[DisplayBuf] fb0=0x%08X fb1=0x%08X bytes=%u\n",
        (uint32_t) (uintptr_t) display_driver_get_buffer(0U),
        (uint32_t) (uintptr_t) display_driver_get_buffer(1U),
        display_driver_frame_bytes()
    );

    /*
     * The single-view debug layer must be exactly 448 x 336 RGB565.
     */
    if ((DEBUG_DISPLAY_WIDTH != DISPLAY_HSIZE_INPUT0) ||
        (DEBUG_DISPLAY_HEIGHT != DISPLAY_VSIZE_INPUT0) ||
        (DEBUG_DISPLAY_STRIDE_BYTES != DISPLAY_BUFFER_STRIDE_BYTES_INPUT0))
    {
        tm_putstring(
            (UB *)"[Display] invalid GLCDC input size for single-view mode.\n"
        );

        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

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
    if (0U == s_initialized)
    {
        return DISPLAY_DRIVER_ERROR_NOT_INITIALIZED;
    }

    uint16_t const pixel =
        (uint16_t) color;

    for (uint8_t buffer_index = 0U;
         buffer_index < 2U;
         buffer_index++)
    {
        uint8_t * const framebuffer =
            display_driver_get_buffer(buffer_index);

        for (uint32_t y = 0U;
             y < DISPLAY_VSIZE_INPUT0;
             y++)
        {
            uint16_t * const row =
                (uint16_t *)
                (
                    framebuffer +
                    y * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
                );

            for (uint32_t x = 0U;
                 x < DISPLAY_HSIZE_INPUT0;
                 x++)
            {
                row[x] = pixel;
            }
        }

#if BSP_CFG_DCACHE_ENABLED
        SCB_CleanDCache_by_Addr(
            (volatile void *) framebuffer,
            (int32_t) display_driver_frame_bytes()
        );
#endif
    }

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


display_driver_status_t display_driver_compose_debug_view_rgb565(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height,
    uint32_t source_stride_bytes)
{
    if ((NULL == source) ||
        (DEBUG_SOURCE_WIDTH != source_width) ||
        (DEBUG_SOURCE_HEIGHT != source_height) ||
        (source_stride_bytes < (source_width * 2U)))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    uint8_t * const framebuffer =
        display_driver_get_back_buffer();

    /*
     * Exact 2x nearest-neighbor enlargement.
     *
     * One source pixel becomes:
     *
     *   p p
     *   p p
     *
     * This avoids multiplication/division inside the inner loop.
     */
    for (uint32_t sy = 0U;
         sy < DEBUG_SOURCE_HEIGHT;
         sy++)
    {
        const uint16_t * const src_row =
            (const uint16_t *)
            (
                source +
                sy * source_stride_bytes
            );

        uint16_t * const dst_row0 =
            (uint16_t *)
            (
                framebuffer +
                (sy * 2U) *
                DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );

        uint16_t * const dst_row1 =
            (uint16_t *)
            (
                framebuffer +
                ((sy * 2U) + 1U) *
                DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );

        for (uint32_t sx = 0U;
             sx < DEBUG_SOURCE_WIDTH;
             sx++)
        {
            uint16_t const pixel =
                src_row[sx];

            uint32_t const dx =
                sx * 2U;

            dst_row0[dx]      = pixel;
            dst_row0[dx + 1U] = pixel;
            dst_row1[dx]      = pixel;
            dst_row1[dx + 1U] = pixel;
        }
    }

    return DISPLAY_DRIVER_OK;
}


display_driver_status_t display_driver_overlay_rect_rgb565(
    uint32_t source_width,
    uint32_t source_height,
    int32_t x1,
    int32_t y1,
    int32_t x2,
    int32_t y2,
    uint16_t color,
    uint32_t thickness)
{
    if ((DEBUG_SOURCE_WIDTH != source_width) ||
        (DEBUG_SOURCE_HEIGHT != source_height) ||
        (0U == thickness))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    if (x1 < 0)
    {
        x1 = 0;
    }

    if (y1 < 0)
    {
        y1 = 0;
    }

    if (x2 >= (int32_t) source_width)
    {
        x2 = (int32_t) source_width - 1;
    }

    if (y2 >= (int32_t) source_height)
    {
        y2 = (int32_t) source_height - 1;
    }

    if ((x2 <= x1) || (y2 <= y1))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }

    /*
     * Source coordinates -> 2x display coordinates.
     */
    int32_t const left   = x1 * 2;
    int32_t const right  = x2 * 2;
    int32_t const top    = y1 * 2;
    int32_t const bottom = y2 * 2;

    uint8_t * const framebuffer =
        display_driver_get_back_buffer();

    for (uint32_t t = 0U;
         t < thickness;
         t++)
    {
        int32_t const l = left   + (int32_t) t;
        int32_t const r = right  - (int32_t) t;
        int32_t const u = top    + (int32_t) t;
        int32_t const d = bottom - (int32_t) t;

        if ((r <= l) || (d <= u))
        {
            break;
        }

        uint16_t * const top_row =
            (uint16_t *)
            (
                framebuffer +
                (uint32_t) u * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );

        uint16_t * const bottom_row =
            (uint16_t *)
            (
                framebuffer +
                (uint32_t) d * DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
            );

        for (int32_t x = l;
             x <= r;
             x++)
        {
            top_row[x]    = color;
            bottom_row[x] = color;
        }

        for (int32_t y = u;
             y <= d;
             y++)
        {
            uint16_t * const row =
                (uint16_t *)
                (
                    framebuffer +
                    (uint32_t) y *
                    DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
                );

            row[l] = color;
            row[r] = color;
        }
    }

    return DISPLAY_DRIVER_OK;
}


static void display_driver_put_pixel_rgb565(
    uint8_t * framebuffer,
    int32_t x,
    int32_t y,
    uint16_t color)
{
    if ((x < 0) ||
        (y < 0) ||
        (x >= (int32_t) DEBUG_DISPLAY_WIDTH) ||
        (y >= (int32_t) DEBUG_DISPLAY_HEIGHT))
    {
        return;
    }

    uint16_t * const row =
        (uint16_t *)
        (
            framebuffer +
            (uint32_t) y *
            DISPLAY_BUFFER_STRIDE_BYTES_INPUT0
        );

    row[x] =
        color;
}


display_driver_status_t display_driver_overlay_vector_rgb565(
    uint32_t source_width,
    uint32_t source_height,
    int32_t source_x,
    int32_t source_y,
    int32_t source_dx,
    int32_t source_dy,
    uint16_t origin_color,
    uint16_t line_color)
{
    if ((DEBUG_SOURCE_WIDTH != source_width) ||
        (DEBUG_SOURCE_HEIGHT != source_height))
    {
        return DISPLAY_DRIVER_ERROR_ARGUMENT;
    }


    int32_t source_x1 =
        source_x + source_dx;

    int32_t source_y1 =
        source_y + source_dy;


    if (source_x1 < 0)
    {
        source_x1 = 0;
    }

    if (source_y1 < 0)
    {
        source_y1 = 0;
    }

    if (source_x1 >= (int32_t) source_width)
    {
        source_x1 =
            (int32_t) source_width - 1;
    }

    if (source_y1 >= (int32_t) source_height)
    {
        source_y1 =
            (int32_t) source_height - 1;
    }


    /*
     * 224x168 source coordinate
     *      -> 448x336 display coordinate.
     */
    int32_t x0 =
        source_x * 2;

    int32_t y0 =
        source_y * 2;

    int32_t const x1 =
        source_x1 * 2;

    int32_t const y1 =
        source_y1 * 2;


    uint8_t * const framebuffer =
        display_driver_get_back_buffer();


    /*
     * Bresenham line.
     */
    int32_t const dx =
        (x1 >= x0) ?
        (x1 - x0) :
        (x0 - x1);

    int32_t const sx =
        (x0 < x1) ? 1 : -1;

    int32_t const abs_dy =
        (y1 >= y0) ?
        (y1 - y0) :
        (y0 - y1);

    int32_t const dy =
        -abs_dy;

    int32_t const sy =
        (y0 < y1) ? 1 : -1;

    int32_t error =
        dx + dy;


    while (1)
    {
        display_driver_put_pixel_rgb565(
            framebuffer,
            x0,
            y0,
            line_color
        );

        if ((x0 == x1) &&
            (y0 == y1))
        {
            break;
        }

        int32_t const error2 =
            error * 2;

        if (error2 >= dy)
        {
            error +=
                dy;

            x0 +=
                sx;
        }

        if (error2 <= dx)
        {
            error +=
                dx;

            y0 +=
                sy;
        }
    }


    /*
     * Draw a 3x3 origin marker after the line,
     * so the starting point remains clearly visible.
     */
    int32_t const origin_x =
        source_x * 2;

    int32_t const origin_y =
        source_y * 2;


    for (int32_t py = -1;
         py <= 1;
         py++)
    {
        for (int32_t px = -1;
             px <= 1;
             px++)
        {
            display_driver_put_pixel_rgb565(
                framebuffer,
                origin_x + px,
                origin_y + py,
                origin_color
            );
        }
    }


    return DISPLAY_DRIVER_OK;
}


display_driver_status_t display_driver_present_composed(void)
{
    fsp_err_t err =
        FSP_ERR_INVALID_ARGUMENT;

    uint8_t * const framebuffer =
        display_driver_get_back_buffer();

    uint32_t const frame_bytes =
        display_driver_frame_bytes();

#if BSP_CFG_DCACHE_ENABLED
    SCB_CleanDCache_by_Addr(
        (volatile void *) framebuffer,
        (int32_t) frame_bytes
    );
#endif

    /*
     * Capture the current Vsync count BEFORE the buffer-change request.
     */
    __DMB();

    uint32_t const vsync_before =
        s_vsync_count;

    for (uint32_t retry = 0U;
         retry < GLCDC_BUFFER_CHANGE_RETRY_MAX;
         retry++)
    {
        err =
            R_GLCDC_BufferChange(
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

        tk_dly_tsk(1);
    }

    if (FSP_SUCCESS != err)
    {
        tm_printf(
            (UB *)"[Display] R_GLCDC_BufferChange failed: fsp_err=%d\n",
            err
        );

        return DISPLAY_DRIVER_ERROR_BUFFER_CHANGE;
    }

    /*
     * Wait until the requested frame has crossed a Vsync boundary before
     * reusing the other display buffer.
     */
    uint32_t wait_ms =
        GLCDC_VSYNC_WAIT_TIMEOUT_MS;

    while ((s_vsync_count == vsync_before) &&
           (wait_ms > 0U))
    {
        tk_dly_tsk(1);
        wait_ms--;
    }

    __DMB();

    if (s_vsync_count == vsync_before)
    {
        tm_putstring(
            (UB *)"[Display] Vsync wait timeout.\n"
        );

        return DISPLAY_DRIVER_ERROR_BUFFER_CHANGE;
    }

    s_video_back_buffer ^= 1U;

    return DISPLAY_DRIVER_OK;
}


/*
 * Legacy compatibility helpers.
 *
 * The GLCDC no longer reads Camera/USB raw-frame slots directly.
 */
uint8_t display_driver_release_pending(void)
{
    return 0U;
}


uint8_t display_driver_arm_release(uint32_t slot)
{
    (void) slot;
    return 0U;
}
