# Get the name of the module directory we're in
MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

# Calculate directories to get sources from, and our object directory
CSOURCE_DIR := $(MODULE)source
ASOURCE_DIR := $(MODULE)source
OBJ_DIR := $(MODULE)obj
INCLUDE_DIR := $(MODULE)include
DEP_DIR := $(MODULE)deps

# Find all candidates for compiling
ASOURCE := $(shell find $(ASOURCE_DIR) -name '*.asm')
CSOURCE := $(shell find $(CSOURCE_DIR) -name '*.c')

# Construct a list of all object files
COBJECTS := $(CSOURCE:.c=.o)
COBJECTS := $(subst $(CSOURCE_DIR), $(OBJ_DIR), $(COBJECTS))
AOBJECTS := $(ASOURCE:.asm=.o)
AOBJECTS := $(subst $(ASOURCE_DIR), $(OBJ_DIR), $(AOBJECTS))

#
# Compiler options
#
CFLAGS=-std=c11 -ffreestanding -nostdlib -c
CINCLUDE := $(patsubst %,-I%, $(shell find $(INCLUDE_DIR) -type d))

# Add prerequisites for full build
IMAGE_ASSETS += $(BUILD)/KERNEL.BIN

# Tell the main makefile to remove our object dir on clean
CLEAN_DIRS += $(OBJ_DIR) $(DEP_DIR)

#
# Include dependencies files automatically generated by previous builds
#
-include $(addprefix $(DEP_DIR)/, $(notdir $(COBJECTS:.o=.d)))

#
# Actually building stuff
#

# Note: Upper case because we use FAT12 and this makes it easy for now 
$(BUILD)/KERNEL.BIN: $(OBJ_DIR)/kernel.elf
	@echo "OBJCOPY $<"

	@$(TOOL)-objcopy $^ -O binary $@

# Note: Build into root build directory
$(OBJ_DIR)/kernel.elf: kernel_directories $(COBJECTS) $(AOBJECTS)

	@echo "LD $^"

	@$(TOOL)-ld -T kernel.ld $(filter-out kernel_directories, $^) -o $@

$(OBJ_DIR)/%.o : $(CSOURCE_DIR)/%.c

	@mkdir -p $(dir $@)

	@echo "CC $<"

	@$(TOOL)-gcc $< -o $@ $(CINCLUDE) $(CFLAGS) -MD -MF $(DEP_DIR)/$*.d 

$(OBJ_DIR)/%.o : $(ASOURCE_DIR)/%.asm

	@mkdir -p $(dir $@)

	@echo "AS $<"

	nasm $< -o $@ -f elf32 -i $(INCLUDE_DIR)

kernel_directories:
	@mkdir -p $(DEP_DIR)
	@mkdir -p $(OBJ_DIR)

.PHONY: kernel_directories