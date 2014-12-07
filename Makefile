DISK_SECTOR_COUNT := $(shell echo $$((32 * 1024)))
PART_SECTOR_COUNT := $(shell echo $$((16 * 1024)))
PART_MKDOSFS_SIZE := $(shell echo $$(($(PART_SECTOR_COUNT) / 2)))
PART_OFFSET_SECTORS :=2048

TOOL := i686-elf
BUILD := build

MODULES := kernel bootloader

OBJECTS :=
IMAGE_ASSETS :=
CLEAN_DIRS := $(BUILD)

nox: directories $(BUILD)/nox-disk.img

include $(patsubst %, %/make.mk, $(MODULES))

$(BUILD)/nox-disk.img: $(BUILD)/nox-fs.img
	
	# Empty file for the disk image
	dd if=/dev/zero of=$@ count=$(DISK_SECTOR_COUNT)
	
	# Partition it with a single 8MiB FAT12 Partition
	@echo "o\nn\np\n1\n$(PART_OFFSET_SECTORS)\n+$(PART_SECTOR_COUNT)\na\n1\nt\n1\nw" | fdisk $@
	
	# Blit in a FAT12 file system
	dd if=$(BUILD)/nox-fs.img of=$@ seek=$(PART_OFFSET_SECTORS) count=$(PART_SECTOR_COUNT) conv=notrunc

	# Blit in the MBR
	dd if=$(BUILD)/mbr.bin of=$@ bs=1 count=446 conv=notrunc

$(BUILD)/nox-fs.img: $(IMAGE_ASSETS)
	mkdosfs -h $(PART_OFFSET_SECTORS) -C -n "NOX" -F 12 $@ $(PART_MKDOSFS_SIZE)

	# Blit in our VBR
	dd if=$(BUILD)/vbr.bin of=$@ bs=1 count=448 skip=62 seek=62 conv=notrunc

	mcopy -i $(BUILD)/nox-fs.img $(BUILD)/BOOT.SYS ::BOOT.SYS
	mcopy -i $(BUILD)/nox-fs.img $(BUILD)/KERNEL.BIN ::KERNEL.BIN

directories:
	@mkdir -p $(BUILD)

run: nox
	bochs -rc bochs_run_on_launch.rc -q

fire:
	@echo *fire crackles*

clean:
	rm -f -r $(CLEAN_DIRS)

.PHONY: clean directories run fire
