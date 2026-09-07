#ifndef OSPI_CACHE_H
#define OSPI_CACHE_H

#include <stdint.h>

typedef enum
{
    OSPI_CACHE_OK                 = 0,
    OSPI_CACHE_ERROR_OPEN         = 1,
    OSPI_CACHE_ERROR_PROTOCOL     = 2,
    OSPI_CACHE_ERROR_WRITE_ENABLE = 3,
    OSPI_CACHE_ERROR_FLASH_CONFIG = 4,
    OSPI_CACHE_ERROR_DEVICE_ID    = 5,
    OSPI_CACHE_ERROR_ERASE        = 6,
    OSPI_CACHE_ERROR_WRITE        = 7,
    OSPI_CACHE_ERROR_TIMEOUT      = 8,
    OSPI_CACHE_ERROR_VERIFY       = 9,
    OSPI_CACHE_ERROR_ARGUMENT     = 10
} ospi_cache_status_t;

ospi_cache_status_t ospi_cache_init(uint32_t * device_id);
ospi_cache_status_t ospi_cache_self_test(void);
ospi_cache_status_t ospi_cache_store(
    uint32_t offset,
    const uint8_t * data,
    uint32_t size
);

const uint8_t * ospi_cache_mapped_ptr(uint32_t offset);

#endif