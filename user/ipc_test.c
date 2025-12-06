/**
 * @file ipc_test.c
 * @brief Programa de prueba para IPC - Servidor
 */

#include "../include/types.h"
#include "../include/usersyscall.h"

#define VGA_BUFFER 0xB8000

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void puts(const char *s) {
    user_write(1, s, strlen(s));
}

/* Proceso servidor - recibe mensajes y responde */
void ipc_server(void) {
    char msg[256];
    uint32_t from_pid;
    int base = 20 * 80;
    volatile uint16_t *vga = (volatile uint16_t *)VGA_BUFFER;
    
    puts("[IPC Server] Started (PID should be 1)\n");
    
    /* Mostrar PID en pantalla */
    const char *label = "IPC Server: ";
    for (int i = 0; label[i]; i++) {
        vga[base + i] = (uint16_t)label[i] | 0x0B00;
    }
    
    int msg_count = 0;
    while (1) {
        /* Recibir mensaje */
        int64_t received = user_ipc_receive(&from_pid, msg, sizeof(msg) - 1);
        
        if (received > 0) {
            msg[received] = '\0';
            msg_count++;
            
            /* Mostrar mensaje recibido */
            char response[256];
            int len = 0;
            
            /* Construir respuesta */
            const char *prefix = "Got: ";
            for (int i = 0; prefix[i]; i++) {
                response[len++] = prefix[i];
            }
            for (int i = 0; msg[i] && len < 250; i++) {
                response[len++] = msg[i];
            }
            response[len++] = '\n';
            response[len] = '\0';
            
            puts(response);
            
            /* Enviar respuesta */
            const char *reply = "ACK";
            user_ipc_send(from_pid, reply, strlen(reply));
            
            /* Actualizar contador en pantalla */
            const char *hex = "0123456789ABCDEF";
            for (int i = 0; i < 8; i++) {
                int shift = (7 - i) * 4;
                char c = hex[(msg_count >> shift) & 0xF];
                vga[base + 12 + i] = (uint16_t)c | 0x0F00;
            }
        }
        
        /* Pequeña espera */
        for (volatile int i = 0; i < 100000; i++);
    }
}

/* Proceso cliente - envía mensajes */
void ipc_client(void) {
    char msg[256];
    uint32_t server_pid = 1; /* Asumimos que el servidor es PID 1 */
    int base = 21 * 80;
    volatile uint16_t *vga = (volatile uint16_t *)VGA_BUFFER;
    int counter = 0;
    
    puts("[IPC Client] Started (PID should be 2)\n");
    
    /* Mostrar label */
    const char *label = "IPC Client: ";
    for (int i = 0; label[i]; i++) {
        vga[base + i] = (uint16_t)label[i] | 0x0E00;
    }
    
    while (1) {
        /* Construir mensaje */
        const char *prefix = "Hello #";
        int len = 0;
        for (int i = 0; prefix[i]; i++) {
            msg[len++] = prefix[i];
        }
        
        /* Convertir contador a string */
        int num = counter++;
        if (num == 0) {
            msg[len++] = '0';
        } else {
            char digits[16];
            int d = 0;
            while (num > 0) {
                digits[d++] = '0' + (num % 10);
                num /= 10;
            }
            for (int i = d - 1; i >= 0; i--) {
                msg[len++] = digits[i];
            }
        }
        msg[len] = '\0';
        
        /* Enviar mensaje */
        user_ipc_send(server_pid, msg, len);
        
        /* Mostrar contador en pantalla */
        const char *hex = "0123456789ABCDEF";
        for (int i = 0; i < 8; i++) {
            int shift = (7 - i) * 4;
            char c = hex[(counter >> shift) & 0xF];
            vga[base + 12 + i] = (uint16_t)c | 0x0F00;
        }
        
        /* Esperar un poco antes de enviar el siguiente */
        for (volatile int i = 0; i < 500000; i++);
    }
}

