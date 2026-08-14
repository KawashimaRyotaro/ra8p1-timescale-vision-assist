/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/***********************************************************************************************************************
 * File Name    : external_memory.h
 * Description  :
 **********************************************************************************************************************/
#ifndef EXTERNAL_MEMORY_H__
#define EXTERNAL_MEMORY_H__

#include "hal_data.h"

FSP_CPP_HEADER
#if __has_include("r_ospi_b.h")
fsp_err_t user_ospi_b_init(void * p_driver_instance);
#endif
FSP_CPP_FOOTER

#endif /* EXTERNAL_MEMORY_H__ */
