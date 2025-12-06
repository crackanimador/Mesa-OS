/**
 * @file ipc.h
 * @brief Inter-Process Communication (IPC) - Sistema de mensajes
 */

#ifndef _IPC_H_
#define _IPC_H_

#include "types.h"
#include "proc.h"

#define IPC_MAX_MESSAGE_SIZE    256
#define IPC_MAX_QUEUE_SIZE      16
#define IPC_ANY_PID             0xFFFFFFFF

/* Tipos de mensaje */
#define IPC_MSG_NORMAL          0x01
#define IPC_MSG_REPLY           0x02

/* Estructura de mensaje */
typedef struct ipc_message {
    uint32_t from_pid;
    uint32_t to_pid;
    uint32_t type;
    uint32_t size;
    uint8_t data[IPC_MAX_MESSAGE_SIZE];
} ipc_message_t;

/* Cola de mensajes por proceso */
typedef struct ipc_queue {
    ipc_message_t messages[IPC_MAX_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} ipc_queue_t;

/* Funciones IPC */
int ipc_init(void);
int ipc_send(uint32_t to_pid, const void *data, size_t size);
int ipc_receive(uint32_t *from_pid, void *data, size_t max_size);
int ipc_call(uint32_t to_pid, const void *request, size_t req_size,
             void *reply, size_t reply_size);

#endif /* _IPC_H_ */

