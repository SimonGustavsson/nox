;*******************************************************************************
;
;  nTh stage bootloader, located in the root directory
;  of the partition, in a file called BOOT.SYS
;
;*******************************************************************************

;*******************************************************************************
; Directives
;*******************************************************************************
resb 0x600
bits 16                 ; We're in real mode
;[map all build/kloader.map]

[section .text.boot]

_start:
jmp 0:0x7C00 + relocate_start - 0x600
KERNEL_STACK_START_FLAT     EQU 0x7FFFF
variables:
    .memMapEntries       dd     0
.endvariables:

msg_a20_interrupt_failed    db "A20 via Int 0x15 failed. Halting.", 0x0D, 0x0A, 0

gdt:
	.null:
		dq              0
	.code:
		dw 				0xFFFF 		; limit 0:15
		dw 				0 			; base 0:15
		db 				0 			; base 16:23
		db 				0x9A		; access bytes
		db 				0xCF		; limit & flags
		db 				0 			; base 24:31
	.data:
		dw 				0xFFFF 		; limit 0:15
		dw 				0 			; base 0:15
		db 				0 			; base 16:23
		db 				0x92		; access bytes
		db 				0xCF		; limit & flags
		db 				0 			; base 24:31
.end:

gdtDescriptor:
		dw 				gdt.end - gdt - 1
		dd              gdt
.end:

%include "bootloader/include/int10.inc"
%include "bootloader/include/int13.inc"

;*******************************************************************************
; Relocation
;*******************************************************************************
relocate_start:
    mov cx, code_end - $$
    xor ax, ax
    mov es, ax
    mov ds, ax
    mov si, 0x7C00
    mov di, 0x600
    rep movsb
    
    ; Far Jump to Relocated Code
    jmp 0:main

;*******************************************************************************
; Main
;
; Preconditions:
;   DS = 0
;   ES = 0
;   AX = 0
;*******************************************************************************
main:
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x09FF

    call reset_disk

    ; Reset the segment registers
    xor ax, ax
    mov es, ax
    mov ds, ax

    ; Before we switch into protected mode,
    ; we want to make sure that we've got the
    ; right video mode
    mov ah, 0x00        ; set video mode
    mov al, 0x03        ; vga 80x25, 16 colours
    int 0x10

	; No interrupts past this point for moving
	; into protect mode
	cli

    ; Create a memory map to pass to the kernel
    call detect_memory

	; Enable A20
	call enableA20

	; setup GDT
	lgdt [gdtDescriptor]

	; Enable protected mode
	mov eax, cr0
	or eax, 0x01
	mov cr0, eax

    jmp 0x08:enterProtectedMode

halt:
    cli
    hlt
KERNEL_LOAD_ADDR            EQU 0x7C00
MEM_MAP_ADDR                EQU (KERNEL_LOAD_ADDR - (128 * 24))
detect_memory:


    ; TODO: ES:DI to address of buffer we want to use
    mov edi, MEM_MAP_ADDR
    xor ebx, ebx
    mov edx, 'PAMS' ; 'SMAP' reversed
    mov ecx, 0x18
    mov eax, 0xE820 ; Query System Memory Map
    int 0x15

    ; These only need checking for the zeroeth pass
    jc .done
    cmp eax, 'PAMS'; 'SMAP' reversed
    jne .done

    .read_entry:
        ; NOTE: don't touch EBX - We need it for the next INT

        ; CL contains entry size
        cmp cl, 20
        jne .twenty_four_byte_entry
        .twenty_byte_entry:
            mov dword [ds:edi + 16], 1
        .twenty_four_byte_entry:

        mov eax, [variables.memMapEntries]
        inc eax
        mov [variables.memMapEntries], eax

        ; Some BIOSes, set ebx to 0, /on/
        ; the last entry, in which case we
        ; should not enumerate further
        test ebx, ebx
        jz .done

        ; Advance to the buffer address entry
        add edi, 24

        ; Query next entry in System Memory map
        mov eax, 0xE820
        mov ecx, 0x18
        int 0x15

        ; If the carry flag is set, it means
        ; that ebx on the /last/ pass was non-zero
        ; but, we didn't get an entry on this pass
        ; so we need to, forget whatever we got
        ; on this pass, and stop enumerating
        jc .done

        ; All good, read next entry
        jmp .read_entry

    .done:
        ret

    ; Entries are stored in ES:DI
    ; Entry format:
    ;   uin64_t  - Base address
    ;   uint64_t - "Length of region" (ignore 0 values)
    ;   uint32_t - Region type
    ;       Type 1: Usable (normal) RAM
    ;       Type 2: Reserved
    ;       Type 3: ACPI reclaimable memory
    ;       Type 4: ACPI NVS memory
    ;       Type 5: Bad memory
    ;   uint32_t - Ext attr (if 24 bytes are returned)
    ;       0 : ignore entry if 0
    ;       1 : Non-volatile
    ;    31:2 : Unused

    ret

enableA20:
	mov ax, 0x2401
	int 0x15
	jc	.fail
	ret

	.fail:
    	mov si, msg_a20_interrupt_failed
    	call print_string_z
		jmp halt

; THIS MUST GO LAST IN THE FILE BECAUSE IT CHANGES
; CODE GENERATION TO USE 32-bit INSTRUCTIONS
enterProtectedMode:

    ; We're in protected mode now, make sure nasm spits
    ; out 32-bit wide instructions, or very, very, VERY bad things happen!
    bits 32

    mov edx, 0x10
    mov ds, edx
    mov es, edx
    mov fs, edx
    mov gs, edx
    mov ss, edx

    ; Setup stack pointer
    mov esp, KERNEL_STACK_START_FLAT

    ; NOTE: We leave interrupts disabled - the Kernel can
    ; re-enable them when it thinks it is ready for them

    ; Tell the kernel how many entries there are
    push dword [variables.memMapEntries]
    push dword MEM_MAP_ADDR

    ; Push a dummy return address on to the stack
    ; so that C can find its parameters, also, if it
    ; attempts to ret, a GPF will occur
    push dword 0

extern kloader_cmain

    ; Jump to the kernel
    jmp 0x08:kloader_cmain

; Note: This label is used to calculate the size of the binary! :-)
code_end:

; NASM Syntax
; vim: ft=nasm expandtab
