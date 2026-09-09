#include <tk/tkernel.h>
#include "hal_data.h"

#include "ruhmi_inference_code/model.h"
#include "ruhmi_inference_code/model_io_data.h"
#include "video_source.h"

#include <stdint.h>
#include <string.h>

volatile fsp_err_t g_npu_worker_open_result = FSP_ERR_NOT_OPEN;

volatile uint32_t g_npu_worker_run_count      = 0U;
volatile uint32_t g_npu_worker_error_count    = 0U;
volatile uint32_t g_npu_worker_last_mismatch  = 0U;
volatile uint32_t g_npu_worker_running        = 0U;

volatile uint32_t g_npu_worker_frame_count      = 0U;
volatile uint32_t g_npu_worker_last_frame_index = 0U;
volatile uint32_t g_npu_worker_last_slot        = 0U;
volatile uint32_t g_npu_worker_frame_sample     = 0U;

static void npu_worker_task(INT stacd, void *exinf)
{
    (void) stacd;
    (void) exinf;

    g_npu_worker_open_result =
        RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);

    if (FSP_SUCCESS != g_npu_worker_open_result)
    {
        return;
    }

    video_source_npu_consumer_enable();

    g_npu_worker_running = 1U;

    while (1)
    {
        uint32_t slot;
        uint32_t frame_index;
    
        const uint8_t * frame =
            video_source_acquire_npu_buffer(
                &slot,
                &frame_index);
            
        if (NULL == frame)
        {
            tk_dly_tsk(1);
            continue;
        }
    
        /*
         * Prove that the NPU worker really sees
         * the USB-loaded RGB565 SDRAM frame.
         */
        const uint16_t * pixels =
            (const uint16_t *) frame;
    
        uint32_t const pixel_count =
            VIDEO_SOURCE_WIDTH *
            VIDEO_SOURCE_HEIGHT;
    
        g_npu_worker_frame_sample =
            ((uint32_t) pixels[0]) ^
            ((uint32_t) pixels[pixel_count / 2U] << 8U) ^
            ((uint32_t) pixels[pixel_count - 1U] << 16U);
    
        g_npu_worker_last_slot        = slot;
        g_npu_worker_last_frame_index = frame_index;
        g_npu_worker_frame_count++;
    
        /*
         * RGB565 frame is no longer needed after
         * future preprocessing has copied/resized it
         * into the model input tensor.
         *
         * For this test we release it immediately.
         */
        video_source_release_npu_buffer(slot);
    
        uint32_t mismatches = 0U;
    
        memcpy(GetModelInputPtr_input_1(),
               model_input_1,
               model_input_1_SIZE);
    
        RunModel(false);
    
        int8_t const * const p_actual =
            GetModelOutputPtr_Identity_70029();
    
        for (uint32_t i = 0U;
             i < model_Identity_COUNT;
             i++)
        {
            if (p_actual[i] != model_Identity[i])
            {
                mismatches++;
            }
        }
    
        g_npu_worker_last_mismatch = mismatches;
    
        if (0U == mismatches)
        {
            g_npu_worker_run_count++;
        }
        else
        {
            g_npu_worker_error_count++;
        }
    }
}

void npu_worker_start(void)
{
    T_CTSK ctsk =
    {
        .tskatr  = TA_HLNG,
        .task    = npu_worker_task,
        .itskpri = 12,
        .stksz   = 2048
    };

    ID const task_id = tk_cre_tsk(&ctsk);

    if (task_id > 0)
    {
        tk_sta_tsk(task_id, 0);
    }
}