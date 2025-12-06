/**
 * @file uaccess.c
 * @brief Implementación de acceso seguro user->kernel
 */

#include "../include/uaccess.h"
#include "../include/mm.h"
#include "../include/console.h"

/* Constante para el inicio del espacio de kernel */
#define KERNEL_VIRT_BASE    0xFFFF800000000000ULL

/**
 * @brief Valida que un rango de memoria está en espacio de usuario
 * @param ptr Puntero de usuario
 * @param len Longitud del buffer
 * @return 1 si válido, 0 si inválido
 *
 * Reglas:
 *  - ptr == NULL y len > 0 => inválido
 *  - [ptr, ptr+len) no debe overflow
 *  - [ptr, ptr+len) debe ser < KERNEL_VIRT_BASE
 */
int validate_user_pointer(const void *ptr, size_t len) {
    uint64_t start = (uint64_t)ptr;
    uint64_t end   = start + len;

    /* NULL + len>0 es inválido */
    if (ptr == NULL && len > 0) {
        console_puts("[uaccess] NULL pointer\n");
        return 0;
    }

    /* len == 0 siempre es válido */
    if (len == 0) {
        return 1;
    }

    /* Overflow aritmético en rango */
    if (end < start) {
        console_puts("[uaccess] Range overflow\n");
        return 0;
    }

    /* No permitir tocar espacio del kernel */
    if (start >= KERNEL_VIRT_BASE || end > KERNEL_VIRT_BASE) {
        console_puts("[uaccess] Pointer in kernel space\n");
        return 0;
    }

    return 1;
}

/**
 * @brief Copia desde memoria de usuario a buffer de kernel, tras validar
 * @param dest  Destino en kernel
 * @param src   Origen en espacio de usuario
 * @param len   Bytes a copiar
 * @return Número de bytes copiados (0 si puntero inválido)
 *
 * NOTA:
 *  - Asume que el CR3 actual contiene también el espacio de usuario
 *    (en tu diseño actual, PML4 compartido).
 *  - Si la página no está mapeada, provocará #PF que será atrapado
 *    por tu handler de excepciones.
 */
size_t copy_from_user(void *dest, const void *src, size_t len) {
    if (!validate_user_pointer(src, len)) {
        return 0;
    }

    const uint8_t *us = (const uint8_t *)src;
    uint8_t       *kd = (uint8_t *)dest;

    for (size_t i = 0; i < len; i++) {
        kd[i] = us[i];
    }

    return len;
}

/**
 * @brief Copia desde buffer de kernel a memoria de usuario, tras validar
 * @param dest  Destino en espacio de usuario
 * @param src   Origen en kernel
 * @param len   Bytes a copiar
 * @return Número de bytes copiados (0 si puntero inválido)
 */
size_t copy_to_user(void *dest, const void *src, size_t len) {
    if (!validate_user_pointer(dest, len)) {
        return 0;
    }

    const uint8_t *ks = (const uint8_t *)src;
    uint8_t       *ud = (uint8_t *)dest;

    for (size_t i = 0; i < len; i++) {
        ud[i] = ks[i];
    }

    return len;
}