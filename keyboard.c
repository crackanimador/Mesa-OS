#include "../include/keyboard.h"
#include "../include/console.h"
#include "../include/io.h"

static keyboard_buffer_t kb_buffer;

static const char scancode_ascii_lower[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_upper[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0
};

static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed  = false;
static volatile bool caps_lock     = false;
static volatile bool extended_key  = false;

static inline uint64_t save_flags_cli(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags));
    return flags;
}

static inline void restore_flags(uint64_t flags) {
    __asm__ volatile ("push %0; popfq" : : "r"(flags));
}

void keyboard_init(void) {
    kb_buffer.head = 0;
    kb_buffer.tail = 0;
    kb_buffer.count = 0;
    shift_pressed = false;
    ctrl_pressed  = false;
    caps_lock     = false;

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    console_puts("[KBD] Keyboard initialized\n");
}

char scancode_to_ascii(uint8_t scancode, bool shift) {
    if (scancode >= 128) return 0;
    bool upper = shift ^ caps_lock;
    return upper ? scancode_ascii_upper[scancode]
                 : scancode_ascii_lower[scancode];
}

bool keyboard_buffer_push(char c) {
    uint64_t flags = save_flags_cli();
    bool ok = false;

    if (kb_buffer.count < KEYBOARD_BUFFER_SIZE) {
        kb_buffer.buffer[kb_buffer.head] = c;
        kb_buffer.head = (kb_buffer.head + 1) % KEYBOARD_BUFFER_SIZE;
        kb_buffer.count++;
        ok = true;
    }

    restore_flags(flags);
    return ok;
}

bool keyboard_buffer_pop(char *c) {
    uint64_t flags = save_flags_cli();
    bool ok = false;

    if (kb_buffer.count > 0) {
        *c = kb_buffer.buffer[kb_buffer.tail];
        kb_buffer.tail = (kb_buffer.tail + 1) % KEYBOARD_BUFFER_SIZE;
        kb_buffer.count--;
        ok = true;
    }

    restore_flags(flags);
    return ok;
}

uint32_t keyboard_buffer_available(void) {
    uint64_t flags = save_flags_cli();
    uint32_t c = kb_buffer.count;
    restore_flags(flags);
    return c;
}

static bool keyboard_buffer_backspace(void) {
    uint64_t flags = save_flags_cli();
    bool ok = false;

    if (kb_buffer.count > 0) {
        uint32_t last = (kb_buffer.head + KEYBOARD_BUFFER_SIZE - 1) % KEYBOARD_BUFFER_SIZE;
        if (kb_buffer.buffer[last] != '\n') {
            kb_buffer.head = last;
            kb_buffer.count--;
            ok = true;
        }
    }

    restore_flags(flags);
    return ok;
}

void keyboard_handle_scancode(uint8_t scancode) {
    /* Detectar tecla extendida (prefijo 0xE0) */
    if (scancode == KEY_EXTENDED) {
        extended_key = true;
        return;
    }
    
    bool release = (scancode & 0x80) != 0;
    uint8_t key  = scancode & 0x7F;
    
    /* Manejar teclas de flecha si es tecla extendida */
    if (extended_key) {
        extended_key = false;
        
        if (release) {
            return; /* Ignorar release de teclas extendidas por ahora */
        }
        
        /* Detectar flechas */
        if (key == KEY_UP) {
            keyboard_buffer_push((char)KEYCODE_UP);
            return;
        } else if (key == KEY_DOWN) {
            keyboard_buffer_push((char)KEYCODE_DOWN);
            return;
        } else if (key == KEY_LEFT) {
            keyboard_buffer_push((char)KEYCODE_LEFT);
            return;
        } else if (key == KEY_RIGHT) {
            keyboard_buffer_push((char)KEYCODE_RIGHT);
            return;
        }
        
        /* Otras teclas extendidas se ignoran por ahora */
        return;
    }

    if (key == KEY_LSHIFT || key == KEY_RSHIFT) {
        shift_pressed = !release;
        return;
    }

    if (key == KEY_LCTRL) {
        ctrl_pressed = !release;
        return;
    }

    if (key == KEY_CAPSLOCK && !release) {
        caps_lock = !caps_lock;
        return;
    }

    if (release) return;

    if (key == KEY_BACKSPACE) {
        if (keyboard_buffer_backspace()) {
            console_putchar('\b');
            console_putchar(' ');
            console_putchar('\b');
        }
        return;
    }

    char c = scancode_to_ascii(key, shift_pressed);
    if (!c) return;

    if (ctrl_pressed && (c == 'l' || c == 'L')) {
        console_clear();
        return;
    }

    if (keyboard_buffer_push(c)) {
        console_putchar(c);
    }
}

bool keyboard_get_shift(void) { return shift_pressed; }
bool keyboard_get_ctrl(void)  { return ctrl_pressed; }