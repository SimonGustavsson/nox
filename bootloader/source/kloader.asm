;*******************************************************************************
;
;  nTh stage bootloader, located in the root directory
;  of the partition, in a file called BOOT.SYS
;
;*******************************************************************************

;*******************************************************************************
; Directives
;*******************************************************************************
org 0x600
bits 16                 ; We're in real mode
[map all build/kloader.map]

;*******************************************************************************
; Defines
;*******************************************************************************
LOADED_VBR                  EQU 0x1000
LOADED_VBR_PARTITION_START  EQU LOADED_VBR + struc_mbr_code_size
VBR_SIZE                    EQU 0x200
KERNEL_LOAD_ADDR            EQU 0x7C00
MEM_MAP_ADDR                EQU (KERNEL_LOAD_ADDR - (128 * 24))
LOADED_ROOTDIR              EQU LOADED_VBR + VBR_SIZE
KERNEL_STACK_START_FLAT     EQU 0x7FFFF

;*******************************************************************************
; Entry Point
;*******************************************************************************
jmp 0:0x7C00 + relocate_start - 0x600

;*******************************************************************************
; Included Code
;*******************************************************************************
%include "fat12.inc"
%include "int10.inc"
%include "int13.inc"
%include "mbr.inc"

;*******************************************************************************
; Constants
;*******************************************************************************
version                     dw 0x0001
kernel_name                 db "KERNEL  BIN" ; DO NOT EDIT, 11 chars

msg_pre                     db "BOOT.SYS Loading...", 0x0D, 0x0A, 0
msg_kernel_found            db "KERNEL.BIN FOUND", 0x0D, 0x0A, 0
msg_kernel_notfound         db "KERNEL.BIN NOT FOUND", 0x0D, 0x0A, 0
msg_a20_interrupt_failed    db "A20 via Int 0x15 failed. Halting.", 0x0D, 0x0A, 0

;*******************************************************************************
; Variables
;*******************************************************************************
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

    mov si, msg_pre
    call print_string_z

    call reset_disk

    ;
    ; Load the MBR so we can get the partition info for the active partition
    ;
    mov eax,    0           ; LBA
    mov dl,     0x80        ; Drive
    mov cx,     0x01        ; Sector Count
    mov di,     LOADED_VBR  ; Destination
    call read_sectors

    ;
    ; Load the VBR so we can get FAT info
    ;

    ; Compute the offset of the active partition entry in the partition table
    xor eax, eax
    mov al, 0                   ; Active partition number
    mov bl, struc_mbr_part_size ; Size of a partition entry
    mul bl                      ; Multiplies al

    mov eax, [LOADED_VBR_PARTITION_START + eax + struc_mbr_part.lba_low]
    call read_sectors

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
    ; LBA is already in EAX
    mov dl,     0x80        ; Drive
    mov cx,     0x04        ; Assuming 4 seconds in root dir
    mov di,     LOADED_ROOTDIR
    call read_sectors

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
    mov si, msg_kernel_notfound
    call print_string_z

    jmp halt

kernelFound:

    ; Keep the location of the entry in bx
    mov bx, ax

    mov si, msg_kernel_found
    call print_string_z

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
    kernel_physical_sector_count    EQU 110
    kernel_virtual_sector_count     EQU 110
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

    ; Read kernel to a familiar place
    ;   EAX already contains the LBA
    mov dl,     0x80        ; Drive
    mov cx,     kernel_physical_sector_count
    mov di,     0x07C0
    mov es,     di          ; Destination Segment
    mov di,     0x0000      ; Destination
    call read_sectors

    ;
    ; Kernel is now loaded to 0x7C00
    ;

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

    ; Jump to the kernel
    jmp 0x08:0x7c00

; Note: This label is used to calculate the size of the binary! :-)
code_end:

; NASM Syntax
; vim: ft=nasm expandtab
