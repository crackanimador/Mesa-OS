#include "../include/types.h"
#include "../include/console.h"

/* En esta fase, seguimos llamando a sys_write directamente como función C */
extern int64_t sys_write(int fd, const char *buf, size_t count);

#define VGA_BUFFER 0xB8000

static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

void user_program_1(void) {
    volatile uint16_t *vga = (volatile uint16_t *)VGA_BUFFER;
    uint64_t counter = 0;
    int base = 18 * 80;

    const char *msg = "[P1] Started\n";
    sys_write(1, msg, kstrlen(msg));

    while (1) {
        const char *label = "Process 1: ";
        for (int i = 0; label[i]; i++) {
            vga[base + i] = (uint16_t)label[i] | 0x0A00;
        }

        const char *hex = "0123456789ABCDEF";
        for (int i = 0; i < 16; i++) {
            int shift = (15 - i) * 4;
            char c = hex[(counter >> shift) & 0xF];
            vga[base + 11 + i] = (uint16_t)c | 0x0F00;
        }

        const char spin[] = "|/-\\";
        vga[base + 30] = (uint16_t)spin[counter & 3] | 0x0E00;

        counter++;
        for (volatile int d = 0; d < 50000; d++);
    }
}

void user_program_2(void) {
    volatile uint16_t *vga = (volatile uint16_t *)VGA_BUFFER;
    uint64_t counter = 0;
    int base = 19 * 80;

    const char *msg = "[P2] Started\n";
    sys_write(1, msg, kstrlen(msg));

    while (1) {
        const char *label = "Process 2: ";
        for (int i = 0; label[i]; i++) {
            vga[base + i] = (uint16_t)label[i] | 0x0C00;
        }

        const char *hex = "0123456789ABCDEF";
        for (int i = 0; i < 16; i++) {
            int shift = (15 - i) * 4;
            char c = hex[(counter >> shift) & 0xF];
            vga[base + 11 + i] = (uint16_t)c | 0x0F00;
        }

        const char spin[] = "+-*/";
        vga[base + 30] = (uint16_t)spin[counter & 3] | 0x0B00;

        counter++;
        for (volatile int d = 0; d < 50000; d++);
    }
}