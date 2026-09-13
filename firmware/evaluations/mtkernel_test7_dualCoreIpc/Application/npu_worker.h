#ifndef NPU_WORKER_H
#define NPU_WORKER_H

#include <stdint.h>

#include "yolox_tiny/postprocessing.h"


void npu_worker_start(void);


/*
 * Legacy exact-frame API.
 *
 * Returns detections only when the result belongs to exactly the
 * requested video-source slot and frame index.
 */
const Detection_t * npu_worker_get_detections(
    uint32_t slot,
    uint32_t frame_index,
    int32_t * p_detection_count
);


/*
 * Copy the newest COMPLETED YOLOX result.
 *
 * This API is intended for asynchronous debug display:
 *
 *   RAW video : 30 FPS
 *   YOLOX     : ~6-7 FPS
 *
 * Display never waits for YOLOX.  The last completed result is copied
 * into p_out_detections and may be reused across several RAW frames.
 *
 * Return:
 *   >= 0 : number of copied detections
 *   -1   : no completed result yet, invalid argument, or snapshot retry
 *
 * p_result_frame_index receives the RAW frame index from which this
 * YOLOX result was produced.
 */
int32_t npu_worker_copy_latest_detections(
    Detection_t * p_out_detections,
    uint32_t capacity,
    uint32_t * p_result_frame_index
);


#endif /* NPU_WORKER_H */
