/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/***********************************************************************************************************************
 * File Name    : external_memory.c
 * Description  :
 **********************************************************************************************************************/
#include "hal_data.h"
#include "common_data.h"
#include "external_memory.h"

#if __has_include("r_ospi_b.h")
fsp_err_t user_ospi_b_init(void * p_driver_instance)
{
    fsp_err_t fsp_status = FSP_SUCCESS;

    spi_flash_instance_t * p_instance = (spi_flash_instance_t *)p_driver_instance;
    ospi_b_instance_ctrl_t * p_ctrl = (ospi_b_instance_ctrl_t *)p_instance->p_ctrl;

    fsp_status = R_OSPI_B_Open(p_instance->p_ctrl, p_instance->p_cfg);
    if(FSP_SUCCESS != fsp_status)
    {
        return fsp_status;
    }

    R_XSPI0_Type * p_reg = p_ctrl->p_reg;

    /* Reset flash device */
    p_reg->LIOCTL_b.RSTCS0 = 0;
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
    p_reg->LIOCTL_b.RSTCS0 = 1;
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);

    fsp_status = R_OSPI_B_SpiProtocolSet(p_instance->p_ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);
    if(FSP_SUCCESS != fsp_status)
    {
        return fsp_status;
    }

    return fsp_status;
}
#endif
