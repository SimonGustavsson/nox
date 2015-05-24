section .text
%include "fat16.inc"

; ================================================================================
; Compile time constants
; ================================================================================
VBR_RUN_ADDRESS            EQU 0x600
VBR_LOAD_ADDRESS           EQU 0x7C00
BOOTLOADER_LOAD_ADDRESS    EQU VBR_LOAD_ADDRESS

; Temporary storage for the current FAT sector
; we're reading whilst following cluster chains
TEMP_FAT_SECTOR_ADDRESS    EQU (VBR_RUN_ADDRESS + 0x200)

DIR_ENTRY_SIZE             EQU 0x20

; ================================================================================
; NASM Directives
; ================================================================================
org VBR_RUN_ADDRESS
bits 16                  ; We're in real mode
[map all build/vbr.map] ; Create map for debugging

; ================================================================================
; Jump instruction
; ================================================================================
; note: Our makefile will fill in Boot Sector for us
; And blit in our code where appropriate, all we have to do
; is supply the jump instruction and the code, simples!
this_bpb: istruc bpb_fat16

    ; First three bytes of the BIOS Parameter Block
    ; is a jump instruction to the code section of the VBR
    jmp code
iend

; ================================================================================
; Variables
; ================================================================================
bootloader_name             db "BOOT    SYS" ; DO NOT EDIT, 11 character
msg_welcome                 db "VBR Loading..."
bootloader_found            db "BOOT.SYS found", 0x0D, 0x0A, 0
bootloader_error            db "BOOT.SYS not found", 0x0D, 0x0A, 0

; Structure for INT 0x13 (0x42) for reading blocks
; From the hard drive
read_packet:
                            db 0x10   ; Packet size (bytes)
                            db 0      ; Reserved
    read_packet_num_blocks  dw 1      ; Blocks to read
    read_packet_buffer      dw 0x1000 ; Buffer address
                            dw 0      ; Memory page
    read_packet_lba         dd 0      ; LBA to read
                            dd 0      ; Extra storage for LBAs > 4 bytes

variables:
    .root_directory_sector  dd 0      ; Sector of root directory
    .fat0_start_sector      dd 0     ; First sector of FAT0
    .last_loaded_fat_sector dd 0     ; Last FAT sector loaded whilst following chain

; ================================================================================
; Start - Relocation
; ================================================================================
code:

    ; Relocate ourselves so we can load the bootloader to the address
    ; we were loaded to, ensuring the same load address for all stages of the boot
    mov cx, 0x200                   ; Relocate 512 bytes
    xor ax, ax
    mov es, ax
    mov ds, ax
    mov si, VBR_LOAD_ADDRESS        ; From
    mov di, VBR_RUN_ADDRESS         ; To
    rep movsb
    
    jmp 0:relocated                 ; Far jump to relocated code

    relocated:

        ; We're now at VBR_RUN_ADDRESS

        ; Print a message to show relocation worked!
        xor ax, ax
        mov es, ax
        mov si, msg_welcome
        call print_int10
    
        ; We need to reset the floppy to use int 0x13 to read things
        .reset_floppy:
            mov ah, 0
            int 0x13
            jc .reset_floppy

        ;
        ; FAT Reading code here! Beware! Dun dun dun
        ;

        ; First get the sector of the first FAT
        xor eax, eax
        mov ax, [this_bpb + bpb_fat16.reserved_sector_count]

        xor ebx, ebx
        mov ebx, [this_bpb + bpb_fat16.hidden_sector_count]

        add eax, ebx
        mov [variables.fat0_start_sector], eax
        ; Calculate root directory sector
        ; (fat_sz * fat_cnt) + fat0_start
        xor eax, eax
        mov ax, [this_bpb+bpb_fat16.fat_count]

        xor ebx, ebx
        mov bx, [this_bpb+bpb_fat16.sectors_per_fat]

        mul ebx ; (fat_sz * fat_cnt)

        add eax, [variables.fat0_start_sector]

        ; Save this value, we'll need it later
        mov [variables.root_directory_sector], eax

        ; We now have the variables we need.
        ; Note: No register state is assumed now

        ; Load the entire root directory into memory
        ; so that we can find the bootloader!

        ; To load the root directory, we need to know its size
        xor eax, eax
        mov ax, [this_bpb + bpb_fat16.max_root_entry_count]
        xchg bx, bx

        mov ebx, DIR_ENTRY_SIZE
        mul ebx

        ; eax = byte size of root directory
        xor ebx, ebx
        mov bx, [this_bpb + bpb_fat16.bytes_per_sector]
        div ebx
        ; TODO: Remainder?

        mov word [read_packet_num_blocks], ax

        ; The root directory is cluster 2
        mov eax, 2
        call get_fat_sector_for_cluster

        call get_fat_entry_for_cluster

        .kernel_not_found:
            xor ax, ax
            mov es, ax
            mov si, bootloader_error
            call print_int10

            hlt

        ;
        ; Functions
        ;

        ; Prints using int 0x10, assumes address to string in ds:si
        print_int10:
            lodsb
            or al, al
            jz .done
            mov ah, 0xE
            int 0x10
            jmp print_int10
            .done:
                ret
    
        ; Note: Assumes cluster number in eax
        ; Returns:
        ;     EAX: Fat sector to load
        ;      DX: Entry offset in sector
        get_fat_sector_for_cluster:

            ; Calculate FAT offset
            mov ecx, 2
            mul ecx
            mov ebx, [this_bpb + bpb_fat16.bytes_per_sector]
            div ebx

            mov ebx, [variables.fat0_start_sector]

            add eax, ebx
            ; EAX = FAT sector to load now
            ; DX  = FAT offset
            ret

        ; Note: assumes cluster number in EAX
        ; Note: assumes entry offset in sector in DX
        get_fat_entry_for_cluster:
            mov [read_packet_lba], eax

            mov ecx, TEMP_FAT_SECTOR_ADDRESS
            mov [read_packet_buffer], ecx
            ; Load in the sector
            ; TODO: Check if it's already loaded (variable for it)
            mov si, read_packet
            mov ah, 0x42
            mov dl, 0x80 ; TODO: Trashing argument here!
            int 0x13

            mov eax, 0x7; TODO: Eax = TEMP_FAT_SECTOR_ADDRESS + dx

            ret
        halt:
            cli
            hlt
; ================================================================================
; Padding & Signature
; ================================================================================
times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab

