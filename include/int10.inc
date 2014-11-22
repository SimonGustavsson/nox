; NOTE: this is length-prefixed string where
;       the length is 16-bits long, which
;       gives a maximal string length of
;       65,536 characters
%macro string16 2
	%1 			dw %1_length
	%1_body 	db %2
	%1_length	equ	$ - %1_body
%endmacro

; arguments:
;	eax: addr of the string16
printString16:
	mov esi, eax

	xor ecx, ecx
	lodsw 
	mov cx, ax

	cmp cx, 0
	jz .printDone

    mov ah, 0eh

	.printChar:
    	lodsb
    	int 10h
    	dec cx
    	jz .printDone
    	jmp .printChar    	
	
	.printDone:
	    ret