#ifndef SHARED_IPC_PROTOCOL_H
#define SHARED_IPC_PROTOCOL_H

#include <stdint.h>

/*
 * EK-RA8P1 SDRAM:
 *   0x68000000 - 0x6FFFFFFF
 *
 * Reserve the last 64 bytes for the dual-core ownership test.
 */
#define SHARED_SDRAM_MAILBOX_ADDR      (0x6FFFFFC0UL)

#define SHARED_SDRAM_MAGIC             (0x5344524DUL)

#define SHARED_OWNER_CPU0              (0U)
#define SHARED_OWNER_CPU1              (1U)

#define SHARED_TEST_SLOT               (2U)
#define SHARED_TEST_REQUEST_VALUE      (0x13579BDFUL)
#define SHARED_TEST_RESPONSE_VALUE     (0x2468ACE0UL)

typedef struct
{
    uint32_t magic;
    uint32_t sequence;
    uint32_t owner;
    uint32_t slot;

    uint32_t request_value;
    uint32_t response_value;

    uint32_t reserved[10];
} shared_sdram_mailbox_t;

#define SHARED_SDRAM_MAILBOX \
    ((volatile shared_sdram_mailbox_t *) SHARED_SDRAM_MAILBOX_ADDR)

#endif