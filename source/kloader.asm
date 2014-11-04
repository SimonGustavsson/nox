; ***************************************************
;
;  nTh stage bootloader, located in the root directory
;  of the partition, in a file called BOOT.SYS
;
;****************************************************

%include "fat12.inc"

org 0x600
bits 16                 ; We're in real mode

[map all build/kloader.map]

jmp 0:0x7C00 + start - 0x600

LOADED_VBR              EQU 0x1000
VBR_SIZE                EQU 0x200
LOADED_ROOTDIR          EQU LOADED_VBR + VBR_SIZE

variables:
    .fatStart            dd     0
    .rootDirectoryStart  dd     0
    .dataRegionStart     dd     0
.endvariables:

; Relocate
start: 
mov cx, 0x200 ; TODO Needs to be size of BOOT.SYS
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

; Block read package tp send  to int13
readPacket:
    db 0x10                         ; Packet size (bytes)
    db 0                            ; Reserved
    readPacketNumBlocks dw 1        ; Blocks to read
    readPacketBuffer dw 0x1000      ; Buffer to read to
    dw 0                            ; Memory Page
    readPacketLBA dd 0              ; LBA to read from
    dd 0                            ; Extra storage for LBAs > 4 bytes

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
	mov dword [readPacketLBA], 0
	mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80 ; Should probably come from Multiboot header info struct?
    int 0x13

    ; Find LBA of first sector
	mov ebx, [readPacketBuffer + 0x1BE + (0 * 0x10)] ; 0 is active part number, get from Multiboot
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
    mov ebx, [LOADED_VBR + bpb.reservedSectors]

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
    mov word [readPacketNumBlocks], 4 ; TODO: Sectors per cluster here
    mov [readPacketLBA], eax

    ; Read kernel to a familiar place
    mov dword [readPacketBuffer], 0x7c00    
    
    ; Get the BIOS to read the sectors
    mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80
    int 0x13

    ; k kernel, go baby, go!
    jmp 0:0x7c00

halt:
    cli
    hlt

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab
