#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "types.h"

#define VGA_BUFFER      0xB8000
#define VGA_WIDTH       80
#define VGA_HEIGHT      25

typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

void console_init(void);
void console_clear(void);
void console_putchar(char c);
void console_puts(const char *str);
void console_put_hex(uint64_t value);
void console_put_dec(uint64_t value);
void console_set_color(vga_color_t fg, vga_color_t bg);

void console_get_cursor(uint32_t *row, uint32_t *col);
void console_set_cursor(uint32_t row, uint32_t col);
void console_putchar_at(char c, uint32_t row, uint32_t col, uint8_t color);

/* Funciones de scroll */
void console_scroll_up(void);
void console_scroll_down(void);
uint32_t console_get_scroll_offset(void);
void console_set_scroll_offset(uint32_t offset);

#endif