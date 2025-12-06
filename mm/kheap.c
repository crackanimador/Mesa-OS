/**
 * @file kheap.c
 * @brief Kernel Heap - Asignador First-Fit con lista enlazada y spinlock
 */

#include "../include/types.h"
#include "../include/kheap.h"
#include "../include/spinlock.h"
#include "../include/console.h"

/* Definido por el linker en linker.ld */
extern uint8_t _kernel_end;

/* Configuración del heap (4 MiB) */
#define KHEAP_SIZE   (4 * 1024 * 1024ULL)
#define KHEAP_START  (((uint64_t)&_kernel_end + 0xFFF) & ~0xFFFULL)
#define KHEAP_END    (KHEAP_START + KHEAP_SIZE)

/* Header de bloque del heap */
typedef struct kheap_block {
    size_t size;                 /* Tamaño útil (sin incluir header) */
    struct kheap_block *next;    /* Siguiente bloque */
    uint8_t free;                /* 1 = libre, 0 = ocupado */
    uint8_t padding[7];          /* padding para alinear a 16 bytes */
} kheap_block_t;

static kheap_block_t *kheap_head = NULL;
static spinlock_t kheap_lock;

/* Alinear a 16 bytes */
static inline size_t align16(size_t size) {
    return (size + 15) & ~((size_t)15);
}

void kheap_init(void) {
    spinlock_init(&kheap_lock);

    kheap_head = (kheap_block_t *)KHEAP_START;
    kheap_head->size = KHEAP_END - KHEAP_START - sizeof(kheap_block_t);
    kheap_head->next = NULL;
    kheap_head->free = 1;

    console_puts("[KHEAP] Kernel heap initialized\n");
    console_puts("        Start: ");
    console_put_hex(KHEAP_START);
    console_puts("\n        End:   ");
    console_put_hex(KHEAP_END);
    console_puts("\n        Size:  ");
    console_put_dec(KHEAP_SIZE);
    console_puts(" bytes\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = align16(size);

    spin_lock(&kheap_lock);

    kheap_block_t *cur = kheap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            /* ¿Dividir el bloque? */
            size_t remaining = cur->size - size;
            if (remaining > sizeof(kheap_block_t) + 16) {
                kheap_block_t *new_block =
                    (kheap_block_t *)((uint8_t *)(cur + 1) + size);
                new_block->size = remaining - sizeof(kheap_block_t);
                new_block->next = cur->next;
                new_block->free = 1;

                cur->size = size;
                cur->next = new_block;
            }

            cur->free = 0;
            void *ret = (void *)(cur + 1);
            spin_unlock(&kheap_lock);
            return ret;
        }
        cur = cur->next;
    }

    spin_unlock(&kheap_lock);
    return NULL; /* Out of memory */
}

void kfree(void *ptr) {
    if (!ptr) return;

    spin_lock(&kheap_lock);

    kheap_block_t *block = ((kheap_block_t *)ptr) - 1;
    block->free = 1;

    /* Coalescer bloques adyacentes libres */
    kheap_block_t *cur = kheap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(kheap_block_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }

    spin_unlock(&kheap_lock);
}