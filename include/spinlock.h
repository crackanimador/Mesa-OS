/**
 * @file spinlock.h
 * @brief Spinlocks simples para sincronización en el kernel
 */
#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "types.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

/* Inicializa el spinlock en estado desbloqueado */
static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

/* Adquiere el spinlock (busy-wait) */
static inline void spin_lock(spinlock_t *lock) {
    while (1) {
        uint32_t tmp = 1;
        __asm__ volatile (
            "lock xchg %0, %1"
            : "+m"(lock->locked), "+r"(tmp)
            :
            : "memory"
        );
        if (tmp == 0) {
            /* Hemos obtenido el lock */
            break;
        }
        __asm__ volatile ("pause");
    }
}

/* Libera el spinlock */
static inline void spin_unlock(spinlock_t *lock) {
    __asm__ volatile ("" ::: "memory");
    lock->locked = 0;
}

#endif /* _SPINLOCK_H */