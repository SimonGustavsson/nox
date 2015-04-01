extern isr_sys_call
extern isr_timer
extern isr_keyboard
extern isr_unknown

; Sys call (0x80)
global isr_sys_call_wrapper
align 4
isr_sys_call_wrapper:
	pushad

  push eax
	call isr_sys_call

	popad
	iret

global isr_timer_wrapper
align 4
isr_timer_wrapper:
  pushad

  call isr_timer

  popad
  iret

global isr_keyboard_wrapper
align 4
isr_keyboard_wrapper:
  pushad

  call isr_keyboard

  popad
  iret

global isr_unknown_wrapper
align 4
isr_unknown_wrapper:
  pushad

  call isr_unknown

  popad
  iret

; NASM Syntax
; vim: ft=nasm expandtab

