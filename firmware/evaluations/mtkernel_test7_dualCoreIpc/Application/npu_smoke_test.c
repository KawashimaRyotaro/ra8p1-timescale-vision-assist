#include <tk/tkernel.h>
#include "hal_data.h"

volatile fsp_err_t g_npu_open_result  = FSP_ERR_NOT_OPEN;
volatile fsp_err_t g_npu_close_result = FSP_ERR_NOT_OPEN;
volatile uint32_t  g_npu_smoke_pass   = 0U;

void npu_smoke_test(void)
{
    g_npu_open_result =
        RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);

    if (FSP_SUCCESS != g_npu_open_result)
    {
        return;
    }

    g_npu_smoke_pass = 1U;

    g_npu_close_result =
        RM_ETHOSU_Close(&g_rm_ethosu0_ctrl);
}