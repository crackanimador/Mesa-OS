;; boot.asm - Transición a Long Mode y salto a kernel_main

global _start
global boot_pml4
global kernel_stack_top
global gdt64

extern kernel_main

section .text
bits 32

CR0_PG          equ (1 << 31)
CR4_PAE         equ (1 << 5)
EFER_MSR        equ 0xC0000080
EFER_LME        equ (1 << 8)

PAGE_PRESENT    equ (1 << 0)
PAGE_WRITABLE   equ (1 << 1)
PAGE_USER       equ (1 << 2)
PAGE_HUGE       equ (1 << 7)

_start:
    cli

    mov edi, ebx    ; multiboot_info en EDI

    ; Verificar soporte Long Mode
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode

    ; ===========================
    ; Configurar tablas de página
    ; ===========================

    ; Limpiar PML4/PDPT/PD
    mov edi, boot_pml4
    xor eax, eax
    mov ecx, 0x1000
    rep stosd

    ; PML4[0] -> PDPT (user, RW)
    mov edi, boot_pml4
    mov eax, boot_pdpt
    or eax, (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)
    mov [edi], eax

    ; PDPT[0] -> PD (user, RW)
    mov edi, boot_pdpt
    mov eax, boot_pd
    or eax, (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)
    mov [edi], eax

    ; PD[0] -> 2MiB @0 (huge, user)
    mov edi, boot_pd
    mov eax, (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_HUGE)
    mov [edi], eax

    ; PD[1] -> 2MiB @0x200000 (huge, user)
    mov eax, 0x200000
    or eax, (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_HUGE)
    mov [edi + 8], eax

    ; ===========================
    ; Habilitar PAE y Long Mode
    ; ===========================
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov eax, boot_pml4
    mov cr3, eax

    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr

    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    ; Cargar GDT 64-bit y saltar
    lgdt [gdt64.pointer]
    jmp 0x08:long_mode_start

.no_long_mode:
    mov dword [0xB8000], 0x4F524F45 ; "ER"
    mov dword [0xB8004], 0x4F214F52 ; "R!"
.halt32:
    hlt
    jmp .halt32

; ===========================
; Código 64-bit
; ===========================
bits 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, kernel_stack_top
    xor rbp, rbp

    ; RDI ya contiene multiboot_info (0-extend de EDI)

    call kernel_main

.halt64:
    cli
    hlt
    jmp .halt64

; ===========================
; GDT 64-bit
; ===========================
section .rodata
align 16

gdt64:
    dq 0                                ; 0x00 null
    dq (1<<43)|(1<<44)|(1<<47)|(1<<53)  ; 0x08 kernel code
    dq (1<<44)|(1<<47)|(1<<41)          ; 0x10 kernel data
    dq (1<<44)|(1<<47)|(1<<41)|(3<<45)  ; 0x18 user data (DPL=3)
    dq (1<<43)|(1<<44)|(1<<47)|(1<<53)|(3<<45) ; 0x20 user code (DPL=3)
    dq 0                                ; 0x28 TSS low
    dq 0                                ; 0x30 TSS high
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

; ===========================
; Tablas de página y stack
; ===========================
section .bss
align 4096
boot_pml4:  resb 4096
boot_pdpt:  resb 4096
boot_pd:    resb 4096

align 16
kernel_stack_bottom: resb 16384
kernel_stack_top: