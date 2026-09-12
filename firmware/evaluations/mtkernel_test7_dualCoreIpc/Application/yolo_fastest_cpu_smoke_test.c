#include <tk/tkernel.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "yolo_fastest_cpu/yolo_fastest_compute.h"


volatile uint32_t g_yolo_fastest_smoke_done = 0U;
volatile uint32_t g_yolo_fastest_smoke_elapsed_ms = 0U;

volatile uint32_t g_yolo_fastest_smoke_head20_changed = 0U;
volatile uint32_t g_yolo_fastest_smoke_head10_changed = 0U;

volatile int8_t g_yolo_fastest_smoke_head20_first = 0;
volatile int8_t g_yolo_fastest_smoke_head10_first = 0;


/*
 * CPU model working memory.
 *
 * Keep the large buffers in external SDRAM.
 */
__attribute__((aligned(16)))
static uint8_t s_yolo_fastest_workspace[kBufferSize_sub_0000];

__attribute__((section(".sdram"), aligned(16)))
static int8_t s_yolo_fastest_input[307200];

__attribute__((section(".sdram"), aligned(16)))
static int8_t s_yolo_fastest_head20[102000];

__attribute__((section(".sdram"), aligned(16)))
static int8_t s_yolo_fastest_head10[25500];


static uint8_t get_time_ms(uint64_t * p_time_ms)
{
    if (NULL == p_time_ms)
    {
        return 0U;
    }

    SYSTIM system_time;

    ER const err = tk_get_otm(&system_time);

    if (err < E_OK)
    {
        return 0U;
    }

    *p_time_ms =
        ((uint64_t)(uint32_t)system_time.hi << 32) |
        (uint64_t)system_time.lo;

    return 1U;
}


static uint32_t count_changed_bytes(
    const int8_t * p_data,
    size_t         size,
    int8_t         initial_value)
{
    uint32_t changed = 0U;

    for (size_t i = 0U; i < size; i++)
    {
        if (p_data[i] != initial_value)
        {
            changed++;
        }
    }

    return changed;
}


void yolo_fastest_cpu_smoke_test(void)
{
    /*
     * Quantized black RGB:
     *
     * real pixel 0
     * zero point -128
     * -> INT8 -128
     */
    memset(
        s_yolo_fastest_input,
        0x80,
        sizeof(s_yolo_fastest_input)
    );

    /*
     * Sentinel value so that we can prove
     * the generated model actually wrote its outputs.
     */
    memset(
        s_yolo_fastest_head20,
        0x55,
        sizeof(s_yolo_fastest_head20)
    );

    memset(
        s_yolo_fastest_head10,
        0x55,
        sizeof(s_yolo_fastest_head10)
    );


    uint64_t start_ms = 0U;
    uint64_t end_ms   = 0U;

    uint8_t const timing_valid =
        get_time_ms(&start_ms);


    yolo_fastest_compute(
        s_yolo_fastest_workspace,
        s_yolo_fastest_input,
        s_yolo_fastest_head20,
        s_yolo_fastest_head10
    );


    if ((0U != timing_valid) &&
        (0U != get_time_ms(&end_ms)))
    {
        g_yolo_fastest_smoke_elapsed_ms =
            (uint32_t)(end_ms - start_ms);
    }


    g_yolo_fastest_smoke_head20_changed =
        count_changed_bytes(
            s_yolo_fastest_head20,
            sizeof(s_yolo_fastest_head20),
            (int8_t)0x55
        );

    g_yolo_fastest_smoke_head10_changed =
        count_changed_bytes(
            s_yolo_fastest_head10,
            sizeof(s_yolo_fastest_head10),
            (int8_t)0x55
        );

    g_yolo_fastest_smoke_head20_first =
        s_yolo_fastest_head20[0];

    g_yolo_fastest_smoke_head10_first =
        s_yolo_fastest_head10[0];

    g_yolo_fastest_smoke_done = 1U;
}