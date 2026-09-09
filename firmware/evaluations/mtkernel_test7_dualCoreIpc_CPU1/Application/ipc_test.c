#include <tk/tkernel.h>
#include "hal_data.h"
#include "ipc_test.h"
#include "shared_ipc_protocol.h"

#define IPC_TEST_REQUEST   (0x12345678UL)
#define IPC_TEST_ACK       (0xA5A5A5A5UL)

volatile uint32_t g_cpu1_ipc_rx_value = 0U;
volatile uint32_t g_cpu1_ipc_rx_count = 0U;
volatile uint32_t g_cpu1_ipc_ack_count = 0U;

volatile fsp_err_t g_cpu1_ipc_init_err = FSP_SUCCESS;
volatile fsp_err_t g_cpu1_ipc_send_err = FSP_SUCCESS;

static volatile uint32_t s_ack_pending = 0U;

volatile uint32_t g_cpu1_shared_request = 0U;
volatile uint32_t g_cpu1_shared_owner = 0U;
volatile uint32_t g_cpu1_shared_sequence = 0U;
volatile uint32_t g_cpu1_shared_pass = 0U;

static void ipc_test_task(INT stacd, void *exinf)
{
    FSP_PARAMETER_NOT_USED(stacd);
    FSP_PARAMETER_NOT_USED(exinf);

    while (1)
    {
        if (0U != s_ack_pending)
        {
            volatile shared_sdram_mailbox_t * const p_shared =
                SHARED_SDRAM_MAILBOX;

            s_ack_pending = 0U;

            __DMB();

            g_cpu1_shared_request  = p_shared->request_value;
            g_cpu1_shared_owner    = p_shared->owner;
            g_cpu1_shared_sequence = p_shared->sequence;

            if ((SHARED_SDRAM_MAGIC == p_shared->magic) &&
                (SHARED_OWNER_CPU1 == p_shared->owner) &&
                (1U == p_shared->sequence) &&
                (SHARED_TEST_SLOT == p_shared->slot) &&
                (SHARED_TEST_REQUEST_VALUE == p_shared->request_value))
            {
                g_cpu1_shared_pass = 1U;

                p_shared->response_value = SHARED_TEST_RESPONSE_VALUE;
                p_shared->sequence       = 2U;

                /*
                * Complete all writes before returning ownership to CPU0.
                */
                __DMB();

                p_shared->owner = SHARED_OWNER_CPU0;

                __DMB();

                g_cpu1_ipc_send_err =
                    R_IPC_MessageSend(&g_ipc0_ctrl, IPC_TEST_ACK);

                if (FSP_SUCCESS == g_cpu1_ipc_send_err)
                {
                    g_cpu1_ipc_ack_count++;
                }
            }
        }

        tk_dly_tsk(10);
    }
}

void ipc1_callback(ipc_callback_args_t *p_args)
{
    if (IPC_EVENT_MESSAGE_RECEIVED == p_args->event)
    {
        g_cpu1_ipc_rx_value = p_args->message;
        g_cpu1_ipc_rx_count++;

        if (IPC_TEST_REQUEST == p_args->message)
        {
            s_ack_pending = 1U;
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

    g_cpu1_ipc_init_err =
        R_IPC_Open(g_ipc0.p_ctrl, g_ipc0.p_cfg);

    if (FSP_SUCCESS != g_cpu1_ipc_init_err)
    {
        return;
    }

    g_cpu1_ipc_init_err =
        R_IPC_Open(g_ipc1.p_ctrl, g_ipc1.p_cfg);

    if (FSP_SUCCESS != g_cpu1_ipc_init_err)
    {
        return;
    }

    task_id = tk_cre_tsk(&ctsk);

    if (task_id > 0)
    {
        tk_sta_tsk(task_id, 0);
    }
}