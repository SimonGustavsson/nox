
; ***************************************************
;
;  nTh stage bootloader, located in the root directory
;  of the partition, in a file called BOOT.SYS
;
;****************************************************

org 0x7c00	; The nTh-1 loader will load us here 
bits 16		; we are still in real mode :(
jmp main	; Get to da code!

msg_welcome  db "Nox - Loading kernel...", 13, 10, 0

print:
	lodsb		; load next byte from string from SI to AL
	or al, al	; Does AL = 0?
	jz .done	; Null-terminator, we're done here
	mov ah, 0eh	; Print character 
	int 10h
	jmp print	; Loop until AL = 0

	.done:
			ret
 
main:
	cli			; clear interrupts
	push cs	    ; Ensure DS=CS
	pop ds

	mov si, msg_welcome
	call print

	cli		; clear interrupts to prevent triple faults
	hlt		; halt the system
 