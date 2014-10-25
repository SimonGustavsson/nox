%include "fat12.inc"

org 0x6000
bits 16                 ; We're in real mode

[map all vbr.map]

localBPB: istruc ebpb_fat1216

    ; First three bytes of the BPB are a short JMP instruction
    ; to the boot code
    jmp (start - 0x6000 + 0x7C00)

iend

start: 

; Relocate
mov cx, 0x200
xor ax, ax
mov es, ax
mov ds, ax
mov si, 0x7C00
mov di, 0x6000
rep movsb

; Far Jump to Relocated Code
jmp 0:loader 

version  dw 0x0001
msg_pre  db "VBR PRE0", 0x0D, 0x0A, 0
msg_post db "VBR POS0", 0x0D, 0x0A, 0

; Block read package tp send  to int13
readPacket:
    db 0x10                         ; Packet size (bytes)
    db 0                            ; Reserved
    readPacketNumBlocks dw 1        ; Blocks to read
    readPacketBuffer dw 0x1000      ; Buffer to read to
    dw 0                            ; Memory Page
    readPacketLBA dd 0              ; LBA to read from
    dd 0                            ; Extra storage for LBAs > 4 bytes

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

    ; TODO: Parse root directory 

    ; TODO: Locate STAGE2.SYS

    ; TODO: Load STAGE2.SYS into memory

    ; TODO: Jump to STAGE2.SYS

.printPost:
    xor ax, ax          ; DS:SI is the address of the message, clear ES
    mov ds, ax  
    mov si, msg_post
    call print

.halt:
    cli
    hlt
    ; Calculate root directory

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab
