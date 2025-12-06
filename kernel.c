/**
 * @file kernel.c
 * @brief Kernel principal - Multitarea en Ring 0
 */

#include "include/types.h"
#include "include/console.h"
#include "include/mm.h"
#include "include/pic.h"
#include "include/tss.h"
#include "include/idt.h"
#include "include/syscall.h"
#include "include/vfs.h"
#include "include/keyboard.h"
#include "include/proc.h"
#include "include/sched.h"
#include "include/disk.h"
#include "include/partition.h"
#include "include/ipc.h"
#include "include/mesafs.h"

extern void user_program_1(void);
extern void user_program_2(void);
extern void ipc_server(void);
extern void ipc_client(void);
extern void shell_main(void);

NORETURN void kernel_panic(const char *file, int line, const char *msg) {
    __asm__ volatile ("cli");
    console_set_color(VGA_WHITE, VGA_RED);
    console_puts("\n\n!!! KERNEL PANIC !!!\n");
    console_set_color(VGA_LIGHT_RED, VGA_BLACK);
    console_puts(file);
    console_puts(":");
    console_put_dec(line);
    console_puts(": ");
    console_puts(msg);
    console_puts("\nSystem halted.\n");
    while (1) __asm__ volatile ("hlt");
}

void kernel_main(void *mb_info) {
    (void)mb_info;

    console_init();
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    console_puts("\n=== MESA-OS Microkernel x86_64 ===\n\n");
    console_set_color(VGA_WHITE, VGA_BLACK);

    /* Inicialización de módulos */
    mm_init((uint64_t)mb_info);
    pic_init();
    tss_init();
    idt_init();
    syscall_init();
    disk_init();
    partition_init();
    vfs_init();
    keyboard_init();
    proc_init();
    ipc_init();
    sched_init();

    /* Mostrar información de particiones */
    partition_print_info();
    
    /* Montar sistema de archivos MesaFS */
    partition_info_t *part = partition_get(0); /* Primera partición */
    if (part && part->type == PART_TYPE_MESAFS) {
        static mesafs_t root_fs;
        if (mesafs_mount(part->lba_start, &root_fs) != 0) {
            console_puts("[BOOT] Formatting MesaFS partition...\n");
            if (mesafs_format(part->lba_start, part->num_sectors) == 0) {
                mesafs_mount(part->lba_start, &root_fs);
            }
        }
    } else {
        console_puts("[BOOT] No MesaFS partition found\n");
    }
    
    console_puts("\n[BOOT] Creating processes...\n\n");

    /* Crear shell como proceso init (PID 1) */
    pcb_t *shell = proc_create((virt_addr_t)shell_main, NULL, 0);
    sched_add_process(shell);
    
    /* Crear procesos de prueba IPC */
    pcb_t *server = proc_create((virt_addr_t)ipc_server, NULL, 0);
    sched_add_process(server);
    
    pcb_t *client = proc_create((virt_addr_t)ipc_client, NULL, 0);
    sched_add_process(client);
    
    /* Procesos de demostración originales (opcionales) */
    pcb_t *p1 = proc_create((virt_addr_t)user_program_1, NULL, 0);
    sched_add_process(p1);

    pcb_t *p2 = proc_create((virt_addr_t)user_program_2, NULL, 0);
    sched_add_process(p2);

    console_puts("\n[BOOT] Starting scheduler via IRQ0...\n\n");

    /* Habilitar timer y teclado */
    pic_unmask_irq(0);
    pic_unmask_irq(1);

    /* Habilitar interrupciones */
    __asm__ volatile ("sti");

    /* Entrar en un loop idle con spinner visible */
    console_puts("=== Kernel idle loop (scheduler active) ===\n");
    
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    uint64_t idle_counter = 0;
    const char spinner[] = "|/-\\";
    int spinner_pos = 17 * 80;  /* Línea 17 para el kernel idle */
    
    /* Mostrar etiqueta del kernel idle */
    const char *idle_label = "Kernel Idle: ";
    for (int i = 0; idle_label[i]; i++) {
        vga[spinner_pos + i] = (uint16_t)idle_label[i] | 0x0700; /* Gris claro */
    }
    
    while (1) {
        /* Mostrar spinner del kernel */
        vga[spinner_pos + 13] = (uint16_t)spinner[idle_counter & 3] | 0x0F00; /* Blanco brillante */
        
        /* Contador hexadecimal */
        const char *hex = "0123456789ABCDEF";
        for (int i = 0; i < 16; i++) {
            int shift = (15 - i) * 4;
            char c = hex[(idle_counter >> shift) & 0xF];
            vga[spinner_pos + 15 + i] = (uint16_t)c | 0x0800; /* Gris oscuro */
        }
        
        idle_counter++;
        __asm__ volatile ("hlt");
    }
}