#ifndef _SCHED_H
#define _SCHED_H

#include "types.h"
#include "proc.h"

#define DEFAULT_TIME_SLICE  10

void sched_init(void);
void sched_add_process(pcb_t *proc);
void schedule(void); /* ahora mismo vacío, scheduling ocurre en IRQ */

#endif