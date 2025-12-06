;; multiboot2.asm - Header Multiboot2 para GRUB

section .multiboot2
align 8

MAGIC           equ 0xE85250D6
ARCH_X86        equ 0
HEADER_LENGTH   equ multiboot_end - multiboot_start

multiboot_start:
    dd MAGIC
    dd ARCH_X86
    dd HEADER_LENGTH
    dd -(MAGIC + ARCH_X86 + HEADER_LENGTH) & 0xFFFFFFFF

    ; Tag de fin
    align 8
    dw 0
    dw 0
    dd 8

multiboot_end: