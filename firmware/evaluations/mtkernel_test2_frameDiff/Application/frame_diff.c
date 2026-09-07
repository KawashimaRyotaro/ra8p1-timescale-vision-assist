#include <stddef.h>
#include "frame_diff.h"


static uint8_t rgb565_to_luma(uint16_t pixel)
{
    uint32_t r5 = (pixel >> 11) & 0x1FU;
    uint32_t g6 = (pixel >> 5)  & 0x3FU;
    uint32_t b5 = pixel & 0x1FU;

    uint32_t r8 = (r5 << 3) | (r5 >> 2);
    uint32_t g8 = (g6 << 2) | (g6 >> 4);
    uint32_t b8 = (b5 << 3) | (b5 >> 2);

    /*
     * 簡易輝度:
     * Y ≈ (R + 2G + B) / 4
     */
    return (uint8_t) ((r8 + (g8 << 1) + b8) >> 2);
}


frame_diff_result_t frame_diff_compute(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint32_t width,
    uint32_t height,
    uint8_t threshold)
{
    frame_diff_result_t result = {0};

    if ((NULL == previous_frame) ||
        (NULL == current_frame)  ||
        (0U == width)            ||
        (0U == height))
    {
        return result;
    }

    uint32_t total_pixels = width * height;
    uint32_t changed_pixels = 0U;
    uint32_t diff_sum = 0U;

    for (uint32_t i = 0U; i < total_pixels; i++)
    {
        uint32_t byte_index = i * 2U;

        uint16_t previous_pixel =
            (uint16_t) previous_frame[byte_index] |
            ((uint16_t) previous_frame[byte_index + 1U] << 8);

        uint16_t current_pixel =
            (uint16_t) current_frame[byte_index] |
            ((uint16_t) current_frame[byte_index + 1U] << 8);

        uint8_t previous_luma =
            rgb565_to_luma(previous_pixel);

        uint8_t current_luma =
            rgb565_to_luma(current_pixel);

        uint32_t diff;

        if (current_luma >= previous_luma)
        {
            diff = current_luma - previous_luma;
        }
        else
        {
            diff = previous_luma - current_luma;
        }

        diff_sum += diff;

        if (diff >= threshold)
        {
            changed_pixels++;
        }
    }

    result.changed_pixels = changed_pixels;
    result.total_pixels = total_pixels;

    result.changed_permille =
        (changed_pixels * 1000U) / total_pixels;

    result.mean_abs_diff =
        diff_sum / total_pixels;

    return result;
}


void frame_diff_build_mask(
    const uint8_t * previous_frame,
    const uint8_t * current_frame,
    uint8_t * mask,
    uint32_t width,
    uint32_t height,
    uint8_t threshold)
{
    if ((NULL == previous_frame) ||
        (NULL == current_frame)  ||
        (NULL == mask))
    {
        return;
    }

    uint32_t total_pixels = width * height;

    for (uint32_t i = 0U; i < total_pixels; i++)
    {
        uint32_t byte_index = i * 2U;

        uint16_t previous_pixel =
            (uint16_t) previous_frame[byte_index] |
            ((uint16_t) previous_frame[byte_index + 1U] << 8);

        uint16_t current_pixel =
            (uint16_t) current_frame[byte_index] |
            ((uint16_t) current_frame[byte_index + 1U] << 8);

        uint8_t previous_luma =
            rgb565_to_luma(previous_pixel);

        uint8_t current_luma =
            rgb565_to_luma(current_pixel);

        uint8_t diff =
            (current_luma >= previous_luma)
            ? (current_luma - previous_luma)
            : (previous_luma - current_luma);

        mask[i] = (diff >= threshold) ? 255U : 0U;
    }
}