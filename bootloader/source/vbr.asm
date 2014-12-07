%include "fat12.inc"

org 0x600
bits 16                 ; We're in real mode

[map all build/vbr.map]


localBPB: istruc ebpb_fat1216

    ; First three bytes of the BPB are a short JMP instruction
    ; to the boot code
    jmp (start - 0x600 + 0x7C00)

iend

variables:
    .fatStart            dd     0
    .rootDirectoryStart  dd     0
    .dataRegionStart     dd     0
start: 

; Relocate
mov cx, 0x200
xor ax, ax
mov es, ax
mov ds, ax
mov si, 0x7C00
mov di, 0x600
rep movsb

; Far Jump to Relocated Code
jmp 0:loader 

version  dw 0x0001
kloader_name db "BOOT    SYS" ; DO NOT EDIT, 11 chars

msg_pre  db "VBR Loading...", 0x0D, 0x0A, 0
msg_kloader_found db "BOOT.SYS FOUND", 0x0D, 0x0A, 0
msg_kloader_notfound db "BOOT.SYS NOT FOUND", 0x0D, 0x0A, 0

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
    mov ah, 0                       ; Reset floppy function
    int 0x13                        ; Call
    jc .tryReset                    ; Try again if carry flag is set

.findKernelLoader:

    ;
    ; calculate the offset to the first FAT
    ; from the root of the drive
    ;

    ; Get the number of hidden sectors into ax    
    mov eax, [localBPB+bpb.hiddenSectors]

    ; reserved sectors are 16-bits, convert to 32-bit
    xor ebx, ebx
    mov bx, [localBPB+bpb.reservedSectors]

    ; this is now the start of the first fat
    ; in sectors
    add ebx, eax

    ; we're going to need this value later on
    mov [variables.fatStart], ebx

    ;
    ; calculate the offset to the root directory
    ; from the first FAT
    ;

    ; get the number of fats
    xor eax, eax
    mov al, [localBPB+bpb.fatCount]

    xor ecx, ecx
    mov cx, [localBPB+bpb.sectorsPerFat]

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

    ; Get the BIOS to read the sectors
    mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80
    int 0x13

    ;
    ; Find kloader (BOOT.SYS) in the root dir
    ;
    mov ax, 0x1000 ; Start addr of rootdir entries

.compareDirEntry:
    mov si, kloader_name
    mov cx, 8 + 3 ; File names are 8 chars long
    mov di, ax
    repe cmpsb ; Compare string while equal
    je kloaderFound

    .compareNextDirEntry:
        cmp ax, 0x1000 + 0x800
        jge kloaderNotFound

        add ax, 0x20 ; go to next entry
        jmp .compareDirEntry

kloaderNotFound:
    xor ax, ax
    mov ds, ax
    mov si, msg_kloader_notfound
    call print

    jmp halt

kloaderFound:
    mov bx, ax ; make sure we don't trash it

    xor ax, ax
    mov ds, ax
    mov si, msg_kloader_found
    call print

.loadKloader:

;  DATA Region = ROOT_DIR_START +
;                ROOT_DIR_SIZE

    ; Calculate the number of sectors in
    ; the root directory - each entry is 32-bytes
    xor eax, eax
    mov ecx, eax
    mov ax, [localBPB+bpb.dirEntryCount]
    
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
    mov cl, [localBPB+bpb.sectorsPerCluster]

    mul ecx

    ; eax = relative lba
    ; result = file absolute lba
    add eax, [variables.dataRegionStart]
    
    ; EAX Now contains the LBA of BOOT.SYS
    mov word [readPacketNumBlocks], 4 ; TODO: Sectors per cluster here
    mov [readPacketLBA], eax

    ; Read kloader to a familiar place
    mov dword [readPacketBuffer], 0x7c00    
    
    ; Get the BIOS to read the sectors
    mov si, readPacket
    mov ah, 0x42
    mov dl, 0x80
    int 0x13

    ; k kloader, up to you, dawg.
    jmp 0:0x7c00

halt:
    cli
    hlt
    ; Calculate root directory

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab