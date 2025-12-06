/**
 * @file proc.h
 * @brief Declaraciones para la gestión de procesos.
 */

#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "mm.h" // Necesario para virt_addr_t

#define MAX_PROCESSES 64
#define KSTACK_SIZE 8192 // 8 KB
#define USER_STACK_SIZE (4 * 1024 * 1024) // 4 MB de pila de usuario
#define USER_HEAP_START 0x400000UL 

typedef enum {
    PROC_STATE_UNUSED,
    PROC_STATE_EMBRYO,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_WAITING,  /* Esperando mensaje IPC */
    PROC_STATE_ZOMBIE,
} proc_state_t;

// Estructura de Bloque de Control de Proceso (PCB)
typedef struct pcb {
    uint32_t pid;
    proc_state_t state;
    uint64_t rsp;          // Puntero al stack de kernel (donde se guarda el contexto)
    uint64_t cr3;          // PML4 física (Tabla de Páginas)
    uint64_t kernel_stack; // Base del stack de kernel
    virt_addr_t entry_point;
    uint64_t heap_start;   
    uint64_t brk;          
    uint64_t time_slice;   

    struct pcb *next;      
} pcb_t;

/* Variables Globales Exportadas */
extern pcb_t *current_process; 
// [FIX] Exportar la lista de procesos para que idt.c y sched.c puedan usarla
extern pcb_t *process_list_head; 

/* Funciones de Gestión de Procesos */
void proc_init(void);
pcb_t *proc_create(virt_addr_t entry, const void *code, size_t code_size);
pcb_t *proc_current(void); 

void context_switch(uint64_t *old_rsp, uint64_t new_rsp, uint64_t new_cr3);

#endif // _PROC_H_