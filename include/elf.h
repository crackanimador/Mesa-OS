/**
 * @file elf.h
 * @brief Formato ELF (Executable and Linkable Format)
 */

#ifndef _ELF_H_
#define _ELF_H_

#include "types.h"

/* Forward declaration */
struct pcb;
typedef struct pcb pcb_t;

#define EI_NIDENT 16

/* Tipos ELF */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4

/* Máquinas */
#define EM_X86_64 62

/* Cabecera ELF */
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} PACKED elf64_ehdr_t;

/* Tipos de segmento */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4

/* Flags de segmento */
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/* Cabecera de segmento */
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} PACKED elf64_phdr_t;

/* Funciones ELF */
int elf_validate(const elf64_ehdr_t *ehdr);
int elf_load_segments(const elf64_ehdr_t *ehdr, const void *file_data,
                      uint64_t *entry_point, pcb_t *proc);

#endif /* _ELF_H_ */

