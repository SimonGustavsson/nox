section .text
;global _start:function (_start.end - _start)
global _start
_start:
    jmp multiboot_entry

section .multiboot
multiboot_header:
align 8
	dd 0xE85250D6
	dd 0 ; x86
    dd (multiboot_header_end - multiboot_header)
	dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))
    dd 0x100000000 - (0xe85250d6 + 0 + (multiboot_header_end - multiboot_header))
address_entry_tag:
    dw 3    ; MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    dw 1    ; OPTIONAL
    dd (address_entry_tag_end - address_entry_tag)    ; size
    dd multiboot_entry
address_entry_tag_end:
end_tag: ; required end tag
dw 0    ; MULTIBOOT_HEADER_TAG_END
dw 0    ; flags
dd 8    ; size
end_tag_end:

multiboot_header_end:

section .bss
align 16
stack_bottom:
resb 16384 ; 16 KiB
stack_top:

section .text
multiboot_entry:
	mov esp, stack_top

    push ebx
    push eax

	extern cmain
	call cmain

	cli
.hang:	hlt
	jmp .hang
;.end

