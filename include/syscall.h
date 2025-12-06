/**
 * @file syscall.h
 * @brief System Call Interface
 */
#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_BRK     12
#define SYS_EXIT    60

/* IPC syscalls */
#define SYS_IPC_SEND    40
#define SYS_IPC_RECEIVE 41
#define SYS_IPC_CALL    42

/* Memory management syscalls */
#define SYS_MMAP        9
#define SYS_MUNMAP      11

/* Executable loading */
#define SYS_EXEC        59

/* Time syscalls */
#define SYS_SLEEP       35
#define SYS_TIME        201

/* Filesystem syscalls */
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_READDIR     78

#define MAX_SYSCALLS 256

typedef struct PACKED {
    uint64_t kernel_rsp;
    uint64_t user_rsp;
    uint64_t current_task;
} cpu_data_t;

void syscall_init(void);

int64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Syscalls individuales */
int64_t sys_read(int fd, char *buf, size_t count);
int64_t sys_write(int fd, const char *buf, size_t count);
uint64_t sys_brk(uint64_t new_brk);
NORETURN void sys_exit(int status);

/* IPC syscalls */
int64_t sys_ipc_send(uint32_t to_pid, const void *data, size_t size);
int64_t sys_ipc_receive(uint32_t *from_pid, void *data, size_t max_size);
int64_t sys_ipc_call(uint32_t to_pid, const void *request, size_t req_size,
                     void *reply, size_t reply_size);

/* Memory management syscalls */
void *sys_mmap(void *addr, size_t length, int prot, int flags);
int sys_munmap(void *addr, size_t length);

/* Executable loading */
NORETURN int sys_exec(const char *path, char *const argv[]);

/* Time syscalls */
int sys_sleep(uint32_t milliseconds);
int64_t sys_time(void);

/* Filesystem syscalls */
int sys_open(const char *path, int flags);
int sys_close(int fd);
int sys_readdir(const char *path, void *buffer, size_t buffer_size);

#endif /* _SYSCALL_H */