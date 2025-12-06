#include "../include/tss.h"
#include "../include/console.h"

/* TSS global */
static tss_t tss __attribute__((aligned(16)));

extern uint8_t gdt64[];
extern uint8_t kernel_stack_top[];

#define TSS_SELECTOR 0x28

typedef struct PACKED {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} tss_descriptor_t;

void tss_init(void) {
    console_puts("[TSS] Initializing TSS...\n");

    /* Limpiar TSS */
    uint8_t *p = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(tss); i++) {
        p[i] = 0;
    }

    tss.rsp0 = (uint64_t)kernel_stack_top;
    tss.ist1 = (uint64_t)kernel_stack_top; /* Simplemente apuntamos al stack principal */

    tss.iopb_offset = sizeof(tss_t);

    /* gdt64 layout:
     * 0x00 null
     * 0x08 kernel code
     * 0x10 kernel data
     * 0x18 user data     <-- 0x1B (USER_DATA_SEL)
     * 0x20 user code     <-- 0x23 (USER_CODE_SEL)
     * 0x28 TSS (16 bytes)
     */
    tss_descriptor_t *desc = (tss_descriptor_t *)&gdt64[5 * 8];

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(tss) - 1);

    desc->limit_low        = (uint16_t)(limit & 0xFFFF);
    desc->base_low         = (uint16_t)(base & 0xFFFF);
    desc->base_mid         = (uint8_t)((base >> 16) & 0xFF);
    desc->access           = 0x89; /* Present, type=0x9 (Available 64-bit TSS), DPL=0 */
    desc->limit_high_flags = (uint8_t)((limit >> 16) & 0x0F) | 0x80; /* Granularity = 1 (4KB) */
    desc->base_mid2        = (uint8_t)((base >> 24) & 0xFF);
    desc->base_high        = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    desc->reserved         = 0;

    /* Cargar el selector de TSS */
    __asm__ volatile("ltr %w0" : : "r" (TSS_SELECTOR));

    console_puts("[TSS] TSS loaded and GDT descriptor configured.\n");
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}