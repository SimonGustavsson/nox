%macro interrupt_dispatcher_0 1
    global %1_dispatcher
    extern %1
    align 4

    %1_dispatcher:
        pushad

        call %1

        popad
        iret
%endmacro

%macro interrupt_dispatcher_1 1
    global %1_dispatcher
    extern %1
    align 4

    %1_dispatcher:
        pushad

        push eax
        call %1
        add esp, 4

        popad
        iret
%endmacro

interrupt_dispatcher_1 isr_sys_call
interrupt_dispatcher_0 isr_timer
interrupt_dispatcher_0 isr_keyboard
interrupt_dispatcher_0 isr_unknown

; NASM Syntax
; vim: ft=nasm expandtab

