
; ***************************************************
;
;  Second stage bootloader, located on the first partition
;  of the first drive, in a file called STAGE2.SYS
;
;****************************************************

org 0x7c00	; offset to 0, we will set segments later 
bits 16		; we are still in real mode 
jmp main	; jump to main 

msg_welcome  db "Stage 2 bootloader...", 13, 10, 0

print:
	lodsb		; load next byte from string from SI to AL
	or al, al	; Does AL = 0?
	jz .done	; Null-terminator, we're done here
	mov ah, 0eh	; Print character 
	int 10h
	jmp print	; Loop until AL = 0

	.done:
			ret		; we are done, so return
 
main:
	cli			; clear interrupts
	push	cs	; Ensure DS=CS
	pop ds

	mov si, msg_welcome
	call print

	cli		; clear interrupts to prevent triple faults
	hlt		; halt the system
 