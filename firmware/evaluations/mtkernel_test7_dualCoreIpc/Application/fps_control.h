#ifndef FPS_CONTROL_H
#define FPS_CONTROL_H

#include <stdint.h>


/*
 * target_fps == 0:
 *     unlimited mode
 */
#define FPS_CONTROL_UNLIMITED    (0U)


void fps_control_init(
    uint32_t target_fps
);

void fps_control_set_target_fps(
    uint32_t target_fps
);

void fps_control_wait_before_frame(void);

void fps_control_usb_read_begin(
    uint32_t frame_index
);

void fps_control_usb_read_end(void);

void fps_control_display_begin(
    uint32_t frame_index
);

void fps_control_display_end(void);

void fps_control_frame_presented(
    uint32_t frame_index
);


#endif /* FPS_CONTROL_H */