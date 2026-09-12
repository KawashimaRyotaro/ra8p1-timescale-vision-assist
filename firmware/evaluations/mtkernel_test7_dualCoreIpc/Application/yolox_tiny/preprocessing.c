/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**
 ******************************************************************************
 * @file    preprocessing.c
 * @brief   YOLOX-Tiny INT8 letterbox resize + BGR->RGB + quantize to int8.
 *
 * Bilinear interpolation with half-pixel centre convention, matching the
 * Python cv2.resize() / PIL behaviour used during training.
 *
 * Quantization: int8_value = round(pixel_float / scale) + zero_point
 *   With scale=1.0, zero_point=-128:  int8_value = pixel_value - 128
 *
 * All buffers are caller-provided — no heap allocation.
 ******************************************************************************
 */

#include "preprocessing.h"
#include "model_metadata.h"
#include <stddef.h>
#include <string.h>

/* ── Bilinear sample helper ──────────────────────────────────────────── */

/**
 * @brief Sample one channel from a uint8 HWC image with bilinear interpolation.
 */
static float bilinear_sample_channel(const uint8_t *p_source_image,
                             int32_t image_width, int32_t image_height,
                             int32_t channels, int32_t channel_idx,
                             float float_x, float float_y)
{
    /* Integer neighbors around the floating-point sample location. */
    int32_t left_x  = (int32_t)float_x;
    int32_t top_y   = (int32_t)float_y;
    int32_t right_x = left_x + 1;
    int32_t bot_y   = top_y + 1;

    /* Clamp to image boundaries */
    if (left_x  < 0)           { left_x  = 0; }
    if (top_y   < 0)           { top_y   = 0; }
    if (right_x >= image_width)  { right_x = image_width  - 1; }
    if (bot_y   >= image_height) { bot_y   = image_height - 1; }

    /* Fractional offsets used as interpolation weights. */
    float weight_x = float_x - (float)left_x;
    float weight_y = float_y - (float)top_y;

    /* Read the four surrounding pixels for the requested channel. */
    float top_left_val     = (float)p_source_image[(top_y * image_width + left_x)  * channels + channel_idx];
    float top_right_val    = (float)p_source_image[(top_y * image_width + right_x) * channels + channel_idx];
    float bottom_left_val  = (float)p_source_image[(bot_y * image_width + left_x)  * channels + channel_idx];
    float bottom_right_val = (float)p_source_image[(bot_y * image_width + right_x) * channels + channel_idx];

    /* Interpolate horizontally at the top row and bottom row. */
    float interpolated_top    = top_left_val    * (1.0F - weight_x) + top_right_val    * weight_x;
    float interpolated_bottom = bottom_left_val * (1.0F - weight_x) + bottom_right_val * weight_x;

    /* Interpolate vertically between the top and bottom blended values. */
    return interpolated_top * (1.0F - weight_y) + interpolated_bottom * weight_y;
}

/* ── Quantize helper ─────────────────────────────────────────────────── */

/**
 * @brief Quantize a float pixel value [0,255] to int8 using input quant params.
 *        int8 = clamp(round(value / scale) + zero_point, -128, 127)
 *        With scale=1.0, zp=-128: int8 = round(value) - 128
 *
 * Optimized: since scale=1.0, skip the float divide entirely.
 * Input range [0,255] maps to [-128,127] which fits int8 exactly — no clamp needed.
 */
static inline int8_t quantize_to_int8(float pixel_value)
{
    /* Round to nearest integer in [0,255]-like range. */
    int32_t rounded_int_value = (int32_t)(pixel_value + 0.5F);
    /* Shift uint8-like range [0..255] to int8 range [-128..127]. */
    return (int8_t)(rounded_int_value - 128);
}

/* ── Main preprocess function ────────────────────────────────────────── */

/**
 * @brief Preprocess source image to model input using letterbox and quantization.
 * @param[in]  source_image         Source BGR image buffer.
 * @param[in]  source_width         Source image width in pixels.
 * @param[in]  source_height        Source image height in pixels.
 * @param[out] p_destination_image  Destination quantized RGB image buffer.
 * @param[in]  destination_width    Destination image width in pixels.
 * @param[in]  destination_height   Destination image height in pixels.
 * @param[out] p_params             Letterbox scale and padding parameters.
 * @return void
 */

void preprocess(const uint8_t   *source_image,
                uint16_t         source_width,
                uint16_t         source_height,
                int8_t          *p_destination_image,
                uint16_t         destination_width,
                uint16_t         destination_height,
                letterbox_params_t *p_params)
{
    /* Convert dimensions to signed integers for arithmetic operations. */
    int32_t source_width_px  = (int32_t)source_width;
    int32_t source_height_px = (int32_t)source_height;
    int32_t dest_width_px    = (int32_t)destination_width;
    int32_t dest_height_px   = (int32_t)destination_height;

    /* Step 1: Compute uniform scale (preserve aspect ratio) */
    /* Compute the resize scale along each axis. */
    float scale_factor_x = (float)dest_width_px  / (float)source_width_px;
    float scale_factor_y = (float)dest_height_px / (float)source_height_px;
    /* Use the smaller scale so aspect ratio is preserved and content fits. */
    float uniform_scale  = (scale_factor_x < scale_factor_y) ? scale_factor_x : scale_factor_y;

    /* Compute resized image size in model space before padding. */
    int32_t resized_width_px  = (int32_t)((float)source_width_px  * uniform_scale);
    int32_t resized_height_px = (int32_t)((float)source_height_px * uniform_scale);

    /* Compute symmetric letterbox padding around resized content. */
    int32_t padding_x = (dest_width_px  - resized_width_px)  / 2;
    int32_t padding_y = (dest_height_px - resized_height_px) / 2;

    /* Return letterbox parameters for coordinate unscaling */
    if (p_params != NULL)
    {
        p_params->scale = uniform_scale; /* Saved for inverse mapping in postprocess. */
        p_params->pad_x = padding_x;     /* Horizontal pad used for unletterbox. */
        p_params->pad_y = padding_y;     /* Vertical pad used for unletterbox. */
    }

    /* Step 2: Fill entire destination with quantized pad value (gray 114 → int8 -14) */
    {
        /* Quantize constant gray padding value once. */
        int8_t pad_quantized = quantize_to_int8((float)INPUT_LETTERBOX_PAD);
        int32_t total_dest_pixels = dest_height_px * dest_width_px * MODEL_INPUT_C;
        /* Fill full destination tensor with padding; resized content overwrites center. */
        (void)memset(p_destination_image, (int)pad_quantized, (size_t)total_dest_pixels);
    }

    /* Step 3: Bilinear resize into the padded region, with BGR → RGB swap,
     *         then quantize to int8 */
    {
        /* Inverse resize scales map destination sample positions back to source. */
        float inv_scale_x = (float)source_width_px  / (float)resized_width_px;
        float inv_scale_y = (float)source_height_px / (float)resized_height_px;
        int32_t dest_row_y;
        int32_t dest_col_x;

        for (dest_row_y = 0; dest_row_y < resized_height_px; dest_row_y++)
        {
            /* Half-pixel centre convention */
            float source_float_y = ((float)dest_row_y + 0.5F) * inv_scale_y - 0.5F;
            int32_t dest_pixel_y  = dest_row_y + padding_y;

            /* Skip if padding math produces an out-of-range row. */
            if ((dest_pixel_y < 0) || (dest_pixel_y >= dest_height_px))
            {
                continue;
            }

            for (dest_col_x = 0; dest_col_x < resized_width_px; dest_col_x++)
            {
                float source_float_x = ((float)dest_col_x + 0.5F) * inv_scale_x - 0.5F;
                int32_t dest_pixel_x  = dest_col_x + padding_x;

                /* Skip if padding math produces an out-of-range column. */
                if ((dest_pixel_x < 0) || (dest_pixel_x >= dest_width_px))
                {
                    continue;
                }

                /* Destination tensor is HWC, so compute contiguous base index. */
                int32_t dest_buffer_idx = (dest_pixel_y * dest_width_px + dest_pixel_x) * MODEL_INPUT_C;

                /*
                 * Source is RGB (raw RGB test images).
                 * Model expects BGR (matching Python cv2 pipeline).
                 * Swap R and B channels during copy.
                 * Then quantize: int8 = pixel_value - 128
                 */
                float red_channel   = bilinear_sample_channel(source_image,
                    source_width_px, source_height_px, MODEL_INPUT_C, 0, source_float_x, source_float_y);  /* R */
                float green_channel = bilinear_sample_channel(source_image,
                    source_width_px, source_height_px, MODEL_INPUT_C, 1, source_float_x, source_float_y);  /* G */
                float blue_channel  = bilinear_sample_channel(source_image,
                    source_width_px, source_height_px, MODEL_INPUT_C, 2, source_float_x, source_float_y);  /* B */

                /* Write quantized RGB channels into destination model input tensor. */
                p_destination_image[dest_buffer_idx + 0] = quantize_to_int8(blue_channel);
                p_destination_image[dest_buffer_idx + 1] = quantize_to_int8(green_channel);
                p_destination_image[dest_buffer_idx + 2] = quantize_to_int8(red_channel);

            }
        }
    }
}


/*
 * Expand one RGB565 channel to 8-bit.
 *
 * channel:
 *   0 = R
 *   1 = G
 *   2 = B
 */
static float rgb565_channel(const uint16_t *source,
                            int32_t width,
                            int32_t height,
                            int32_t x,
                            int32_t y,
                            int32_t channel)
{
    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    if (x >= width)
    {
        x = width - 1;
    }

    if (y >= height)
    {
        y = height - 1;
    }

    uint16_t const pixel =
        source[(y * width) + x];

    uint32_t value;

    if (0 == channel)
    {
        uint32_t const r5 =
            ((uint32_t) pixel >> 11U) & 0x1FU;

        value = (r5 << 3U) | (r5 >> 2U);
    }
    else if (1 == channel)
    {
        uint32_t const g6 =
            ((uint32_t) pixel >> 5U) & 0x3FU;

        value = (g6 << 2U) | (g6 >> 4U);
    }
    else
    {
        uint32_t const b5 =
            (uint32_t) pixel & 0x1FU;

        value = (b5 << 3U) | (b5 >> 2U);
    }

    return (float) value;
}


static float bilinear_sample_rgb565(const uint16_t *source,
                                    int32_t width,
                                    int32_t height,
                                    int32_t channel,
                                    float x,
                                    float y)
{
    int32_t x0 = (int32_t) x;
    int32_t y0 = (int32_t) y;

    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;

    float const wx = x - (float) x0;
    float const wy = y - (float) y0;

    float const v00 =
        rgb565_channel(source, width, height,
                       x0, y0, channel);

    float const v10 =
        rgb565_channel(source, width, height,
                       x1, y0, channel);

    float const v01 =
        rgb565_channel(source, width, height,
                       x0, y1, channel);

    float const v11 =
        rgb565_channel(source, width, height,
                       x1, y1, channel);

    float const top =
        v00 * (1.0F - wx) +
        v10 * wx;

    float const bottom =
        v01 * (1.0F - wx) +
        v11 * wx;

    return top * (1.0F - wy) +
           bottom * wy;
}


void preprocess_rgb565(const uint16_t *source_image,
                       uint16_t source_width,
                       uint16_t source_height,
                       int8_t *p_destination_image,
                       uint16_t destination_width,
                       uint16_t destination_height,
                       letterbox_params_t *p_params)
{
    if ((NULL == source_image) ||
        (NULL == p_destination_image))
    {
        return;
    }

    int32_t const src_w =
        (int32_t) source_width;

    int32_t const src_h =
        (int32_t) source_height;

    int32_t const dst_w =
        (int32_t) destination_width;

    int32_t const dst_h =
        (int32_t) destination_height;


    /*
     * Letterbox geometry.
     */
    float const scale_x =
        (float) dst_w / (float) src_w;

    float const scale_y =
        (float) dst_h / (float) src_h;

    float const scale =
        (scale_x < scale_y)
        ? scale_x
        : scale_y;


    int32_t const resized_w =
        (int32_t) ((float) src_w * scale);

    int32_t const resized_h =
        (int32_t) ((float) src_h * scale);

    int32_t const pad_x =
        (dst_w - resized_w) / 2;

    int32_t const pad_y =
        (dst_h - resized_h) / 2;


    if (NULL != p_params)
    {
        p_params->scale = scale;
        p_params->pad_x = pad_x;
        p_params->pad_y = pad_y;
    }


    /*
     * YOLOX letterbox value = 114.
     * Quantized with zero-point -128:
     * 114 - 128 = -14.
     */
    int8_t const pad_value =
        quantize_to_int8(
            (float) INPUT_LETTERBOX_PAD
        );

    memset(
        p_destination_image,
        (int) pad_value,
        (size_t)
        (dst_w *
         dst_h *
         MODEL_INPUT_C)
    );


    float const inv_scale_x =
        (float) src_w /
        (float) resized_w;

    float const inv_scale_y =
        (float) src_h /
        (float) resized_h;


    for (int32_t dy = 0;
         dy < resized_h;
         dy++)
    {
        float const src_y =
            ((float) dy + 0.5F) *
            inv_scale_y -
            0.5F;

        int32_t const out_y =
            dy + pad_y;


        for (int32_t dx = 0;
             dx < resized_w;
             dx++)
        {
            float const src_x =
                ((float) dx + 0.5F) *
                inv_scale_x -
                0.5F;

            int32_t const out_x =
                dx + pad_x;

            int32_t const out_index =
                (out_y * dst_w + out_x) *
                MODEL_INPUT_C;


            /*
             * RGB565 source:
             *   R = channel 0
             *   G = channel 1
             *   B = channel 2
             *
             * Keep the same effective ordering as the
             * existing preprocess():
             *
             * destination = B, G, R
             */
            float const red =
                bilinear_sample_rgb565(
                    source_image,
                    src_w,
                    src_h,
                    0,
                    src_x,
                    src_y
                );

            float const green =
                bilinear_sample_rgb565(
                    source_image,
                    src_w,
                    src_h,
                    1,
                    src_x,
                    src_y
                );

            float const blue =
                bilinear_sample_rgb565(
                    source_image,
                    src_w,
                    src_h,
                    2,
                    src_x,
                    src_y
                );


            p_destination_image[out_index + 0] =
                quantize_to_int8(blue);

            p_destination_image[out_index + 1] =
                quantize_to_int8(green);

            p_destination_image[out_index + 2] =
                quantize_to_int8(red);
        }
    }
}


void preprocess_rgb565_strided(
    const uint16_t * source_image,
    uint16_t         source_width,
    uint16_t         source_height,
    uint32_t         source_stride_bytes,
    int8_t         * p_destination_image,
    uint16_t         destination_width,
    uint16_t         destination_height,
    letterbox_params_t * p_params)
{
    if ((NULL == source_image) ||
        (NULL == p_destination_image))
    {
        return;
    }

    /*
     * This fast path is specifically for the final Camera Frame Contract:
     *
     * source      : 224 x 168 RGB565
     * source row  : 2048-byte stride
     * destination : 224 x 224 x 3 INT8
     *
     * No resize is necessary.
     */
    if ((224U != source_width) ||
        (168U != source_height) ||
        (224U != destination_width) ||
        (224U != destination_height))
    {
        return;
    }


    /*
     * 224x168 fits horizontally without scaling.
     *
     * 224 - 168 = 56
     * top padding    = 28
     * bottom padding = 28
     */
    if (NULL != p_params)
    {
        p_params->scale = 1.0F;
        p_params->pad_x = 0;
        p_params->pad_y = 28;
    }


    /*
     * Model letterbox pad:
     *
     * uint8 114
     *   -> INT8 114 - 128
     *   -> -14
     */
    memset(
        p_destination_image,
        (int) ((int8_t) -14),
        (size_t) destination_width *
        (size_t) destination_height *
        3U
    );


    for (uint32_t y = 0U;
         y < source_height;
         y++)
    {
        /*
         * Frame Cache rows are not tightly packed.
         *
         * row 0 : source + 0
         * row 1 : source + 2048 bytes
         * ...
         */
        const uint8_t * const source_row_bytes =
            ((const uint8_t *) source_image) +
            (y * source_stride_bytes);

        const uint16_t * const source_row =
            (const uint16_t *) source_row_bytes;


        uint32_t const destination_y =
            y + 28U;

        int8_t * const destination_row =
            p_destination_image +
            ((destination_y *
              destination_width) *
             3U);


        for (uint32_t x = 0U;
             x < source_width;
             x++)
        {
            uint16_t const rgb565 =
                source_row[x];


            /*
             * RGB565:
             *
             * RRRRR GGGGGG BBBBB
             */
            uint8_t const red5 =
                (uint8_t)
                ((rgb565 >> 11U) & 0x1FU);

            uint8_t const green6 =
                (uint8_t)
                ((rgb565 >> 5U) & 0x3FU);

            uint8_t const blue5 =
                (uint8_t)
                (rgb565 & 0x1FU);


            /*
             * Expand exactly to 8-bit range.
             */
            uint8_t const red8 =
                (uint8_t)
                ((red5 << 3U) |
                 (red5 >> 2U));

            uint8_t const green8 =
                (uint8_t)
                ((green6 << 2U) |
                 (green6 >> 4U));

            uint8_t const blue8 =
                (uint8_t)
                ((blue5 << 3U) |
                 (blue5 >> 2U));


            uint32_t const destination_index =
                x * 3U;

            /*
             * Keep the same channel order as the currently
             * validated YOLOX RGB565 preprocessing path:
             *
             * B, G, R
             *
             * Quantization:
             * int8 = uint8 - 128
             */
            destination_row[
                destination_index + 0U
            ] =
                (int8_t)
                ((int32_t) blue8 - 128);

            destination_row[
                destination_index + 1U
            ] =
                (int8_t)
                ((int32_t) green8 - 128);

            destination_row[
                destination_index + 2U
            ] =
                (int8_t)
                ((int32_t) red8 - 128);
        }
    }
}