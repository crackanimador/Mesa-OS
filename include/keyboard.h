#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

#define KEYBOARD_BUFFER_SIZE    256

/* Scancodes especiales */
#define KEY_ESCAPE              0x01
#define KEY_BACKSPACE           0x0E
#define KEY_TAB                 0x0F
#define KEY_ENTER               0x1C
#define KEY_LCTRL               0x1D
#define KEY_LSHIFT              0x2A
#define KEY_RSHIFT              0x36
#define KEY_LALT                0x38
#define KEY_CAPSLOCK            0x3A

/* Teclas extendidas (prefijo 0xE0) */
#define KEY_EXTENDED            0xE0
#define KEY_UP                  0x48
#define KEY_DOWN                0x50
#define KEY_LEFT                0x4B
#define KEY_RIGHT               0x4D
#define KEY_HOME                0x47
#define KEY_END                 0x4F
#define KEY_PAGEUP              0x49
#define KEY_PAGEDOWN            0x51

typedef struct {
    char buffer[KEYBOARD_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} keyboard_buffer_t;

void keyboard_init(void);
bool keyboard_buffer_push(char c);
bool keyboard_buffer_pop(char *c);
uint32_t keyboard_buffer_available(void);
char scancode_to_ascii(uint8_t scancode, bool shift);
void keyboard_handle_scancode(uint8_t scancode);
bool keyboard_get_shift(void);
bool keyboard_get_ctrl(void);

/* Códigos especiales para teclas de flecha */
#define KEYCODE_UP              0x80
#define KEYCODE_DOWN            0x81
#define KEYCODE_LEFT            0x82
#define KEYCODE_RIGHT           0x83

#endif