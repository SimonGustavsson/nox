org 0x7C00 				; Origin where the BIOS loads us
bits 16    				; We're in real mode

[map all mbr.map]

PARTITION_TABLE_OFFSET_0 EQU 0x1BE
PARTITION_TABLE_OFFSET_1 EQU PARTITION_TABLE_OFFSET_0 + 0x16
PARTITION_TABLE_OFFSET_2 EQU PARTITION_TABLE_OFFSET_1 + 0x16
PARTITION_TABLE_OFFSET_3 EQU PARTITION_TABLE_OFFSET_2 + 0x16

; VBR_OFFSET_BYTES_PER_SECTOR	EQU 0x0B
VBR_ADDRESS							EQU 0x1000
VBR_OFFSET_RESERVED_SECTOR_COUNT	EQU 0x0E

start: jmp loader ; Jump past the MBR info

msg_pre  db "MBR PRE0", 13, 10, 0
msg_post db "MBR POS0", 13, 10, 0

; Block read package tp send  to int13
readPkg:
	db 10 					; Package size (bytes)
	db 0 					; Reserved
	readPkgNumBlocks dw 1 	; Blocks to read
	readPkgBuffer dw 0x1000	; Buffer to read to
	dw 0 					; Memory Page
	readPkgLBA dd 0			; LBA to read from
	dd 0					; Extra storage for LBAs > 4 bytes

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
	xor ax, ax			; ES:SI is the address of the message, clear ES
	mov es, ax	
	mov si, msg_pre
	call print

.tryReset:
	mov ah, 0		; Reset floppy function
	int 0x13		; Call
	jc .tryReset	; Try again if carry flag is set

.readVBR:
	; DL is the drive number, BIOS will have set it to the boot drive,
	; which in our case ought to be 0x80

	xor bx, bx
	mov es, bx     ; Zero the destination segment register, offset in BX

	; Read the VBR from the first partition
	; First prepare the package
	mov word ax, [start + PARTITION_TABLE_OFFSET_0 + 0x8]
	mov word [readPkgLBA], ax
	mov word ax, [start + PARTITION_TABLE_OFFSET_0 + 0xA]
	mov word [readPkgLBA + 2], ax

	; Up to you, big man!
	mov si, readPkg
	mov ah, 0x42
	int 0x13

	; The VBR is now loaded at 0x1000
	mov ax, [VBR_ADDRESS + VBR_OFFSET_RESERVED_SECTOR_COUNT]

	; TODO: Parse root directory 

	; TODO: Locate STAGE2.SYS

	; TODO: Load STAGE2.SYS into memory

	; TODO: Jump to STAGE2.SYS

.printVolumeLabel:
	xor ax, ax			; ES:SI is the address of the message, clear ES
	mov es, ax	
	mov si, 0x1000 + 3	; Volume label is 3 bytes into the VBR and is null padded :trollface:
	call print

.printPost:
	xor ax, ax			; ES:SI is the address of the message, clear ES
	mov es, ax	
	mov si, msg_post
	call print

.halt:
	cli
	hlt
	; Calculate root directory

times 510 - ($-$$) db 0 ; Fill 0's until the 510th byte
dw 0xAA55 			    ; MBR magic bytes
