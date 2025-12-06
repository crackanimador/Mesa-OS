; =============================================================================
; switch.asm - Context Switch para multitarea en Ring 0 (CORREGIDO)
;
; void context_switch(uint64_t *old_rsp, uint64_t new_rsp, uint64_t new_cr3)
;
; RDI = &old_rsp
; RSI = new_rsp
; RDX = new_cr3 (Tabla de Páginas)
; =============================================================================

section .text
bits 64

global context_switch

context_switch:
    ;; Deshabilitar interrupciones para atomicidad
    cli

    ;; Guardar registros callee-saved en el stack actual
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ;; Guardar RSP actual en el PCB del proceso viejo
    mov [rdi], rsp

    ;; ---------------------------------------------------------------------
    ;; CRÍTICO: Cargar la Tabla de Páginas del nuevo proceso (new_cr3)
    mov cr3, rdx
    ;; ---------------------------------------------------------------------

    ;; Cargar RSP del proceso nuevo
    mov rsp, rsi

    ;; Restaurar registros callee-saved del proceso nuevo
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ;; Habilitar interrupciones
    sti

    ;; RET salta al RIP guardado en el stack del nuevo proceso (usermode_entry)
    ret