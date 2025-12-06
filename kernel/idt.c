#include "../include/types.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/io.h"
#include "../include/console.h"
#include "../include/proc.h"
#include "../include/sched.h"
#include "../include/tss.h"
#include "../include/keyboard.h"
#include "../include/mm.h"

#define IDT_ENTRIES 256
#define KERNEL_CS 0x08

static idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(16)));
static idt_register_t idtr;

/* Declaraciones externas de los stubs de ISR (en isr.asm) */
extern void isr_stub_0(void);
extern void irq_stub_0(void);
extern void irq_stub_1(void);

// Aliases para compatibilidad
#define isr_stub_32 irq_stub_0
#define isr_stub_33 irq_stub_1

void idt_set_gate(uint8_t num, uint64_t handler, uint8_t type_attr, uint8_t ist) {
    idt[num].offset_low   = (uint16_t)(handler & 0xFFFF);
    idt[num].selector     = KERNEL_CS;
    idt[num].ist          = ist;
    idt[num].type_attr    = type_attr;
    idt[num].offset_mid   = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high  = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].reserved     = 0;
}

void idt_init(void) {
    console_puts("[IDT] Initializing IDT...\n");

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint64_t)&idt;

    // Inicializar todas las entradas con un handler genérico
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_0, IDT_INTERRUPT_GATE, 0);
    }

    // Configurar IRQs (Ejemplo: IRQ 0 = Timer, IRQ 1 = Keyboard)
    idt_set_gate(32, (uint64_t)isr_stub_32, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(33, (uint64_t)isr_stub_33, IDT_INTERRUPT_GATE, 0);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    console_puts("[IDT] IDT loaded.\n");
}

void exception_handler(interrupt_frame_t *frame) {
    /* Page Fault (excepción 14) - Lazy loading de páginas */
    if (frame->int_no == 14) {
        uint64_t fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        
        /* Verificar si es un page fault válido (página no presente) */
        if (frame->error_code & 0x01) {
            /* Página presente pero con error de permisos */
            console_puts("[PF] Permission fault at ");
            console_put_hex(fault_addr);
            console_puts("\n");
            /* Por ahora, simplemente matamos el proceso */
            if (current_process) {
                current_process->state = PROC_STATE_ZOMBIE;
            }
            return;
        }
        
        /* Intentar mapear la página (lazy loading) */
        pcb_t *proc = proc_current();
        if (proc) {
            /* Verificar que la dirección está en espacio de usuario */
            if (fault_addr < 0xFFFF800000000000ULL) {
                /* Asignar página física */
                phys_addr_t phys = pmm_alloc_page();
                if (phys) {
                    /* Mapear página */
                    uint64_t *pml4 = mm_get_kernel_pml4();
                    uint64_t page_flags = (1ULL << 0) | (1ULL << 1) | (1ULL << 2); /* PRESENT | WRITABLE | USER */
                    mm_map_page(pml4, fault_addr & ~0xFFFULL, phys, page_flags);
                    return; /* Página mapeada, continuar */
                }
            }
        }
        
        console_puts("[PF] Unhandled page fault at ");
        console_put_hex(fault_addr);
        console_puts("\n");
        if (current_process) {
            current_process->state = PROC_STATE_ZOMBIE;
        }
        return;
    }
    
    /* Otras excepciones */
    console_puts("\n!!! EXCEPTION ");
    console_put_dec(frame->int_no);
    console_puts(" !!!\n");
    console_puts("Error code: ");
    console_put_hex(frame->error_code);
    console_puts("\nRIP: ");
    console_put_hex(frame->rip);
    console_puts("\n");
    
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* Manejador de interrupciones que puede cambiar de contexto */
uint64_t irq_handler_with_switch(uint64_t current_rsp) {
    // Extraer el número de IRQ del stack frame
    uint64_t *stack = (uint64_t *)current_rsp;
    // El stack tiene: r15-rax (15 registros), luego error_code, int_no
    uint64_t int_no = stack[15];  // int_no está después de los 15 registros
    uint64_t irq = int_no - 32;

    switch (irq) {
        case 0: { /* Timer (IRQ 0) - Scheduling */
            if (current_process) {
                // Guardar el RSP del proceso actual
                current_process->rsp = current_rsp;
                current_process->time_slice--;
                
                // Si el time slice ha terminado, cambiar de proceso
                if (current_process->time_slice == 0) {
                    // Buscar el siguiente proceso en la lista circular
                    pcb_t *next = current_process->next;
                    
                    // Buscar un proceso READY (no WAITING ni ZOMBIE)
                    while (next && next != current_process && 
                           (next->state != PROC_STATE_READY && 
                            next->state != PROC_STATE_RUNNING)) {
                        next = next->next;
                    }

                    if (next && next != current_process &&
                        (next->state == PROC_STATE_READY ||
                         next->state == PROC_STATE_RUNNING)) {
                        
                        if (current_process->state == PROC_STATE_RUNNING)
                            current_process->state = PROC_STATE_READY;

                        next->state = PROC_STATE_RUNNING;
                        next->time_slice = DEFAULT_TIME_SLICE;

                        tss_set_rsp0(next->kernel_stack + KSTACK_SIZE);
                        
                        current_process = next;
                        
                        // Cambiar CR3 si es necesario
                        if (next->cr3 != 0) {
                            __asm__ volatile("mov %0, %%cr3" : : "r"(next->cr3) : "memory");
                        }
                        
                        // Retornar el nuevo RSP
                        pic_send_eoi(irq);
                        return next->rsp;
                    } else {
                        current_process->time_slice = DEFAULT_TIME_SLICE;
                    }
                }
            }
        } break;
        case 1: { /* Keyboard */
            uint8_t scancode = inb(0x60);
            keyboard_handle_scancode(scancode);
        } break;
        default: {
            console_puts("Unhandled IRQ: ");
            console_put_dec(irq);
            console_puts("\n");
        } break;
    }

    pic_send_eoi(irq);
    return current_rsp;  // Retornar el mismo RSP si no hay cambio de contexto
}