#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <stdint.h>
#include <tk/tkernel.h>

/*
 * Debug overlay bit mask.
 *
 * Multiple overlays can be enabled simultaneously.
 *
 *   0 = RAW only
 *   1 = BBOX
 *   2 = VECTOR
 *   4 = ACTIVITY
 *
 * Examples:
 *   3 = BBOX + VECTOR
 *   5 = BBOX + ACTIVITY
 *   6 = VECTOR + ACTIVITY
 *   7 = BBOX + VECTOR + ACTIVITY
 */
#define DISPLAY_OVERLAY_NONE        (0U)
#define DISPLAY_OVERLAY_BBOX        (1U << 0)
#define DISPLAY_OVERLAY_VECTOR      (1U << 1)
#define DISPLAY_OVERLAY_ACTIVITY    (1U << 2)

#define DISPLAY_OVERLAY_ALL \
    (DISPLAY_OVERLAY_BBOX | \
     DISPLAY_OVERLAY_VECTOR | \
     DISPLAY_OVERLAY_ACTIVITY)


extern volatile uint32_t g_display_overlay_mask;


ER display_task_create(void);
ER display_task_start(void);


void display_task_set_overlay_mask(
    uint32_t overlay_mask
);

#endif /* DISPLAY_TASK_H */
