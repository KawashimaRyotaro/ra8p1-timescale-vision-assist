#ifndef MOTION_TASK_H
#define MOTION_TASK_H

#include <stdint.h>
#include <tk/tkernel.h>

#include "motion_preprocess.h"


#define MOTION_GRAY_BUFFER_COUNT    (2U)


/*
 * Exposed for debugger inspection during CP1.
 *
 * Two compact internal-RAM buffers:
 *   2 x 112 x 84 x 1 byte = 18,816 bytes.
 */
extern uint8_t g_motion_gray_buffers
    [MOTION_GRAY_BUFFER_COUNT]
    [MOTION_GRAY_PIXELS];

extern volatile uint32_t g_motion_gray_frame_count;
extern volatile uint32_t g_motion_gray_last_frame_index;
extern volatile uint32_t g_motion_gray_last_buffer;
extern volatile uint32_t g_motion_gray_consecutive_count;
extern volatile uint32_t g_motion_gray_gap_count;
extern volatile uint32_t g_motion_gray_last_checksum;


ER motion_task_create(void);
ER motion_task_start(void);


#endif /* MOTION_TASK_H */
