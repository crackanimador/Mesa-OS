/**
 * @file usersyscall.h
 * @brief Macros para hacer syscalls desde código de usuario
 */

#ifndef _USERSYSCALL_H_
#define _USERSYSCALL_H_

#include "types.h"

/* Macro para hacer syscalls */
#define SYSCALL0(num) ({ \
    int64_t ret; \
    __asm__ volatile ( \
        "mov %1, %%rax\n\t" \
        "syscall\n\t" \
        : "=a"(ret) \
        : "i"(num) \
        : "rcx", "r11", "memory" \
    ); \
    ret; \
})

#define SYSCALL1(num, arg1) ({ \
    int64_t ret; \
    __asm__ volatile ( \
        "mov %1, %%rax\n\t" \
        "mov %2, %%rdi\n\t" \
        "syscall\n\t" \
        : "=a"(ret) \
        : "i"(num), "r"((uint64_t)(arg1)) \
        : "rcx", "r11", "memory" \
    ); \
    ret; \
})

#define SYSCALL2(num, arg1, arg2) ({ \
    int64_t ret; \
    __asm__ volatile ( \
        "mov %1, %%rax\n\t" \
        "mov %2, %%rdi\n\t" \
        "mov %3, %%rsi\n\t" \
        "syscall\n\t" \
        : "=a"(ret) \
        : "i"(num), "r"((uint64_t)(arg1)), "r"((uint64_t)(arg2)) \
        : "rcx", "r11", "memory" \
    ); \
    ret; \
})

#define SYSCALL3(num, arg1, arg2, arg3) ({ \
    int64_t ret; \
    __asm__ volatile ( \
        "mov %1, %%rax\n\t" \
        "mov %2, %%rdi\n\t" \
        "mov %3, %%rsi\n\t" \
        "mov %4, %%rdx\n\t" \
        "syscall\n\t" \
        : "=a"(ret) \
        : "i"(num), "r"((uint64_t)(arg1)), "r"((uint64_t)(arg2)), "r"((uint64_t)(arg3)) \
        : "rcx", "r11", "memory" \
    ); \
    ret; \
})

/* Wrappers para syscalls comunes */
static inline int64_t user_write(int fd, const char *buf, size_t count) {
    return SYSCALL3(1, fd, buf, count);
}

static inline int64_t user_read(int fd, char *buf, size_t count) {
    return SYSCALL3(0, fd, buf, count);
}

static inline int64_t user_ipc_send(uint32_t to_pid, const void *data, size_t size) {
    return SYSCALL3(40, to_pid, data, size);
}

static inline int64_t user_ipc_receive(uint32_t *from_pid, void *data, size_t max_size) {
    return SYSCALL3(41, from_pid, data, max_size);
}

static inline void user_exit(int status) {
    SYSCALL1(60, status);
    __builtin_unreachable();
}

static inline int user_open(const char *path, int flags) {
    return (int)SYSCALL2(2, path, flags);
}

static inline int user_close(int fd) {
    return (int)SYSCALL1(3, fd);
}

static inline int user_readdir(const char *path, void *buffer, size_t buffer_size) {
    return (int)SYSCALL3(78, path, buffer, buffer_size);
}

#endif /* _USERSYSCALL_H_ */

