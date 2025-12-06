#ifndef _IDT_H
#define _IDT_H

#include "types.h"

#define IDT_ENTRIES         256

#define IDT_INTERRUPT_GATE  0x8E
#define IDT_TRAP_GATE       0x8F

#define IRQ_BASE            32
#define IRQ_TIMER           (IRQ_BASE + 0)
#define IRQ_KEYBOARD        (IRQ_BASE + 1)

typedef struct PACKED {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

typedef struct PACKED {
    uint16_t limit;
    uint64_t base;
} idt_register_t;

typedef struct PACKED {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr, uint8_t ist);

void exception_handler(interrupt_frame_t *frame);
uint64_t irq_handler_with_switch(uint64_t current_rsp);

uint64_t get_timer_ticks(void);
uint64_t get_context_switches(void);

#endif