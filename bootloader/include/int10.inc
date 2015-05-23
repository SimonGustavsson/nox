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
print_string_16:
	mov esi, eax

	xor ecx, ecx
	lodsw
	mov cx, ax

	cmp cx, 0
	jz .done

    mov ah, 0eh

	.print_char:
    	lodsb
    	int 10h
    	dec cx
    	jz .done
    	jmp .print_char

	.done:
	    ret

; arguments
;	eax: addr of the string
;	ecx: count of bytes to print
print_string_n:

	cmp ecx, 0
	je .done

	mov esi, eax

    mov ah, 0eh

    .print_char:
    	lodsb
    	int 10h
    	dec ecx
		jnz .print_char

	.done:
	    ret

; arguments
;	[ds:si]: addr of the string
print_string_z:

    mov ah, 0eh

    .print_char:
        lodsb

        or al, al
        jz .done

        int 10h

        jmp .print_char

    .done:
        ret

; NASM Syntax
; vim: ft=nasm expandtab
