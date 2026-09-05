#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "dummy_tasks.h"


#define DUMMY_AI_PRIORITY       (20)
#define DUMMY_OUTPUT_PRIORITY   (30)

#define DUMMY_AI_STACK_SIZE     (2048)
#define DUMMY_OUTPUT_STACK_SIZE (2048)


LOCAL ID s_dummy_ai_task_id     = 0;
LOCAL ID s_dummy_output_task_id = 0;


LOCAL void dummy_ai_task_entry(
    INT stacd,
    void *exinf
);


LOCAL void dummy_output_task_entry(
    INT stacd,
    void *exinf
);


LOCAL T_CTSK s_dummy_ai_ctsk =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = dummy_ai_task_entry,
    .itskpri = DUMMY_AI_PRIORITY,
    .stksz   = DUMMY_AI_STACK_SIZE,
};


LOCAL T_CTSK s_dummy_output_ctsk =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = dummy_output_task_entry,
    .itskpri = DUMMY_OUTPUT_PRIORITY,
    .stksz   = DUMMY_OUTPUT_STACK_SIZE,
};


ER dummy_tasks_create(void)
{
    ID task_id;


    task_id = tk_cre_tsk(&s_dummy_ai_ctsk);

    if (task_id < E_OK)
    {
        return (ER) task_id;
    }

    s_dummy_ai_task_id = task_id;


    task_id = tk_cre_tsk(&s_dummy_output_ctsk);

    if (task_id < E_OK)
    {
        return (ER) task_id;
    }

    s_dummy_output_task_id = task_id;


    return E_OK;
}


ER dummy_tasks_start(void)
{
    ER err;


    if (
        (s_dummy_ai_task_id <= 0) ||
        (s_dummy_output_task_id <= 0)
    )
    {
        return E_ID;
    }


    err = tk_sta_tsk(
        s_dummy_ai_task_id,
        0
    );

    if (err < E_OK)
    {
        return err;
    }


    err = tk_sta_tsk(
        s_dummy_output_task_id,
        0
    );

    if (err < E_OK)
    {
        return err;
    }


    return E_OK;
}


LOCAL void dummy_ai_task_entry(
    INT stacd,
    void *exinf
)
{
    (void) stacd;
    (void) exinf;


    tm_putstring(
        (UB *)"[AI] dummy task started.\n"
    );


    while (1)
    {
        tm_putstring(
            (UB *)"[AI] dummy processing.\n"
        );

        tk_dly_tsk(2000);
    }
}


LOCAL void dummy_output_task_entry(
    INT stacd,
    void *exinf
)
{
    (void) stacd;
    (void) exinf;


    tm_putstring(
        (UB *)"[Output] dummy task started.\n"
    );


    while (1)
    {
        tm_putstring(
            (UB *)"[Output] dummy processing.\n"
        );

        tk_dly_tsk(3000);
    }
}