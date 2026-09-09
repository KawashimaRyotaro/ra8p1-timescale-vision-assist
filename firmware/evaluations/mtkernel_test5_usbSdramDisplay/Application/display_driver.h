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

display_driver_status_t display_driver_present_rgb888(
    const uint8_t * source,
    uint32_t source_width,
    uint32_t source_height
);


#endif /* DISPLAY_DRIVER_H */