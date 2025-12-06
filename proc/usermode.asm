; =============================================================================
; usermode.asm - Rutina de salto final al modo de usuario (Ring 3)
;
; El 'context_switch' retorna a esta función. El RSP apunta al marco IRETQ.
; =============================================================================

section .text
bits 64

global usermode_entry

usermode_entry:
    ; IRETQ toma los 5 valores del stack (SS, RSP, RFLAGS, CS, RIP) y realiza
    ; la transición de privilegio (Ring 0 -> Ring 3).
    iretq