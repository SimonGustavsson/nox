# Get the name of the module directory we're in
MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

# Calculate directories to get sources from, and our object directory
CSOURCE_DIR := $(MODULE)source
ASOURCE_DIR := $(MODULE)source
OBJ_DIR := $(MODULE)obj
INCLUDE_DIR := $(MODULE)include
DEP_DIR := $(MODULE)deps

KERNEL_LINKER_SCRIPT := $(MODULE)kernel.ld
KLOADER_LINKER_SCRIPT := $(MODULE)kloader.ld

# Find all candidates for compiling
ASOURCE := $(shell find $(ASOURCE_DIR) -name '*.asm')
CSOURCE := $(shell find $(CSOURCE_DIR) -name '*.c')

# Remove kloader from standard Nox
ASOURCE := $(filter-out $(ASOURCE_DIR)/kloader/kloader_start.asm, $(ASOURCE))
CSOURCE := $(filter-out $(CSOURCE_DIR)/kloader/kloader_main.c, $(CSOURCE))

# Construct a list of all object files
COBJECTS := $(CSOURCE:.c=.o)
COBJECTS := $(subst $(CSOURCE_DIR), $(OBJ_DIR), $(COBJECTS))
AOBJECTS := $(ASOURCE:.asm=.o)
AOBJECTS := $(subst $(ASOURCE_DIR), $(OBJ_DIR), $(AOBJECTS))

#
# Compiler options
#
PLATFORM_DEBUG:=PLATFORM_DEBUG_BOCHS
PLATFORM_ARCH:=PLATFORM_ARCH_X86
PLATFORM_BITS_VAL:=32
CFLAGS=-std=c11 \
       -ffreestanding \
       -nostdlib \
       -Wall \
       -Werror \
       -c \
       -D $(PLATFORM_DEBUG) \
       -D $(PLATFORM_ARCH) \
       -D PLATFORM_BITS=$(PLATFORM_BITS_VAL)
CINCLUDE := $(patsubst %,-I%, $(shell find $(INCLUDE_DIR) -type d))

# Add prerequisites for full build
FS_FILES += $(FS_DIR)/kernel.elf $(FS_DIR)/BOOT.SYS

# Tell the main makefile to remove our object dir on clean
CLEAN_DIRS += $(OBJ_DIR) $(DEP_DIR)

#
# Include dependencies files automatically generated by previous builds
#
-include $(addprefix $(DEP_DIR)/, $(notdir $(COBJECTS:.o=.d)))

#
# Actually building stuff
#
kernel: kernel_directories $(BUILD_DIR)/kernel.elf

# Note: Build into root build directory
$(BUILD_DIR)/kernel.elf: $(COBJECTS) $(AOBJECTS) $(KERNEL_LINKER_SCRIPT)

	@echo "LD      $@"

	@$(TOOL)-ld -T $(KERNEL_LINKER_SCRIPT) -Map=$(BUILD_DIR)/kernel.map $(filter-out kernel_directories, $^) -o $@

$(OBJ_DIR)/%.o : $(CSOURCE_DIR)/%.c

	@mkdir -p $(dir $@)
	@mkdir -p $(DEP_DIR)/$(dir $*)
	@echo "CC      $<"

	@$(TOOL)-gcc $< -o $@ $(CINCLUDE) $(CFLAGS) -MD -MF $(DEP_DIR)/$*.d

$(OBJ_DIR)/%.o : $(ASOURCE_DIR)/%.asm

	@mkdir -p $(dir $@)

	@echo "AS      $<"

	@nasm $< -o $@ -f elf32 -i $(INCLUDE_DIR)

#################################################################################
#
# Kloader
#
################################################################################
KLOADER_CSOURCES := $(CSOURCE_DIR)/ata.c $(CSOURCE_DIR)/fat.c $(CSOURCE_DIR)/fs.c $(CSOURCE_DIR)/kloader/kloader_main.c $(CSOURCE_DIR)/mem_mgr.c $(CSOURCE_DIR)/pio.c $(CSOURCE_DIR)/screen.c $(CSOURCE_DIR)/terminal.c $(CSOURCE_DIR)/string.c $(CSOURCE_DIR)/elf.c
KLOADER_ASOURCES := $(CSOURCE_DIR)/kloader/kloader_start.asm

KLOADER_OBJECTS := $(KLOADER_CSOURCES:.c=.o)
KLOADER_OBJECTS += $(KLOADER_ASOURCES:.asm=.o)
KLOADER_OBJECTS := $(subst $(CSOURCE_DIR), $(OBJ_DIR), $(KLOADER_OBJECTS))
KLOADER_OBJECTS := $(subst $(ASOURCE_DIR), $(OBJ_DIR), $(KLOADER_OBJECTS))

$(BUILD_DIR)/BOOT.SYS: $(BUILD_DIR)/boot.elf
	@echo "OBJCOPY $<"
	@@$(TOOL)-objcopy $^ -O binary --set-section-flags .bss=alloc,load,contents $@

$(BUILD_DIR)/boot.elf: $(KLOADER_OBJECTS) $(KLOADER_LINKER_SCRIPT)
	@echo "LD      $@" 
	@$(TOOL)-ld -T $(KLOADER_LINKER_SCRIPT) $(filter-out kernel_directories, $^) -o $@

kernel_directories:
	@mkdir -p $(DEP_DIR)
	@mkdir -p $(OBJ_DIR)

.PHONY: kernel_directories
