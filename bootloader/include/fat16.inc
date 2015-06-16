struc bpb_fat16
    .jump                  resb 3
    .oem                   resb 8
    .bytes_per_sector      resw 1
    .sectors_per_cluster   resb 1
    .reserved_sector_count resw 1
    .fat_count             resb 1
    .max_root_entry_count  resw 1
    .sector_count          resw 1
    .media_type            resb 1
    .sectors_per_fat       resw 1
    .sectors_per_track     resw 1
    .head_count            resw 1
    .hidden_sector_count   resd 1
    .sector_count_large    resd 1
    ;
    ; EBPB Values here for FAT12/16
    ;
    .drive_number          resb 1
    .reserved              resb 1
    .signature             resb 1
    .volume_id             resd 1
    .volume_label          resb 11
    .system_id             resb 8
    ; Don't include these, as they make it difficult
    ; to calculate size of VBR - and we'll never
    ; reference them anyway! :-)
    ;.boot_code             resb 448
    ;.partition_signature   resw 1
endstruc 

struc dir_entry
    .filename           resb    8
    .ext                resb    3

    ; Attribute byte description:
    ; 76 5 4 3 2 1 0
    ; RR A D V S H R
    ; RR: Reserved
    ; A: Archive
    ; D: Directory
    ; V: Volume Name
    ; S: System file
    ; H: Hidden
    ; R: Read-Only
    .attributes           resb    1
    .reserved             resb    1 ; Reserved for WinNT
    .creation             resb    1 ; Millisecond timestamp (Actual 100th of a second)

    ; 15:11 - Hours (0-23)
    ; 10:7  - Minutes (1-12)
    ; 6:0   - Seconds (0-29) (Multiply by 2)
    .creation_time        resw    1

    ; 15:11 - Day of month (1-31)
    ; 10:7 - Month of year (1-12)
    ; 6:0 - Years from 1980 (0-127, 1980-2107)
    .creation_date        resw    1
    .last_access_date     resw    1
    .reserved_FAT32       resw    1
    .last_write_Time      resw    1 ; See above for description
    .last_write_date      resw    1 ; See above for description
    .start_cluster        resw    1 ;
    .file_size            resd    1
endstruc

; NASM Syntax
; vim: ft=nasm expandtab
