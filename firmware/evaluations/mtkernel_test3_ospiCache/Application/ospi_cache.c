#include "ospi_cache.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <tm/tmonitor.h>

#include "hal_data.h"


/* EK-RA8P1 onboard OSPI flash: MX25LW51245G */
#define OSPI_CACHE_EXPECTED_DEVICE_ID          (0x003A86C2UL)

/* Memory-mapped CS1 area */
#define OSPI_CACHE_TEST_ADDRESS                (0x90010000UL)

/* Dedicated self-test sector */
#define OSPI_CACHE_SECTOR_SIZE                 (4096U)
#define OSPI_CACHE_TEST_DATA_SIZE              (64U)

/* Typical operation timeout with margin */
#define OSPI_CACHE_ERASE_TIMEOUT_US            (100000U)
#define OSPI_CACHE_WRITE_TIMEOUT_US            (10000U)

/* MX25LW51245G commands used in SPI mode */
#define OSPI_CMD_WRITE_ENABLE                  (0x06U)
#define OSPI_CMD_READ_STATUS                   (0x05U)
#define OSPI_CMD_READ_ID                       (0x9FU)

#define OSPI_CMD_WRITE_CR2                     (0x72U)
#define OSPI_CMD_READ_CR2                      (0x71U)

#define OSPI_CR2_300_ADDRESS                   (0x00000300UL)
#define OSPI_CR2_300_DATA                      (0x05U)

#define OSPI_STATUS_WEL_MASK                   (0x02U)

#define OSPI_CACHE_BASE_ADDRESS       (0x90000000UL)
#define OSPI_CACHE_CAPACITY_BYTES     (64UL * 1024UL * 1024UL)

#define OSPI_CACHE_FLASH_PAGE_SIZE        (256U)
#define OSPI_CACHE_WRITE_CHUNK_SIZE       (64U)


static ospi_cache_status_t ospi_cache_write_enable(void);
static ospi_cache_status_t ospi_cache_wait_ready(uint32_t timeout_us);
static ospi_cache_status_t ospi_cache_configure_flash(void);
static ospi_cache_status_t ospi_cache_read_device_id(uint32_t * device_id);


/*
 * Send WREN (0x06), then confirm WEL bit in Status Register.
 */
static ospi_cache_status_t ospi_cache_write_enable(void)
{
    fsp_err_t err;

    spi_flash_direct_transfer_t transfer =
    {
        .command        = OSPI_CMD_WRITE_ENABLE,
        .address        = 0U,
        .data           = 0U,
        .command_length = 1U,
        .address_length = 0U,
        .data_length    = 0U,
        .dummy_cycles   = 0U
    };

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_WRITE_ENABLE;
    }

    transfer.command        = OSPI_CMD_READ_STATUS;
    transfer.address        = 0U;
    transfer.data           = 0U;
    transfer.command_length = 1U;
    transfer.address_length = 0U;
    transfer.data_length    = 1U;
    transfer.dummy_cycles   = 0U;

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_READ
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_WRITE_ENABLE;
    }

    if (0U == (transfer.data & OSPI_STATUS_WEL_MASK))
    {
        return OSPI_CACHE_ERROR_WRITE_ENABLE;
    }

    return OSPI_CACHE_OK;
}


/*
 * Poll flash status through FSP until erase/program has completed.
 */
static ospi_cache_status_t ospi_cache_wait_ready(uint32_t timeout_us)
{
    spi_flash_status_t status;
    fsp_err_t err;

    memset(&status, 0, sizeof(status));

    do
    {
        err = R_OSPI_B_StatusGet(
            &g_ospi_ctrl,
            &status
        );

        if (FSP_SUCCESS != err)
        {
            return OSPI_CACHE_ERROR_TIMEOUT;
        }

        if (!status.write_in_progress)
        {
            return OSPI_CACHE_OK;
        }

        if (0U == timeout_us)
        {
            return OSPI_CACHE_ERROR_TIMEOUT;
        }

        R_BSP_SoftwareDelay(
            1U,
            BSP_DELAY_UNITS_MICROSECONDS
        );

        timeout_us--;

    } while (true);

    return OSPI_CACHE_ERROR_TIMEOUT;
}


/*
 * Match the flash-side read latency to the FSP configuration.
 *
 * MX25LW51245G:
 *   Write CR2 command : 0x72
 *   CR2 address       : 0x00000300
 *   data              : 0x05
 */
static ospi_cache_status_t ospi_cache_configure_flash(void)
{
    ospi_cache_status_t status;
    fsp_err_t err;

    spi_flash_direct_transfer_t transfer =
    {
        .command        = OSPI_CMD_WRITE_CR2,
        .address        = OSPI_CR2_300_ADDRESS,
        .data           = OSPI_CR2_300_DATA,
        .command_length = 1U,
        .address_length = 4U,
        .data_length    = 1U,
        .dummy_cycles   = 0U
    };

    status = ospi_cache_write_enable();

    if (OSPI_CACHE_OK != status)
    {
        return status;
    }

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_FLASH_CONFIG;
    }

    /*
     * Read CR2 back and verify.
     */
    transfer.command        = OSPI_CMD_READ_CR2;
    transfer.address        = OSPI_CR2_300_ADDRESS;
    transfer.data           = 0U;
    transfer.command_length = 1U;
    transfer.address_length = 4U;
    transfer.data_length    = 1U;
    transfer.dummy_cycles   = 0U;

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_READ
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_FLASH_CONFIG;
    }

    if (OSPI_CR2_300_DATA != (uint8_t) transfer.data)
    {
        return OSPI_CACHE_ERROR_FLASH_CONFIG;
    }

    return OSPI_CACHE_OK;
}


static ospi_cache_status_t ospi_cache_read_device_id(uint32_t * device_id)
{
    fsp_err_t err;

    if (NULL == device_id)
    {
        return OSPI_CACHE_ERROR_DEVICE_ID;
    }

    spi_flash_direct_transfer_t transfer =
    {
        .command        = OSPI_CMD_READ_ID,
        .address        = 0U,
        .data           = 0U,
        .command_length = 1U,
        .address_length = 0U,
        .data_length    = 3U,
        .dummy_cycles   = 0U
    };

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_READ
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_DEVICE_ID;
    }

    *device_id = transfer.data;

    if (OSPI_CACHE_EXPECTED_DEVICE_ID != transfer.data)
    {
        return OSPI_CACHE_ERROR_DEVICE_ID;
    }

    return OSPI_CACHE_OK;
}


ospi_cache_status_t ospi_cache_init(uint32_t * device_id)
{
    fsp_err_t err;
    ospi_cache_status_t status;

    err = R_OSPI_B_Open(
        &g_ospi_ctrl,
        &g_ospi_cfg
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_OPEN;
    }

    /*
     * Power-on default of MX25LW51245G is SPI mode.
     */
    err = R_OSPI_B_SpiProtocolSet(
        &g_ospi_ctrl,
        SPI_FLASH_PROTOCOL_EXTENDED_SPI
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_PROTOCOL;
    }

    /*
     * Set flash-side dummy-cycle configuration.
     */
    status = ospi_cache_configure_flash();

    if (OSPI_CACHE_OK != status)
    {
        return status;
    }

    /*
     * Finally confirm that the expected physical device is present.
     */
    return ospi_cache_read_device_id(device_id);
}


ospi_cache_status_t ospi_cache_self_test(void)
{
    static const uint8_t write_data[OSPI_CACHE_TEST_DATA_SIZE] =
    {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F,

        0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F,

        0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F,

        0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F
    };

    uint8_t read_data[OSPI_CACHE_TEST_DATA_SIZE];
    uint8_t * const flash_address =
        (uint8_t *) OSPI_CACHE_TEST_ADDRESS;

    fsp_err_t err;
    ospi_cache_status_t status;

    /*
     * 1. Erase exactly one 4 KiB sector.
     *
     * WARNING:
     * 0x90010000 - 0x90010FFF will be erased.
     */
    err = R_OSPI_B_Erase(
        &g_ospi_ctrl,
        flash_address,
        OSPI_CACHE_SECTOR_SIZE
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_ERASE;
    }

    status = ospi_cache_wait_ready(
        OSPI_CACHE_ERASE_TIMEOUT_US
    );

    if (OSPI_CACHE_OK != status)
    {
        return status;
    }

    /*
     * 2. Program 64 bytes.
     */
    err = R_OSPI_B_Write(
        &g_ospi_ctrl,
        write_data,
        flash_address,
        OSPI_CACHE_TEST_DATA_SIZE
    );

    if (FSP_SUCCESS != err)
    {
        return OSPI_CACHE_ERROR_WRITE;
    }

    status = ospi_cache_wait_ready(
        OSPI_CACHE_WRITE_TIMEOUT_US
    );

    if (OSPI_CACHE_OK != status)
    {
        return status;
    }

    /*
     * 3. OSPI is memory mapped.
     *    Reading the address issues the configured flash read transaction.
     */
    memcpy(
        read_data,
        flash_address,
        OSPI_CACHE_TEST_DATA_SIZE
    );

    /*
     * 4. Verify.
     */
    if (0 != memcmp(
            write_data,
            read_data,
            OSPI_CACHE_TEST_DATA_SIZE))
    {
        return OSPI_CACHE_ERROR_VERIFY;
    }

    return OSPI_CACHE_OK;
}


ospi_cache_status_t ospi_cache_store(
    uint32_t offset,
    const uint8_t * data,
    uint32_t size)
{
    fsp_err_t err;
    ospi_cache_status_t status;

    if ((NULL == data) || (0U == size))
    {
        return OSPI_CACHE_ERROR_ARGUMENT;
    }

    /*
     * This function erases sectors before programming, therefore
     * the destination must start at a 4 KiB sector boundary.
     */
    if (0U != (offset % OSPI_CACHE_SECTOR_SIZE))
    {
        return OSPI_CACHE_ERROR_ARGUMENT;
    }

    if ((offset >= OSPI_CACHE_CAPACITY_BYTES) ||
        (size > (OSPI_CACHE_CAPACITY_BYTES - offset)))
    {
        return OSPI_CACHE_ERROR_ARGUMENT;
    }

    uint8_t * const flash_base =
        (uint8_t *) (OSPI_CACHE_BASE_ADDRESS + offset);

    /*
     * Number of sectors that contain this data.
     */
    uint32_t erase_size =
        ((size + OSPI_CACHE_SECTOR_SIZE - 1U) /
         OSPI_CACHE_SECTOR_SIZE) *
        OSPI_CACHE_SECTOR_SIZE;

    /*
     * 1. Erase all sectors occupied by the data.
     */
    for (uint32_t pos = 0U;
         pos < erase_size;
         pos += OSPI_CACHE_SECTOR_SIZE)
    {
        err = R_OSPI_B_Erase(
            &g_ospi_ctrl,
            flash_base + pos,
            OSPI_CACHE_SECTOR_SIZE
        );

        if (FSP_SUCCESS != err)
        {
            tm_printf(
                (UB *)"[OSPI] erase failed: pos=%u err=%d\n",
                pos,
                err
            );

            return OSPI_CACHE_ERROR_ERASE;
        }

        status =
            ospi_cache_wait_ready(OSPI_CACHE_ERASE_TIMEOUT_US);

        if (OSPI_CACHE_OK != status)
        {
            return status;
        }
    }

    /*
     * 2. Program the data page by page.
     *
     * MX25LW51245G / r_ospi_b uses a 256-byte program page.
     * R_OSPI_B_Write() must not cross a page boundary.
     */
    uint32_t pos = 0U;

    while (pos < size)
    {
        uint32_t remaining = size - pos;

        uint32_t chunk =
            (remaining > OSPI_CACHE_WRITE_CHUNK_SIZE)
            ? OSPI_CACHE_WRITE_CHUNK_SIZE
            : remaining;

        err = R_OSPI_B_Write(
            &g_ospi_ctrl,
            data + pos,
            flash_base + pos,
            chunk
        );

        if (FSP_SUCCESS != err)
        {
            tm_printf(
                (UB *)"[OSPI] write failed:"
                    " pos=%u chunk=%u err=%d\n",
                pos,
                chunk,
                err
            );

            return OSPI_CACHE_ERROR_WRITE;
        }

        status =
            ospi_cache_wait_ready(
                OSPI_CACHE_WRITE_TIMEOUT_US
            );

        if (OSPI_CACHE_OK != status)
        {
            tm_printf(
                (UB *)"[OSPI] write wait failed:"
                    " pos=%u status=%d\n",
                pos,
                status
            );

            return status;
        }

        pos += chunk;
    }

    /*
     * 3. Verify by memory-mapped read.
     */
    for (uint32_t i = 0U; i < size; i++)
    {
        uint8_t actual   = flash_base[i];
        uint8_t expected = data[i];

        if (actual != expected)
        {
            tm_printf(
                (UB *)"[OSPI] verify mismatch:"
                    " pos=%u"
                    " addr=0x%08X"
                    " expected=0x%02X"
                    " actual=0x%02X\n",
                i,
                (uint32_t)(flash_base + i),
                expected,
                actual
            );

            return OSPI_CACHE_ERROR_VERIFY;
        }
    }

    tm_printf(
        (UB *)"[OSPI] verify OK: %u bytes\n",
        size
    );

    return OSPI_CACHE_OK;

        return OSPI_CACHE_OK;
    }


const uint8_t * ospi_cache_mapped_ptr(uint32_t offset)
{
    if (offset >= OSPI_CACHE_CAPACITY_BYTES)
    {
        return NULL;
    }

    return (const uint8_t *)
        (OSPI_CACHE_BASE_ADDRESS + offset);
}