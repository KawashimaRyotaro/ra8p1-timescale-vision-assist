#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <stdint.h>
#include <tk/tkernel.h>

typedef enum
{
    DISPLAY_DEBUG_VIEW_RAW_YOLO = 0,
    DISPLAY_DEBUG_VIEW_RAW,
    DISPLAY_DEBUG_VIEW_DIFF,
    DISPLAY_DEBUG_VIEW_MOTION,
    DISPLAY_DEBUG_VIEW_RISK,

    DISPLAY_DEBUG_VIEW_COUNT

} display_debug_view_t;

/*
 * Current debug view.
 *
 * Can also be changed directly from the debugger:
 *   0 = RAW + YOLO bbox
 *   1 = RAW
 *   2 = DIFF    (currently falls back to RAW)
 *   3 = MOTION  (currently falls back to RAW)
 *   4 = RISK    (currently falls back to RAW)
 */
extern volatile uint32_t g_display_debug_view;

ER display_task_create(void);
ER display_task_start(void);

void display_task_set_debug_view(
    display_debug_view_t view
);

#endif /* DISPLAY_TASK_H */
