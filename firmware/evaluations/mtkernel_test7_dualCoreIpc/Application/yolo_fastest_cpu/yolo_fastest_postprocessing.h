/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**
 ******************************************************************************
 * @file    postprocessing.h
 * @brief   YOLO-Fastest 1.1 INT8 anchor-based decoding + per-class NMS for MCU.
 *
 * Decodes two raw anchor-based YOLO heads:
 *   Head 0: [1, 10, 10, 255]  (stride=32, large objects)
 *   Head 1: [1, 20, 20, 255]  (stride=16, small objects)
 *
 * Each cell: 3 anchors ÁE85 channels = 255
 *   85 = 4 (tx,ty,tw,th) + 1 (objectness) + 80 (class logits)
 *
 * Detection layout:
 *   [x1, y1, x2, y2]   Eabsolute pixel coords in the ORIGINAL image
 *   score               Esigmoid(obj) ÁEsigmoid(class) confidence
 *   cls_id              ECOCO class index [0, 79]
 ******************************************************************************
 */

#ifndef YOLO_FASTEST_PREPROCESSING_H
#define YOLO_FASTEST_PREPROCESSING_H

#include <stdint.h>
#include "model_metadata.h"

typedef struct
{
    float    scale;
    int32_t  pad_x;
    int32_t  pad_y;
} yolo_fastest_letterbox_params_t;

#define SIGMOID_ONE_F                    (1.0F)
#define BOX_HALF_FACTOR                  (0.5F)

#define YOLO_BOX_TX_INDEX                (0U)
#define YOLO_BOX_TY_INDEX                (1U)
#define YOLO_BOX_TW_INDEX                (2U)
#define YOLO_BOX_TH_INDEX                (3U)
#define YOLO_OBJECTNESS_INDEX            (4U)
#define YOLO_CLASS_LOGITS_START_INDEX    (5U)

#define IOU_EPSILON_F                    (1e-6F)
#define SUPPRESSED_SCORE_F               (-1.0F)

#define MAX_RAW_DETS                     (MODEL_TOTAL_ANCHORS)

/* ── Max detections buffer (post-NMS) ────────────────────────────────── */
#define MAX_DETECTIONS          POSTPROC_MAX_DETS

/* ── Detection result structure ──────────────────────────────────────── */
typedef struct
{
    float    x1;        /**< Top-left x (original image pixels)              */
    float    y1;        /**< Top-left y (original image pixels)              */
    float    x2;        /**< Bottom-right x (original image pixels)          */
    float    y2;        /**< Bottom-right y (original image pixels)          */
    float    score;     /**< sigmoid(obj) ÁEsigmoid(class) confidence        */
    uint32_t cls_id;    /**< COCO class index [0, 79]                        */
} Detection_t;

/**
 * @brief  Decode raw YOLO-Fastest INT8 dual-head output ↁEfiltered Detection_t array.
 *
 * Pipeline:
 *   1. Dequantize int8 output to float32 per head
 *   2. For each grid cell, for each anchor:
 *      a. sigmoid(objectness) ↁEearly exit if below threshold
 *      b. sigmoid(tx,ty) + grid offset ↁEcentre, exp(tw,th) ÁEanchor ↁEsize
 *      c. sigmoid(class logits) ↁEargmax class
 *      d. score = obj ÁEclass_conf
 *   3. Undo letterbox padding ↁEoriginal image coordinates
 *   4. Per-class greedy NMS
 *
 * @param[in]  p_raw_head0   Flat int8 array [10 ÁE10 ÁE255] (stride-32 head).
 * @param[in]  p_raw_head1   Flat int8 array [20 ÁE20 ÁE255] (stride-16 head).
 * @param[in]  p_params      Letterbox parameters from preprocess().
 * @param[in]  orig_w        Original image width  (pixels).
 * @param[in]  orig_h        Original image height (pixels).
 * @param[in]  score_thresh  Minimum detection score to keep.
 * @param[in]  nms_thresh    IoU threshold for per-class NMS.
 * @param[out] p_out_dets    Output array; must hold at least MAX_DETECTIONS entries.
 * @return                   Number of detections written to p_out_dets.
 */
int32_t yolo_fastest_postprocess(const int8_t           *p_raw_head0,
                    const int8_t           *p_raw_head1,
                    const yolo_fastest_letterbox_params_t *p_params,
                    uint32_t               orig_w,
                    uint32_t               orig_h,
                    float                  score_thresh,
                    float                  nms_thresh,
                    Detection_t           *p_out_dets);

#endif /* POSTPROCESSING_H */


