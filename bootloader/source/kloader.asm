;*******************************************************************************
;
;  nTh stage bootloader, located in the root directory
;  of the partition, in a file called BOOT.SYS
;
;*******************************************************************************
%include "fat12.inc"

org 0x600
bits 16                 ; We're in real mode

[map all build/kloader.map]

jump_start:
jmp 0:0x7C00 + code_start - 0x600

LOADED_VBR              EQU 0x1000
VBR_SIZE                EQU 0x200
LOADED_ROOTDIR          EQU LOADED_VBR + VBR_SIZE
KERNEL_LOAD_ADDR        EQU 0x7C00
MEM_MAP_ADDR            EQU (KERNEL_LOAD_ADDR - (128 * 24))

KERNEL_STACK_START_FLAT EQU 0x7FFFF

variables:
    .fatStart            dd     0
    .rootDirectoryStart  dd     0
    .dataRegionStart     dd     0
    .memMapEntries       dd     0
.endvariables:

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

; Relocate
code_start:
mov cx, code_end - jump_start
xor ax, ax
mov es, ax
mov ds, ax
mov si, 0x7C00
mov di, 0x600
rep movsb

; Far Jump to Relocated Code
jmp 0:loader

version  dw 0x0001
kernel_name db "KERNEL  BIN" ; DO NOT EDIT, 11 chars

msg_pre db "BOOT.SYS Loading...", 0x0D, 0x0A, 0
msg_kernel_found db "KERNEL.BIN FOUND", 0x0D, 0x0A, 0
msg_kernel_notfound db "KERNEL.BIN NOT FOUND", 0x0D, 0x0A, 0
msg_a20_interrupt_failed db "A20 via Int 0x15 failed. Halting.", 0x0D, 0x0A, 0

; Block read package tp send  to int13
readPacket:
                        db 0x10     ; Packet size (bytes)
                        db 0        ; Reserved
    readPacketNumBlocks dw 1        ; Blocks to read
    readPacketBuffer    dw 0x1000   ; Buffer to read to
    readPacketSegment   dw 0x0000   ; Segment
    readPacketLBA       dd 0        ; LBA to read from
                        dd 0        ; Extra storage for LBAs > 4 bytes

; Assumes address to string is in ds:si
print:
    lodsb
    or al, al
    jz .print_done
    mov ah, 0eh
    int 10h
    jmp print
.print_done:
    ret

loader:

.printPre:
    xor ax, ax                      ; DS:SI is the address of the message, clear ES
    mov es, ax
    mov si, msg_pre
    call print

.tryReset:
    mov ah, 0                       ; Reset floppy/hdd function
    int 0x13                        ; Call
    jc .tryReset                    ; Try again if carry flag is set

.findKernel:

	; TODO: Load MBR from first sector of disc

	;
	; Step 0: Load MBR into memory
	;
	mov word [readPacketBuffer], LOADED_VBR
	mov dword [readPacketLBA], 0
	mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80 ; Should probably come from Multiboot header info struct?
    int 0x13

    ; Find LBA of first sector
	mov ebx, LOADED_VBR + 0x1BE + (0 * 0x10) ; 0 is active part number, get from Multiboot
	mov ebx, [ebx + 0x8] ; Store LBA of first sector in EBX

	;
	; Step 1: Load VBR from active partition
	;
	mov dword [readPacketLBA], ebx
	mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80 ; Should probably come from Multiboot header info struct?
    int 0x13

    ;
    ; calculate the offset to the first FAT
    ; from the root of the drive
    ;

    ; Get the number of hidden sectors into eax
    mov eax, [LOADED_VBR + bpb.hiddenSectors]

    ; Reserved sectors are 16-bits, convert to 32-bit
    xor ebx, ebx
    mov bx, word [LOADED_VBR + bpb.reservedSectors]

    ; This is now the start of the first fat
    ; in sectors
    add ebx, eax

    ; We're going to need this value later on
    mov [variables.fatStart], ebx

    ;
    ; Calculate the offset to the root directory
    ; from the first FAT
    ;

    ; get the number of fats
    xor eax, eax
    mov al, [LOADED_VBR + bpb.fatCount]

    xor ecx, ecx
    mov cx, [LOADED_VBR + bpb.sectorsPerFat]

    mul ecx
    ;
    ; get the sector offset to the root directory
    ; from the start of the drive
    ;
    add eax, ebx

    ; we're going to need this value later on
    mov [variables.rootDirectoryStart], eax

    ;
    ; read the root sector into memory
    ;
    mov word [readPacketNumBlocks], 4 ; Assume 4 sector root directory
    mov [readPacketLBA], eax
    mov word [readPacketBuffer], LOADED_ROOTDIR ; Store it right after where we loaded the VBR

    ; Get the BIOS to read the sectors
    mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80
    int 0x13

    ; NOTE: LOADED_VBR is now fucked, use no more

    ;
    ; Find kloader (BOOT.SYS) in the root dir
    ;
    mov ax, LOADED_ROOTDIR ; Start addr of rootdir entries

.compareDirEntry:

    mov si, kernel_name
    mov cx, 8 + 3 ; File names are 8 chars long
    mov di, ax
    repe cmpsb ; Compare string while equal
    je kernelFound

    .compareNextDirEntry:
        cmp ax, LOADED_ROOTDIR + 0x800 ; Make sure we don't go past 4 sectors worth of root dir
        jge kernelNotFound

        add ax, 0x20 ; go to next entry
        jmp .compareDirEntry

kernelNotFound:
    xor ax, ax
    mov ds, ax
    mov si, msg_kernel_notfound
    call print

    jmp halt

kernelFound:
    mov bx, ax ; make sure we don't trash it

    xor ax, ax
    mov ds, ax
    mov si, msg_kernel_found
    call print

.loadKernel:

;  DATA Region = ROOT_DIR_START +
;                ROOT_DIR_SIZE

    ; Calculate the number of sectors in
    ; the root directory - each entry is 32-bytes
    xor eax, eax
    mov ecx, eax
    mov ax, [LOADED_VBR+bpb.dirEntryCount]

    mov cl, 32
    mul ecx
    mov ecx, 512
    div ecx

    add eax, [variables.rootDirectoryStart]
    mov [variables.dataRegionStart], eax

; FILE LBA = DATA Region +
;            ({CLUSTER_NUMBER} * SECTORS_PER_CLUSTER);

    xor eax, eax
    mov ax, [bx+dir_entry.startCluster]
    sub eax, 2

    xor ecx, ecx
    mov cl, [LOADED_VBR+bpb.sectorsPerCluster]

    mul ecx

    ; eax = relative lba
    ; result = file absolute lba
    add eax, [variables.dataRegionStart]

    ; EAX Now contains the LBA of KERNEL.BIN

    ;
    ; TODO
    ;   This currently just loads 10240 bytes, assuming the size of the kernel
    ;   does not exceed this. We keep hitting the limit and incrementing it.
    ;   We'll get around to actually just reading the file size one of these days I'm sure..
    kernel_physical_sector_count    EQU 40
    kernel_virtual_sector_count     EQU 80
    kernel_virtual_dword_count      EQU (kernel_virtual_sector_count * 512 / 4)

    push eax
    push ecx
    push edi
    push es

    mov  ax, 0x7C0
    mov  es,  ax
    mov  edi, 0x0

    xor  eax, eax
    mov  ecx, kernel_virtual_dword_count
    rep stosd

    pop  es
    pop  edi
    pop  ecx
    pop  eax

    mov word [readPacketNumBlocks], kernel_physical_sector_count
    mov [readPacketLBA], eax

    ; Read kernel to a familiar place
    mov word [readPacketSegment], 0x7C0
    mov word [readPacketBuffer], 0

    ; Get the BIOS to read the sectors
    mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80
    int 0x13

    ; Before we switch into protected mode,
    ; we want to make sure that we've got the
    ; right video mode
    mov ah, 0x00        ; set video mode
    mov al, 0x03        ; vga 80x25, 16 colours
    int 0x10

    ;
    ; Kernel is now loaded to 0x7C00
    ;

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

detect_memory:

    ; TODO: ES:DI to address of buffer we want to use
    mov edi, MEM_MAP_ADDR
    xor ebx, ebx
    mov edx, 0x534D4150
    mov ecx, 0x18
    mov eax, 0xE820 ; Query System Memory Map
    int 0x15

    ; These only need checking for the zeroeth pass
    jc .done
    cmp eax, 0x534D4150
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

        inc edx ; This is the entry count

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
	    xor ax, ax                      ; DS:SI is the address of the message, clear ES
    	mov es, ax
    	mov si, msg_a20_interrupt_failed
    	call print
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

    ; Jump to the kernel
    jmp 0x08:0x7c00

; Note: This label is used to calculate the size of the binary! :-)
code_end:

; NASM Syntax
; vim: ft=nasm expandtab
