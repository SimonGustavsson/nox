DISK_SECTOR_COUNT := $(shell echo $$((32 * 1024)))
PART_SECTOR_COUNT := $(shell echo $$((16 * 1024)))
PART_MKDOSFS_SIZE := $(shell echo $$((($(PART_SECTOR_COUNT) / 2) * 2)))
PART_OFFSET_SECTORS :=2048

TOOL := i686-elf
BUILD := build

MODULES := kernel bootloader

# These variables are populated by modules
IMAGE_ASSETS :=
CLEAN_DIRS := $(BUILD)

nox: directories tags $(BUILD)/nox-disk.img

include $(patsubst %, %/make.mk, $(MODULES))

$(BUILD)/nox-disk.img: $(BUILD)/nox-fs.img
	@echo "FDISK $<"

# Empty file for the disk image
	@dd if=/dev/zero of=$@ count=$(DISK_SECTOR_COUNT) > /dev/null 2>&1

# Initialize file system (not necessary, but stops some warnings from fdisk)
	@mkfs.fat $@ > /dev/null

# Partition it with a single 8MiB FAT16 Partition
# o = Create empty DOS partiton table
# n = Add new partition
# p = Primary partition
# 1 = Partition number
# _ = First sector (2048, default)
# _ = Last sector (size) 16384
# a = Toggle bootable flag
# 1 = Partition number
# t = Change partition system id
# 1 = (Fs Type, 1 = FAT12, 4 = FAT16 (<32MB), 6 = FAT16, B = Fat32, C = Fat32 (LBA)
# w = Write table and exit
	@echo "o\nn\np\n1\n$(PART_OFFSET_SECTORS)\n+$(PART_SECTOR_COUNT)\na\n1\nt\n4\nw" | fdisk $@ > /dev/null

# Blit in a FAT16 file system
	@dd if=$(BUILD)/nox-fs.img of=$@ seek=$(PART_OFFSET_SECTORS) count=$(PART_SECTOR_COUNT) conv=notrunc > /dev/null 2>&1

# Blit in the MBR
	@dd if=$(BUILD)/mbr.bin of=$@ bs=1 count=446 conv=notrunc > /dev/null 2>&1

$(BUILD)/nox-fs.img: $(IMAGE_ASSETS)
	@echo "MKDOSFS $<"

# Remove if it already exists to prevent error from mkdosfs
	@rm -f $@

	@mkdosfs -h $(PART_OFFSET_SECTORS) -C -n "NOX" -F 16 $@ $(PART_MKDOSFS_SIZE) > /dev/null

# Blit in our VBR
# First, blit in the JMP instruction (3 bytes)
	@dd if=$(BUILD)/vbr.bin of=$@ bs=1 count=3 conv=notrunc > /dev/null 2>&1
# Secondly, blit in the code section, skipping past the BPB
	@dd if=$(BUILD)/vbr.bin of=$@ bs=1 count=448 skip=62 seek=62 conv=notrunc > /dev/null 2>&1
	@mcopy -i $@ $(BUILD)/BOOT.SYS ::BOOT.SYS
	@mcopy -i $@ $(BUILD)/KERNEL.BIN ::KERNEL.BIN
	@mcopy -i $@ kernel/obj/kernel.elf ::KERNELSS.ELF

directories:
	@mkdir -p $(BUILD)

TAG_FILES := $(shell find . '(' -name *.c -o -name *.h -o -name *.asm ')')

tags: $(TAG_FILES)
	@ctags $^

run:
	@bochs -rc bochs_run_on_launch.rc -q

fire:
	@echo *fire crackles*

clean:
	rm -f -r $(CLEAN_DIRS)

.PHONY: clean directories run fire
