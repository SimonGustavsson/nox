# Get the name of the module directory we're in
MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

# Calculate directories to get sources from, and our object directory
CSOURCE_DIR := $(MODULE)source
OBJ_DIR := $(MODULE)obj
INCLUDE_DIR := $(MODULE)include

# Find all candidates for compiling
CSOURCE := $(shell find $(CSOURCE_DIR) -name '*.c')

# Construct a list of all object files
COBJECTS := $(CSOURCE:.c=.o)
COBJECTS := $(subst $(CSOURCE_DIR), $(OBJ_DIR), $(COBJECTS))

#
# Compiler options
#
CFLAGS=-std=c11 -ffreestanding -nostdlib -c
CINCLUDE := $(patsubst %,-I%, $(shell find $(INCLUDE_DIR) -type d))

# Add prerequisites for full build
IMAGE_ASSETS += $(BUILD)/KERNEL.BIN

# Tell the main makefile to remove our object dir on clean
CLEAN_DIRS += $(OBJ_DIR)

#
# Actually building stuff
#

# Note: Upper case because we use FAT12 and this makes it easy for now 
$(BUILD)/KERNEL.BIN: $(OBJ_DIR)/kernel.elf
	@$(TOOL)-objcopy $^ -O binary $@

# Note: Build into root build directory
$(OBJ_DIR)/kernel.elf: $(COBJECTS)
	@$(TOOL)-ld -T kernel.ld $^ -o $@

$(OBJ_DIR)/%.o : $(CSOURCE_DIR)/%.c

	@mkdir -p $(dir $@)

	@$(TOOL)-gcc $< -o $@ $(CINCLUDE) $(CFLAGS)
