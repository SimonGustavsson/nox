; Bios Parameter Block Structures
BOOTABLE_SIGNATURE      equ     0xaa55
FAT_SIGNATURE_1         equ     0x28
FAT_SIGNATURE_2         equ     0x29
MEDIA_TYPE_FIXED_DISC   equ     0xf8

struc bpb
    .jump               resb    3
    .oem                resb    8
    .bytesPerSector     resw    1
    .sectorsPerCluster  resb    1
    .reservedSectors    resw    1
    .fatCount           resb    1
    .dirEntryCount      resw    1
    .sectorCount        resw    1
    .mediaType          resb    1
    .sectorsPerFat      resw    1
    .sectorsPerTrack    resw    1
    .heads              resw    1
    .hiddenSectors      resd    1
    .sectorCountLarge   resd    1
endstruc

struc ebpb_fat1216
    .bpb                resb    bpb_size
    .driveNumber        resb    1
    .flags              resb    1
    .signature          resb    1
    .volumeId           resb    4
    .volumeLabel        resb    11
    .systemId           resb    8
    .bootCode           resb    448
    .bootSignature      resb    2
endstruc

struc ebpb_fat32
    .bpb                resb    bpb_size
    .sectorsPerFat32    resd    1
    .flags              resw    1
    .version            resw    1
    .rootCluster        resd    1
    .fsInfoSector       resw    1
    .bootBackupSector   resw    1
    .reserved           resb    12
    .driveNumber        resb    1
    .flags2             resb    1
    .signature          resb    1
    .volumeId           resb    4
    .volumeLabel        resb    11
    .systemId           resb    8
    .bootCode           resb    420
    .bootSignature      resb    2
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
    .attributes         resb    1
    .reserved           resb    1 ; Reserved for WinNT
    .creation           resb    1 ; Millisecond timestamp (Actual 100th of a second)

    ; 15:11 - Hours (0-23)
    ; 10:7  - Minutes (1-12)
    ; 6:0   - Seconds (0-29) (Multiply by 2)
    .creationTime       resw    1

    ; 15:11 - Day of month (1-31)
    ; 10:7 - Month of year (1-12)
    ; 6:0 - Years from 1980 (0-127, 1980-2107)
    .creationDate       resw    1
    .lastAccessDate     resw    1
    .reservedFat32      resw    1
    .lastWriteTime      resw    1 ; See above for description
    .lastWriteDate      resw    1 ; See above for description
    .startCluster       resw    1 ; 
    .fileSize           resd    1
endstruc
