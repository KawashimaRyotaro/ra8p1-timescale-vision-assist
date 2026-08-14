/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/***********************************************************************************************************************
 * File Name    : time_counter.h
 * Description  :
 **********************************************************************************************************************/
#ifndef TIME_COUNTER_H_
#define TIME_COUNTER_H_

#include "hal_data.h"

FSP_CPP_HEADER
void     TimeCounter_Init(void);
void     TimeCounter_Disable(void);
void     TimeCounter_CountReset(void);
uint32_t TimeCounter_CurrentCountGet(void);
uint32_t TimeCounter_ConvertFromCountValueToMs(uint32_t count);
FSP_CPP_FOOTER

#endif /* TIME_COUNTER_H_ */
