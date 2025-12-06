#include "../include/sched.h"
#include "../include/proc.h"
#include "../include/tss.h"
#include "../include/console.h"

void sched_init(void) {
    console_puts("[SCHED] Initializing Round-Robin scheduler...\n");
}

void sched_add_process(pcb_t *proc) {
    if (!proc || proc->state != PROC_STATE_READY) return;
    proc->time_slice = DEFAULT_TIME_SLICE;
    if (!process_list_head) {
        process_list_head = proc;
        proc->next = proc;
    } else {
        pcb_t *tail = process_list_head;
        while (tail->next != process_list_head) tail = tail->next;
        tail->next = proc;
        proc->next = process_list_head;
    }
}

void schedule(void) {
    /* Si no hay proceso actual (primer arranque) */
    if (!current_process) {
        if (!process_list_head) return; /* No hay procesos */
        
        pcb_t *next_process = process_list_head;
        next_process->state = PROC_STATE_RUNNING;
        next_process->time_slice = DEFAULT_TIME_SLICE;
        
        tss_set_rsp0(next_process->kernel_stack + KSTACK_SIZE);
        
        /* En el primer switch, no hay "old_rsp" que guardar, pero
           la función lo espera. Pasamos un puntero dummy. */
        uint64_t dummy;
        context_switch(&dummy, next_process->rsp, next_process->cr3);
        
        /* Cuando retorne, ya estamos en el nuevo proceso */
        current_process = next_process;
        return;
    }

    if (current_process->time_slice > 0) {
        current_process->time_slice--;
        return;
    }

    pcb_t *old_process = current_process;
    pcb_t *next_process = old_process->next;
    
    while (next_process->state != PROC_STATE_READY &&
           next_process != old_process) {
        next_process = next_process->next;
    }

    if (next_process == old_process) {
        old_process->time_slice = DEFAULT_TIME_SLICE;
        return;
    }

    if (old_process->state == PROC_STATE_RUNNING) {
        old_process->state = PROC_STATE_READY;
    }
    next_process->state = PROC_STATE_RUNNING;
    next_process->time_slice = DEFAULT_TIME_SLICE;

    tss_set_rsp0(next_process->kernel_stack + KSTACK_SIZE);
    
    context_switch(&old_process->rsp, next_process->rsp, next_process->cr3);
    
    current_process = next_process;
}