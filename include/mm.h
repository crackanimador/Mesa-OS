/**
 * @file mm.h
 * @brief Declaraciones para la gestión de memoria (Paging, KHeap).
 */

#ifndef _MM_H_
#define _MM_H_

#include "types.h"

// Tipos de dirección (aunque suelen ser solo uint64_t)
typedef uint64_t virt_addr_t;
typedef uint64_t phys_addr_t;

/* Declaraciones de Heap de Kernel */
void kheap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

/* Declaraciones de Paging */
void mm_init(uint64_t kernel_pml4_addr);
void mm_map_page(uint64_t *pml4, virt_addr_t virt, phys_addr_t phys, uint64_t flags);
void mm_unmap_page(uint64_t *pml4, virt_addr_t virt);
void mm_switch_pml4(uint64_t pml4_addr);
uint64_t *mm_get_kernel_pml4(void);
phys_addr_t pmm_alloc_page(void);
void pmm_free_page(phys_addr_t addr);

// [FIX para proc/proc.c: undefined reference to `mm_create_user_pml4`]
uint64_t mm_create_user_pml4(void); 

// Flags de página (Ejemplos típicos)
#define PAGE_PRESENT  (1 << 0)
#define PAGE_WRITE    (1 << 1)
#define PAGE_USER     (1 << 2) // CRÍTICO para Ring 3
#define PAGE_NX       (1ULL << 63)

#endif // _MM_H_