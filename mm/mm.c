/**
 * @file mm.c
 * @brief Gestión de memoria física y virtual básica
 */

#include "../include/mm.h"
#include "../include/console.h"
#include "../include/kheap.h"

/* Constantes de paginación */
#define PAGE_SIZE           4096
#define USER_STACK_TOP      0x7FFFF0000000UL

/* Flags de página */
#define PTE_PRESENT         (1ULL << 0)
#define PTE_WRITABLE        (1ULL << 1)
#define PTE_USER            (1ULL << 2)
#define PTE_HUGE            (1ULL << 7)
#define PTE_ADDR_MASK       0x000FFFFFFFFFF000ULL

/* PML4 inicial (configurada en boot.asm) */
extern uint64_t boot_pml4[];

/* Forward declarations */
phys_addr_t pmm_alloc_page(void);
void pmm_free_page(phys_addr_t addr);
uint64_t *mm_get_kernel_pml4(void);

/* Asignador físico muy simple: asumimos 128 MiB usables */
#define MAX_PHYS_PAGES   (128 * 1024 * 1024ULL / PAGE_SIZE)
static uint8_t phys_bitmap[MAX_PHYS_PAGES / 8];

static inline void bitmap_set(uint64_t i)   { phys_bitmap[i/8] |=  (1 << (i%8)); }
static inline void bitmap_clear(uint64_t i) { phys_bitmap[i/8] &= ~(1 << (i%8)); }
static inline bool bitmap_test(uint64_t i)  { return phys_bitmap[i/8] &   (1 << (i%8)); }

static uint64_t find_free_page(void) {
    for (uint64_t i = 0; i < MAX_PHYS_PAGES; i++) {
        if (!bitmap_test(i)) return i;
    }
    return (uint64_t)-1;
}

void mm_init(uint64_t kernel_pml4_addr) {
    (void)kernel_pml4_addr;
    console_puts("[MM] Initializing memory...\n");

    /* Marcar las primeras 2MiB como usadas (kernel + tablas de página) */
    for (uint64_t i = 0; i < (2 * 1024 * 1024ULL / PAGE_SIZE); i++) {
        bitmap_set(i);
    }

    console_puts("     Physical pages: ");
    console_put_dec(MAX_PHYS_PAGES);
    console_puts("\n");

    /* Inicializar heap de kernel */
    kheap_init();

    /* === Mapeo de pila de usuario (1 página bajo USER_STACK_TOP) === */
    console_puts("[MM] Mapping user stack page...\n");
    phys_addr_t usp_phys = pmm_alloc_page();
    if (usp_phys) {
        uint64_t  *pml4    = mm_get_kernel_pml4();
        virt_addr_t usp_va = USER_STACK_TOP - PAGE_SIZE;

        mm_map_page(pml4, usp_va, usp_phys,
                    PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        {
            console_puts("     User stack mapped at: ");
            console_put_hex(usp_va);
            console_puts("\n");
        }
    } else {
        console_puts("     Out of memory for user stack\n");
    }

    console_puts("[MM] Ready\n");
}

phys_addr_t pmm_alloc_page(void) {
    uint64_t idx = find_free_page();
    if (idx == (uint64_t)-1) {
        console_puts("[PMM] Out of physical memory\n");
        return 0;
    }
    bitmap_set(idx);
    return idx * PAGE_SIZE;
}

void pmm_free_page(phys_addr_t addr) {
    if (addr & (PAGE_SIZE - 1)) return;
    uint64_t idx = addr / PAGE_SIZE;
    if (idx >= MAX_PHYS_PAGES) return;
    bitmap_clear(idx);
}

/* Helpers para índices de tablas */
static inline uint64_t pml4_index(virt_addr_t addr) { return (addr >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(virt_addr_t addr) { return (addr >> 30) & 0x1FF; }
static inline uint64_t pd_index  (virt_addr_t addr) { return (addr >> 21) & 0x1FF; }
static inline uint64_t pt_index  (virt_addr_t addr) { return (addr >> 12) & 0x1FF; }

/* Reserva una nueva tabla de 4KiB (para PDPT, PD o PT) */
static uint64_t *alloc_table(void) {
    phys_addr_t phys = pmm_alloc_page();
    if (!phys) return NULL;
    uint64_t *tbl = (uint64_t *)phys;
    for (int i = 0; i < 512; i++) tbl[i] = 0;
    return tbl;
}

void mm_map_page(uint64_t *pml4, virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    if (!pml4) return;
    if ((virt & (PAGE_SIZE - 1)) || (phys & (PAGE_SIZE - 1))) return;

    uint64_t pml4i = pml4_index(virt);
    uint64_t pdpti = pdpt_index(virt);
    uint64_t pdi   = pd_index(virt);
    uint64_t pti   = pt_index(virt);

    uint64_t *pdpt, *pd, *pt;

    /* PDPT */
    if (!(pml4[pml4i] & PTE_PRESENT)) {
        pdpt = alloc_table();
        if (!pdpt) return;
        pml4[pml4i] = ((uint64_t)pdpt) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else {
        pdpt = (uint64_t *)(pml4[pml4i] & PTE_ADDR_MASK);
    }

    /* PD */
    if (!(pdpt[pdpti] & PTE_PRESENT)) {
        pd = alloc_table();
        if (!pd) return;
        pdpt[pdpti] = ((uint64_t)pd) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else {
        pd = (uint64_t *)(pdpt[pdpti] & PTE_ADDR_MASK);
    }

    /* PT */
    if (!(pd[pdi] & PTE_PRESENT)) {
        /* Si fuera una huge page (2MiB), no podemos subdividirla */
        if (pd[pdi] & PTE_HUGE) {
            console_puts("[MM] ERROR: Cannot map over huge page\n");
            return;
        }
        pt = alloc_table();
        if (!pt) return;
        pd[pdi] = ((uint64_t)pt) | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else {
        pt = (uint64_t *)(pd[pdi] & PTE_ADDR_MASK);
    }

    pt[pti] = phys | flags | PTE_PRESENT;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t *mm_get_kernel_pml4(void) {
    return boot_pml4;
}

uint64_t mm_create_user_pml4(void) {
    /* Por ahora, compartimos el PML4 del kernel */
    /* En un diseño completo, clonarías las entradas del kernel y crearías nuevas para usuario */
    return (uint64_t)boot_pml4;
}