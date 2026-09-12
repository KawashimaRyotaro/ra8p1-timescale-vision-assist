/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "hal_data.h"

/*
 * Manual OSPI startup for EK-RA8P1 onboard MX25LW51245G.
 *
 * The model image is linked at OSPI0_CS1 (0x90000000) and copied
 * to SDRAM during C runtime initialization.  Therefore OSPI must be
 * memory-mapped before SystemRuntimeInit(1).
 */

#define OSPI_STARTUP_CMD_WRITE_ENABLE    (0x06U)
#define OSPI_STARTUP_CMD_WRITE_CR2       (0x72U)
#define OSPI_STARTUP_CMD_READ_CR2        (0x71U)

#define OSPI_STARTUP_CR2_ADDRESS         (0x00000300UL)
#define OSPI_STARTUP_CR2_VALUE           (0x05U)

/* Debug values:
 * 0 = not started
 * 1 = R_OSPI_B_Open
 * 2 = protocol setup
 * 3 = write enable
 * 4 = CR2 write
 * 5 = CR2 readback
 * 6 = initialization complete
 */
volatile uint32_t g_ospi_startup_stage = 0U;
volatile fsp_err_t g_ospi_startup_result = FSP_SUCCESS;


static fsp_err_t ospi_startup_init(void)
{
    fsp_err_t err;

    /*
     * Open OSPI_B0 / CS1.
     */
    g_ospi_startup_stage = 1U;

    err = R_OSPI_B_Open(
        &g_ospi_ctrl,
        &g_ospi_cfg);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /*
     * MX25LW51245G powers up in Extended SPI mode.
     * Use the same protocol as the already verified OSPI test code.
     */
    g_ospi_startup_stage = 2U;

    err = R_OSPI_B_SpiProtocolSet(
        &g_ospi_ctrl,
        SPI_FLASH_PROTOCOL_EXTENDED_SPI);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /*
     * Write Enable (06h).
     */
    g_ospi_startup_stage = 3U;

    spi_flash_direct_transfer_t transfer =
    {
        .command        = OSPI_STARTUP_CMD_WRITE_ENABLE,
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
        SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /*
     * Configure MX25LW51245G CR2[0x300].
     * This is the same setting used by the previously working
     * ospi_cache implementation.
     */
    g_ospi_startup_stage = 4U;

    transfer.command        = OSPI_STARTUP_CMD_WRITE_CR2;
    transfer.address        = OSPI_STARTUP_CR2_ADDRESS;
    transfer.data           = OSPI_STARTUP_CR2_VALUE;
    transfer.command_length = 1U;
    transfer.address_length = 4U;
    transfer.data_length    = 1U;
    transfer.dummy_cycles   = 0U;

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /*
     * Read CR2 back.
     */
    g_ospi_startup_stage = 5U;

    transfer.command        = OSPI_STARTUP_CMD_READ_CR2;
    transfer.address        = OSPI_STARTUP_CR2_ADDRESS;
    transfer.data           = 0U;
    transfer.command_length = 1U;
    transfer.address_length = 4U;
    transfer.data_length    = 1U;
    transfer.dummy_cycles   = 0U;

    err = R_OSPI_B_DirectTransfer(
        &g_ospi_ctrl,
        &transfer,
        SPI_FLASH_DIRECT_TRANSFER_DIR_READ);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    if (OSPI_STARTUP_CR2_VALUE != (uint8_t) transfer.data)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    g_ospi_startup_stage = 6U;

    return FSP_SUCCESS;
}

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);

FSP_CPP_FOOTER

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart (bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

#if BSP_CFG_OSPI_B_STARTUP_ENABLED && defined(BSP_CFG_OSPI_B_STARTUP_FN)
    if (BSP_WARM_START_POST_CLOCK == event)
    {
        /* Setup OSPI_B SiP flash and initialize it. */
        R_BSP_OspiBInit(BSP_CFG_OSPI_B_STARTUP_FN, true);
    }
#endif

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /*
        * 1. Configure physical pins first.
        */
        R_IOPORT_Open(&IOPORT_CFG_CTRL, &IOPORT_CFG_NAME);

    #if BSP_CFG_SDRAM_ENABLED

        /*
        * 2. Initialize SDRAM before external C runtime data is copied.
        */
        R_BSP_SdramInit(true);
    #endif

        /*
        * 3. Initialize OSPI0 CS1 before SystemRuntimeInit(1)
        *    reads from 0x90000000.
        */
        g_ospi_startup_result = ospi_startup_init();

        if (FSP_SUCCESS != g_ospi_startup_result)
        {
            /*
            * Stop here instead of allowing the later memcpy to BusFault.
            *
            * Inspect:
            *   g_ospi_startup_stage
            *   g_ospi_startup_result
            */
            while (1)
            {
                __NOP();
            }
        }
    }
}
