AS := nasm

# Get the name of the module directory we're in
MODULE := $(dir $(lastword $(MAKEFILE_LIST)))

ASM_INCLUDE := $(MODULE)include/
SOURCE := $(MODULE)source

bootloader: $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/vbr.bin

$(BUILD_DIR)/mbr.bin: $(SOURCE)/mbr.asm
	@echo "$(TIME) AS       $<"
	
	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)

$(BUILD_DIR)/vbr.bin: $(SOURCE)/vbr.asm
	@echo "$(TIME) AS       $<"

	@$(AS) $^ -o $@ -f bin -i $(ASM_INCLUDE)

.PHONY: bootloader
