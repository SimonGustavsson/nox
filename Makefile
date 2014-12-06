TOOL=i686-elf
SOURCE=source
BUILD=build
INCLUDE=include
DISK_SECTOR_COUNT=$(shell echo $$((32 * 1024)))
PART_SECTOR_COUNT=$(shell echo $$((16 * 1024)))
PART_MKDOSFS_SIZE=$(shell echo $$(($(PART_SECTOR_COUNT) / 2)))
PART_OFFSET_SECTORS=2048
CFLAGS=-std=c11 -ffreestanding -nostdlib -c

nox: directories $(BUILD)/nox-disk.img

$(BUILD)/nox-disk.img: $(BUILD)/nox-fs.img $(BUILD)/mbr.bin $(BUILD)/vbr.bin
	
	# Empty file for the disk image
	dd if=/dev/zero of=$@ count=$(DISK_SECTOR_COUNT)
	
	# Partition it with a single 8MiB FAT12 Partition
	@echo "o\nn\np\n1\n$(PART_OFFSET_SECTORS)\n+$(PART_SECTOR_COUNT)\na\n1\nt\n1\nw" | fdisk $@
	
	# Blit in a FAT12 file system
	dd if=$(BUILD)/nox-fs.img of=$@ seek=$(PART_OFFSET_SECTORS) count=$(PART_SECTOR_COUNT) conv=notrunc

	# Blit in the MBR
	dd if=$(BUILD)/mbr.bin of=$@ bs=1 count=446 conv=notrunc

$(BUILD)/nox-fs.img: $(BUILD)/vbr.bin $(BUILD)/BOOT.SYS $(BUILD)/KERNEL.BIN
	mkdosfs -h $(PART_OFFSET_SECTORS) -C -n "NOX" -F 12 $@ $(PART_MKDOSFS_SIZE)

	# Blit in our VBR
	dd if=$(BUILD)/vbr.bin of=$@ bs=1 count=448 skip=62 seek=62 conv=notrunc

	mcopy -i $(BUILD)/nox-fs.img $(BUILD)/BOOT.SYS ::BOOT.SYS
	mcopy -i $(BUILD)/nox-fs.img $(BUILD)/KERNEL.BIN ::KERNEL.BIN

$(BUILD)/mbr.bin: $(SOURCE)/mbr.asm
	nasm $^ -o $@ -f bin -i include/

$(BUILD)/vbr.bin: $(SOURCE)/vbr.asm
	nasm $^ -o $@ -f bin -i include/

# Note: Upper case because we use FAT12 and this makes it easy for now 
$(BUILD)/KERNEL.BIN: $(BUILD)/kernel.elf
	$(TOOL)-objcopy $^ -O binary $@

$(BUILD)/kernel.elf: $(BUILD)/terminal.o $(BUILD)/pio.o $(BUILD)/pci.o $(BUILD)/kernel.o
	$(TOOL)-ld -T kernel.ld $^ -o $@

$(BUILD)/pio.o: $(SOURCE)/pio.c
	$(TOOL)-gcc $^ -o $@ -Iinclude/ $(CFLAGS)

$(BUILD)/pci.o: $(SOURCE)/pci.c
	$(TOOL)-gcc $^ -o $@ -Iinclude/ $(CFLAGS)

$(BUILD)/terminal.o: $(SOURCE)/terminal.c
	$(TOOL)-gcc $^ -o $@ -Iinclude/ $(CFLAGS)

$(BUILD)/idt.o: $(SOURCE)/idt.c
	$(TOOL)-gcc $^ -o $@ -Iinclude/ $(CFLAGS)

$(BUILD)/kernel.o: $(SOURCE)/kernel.c
	$(TOOL)-gcc $^ -o $@ -Iinclude/ $(CFLAGS)

$(BUILD)/BOOT.SYS: $(SOURCE)/kloader.asm
	nasm $^ -o $@ -f bin -i include/

.PHONY: clean directories run fire
clean:
	rm -f -r $(BUILD)

directories:
	@mkdir -p $(BUILD)

run: nox
	bochs -rc bochs_run_on_launch.rc -q

fire:
	@echo *fire crackles*
