#include <tk/tkernel.h>
#include "ipc_test.h"

volatile UW g_cpu1_heartbeat = 0U;

EXPORT INT usermain(void)
{
    ipc_test_start();

    while (1)
    {
        g_cpu1_heartbeat++;

        tk_dly_tsk(100);
    }

    return 0;
}