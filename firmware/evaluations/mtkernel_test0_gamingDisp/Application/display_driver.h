#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>


typedef enum
{
    DISPLAY_DRIVER_OK = 0,

    DISPLAY_DRIVER_ERROR_OPEN,
    DISPLAY_DRIVER_ERROR_START,
    DISPLAY_DRIVER_ERROR_BACKLIGHT,
    DISPLAY_DRIVER_ERROR_NOT_INITIALIZED

} display_driver_status_t;


display_driver_status_t display_driver_init(void);

display_driver_status_t display_driver_fill(
    uint32_t color
);

display_driver_status_t display_driver_backlight_on(void);


#endif /* DISPLAY_DRIVER_H */