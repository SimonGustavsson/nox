struc struc_part_entry
    .status             resb    1
    .first_head         resb    1
    .first_sector       resb    1
    .first_cylinder     resb    1
    .type               resb    1
    .last_head          resb    1
    .last_sector        resb    1
    .last_cylinder      resb    1
    .lba_low            resb    2
    .lba_high           resb    2
    .sector_count       resb    4
endstruc

struc struc_mbr
    .code               resb    446
    .part0              resb    struc_part_entry_size
    .part1              resb    struc_part_entry_size
    .part2              resb    struc_part_entry_size
    .part3              resb    struc_part_entry_size
    .signature          resw    1
endstruc

; NASM Syntax
; vim: ft=nasm expandtab
