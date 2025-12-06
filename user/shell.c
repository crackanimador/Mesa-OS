/**
 * @file shell.c
 * @brief Shell básico para MesaOS-Lite
 */

#include "../include/types.h"
#include "../include/usersyscall.h"

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void puts(const char *s) {
    user_write(1, s, strlen(s));
}

static void shell_prompt(void) {
    puts("mesaos> ");
}

static void shell_loop(void) {
    char buffer[256];
    
    puts("\n=== MesaOS-Lite Shell ===\n");
    
    while (1) {
        shell_prompt();
        
        /* Leer comando */
        int64_t n = user_read(0, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            continue;
        }
        
        buffer[n] = '\0';
        
        /* Procesar comando simple */
        if (buffer[0] == '\n' || buffer[0] == '\0') {
            continue;
        }
        
        /* Comando "exit" */
        if (buffer[0] == 'e' && buffer[1] == 'x' && 
            buffer[2] == 'i' && buffer[3] == 't' && 
            (buffer[4] == '\n' || buffer[4] == '\0')) {
            puts("Goodbye!\n");
            user_exit(0);
        }
        
        /* Comando "help" */
        if (buffer[0] == 'h' && buffer[1] == 'e' && 
            buffer[2] == 'l' && buffer[3] == 'p' && 
            (buffer[4] == '\n' || buffer[4] == '\0')) {
            puts("Available commands:\n");
            puts("  help  - Show this help\n");
            puts("  exit  - Exit shell\n");
            puts("  ls    - List files in current directory\n");
            puts("  cat   - Show file contents\n");
            puts("  echo  - Print text\n");
            puts("  clear - Clear screen\n");
            continue;
        }
        
        /* Comando "ls" */
        if (buffer[0] == 'l' && buffer[1] == 's' && 
            (buffer[2] == '\n' || buffer[2] == '\0' || buffer[2] == ' ')) {
            char dir_path[256] = "/";
            char dir_buffer[1024];
            
            int result = user_readdir(dir_path, dir_buffer, sizeof(dir_buffer) - 1);
            if (result > 0) {
                dir_buffer[result] = '\0';
                puts(dir_buffer);
            } else {
                puts("Directory is empty or error occurred.\n");
            }
            continue;
        }
        
        /* Comando "cat" */
        if (buffer[0] == 'c' && buffer[1] == 'a' && buffer[2] == 't' && buffer[3] == ' ') {
            /* Extraer nombre de archivo */
            int i = 4;
            char filename[256];
            int fn_len = 0;
            while (buffer[i] != '\n' && buffer[i] != '\0' && fn_len < 255) {
                filename[fn_len++] = buffer[i++];
            }
            filename[fn_len] = '\0';
            
            if (fn_len > 0) {
                /* Abrir archivo */
                int fd = user_open(filename, 0);
                if (fd < 0) {
                    puts("Error: File not found or cannot open.\n");
                } else {
                    /* Leer archivo usando sys_read (simplificado) */
                    char file_buffer[512];
                    int64_t bytes_read = user_read(fd, file_buffer, sizeof(file_buffer) - 1);
                    if (bytes_read > 0) {
                        file_buffer[bytes_read] = '\0';
                        puts(file_buffer);
                        if (bytes_read == sizeof(file_buffer) - 1) {
                            puts("...\n");
                        }
                    }
                    user_close(fd);
                }
            } else {
                puts("Usage: cat <filename>\n");
            }
            continue;
        }
        
        /* Comando "echo" */
        if (buffer[0] == 'e' && buffer[1] == 'c' && buffer[2] == 'h' && buffer[3] == 'o' && buffer[4] == ' ') {
            /* Imprimir el resto de la línea */
            int i = 5;
            while (buffer[i] != '\n' && buffer[i] != '\0' && i < 255) {
                i++;
            }
            if (i > 5) {
                user_write(1, &buffer[5], i - 5);
                puts("\n");
            } else {
                puts("\n");
            }
            continue;
        }
        
        /* Comando "clear" */
        if (buffer[0] == 'c' && buffer[1] == 'l' && buffer[2] == 'e' && 
            buffer[3] == 'a' && buffer[4] == 'r' && 
            (buffer[5] == '\n' || buffer[5] == '\0')) {
            /* Limpiar pantalla VGA */
            volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
            for (int i = 0; i < 80 * 25; i++) {
                vga[i] = 0x0720; /* Espacio en blanco */
            }
            continue;
        }
        
        /* Comando desconocido */
        puts("Unknown command: ");
        user_write(1, buffer, n);
        puts("\nType 'help' for available commands.\n");
    }
}

void shell_main(void) {
    shell_loop();
}

