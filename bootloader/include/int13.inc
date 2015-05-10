; INT 0x13 AH=0x42 Read Packet
struc struc_read_packet
    .packet_size        resb    1
    .reserved           resb    1
    .num_blocks         resw    1
    .dest_offset        resw    1
    .dest_segment       resw    1
    .lba                resd    1
    .lba_ext            resd    1
endstruc

; arguments:
;   eax - LBA
;   dl - drive number
;   cx - number of sectors
;   es - destination segment
;   di - destination offset
read_sectors:
    push ebp
    push eax
    push esi

    sub sp, struc_read_packet_size
    mov bp, sp

    mov byte  [bp+struc_read_packet.packet_size],   struc_read_packet_size
    mov byte  [bp+struc_read_packet.reserved],      0x00
    mov word  [bp+struc_read_packet.num_blocks],    cx
    mov word  [bp+struc_read_packet.dest_offset],   di
    mov word  [bp+struc_read_packet.dest_segment],  es
    mov dword [bp+struc_read_packet.lba],           eax
    mov dword [bp+struc_read_packet.lba_ext],       0

    mov si, bp
    mov ah, 0x42
    int 0x13

    add sp, struc_read_packet_size

    pop esi
    pop eax
    pop ebp

    ret
