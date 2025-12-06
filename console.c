#include "../include/console.h"
#include "../include/io.h"

static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_BUFFER;
static uint32_t cursor_row = 0;
static uint32_t cursor_col = 0;
static uint32_t scroll_offset = 0;  /* Offset de scroll (líneas desplazadas hacia arriba) */
static uint8_t current_color = 0x0F;

/* Buffer de historial de pantalla (para scroll) */
#define CONSOLE_HISTORY_LINES 100
static uint16_t screen_history[CONSOLE_HISTORY_LINES * VGA_WIDTH];
static uint32_t history_start = 0;  /* Índice de la primera línea en el historial */
static uint32_t history_count = 0; /* Número de líneas en el historial */

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint32_t vga_offset(uint32_t row, uint32_t col) {
    return row * VGA_WIDTH + col;
}

static void update_cursor(void) {
    uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void console_scroll(void) {
    /* Guardar línea que se va a perder en el historial */
    if (history_count < CONSOLE_HISTORY_LINES) {
        uint32_t hist_idx = (history_start + history_count) % CONSOLE_HISTORY_LINES;
        for (uint32_t col = 0; col < VGA_WIDTH; col++) {
            screen_history[hist_idx * VGA_WIDTH + col] = 
                vga_buffer[vga_offset(0, col)];
        }
        history_count++;
    } else {
        /* Historial lleno, sobrescribir la más antigua */
        for (uint32_t col = 0; col < VGA_WIDTH; col++) {
            screen_history[history_start * VGA_WIDTH + col] = 
                vga_buffer[vga_offset(0, col)];
        }
        history_start = (history_start + 1) % CONSOLE_HISTORY_LINES;
    }
    
    /* Scroll visual */
    for (uint32_t row = 0; row < VGA_HEIGHT - 1; row++) {
        for (uint32_t col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[vga_offset(row, col)] =
                vga_buffer[vga_offset(row + 1, col)];
        }
    }
    for (uint32_t col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[vga_offset(VGA_HEIGHT - 1, col)] = vga_entry(' ', current_color);
    }
}

/* Redibujar pantalla desde el historial según scroll_offset */
static void console_redraw_from_history(void) {
    /* Calcular qué líneas mostrar */
    /* scroll_offset = 0 significa mostrar las líneas más recientes */
    /* scroll_offset aumenta cuando scrolleamos hacia arriba (líneas más antiguas) */
    
    for (uint32_t row = 0; row < VGA_HEIGHT; row++) {
        /* Calcular qué línea del historial mostrar en esta fila de pantalla */
        /* La última línea visible debe ser la más reciente */
        int32_t hist_line_idx = (int32_t)history_count - (int32_t)scroll_offset - (VGA_HEIGHT - 1 - row);
        
        if (hist_line_idx >= 0 && hist_line_idx < (int32_t)history_count) {
            /* Mostrar desde historial */
            uint32_t hist_idx = (history_start + (uint32_t)hist_line_idx) % CONSOLE_HISTORY_LINES;
            for (uint32_t col = 0; col < VGA_WIDTH; col++) {
                vga_buffer[vga_offset(row, col)] = screen_history[hist_idx * VGA_WIDTH + col];
            }
        } else if (hist_line_idx < 0) {
            /* Líneas antes del historial - mostrar espacios */
            for (uint32_t col = 0; col < VGA_WIDTH; col++) {
                vga_buffer[vga_offset(row, col)] = vga_entry(' ', current_color);
            }
        } else {
            /* Líneas después del historial - mostrar desde buffer VGA actual */
            /* Esto ocurre cuando scroll_offset es negativo o muy grande */
            int32_t vga_src_row = (int32_t)row + (hist_line_idx - (int32_t)history_count);
            if (vga_src_row >= 0 && vga_src_row < (int32_t)VGA_HEIGHT) {
                for (uint32_t col = 0; col < VGA_WIDTH; col++) {
                    vga_buffer[vga_offset(row, col)] = 
                        vga_buffer[vga_offset((uint32_t)vga_src_row, col)];
                }
            } else {
                for (uint32_t col = 0; col < VGA_WIDTH; col++) {
                    vga_buffer[vga_offset(row, col)] = vga_entry(' ', current_color);
                }
            }
        }
    }
}

void console_init(void) {
    current_color = (VGA_WHITE) | (VGA_BLACK << 4);
    scroll_offset = 0;
    history_start = 0;
    history_count = 0;
    console_clear();
    update_cursor();
}

void console_clear(void) {
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', current_color);
    }
    cursor_row = 0;
    cursor_col = 0;
    update_cursor();
}

void console_putchar(char c) {
    switch (c) {
        case '\n':
            cursor_col = 0;
            cursor_row++;
            break;
        case '\r':
            cursor_col = 0;
            break;
        case '\t':
            cursor_col = (cursor_col + 8) & ~7;
            if (cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
        case '\b':
            if (cursor_col > 0) {
                cursor_col--;
            } else if (cursor_row > 0) {
                cursor_row--;
                cursor_col = VGA_WIDTH - 1;
            }
            break;
        default:
            if (c >= ' ' || (uint8_t)c >= 0x80) {
                vga_buffer[vga_offset(cursor_row, cursor_col)] =
                    vga_entry(c, current_color);
                cursor_col++;
            }
            break;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }

    while (cursor_row >= VGA_HEIGHT) {
        console_scroll();
        cursor_row = VGA_HEIGHT - 1; /* Mantener cursor en última línea */
    }

    update_cursor();
}

void console_puts(const char *str) {
    while (*str) {
        console_putchar(*str++);
    }
}

void console_put_hex(uint64_t value) {
    const char *hex = "0123456789ABCDEF";
    console_puts("0x");
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t d = (value >> i) & 0xF;
        if (d || started || i == 0) {
            console_putchar(hex[d]);
            started = 1;
        }
    }
}

void console_put_dec(uint64_t value) {
    if (value == 0) {
        console_putchar('0');
        return;
    }
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }
    console_puts(&buf[i]);
}

void console_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = (fg & 0x0F) | ((bg & 0x0F) << 4);
}

void console_get_cursor(uint32_t *row, uint32_t *col) {
    if (row) *row = cursor_row;
    if (col) *col = cursor_col;
}

void console_set_cursor(uint32_t row, uint32_t col) {
    if (row < VGA_HEIGHT) cursor_row = row;
    if (col < VGA_WIDTH)  cursor_col = col;
    update_cursor();
}

void console_putchar_at(char c, uint32_t row, uint32_t col, uint8_t color) {
    if (row < VGA_HEIGHT && col < VGA_WIDTH) {
        vga_buffer[vga_offset(row, col)] = vga_entry(c, color);
    }
}

/* Funciones de scroll */
void console_scroll_up(void) {
    if (scroll_offset < history_count) {
        scroll_offset++;
        console_redraw_from_history();
        update_cursor();
    }
}

void console_scroll_down(void) {
    if (scroll_offset > 0) {
        scroll_offset--;
        console_redraw_from_history();
        update_cursor();
    }
}

uint32_t console_get_scroll_offset(void) {
    return scroll_offset;
}

void console_set_scroll_offset(uint32_t offset) {
    if (offset <= history_count) {
        scroll_offset = offset;
        console_redraw_from_history();
        update_cursor();
    }
}