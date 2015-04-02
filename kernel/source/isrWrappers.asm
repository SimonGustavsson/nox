%macro interrupt_dispatcher 1
    global %1_dispatcher
    extern %1
    align 4

    %1_dispatcher:
        pushad
        call %1
        popad
        iret
%endmacro

interrupt_dispatcher isr_sys_call
interrupt_dispatcher isr_timer
interrupt_dispatcher isr_keyboard
interrupt_dispatcher isr_unknown

; NASM Syntax
; vim: ft=nasm expandtab

