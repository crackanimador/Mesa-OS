; =============================================================================
; syscall_entry.asm - Entry point para la instrucción SYSCALL
;
; Estado al entrar (Long mode):
;   RCX = RIP de "usuario" (dirección de retorno)
;   R11 = RFLAGS de "usuario"
;   RSP = stack "usuario" (no cambia automáticamente)
;   RAX = número de syscall
;   RDI, RSI, RDX, R10, R8, R9 = argumentos
;
; Usamos GSBASE/KERNEL_GSBASE para acceder a cpu_data_t:
;   [gs:0]  = kernel_rsp
;   [gs:8]  = user_rsp
; =============================================================================

section .text
bits 64

global syscall_entry
extern syscall_handler

syscall_entry:
    ; Cambiar a contexto de kernel: GS <- KERNEL_GSBASE
    swapgs

    ; Guardar RSP del "usuario"
    mov [gs:8], rsp         ; cpu_data.user_rsp = RSP
    mov rsp, [gs:0]         ; RSP = kernel_rsp

    ; Guardar RCX/R11 (RIP/RFLAGS de usuario) para sysretq
    push r11                ; [RSP] = R11
    push rcx                ; [RSP] = RCX

    ; Guardar registros callee-saved de kernel
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; ------------------------------------------------------------
    ; Preparar argumentos para syscall_handler (convención SysV):
    ;   RDI, RSI, RDX, RCX, R8, R9
    ;
    ; Entramos con:
    ;   RAX = num
    ;   RDI, RSI, RDX, R10, R8, R9 = args
    ; ------------------------------------------------------------

    ; Mover R10 -> RCX (arg4)
    mov rcx, r10

    ; Guardar num temporalmente en R10
    mov r10, rax

    ; Reordenar via stack
    push r9             ; posible arg6
    push r8             ; arg5
    push rcx            ; arg4
    push rdx            ; arg3
    push rsi            ; arg2
    push rdi            ; arg1

    mov rdi, r10        ; num
    pop rsi             ; arg1
    pop rdx             ; arg2
    pop rcx             ; arg3
    pop r8              ; arg4
    pop r9              ; arg5
    add rsp, 8          ; descartar posible arg6

    ; Llamar al handler C
    sti
    call syscall_handler
    cli

    ; Restaurar callee-saved
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restaurar RCX (RIP) y R11 (RFLAGS) de usuario
    pop rcx
    pop r11

    ; Restaurar stack original del "usuario"
    mov rsp, [gs:8]

    ; Volver a GS del lado usuario (o neutro)
    swapgs

    ; Volver con sysretq: usa RCX (RIP) y R11 (RFLAGS)
    o64 sysret