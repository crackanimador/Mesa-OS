/**
 * @file elf.c
 * @brief Cargador ELF
 */

#include "../include/elf.h"
#include "../include/console.h"
#include "../include/mm.h"
#include "../include/proc.h"

#define ELF_MAGIC 0x464C457F  /* "\x7FELF" */

int elf_validate(const elf64_ehdr_t *ehdr) {
    if (!ehdr) {
        return 0;
    }
    
    /* Verificar firma mágica */
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        return 0;
    }
    
    /* Verificar clase (64-bit) */
    if (ehdr->e_ident[4] != 2) { /* ELFCLASS64 */
        return 0;
    }
    
    /* Verificar tipo ejecutable */
    if (ehdr->e_type != ET_EXEC) {
        return 0;
    }
    
    /* Verificar arquitectura x86_64 */
    if (ehdr->e_machine != EM_X86_64) {
        return 0;
    }
    
    return 1;
}

int elf_load_segments(const elf64_ehdr_t *ehdr, const void *file_data,
                      uint64_t *entry_point, pcb_t *proc) {
    if (!elf_validate(ehdr)) {
        console_puts("[ELF] Invalid ELF file\n");
        return -1;
    }
    
    if (!proc) {
        console_puts("[ELF] No process provided\n");
        return -1;
    }
    
    *entry_point = ehdr->e_entry;
    
    /* Cargar segmentos */
    const uint8_t *data = (const uint8_t *)file_data;
    const elf64_phdr_t *phdr = (const elf64_phdr_t *)(data + ehdr->e_phoff);
    
    extern uint64_t *mm_get_kernel_pml4(void);
    extern phys_addr_t pmm_alloc_page(void);
    extern void mm_map_page(uint64_t *pml4, virt_addr_t virt, phys_addr_t phys, uint64_t flags);
    
    uint64_t *pml4 = mm_get_kernel_pml4();
    const uint64_t PAGE_SIZE = 4096;
    const uint64_t PTE_PRESENT = (1ULL << 0);
    const uint64_t PTE_WRITABLE = (1ULL << 1);
    const uint64_t PTE_USER = (1ULL << 2);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            console_puts("[ELF] Loading segment at ");
            console_put_hex(phdr[i].p_vaddr);
            console_puts(" (");
            console_put_dec(phdr[i].p_memsz);
            console_puts(" bytes)\n");
            
            /* Mapear páginas para el segmento */
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;
            
            /* Alinear dirección virtual a página */
            uint64_t start_page = vaddr & ~(PAGE_SIZE - 1);
            uint64_t end_page = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            
            /* Mapear cada página */
            for (uint64_t page_addr = start_page; page_addr < end_page; page_addr += PAGE_SIZE) {
                phys_addr_t phys = pmm_alloc_page();
                if (!phys) {
                    console_puts("[ELF] Out of memory\n");
                    return -1;
                }
                
                /* Determinar flags de página */
                uint64_t flags = PTE_PRESENT | PTE_USER;
                if (phdr[i].p_flags & PF_W) {
                    flags |= PTE_WRITABLE;
                }
                
                mm_map_page(pml4, page_addr, phys, flags);
                
                /* Copiar datos del archivo a la página si está dentro de filesz */
                if (page_addr >= vaddr && page_addr < vaddr + filesz) {
                    uint64_t copy_offset = page_addr - vaddr;
                    uint64_t copy_size = PAGE_SIZE;
                    if (copy_offset + copy_size > filesz) {
                        copy_size = filesz - copy_offset;
                    }
                    
                    /* Mapear página temporalmente para escribir */
                    uint8_t *page_ptr = (uint8_t *)(page_addr);
                    const uint8_t *src = data + offset + copy_offset;
                    for (uint64_t j = 0; j < copy_size; j++) {
                        page_ptr[j] = src[j];
                    }
                    
                    /* Limpiar el resto de la página si memsz > filesz */
                    if (copy_size < PAGE_SIZE && memsz > filesz) {
                        for (uint64_t j = copy_size; j < PAGE_SIZE; j++) {
                            page_ptr[j] = 0;
                        }
                    }
                } else if (page_addr < vaddr + memsz) {
                    /* Página en .bss - inicializar a cero */
                    uint8_t *page_ptr = (uint8_t *)(page_addr);
                    for (uint64_t j = 0; j < PAGE_SIZE; j++) {
                        page_ptr[j] = 0;
                    }
                }
            }
        }
    }
    
    return 0;
}

