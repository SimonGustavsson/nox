DISK_SECTOR_COUNT := $(shell echo $$((32 * 1024)))
PART_SECTOR_COUNT := $(shell echo $$((16 * 1024)))
PART_MKDOSFS_SIZE := $(shell echo $$(($(PART_SECTOR_COUNT) / 2)))
PART_OFFSET_SECTORS :=2048

TOOL := i686-elf
BUILD := build

MODULES := kernel bootloader

# These variables are populated by modules
IMAGE_ASSETS :=
CLEAN_DIRS := $(BUILD)

nox: directories $(BUILD)/nox-disk.img

include $(patsubst %, %/make.mk, $(MODULES))

$(BUILD)/nox-disk.img: $(BUILD)/nox-fs.img
	@echo "FDISK $<"
	
# Empty file for the disk image
	@dd if=/dev/zero of=$@ count=$(DISK_SECTOR_COUNT) > /dev/null 2>&1
	
# Partition it with a single 8MiB FAT12 Partition
	@echo "o\nn\np\n1\n$(PART_OFFSET_SECTORS)\n+$(PART_SECTOR_COUNT)\na\n1\nt\n1\nw" | fdisk $@ > /dev/null
	
# Blit in a FAT12 file system
	@dd if=$(BUILD)/nox-fs.img of=$@ seek=$(PART_OFFSET_SECTORS) count=$(PART_SECTOR_COUNT) conv=notrunc > /dev/null 2>&1

# Blit in the MBR
	@dd if=$(BUILD)/mbr.bin of=$@ bs=1 count=446 conv=notrunc > /dev/null 2>&1

$(BUILD)/nox-fs.img: $(IMAGE_ASSETS)
	@echo "MKDOSFS $<"
	@mkdosfs -h $(PART_OFFSET_SECTORS) -C -n "NOX" -F 12 $@ $(PART_MKDOSFS_SIZE) > /dev/null

# Blit in our VBR
	@dd if=$(BUILD)/vbr.bin of=$@ bs=1 count=448 skip=62 seek=62 conv=notrunc > /dev/null 2>&1

	@mcopy -i $@ $(BUILD)/BOOT.SYS ::BOOT.SYS
	@mcopy -i $@ $(BUILD)/KERNEL.BIN ::KERNEL.BIN

directories:
	@mkdir -p $(BUILD)

run: nox
	bochs -rc bochs_run_on_launch.rc -q

fire:
	@echo *fire crackles*

clean:
	rm -f -r $(CLEAN_DIRS)

.PHONY: clean directories run fire
