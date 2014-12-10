
; Sys call (0x80)
global isr_sysCallWrapper
align 4
isr_sysCallWrapper:
	pushad

	xchg bx, bx ; Bochs breakpoint

	call isr_sysCallWrapper

	popad
	iret