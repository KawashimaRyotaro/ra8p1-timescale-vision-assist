#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H


#include <tk/tkernel.h>

/*
 * Create the display task.
 *
 * Return:
 *   E_OK       : success
 *   < E_OK     : μT-Kernel error code
 */
ER display_task_create(void);


/*
 * Start the display task.
 *
 * Return:
 *   E_OK       : success
 *   < E_OK     : μT-Kernel error code
 */
ER display_task_start(void);


#endif /* DISPLAY_TASK_H */