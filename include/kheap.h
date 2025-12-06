/**
 * @file kheap.h
 * @brief Asignador de heap del kernel
 */
#ifndef _KHEAP_H
#define _KHEAP_H

#include "types.h"

/**
 * @brief Inicializa el heap del kernel
 */
void kheap_init(void);

/**
 * @brief Asigna memoria en el heap del kernel
 * @param size Bytes solicitados
 * @return Puntero alineado o NULL si no hay memoria
 */
void *kmalloc(size_t size);

/**
 * @brief Libera memoria asignada con kmalloc
 */
void kfree(void *ptr);

#endif /* _KHEAP_H */