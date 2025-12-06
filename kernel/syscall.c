/**
 * @file syscall.c
 * @brief System Calls - I/O + Heap de usuario (sys_brk) + validación de punteros
 */

#include "../include/syscall.h"
#include "../include/console.h"
#include "../include/proc.h"
#include "../include/vfs.h"
#include "../include/mm.h"
#include "../include/uaccess.h"
#include "../include/ipc.h"
#include "../include/elf.h"
#include "../include/mesafs.h"
#include "../include/partition.h"
#include "../include/disk.h"
#include "../include/kheap.h"
#include "../include/elf.h"

/* ============== ESTRUCTURA PER-CPU PARA SYSCALL ============== */

cpu_data_t cpu_data __attribute__((aligned(16)));

/* ============== MSRs ============== */

#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_SFMASK          0xC0000084
#define MSR_KERNEL_GSBASE   0xC0000102

extern void syscall_entry(void);
extern uint8_t kernel_stack_top[];

/* Límite superior del heap de usuario (ejemplo: 512 MiB) */
#define USER_HEAP_MAX       0x0000000020000000ULL

/* Constantes de paginación */
#define PAGE_SIZE           4096
#define KERNEL_VIRT_BASE    0xFFFF800000000000ULL

/* Flags de página */
#define PTE_PRESENT         (1ULL << 0)
#define PTE_WRITABLE        (1ULL << 1)
#define PTE_USER            (1ULL << 2)

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low  = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

/* ============== HELPERS DE PAGINACIÓN ============== */
/* Las funciones pmm_alloc_page, pmm_free_page y mm_get_kernel_pml4
 * están definidas en mm/mm.c y declaradas en include/mm.h
 */

static inline uint64_t page_align_up(uint64_t addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static inline uint64_t page_align_down(uint64_t addr) {
    return addr & ~(PAGE_SIZE - 1);
}

/* ============== SYSCALL: WRITE ============== */

int64_t sys_write(int fd, const char *buf, size_t count) {
    vfs_node_t *node = NULL;

    if (fd == 1) {
        node = vfs_get_console_out();
    } else if (fd == 2) {
        node = vfs_get_console_err();
    } else {
        return -1;  /* EBADF */
    }

    if (!node) {
        return -1;
    }

    char kbuf[256];
    size_t to_copy = count;
    if (to_copy > sizeof(kbuf)) {
        to_copy = sizeof(kbuf);
    }

    if (!validate_user_pointer(buf, to_copy)) {
        console_puts("[sys_write] Invalid user pointer\n");
        return -1;
    }

    if (copy_from_user(kbuf, buf, to_copy) != to_copy) {
        console_puts("[sys_write] copy_from_user failed\n");
        return -1;
    }

    return vfs_write(node, kbuf, to_copy, 0);
}

/* ============== SYSCALL: READ ============== */

int64_t sys_read(int fd, char *buf, size_t count) {
    if (fd == 0) {
        /* stdin - leer desde consola */
        vfs_node_t *node = vfs_get_console_in();
        if (!node) {
            return -1;
        }

        char kbuf[256];
        size_t to_read = count;
        if (to_read > sizeof(kbuf)) {
            to_read = sizeof(kbuf);
        }

        int64_t n = vfs_read(node, kbuf, to_read, 0);
        if (n <= 0) {
            return n;
        }

        if (!validate_user_pointer(buf, (size_t)n)) {
            console_puts("[sys_read] Invalid user pointer (dst)\n");
            return -1;
        }

        uint8_t *dst = (uint8_t *)buf;
        for (int64_t i = 0; i < n; i++) {
            dst[i] = (uint8_t)kbuf[i];
        }

        return n;
    } else if (fd > 2) {
        /* File descriptor es un inode de mesafs (simplificado) */
        extern mesafs_t *mesafs_get_mounted(void);
        extern int mesafs_read(mesafs_t *fs, uint32_t inode, void *buf, size_t count, uint64_t offset);
        
        mesafs_t *fs = mesafs_get_mounted();
        if (!fs) {
            return -1;
        }
        
        uint32_t inode = (uint32_t)fd;
        
        /* Leer desde archivo */
        char kbuf[512];
        size_t to_read = count;
        if (to_read > sizeof(kbuf)) {
            to_read = sizeof(kbuf);
        }
        
        int read_result = mesafs_read(fs, inode, kbuf, to_read, 0);
        if (read_result < 0) {
            return -1;
        }
        
        if (!validate_user_pointer(buf, (size_t)read_result)) {
            return -1;
        }
        
        /* Copiar a buffer de usuario */
        uint8_t *dst = (uint8_t *)buf;
        for (int i = 0; i < read_result; i++) {
            dst[i] = (uint8_t)kbuf[i];
        }
        
        return read_result;
    }
    
    return -1;  /* fd inválido */
}

/* ============== SYSCALL: BRK (HEAP DE USUARIO) ============== */

/**
 * @brief Ajusta el break de memoria del proceso (heap de usuario)
 * @param new_brk Nueva dirección de break (0 = consulta)
 * @return Nuevo valor efectivo de brk (o el anterior si error)
 *
 * Garantías:
 *  - No invade USER_HEAP_MAX
 *  - No invade KERNEL_VIRT_BASE
 *  - No se acerca demasiado al stack de usuario (USER_STACK_TOP - guardia)
 *  - En expansión, asegura que el rango está mapeado con PTE_USER|PTE_WRITABLE
 */
uint64_t sys_brk(uint64_t new_brk) {
    pcb_t *proc = proc_current();
    if (!proc) {
        return 0;
    }

    /* Consulta: brk(0) → devolver break actual */
    if (new_brk == 0) {
        return proc->brk;
    }

    uint64_t old_brk = proc->brk;

    /* Rango básico: [heap_start, USER_HEAP_MAX) y fuera de kernel */
    if (new_brk < proc->heap_start) {
        return old_brk;
    }
    if (new_brk >= USER_HEAP_MAX) {
        return old_brk;
    }
    if (new_brk >= KERNEL_VIRT_BASE) {
        return old_brk;
    }

    /* Evitar colisión heap/stack de usuario:
     * dejamos una zona de guardia de, por ejemplo, 16 páginas (64 KiB)
     */
#ifdef USER_STACK_TOP
    const uint64_t GUARD_SIZE = 16ULL * PAGE_SIZE;
    if (new_brk >= (USER_STACK_TOP - GUARD_SIZE)) {
        console_puts("[sys_brk] Denied: collision with user stack\n");
        return old_brk;
    }
#endif

    /* Expansión: mapear nuevas páginas */
    if (new_brk > old_brk) {
        uint64_t old_aligned = page_align_up(old_brk);
        uint64_t new_aligned = page_align_up(new_brk);

        for (uint64_t addr = old_aligned; addr < new_aligned; addr += PAGE_SIZE) {
            phys_addr_t phys = pmm_alloc_page();
            if (!phys) {
                console_puts("[sys_brk] Out of physical memory\n");
                return old_brk;
            }

            /* En esta fase, todos comparten PML4 del kernel; en un diseño
             * con CR3 por proceso, usaríamos (uint64_t *)proc->cr3.
             */
            uint64_t *pml4 = mm_get_kernel_pml4();
            mm_map_page(pml4, addr, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        }

        proc->brk = new_brk;
        return proc->brk;
    }

    /* Reducción lógica (no liberamos físicamente todavía) */
    if (new_brk < old_brk) {
        proc->brk = new_brk;
        return proc->brk;
    }

    return proc->brk;
}

/* ============== SYSCALL: EXIT ============== */

NORETURN void sys_exit(int status) {
    console_puts("\n[PROCESS] Exit with status: ");
    console_put_dec((uint64_t)status);
    console_puts("\n");

    if (current_process) {
        current_process->state = PROC_STATE_ZOMBIE;
    }

    __asm__ volatile ("sti");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ============== SYSCALL: IPC_SEND ============== */

int64_t sys_ipc_send(uint32_t to_pid, const void *data, size_t size) {
    if (!validate_user_pointer(data, size)) {
        return -1;
    }
    
    char kbuf[IPC_MAX_MESSAGE_SIZE];
    if (size > IPC_MAX_MESSAGE_SIZE) {
        size = IPC_MAX_MESSAGE_SIZE;
    }
    
    if (copy_from_user(kbuf, data, size) != size) {
        return -1;
    }
    
    return ipc_send(to_pid, kbuf, size);
}

/* ============== SYSCALL: IPC_RECEIVE ============== */

int64_t sys_ipc_receive(uint32_t *from_pid, void *data, size_t max_size) {
    if (from_pid && !validate_user_pointer(from_pid, sizeof(uint32_t))) {
        return -1;
    }
    
    if (!validate_user_pointer(data, max_size)) {
        return -1;
    }
    
    uint32_t kfrom = IPC_ANY_PID;
    if (from_pid) {
        if (copy_from_user(&kfrom, from_pid, sizeof(uint32_t)) != sizeof(uint32_t)) {
            return -1;
        }
    }
    
    char kbuf[IPC_MAX_MESSAGE_SIZE];
    int received = ipc_receive(&kfrom, kbuf, max_size);
    
    if (received > 0) {
        if (copy_to_user(data, kbuf, (size_t)received) != (size_t)received) {
            return -1;
        }
        if (from_pid) {
            if (copy_to_user(from_pid, &kfrom, sizeof(uint32_t)) != sizeof(uint32_t)) {
                return -1;
            }
        }
    }
    
    return received;
}

/* ============== SYSCALL: IPC_CALL ============== */

int64_t sys_ipc_call(uint32_t to_pid, const void *request, size_t req_size,
                     void *reply, size_t reply_size) {
    if (!validate_user_pointer(request, req_size) ||
        !validate_user_pointer(reply, reply_size)) {
        return -1;
    }
    
    char kreq[IPC_MAX_MESSAGE_SIZE];
    char krep[IPC_MAX_MESSAGE_SIZE];
    
    if (req_size > IPC_MAX_MESSAGE_SIZE) req_size = IPC_MAX_MESSAGE_SIZE;
    if (reply_size > IPC_MAX_MESSAGE_SIZE) reply_size = IPC_MAX_MESSAGE_SIZE;
    
    if (copy_from_user(kreq, request, req_size) != req_size) {
        return -1;
    }
    
    int result = ipc_call(to_pid, kreq, req_size, krep, reply_size);
    
    if (result > 0) {
        if (copy_to_user(reply, krep, (size_t)result) != (size_t)result) {
            return -1;
        }
    }
    
    return result;
}

/* ============== SYSCALL: MMAP ============== */

void *sys_mmap(void *addr, size_t length, int prot, int flags) {
    (void)prot; (void)flags; /* TODO: Implementar flags y protección */
    
    pcb_t *proc = proc_current();
    if (!proc) {
        return (void *)-1;
    }
    
    /* Alinear a página */
    uint64_t start = (uint64_t)addr;
    if (start == 0) {
        start = proc->brk; /* Usar break como dirección base si no se especifica */
    }
    start = page_align_up(start);
    uint64_t end = page_align_up(start + length);
    
    /* Mapear páginas */
    uint64_t *pml4 = mm_get_kernel_pml4();
    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        phys_addr_t phys = pmm_alloc_page();
        if (!phys) {
            return (void *)-1;
        }
        mm_map_page(pml4, addr, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }
    
    return (void *)start;
}

/* ============== SYSCALL: MUNMAP ============== */

int sys_munmap(void *addr, size_t length) {
    (void)addr; (void)length; /* TODO: Implementar desmapeo */
    return 0;
}

/* ============== SYSCALL: EXEC ============== */

NORETURN int sys_exec(const char *path, char *const argv[]) {
    /* Procesar argumentos */
    char **kargv = NULL;
    int argc = 0;
    
    if (argv) {
        /* Contar argumentos */
        char **arg_ptr = argv;
        while (*arg_ptr) {
            argc++;
            arg_ptr++;
        }
        
        if (argc > 0) {
            /* Validar punteros de argumentos */
            for (int i = 0; i < argc; i++) {
                if (!validate_user_pointer(argv[i], 256)) {
                    console_puts("[sys_exec] Invalid argv pointer\n");
                    sys_exit(-1);
                }
            }
            
            /* Copiar argumentos a kernel (simplificado - solo contamos) */
            console_puts("[sys_exec] Arguments: ");
            console_put_dec(argc);
            console_puts("\n");
        }
    }
    
    pcb_t *proc = proc_current();
    if (!proc) {
        console_puts("[sys_exec] No current process\n");
        sys_exit(-1);
    }
    
    /* Validar puntero de path */
    if (!validate_user_pointer(path, 256)) {
        console_puts("[sys_exec] Invalid path pointer\n");
        sys_exit(-1);
    }
    
    /* Copiar path a kernel */
    char kpath[256];
    size_t path_len = 0;
    for (int i = 0; i < 255; i++) {
        char c;
        if (copy_from_user(&c, &path[i], 1) != 1) {
            sys_exit(-1);
        }
        kpath[i] = c;
        if (c == '\0') {
            path_len = i;
            break;
        }
    }
    kpath[255] = '\0';
    
    console_puts("[sys_exec] Loading: ");
    console_puts(kpath);
    console_puts("\n");
    
    /* Obtener sistema de archivos montado */
    extern mesafs_t *mesafs_get_mounted(void);
    extern uint32_t mesafs_find_file(mesafs_t *fs, const char *path);
    extern int mesafs_read(mesafs_t *fs, uint32_t inode, void *buf, size_t count, uint64_t offset);
    
    mesafs_t *fs = mesafs_get_mounted();
    if (!fs) {
        console_puts("[sys_exec] No filesystem mounted\n");
        sys_exit(-1);
    }
    
    /* Buscar archivo */
    uint32_t file_inode = mesafs_find_file(fs, kpath);
    if (file_inode == 0) {
        console_puts("[sys_exec] File not found: ");
        console_puts(kpath);
        console_puts("\n");
        sys_exit(-1);
    }
    
    /* Leer inodo para obtener tamaño */
    mesafs_inode_t file_inode_data;
    if (mesafs_read_inode(fs, file_inode, &file_inode_data) != 0) {
        console_puts("[sys_exec] Failed to read inode\n");
        sys_exit(-1);
    }
    
    if (file_inode_data.type != MESAFS_TYPE_FILE) {
        console_puts("[sys_exec] Not a regular file\n");
        sys_exit(-1);
    }
    
    /* Leer archivo completo */
    void *file_buffer = kmalloc(file_inode_data.size);
    if (!file_buffer) {
        console_puts("[sys_exec] Out of memory\n");
        sys_exit(-1);
    }
    
    int read_result = mesafs_read(fs, file_inode, file_buffer, file_inode_data.size, 0);
    if (read_result != (int)file_inode_data.size) {
        console_puts("[sys_exec] Failed to read file\n");
        kfree(file_buffer);
        sys_exit(-1);
    }
    
    /* Validar ELF */
    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)file_buffer;
    if (!elf_validate(ehdr)) {
        console_puts("[sys_exec] Invalid ELF file\n");
        kfree(file_buffer);
        sys_exit(-1);
    }
    
    /* Cargar segmentos ELF */
    uint64_t entry_point;
    if (elf_load_segments(ehdr, file_buffer, &entry_point, proc) != 0) {
        console_puts("[sys_exec] Failed to load ELF segments\n");
        kfree(file_buffer);
        sys_exit(-1);
    }
    
    /* Actualizar punto de entrada del proceso */
    proc->entry_point = (virt_addr_t)entry_point;
    
    /* Liberar buffer del archivo */
    kfree(file_buffer);
    
    console_puts("[sys_exec] Successfully loaded ELF, entry: ");
    console_put_hex(entry_point);
    console_puts("\n");
    
    /* TODO: Reemplazar proceso actual con el nuevo ejecutable */
    /* Por ahora, simplemente saltamos al nuevo entry point */
    console_puts("[sys_exec] Jumping to entry point...\n");
    
    /* Cambiar RIP del proceso al nuevo entry point */
    /* Esto requiere modificar el contexto guardado en el stack */
    /* Por ahora, simplemente retornamos error ya que es complejo */
    console_puts("[sys_exec] Process replacement not yet fully implemented\n");
    sys_exit(0);
}

/* ============== SYSCALL: SLEEP ============== */

int sys_sleep(uint32_t milliseconds) {
    (void)milliseconds; /* TODO: Implementar con scheduler */
    /* Por ahora, simplemente retornamos */
    return 0;
}

/* ============== SYSCALL: TIME ============== */

int64_t sys_time(void) {
    /* TODO: Implementar con RTC */
    static uint64_t fake_time = 0;
    return (int64_t)++fake_time;
}

/* ============== SYSCALL: OPEN ============== */

int sys_open(const char *path, int flags) {
    (void)flags; /* Por ahora ignoramos flags */
    
    if (!validate_user_pointer(path, 256)) {
        return -1;
    }
    
    /* Copiar path */
    char kpath[256];
    for (int i = 0; i < 255; i++) {
        char c;
        if (copy_from_user(&c, &path[i], 1) != 1) {
            return -1;
        }
        kpath[i] = c;
        if (c == '\0') {
            break;
        }
    }
    kpath[255] = '\0';
    
    /* Buscar archivo en mesafs */
    extern mesafs_t *mesafs_get_mounted(void);
    extern uint32_t mesafs_find_file(mesafs_t *fs, const char *path);
    
    mesafs_t *fs = mesafs_get_mounted();
    if (!fs) {
        return -1;
    }
    
    uint32_t inode = mesafs_find_file(fs, kpath);
    if (inode == 0) {
        return -1; /* Archivo no encontrado */
    }
    
    /* Por ahora, retornamos el inode como file descriptor */
    /* En un sistema completo, crearíamos un file_descriptor_t */
    return (int)inode;
}

/* ============== SYSCALL: CLOSE ============== */

int sys_close(int fd) {
    (void)fd; /* Por ahora no hacemos nada */
    return 0;
}

/* ============== SYSCALL: READDIR ============== */

int sys_readdir(const char *path, void *buffer, size_t buffer_size) {
    if (!validate_user_pointer(path, 256) || !validate_user_pointer(buffer, buffer_size)) {
        return -1;
    }
    
    /* Copiar path */
    char kpath[256];
    for (int i = 0; i < 255; i++) {
        char c;
        if (copy_from_user(&c, &path[i], 1) != 1) {
            return -1;
        }
        kpath[i] = c;
        if (c == '\0') {
            break;
        }
    }
    kpath[255] = '\0';
    
    /* Buscar directorio */
    extern mesafs_t *mesafs_get_mounted(void);
    extern uint32_t mesafs_find_file(mesafs_t *fs, const char *path);
    extern int mesafs_list_dir(mesafs_t *fs, uint32_t dir_inode, mesafs_dirent_t *entries, size_t max_entries);
    
    mesafs_t *fs = mesafs_get_mounted();
    if (!fs) {
        return -1;
    }
    
    uint32_t dir_inode = mesafs_find_file(fs, kpath);
    if (dir_inode == 0) {
        return -1;
    }
    
    /* Listar directorio */
    mesafs_dirent_t entries[64];
    int count = mesafs_list_dir(fs, dir_inode, entries, 64);
    if (count < 0) {
        return -1;
    }
    
    /* Copiar entradas a buffer de usuario */
    char *output = (char *)buffer;
    int pos = 0;
    
    for (int i = 0; i < count && pos < (int)buffer_size - 1; i++) {
        /* Formato: "nombre\n" */
        for (int j = 0; j < entries[i].name_len && pos < (int)buffer_size - 2; j++) {
            output[pos++] = entries[i].name[j];
        }
        if (pos < (int)buffer_size - 1) {
            output[pos++] = '\n';
        }
    }
    
    if (pos < (int)buffer_size) {
        output[pos] = '\0';
    }
    
    return pos;
}

/* ============== TABLA DE SYSCALLS ============== */

typedef int64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static syscall_fn syscall_table[MAX_SYSCALLS] = {
    [SYS_READ]       = (syscall_fn)sys_read,
    [SYS_WRITE]      = (syscall_fn)sys_write,
    [SYS_BRK]        = (syscall_fn)sys_brk,
    [SYS_EXIT]       = (syscall_fn)sys_exit,
    [SYS_IPC_SEND]   = (syscall_fn)sys_ipc_send,
    [SYS_IPC_RECEIVE]= (syscall_fn)sys_ipc_receive,
    [SYS_IPC_CALL]   = (syscall_fn)sys_ipc_call,
    [SYS_MMAP]       = (syscall_fn)sys_mmap,
    [SYS_MUNMAP]     = (syscall_fn)sys_munmap,
    [SYS_EXEC]       = (syscall_fn)sys_exec,
    [SYS_SLEEP]      = (syscall_fn)sys_sleep,
    [SYS_TIME]       = (syscall_fn)sys_time,
    [SYS_OPEN]       = (syscall_fn)sys_open,
    [SYS_CLOSE]      = (syscall_fn)sys_close,
    [SYS_READDIR]    = (syscall_fn)sys_readdir,
};

/* ============== HANDLER PRINCIPAL ============== */

int64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg4;
    (void)arg5;

    if (num >= MAX_SYSCALLS || syscall_table[num] == NULL) {
        console_puts("[SYSCALL] Unknown syscall: ");
        console_put_dec(num);
        console_puts("\n");
        return -1;
    }

    return syscall_table[num](arg1, arg2, arg3, arg4, arg5);
}

/* ============== INICIALIZACIÓN DE SYSCALL ============== */

void syscall_init(void) {
    console_puts("[SYSCALL] Initializing system call interface...\n");

    cpu_data.kernel_rsp   = (uint64_t)kernel_stack_top;
    cpu_data.user_rsp     = 0;
    cpu_data.current_task = 0;

    wrmsr(MSR_KERNEL_GSBASE, (uint64_t)&cpu_data);

    /* STAR:
     * Bits 47:32 -> SYSCALL CS/SS (kernel)  = 0x08 (CS), SS = 0x10
     * Bits 63:48 -> SYSRET CS base (user)   = 0x13 (CS user - 16)
     */
    uint64_t star = 0;
    star |= ((uint64_t)0x08) << 32;  /* SYSCALL CS (kernel code) */
    star |= ((uint64_t)0x13) << 48;  /* SYSRET base (user code - 16) */
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_SFMASK, 0x200);        /* limpiar IF en RFLAGS durante SYSCALL */

    console_puts("       Syscalls: read(0), write(1), open(2), close(3), brk(12), exit(60)\n");
    console_puts("                ipc_send(40), ipc_receive(41), ipc_call(42)\n");
    console_puts("                mmap(9), munmap(11), exec(59), sleep(35), time(201)\n");
    console_puts("                readdir(78)\n");
    console_puts("       Status: ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_puts("OK\n");
    console_set_color(VGA_WHITE, VGA_BLACK);
}