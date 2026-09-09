#include <tk/tkernel.h>
#include "hal_data.h"
#include "ipc_test.h"
#include "shared_ipc_protocol.h"

#define IPC_TEST_REQUEST   (0x12345678UL)
#define IPC_TEST_ACK       (0xA5A5A5A5UL)

volatile uint32_t g_cpu0_ipc_ack_value = 0U;
volatile uint32_t g_cpu0_ipc_ack_count = 0U;
volatile fsp_err_t g_cpu0_ipc_init_err = FSP_SUCCESS;
volatile fsp_err_t g_cpu0_ipc_send_err = FSP_SUCCESS;

volatile uint32_t g_cpu0_shared_response = 0U;
volatile uint32_t g_cpu0_shared_owner = 0U;
volatile uint32_t g_cpu0_shared_sequence = 0U;
volatile uint32_t g_cpu0_shared_pass = 0U;

static volatile uint32_t s_ack_received = 0U;

static void ipc_test_task(INT stacd, void *exinf)
{
    FSP_PARAMETER_NOT_USED(stacd);
    FSP_PARAMETER_NOT_USED(exinf);

    /*
     * CPU1 is started before CPU0 enters μT-Kernel.
     * Give CPU1 enough time to initialize its IPC instances.
     */
    tk_dly_tsk(500);

    g_cpu0_ipc_send_err =
        R_IPC_MessageSend(&g_ipc1_ctrl, IPC_TEST_REQUEST);

    volatile shared_sdram_mailbox_t * const p_shared =
        SHARED_SDRAM_MAILBOX;

    p_shared->owner          = SHARED_OWNER_CPU0;
    p_shared->magic          = SHARED_SDRAM_MAGIC;
    p_shared->sequence       = 1U;
    p_shared->slot           = SHARED_TEST_SLOT;
    p_shared->request_value  = SHARED_TEST_REQUEST_VALUE;
    p_shared->response_value = 0U;

    /*
    * Publish all payload fields before transferring ownership.
    */
    __DMB();

    p_shared->owner = SHARED_OWNER_CPU1;

    __DMB();

    while (1)
    {
        if (0U != s_ack_received)
        {
            s_ack_received = 0U;

            __DMB();

            g_cpu0_shared_response = p_shared->response_value;
            g_cpu0_shared_owner    = p_shared->owner;
            g_cpu0_shared_sequence = p_shared->sequence;

            if ((SHARED_TEST_RESPONSE_VALUE == p_shared->response_value) &&
                (SHARED_OWNER_CPU0 == p_shared->owner) &&
                (2U == p_shared->sequence))
            {
                g_cpu0_shared_pass = 1U;
            }
        }

        tk_dly_tsk(10);
    }
}

void ipc0_callback(ipc_callback_args_t *p_args)
{
    if (IPC_EVENT_MESSAGE_RECEIVED == p_args->event)
    {
        g_cpu0_ipc_ack_value = p_args->message;
        g_cpu0_ipc_ack_count++;

        if (IPC_TEST_ACK == p_args->message)
        {
            s_ack_received = 1U;
        }
    }
}

void ipc_test_start(void)
{
    ID task_id;

    T_CTSK ctsk =
    {
        .tskatr  = TA_HLNG,
        .task    = ipc_test_task,
        .itskpri = 20,
        .stksz   = 1024
    };

    g_cpu0_ipc_init_err =
        R_IPC_Open(g_ipc0.p_ctrl, g_ipc0.p_cfg);

    if (FSP_SUCCESS != g_cpu0_ipc_init_err)
    {
        return;
    }

    g_cpu0_ipc_init_err =
        R_IPC_Open(g_ipc1.p_ctrl, g_ipc1.p_cfg);

    if (FSP_SUCCESS != g_cpu0_ipc_init_err)
    {
        return;
    }

    task_id = tk_cre_tsk(&ctsk);

    if (task_id > 0)
    {
        tk_sta_tsk(task_id, 0);
    }
}