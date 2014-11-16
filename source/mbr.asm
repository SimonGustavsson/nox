%include "fat12.inc"

; NOTE: BIOS will load us to either 0x07C0:0, 
; or, 0:0x7C00. We relocate to 0:0x600 immediately

org 0x600               ; We will relocate us here
bits 16                 ; We're in real mode
[map all build/mbr.map] ; Debug information

; Address we will read the VBR to
VBR_ADDRESS                         EQU 0x7C00
VBR_OFFSET_RESERVED_SECTOR_COUNT    EQU 0x0E

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

version  dw 0x0001
msg_pre  db "MBR - Locating active partition...", 0x0D, 0x0A, 0

; Block read package to send to int13
readPacket:
                        db 0x10        ; Packet size (bytes)
                        db 0           ; Reserved
    readPacketNumBlocks dw 1           ; Blocks to read
    readPacketBuffer    dw VBR_ADDRESS ; Buffer to read to
                        dw 0           ; Memory Page
    readPacketLBA       dd 0           ; LBA to read from
                        dd 0           ; Extra storage for LBAs > 4 bytes

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0eh
    int 10h
    jmp print

    .done:
        ret

printBoundedString:
    dec cx
    jz .print_done
    lodsb
    mov ah, 0eh
    int 10h
    jmp printBoundedString
.print_done:
    ret

loader: 

.printPre:
    xor ax, ax                      ; ES:SI is the address of the message, clear ES
    mov es, ax  
    mov si, msg_pre
    call print

.tryReset:
    mov ah, 0                       ; Reset floppy function
    int 0x13                        ; Call
    jc .tryReset                    ; Try again if carry flag is set

.readVBR:
    ; DL is the drive number, BIOS will have set it to the boot drive,
    ; which in our case ought to be 0x80

    xor bx, bx
    mov es, bx                      ; Zero the destination segment register, offset in BX

    ; Read the VBR from the first partition
    ; First prepare the package
    mov word ax, [start + mbr.part0 + part_entry.lbaLo]
    mov word [readPacketLBA], ax
    mov word ax, [start + mbr.part0 + part_entry.lbaHi]
    mov word [readPacketLBA + 2], ax

    ; Up to you, big man!
    mov si, readPacket
    mov ah, 0x42
    int 0x13

    ; The VBR is now loaded at 0x1000
    mov ax, [VBR_ADDRESS + VBR_OFFSET_RESERVED_SECTOR_COUNT]

    ; Jump to the VBR
    jmp 0:VBR_ADDRESS

.halt:
    cli
    hlt
    ; Calculate root directory

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab
