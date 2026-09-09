#include "hal_data.h"

#include "ruhmi_inference_code/model.h"
#include "ruhmi_inference_code/model_io_data.h"

#include <stdint.h>
#include <string.h>

volatile fsp_err_t g_npu_inference_open_result  = FSP_ERR_NOT_OPEN;
volatile fsp_err_t g_npu_inference_close_result = FSP_ERR_NOT_OPEN;

volatile uint32_t g_npu_inference_mismatches = 0U;
volatile uint32_t g_npu_inference_pass       = 0U;

void npu_inference_test(void)
{
    g_npu_inference_pass       = 0U;
    g_npu_inference_mismatches = 0U;

    g_npu_inference_open_result =
        RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);

    if (FSP_SUCCESS != g_npu_inference_open_result)
    {
        return;
    }

    /* Copy the known-good reference input into the NPU arena. */
    memcpy(GetModelInputPtr_input_1(),
           model_input_1,
           model_input_1_SIZE);

    /* Actual Ethos-U55 inference. */
    RunModel(false);

    int8_t const * const p_actual =
        GetModelOutputPtr_Identity_70029();

    for (uint32_t i = 0U; i < model_Identity_COUNT; i++)
    {
        if (p_actual[i] != model_Identity[i])
        {
            g_npu_inference_mismatches++;
        }
    }

    if (0U == g_npu_inference_mismatches)
    {
        g_npu_inference_pass = 1U;
    }

    g_npu_inference_close_result =
        RM_ETHOSU_Close(&g_rm_ethosu0_ctrl);
}