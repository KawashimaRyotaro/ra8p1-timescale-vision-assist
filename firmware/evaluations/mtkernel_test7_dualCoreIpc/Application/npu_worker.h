#ifndef NPU_WORKER_H
#define NPU_WORKER_H

#include <stdint.h>

#include "yolox_tiny/postprocessing.h"


void npu_worker_start(void);


/*
 * Return detections belonging to exactly this
 * video-source slot and frame index.
 *
 * NULL:
 *     result not ready yet
 *
 * non-NULL:
 *     *p_detection_count contains the number
 *     of valid Detection_t entries.
 */
const Detection_t * npu_worker_get_detections(
    uint32_t slot,
    uint32_t frame_index,
    int32_t * p_detection_count
);


#endif /* NPU_WORKER_H */