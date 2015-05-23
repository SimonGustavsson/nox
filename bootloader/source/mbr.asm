%include "mbr.inc"
%include "fat12.inc"

; NOTE: BIOS will load us to either 0x07C0:0,
; or, 0:0x7C00. We relocate to 0:0x600 immediately

org 0x600               ; We will relocate us here
bits 16                 ; We're in real mode
[map all build/mbr.map] ; Debug information

; Address we will read the VBR to
VBR_ADDRESS                         EQU 0x7C00

start:

    ; Relocate
    mov cx, 0x200
    xor ax, ax
    mov es, ax
    mov ds, ax
    mov si, 0x7C00
    mov di, 0x0600
    rep movsb

    ; Far Jump to Relocated Code
    jmp 0:loader

%include "int10.inc"
string16 messageLoading, {"Locating active partition...", 0x0D, 0x0A}
string16 messageNoActive, {"No active partition...", 0x0D, 0x0A}

; Block read package to send to int13
readPacket:
                        db 0x10        ; Packet size (bytes)
                        db 0           ; Reserved
    readPacketNumBlocks dw 1           ; Blocks to read
    readPacketBuffer    dw VBR_ADDRESS ; Buffer to read to
                        dw 0           ; Memory Page
    readPacketLBA       dd 0           ; LBA to read from
                        dd 0           ; Extra storage for LBAs > 4 bytes

loader:

    ; setup segments
    xor ax, ax
    mov es, ax
    mov ds, ax

    ; need a stack, grow downwards from
    ; the boot location gives us plenty
    ; of space
    mov ax, 0x7B00
    mov ss, ax

.printPre:
    mov eax, messageLoading
    call print_string_16

.tryReset:
    mov ah, 0                       ; Reset floppy function
    int 0x13                        ; Call
    jc .tryReset                    ; Try again if carry flag is set

.readVBR:

    ; Read the VBR from the first partition
    ; First prepare the package

    mov ecx, 4 ; 4 Partition entries
    mov ebx, start + struc_mbr.part0

    .readPartitionEntries:

        mov al, byte [ebx + struc_mbr_part.status]
        cmp al, 0x80
        je .found

        dec ecx
        jz .notFound

        add ebx, struc_mbr_part_size
        jmp .readPartitionEntries

    .found:
        mov word ax, [ebx + struc_mbr_part.lba_low]
        mov word [readPacketLBA], ax
        mov word ax, [ebx + struc_mbr_part.lba_high]
        mov word [readPacketLBA + 2], ax

        ; Note: DL is the drive number, BIOS will have set it to the boot drive,
        ; which in our case ought to be 0x80
        ; Up to you, big man!
        mov si, readPacket
        mov ah, 0x42
        int 0x13

        ; Jump to the VBR
        jmp 0:VBR_ADDRESS

    .notFound:
        mov eax, messageNoActive
        call print_string_16
        call halt

halt:
    cli
    hlt

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab
