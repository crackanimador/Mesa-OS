/**
 * @file uaccess.h
 * @brief Acceso seguro a memoria de usuario (user -> kernel)
 */
#ifndef _UACCESS_H
#define _UACCESS_H

#include "types.h"

/**
 * @brief Valida que un rango de memoria está en espacio de usuario
 * @param ptr Puntero de usuario
 * @param len Longitud del buffer
 * @return 1 si válido, 0 si inválido
 *
 * Regla: el rango [ptr, ptr+len) no debe cruzar a KERNEL_VIRT_BASE.
 */
int validate_user_pointer(const void *ptr, size_t len);

/**
 * @brief Copia desde memoria de usuario a buffer de kernel, tras validar
 * @param dest  Destino en kernel
 * @param src   Origen en espacio de usuario
 * @param len   Bytes a copiar
 * @return Número de bytes copiados (0 si puntero inválido)
 */
size_t copy_from_user(void *dest, const void *src, size_t len);

/**
 * @brief Copia desde buffer de kernel a memoria de usuario, tras validar
 * @param dest  Destino en espacio de usuario
 * @param src   Origen en kernel
 * @param len   Bytes a copiar
 * @return Número de bytes copiados (0 si puntero inválido)
 */
size_t copy_to_user(void *dest, const void *src, size_t len);

#endif /* _UACCESS_H */