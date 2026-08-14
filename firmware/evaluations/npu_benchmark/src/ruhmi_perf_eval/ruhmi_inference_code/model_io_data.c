/*
 * This file is developed by EdgeCortix Inc. to be used with certain Renesas Electronics Hardware only.
 *
 * Copyright © 2025 EdgeCortix Inc. Licensed to Renesas Electronics Corporation with the
 * right to sublicense under the Apache License, Version 2.0.
 *
 * This file also includes source code originally developed by the Renesas Electronics Corporation.
 * The Renesas disclaimer below applies to any Renesas-originated portions for usage of the code.
 *
 * The Renesas Electronics Corporation
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED 'AS IS' AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
 * SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Changed from original python code to C source code.
 * Copyright (C) 2017 Renesas Electronics Corporation. All rights reserved.
 *
 * This file also includes source codes originally developed by the TensorFlow Authors which were distributed under the following conditions.
 *
 * The TensorFlow Authors
 * Copyright 2023 The Apache Software Foundation
 *
 * This product includes software developed at
 * The Apache Software Foundation (http://www.apache.org/).
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "model_io_data.h"

// Input tensor: input_1
// Data Type: INT8
// Shape: [1, 640]
// Number of elements: 640
// Size in bytes: 640
int8_t model_input_1[] = {
  12, 23, 55, 88, 26, 91, 11, 88, -20, 31,
  37, -30, -16, -52, 100, -114, 118, -59, -30, -6,
  74, 79, 7, -6, 17, -28, 108, 86, -110, -42,
  -106, 37, -123, -34, 85, 117, 71, -93, 94, 94,
  122, -7, 76, 77, -10, 5, 71, 45, -98, 56,
  35, 20, -92, 9, 113, 66, 5, -101, -22, -7,
  -61, -81, 70, 60, -12, -73, 17, -94, -124, -46,
  30, -90, 28, -72, 29, -30, 113, 103, 46, -13,
  -36, 28, -17, 103, 50, -103, -113, 120, 42, 39,
  43, -85, -75, -37, -95, 64, -48, 27, -35, -45,
  17, -119, -16, 34, 125, 117, -102, 39, -75, 34,
  -87, 126, 39, 20, -64, -22, -9, -7, -66, 31,
  -88, -42, -100, 44, 40, -47, -93, 71, -78, 115,
  -34, 41, 82, -125, -104, 31, 86, 44, -104, 120,
  121, 96, -9, 2, 122, -114, 26, -13, 61, -123,
  -118, -15, -56, 122, -98, -36, -53, -5, -98, 48,
  -47, 97, -22, 107, -112, -73, 49, 16, 17, 93,
  -61, 2, 5, 106, -104, 107, 19, -107, 109, -57,
  -47, -126, 42, 87, -95, 37, 55, 87, -54, -61,
  -82, -27, 22, 13, -123, -86, 84, -34, -127, -91,
  45, 17, -59, 52, 60, -55, 118, -18, -65, 65,
  19, -27, 23, 101, 18, 35, -71, 100, 115, 46,
  -14, -14, 88, 122, 51, -99, -52, 68, 80, -23,
  -27, 44, 97, -65, 20, -48, 97, 119, 49, 22,
  57, 40, 0, 8, 116, -69, 36, -27, -20, 30,
  27, -7, -124, -8, -51, 55, 41, -55, -54, -30,
  30, 63, -19, 96, -94, -102, -52, -105, 17, -38,
  23, 13, 19, -120, 39, 120, 38, -46, -18, -72,
  101, -92, -34, -104, -17, 123, 100, -62, 78, 9,
  52, -14, -103, -103, 107, -38, 54, -8, 127, 87,
  -90, 103, 94, -119, -87, 2, 29, -86, -97, 71,
  89, 93, 78, -23, 17, -93, -24, -120, -111, 123,
  50, -33, -12, -21, 56, -116, 93, -35, 121, -124,
  91, -69, -126, 67, -36, 113, 58, 63, -85, -42,
  5, -3, -115, -42, -77, -83, -124, -85, 75, -10,
  -71, 95, -40, 113, 109, 27, 52, 24, -120, 72,
  -86, 0, 31, -116, 19, 50, -68, 126, 111, -60,
  29, 45, 9, 93, 23, 64, 58, 118, -49, 13,
  -27, -74, -75, -72, -81, -73, 113, 17, 61, -13,
  -3, 120, -70, 46, -63, -107, -114, -114, -17, -4,
  -49, 97, 50, 121, -32, 30, -83, 10, -122, 90,
  -111, 62, 45, -6, -12, 45, 9, 27, 101, 54,
  125, -8, -73, -12, 41, 104, -61, -93, -123, -70,
  66, 97, -47, 103, -30, 37, 22, -45, 84, 5,
  33, -128, 95, -49, -58, -20, 76, 98, -81, 46,
  115, -12, 47, -5, -73, 73, 114, -70, 59, 97,
  -63, -48, -74, 117, 4, -8, -122, 54, -75, -89,
  -20, 58, -33, 37, -10, -73, -57, -81, 22, 78,
  93, 63, -98, 44, 4, -58, -95, -84, 55, 52,
  -27, -10, 16, 87, -82, -76, -91, -86, -4, -97,
  -37, 56, 112, -121, 67, 63, 63, -105, 103, -73,
  -107, 63, 13, 59, 21, -117, 118, -75, -54, -55,
  -67, 45, -103, -112, -124, 14, 109, -126, 43, 85,
  72, 123, -56, 52, 22, -82, -112, 3, -4, 20,
  122, 73, 96, 27, -42, -73, 118, -12, -69, 96,
  115, -2, 112, 55, 76, -4, 33, 53, 95, -1,
  -53, 88, 89, -79, 30, 69, -125, 121, -40, 92,
  -91, 71, 123, 124, -6, 64, -1, -127, 35, -60,
  -34, -23, -93, -19, 82, -52, -80, -26, 2, -98,
  -71, 123, -103, -25, 92, 17, 121, -41, 117, 73,
  104, -23, 70, -37, -43, -26, -108, -51, -24, 70,
  -69, 109, -95, -45, -115, 115, 57, -125, -126, 8,
  69, -51, -91, 98, -108, -64, -106, 45, 44, 79,
  -66, -18, -21, 64, 14, 84, 92, -31, 58, -104,
};

// Output tensor: Identity
// Data Type: INT8
// Shape: [1, 640]
// Number of elements: 640
// Size in bytes: 640
int8_t model_Identity[] = {
  -28, 15, 44, 65, 60, 56, 51, 64, 53, 55,
  55, 46, 33, 33, 29, 31, 21, 20, 18, 26,
  33, 34, 21, 20, 18, 13, 6, 11, 6, 10,
  9, 16, 22, 19, 8, 12, 12, 21, 19, 17,
  20, 24, 20, 13, 7, 16, 17, 10, 8, 13,
  16, 20, 20, 16, 14, 13, 8, 7, 5, -1,
  1, 10, 12, 2, -4, -3, 0, 0, 1, 2,
  -1, 1, 3, 9, 16, 11, 10, 6, 14, 11,
  3, 4, -3, -6, -10, -15, -13, -12, -3, 3,
  3, 1, -5, -10, -6, -4, -11, -11, -4, -8,
  0, -3, 0, -1, -2, -4, -3, -7, -15, -11,
  -10, -9, -8, -13, -13, -3, 0, -4, -10, -7,
  0, 2, 0, 4, 4, 1, -22, -61, -27, 17,
  45, 66, 60, 56, 51, 65, 54, 55, 55, 48,
  35, 34, 30, 31, 21, 21, 21, 27, 34, 36,
  22, 21, 16, 13, 7, 12, 8, 13, 10, 18,
  24, 20, 10, 13, 12, 22, 20, 18, 19, 23,
  20, 15, 9, 19, 17, 11, 7, 12, 15, 20,
  19, 15, 12, 14, 10, 9, 4, -3, 2, 9,
  11, 1, -4, -3, 0, 1, 2, 3, -1, 1,
  4, 13, 19, 13, 13, 8, 17, 15, 7, 8,
  2, -3, -7, -12, -10, -9, 1, 9, 6, 6,
  1, -4, 0, 2, -7, -5, 2, -1, 7, 4,
  6, 7, 5, 1, 3, -1, -11, -7, -5, -4,
  -3, -9, -8, 2, 4, 3, -6, -2, 5, 8,
  6, 15, 13, 8, -15, -52, -27, 15, 41, 62,
  56, 53, 50, 64, 53, 55, 55, 48, 36, 35,
  30, 32, 21, 23, 24, 28, 33, 34, 22, 21,
  17, 13, 7, 13, 10, 14, 11, 17, 24, 21,
  12, 15, 15, 24, 23, 20, 22, 25, 21, 16,
  10, 23, 20, 11, 8, 13, 17, 21, 21, 17,
  16, 15, 11, 10, 7, -2, 3, 12, 13, 3,
  -1, 0, 2, 3, 6, 6, 3, 6, 9, 15,
  19, 13, 14, 11, 18, 15, 8, 7, 3, 0,
  -4, -10, -6, -4, 4, 6, 4, 7, 1, -3,
  0, 2, -7, -5, 1, -2, 4, 3, 6, 7,
  5, 0, 2, -2, -12, -9, -5, -4, -5, -9,
  -7, 1, 4, 3, -5, -3, 5, 7, 6, 16,
  13, 7, -15, -53, -29, 11, 37, 59, 54, 51,
  49, 61, 50, 53, 54, 47, 36, 36, 31, 32,
  21, 23, 25, 29, 32, 33, 21, 20, 16, 14,
  7, 13, 10, 14, 10, 16, 21, 20, 12, 16,
  15, 24, 23, 22, 23, 26, 22, 16, 11, 23,
  20, 13, 10, 14, 18, 21, 23, 21, 18, 16,
  11, 14, 11, 4, 7, 15, 16, 6, 2, 3,
  5, 5, 9, 10, 7, 9, 12, 17, 20, 14,
  14, 13, 19, 16, 9, 7, 4, 0, -5, -10,
  -5, -2, 4, 5, 2, 4, -2, -6, -3, -1,
  -8, -9, -3, -7, -1, -2, 0, 1, 1, -5,
  -4, -6, -15, -12, -9, -7, -8, -12, -11, -2,
  0, -2, -9, -6, 1, 3, 2, 10, 7, 2,
  -21, -59, -30, 11, 36, 59, 55, 52, 50, 60,
  50, 53, 53, 46, 38, 38, 33, 33, 22, 25,
  26, 30, 34, 34, 21, 19, 16, 12, 6, 12,
  8, 14, 11, 17, 22, 20, 12, 14, 14, 22,
  21, 19, 22, 26, 23, 17, 10, 22, 18, 11,
  9, 12, 17, 20, 20, 19, 17, 14, 10, 10,
  7, 2, 5, 12, 13, 3, 0, 0, 2, 2,
  6, 8, 5, 6, 8, 14, 18, 11, 11, 10,
  18, 15, 7, 6, 1, -2, -9, -15, -10, -7,
  1, 4, 0, 1, -3, -9, -6, -3, -10, -10,
  -5, -9, -3, -3, -2, 0, 0, -5, -5, -6,
  -14, -12, -8, -8, -7, -12, -11, -1, -1, -3,
  -9, -7, -1, 2, 0, 7, 5, 0, -22, -60,
};

