#include "../include/pic.h"
#include "../include/io.h"
#include "../include/console.h"

void pic_init(void) {
    console_puts("[PIC] Initializing and remapping...\n");

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, 0x20); /* IRQ0-7 -> INT 32-39 */
    outb(PIC2_DATA, 0x28); /* IRQ8-15 -> INT 40-47 */

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    console_puts("      Master: 0x20-0x27, Slave: 0x28-0x2F\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port);
    mask &= ~(1 << irq);
    outb(port, mask);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    mask = inb(port);
    mask |= (1 << irq);
    outb(port, mask);
}