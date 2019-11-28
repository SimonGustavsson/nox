.DEFAULT_GOAL = nox
DISK_SECTOR_COUNT := $(shell echo $$((32 * 1024)))
PART_SECTOR_COUNT := $(shell echo $$((16 * 1024)))
PART_MKDOSFS_SIZE := $(shell echo $$((($(PART_SECTOR_COUNT) / 2) * 2)))
PART_OFFSET_SECTORS :=2048
PART_OFFSET_BYTES := $(shell echo $$(($(PART_OFFSET_SECTORS) * 512)))

TOOL := i686-elf

# This is the name of the final (complete) disk image
IMAGE_NAME := nox-disk

# Directories
BUILD_DIR := build
CLEAN_DIRS := $(BUILD_DIR)
IMAGE_PATH := $(BUILD_DIR)/$(IMAGE_NAME).img
FS_DIR := $(BUILD_DIR)/fs
FS_FILES :=
TAG_FILES := $(shell find . '(' -name *.c -o -name *.h -o -name *.asm ')')

TIME = [$(shell date +%H:%M:%S)]

# Modules are folders that exist in the repo root
IGNORED_DIRS := . ./.git ./$(BUILD_DIR)
_DIRS := $(shell find . -maxdepth 1 -type d | LC_ALL=c sort)
MODULES := $(notdir $(filter-out $(IGNORED_DIRS),$(_DIRS)))

# Include their make files
include $(patsubst %, %/make.mk, $(MODULES))

nox: directories tags $(IMAGE_PATH) $(MODULES) $(FS_FILES)

$(FS_FILES) : $(FS_DIR)/%: $(BUILD_DIR)/%
	@echo "$(TIME) CP       $(BUILD_DIR)/$(notdir $@) -> $@"
	@cp $(BUILD_DIR)/$(notdir $@) $@
	@echo "$(TIME) MCOPY:   $@ :: $(shell echo $(notdir $@) | tr a-z A-Z)"

# NOTE: HACK: TODO: WARNING: "1M" After the image name,
#       We assume the partition starts at this offset.
	@mcopy -o -i $(BUILD_DIR)/nox-disk.img@@1M $@ ::$(shell echo $(notdir $@) | tr a-z A-Z)

$(BUILD_DIR)/$(IMAGE_NAME).img: $(BUILD_DIR)/nox-fs.img $(BUILD_DIR)/mbr.bin
	@echo "$(TIME) MKFS.FAT $<"
	@dd if=/dev/zero of=$@ count=$(DISK_SECTOR_COUNT) > /dev/null 2>&1
	@mkfs.fat $@ > /dev/null

	@echo "$(TIME) FDISK    $@"
	@echo "o\nn\np\n1\n$(PART_OFFSET_SECTORS)\n+$(PART_SECTOR_COUNT)\na\n1\nt\n4\nw" | fdisk $@ > /dev/null 2>&1
	@dd if=$(BUILD_DIR)/nox-fs.img of=$@ seek=$(PART_OFFSET_SECTORS) count=$(PART_SECTOR_COUNT) conv=notrunc > /dev/null 2>&1
	@dd if=$(BUILD_DIR)/mbr.bin of=$@ bs=1 count=446 conv=notrunc > /dev/null 2>&1

$(BUILD_DIR)/nox-fs.img: $(BUILD_DIR)/vbr.bin
	@echo "$(TIME) RM       $@"
	@rm -f $@

	@echo "$(TIME) MKDOSFS  $@"
	@mkdosfs -h $(PART_OFFSET_SECTORS) -C -n "NOX" -F 16 $@ $(PART_MKDOSFS_SIZE) > /dev/null
	@dd if=$(BUILD_DIR)/vbr.bin of=$@ bs=1 count=3 conv=notrunc > /dev/null 2>&1
	@dd if=$(BUILD_DIR)/vbr.bin of=$@ bs=1 count=448 skip=62 seek=62 conv=notrunc > /dev/null 2>&1

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(FS_DIR)

tags: $(TAG_FILES)
	@ctags $^

run:
	@bochs -rc bochs_run_on_launch.rc -q

fire:
	@echo *fire crackles*

clean:
	rm -f -r $(CLEAN_DIRS)

LOOP_DEV_0=
LOOP_DEV_1=
FINAL_IMAGE_PATH=build/nox-with-grub.img

withgrub: dev0 dev1
	@echo "Using loopback devices  '$(LOOP_DEV_0)' and '$(LOOP_DEV_1)'"
	@sudo mke2fs $(LOOP_DEV_1)
	@sudo mkdosfs -F32 -f 2 $(LOOP_DEV_1)
	@mkdir -p build/final
	@sudo mount $(LOOP_DEV_1) build/final
	sudo grub-install --root-directory=/home/simon/src/nox/build/final --no-floppy --modules="normal part_msdos ext2 multiboot" $(LOOP_DEV_0)
	@sudo cp grub.cfg build/final/boot/grub/grub.cfg
	@sudo cp build/kernel.elf build/final/boot/nox-kernel.elf
	@sync
	@sudo losetup -d $(LOOP_DEV_0)
	@sudo losetup -d $(LOOP_DEV_1)
	@sudo umount build/final

dev0: $(FINAL_IMAGE_PATH)
	$(eval LOOP_DEV_0=$(shell sudo losetup -f))
	$(shell sudo losetup $(LOOP_DEV_0) $(FINAL_IMAGE_PATH))

dev1: $(FINAL_IMAGE_PATH)
	$(eval LOOP_DEV_1=$(shell sudo losetup -f))
	$(shell sudo losetup $(LOOP_DEV_1) $(FINAL_IMAGE_PATH) -o 1048576)

$(FINAL_IMAGE_PATH):
	@rm -f $(FINAL_IMAGE_PATH)
	@dd if=/dev/zero of=$(FINAL_IMAGE_PATH) bs=512 count=131072
	@echo "n\np\n1\n\n\na\nw" | fdisk $(FINAL_IMAGE_PATH) > /dev/null 2>&1

.PHONY: nox directories clean final test dev0 dev1 $(FINAL_IMAGE_PATH)

