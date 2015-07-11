; ================================================================================
; Compile time constants
; ================================================================================
VBR_RUN_ADDRESS            EQU 0x600
VBR_LOAD_ADDRESS           EQU 0x7C00
BOOTLOADER_LOAD_ADDRESS    EQU VBR_LOAD_ADDRESS
VBR_STACK                  EQU VBR_RUN_ADDRESS
READ_BUFFER_ADDRESS        EQU (VBR_LOAD_ADDRESS)
DIR_ENTRY_SIZE             EQU 0x20

; "Variables"
ROOT_DIR_SECTOR_ADDRESS    EQU (VBR_RUN_ADDRESS       + 0x200)
FAT0_SECTOR_ADDRESS        EQU (ROOT_DIR_SECTOR_ADDRESS   + 4)
DATA_SECTOR_START_ADDRESS  EQU (FAT0_SECTOR_ADDRESS       + 4)
CLUSTER_BYTE_SIZE          EQU (DATA_SECTOR_START_ADDRESS + 4)
TMP_FAT_BUFFER_ADDRESS     EQU (CLUSTER_BYTE_SIZE         + 4)

; ================================================================================
; NASM Directives
; ================================================================================
section .text
%include "fat16.inc"
org VBR_RUN_ADDRESS
bits 16                 ; We're in real mode
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
msg_welcome                 db "NOX", 0x0D, 0x0A, 0
bootloader_found            db "VBR:)", 0x0D, 0x0A, 0
bootloader_error            db "VBR:(", 0x0D, 0x0A, 0

; Structure for INT 0x13 (0x42) for reading blocks from disk
read_packet:
                            db 0x10                ; Packet size (bytes)
                            db 0                   ; Reserved
    read_packet_num_blocks  dw 1                   ; Blocks to read
    read_packet_buffer      dw READ_BUFFER_ADDRESS ; Buffer address
                            dw 0                   ; Memory page
    read_packet_lba         dd 0                   ; LBA to read
                            dd 0                   ; Extra storage for LBAs > 4 bytes

; ================================================================================
; Start - Relocation
; ================================================================================
code:

    ;
    ; First of all, the MBR will hand us the LBA of
    ; the partition we're on in EAX, so that we don't have to
    ; reach into the partition table ourselves
    ; (We don't use the values in the BPB for hidden/reserved
    ; because its a stupid idea to put those values there)
    ;

    ; The MBR gives us the LBA of the partition,
    ; However, for FAT calculations, we are only interested in
    ; where the FAT region starts, so skip past the VBR sector
    add eax, 1
    mov [FAT0_SECTOR_ADDRESS], eax; Save the arg we got from the MBR

    ; Relocate ourselves so we can load the bootloader to the address
    ; we were loaded to, ensuring the same load address for all stages of the boot
    xor ax, ax
    mov word cx,  0x200                   ; Relocate 512 bytes
    mov es, ax
    mov ds, ax
    mov si, VBR_LOAD_ADDRESS        ; From
    mov di, VBR_RUN_ADDRESS         ; To
    rep movsb

    jmp 0:relocated                 ; Far jump to relocated code

; ================================================================================
; Relocated code
; ================================================================================
relocated:
    xor ax, ax
    mov es, ax
    mov si, msg_welcome
    call print_int10

    mov sp, VBR_STACK           ; Stack setup

    .reset_floppy:              ; Prepare for reading
        mov ah, 0
        int 0x13
        jc .reset_floppy

    ;
    ; FAT Reading code here! Beware! Dun dun dun
    ;
    ; ROOT_DIR_SECTOR = (FAT_SIZE * FAT_COUNT) + FAT0_START_SECTOR
    ; Calculate root directory sector
    ; (fat_sz * fat_cnt) + fat0_start
    mov ax, [this_bpb+bpb_fat16.fat_count]       ; Most likely 2
    mov bx, [this_bpb+bpb_fat16.sectors_per_fat] ; Most likely 32
    mul bx                                       ; (fat_sz * fat_cnt)
    add eax, [FAT0_SECTOR_ADDRESS]               ; + fat0_start
    mov [ROOT_DIR_SECTOR_ADDRESS], eax

    xor ax, ax
    mov al, [this_bpb + bpb_fat16.sectors_per_cluster]
    mov [read_packet_num_blocks], ax                  ; We always read full clusters

    ; We need to know the cluster size (in bytes) to increment the buffer
    ; every time we read a cluster
    mov bx, [this_bpb + bpb_fat16.bytes_per_sector]
    mul bx
    mov [CLUSTER_BYTE_SIZE], eax

    ; Calculate size of root directory
    mov ax, [this_bpb + bpb_fat16.max_root_entry_count]
    mov bx, DIR_ENTRY_SIZE
    mul bx

    ; AX now contains the size of the root directory in bytes
    ; But how many sectors is that?
    mov bx, [this_bpb + bpb_fat16.bytes_per_sector]
    div bx

    ; Save root dir size, we need it after the calculations
    mov cx, ax
    add ax, [ROOT_DIR_SECTOR_ADDRESS]
    mov [DATA_SECTOR_START_ADDRESS], eax

    ; Restore root dir sector size
    mov ax, cx
    ; AX now contains the amount of sectors
    ; the root directory occupies
    mov [read_packet_num_blocks], ax
    mov eax, [ROOT_DIR_SECTOR_ADDRESS]
    call read_int13

    ; The root directory should now be at READ_BUFFER_ADDRESS
    mov ax, [this_bpb + bpb_fat16.max_root_entry_count]
    mov word bx, 0x20
    mul bx
    mov bx, READ_BUFFER_ADDRESS
    add ax, bx
    mov bx, ax

    ; BX now contains the max address of the root directory
    mov ax, READ_BUFFER_ADDRESS

    .find_kloader:
        mov si, bootloader_name
        mov cx, 8 + 3
        mov di, ax
        repe cmpsb
        je .kloader_found

        ; Not this entry, advance to next
        cmp ax, bx
        jl .kloader_not_found

        add ax, 0x20
        jmp short .find_kloader

    .kloader_found:

        mov bx, ax
        mov cx, [bx + dir_entry.start_cluster]
        ; Tell the world we found the bootloader
        xor ax, ax
        mov es, ax
        mov si, bootloader_found
        call print_int10

        ; Load it into memory
        mov ax, cx
        call read

        ; And jump to it!
        jmp READ_BUFFER_ADDRESS

    .kloader_not_found:
        xor ax, ax
        mov es, ax
        mov si, bootloader_error
        call print_int10

    halt:
        cli
        hlt

;===================================================================================
; Functions
;===================================================================================
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

; In: EAX - LBA to start reading at
read_int13:
    mov [read_packet_lba], eax
    mov si, read_packet
    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    ret

; Reads the given cluster and all that follows it
; IN: eax - Cluster to read
read:
    ; Reset clusters read so that we read to the start of the buffer
    mov word [read_packet_buffer], READ_BUFFER_ADDRESS

    .read_loop:
        ; Before we start trashing registers, save the cluster we just read
        ; so we can retrive it later to find its FAT entry
        push eax

        ; Calculate LBA of cluster: first_data_sector + ((n - 2) * sectors_per_cluster)
        sub eax, 2                                          ; n - 2
        xor ebx, ebx
        mov bl, [this_bpb + bpb_fat16.sectors_per_cluster]
        mul ebx                                             ; * sectors_per_cluster
        add eax, [DATA_SECTOR_START_ADDRESS]                  ; + first_data_sector
        call read_int13

        ; TODO: Int 13h error checking

        ; Increment buffer
        mov ax, [read_packet_buffer]
        add ax, [CLUSTER_BYTE_SIZE]
        mov [read_packet_buffer], ax

        ; Get the index of the cluster we just read back out
        pop eax

        ; Get FAT entry for this cluster: FAT0_SECTOR + ((n * 2) / BYTES_PER_SECTOR)
        mov ecx, 2
        mul ecx
        mov ebx, [this_bpb + bpb_fat16.bytes_per_sector]
        div ebx
        mov ebx, [FAT0_SECTOR_ADDRESS]
        add eax, ebx
        mov cx, dx

        ; EAX now contains the sector to read
        ;  DX now contains the fat entry offset

        ; Save the read buffer address and read FAT sector to a temp buffer
        mov bx, [read_packet_buffer]
        push bx

        mov word [read_packet_num_blocks], 1

        mov bx, TMP_FAT_BUFFER_ADDRESS
        mov [read_packet_buffer], bx

        call read_int13

        ; TODO: Int 13h error checking

        ; Restore file read buffer
        pop ax
        mov [read_packet_buffer], ax

        ; Get address to entry
        xor eax, eax
        add cx, TMP_FAT_BUFFER_ADDRESS ; Add base address to entry offset
        mov ax, cx
        mov word ax, [eax]

        ; Restore sectors to read
        mov bl, [this_bpb + bpb_fat16.sectors_per_cluster]
        mov [read_packet_num_blocks], bl                  ; We always read full clusters

        ; EAX should now contain the FAT entry
        cmp ax, 0xFFF7

        ; Buffer has already been all set up so we're ready
        ; to read the next cluster
        jb .read_loop

    .done:
        ret

; ================================================================================
; Padding & Signature
; ================================================================================
times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55               ; MBR magic bytes

; NASM Syntax
; vim: ft=nasm expandtab

