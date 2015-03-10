extern isr_sysCall
extern PIT_HandleIRQ0

; Sys call (0x80)
global isr_sysCallWrapper
align 4
isr_sysCallWrapper:
	pushad

    push eax
	call isr_sysCall

	popad
	iret

global isr_timerWrapper
align 4
isr_timerWrapper:
    xchg bx, bx
    pushad


    push eax
    call PIT_HandleIRQ0

    popad
    iret

; NASM Syntax
; vim: ft=nasm expandtab

