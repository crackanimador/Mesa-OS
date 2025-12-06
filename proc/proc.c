/**
 * @file proc.c
 * @brief Gestión de procesos (PCB, creación, contexto inicial) - Ring 0
 */

#include "../include/proc.h"
#include "../include/mm.h"
#include "../include/console.h"
#include "../include/types.h" 

/* -----------------------------------------------------------
 * Reemplazo de memset si <string.h> no está disponible
 * ----------------------------------------------------------- */
static void *kernel_memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}


/* ============== CONSTANTES CRÍTICAS ============== */
// Selectores de Segmento de Usuario 
#define USER_DATA_SEL   0x1B // 0x18 | 3
#define USER_CODE_SEL   0x23 // 0x20 | 3

// Dirección de la pila de usuario (Ej. 128TB - 4KB)
#define USER_STACK_ADDR 0x7FFFF0000000UL 

/* ============== VARIABLES GLOBALES ============== */

static pcb_t process_table[MAX_PROCESSES];
static uint8_t kernel_stacks[MAX_PROCESSES][KSTACK_SIZE] 
    __attribute__((aligned(16)));

// Definición de las variables globales (sin 'static' para coincidir con extern)
pcb_t *current_process   = NULL;
pcb_t *process_list_head = NULL; 

static uint32_t next_pid = 1;

/* ============== AUXILIARES ============== */

static pcb_t *alloc_pcb(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_STATE_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

static int pcb_index(pcb_t *p) {
    return (int)(p - process_table);
}

// Implementación de la función declarada en proc.h
pcb_t *proc_current(void) {
    return current_process;
}

/* Handler para cuando un proceso retorna */
static void process_exit_handler(void) {
    console_puts("\n[PROC] Process exited unexpectedly!\n");
    if (current_process) {
        current_process->state = PROC_STATE_ZOMBIE;
    }
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* ============== INICIALIZACIÓN ============== */

void proc_init(void) {
    console_puts("[PROC] Initializing process table...\n");
    kernel_memset(process_table, 0, sizeof(process_table));
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROC_STATE_UNUSED;
    }
}


/* ============== CREACIÓN DE PROCESO CORREGIDA ============== */

pcb_t *proc_create(virt_addr_t entry, const void *code, size_t code_size) {
    (void)code; (void)code_size;

    pcb_t *p = alloc_pcb();
    if (!p) {
        console_puts("[PROC] ERROR: No free PCB\n");
        return NULL;
    }

    int idx = pcb_index(p);

    console_puts("[PROC] Creating PID=");
    console_put_dec(next_pid);
    console_puts(" (slot ");
    console_put_dec(idx);
    console_puts(")\n");

    p->pid   = next_pid++;
    p->state = PROC_STATE_EMBRYO;
    p->next  = NULL;
    p->time_slice = 0;
    
    p->cr3 = mm_create_user_pml4(); 
    if (p->cr3 == 0) {
        console_puts("[PROC] ERROR: Failed to create user PML4\n");
        return NULL;
    }
    
    p->entry_point  = entry;
    // Guardamos la base del stack
    p->kernel_stack = (uint64_t)&kernel_stacks[idx][0];
    virt_addr_t stack_top = p->kernel_stack + KSTACK_SIZE;

    p->heap_start = USER_HEAP_START; 
    p->brk        = p->heap_start;


    /*
     * === CONSTRUCCIÓN DEL CONTEXTO DE KERNEL STACK ===
     */
    uint64_t *sp = (uint64_t *)stack_top;

    // 1. Marco de IRETQ (SS, RSP, RFLAGS, CS, RIP)
    *(--sp) = USER_DATA_SEL;        /* SS_user (0x1B) */
    *(--sp) = USER_STACK_ADDR;      /* RSP_user (Tope de pila de usuario) */
    *(--sp) = 0x202;                /* RFLAGS (IF=1, Interrupciones habilitadas) */
    *(--sp) = USER_CODE_SEL;        /* CS_user (0x23) */
    *(--sp) = entry;                /* RIP_user (Punto de entrada) */
    
    // 2. Registros guardados por context_switch (R15 a RBP)
    *(--sp) = 0; // R15
    *(--sp) = 0; // R14
    *(--sp) = 0; // R13
    *(--sp) = 0; // R12
    *(--sp) = 0; // RBX
    *(--sp) = 0; // RBP

    // 3. RIP de retorno para el 'ret' de context_switch
    extern void usermode_entry(void); 
    *(--sp) = (uint64_t)usermode_entry; 

    
    p->rsp = (uint64_t)sp; // El PCB guarda el puntero al inicio del contexto.

    console_puts("       entry:  ");
    console_put_hex(p->entry_point);
    console_puts("\n       kstack: ");
    console_put_hex(p->kernel_stack);
    console_puts("\n       Initial RSP: ");
    console_put_hex(p->rsp);
    console_puts("\n");

    p->state = PROC_STATE_READY;
    return p;
}