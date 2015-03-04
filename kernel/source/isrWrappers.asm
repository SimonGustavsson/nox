extern isr_sysCall

; Sys call (0x80)
global isr_sysCallWrapper
align 4
isr_sysCallWrapper:
	pushad

    xchg bx, bx ; Bochs breakpoint

    push eax
	call isr_sysCall

	popad
	iret

; NASM Syntax
; vim: ft=nasm expandtab

