extern isr_sysCall
extern isr_timer
extern isr_keyboard
extern isr_unknown

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
  pushad

  call isr_timer

  popad
  iret

global isr_keyboardWrapper
align 4
isr_keyboardWrapper:
  pushad

  call isr_keyboard

  popad
  iret

global isr_unknownWrapper
align 4
isr_unknownWrapper:
  pushad

  call isr_unknown

  popad
  iret

; NASM Syntax
; vim: ft=nasm expandtab

