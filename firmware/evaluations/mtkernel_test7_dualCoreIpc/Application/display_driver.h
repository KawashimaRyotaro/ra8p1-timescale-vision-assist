#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>


typedef enum
{
    DISPLAY_DRIVER_OK = 0,

    DISPLAY_DRIVER_ERROR_OPEN,
    DISPLAY_DRIVER_ERROR_START,
    DISPLAY_DRIVER_ERROR_BACKLIGHT,
    DISPLAY_DRIVER_ERROR_NOT_INITIALIZED,
    DISPLAY_DRIVER_ERROR_ARGUMENT,
    DISPLAY_DRIVER_ERROR_BUFFER_CHANGE

} display_driver_status_t;


display_driver_status_t display_driver_init(void);

display_driver_status_t display_driver_fill(
    uint32_t color
);

display_driver_status_t display_driver_backlight_on(void);

display_driver_status_t display_driver_present_rgb565(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height
);

uint8_t display_driver_release_pending(void);

uint8_t display_driver_arm_release(
    uint32_t slot
);

display_driver_status_t display_driver_draw_rect_rgb565(
    uint8_t * frame,
    uint32_t frame_width,
    uint32_t frame_height,
    int32_t x1,
    int32_t y1,
    int32_t x2,
    int32_t y2,
    uint16_t color,
    uint32_t thickness
);

#endif /* DISPLAY_DRIVER_H */